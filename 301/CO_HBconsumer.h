/**
 * CANopen Heartbeat consumer protocol.
 *
 * @file        CO_HBconsumer.h
 * @ingroup     CO_HBconsumer
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_HBconsumer Heartbeat consumer
 * @ingroup CO_CANopen_301
 * @{
 *
 * CANopen Heartbeat consumer protocol.
 *
 * Heartbeat consumer monitors Heartbeat messages from remote nodes. If any
 * monitored node don't send his Heartbeat in specified time, Heartbeat consumer
 * sends emergency message. If all monitored nodes are operational, then
 * variable _allMonitoredOperational_ inside CO_HBconsumer_t is set to true.
 * Monitoring starts after the reception of the first HeartBeat (not bootup).
 *
 * Heartbeat set up is done by writing to the OD registers 0x1016 or by using
 * the function _CO_HBconsumer_initEntry()_
 *
 * @see  @ref CO_NMT_Heartbeat
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
    /** Of the remote node (Heartbeat payload) */
    CO_NMT_internalState_t NMTstate;
    /** Current heartbeat state */
    CO_HBconsumer_state_t HBstate;
    /** Time since last heartbeat received */
    uint32_t timeoutTimer;
    /** Consumer heartbeat time from OD */
    uint32_t time_us;
    /** Indication if new Heartbeat message received from the CAN bus */
    volatile void *CANrxNew;
#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_HBconsumer_initCallbackPre() or NULL */
    void              (*pFunctSignalPre)(void *object);
    /** From CO_HBconsumer_initCallbackPre() or NULL */
    void               *functSignalObjectPre;
#endif
#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE) || defined CO_DOXYGEN
    /** Previous value of the remote node (Heartbeat payload) */
    CO_NMT_internalState_t NMTstatePrev;
#endif
#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI) || defined CO_DOXYGEN
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
typedef struct{
    CO_EM_t            *em;               /**< From CO_HBconsumer_init() */
    const uint32_t     *HBconsTime;       /**< From CO_HBconsumer_init() */
    CO_HBconsNode_t    *monitoredNodes;   /**< From CO_HBconsumer_init() */
    uint8_t             numberOfMonitoredNodes; /**< From CO_HBconsumer_init() */
    /** True, if all monitored nodes are active or no node is
        monitored. Can be read by the application */
    bool_t              allMonitoredActive;
    /** True, if all monitored nodes are NMT operational or no node is
        monitored. Can be read by the application */
    uint8_t             allMonitoredOperational;
    bool_t              NMTisPreOrOperationalPrev; /**< previous state of var */
    CO_CANmodule_t     *CANdevRx;         /**< From CO_HBconsumer_init() */
    uint16_t            CANdevRxIdxStart; /**< From CO_HBconsumer_init() */
#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE) || defined CO_DOXYGEN
    /** Callback for remote NMT changed event.
     *  From CO_HBconsumer_initCallbackNmtChanged() or NULL. */
    void (*pFunctSignalNmtChanged)(uint8_t nodeId,
                                   CO_NMT_internalState_t state,
                                   void *object);
    /** Pointer to object */
    void *pFunctSignalObjectNmtChanged;
#endif
}CO_HBconsumer_t;


/**
 * Initialize Heartbeat consumer object.
 *
 * Function must be called in the communication reset section.
 *
 * @param HBcons This object will be initialized.
 * @param em Emergency object.
 * @param SDO SDO server object.
 * @param HBconsTime Pointer to _Consumer Heartbeat Time_ array
 * from Object Dictionary (index 0x1016). Size of array is equal to numberOfMonitoredNodes.
 * @param monitoredNodes Pointer to the externaly defined array of the same size
 * as numberOfMonitoredNodes.
 * @param numberOfMonitoredNodes Total size of the above arrays.
 * @param CANdevRx CAN device for Heartbeat reception.
 * @param CANdevRxIdxStart Starting index of receive buffer in the above CAN device.
 * Number of used indexes is equal to numberOfMonitoredNodes.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_HBconsumer_init(
        CO_HBconsumer_t        *HBcons,
        CO_EM_t                *em,
        CO_SDO_t               *SDO,
        const uint32_t          HBconsTime[],
        CO_HBconsNode_t         monitoredNodes[],
        uint8_t                 numberOfMonitoredNodes,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdxStart);

/**
 * Initialize one Heartbeat consumer entry
 *
 * Calling this function has the same affect as writing to the corresponding
 * entries in the Object Dictionary (index 0x1016)
 * @remark The values in the Object Dictionary must be set manually by the
 * calling function so that heartbeat consumer behaviour matches the OD value.
 *
 * @param HBcons This object.
 * @param idx index of the node in HBcons object
 * @param nodeId see OD 0x1016 description
 * @param consumerTime_ms in milliseconds. see OD 0x1016 description
 * @return
 */
CO_ReturnError_t CO_HBconsumer_initEntry(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        uint8_t                 nodeId,
        uint16_t                consumerTime_ms);

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

#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE) || defined CO_DOXYGEN
/**
 * Initialize Heartbeat consumer NMT changed callback function.
 *
 * Function initializes optional callback function, which is called when NMT
 * state from the remote node changes.
 *
 * @param HBcons This object.
 * @param object Pointer to object, which will be passed to pFunctSignal().
 *               Can be NULL.
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_HBconsumer_initCallbackNmtChanged(
        CO_HBconsumer_t        *HBcons,
        void                   *object,
        void                  (*pFunctSignal)(uint8_t nodeId,
                                              CO_NMT_internalState_t state,
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

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
