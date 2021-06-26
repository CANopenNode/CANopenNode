/**
 * CANopen Heartbeat consumer protocol.
 *
 * @file        CO_HBconsumer.h
 * @ingroup     CO_HBconsumer
 * @author      Janez Paternoster
 * @copyright   2021 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CO_HB_CONS_H
#define CO_HB_CONS_H

#include "301/CO_driver.h"
#include "301/CO_ODinterface.h"
#include "301/CO_NMT_Heartbeat.h"
#include "301/CO_Emergency.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_HB_CONS
#define CO_CONFIG_HB_CONS (CO_CONFIG_HB_CONS_ENABLE | \
                           CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE | \
                           CO_CONFIG_GLOBAL_FLAG_TIMERNEXT | \
                           CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC)
#endif

#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_HBconsumer Heartbeat consumer
 * CANopen Heartbeat consumer protocol.
 *
 * @ingroup CO_CANopen_301
 * @{
 * Heartbeat consumer monitors Heartbeat messages from remote nodes. If any
 * monitored node don't send his Heartbeat in specified time, Heartbeat consumer
 * sends emergency message. If all monitored nodes are operational, then
 * variable _allMonitoredOperational_ inside CO_HBconsumer_t is set to true.
 * Monitoring starts after the reception of the first HeartBeat (not bootup).
 *
 * Heartbeat set up is done by writing to the OD registers 0x1016.
 * To setup heartbeat consumer by application, use
 * @code ODR_t odRet = OD_set_u32(entry, subIndex, val, false); @endcode
 *
 * @see @ref CO_NMT_Heartbeat
 */

/**
 * Heartbeat state of a node
 */
typedef enum {
  CO_HBconsumer_UNCONFIGURED = 0x00U,     /**< Consumer entry inactive */
  CO_HBconsumer_UNKNOWN      = 0x01U,     /**< Consumer enabled, but no heartbeat received yet */
  CO_HBconsumer_ACTIVE       = 0x02U,     /**< Heartbeat received within set time */
  CO_HBconsumer_TIMEOUT      = 0x03U,     /**< No heatbeat received for set time */
} CO_HBconsumer_state_t;


/**
 * One monitored node inside CO_HBconsumer_t.
 */
typedef struct {
    /** Node Id of the monitored node */
    uint8_t nodeId;
    /** NMT state of the remote node (Heartbeat payload) */
    CO_NMT_internalState_t NMTstate;
    /** Current heartbeat monitoring state of the remote node */
    CO_HBconsumer_state_t HBstate;
    /** Time since last heartbeat received */
    uint32_t timeoutTimer;
    /** Consumer heartbeat time from OD */
    uint32_t time_us;
    /** Indication if new Heartbeat message received from the CAN bus */
    volatile void *CANrxNew;
#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_HBconsumer_initCallbackPre() or NULL */
    void (*pFunctSignalPre)(void *object);
    /** From CO_HBconsumer_initCallbackPre() or NULL */
    void *functSignalObjectPre;
#endif
#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE) \
    || ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI) \
    || defined CO_DOXYGEN
    /** Previous value of the remote node (Heartbeat payload) */
    CO_NMT_internalState_t NMTstatePrev;
#endif
#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI) || defined CO_DOXYGEN
    /** Callback for remote NMT changed event.
     *  From CO_HBconsumer_initCallbackNmtChanged() or NULL. */
    void (*pFunctSignalNmtChanged)(uint8_t nodeId, uint8_t idx,
                                   CO_NMT_internalState_t NMTstate,
                                   void *object);
    /** Pointer to object */
    void *pFunctSignalObjectNmtChanged;
    /** Callback for heartbeat state change to active event.
     *  From CO_HBconsumer_initCallbackHeartbeatStarted() or NULL. */
    void (*pFunctSignalHbStarted)(uint8_t nodeId, uint8_t idx, void *object);
    /** Pointer to object */
    void *functSignalObjectHbStarted;
    /** Callback for consumer timeout event.
     *  From CO_HBconsumer_initCallbackTimeout() or NULL. */
    void (*pFunctSignalTimeout)(uint8_t nodeId, uint8_t idx, void *object);
    /** Pointer to object */
    void *functSignalObjectTimeout;
    /** Callback for remote reset event.
     *  From CO_HBconsumer_initCallbackRemoteReset() or NULL. */
    void (*pFunctSignalRemoteReset)(uint8_t nodeId, uint8_t idx, void *object);
    /** Pointer to object */
    void *functSignalObjectRemoteReset;
