/**
 * CANopen Safety Related Data Object protocol.
 *
 * @file        CO_SRDO.h
 * @ingroup     CO_SRDO
 * @author      Robert Grüning
 * @copyright   2020 - 2020 Robert Grüning
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

#ifndef CO_SRDO_H
#define CO_SRDO_H

#include "301/CO_driver.h"
#include "301/CO_SDOserver.h"
#include "301/CO_Emergency.h"
#include "301/CO_NMT_Heartbeat.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_SRDO
#define CO_CONFIG_SRDO (0)
#endif
#ifndef CO_CONFIG_SRDO_MINIMUM_DELAY
#define CO_CONFIG_SRDO_MINIMUM_DELAY 0
#endif

#if ((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_SRDO SRDO
 * CANopen Safety Related Data Object protocol.
 *
 * @ingroup CO_CANopen_304
 * @{
 * The functionality is very similar to that of the PDOs.
 * The main differences is every message is send and received twice.
 * The second message must be bitwise inverted. The delay between the two messages and between each message pair is monitored.
 * The distinction between sending and receiving SRDO is made at runtime (for PDO it is compile time).
 * If the security protocol is used, at least one SRDO is mandatory.
 */

/** Maximum size of SRDO message, 8 for standard CAN */
#ifndef CO_SRDO_MAX_SIZE
#define CO_SRDO_MAX_SIZE 8
#endif

/** Maximum number of entries, which can be mapped to PDO, 8 for standard CAN,
 * may be less to preserve RAM usage */
#ifndef CO_SRDO_MAX_MAPPED_ENTRIES
#define CO_SRDO_MAX_MAPPED_ENTRIES 8
#endif

#ifndef CO_SRDO_OWN_TYPES
/** Variable of type CO_SRDO_size_t contains data length in bytes of SRDO */
typedef uint8_t CO_SRDO_size_t;
#endif


/**
 * Gurad Object for SRDO
 * monitors:
 * - access to CRC objects
 * - access configuration valid flag
 * - change in operation state
 */
typedef struct{
    CO_NMT_internalState_t *operatingState;     /**< pointer to current operation state */
    CO_NMT_internalState_t  operatingStatePrev; /**< last operation state */
    uint8_t                *configurationValid; /**< pointer to the configuration valid flag in OD */
    uint8_t                 checkCRC;           /**< specifies whether a CRC check should be performed */
    
    /** Extension for OD object */
    OD_extension_t OD_13FE_extension;
    OD_extension_t OD_13FF_extension;
}CO_SRDOGuard_t;

/**
 * SRDO object.
 */
typedef struct{
    CO_EM_t                *em;                  /**< From CO_SRDO_init() */
    CO_SDOserver_t         *SDO;                 /**< From CO_SRDO_init() */
    CO_SRDOGuard_t         *SRDOGuard;           /**< From CO_SRDO_init() */
    /** Number of mapped objects in SRDO */
    uint8_t                 mappedObjectsCount;
    /** Pointers to 2*8 data objects, where SRDO will be copied */
    uint8_t                *mapPointer[2][CO_SRDO_MAX_SIZE];
    /** Data length of the received SRDO message. Calculated from mapping */
    CO_SRDO_size_t          dataLength;
    uint8_t                 nodeId;              /**< From CO_SRDO_init() */
    uint16_t                defaultCOB_ID[2];    /**< From CO_SRDO_init() */
    /** 0 - invalid, 1 - tx, 2 - rx */
    uint8_t                 valid;
    OD_entry_t *SRDOCommPar;         /**< From CO_SRDO_init() */
    OD_entry_t  *SRDOMapPar;          /**< From CO_SRDO_init() */
    const uint16_t         *checksum;            /**< From CO_SRDO_init() */
    CO_CANmodule_t         *CANdevRx;            /**< From CO_SRDO_init() */
    CO_CANmodule_t         *CANdevTx;            /**< From CO_SRDO_init() */
    CO_CANtx_t             *CANtxBuff[2];        /**< CAN transmit buffer inside CANdevTx */
    uint16_t                CANdevRxIdx[2];      /**< From CO_SRDO_init() */
    uint16_t                CANdevTxIdx[2];      /**< From CO_SRDO_init() */
    uint8_t                 toogle;              /**< defines the current state */
    uint32_t                timer;               /**< transmit timer and receive timeout */
    /** Variable indicates, if new SRDO message received from CAN bus. */
    volatile void          *CANrxNew[2];
    /** 2*8 data bytes of the received message. */
    uint8_t                 CANrxData[2][8];
    /** From CO_SRDO_initCallbackEnterSafeState() or NULL */
    void                  (*pFunctSignalSafe)(void *object);
    /** From CO_SRDO_initCallbackEnterSafeState() or NULL */
    void                   *functSignalObjectSafe;
#if ((CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_SRDO_initCallbackPre() or NULL */
    void                  (*pFunctSignalPre)(void *object);
    /** From CO_SRDO_initCallbackPre() or NULL */
    void                   *functSignalObjectPre;
#endif
    /** Extension for OD object */
    OD_extension_t OD_communicationParam_ext;
    OD_extension_t OD_mappingParam_extension;
    
    uint8_t             CommPar_informationDirection;
    uint16_t            CommPar_safetyCycleTime;
    uint8_t             CommPar_safetyRelatedValidationTime;
    uint8_t             CommPar_transmissionType;
    uint32_t            CommPar_COB_ID1_normal;
    uint32_t            CommPar_COB_ID2_inverted;
}CO_SRDO_t;

