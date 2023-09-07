/**
 * CANopen Node Guarding slave and master objects.
 *
 * @file        CO_Node_Guarding.h
 * @ingroup     CO_Node_Guarding
 * @author      Janez Paternoster
 * @copyright   2023 Janez Paternoster
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

#ifndef CO_NODE_GUARDING_H
#define CO_NODE_GUARDING_H

#if CO_NODE_GUARDING_SLAVE > 0

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_Node_Guarding Node Guarding
 * CANopen Node Guarding, an older alternative to the Heartbeat protocol.
 *
 * @ingroup CO_CANopen_301
 * @{
 * Node guarding master pools each node guarding slave at time intervals, called
 * guard time. Master sends a CAN RTR message, and slave responds. Slave also
 * monitors presence of RTR message from master and indicates error, if it
 * wasn't received within life time. ('Life time' is 'Guard time' multiplied by
 * 'Life time factor').
 *
 * Adding Node guarding to the project:
 *  - Make sure, driver supports it. RTR bit should be part of CAN identifier.
 *  - Enable it with 'CO_NODE_GUARDING_SLAVE 1'
 *    and/or 'CO_NODE_GUARDING_MASTER n_slots' macros, where n_slots is number
 *    of available slots for monitored nodes, between 1 and 127.
 *  - For slave add 0x100C and 0x100D objects to the Object dictionary.
 *  - For master use CO_nodeGuardingMaster_initNode() to add monitored nodes.
 *
 * @warning Usage of Node guarding is not recommended, as it is outdated and
 * uses RTR CAN functionality, which is also not recommended. Use Heartbeat and
 * Heartbeat consumer, if possible.
 *
 * ### Node Guarding slave response message contents:
 *
 *   Byte, bits     | Description
 *   ---------------|-----------------------------------------------------------
 *     0, bits 0..6 | @ref CO_NMT_internalState_t
 *     0, bit 7     | toggle bit
 *
 * See @ref CO_Default_CAN_ID_t for CAN identifiers.
 */

/**
 * Node Guarding slave object
 */
typedef struct {
    /** From CO_nodeGuardingSlave_init() */
    CO_EM_t *em;
    /** Indicates, if new rtr message received from CAN bus */
    volatile void *CANrxNew;
    /** Timer for monitoring Life time, counting down. */
    uint32_t lifeTimer;
    /** Toggle bit for response */
    uint8_t toggle;
    /** True if rtr from master is missing */
    uint8_t lifeTimeTimeout;
    /** From CO_nodeGuardingSlave_init() */
    CO_CANmodule_t *CANdevTx;
    /** CAN transmit buffer for the message */
    CO_CANtx_t *CANtxBuff;
} CO_nodeGuardingSlave_t;


/**
 * Initialize Node Guarding slave object.
 *
 * Function must be called in the communication reset section.
 *
 * @param ngs This object will be initialized.
 * @param em Emergency object.
 * @param CANidNodeGuarding CAN identifier for Node Guarding rtr and response
 * message (usually CO_CAN_ID_HEARTBEAT + nodeId).
 * @param CANdevRx CAN device for Node Guarding rtr reception.
 * @param CANdevRxIdx Index of the receive buffer in the above CAN device.
 * @param CANdevTx CAN device for Node Guarding response transmission.
 * @param CANdevTxIdx Index of the transmit buffer in the above CAN device.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO on success.
 */
CO_ReturnError_t CO_nodeGuardingSlave_init(CO_nodeGuardingSlave_t *ngs,
                                           CO_EM_t *em,
                                           uint16_t CANidNodeGuarding,
                                           CO_CANmodule_t *CANdevRx,
                                           uint16_t CANdevRxIdx,
                                           CO_CANmodule_t *CANdevTx,
                                           uint16_t CANdevTxIdx);


/**
 * Process Node Guarding slave.
 *
 * Function must be called cyclically.
 *
 * @param ngs This object.
 * @param NMTstate NMT operating state.
 * @param timeDifference_ms Time difference from previous function call in
 * milliseconds.
 * @param guardTime OD entry from 0x100C -"Guard time"
 * @param lifeTimeFactor OD entry from 0x100D -"Life time factor"
 */
void CO_nodeGuardingSlave_process(CO_nodeGuardingSlave_t *ngs,
                                  CO_NMT_internalState_t NMTstate,
                                  uint16_t timeDifference_ms,
                                  uint16_t guardTime,
                                  uint8_t lifeTimeFactor);