#endif
} CO_HBconsNode_t;


/**
 * Heartbeat consumer object.
 *
 * Object is initilaized by CO_HBconsumer_init(). It contains an array of
 * CO_HBconsNode_t objects.
 */
typedef struct {
    /** From CO_HBconsumer_init() */
    CO_EM_t *em;
    /** Array of monitored nodes, from CO_HBconsumer_init() */
    CO_HBconsNode_t *monitoredNodes;
    /** Actual number of monitored nodes, size-of-the-above-array or
     * number-of-array-elements-in-OD-0x1016, whichever is smaller. */
    uint8_t numberOfMonitoredNodes;
    /** True, if all monitored nodes are active or no node is monitored. Can be
     * read by the application */
    bool_t allMonitoredActive;
    /** True, if all monitored nodes are NMT operational or no node is
     * monitored. Can be read by the application */
    bool_t allMonitoredOperational;
    /** previous state of the variable */
    bool_t NMTisPreOrOperationalPrev;
    /** From CO_HBconsumer_init() */
    CO_CANmodule_t *CANdevRx;
    /** From CO_HBconsumer_init() */
    uint16_t CANdevRxIdxStart;
#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_OD_DYNAMIC) || defined CO_DOXYGEN
    /** Extension for OD object */
    OD_extension_t OD_1016_extension;
#endif
#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE) || defined CO_DOXYGEN
    /** Callback for remote NMT changed event.
     *  From CO_HBconsumer_initCallbackNmtChanged() or NULL. */
    void (*pFunctSignalNmtChanged)(uint8_t nodeId, uint8_t idx,
                                   CO_NMT_internalState_t NMTstate,
                                   void *object);
    /** Pointer to object */
    void *pFunctSignalObjectNmtChanged;
#endif
} CO_HBconsumer_t;


/**
 * Initialize Heartbeat consumer object.
 *
 * Function must be called in the communication reset section.
 *
 * @param HBcons This object will be initialized.
 * @param em Emergency object.
 * @param monitoredNodes Array of monitored nodes, must be defined externally.
 * @param monitoredNodesCount Size of the above array, usually equal to number
 * of array elements in OD 0x1016, valid values are 1 to 127
 * @param OD_1016_HBcons OD entry for 0x1016 - "Consumer heartbeat time", entry
 * is required, IO extension will be applied.
 * @param CANdevRx CAN device for Heartbeat reception.
 * @param CANdevRxIdxStart Starting index of receive buffer in the above CAN
 * device. Number of used indexes is equal to monitoredNodesCount.
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return @ref CO_ReturnError_t CO_ERROR_NO in case of success.
 */
CO_ReturnError_t CO_HBconsumer_init(CO_HBconsumer_t *HBcons,
                                    CO_EM_t *em,
                                    CO_HBconsNode_t *monitoredNodes,
                                    uint8_t monitoredNodesCount,
                                    OD_entry_t *OD_1016_HBcons,
                                    CO_CANmodule_t *CANdevRx,
                                    uint16_t CANdevRxIdxStart,
                                    uint32_t *errInfo);


#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize Heartbeat consumer callback function.
 *
 * Function initializes optional callback function, which should immediately
 * start processing of CO_HBconsumer_process() function.
 * Callback is called after HBconsumer message is received from the CAN bus.
 *
 * @param HBcons This object.
 * @param object Pointer to object, which will be passed to pFunctSignal(). Can be NULL
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_HBconsumer_initCallbackPre(
        CO_HBconsumer_t        *HBcons,
        void                   *object,
        void                  (*pFunctSignal)(void *object));
#endif

#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE) \
    || ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI) \
    || defined CO_DOXYGEN
