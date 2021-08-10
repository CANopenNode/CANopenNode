/**
 * CANopen Process Data Object protocol.
 *
 * @file        CO_PDO.h
 * @ingroup     CO_PDO
 * @author      Janez Paternoster
 * @copyright   2021 Janez Paternoster
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

#ifndef CO_PDO_H
#define CO_PDO_H

#include "301/CO_ODinterface.h"
#include "301/CO_Emergency.h"
#include "301/CO_SYNC.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_PDO
#define CO_CONFIG_PDO (CO_CONFIG_RPDO_ENABLE | \
                       CO_CONFIG_TPDO_ENABLE | \
                       CO_CONFIG_RPDO_TIMERS_ENABLE | \
                       CO_CONFIG_TPDO_TIMERS_ENABLE | \
                       CO_CONFIG_PDO_SYNC_ENABLE | \
                       CO_CONFIG_PDO_OD_IO_ACCESS | \
                       CO_CONFIG_GLOBAL_RT_FLAG_CALLBACK_PRE | \
                       CO_CONFIG_GLOBAL_FLAG_TIMERNEXT | \
                       CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC)
#endif

#if ((CO_CONFIG_PDO) & (CO_CONFIG_RPDO_ENABLE | CO_CONFIG_TPDO_ENABLE)) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_PDO PDO
 * CANopen Process Data Object protocol.
 *
 * @ingroup CO_CANopen_301
 * @{
 * Process data objects are used for real-time data transfer with no protocol
 * overhead.
 *
 * TPDO with specific identifier is transmitted by one device and recieved by
 * zero or more devices as RPDO. PDO communication parameters(COB-ID,
 * transmission type, etc.) are in the Object Dictionary at index 0x1400+ and
 * 0x1800+. PDO mapping parameters (size and contents of the PDO) are in the
 * Object Dictionary at index 0x1600+ and 0x1A00+.
 *
 * Features of the PDO as implemented in CANopenNode:
 *  - Dynamic PDO mapping.
 *  - Map granularity of one byte.
 *  - Data from OD variables are accessed via @ref OD_IO_t read()/write()
 *    functions, which gives a great usefulness to the application.
 *  - For systems with very low memory and processing capabilities there is a
 *    simplified @ref CO_CONFIG_PDO option, where instead of read()/write()
 *    access, PDO data are copied directly to/from memory locations of
 *    OD variables.
 *  - After RPDO is received from CAN bus, its data are copied to internal
 *    buffer (inside fast CAN receive interrupt). Function CO_RPDO_process()
 *    (called by application) copies data to the mapped objects in the Object
 *    Dictionary. Synchronous RPDOs are processed AFTER reception of the next
 *    SYNC message.
 *  - Function CO_TPDO_process() (called by application) sends TPDO when
 *    necessary. There are different transmission types possible, controlled by:
 *    SYNC message, event timer, @ref CO_TPDOsendRequest() by application or
 *    @ref OD_requestTPDO(), where application can request TPDO for OD
 *    variable mapped to any of them. In later case application may, for
 *    example, monitor change of state of the OD variable and indicate TPDO
 *    request on it.
 *
 * @anchor CO_PDO_CAN_ID
 * ### CAN identifiers for PDO

 * Each PDO can be configured with any valid 11-bit CAN identifier. Lower
 * numbers have higher priorities on CAN bus. As a general rule, each CAN
 * message is identified with own CAN-ID, which must be unique and produced by
 * single source. The same is with PDO objects: Any TPDO produced on the CANopen
 * network must have unique CAN-ID and there can be zero to many RPDOs (from
 * different devices) configured to match the CAN-ID of the TPDO of interest.
 *
 * CANopen standard provides pre-defined connection sets for four RPDOs and four
 * TPDOs on each device with specific 7-bit Node-ID. These are default values
 * and are usable in configuration, where CANopen network contains a master
 * device, which directly communicates with many slaves. In de-centralized
 * systems, where devices operate without a master, it makes sense to configure
 * CAN-IDs of the RPDOs to the non-default values.
 *
 * Default CAN identifiers for first four TPDOs on device with specific CANopen
 * Node-Id are: 0x180+NodeId, 0x280+NodeId, 0x380+NodeId and 0x480+NodeId.
 *
 * Default CAN identifiers for first four RPDOs on device with specific CANopen
 * Node-Id are: 0x200+NodeId, 0x300+NodeId, 0x400+NodeId and 0x500+NodeId.
 *
 * CANopenNode handles default (pre-defined) CAN-IDs. If it is detected, that
 * PDO is configured with default CAN-ID (when writing to OD variable PDO
 * communication parameter, COB-ID), then COB-ID is stored without Node-Id to
 * the Object Dictionary. If Node-ID is changed, then COB-ID will always contain
 * correct default CAN-ID (default CAN-ID + Node-ID). If PDO is configured with
 * non-default CAN-ID, then it will be stored to the Object Dictionary as is.
 *
 * If configuration CO_CONFIG_FLAG_OD_DYNAMIC is enabled in @ref CO_CONFIG_PDO,
 * then PDOs can be configured dynamically, also in NMT operational state.
 * Otherwise PDOs are configured only in reset communication section and also
 * default CAN-IDs are always stored to OD as is, no default node-id is handled.
 *
 * Configure PDO by writing to the OD variables in the following procedure:
 * - Disable the PDO by setting bit-31 to 1 in PDO communication parameter,
 *   COB-ID
 * - Node-Id can be configured only when PDO is disabled.
 * - Disable mapping by setting PDO mapping parameter, sub index 0 to 0
 * - Configure mapping
 * - Enable mapping by setting PDO mapping param, sub 0 to number of mapped
 *   objects
 * - Enable the PDO by setting bit-31 to 0 in PDO communication parameter,
 *   COB-ID
 */