/**
 * Initialize SRDOGuard object.
 *
 * Function must be called in the communication reset section.
 *
 * @param SRDOGuard This object will be initialized.
 * @param SDO SDO object.
 * @param operatingState Pointer to variable indicating CANopen device NMT internal state.
 * @param configurationValid Pointer to variable with the SR valid flag
 * @param idx_SRDOvalid Index in Object Dictionary
 * @param idx_SRDOcrc Index in Object Dictionary
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_SRDOGuard_init(
        CO_SRDOGuard_t         *SRDOGuard,
        CO_SDOserver_t         *SDO,
        CO_NMT_internalState_t *operatingState,
        uint8_t                *configurationValid,
        OD_entry_t                *OD_13FE_SRDOValid, 
        OD_entry_t                *OD_13FF_SRDOCRC,
        uint32_t *errInfo);

/**
 * Process operation and valid state changes.
 *
 * @param SRDOGuard This object.
 * @return uint8_t command for CO_SRDO_process().
 * - bit 0 entered operational
 * - bit 1 validate checksum
 */
uint8_t CO_SRDOGuard_process(
        CO_SRDOGuard_t         *SRDOGuard);

/**
 * Initialize SRDO object.
 *
 * Function must be called in the communication reset section.
 *
 * @param SRDO This object will be initialized.
 * @param SRDOGuard SRDOGuard object.
 * @param em Emergency object.
 * @param SDO SDO object.
 * @param nodeId CANopen Node ID of this device. If default COB_ID is used, value will be added.
 * @param defaultCOB_ID Default COB ID for this SRDO (without NodeId).
 * @param SRDOCommPar Pointer to _SRDO communication parameter_ record from Object
 * dictionary (index 0x1301+).
 * @param SRDOMapPar Pointer to _SRDO mapping parameter_ record from Object
 * dictionary (index 0x1381+).
 * @param checksum
 * @param idx_SRDOCommPar Index in Object Dictionary
 * @param idx_SRDOMapPar Index in Object Dictionary
 * @param CANdevRx CAN device used for SRDO reception.
 * @param CANdevRxIdxNormal Index of receive buffer in the above CAN device.
 * @param CANdevRxIdxInverted Index of receive buffer in the above CAN device.
 * @param CANdevTx CAN device used for SRDO transmission.
 * @param CANdevTxIdxNormal Index of transmit buffer in the above CAN device.
 * @param CANdevTxIdxInverted Index of transmit buffer in the above CAN device.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_SRDO_init(
        CO_SRDO_t              *SRDO,
        CO_SRDOGuard_t         *SRDOGuard,
        CO_EM_t                *em,
        CO_SDOserver_t         *SDO,
        uint8_t                 nodeId,
        uint16_t                defaultCOB_ID,
        OD_entry_t             *SRDOCommPar,
        OD_entry_t             *SRDOMapPar,
        const uint16_t         *checksum,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdxNormal,
        uint16_t                CANdevRxIdxInverted,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdxNormal,
        uint16_t                CANdevTxIdxInverted,
        uint32_t *errInfo);

#if ((CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize SRDO callback function.
 *
 * Function initializes optional callback function, which should immediately
 * start processing of CO_SRDO_process() function.
 * Callback is called after SRDO message is received from the CAN bus.
 *
 * @param SRDO This object.
 * @param object Pointer to object, which will be passed to pFunctSignalPre(). Can be NULL
 * @param pFunctSignalPre Pointer to the callback function. Not called if NULL.
 */
void CO_SRDO_initCallbackPre(
        CO_SRDO_t              *SRDO,
        void                   *object,
        void                  (*pFunctSignalPre)(void *object));
#endif

/**
 * Initialize SRDO callback function.
 *
 * Function initializes optional callback function, that is called when SRDO enters a safe state.
 * This happens when a timeout is reached or the data is inconsistent. The safe state itself is not further defined.
 * One measure, for example, would be to go back to the pre-operational state
 * Callback is called from CO_SRDO_process().
 *
 * @param SRDO This object.
 * @param object Pointer to object, which will be passed to pFunctSignalSafe(). Can be NULL
 * @param pFunctSignalSafe Pointer to the callback function. Not called if NULL.
 */
void CO_SRDO_initCallbackEnterSafeState(
        CO_SRDO_t              *SRDO,
        void                   *object,
        void                  (*pFunctSignalSafe)(void *object));


/**
 * Send SRDO on event
 *
 * Sends SRDO before the next refresh timer tiggers. The message itself is send in CO_SRDO_process().
 * After the transmission the timer is reset to the full refresh time.
 *
 * @param SRDO This object.
 * @return CO_ReturnError_t CO_ERROR_NO if request is granted
 */
CO_ReturnError_t CO_SRDO_requestSend(
        CO_SRDO_t              *SRDO);

/**
 * Process transmitting/receiving SRDO messages.
 *
 *  This function verifies the checksum on demand.
 *  This function also configures the SRDO on operation state change to operational
 *
 * @param SRDO This object.
 * @param commands result from CO_SRDOGuard_process().
 * @param timeDifference_us Time difference from previous function call in [microseconds].
 * @param [out] timerNext_us info to OS.
 */
void CO_SRDO_process(
        CO_SRDO_t              *SRDO,
        uint8_t                 commands,
        uint32_t                timeDifference_us,
        uint32_t               *timerNext_us);

/** @} */ /* CO_SRDO */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE */

#endif /* CO_SRDO_H */