/**
 * Inquire, if Node guarding slave detected life time timeout
 *
 * Error is reset after pool request from master.
 *
 * @param ngs This object.
 *
 * @return true, if life time timeout was detected.
 */
static inline uint8_t CO_nodeGuardingSlave_isTimeout(CO_nodeGuardingSlave_t *ngs)
{
    return (ngs == NULL) || ngs->lifeTimeTimeout;
}


/** @} */ /* CO_Node_Guarding */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_NODE_GUARDING_SLAVE > 0 */




#if CO_NODE_GUARDING_MASTER > 0

#if CO_NODE_GUARDING_MASTER > 127
#error CO_NODE_GUARDING_MASTER value is wrong!
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup CO_Node_Guarding
 * @{
 */

/**
 * Node Guarding master - monitored node
 */
typedef struct {
    /** Guard time in milliseconds */
    uint16_t guardTime_ms;
    /** Guard timer in milliseconds, counting down */
    uint16_t guardTimer;
    /** CAN identifier (CO_CAN_ID_HEARTBEAT + Node Id) */
    uint16_t ident;
    /** NMT operating state */
    CO_NMT_internalState_t NMTstate;
    /** toggle bit7, expected from the next received message */
    uint8_t toggle;
    /** True, if response was received since last rtr message */
    uint8_t responseRecived;
    /** True, if CANtxBuff was busy since last processing */
    uint8_t CANtxWasBusy;
    /** True, if monitoring is active (response within time). */
    uint8_t monitoringActive;
} CO_nodeGuardingMasterNode_t;

/**
 * Node Guarding master object
 */
typedef struct {
    /** From CO_nodeGuardingMaster_init() */
    CO_EM_t *em;
    /** From CO_nodeGuardingMaster_init() */
    CO_CANmodule_t *CANdevTx;
    /** From CO_nodeGuardingMaster_init() */
    uint16_t CANdevTxIdx;
    /** CAN transmit buffer for the message */
    CO_CANtx_t *CANtxBuff;
    /** True, if all monitored nodes are active or no node is monitored. Can be
     * read by the application */
    uint8_t allMonitoredActive;
    /** True, if all monitored nodes are NMT operational or no node is
     * monitored. Can be read by the application */
    uint8_t allMonitoredOperational;
    /** Array of monitored nodes */
    CO_nodeGuardingMasterNode_t nodes[CO_NODE_GUARDING_MASTER];
} CO_nodeGuardingMaster_t;

/**
 * Initialize Node Guarding master object.
 *
 * Function must be called in the communication reset section.
 *
 * @param ngm This object will be initialized.
 * @param em Emergency object.
 * @param CANdevRx CAN device for Node Guarding reception.
 * @param CANdevRxIdx Index of the receive buffer in the above CAN device.
 * @param CANdevTx CAN device for Node Guarding rtr transmission.
 * @param CANdevTxIdx Index of the transmit buffer in the above CAN device.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO on success.
 */
CO_ReturnError_t CO_nodeGuardingMaster_init(CO_nodeGuardingMaster_t *ngm,
                                            CO_EM_t *em,
                                            CO_CANmodule_t *CANdevRx,
                                            uint16_t CANdevRxIdx,
                                            CO_CANmodule_t *CANdevTx,
                                            uint16_t CANdevTxIdx);

/**
 * Initialize node inside Node Guarding master object.
 *
 * Function may be called any time after CO_nodeGuardingMaster_init(). It
 * configures monitoring of the remote node.
 *
 * @param ngm Node Guarding master object.
 * @param em Emergency object.
 * @param index Index of the slot, which will be configured.
 * 0 <= index < CO_NODE_GUARDING_MASTER.
 * @param nodeId Node Id of the monitored node.
 * @param guardTime_ms Guard time of the monitored node.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO on success.
 */
CO_ReturnError_t CO_nodeGuardingMaster_initNode(CO_nodeGuardingMaster_t *ngm,
                                                uint8_t index,
                                                uint8_t nodeId,
                                                uint16_t guardTime_ms);

/**
 * Process Node Guarding master.
 *
 * Function must be called cyclically.
 *
 * @param ngm This object.
 * @param timeDifference_ms Time difference from previous function call in
 * milliseconds.
 */
void CO_nodeGuardingMaster_process(CO_nodeGuardingMaster_t *ngm,
                                   uint16_t timeDifference_ms);

/** @} */ /* @addtogroup CO_Node_Guarding */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_NODE_GUARDING_MASTER > 0 */

#endif /* CO_NODE_GUARDING_H */