/** Maximum size of PDO message, 8 for standard CAN */
#ifndef CO_PDO_MAX_SIZE
#define CO_PDO_MAX_SIZE 8
#endif

/** Maximum number of entries, which can be mapped to PDO, 8 for standard CAN,
 * may be less to preserve RAM usage */
#ifndef CO_PDO_MAX_MAPPED_ENTRIES
#define CO_PDO_MAX_MAPPED_ENTRIES 8
#endif

/** Number of CANopen RPDO objects, which uses default CAN indentifiers.
 * By default first four RPDOs have pre-defined CAN identifiers, which depends
 * on node-id. This constant may be set to 0 to disable functionality or set
 * to any other value. For example, if there are several logical devices inside
 * single CANopen device, then more than four RPDOs may have pre-defined CAN
 * identifiers. In that case RPDO5 has CAN_ID=0x200+NodeId+1, RPDO6 has
 * CAN_ID=0x300+NodeId+1, RPDO9 has CAN_ID=0x200+NodeId+2 and so on. */
#ifndef CO_RPDO_DEFAULT_CANID_COUNT
#define CO_RPDO_DEFAULT_CANID_COUNT 4
#endif

/** Number of CANopen TPDO objects, which uses default CAN indentifiers.
 * If value is more than four, then pre-defined pre-defined CAN identifiers are:
 * TPDO5 has CAN_ID=0x180+NodeId+1, TPDO6 has CAN_ID=0x280+NodeId+1,
 * TPDO9 has CAN_ID=0x180+NodeId+2 and so on.
 * For description see @ref CO_RPDO_DEFAULT_CANID_COUNT. */
#ifndef CO_TPDO_DEFAULT_CANID_COUNT
#define CO_TPDO_DEFAULT_CANID_COUNT 4
#endif

#ifndef CO_PDO_OWN_TYPES
/** Variable of type CO_PDO_size_t contains data length in bytes of PDO */
typedef uint8_t CO_PDO_size_t;
#endif

/**
 * PDO transmission Types
 */
typedef enum {
    CO_PDO_TRANSM_TYPE_SYNC_ACYCLIC = 0, /**< synchronous (acyclic) */
    CO_PDO_TRANSM_TYPE_SYNC_1 = 1, /**< synchronous (cyclic every sync) */
    CO_PDO_TRANSM_TYPE_SYNC_240 = 0xF0, /**< synchronous (cyclic every 240-th
    sync) */
    CO_PDO_TRANSM_TYPE_SYNC_EVENT_LO = 0xFE, /**< event-driven, lower value
    (manufacturer specific),  */
    CO_PDO_TRANSM_TYPE_SYNC_EVENT_HI = 0xFF /**< event-driven, higher value
    (device profile and application profile specific) */
} CO_PDO_transmissionTypes_t;

