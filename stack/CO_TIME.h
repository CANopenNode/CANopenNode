/**
 * CANopen TIME object protocol.
 *
 * @file        CO_TIME.c
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

#include "CO_OD.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_TIME TIME
 * @ingroup CO_CANopen
 * @{
 *
 * CANopen TIME object protocol.
 *
 * For CAN identifier see #CO_Default_CAN_ID_t
 *
 * TIME message is used for time synchronization of the nodes on network. One node
 * should be TIME producer, others can be TIME consumers. This is configured with
 * COB_ID_TIME object 0x1012 :
 *
 * - bit 31 should be set for a consumer
 * - bit 30 should be set for a producer
 *
 *
 * ###TIME CONSUMER
 *
 * CO_TIME_init() configuration :
 * - COB_ID_TIME : 0x80000100L -> TIME consumer with TIME_COB_ID = 0x100
 * - TIMECyclePeriod :
 *      - 0 -> no EMCY will be transmitted in case of TIME timeout
 *      - X -> an EMCY will be transmitted in case of TIME timeout (X * 1.5) ms
 *
 * Latest time value is stored in \p CO->TIME->Time variable.
 *
 *
 * ###TIME PRODUCER
 *
 * CO_TIME_init() configuration :
 * - COB_ID_TIME : 0x40000100L -> TIME producer with TIME_COB_ID = 0x100
 * - TIMECyclePeriod : Time transmit period in ms
 *
 * Write time value in \p CO->TIME->Time variable, this will be sent at TIMECyclePeriod.
 */

#define TIME_MSG_LENGTH 6U

/**
 * TIME producer and consumer object.
 */
typedef struct{
    CO_EM_t            *em;             /**< From CO_TIME_init() */
    uint8_t            *operatingState; /**< From CO_TIME_init() */
	/** True, if device is TIME consumer. Calculated from _COB ID TIME Message_
    variable from Object dictionary (index 0x1012). */
    bool_t              isConsumer;
	/** True, if device is TIME producer. Calculated from _COB ID TIME Message_
    variable from Object dictionary (index 0x1012). */
    bool_t              isProducer;
    uint16_t            COB_ID;         /**< From CO_TIME_init() */
    /** TIME period time in [milliseconds]. Set to TIME period to enable
    timeout detection */
    uint32_t            periodTime;
    /** TIME period timeout time in [milliseconds].
    (periodTimeoutTime = periodTime * 1,5) */
    uint32_t            periodTimeoutTime;
    /** Variable indicates, if new TIME message received from CAN bus */
    volatile void      *CANrxNew;
    /** Timer for the TIME message in [microseconds].
    Set to zero after received or transmitted TIME message */
    uint32_t            timer;
    /** Set to nonzero value, if TIME with wrong data length is received from CAN */
    uint16_t            receiveError;
    CO_CANmodule_t     *CANdevRx;       /**< From CO_TIME_init() */
    uint16_t            CANdevRxIdx;    /**< From CO_TIME_init() */
	CO_CANmodule_t     *CANdevTx;       /**< From CO_TIME_init() */
    uint16_t            CANdevTxIdx;    /**< From CO_TIME_init() */
    CO_CANtx_t         *TXbuff;         /**< CAN transmit buffer */
    TIME_OF_DAY         Time;
}CO_TIME_t;

/**
 * Initialize TIME object.
 *
 * Function must be called in the communication reset section.
 *
 * @param TIME This object will be initialized.
 * @param em Emergency object.
 * @param SDO SDO server object.
 * @param operatingState Pointer to variable indicating CANopen device NMT internal state.
 * @param COB_ID_TIMEMessage Should be intialized with CO_CAN_ID_TIME_STAMP
 * @param TIMECyclePeriod TIME period in ms (may also be used in consumer mode for timeout detection (1.5x period)).
 * @param CANdevRx CAN device for TIME reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_TIME_init(
        CO_TIME_t              *TIME,
        CO_EM_t                *em,
        CO_SDO_t               *SDO,
        uint8_t                *operatingState,
        uint32_t                COB_ID_TIMEMessage,
        uint32_t                TIMECyclePeriod,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx);

/**
 * Process TIME communication.
 *
 * Function must be called cyclically.
 *
 * @param TIME This object.
 * @param timeDifference_ms Time difference from previous function call in [milliseconds].
 *
 * @return 0: No special meaning.
 * @return 1: New TIME message recently received (consumer) / transmited (producer).
 */
uint8_t CO_TIME_process(
        CO_TIME_t              *TIME,
        uint32_t                timeDifference_ms);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
