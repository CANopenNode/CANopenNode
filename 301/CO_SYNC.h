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

#include "301/CO_driver.h"
#include "301/CO_ODinterface.h"
#include "301/CO_Emergency.h"


/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_SYNC
#define CO_CONFIG_SYNC (CO_CONFIG_SYNC_ENABLE | \
                        CO_CONFIG_SYNC_PRODUCER | \
                        CO_CONFIG_GLOBAL_RT_FLAG_CALLBACK_PRE | \
                        CO_CONFIG_GLOBAL_FLAG_TIMERNEXT | \
                        CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC)
#endif

#if ((CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_SYNC SYNC
 * CANopen Synchronisation protocol.
 *
 * @ingroup CO_CANopen_301
 * @{
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
typedef struct {
    /** From CO_SYNC_init() */
    CO_EM_t *em;
    /** Indicates, if new SYNC message received from CAN bus */
    volatile void *CANrxNew;
    /** Set to nonzero value, if SYNC with wrong data length is received */
    uint8_t receiveError;
    /** Variable toggles, if new SYNC message received from CAN bus */
    bool_t CANrxToggle;
    /** Sync timeout monitoring: 0 = not started; 1 = started; 2 = sync timeout
     * error state */
    uint8_t timeoutError;
    /** Value from _Synchronous counter overflow value_ variable from Object
    dictionary (index 0x1019) */
    uint8_t counterOverflowValue;
    /** Counter of the SYNC message if counterOverflowValue is different than
     * zero */
    uint8_t counter;
    /** True, if current time is outside "synchronous window" (OD 1007) */
    bool_t syncIsOutsideWindow;
    /** Timer for the SYNC message in [microseconds].
    Set to zero after received or transmitted SYNC message */
    uint32_t timer;
    /**Pointer to variable in OD, "Communication cycle period" in microseconds*/
    uint32_t *OD_1006_period;
    /** Pointer to variable in OD, "Synchronous window length" in microseconds*/
    uint32_t *OD_1007_window;

#if ((CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER) || defined CO_DOXYGEN
    /** True, if device is SYNC producer. Calculated from _COB ID SYNC Message_
    variable from Object dictionary (index 0x1005). */
    bool_t isProducer;
    /** CAN transmit buffer inside CANdevTx */
    CO_CANtx_t *CANtxBuff;
#endif

#if ((CO_CONFIG_SYNC) & CO_CONFIG_FLAG_OD_DYNAMIC) || defined CO_DOXYGEN
    /** From CO_SYNC_init() */
    CO_CANmodule_t *CANdevRx;
    /** From CO_SYNC_init() */
    uint16_t CANdevRxIdx;
    /** Extension for OD object */
    OD_extension_t OD_1005_extension;
    /** CAN ID of the SYNC message. Calculated from _COB ID SYNC Message_
    variable from Object dictionary (index 0x1005). */
    uint16_t CAN_ID;
 #if ((CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER) || defined CO_DOXYGEN
    /** From CO_SYNC_init() */
    CO_CANmodule_t *CANdevTx;
    /** From CO_SYNC_init() */
    uint16_t CANdevTxIdx;
    /** Extension for OD object */
    OD_extension_t OD_1019_extension;
 #endif
#endif

#if ((CO_CONFIG_SYNC) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_SYNC_initCallbackPre() or NULL */
    void (*pFunctSignalPre)(void *object);
    /** From CO_SYNC_initCallbackPre() or NULL */
    void *functSignalObjectPre;
#endif
} CO_SYNC_t;


/** Return value for @ref CO_SYNC_process */
typedef enum {
    /** No SYNC event in last cycle */
    CO_SYNC_NONE = 0,
    /** SYNC message was received or transmitted in last cycle */
    CO_SYNC_RX_TX = 1,
    /** Time has just passed SYNC window (OD_1007) in last cycle */
    CO_SYNC_PASSED_WINDOW = 2
} CO_SYNC_status_t;


/**
 * Initialize SYNC object.
 *
 * Function must be called in the communication reset section.
 *
 * @param SYNC This object will be initialized.
 * @param em Emergency object.
 * @param OD_1005_cobIdSync OD entry for 0x1005 - "COB-ID SYNC message",
 * entry is required.
 * @param OD_1006_commCyclePeriod OD entry for 0x1006 - "Communication cycle
 * period", entry is required if device is sync producer.
 * @param OD_1007_syncWindowLen OD entry for 0x1007 - "Synchronous window
 * length", entry is optional, may be NULL.
 * @param OD_1019_syncCounterOvf OD entry for 0x1019 - "Synchronous counter
 * overflow value", entry is optional, may be NULL.
 * @param CANdevRx CAN device for SYNC reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param CANdevTx CAN device for SYNC transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO on success.
 */
CO_ReturnError_t CO_SYNC_init(CO_SYNC_t *SYNC,
                              CO_EM_t *em,
                              OD_entry_t *OD_1005_cobIdSync,
                              OD_entry_t *OD_1006_commCyclePeriod,
                              OD_entry_t *OD_1007_syncWindowLen,
                              OD_entry_t *OD_1019_syncCounterOvf,
                              CO_CANmodule_t *CANdevRx,
                              uint16_t CANdevRxIdx,
#if ((CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER) || defined CO_DOXYGEN
                              CO_CANmodule_t *CANdevTx,
                              uint16_t CANdevTxIdx,
#endif
                              uint32_t *errInfo);


#if ((CO_CONFIG_SYNC) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize SYNC callback function.
 *
 * Function initializes optional callback function, which should immediately
 * start processing of CO_SYNC_process() function.
 * Callback is called after SYNC message is received from the CAN bus.
 *
 * @param SYNC This object.
 * @param object Pointer to object, which will be passed to pFunctSignalPre().
 * @param pFunctSignalPre Pointer to the callback function. Not called if NULL.
 */
void CO_SYNC_initCallbackPre(CO_SYNC_t *SYNC,
                             void *object,
                             void (*pFunctSignalPre)(void *object));
#endif


#if ((CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER) || defined CO_DOXYGEN
/**
 * Send SYNC message.
 *
 * This function prepares and sends a SYNC object. The application should only
 * call this if direct control of SYNC transmission is needed, otherwise use
 * CO_SYNC_process().
 *
 * @param SYNC SYNC object.
 *
 * @return Same as CO_CANsend().
 */
static inline CO_ReturnError_t CO_SYNCsend(CO_SYNC_t *SYNC) {
    if (++SYNC->counter > SYNC->counterOverflowValue) SYNC->counter = 1;
    SYNC->timer = 0;
    SYNC->CANrxToggle = SYNC->CANrxToggle ? false : true;
    SYNC->CANtxBuff->data[0] = SYNC->counter;
    return CO_CANsend(SYNC->CANdevTx, SYNC->CANtxBuff);
}
#endif


/**
 * Process SYNC communication.
 *
 * Function must be called cyclically.
 *
 * @param SYNC This object.
 * @param NMTisPreOrOperational True if this node is NMT_PRE_OPERATIONAL or
 * NMT_OPERATIONAL state.
 * @param timeDifference_us Time difference from previous function call in
 * [microseconds].
 * @param [out] timerNext_us info to OS - see CO_process().
 *
 * @return @ref CO_SYNC_status_t
 */
CO_SYNC_status_t CO_SYNC_process(CO_SYNC_t *SYNC,
                                 bool_t NMTisPreOrOperational,
                                 uint32_t timeDifference_us,
                                 uint32_t *timerNext_us);

/** @} */ /* CO_SYNC */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE */

#endif /* CO_SYNC_H */