/**
 * PDO object, common properties
 */
typedef struct {
    /** From CO_xPDO_init() */
    CO_EM_t *em;
    /** From CO_xPDO_init() */
    CO_CANmodule_t *CANdev;
    /** True, if PDO is enabled and valid */
    bool_t valid;
    /** Data length of the received PDO message. Calculated from mapping */
    CO_PDO_size_t dataLength;
    /** Number of mapped objects in PDO */
    uint8_t mappedObjectsCount;
#if ((CO_CONFIG_PDO) & CO_CONFIG_PDO_OD_IO_ACCESS) || defined CO_DOXYGEN
    /** Object dictionary interface for all mapped entries. OD_IO.dataOffset has
     * special usage with PDO. It stores information about mappedLength of
     * the variable. mappedLength can be less or equal to the OD_IO.dataLength.
     * mappedLength greater than OD_IO.dataLength indicates erroneous mapping.
     * OD_IO.dataOffset is set to 0 before read/write function call and after
     * the call OD_IO.dataOffset is set back to mappedLength. */
    OD_IO_t OD_IO[CO_PDO_MAX_MAPPED_ENTRIES];
  #if OD_FLAGS_PDO_SIZE > 0
    /** Pointer to byte, which contains PDO flag bit from @ref OD_extension_t */
    uint8_t *flagPDObyte[CO_PDO_MAX_MAPPED_ENTRIES];
    /** Bitmask for the flagPDObyte */
    uint8_t flagPDObitmask[CO_PDO_MAX_MAPPED_ENTRIES];
  #endif
#else
    /* Pointers to data objects inside OD, where PDO will be copied */
    uint8_t *mapPointer[CO_PDO_MAX_SIZE];
  #if OD_FLAGS_PDO_SIZE > 0
    uint8_t *flagPDObyte[CO_PDO_MAX_SIZE];
    uint8_t flagPDObitmask[CO_PDO_MAX_SIZE];
  #endif
#endif
#if ((CO_CONFIG_PDO) & CO_CONFIG_FLAG_OD_DYNAMIC) || defined CO_DOXYGEN
    /** True for RPDO, false for TPDO */
    bool_t isRPDO;
    /** From CO_xPDO_init() */
    OD_t *OD;
    /** From CO_xPDO_init() */
    uint16_t CANdevIdx;
    /** From CO_xPDO_init() */
    uint16_t preDefinedCanId;
    /** Currently configured CAN identifier */
    uint16_t configuredCanId;
    /** Extension for OD object */
    OD_extension_t OD_communicationParam_ext;
    /** Extension for OD object */
    OD_extension_t OD_mappingParam_extension;
#endif
} CO_PDO_common_t;


/*******************************************************************************
 *      R P D O
 ******************************************************************************/
#if ((CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE) || defined CO_DOXYGEN
/**
 * Number of buffers for received CAN message for RPDO
 */
#if ((CO_CONFIG_PDO) & CO_CONFIG_PDO_SYNC_ENABLE) || defined CO_DOXYGEN
#define CO_RPDO_CAN_BUFFERS_COUNT 2
#else
#define CO_RPDO_CAN_BUFFERS_COUNT 1
#endif


/**
 * RPDO object.
 */
typedef struct {
    /** PDO common properties, must be first element in this object */
    CO_PDO_common_t PDO_common;
    /** Variable indicates, if new PDO message received from CAN bus. */
    volatile void *CANrxNew[CO_RPDO_CAN_BUFFERS_COUNT];
    /** CO_PDO_MAX_SIZE data bytes of the received message. */
    uint8_t CANrxData[CO_RPDO_CAN_BUFFERS_COUNT][CO_PDO_MAX_SIZE];
    /** Indication of RPDO length errors, use with CO_PDO_receiveErrors_t */
    uint8_t receiveError;
#if ((CO_CONFIG_PDO) & CO_CONFIG_PDO_SYNC_ENABLE) || defined CO_DOXYGEN
    /** From CO_RPDO_init() */
    CO_SYNC_t *SYNC;
    /** True if transmissionType <= 240 */
    bool_t synchronous;
#endif
#if ((CO_CONFIG_PDO) & CO_CONFIG_RPDO_TIMERS_ENABLE) || defined CO_DOXYGEN
    /** Maximum timeout time between received PDOs in microseconds. Configurable
     * by OD variable RPDO communication parameter, event-timer. */
    uint32_t timeoutTime_us;
    /** Timeout timer variable in microseconds */
    uint32_t timeoutTimer;
#endif
#if ((CO_CONFIG_PDO) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_RPDO_initCallbackPre() or NULL */
    void (*pFunctSignalPre)(void *object);
    /** From CO_RPDO_initCallbackPre() or NULL */
    void *functSignalObjectPre;
#endif
} CO_RPDO_t;


