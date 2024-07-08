/**
 * CANopen Global fail-safe command protocol.
 *
 * @file        CO_GFC.h
 * @ingroup     CO_GFC
 * @author      Robert Grüning
 * @copyright   2020 Robert Grüning
 * @copyright   2024 Janez Paternoster
 *
 * This file is part of <https://github.com/CANopenNode/CANopenNode>, a CANopen Stack.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
 * file except in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and limitations under the License.
 */

#ifndef CO_GFC_H
#define CO_GFC_H

#include "301/CO_driver.h"
#include "301/CO_ODinterface.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_GFC
#define CO_CONFIG_GFC (0)
#endif

#if (((CO_CONFIG_GFC)&CO_CONFIG_GFC_ENABLE) != 0) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_GFC GFC
 * Global fail-safe command protocol.
 *
 * @ingroup CO_CANopen_304
 * @{
 * Very simple consumer/producer protocol. A net can have multiple GFC producer and multiple GFC consumer. On a
 * safety-relevant the producer can send a GFC message (ID 1, DLC 0). The consumer can use this message to start the
 * transition to a safe state. The GFC is optional for the security protocol and is not monitored (timed).
 */

/**
 * GFC object.
 */
typedef struct {
    bool_t valid;                   /**< From OD parameter 1300 */
    OD_extension_t OD_gfcParam_ext; /**< Extension for OD object */
#if (((CO_CONFIG_GFC)&CO_CONFIG_GFC_PRODUCER) != 0) || defined CO_DOXYGEN
    CO_CANmodule_t* CANdevTx; /**< From CO_GFC_init() */
    CO_CANtx_t* CANtxBuff;    /**< CAN transmit buffer inside CANdevTx */
#endif
#if (((CO_CONFIG_GFC)&CO_CONFIG_GFC_CONSUMER) != 0) || defined CO_DOXYGEN
    void (*pFunctSignalSafe)(void* object); /**< From CO_GFC_initCallbackEnterSafeState() or NULL */
    void* functSignalObjectSafe;            /**< From CO_GFC_initCallbackEnterSafeState() or NULL */
#endif
} CO_GFC_t;

/**
 * Initialize GFC object.
 *
 * Function must be called in the communication reset section.
 *
 * @param GFC This object will be initialized.
 * @param OD_1300_gfcParameter Pointer to _Global fail-safe command parameter_ variable from Object dictionary (index
 * 0x1300).
 * @param GFC_CANdevRx  CAN device used for SRDO reception.
 * @param GFC_rxIdx Index of receive buffer in the above CAN device.
 * @param CANidRxGFC GFC CAN ID for reception
 * @param GFC_CANdevTx AN device used for SRDO transmission.
 * @param GFC_txIdx Index of transmit buffer in the above CAN device.
 * @param CANidTxGFC GFC CAN ID for transmission
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_GFC_init(CO_GFC_t* GFC, OD_entry_t* OD_1300_gfcParameter, CO_CANmodule_t* GFC_CANdevRx,
                             uint16_t GFC_rxIdx, uint16_t CANidRxGFC, CO_CANmodule_t* GFC_CANdevTx, uint16_t GFC_txIdx,
                             uint16_t CANidTxGFC);

#if (((CO_CONFIG_GFC)&CO_CONFIG_GFC_CONSUMER) != 0) || defined CO_DOXYGEN
/**
 * Initialize GFC callback function.
 *
 * Function initializes optional callback function, that is called when GFC is received. Callback is called from receive
 * function (interrupt).
 *
 * @param GFC This object.
 * @param object Pointer to object, which will be passed to pFunctSignalSafe(). Can be NULL
 * @param pFunctSignalSafe Pointer to the callback function. Not called if NULL.
 */
void CO_GFC_initCallbackEnterSafeState(CO_GFC_t* GFC, void* object, void (*pFunctSignalSafe)(void* object));
#endif

#if (((CO_CONFIG_GFC)&CO_CONFIG_GFC_PRODUCER) != 0) || defined CO_DOXYGEN
/**
 * Send GFC message.
 *
 * It should be called by application, for example after a safety-relevant change.
 *
 * @param GFC GFC object.
 *
 * @return Same as CO_CANsend().
 */
CO_ReturnError_t CO_GFCsend(CO_GFC_t* GFC);
#endif

/** @} */ /* CO_GFC */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* (CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE */

#endif /* CO_GFC_H */
