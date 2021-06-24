/**
 * CANopen Time-stamp protocol.
 *
 * @file        CO_TIME.h
 * @ingroup     CO_TIME
 * @author      Julien PEYREGNE
 * @copyright   2019 - 2020 Janez Paternoster
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

#ifndef CO_TIME_H
#define CO_TIME_H

#include "301/CO_driver.h"
#include "301/CO_ODinterface.h"
#include "301/CO_NMT_Heartbeat.h"


/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_TIME
#define CO_CONFIG_TIME (CO_CONFIG_TIME_ENABLE | \
                        CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE | \
                        CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC)
#endif

#if ((CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_TIME TIME
 * CANopen Time-stamp protocol.
 *
 * @ingroup CO_CANopen_301
 * @{
 * For CAN identifier see  @ref CO_Default_CAN_ID_t
 *
 * TIME message is used for time synchronization of the nodes on the network.
 * One node should be TIME producer, others can be TIME consumers. This is
 * configured by COB_ID_TIME object 0x1012:
 *
 * - bit 31 should be set for a consumer
 * - bit 30 should be set for a producer
 * - bits 0..10 is CAN-ID, 0x100 by default
 *
 * Current time can be read from @p CO_TIME_t->ms (milliseconds after midnight)
 * and @p CO_TIME_t->days (number of days since January 1, 1984). Those values
 * are updated on each @ref CO_TIME_process() call, either from internal timer
 * or from received time stamp message.
 *
 * Current time can be set with @ref CO_TIME_set() function, which is necessary
 * at least once, if time producer. If configured, time stamp message is
 * send from @ref CO_TIME_process() in intervals specified by @ref CO_TIME_set()
 */


/** Length of the TIME message */
#define CO_TIME_MSG_LENGTH 6


/**
 * TIME producer and consumer object.
 */
typedef struct {
    /** Received timestamp data */
    uint8_t timeStamp[CO_TIME_MSG_LENGTH];
    /** Milliseconds after midnight */
    uint32_t ms;
    /** Number of days since January 1, 1984 */
    uint16_t days;
    /** Residual microseconds calculated inside CO_TIME_process() */
    uint16_t residual_us;
	/** True, if device is TIME consumer. Calculated from _COB ID TIME Message_
    variable from Object dictionary (index 0x1012). */
    bool_t isConsumer;
    /** True, if device is TIME producer. Calculated from _COB ID TIME Message_
    variable from Object dictionary (index 0x1012). */
    bool_t isProducer;
    /** Variable indicates, if new TIME message received from CAN bus */
    volatile void *CANrxNew;
#if ((CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER) || defined CO_DOXYGEN
    /** Interval for time producer in milli seconds */
    uint32_t producerInterval_ms;
    /** Sync producer timer */
    uint32_t producerTimer_ms;
    /** From CO_TIME_init() */
    CO_CANmodule_t *CANdevTx;
    /** CAN transmit buffer */
    CO_CANtx_t *CANtxBuff;
#endif
#if ((CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_TIME_initCallbackPre() or NULL */
    void (*pFunctSignalPre)(void *object);
    /** From CO_TIME_initCallbackPre() or NULL */
    void *functSignalObjectPre;
#endif
#if ((CO_CONFIG_TIME) & CO_CONFIG_FLAG_OD_DYNAMIC) || defined CO_DOXYGEN
    /** Extension for OD object */
    OD_extension_t OD_1012_extension;
#endif
} CO_TIME_t;


/**
 * Initialize TIME object.
 *
 * Function must be called in the communication reset section.
 *
 * @param TIME This object will be initialized.
 * @param OD_1012_cobIdTimeStamp OD entry for 0x1012 - "COB-ID time stamp",
 * entry is required.
 * @param CANdevRx CAN device for TIME reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param CANdevTx CAN device for TIME transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO on success.
 */
CO_ReturnError_t CO_TIME_init(CO_TIME_t *TIME,
                              OD_entry_t *OD_1012_cobIdTimeStamp,
                              CO_CANmodule_t *CANdevRx,
                              uint16_t CANdevRxIdx,
#if ((CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER) || defined CO_DOXYGEN
                              CO_CANmodule_t *CANdevTx,
                              uint16_t CANdevTxIdx,
#endif
                              uint32_t *errInfo);


#if ((CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize TIME callback function.
 *
 * Function initializes optional callback function, which should immediately
 * start processing of CO_TIME_process() function.
 * Callback is called after TIME message is received from the CAN bus.
 *
 * @param TIME This object.
 * @param object Pointer to object, which will be passed to pFunctSignalPre().
 * @param pFunctSignalPre Pointer to the callback function. Not called if NULL.
 */
void CO_TIME_initCallbackPre(CO_TIME_t *TIME,
                             void *object,
                             void (*pFunctSignalPre)(void *object));
#endif


/**
 * Set current time
 *
 * @param TIME This object.
 * @param ms Milliseconds after midnight
 * @param days Number of days since January 1, 1984
 * @param producerInterval_ms Interval time for time producer in milliseconds
 */
static inline void CO_TIME_set(CO_TIME_t *TIME,
                               uint32_t ms,
                               uint16_t days,
                               uint32_t producerInterval_ms)
{
    (void)producerInterval_ms; /* may be unused */

    if (TIME != NULL) {
        TIME->residual_us = 0;
        TIME->ms = ms;
        TIME->days = days;
#if ((CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER)
        TIME->producerTimer_ms = TIME->producerInterval_ms =producerInterval_ms;
#endif
    }
}


/**
 * Process TIME object.
 *
 * Function must be called cyclically. It updates internal time from received
 * time stamp message or from timeDifference_us. It also sends produces
 * timestamp message, if producer and producerInterval_ms is set.
 *
 * @param TIME This object.
 * @param timeDifference_us Time difference from previous function call in
 * [microseconds].
 * @param NMTisPreOrOperational True if this node is NMT_PRE_OPERATIONAL or
 * NMT_OPERATIONAL state.
 *
 * @return True if new TIME stamp message recently received (consumer).
 */
bool_t CO_TIME_process(CO_TIME_t *TIME,
                       bool_t NMTisPreOrOperational,
                       uint32_t timeDifference_us);


/** @} */ /* CO_TIME */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE */

#endif /* CO_TIME_H */
