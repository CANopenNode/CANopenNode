/**
 * CANopen Heartbeat consumer protocol.
 *
 * @file        CO_HBconsumer.h
 * @ingroup     CO_HBconsumer
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
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


#ifndef CO_HB_CONS_H
#define CO_HB_CONS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_HBconsumer Heartbeat consumer
 * @ingroup CO_CANopen
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
 * @see  @ref CO_NMT_Heartbeat
 */


/**
 * One monitored node inside CO_HBconsumer_t.
 */
typedef struct{
    uint8_t             NMTstate;       /**< Of the remote node */
    bool_t              monStarted;     /**< True after reception of the first Heartbeat mesage */
    uint16_t            timeoutTimer;   /**< Time since last heartbeat received */
    uint16_t            time;           /**< Consumer heartbeat time from OD */
    bool_t              CANrxNew;       /**< True if new Heartbeat message received from the CAN bus */
}CO_HBconsNode_t;


/**
 * Heartbeat consumer object.
 *
 * Object is initilaized by CO_HBconsumer_init(). It contains an array of
 * CO_HBconsNode_t objects.
 */
typedef struct{
    CO_EM_t            *em;             /**< From CO_HBconsumer_init() */
    const uint32_t     *HBconsTime;     /**< From CO_HBconsumer_init() */
    CO_HBconsNode_t    *monitoredNodes; /**< From CO_HBconsumer_init() */
    uint8_t             numberOfMonitoredNodes; /**< From CO_HBconsumer_init() */
    /** True, if all monitored nodes are NMT operational or no node is
        monitored. Can be read by the application */
    uint8_t             allMonitoredOperational;
    CO_CANmodule_t     *CANdevRx;       /**< From CO_HBconsumer_init() */
    uint16_t            CANdevRxIdxStart; /**< From CO_HBconsumer_init() */
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
 * Process Heartbeat consumer object.
 *
 * Function must be called cyclically.
 *
 * @param HBcons This object.
 * @param NMTisPreOrOperational True if this node is NMT_PRE_OPERATIONAL or NMT_OPERATIONAL.
 * @param timeDifference_ms Time difference from previous function call in [milliseconds].
 */
void CO_HBconsumer_process(
        CO_HBconsumer_t        *HBcons,
        bool_t                  NMTisPreOrOperational,
        uint16_t                timeDifference_ms);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
