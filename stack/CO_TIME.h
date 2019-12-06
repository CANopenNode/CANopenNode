/**
 * CANopen TIME object protocol.
 *
 * @file        CO_TIME.c
 * @ingroup     CO_TIME
 * @author      Julien PEYREGNE
 * @copyright   
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free and open source software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Following clarification and special exception to the GNU General Public
 * License is included to the distribution terms of CANopenNode:
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library. Thus, the terms and
 * conditions of the GNU General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this library give
 * you permission to link this library with independent modules to
 * produce an executable, regardless of the license terms of these
 * independent modules, and to copy and distribute the resulting
 * executable under terms of your choice, provided that you also meet,
 * for each linked independent module, the terms and conditions of the
 * license of that module. An independent module is a module which is
 * not derived from or based on this library. If you modify this
 * library, you may extend this exception to your version of the
 * library, but you are not obliged to do so. If you do not wish
 * to do so, delete this exception statement from your version.
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
	CO_TIME_t 		*TIME, 
    CO_EM_t         *em,
	CO_SDO_t 		*SDO,
    uint8_t         *operatingState,
	uint32_t        COB_ID_TIMEMessage,
	uint32_t        TIMECyclePeriod,
	CO_CANmodule_t  *CANdevRx,
	uint16_t        CANdevRxIdx,
    CO_CANmodule_t *CANdevTx,
    uint16_t        CANdevTxIdx);
	
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
        CO_TIME_t  *TIME,
        uint32_t timeDifference_ms);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