/**
 * Initialize RPDO object.
 *
 * Function must be called in the end of the communication reset section, after
 * all application initialization. Otherwise mapping to application OD variables
 * will not be correct.
 *
 * @param RPDO This object will be initialized.
 * @param OD Object Dictionary.
 * @param em Emergency object.
 * @param SYNC SYNC object, may be NULL.
 * @param preDefinedCanId CAN identifier from pre-defined connection set,
 * including node-id for first four PDOs, or 0 otherwise, see @ref CO_PDO_CAN_ID
 * @param OD_14xx_RPDOCommPar OD entry for 0x1400+ - "RPDO communication
 * parameter", entry is required.
 * @param OD_16xx_RPDOMapPar OD entry for 0x1600+ - "RPDO mapping parameter",
 * entry is required.
 * @param CANdevRx CAN device for PDO reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO on success.
 */
CO_ReturnError_t CO_RPDO_init(CO_RPDO_t *RPDO,
                              OD_t *OD,
                              CO_EM_t *em,
#if ((CO_CONFIG_PDO) & CO_CONFIG_PDO_SYNC_ENABLE) || defined CO_DOXYGEN
                              CO_SYNC_t *SYNC,
#endif
                              uint16_t preDefinedCanId,
                              OD_entry_t *OD_14xx_RPDOCommPar,
                              OD_entry_t *OD_16xx_RPDOMapPar,
                              CO_CANmodule_t *CANdevRx,
                              uint16_t CANdevRxIdx,
                              uint32_t *errInfo);


#if ((CO_CONFIG_PDO) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize RPDO callback function.
 *
 * Function initializes optional callback function, which should immediately
 * start processing of CO_RPDO_process() function.
 * Callback is called after RPDO message is received from the CAN bus.
 *
 * @param RPDO This object.
 * @param object Pointer to object, which will be passed to pFunctSignalPre().
 * @param pFunctSignalPre Pointer to the callback function. Not called if NULL.
 */
void CO_RPDO_initCallbackPre(CO_RPDO_t *RPDO,
                             void *object,
                             void (*pFunctSignalPre)(void *object));
#endif


/**
 * Process received PDO messages.
 *
 * Function must be called cyclically in any NMT state. It copies data from RPDO
 * to Object Dictionary variables if: new PDO receives and PDO is valid and NMT
 * operating state is operational. Synchronous RPDOs are processed after next
 * SYNC message.
 *
 * @param RPDO This object.
 * @param timeDifference_us Time difference from previous function call.
 * @param [out] timerNext_us info to OS - see CO_process().
 * @param NMTisOperational True if this node is in NMT_OPERATIONAL state.
 * @param syncWas True, if CANopen SYNC message was just received or
 * transmitted.
 */
void CO_RPDO_process(CO_RPDO_t *RPDO,
#if ((CO_CONFIG_PDO) & CO_CONFIG_RPDO_TIMERS_ENABLE) || defined CO_DOXYGEN
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us,
#endif
                     bool_t NMTisOperational,
                     bool_t syncWas);
#endif /* (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE */


/*******************************************************************************
 *      T P D O
 ******************************************************************************/
#if ((CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE) || defined CO_DOXYGEN
/**
 * TPDO object.
 */