/**
 * Initialize Heartbeat consumer NMT changed callback function.
 *
 * Function initializes optional callback function, which is called when NMT
 * state from the remote node changes.
 *
 * @param HBcons This object.
 * @param idx index of the node in HBcons object (only when
 *            CO_CONFIG_HB_CONS_CALLBACK_MULTI is enabled)
 * @param object Pointer to object, which will be passed to pFunctSignal().
 *               Can be NULL.
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_HBconsumer_initCallbackNmtChanged(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        void                   *object,
        void                  (*pFunctSignal)(uint8_t nodeId, uint8_t idx,
                                              CO_NMT_internalState_t NMTstate,
                                              void *object));
#endif

#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI) || defined CO_DOXYGEN
/**
 * Initialize Heartbeat consumer started callback function.
 *
 * Function initializes optional callback function, which is called for the
 * first received heartbeat after activating hb consumer or timeout.
 * Function may wake up external task, which handles this event.
 *
 * @param HBcons This object.
 * @param idx index of the node in HBcons object
 * @param object Pointer to object, which will be passed to pFunctSignal(). Can be NULL
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_HBconsumer_initCallbackHeartbeatStarted(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        void                   *object,
        void                  (*pFunctSignal)(uint8_t nodeId, uint8_t idx, void *object));

/**
 * Initialize Heartbeat consumer timeout callback function.
 *
 * Function initializes optional callback function, which is called when the node
 * state changes from active to timeout. Function may wake up external task,
 * which handles this event.
 *
 * @param HBcons This object.
 * @param idx index of the node in HBcons object
 * @param object Pointer to object, which will be passed to pFunctSignal(). Can be NULL
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_HBconsumer_initCallbackTimeout(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        void                   *object,
        void                  (*pFunctSignal)(uint8_t nodeId, uint8_t idx, void *object));

/**
 * Initialize Heartbeat consumer remote reset detected callback function.
 *
 * Function initializes optional callback function, which is called when a bootup
 * message is received from the remote node. Function may wake up external task,
 * which handles this event.
 *
 * @param HBcons This object.
 * @param idx index of the node in HBcons object
 * @param object Pointer to object, which will be passed to pFunctSignal(). Can be NULL
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_HBconsumer_initCallbackRemoteReset(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        void                   *object,
        void                  (*pFunctSignal)(uint8_t nodeId, uint8_t idx, void *object));
#endif /* (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI */

/**
 * Process Heartbeat consumer object.
 *
 * Function must be called cyclically.
 *
 * @param HBcons This object.
 * @param NMTisPreOrOperational True if this node is NMT_PRE_OPERATIONAL or NMT_OPERATIONAL.
 * @param timeDifference_us Time difference from previous function call in [microseconds].
 * @param [out] timerNext_us info to OS - see CO_process().
 */
void CO_HBconsumer_process(
        CO_HBconsumer_t        *HBcons,
        bool_t                  NMTisPreOrOperational,
        uint32_t                timeDifference_us,
        uint32_t               *timerNext_us);


#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_QUERY_FUNCT) || defined CO_DOXYGEN
/**
 * Get the heartbeat producer object index by node ID
 *
 * @param HBcons This object.
 * @param nodeId producer node ID
 * @return index. -1 if not found
 */
int8_t CO_HBconsumer_getIdxByNodeId(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 nodeId);

/**
 * Get the current state of a heartbeat producer by the index in OD 0x1016
 *
 * @param HBcons This object.
 * @param idx object sub index
 * @return #CO_HBconsumer_state_t
 */
CO_HBconsumer_state_t CO_HBconsumer_getState(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx);

/**
 * Get the current NMT state of a heartbeat producer by the index in OD 0x1016
 *
 * NMT state is only available when heartbeat is enabled for this index!
 *
 * @param HBcons This object.
 * @param idx object sub index
 * @param [out] nmtState of this index
 * @retval 0 NMT state has been received and is valid
 * @retval -1 not valid
 */
int8_t CO_HBconsumer_getNmtState(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        CO_NMT_internalState_t *nmtState);

#endif /* (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_QUERY_FUNCT */

/** @} */ /* CO_HBconsumer */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE */

#endif /* CO_HB_CONS_H */
