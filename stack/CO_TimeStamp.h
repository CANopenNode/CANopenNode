/**
 * CANopen TimeStamp object protocol.
 *
 * @file        CO_TimeStamp.c
 * @ingroup     CO_TimeStamp
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


#ifndef CO_TS_H
#define CO_TS_H

#include "CO_OD.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_TS TS
 * @ingroup CO_CANopen
 * @{
 *
 * CANopen TimeStamp object protocol.
 *
 * For CAN identifier see #CO_Default_CAN_ID_t
 *
 * TS message is used for time synchronization of the nodes on network. One node
 * should be TS producer, others can be TS consumers. 
 */
    
#define EMCY_MSG_LENGTH 6U

/**
 * TS producer and consumer object.
 */
typedef struct{
    CO_EM_t            *em;             /**< From CO_TS_init() */
    uint8_t            *operatingState; /**< From CO_TS_init() */
    uint16_t            COB_ID;         /**< From CO_TS_init() */
    /** TS period time in [milliseconds]. Set to TS period to enable 
    timeout detection */
    uint32_t            periodTime;
    /** TS period timeout time in [milliseconds].
    (periodTimeoutTime = periodTime * 1,5) */
    uint32_t            periodTimeoutTime;
    /** Variable indicates, if new TS message received from CAN bus */
    bool_t              CANrxNew;
    /** Timer for the TS message in [microseconds].
    Set to zero after received or transmitted TS message */
    uint32_t            timer;
    /** Set to nonzero value, if TS with wrong data length is received from CAN */
    uint16_t            receiveError;
    CO_CANmodule_t     *CANdevRx;       /**< From CO_TS_init() */
    uint16_t            CANdevRxIdx;    /**< From CO_TS_init() */
    TIME_OF_DAY         Time;
}CO_TS_t;

/**
 * Initialize TS object.
 *
 * Function must be called in the communication reset section.
 *
 * @param TS This object will be initialized.
 * @param em Emergency object.
 * @param SDO SDO server object.
 * @param operatingState Pointer to variable indicating CANopen device NMT internal state.
 * @param COB_ID_TSMessage Should be intialized with CO_CAN_ID_TIME_STAMP
 * @param TSCyclePeriod Set to TS period to enable timeout detection (1,5x period) or 0.
 * @param CANdevRx CAN device for TS reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_TS_init(
	CO_TS_t 		*TS, 
    CO_EM_t         *em,
	CO_SDO_t 		*SDO,
    uint8_t         *operatingState,
	uint32_t        COB_ID_TSMessage,
	uint32_t        TSCyclePeriod,
	CO_CANmodule_t  *CANdevRx,
	uint16_t        CANdevRxIdx);
	
/**
 * Process Timestamp communication.
 *
 * Function must be called cyclically.
 *
 * @param TS This object.
 * @param timeDifference_ms Time difference from previous function call in [milliseconds].
 *
 * @return 0: No special meaning.
 * @return 1: New TS message recently received.
 */
uint8_t CO_TS_process(
        CO_TS_t  *TS,
        uint32_t timeDifference_ms);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
