/**
 * CANopen Safety Related Data Object protocol.
 *
 * @file        CO_SRDO.h
 * @ingroup     CO_SRDO
 * @author      Robert Grüning
 * @copyright   2020 Robert Grüning
 * @copyright   2024 temi54c1l8@github
 * @copyright   2024 Janez Paternoster
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

#include "301/CO_Emergency.h"
#include "301/CO_ODinterface.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_SRDO
#define CO_CONFIG_SRDO (0)
#endif
#ifndef CO_CONFIG_SRDO_MINIMUM_DELAY
#define CO_CONFIG_SRDO_MINIMUM_DELAY 0U
#endif

#if ((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_SRDO SRDO
 * CANopen Safety Related Data Object protocol
 *
 * @ingroup CO_CANopen_304
 * @{
 * Safety Related Data Object protocol is specified by standard EN 50325-5:2010 (formerly CiA304).
 * Its functionality is very similar to that of the PDOs. The main differences is every message is send and received twice.
 * The second message must be bitwise inverted. The delay between the two messages and between each message pair is monitored.
 * The distinction between sending and receiving SRDO is made at runtime (for PDO it is compile time).
 * If the security protocol is used, at least one SRDO is mandatory.
 *
 * If there is erroneous structure of OD entries for SRDO parameters, then @CO_SRDO_init() function
 * returns error and CANopen device doesn't work. It is necessary to repair Object Dictionary and reprogram the device.
 *
 * If there are erroneous values inside SRDO parameters, then Emergency message CO_EM_SRDO_CONFIGURATION is sent.
 * Info code (32bit) contains OD index, subindex and additional byte, which helps to determine erroneous OD object.
 *
 * SRDO configuration consists of one @CO_SRDO_init_start(), @CO_SRDO_init() for each SRDO and one @CO_SRDO_init_end().
 * These may be called in CANopen initialization section after all other CANopen objects are initialized.
 * If SRDO OD parameters are edited (in NMT pre-operational state), NMT communication reset is necessary.
 * Alternatively SRDO configuration may be executed just after transition to NMT operational state.
 *
 * @CO_SRDO_process() must be executed cyclically, similar as PDO processing. Function is fast, no time consuming tasks.
 * Function returns @CO_SRDO_state_t value, which may be used to determine working-state or safe-state of safety related
 * device. If return values from all SRDO objects are >= CO_SRDO_state_communicationEstablished, then working state is allowed.
 * Otherwise SR device must be in safe state.
 *
 * Requirement for mapped objects:
 *  - @OD_attributes_t must have set bit ODA_RSRDO or ODA_RSRDO or ODA_TRSRDO (by CANopenEditor).
 */

/** Maximum size of SRDO message, 8 for standard CAN */
#ifndef CO_SRDO_MAX_SIZE
#define CO_SRDO_MAX_SIZE 8U
#endif

/** Maximum number of entries, which can be mapped to SRDO, 2*8 for standard
 * CAN, may be less to preserve RAM usage. Must be multiple of 2. */
#ifndef CO_SRDO_MAX_MAPPED_ENTRIES
#define CO_SRDO_MAX_MAPPED_ENTRIES 16U
#endif

#ifndef CO_SRDO_OWN_TYPES
/** Variable of type CO_SRDO_size_t contains data length in bytes of SRDO */
typedef uint8_t CO_SRDO_size_t;
#endif

/**
 * SRDO internal state
 */
typedef enum {
    CO_SRDO_state_error_internal = -10, /**< internal software error */
    CO_SRDO_state_error_configuration = -9, /**< error in parameters, emergency message was sent */
    CO_SRDO_state_error_txNotInverted = -6, /**< Transmitting SRDO messages was not inverted */
    CO_SRDO_state_error_txFail = -5, /**< SRDO CAN message transmission failed */
    CO_SRDO_state_error_rxTimeoutSRVT = -4, /**< SRDO message didn't receive inside SRVT time */
    CO_SRDO_state_error_rxTimeoutSCT = -3, /**< SRDO inverted message didn't receive inside SCT time */
    CO_SRDO_state_error_rxNotInverted = -2, /**< Received SRDO messages was not inverted */
    CO_SRDO_state_error_rxShort = -1, /**< Received SRDO message is too short */
    CO_SRDO_state_unknown = 0, /**< unknown state, set by @CO_SRDO_init */
    CO_SRDO_state_nmtNotOperational = 1, /**< Internal NMT operating state is not NMT operational */
    CO_SRDO_state_initializing = 2, /**< Just entered NMT operational state, SRDO message not yet received or transmitted */
    CO_SRDO_state_communicationEstablished = 3, /**< SRDO communication established, fully functional */
    CO_SRDO_state_deleted = 10 /**< informationDirection for this SRDO is set to 0 */
} CO_SRDO_state_t;