typedef struct {
    /** PDO common properties, must be first element in this object */
    CO_PDO_common_t PDO_common;
    /** CAN transmit buffer inside CANdev */
    CO_CANtx_t *CANtxBuff;
    /** Copy of the variable from object dictionary */
    uint8_t transmissionType;
    /** If this flag is set and TPDO is event driven (transmission type is 0,
     * 254 or 255), then PDO will be sent by CO_TPDO_process(). */
    bool_t sendRequest;
#if ((CO_CONFIG_PDO) & CO_CONFIG_PDO_SYNC_ENABLE) || defined CO_DOXYGEN
    /** From CO_TPDO_init() */
    CO_SYNC_t *SYNC;
    /** Copy of the variable from object dictionary */
    uint8_t syncStartValue;
    /** SYNC counter used for PDO sending */
    uint8_t syncCounter;
#endif
#if ((CO_CONFIG_PDO) & CO_CONFIG_TPDO_TIMERS_ENABLE) || defined CO_DOXYGEN
    /** Inhibit time from object dictionary translated to microseconds */
    uint32_t inhibitTime_us;
    /** Event time from object dictionary translated to microseconds */
    uint32_t eventTime_us;
    /** Inhibit timer variable in microseconds */
    uint32_t inhibitTimer;
    /** Event timer variable in microseconds */
    uint32_t eventTimer;
#endif
} CO_TPDO_t;


/**
 * Initialize TPDO object.
 *
 * Function must be called in the end of the communication reset section, after
 * all application initialization. Otherwise mapping to application OD variables
 * will not be correct.
 *
 * @param TPDO This object will be initialized.
 * @param OD Object Dictionary.
 * @param em Emergency object.
 * @param SYNC SYNC object, may be NULL.
 * @param preDefinedCanId CAN identifier from pre-defined connection set,
 * including node-id for first four PDOs, or 0 otherwise, see @ref CO_PDO_CAN_ID
 * @param OD_18xx_TPDOCommPar OD entry for 0x1800+ - "TPDO communication
 * parameter", entry is required.
 * @param OD_1Axx_TPDOMapPar OD entry for 0x1A00+ - "TPDO mapping parameter",
 * entry is required.
 * @param CANdevTx CAN device used for PDO transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO on success.
 */
CO_ReturnError_t CO_TPDO_init(CO_TPDO_t *TPDO,
                              OD_t *OD,
                              CO_EM_t *em,
#if ((CO_CONFIG_PDO) & CO_CONFIG_PDO_SYNC_ENABLE) || defined CO_DOXYGEN
                              CO_SYNC_t *SYNC,
#endif
                              uint16_t preDefinedCanId,
                              OD_entry_t *OD_18xx_TPDOCommPar,
                              OD_entry_t *OD_1Axx_TPDOMapPar,
                              CO_CANmodule_t *CANdevTx,
                              uint16_t CANdevTxIdx,
                              uint32_t *errInfo);


/**
 * Request transmission of TPDO message.
 *
 * If TPDO transmission type is 0, 254 or 255, then TPDO will be sent by
 * @ref CO_TPDO_process() after inhibit timer expires. See also
 * @ref OD_requestTPDO() and @ref OD_TPDOtransmitted().
 *
 * @param TPDO TPDO object.
 */
static inline void CO_TPDOsendRequest(CO_TPDO_t *TPDO) {
    if (TPDO != NULL) TPDO->sendRequest = true;
}


/**
 * Process transmitting PDO messages.
 *
 * Function must be called cyclically in any NMT state. It prepares and sends
 * TPDO if necessary.
 *
 * @param TPDO This object.
 * @param timeDifference_us Time difference from previous function call.
 * @param [out] timerNext_us info to OS - see CO_process().
 * @param NMTisOperational True if this node is in NMT_OPERATIONAL state.
 * @param syncWas True, if CANopen SYNC message was just received or
 * transmitted.
 */
void CO_TPDO_process(CO_TPDO_t *TPDO,
#if ((CO_CONFIG_PDO) & CO_CONFIG_TPDO_TIMERS_ENABLE) || defined CO_DOXYGEN
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us,
#endif
                     bool_t NMTisOperational,
                     bool_t syncWas);
#endif /* (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE */

/** @} */ /* CO_PDO */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* (CO_CONFIG_PDO) & (CO_CONFIG_RPDO_ENABLE | CO_CONFIG_TPDO_ENABLE) */

#endif /* CO_PDO_H */
