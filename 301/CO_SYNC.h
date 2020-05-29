/**
 * CANopen Synchronisation protocol.
 *
 * @file        CO_SYNC.h
 * @ingroup     CO_SYNC
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


#ifndef CO_SYNC_H
#define CO_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_SYNC SYNC
 * @ingroup CO_CANopen_301
 * @{
 *
 * CANopen Synchronisation protocol.
 *
 * For CAN identifier see #CO_Default_CAN_ID_t
 *
 * SYNC message is used for synchronization of the nodes on network. One node
 * can be SYNC producer, others can be SYNC consumers. Synchronous TPDOs are
 * transmitted after the CANopen SYNC message. Synchronous received PDOs are
 * accepted(copied to OD) immediatelly after the reception of the next SYNC
 * message.
 *
 * ####Contents of SYNC message
 * By default SYNC message has no data. If _Synchronous counter overflow value_
 * from Object dictionary (index 0x1019) is different than 0, SYNC message has
 * one data byte: _counter_ incremented by 1 with every SYNC transmission.
 *
 * ####SYNC in CANopenNode
 * According to CANopen, synchronous RPDOs must be processed after reception of
 * the next sync messsage. For that reason, there is a double receive buffer
 * for each synchronous RPDO. At the moment, when SYNC is received or
 * transmitted, internal variable CANrxToggle toggles. That variable is then
 * used by synchronous RPDO to determine, which of the two buffers is used for
 * RPDO reception and which for RPDO processing.
 */


/**
 * SYNC producer and consumer object.
 */
typedef struct{
    CO_EM_t            *em;             /**< From CO_SYNC_init() */
    CO_NMT_internalState_t *operatingState; /**< From CO_SYNC_init() */
    /** True, if device is SYNC producer. Calculated from _COB ID SYNC Message_
    variable from Object dictionary (index 0x1005). */
    bool_t              isProducer;
    /** COB_ID of SYNC message. Calculated from _COB ID SYNC Message_
    variable from Object dictionary (index 0x1005). */
    uint16_t            COB_ID;
    /** Sync period time in [microseconds]. Calculated from _Communication cycle period_
    variable from Object dictionary (index 0x1006). */
    uint32_t            periodTime;
    /** Sync period timeout time in [microseconds].
    (periodTimeoutTime = periodTime * 1,5) */
    uint32_t            periodTimeoutTime;
    /** Value from _Synchronous counter overflow value_ variable from Object
    dictionary (index 0x1019) */
    uint8_t             counterOverflowValue;
    /** True, if current time is inside synchronous window.
    In this case synchronous PDO may be sent. */
    bool_t              curentSyncTimeIsInsideWindow;
    /** Indicates, if new SYNC message received from CAN bus */
    volatile void      *CANrxNew;
    /** Variable toggles, if new SYNC message received from CAN bus */
    bool_t              CANrxToggle;
    /** Counter of the SYNC message if counterOverflowValue is different than zero */
    uint8_t             counter;
    /** Timer for the SYNC message in [microseconds].
    Set to zero after received or transmitted SYNC message */
    uint32_t            timer;
    /** Set to nonzero value, if SYNC with wrong data length is received from CAN */
    uint16_t            receiveError;
#if ((CO_CONFIG_SYNC) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_SYNC_initCallbackPre() or NULL */
    void              (*pFunctSignalPre)(void *object);
    /** From CO_SYNC_initCallbackPre() or NULL */
    void               *functSignalObjectPre;
#endif
    CO_CANmodule_t     *CANdevRx;       /**< From CO_SYNC_init() */
    uint16_t            CANdevRxIdx;    /**< From CO_SYNC_init() */
    CO_CANmodule_t     *CANdevTx;       /**< From CO_SYNC_init() */
    CO_CANtx_t         *CANtxBuff;      /**< CAN transmit buffer inside CANdevTx */
    uint16_t            CANdevTxIdx;    /**< From CO_SYNC_init() */
}CO_SYNC_t;


/** Return value for #CO_SYNC_process */
typedef enum {
    CO_SYNC_NONE            = 0, /**< SYNC not received */
    CO_SYNC_RECEIVED        = 1, /**< SYNC received */
    CO_SYNC_OUTSIDE_WINDOW  = 2  /**< SYNC received outside SYNC window */
} CO_SYNC_status_t;


/**
 * Initialize SYNC object.
 *
 * Function must be called in the communication reset section.
 *
 * @param SYNC This object will be initialized.
 * @param em Emergency object.
 * @param SDO SDO server object.
 * @param operatingState Pointer to variable indicating CANopen device NMT internal state.
 * @param COB_ID_SYNCMessage From Object dictionary (index 0x1005).
 * @param communicationCyclePeriod From Object dictionary (index 0x1006).
 * @param synchronousCounterOverflowValue From Object dictionary (index 0x1019).
 * @param CANdevRx CAN device for SYNC reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param CANdevTx CAN device for SYNC transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_SYNC_init(
        CO_SYNC_t              *SYNC,
        CO_EM_t                *em,
        CO_SDO_t               *SDO,
        CO_NMT_internalState_t *operatingState,
        uint32_t                COB_ID_SYNCMessage,
        uint32_t                communicationCyclePeriod,
        uint8_t                 synchronousCounterOverflowValue,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx);


#if ((CO_CONFIG_SYNC) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize SYNC callback function.
 *
 * Function initializes optional callback function, which should immediately
 * start processing of CO_SYNC_process() function.
 * Callback is called after SYNC message is received from the CAN bus.
 *
 * @param SYNC This object.
 * @param object Pointer to object, which will be passed to pFunctSignalPre(). Can be NULL
 * @param pFunctSignalPre Pointer to the callback function. Not called if NULL.
 */
void CO_SYNC_initCallbackPre(
        CO_SYNC_t              *SYNC,
        void                   *object,
        void                  (*pFunctSignalPre)(void *object));
#endif


/**
 * Process SYNC communication.
 *
 * Function must be called cyclically.
 *
 * @param SYNC This object.
 * @param timeDifference_us Time difference from previous function call in [microseconds].
 * @param ObjDict_synchronousWindowLength _Synchronous window length_ variable from
 * Object dictionary (index 0x1007).
 * @param [out] timerNext_us info to OS - see CO_process_SYNC_PDO().
 *
 * @return #CO_SYNC_status_t: CO_SYNC_NONE, CO_SYNC_RECEIVED or CO_SYNC_OUTSIDE_WINDOW.
 */
CO_SYNC_status_t CO_SYNC_process(
        CO_SYNC_t              *SYNC,
        uint32_t                timeDifference_us,
        uint32_t                ObjDict_synchronousWindowLength,
        uint32_t               *timerNext_us);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