/**
 * Guard Object for SRDO.
 *
 * Guard object monitors all SRDO objects for:
 * - access to CRC objects
 * - access configuration valid flag
 * - change in operation state
 */
typedef struct {
    /** True if NMT operating state is operational */
    bool_t NMTisOperational;
    /** True if all SRDO objects are properly configured. Set after successful
     * finish of all @CO_SRDO_init() functions. Cleared on configuration change. */
    bool_t configurationValid;
    /** Private helper variable set on the start of SRDO configuration */
    bool_t _configurationValid;
    /** Object for input / output on the OD variable 13FE:00. Configuration
     * of any of the the SRDO parameters will write 0 to that variable. */
    OD_IO_t OD_IO_configurationValid;
    /** Extension for OD object */
    OD_extension_t OD_13FE_extension;
    /** Extension for OD object */
    OD_extension_t OD_13FF_extension;
} CO_SRDOGuard_t;

/**
 * SRDO object.
 */
typedef struct {
    CO_SRDOGuard_t* SRDOGuard; /**< From CO_SRDO_init() */
    CO_EM_t* em;               /**< From CO_SRDO_init() */
    uint16_t defaultCOB_ID;    /**< From CO_SRDO_init() */
    uint8_t nodeId;            /**< From CO_SRDO_init() */
    CO_CANmodule_t* CANdevTx[2];/**< From CO_SRDO_init() */
    /** Internal state of this SRDO. */
    CO_SRDO_state_t internalState;
    /** Copy of variable, internal usage. */
    bool_t NMTisOperationalPrevious;
    /** 0 - SRDO is disabled; 1 - SRDO is producer (tx); 2 - SRDO is consumer (rx) */
    uint8_t informationDirection;
    /** Safety Cycle Time from object dictionary translated to microseconds */
    uint32_t cycleTime_us;
    /** Safety related validation time from object dictionary translated to microseconds */
    uint32_t validationTime_us;
    /** cycle timer variable in microseconds */
    uint32_t cycleTimer;
    /** validation timer variable in microseconds */
    uint32_t validationTimer;
    /** Data length of the received SRDO message. Calculated from mapping */
    CO_SRDO_size_t dataLength;
    /** Number of mapped objects in SRDO */
    uint8_t mappedObjectsCount;
    /** Object dictionary interface for all mapped entries. OD_IO.dataOffset has
     * special usage with SRDO. It stores information about mappedLength of
     * the variable. mappedLength can be less or equal to the OD_IO.dataLength.
     * mappedLength greater than OD_IO.dataLength indicates erroneous mapping.
     * OD_IO.dataOffset is set to 0 before read/write function call and after
     * the call OD_IO.dataOffset is set back to mappedLength. */
    OD_IO_t OD_IO[CO_SRDO_MAX_MAPPED_ENTRIES];
    /** CAN transmit buffers inside CANdevTx */
    CO_CANtx_t* CANtxBuff[2];
    /** Variable indicates, if new SRDO message received from CAN bus. */
    volatile void* CANrxNew[2];
    /** true, if received SRDO is too short */
    bool_t rxSrdoShort;
    /** two buffers of data bytes for the received message. */
    uint8_t CANrxData[2][CO_SRDO_MAX_SIZE];
    /** If true, next processed SRDO message is normal (not inverted) */
    bool_t nextIsNormal;
    /** Extension for OD object */
    OD_extension_t OD_communicationParam_ext;
    /** Extension for OD object */
    OD_extension_t OD_mappingParam_extension;
#if ((CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_SRDO_initCallbackPre() or NULL */
    void (*pFunctSignalPre)(void* object);
    /** From CO_SRDO_initCallbackPre() or NULL */
    void* functSignalObjectPre;
#endif
} CO_SRDO_t;

/**
 * Initialize SRDOGuard object.
 *
 * Function must be called in the communication reset section before @CO_SRDO_init functions.
 *
 * @param SRDOGuard This object will be initialized.
 * @param OD_13FE_configurationValid Pointer to _Configuration valid_ variable from Object
 * dictionary (index 0x13FE).
 * @param OD_13FF_safetyConfigurationSignature Pointer to _Safety configuration signature_ variable
 * from Object dictionary (index 0x13FF).
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_SRDO_init_start(CO_SRDOGuard_t* SRDOGuard, OD_entry_t* OD_13FE_configurationValid,
                                    OD_entry_t* OD_13FF_safetyConfigurationSignature, uint32_t* errInfo);

/**
 * Finalize SRDOGuard object.
 *
 * Function must be called in the communication reset section after @CO_SRDO_init functions.
 *
 * @param SRDOGuard This object will be finalized.
 */
static inline void CO_SRDO_init_end(CO_SRDOGuard_t* SRDOGuard) {
    SRDOGuard->configurationValid = SRDOGuard->_configurationValid;
}

/**
 * Initialize SRDO object.
 *
 * Function must be called in the communication reset section.
 *
 * @param SRDO This object will be initialized.
 * @param SRDO_Index OD index of this SRDO, 0 for the first.
 * @param SRDOGuard SRDOGuard object.
 * @param OD CANopen Object Dictionary
 * @param em Emergency object.
 * @param nodeId CANopen Node ID of this device. If default COB_ID is used, value will be added.
 * @param defaultCOB_ID Default COB ID for this SRDO for plain data (without NodeId).
 * @param OD_130x_SRDOCommPar Pointer to _SRDO communication parameter_ record from Object
 * dictionary (index 0x1301+).
 * @param OD_138x_SRDOMapPar Pointer to _SRDO mapping parameter_ record from Object
 * dictionary (index 0x1381+).
 * @param OD_13FE_configurationValid Pointer to _Configuration valid_ variable from Object
 * dictionary (index 0x13FE).
 * @param OD_13FF_safetyConfigurationSignature Pointer to _Safety configuration signature_ variable
 * from Object dictionary (index 0x13FF).
 * @param CANdevRx CAN device used for SRDO reception.
 * @param CANdevRxIdxNormal Index of receive buffer in the above CAN device.
 * @param CANdevRxIdxInverted Index of receive buffer in the above CAN device.
 * @param CANdevTx CAN device used for SRDO transmission.
 * @param CANdevTxIdxNormal Index of transmit buffer in the above CAN device.
 * @param CANdevTxIdxInverted Index of transmit buffer in the above CAN device.
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT or CO_ERROR_OD_PARAMETERS.
 */
CO_ReturnError_t CO_SRDO_init(CO_SRDO_t* SRDO, uint8_t SRDO_Index, CO_SRDOGuard_t* SRDOGuard, OD_t* OD, CO_EM_t* em,
                              uint8_t nodeId, uint16_t defaultCOB_ID, OD_entry_t* OD_130x_SRDOCommPar,
                              OD_entry_t* OD_138x_SRDOMapPar, OD_entry_t* OD_13FE_configurationValid,
                              OD_entry_t* OD_13FF_safetyConfigurationSignature, CO_CANmodule_t* CANdevRxNormal,
                              CO_CANmodule_t* CANdevRxInverted, uint16_t CANdevRxIdxNormal, uint16_t CANdevRxIdxInverted,
                              CO_CANmodule_t* CANdevTxNormal, CO_CANmodule_t* CANdevTxInverted,
                              uint16_t CANdevTxIdxNormal, uint16_t CANdevTxIdxInverted, uint32_t* errInfo);

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
void CO_SRDO_initCallbackPre(CO_SRDO_t* SRDO, void* object, void (*pFunctSignalPre)(void* object));
#endif

/**
 * Send SRDO on event
 *
 * Sends SRDO before the next refresh timer tiggers. The message itself is send
 * in @CO_SRDO_process(). Note that RTOS have to trigger its processing quickly.
 * After the transmission the timer is reset to the full refresh time.
 *
 * @param SRDO This object.
 * @return CO_ReturnError_t CO_ERROR_NO if request is granted
 */
CO_ReturnError_t CO_SRDO_requestSend(CO_SRDO_t* SRDO);

/**
 * Process transmitting/receiving individual SRDO message.
 *
 * @param SRDO This object.
 * @param timeDifference_us Time difference from previous function call in [microseconds].
 * @param [out] timerNext_us info to OS, may be null.
 * @param NMTisOperational True if this node is in NMT_OPERATIONAL state.
 *
 * @return CO_SRDO_state_t internal state
 */
CO_SRDO_state_t CO_SRDO_process(CO_SRDO_t* SRDO, uint32_t timeDifference_us, uint32_t* timerNext_us, bool_t NMTisOperational);

/** @} */ /* CO_SRDO */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE */

#endif /* CO_SRDO_H */
