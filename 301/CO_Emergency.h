/**
 * CANopen Emergency protocol.
 *
 * @file        CO_Emergency.h
 * @ingroup     CO_Emergency
 * @author      Janez Paternoster
 * @copyright   2020 Janez Paternoster
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

#ifndef CO_EMERGENCY_H
#define CO_EMERGENCY_H

#include "301/CO_driver.h"
#include "301/CO_ODinterface.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_EM
#define CO_CONFIG_EM (CO_CONFIG_EM_PRODUCER | \
                      CO_CONFIG_EM_HISTORY | \
                      CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE | \
                      CO_CONFIG_GLOBAL_FLAG_TIMERNEXT)
#endif
#ifndef CO_CONFIG_EM_ERR_STATUS_BITS_COUNT
#define CO_CONFIG_EM_ERR_STATUS_BITS_COUNT (10*8)
#endif
#ifndef CO_CONFIG_ERR_CONDITION_GENERIC
#define CO_CONFIG_ERR_CONDITION_GENERIC (em->errorStatusBits[5] != 0)
#endif
#ifndef CO_CONFIG_ERR_CONDITION_COMMUNICATION
#define CO_CONFIG_ERR_CONDITION_COMMUNICATION (em->errorStatusBits[2] \
                                            || em->errorStatusBits[3])
#endif
#ifndef CO_CONFIG_ERR_CONDITION_MANUFACTURER
#define CO_CONFIG_ERR_CONDITION_MANUFACTURER (em->errorStatusBits[8] \
                                           || em->errorStatusBits[9])
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_Emergency Emergency
 * CANopen Emergency protocol.
 *
 * @ingroup CO_CANopen_301
 * @{
 * Error control and Emergency is used for control internal error state
 * and for sending a CANopen Emergency message.
 *
 * In case of error condition stack or application calls CO_errorReport()
 * function with indication of the error. Specific error condition is reported
 * (with CANopen Emergency message) only the first time after it occurs.
 * Internal state of specific error condition is indicated by internal bitfield
 * variable, with space for maximum @ref CO_CONFIG_EM_ERR_STATUS_BITS_COUNT
 * bits. Meaning for each bit is described by @ref CO_EM_errorStatusBits_t.
 * Specific error condition can be reset by CO_errorReset() function. In that
 * case Emergency message is sent with CO_EM_NO_ERROR indication.
 *
 * Some error conditions are informative and some are critical. Critical error
 * conditions set the corresponding bit in @ref CO_errorRegister_t. Critical
 * error conditions for generic error are specified by
 * @ref CO_CONFIG_ERR_CONDITION_GENERIC macro. Similar macros are defined for
 * other error bits in in @ref CO_errorRegister_t.
 *
 * ### Emergency producer
 * If @ref CO_CONFIG_EM has CO_CONFIG_EM_PRODUCER enabled, then CANopen
 * Emergency message will be sent on each change of any error condition.
 * Emergency message contents are:
 *
 *   Byte | Description
 *   -----|-----------------------------------------------------------
 *   0..1 | @ref CO_EM_errorCode_t
 *   2    | @ref CO_errorRegister_t
 *   3    | Index of error condition (see @ref CO_EM_errorStatusBits_t).
 *   4..7 | Additional informative argument to CO_errorReport() function.
 *
 * ### Error history
 * If @ref CO_CONFIG_EM has CO_CONFIG_EM_HISTORY enabled, then latest errors
 * can be read from _Pre Defined Error Field_ (object dictionary, index 0x1003).
 * Contents corresponds to bytes 0..3 from the Emergency message.
 *
 * ### Emergency consumer
 * If @ref CO_CONFIG_EM has CO_CONFIG_EM_CONSUMER enabled, then callback can be
 * registered by @ref CO_EM_initCallbackRx() function.
 */


/**
 * CANopen Error register.
 *
 * Mandatory for CANopen, resides in object dictionary, index 0x1001.
 *
 * Error register is calculated from internal bitfield variable, critical bits.
 * See @ref CO_EM_errorStatusBits_t and @ref CO_STACK_CONFIG_EMERGENCY for error
 * condition macros.
 *
 * Internal errors may prevent device to stay in NMT Operational state and
 * changes may switch between the states. See @ref CO_NMT_control_t for details.
 */
typedef enum {
    CO_ERR_REG_GENERIC_ERR   = 0x01U, /**< bit 0, generic error */
    CO_ERR_REG_CURRENT       = 0x02U, /**< bit 1, current */
    CO_ERR_REG_VOLTAGE       = 0x04U, /**< bit 2, voltage */
    CO_ERR_REG_TEMPERATURE   = 0x08U, /**< bit 3, temperature */
    CO_ERR_REG_COMMUNICATION = 0x10U, /**< bit 4, communication error */
    CO_ERR_REG_DEV_PROFILE   = 0x20U, /**< bit 5, device profile specific */
    CO_ERR_REG_RESERVED      = 0x40U, /**< bit 6, reserved (always 0) */
    CO_ERR_REG_MANUFACTURER  = 0x80U  /**< bit 7, manufacturer specific */
} CO_errorRegister_t;


/**
 * CANopen Error code
 *
 * Standard error codes according to CiA DS-301 and DS-401.
 */
typedef enum {
    /** 0x00xx, error Reset or No Error */
    CO_EMC_NO_ERROR = 0x0000U,
    /** 0x10xx, Generic Error */
    CO_EMC_GENERIC = 0x1000U,
    /** 0x20xx, Current */
    CO_EMC_CURRENT = 0x2000U,
    /** 0x21xx, Current, device input side */
    CO_EMC_CURRENT_INPUT = 0x2100U,
    /** 0x22xx, Current inside the device */
    CO_EMC_CURRENT_INSIDE = 0x2200U,
    /** 0x23xx, Current, device output side */
    CO_EMC_CURRENT_OUTPUT = 0x2300U,
    /** 0x30xx, Voltage */
    CO_EMC_VOLTAGE = 0x3000U,
    /** 0x31xx, Mains Voltage */
    CO_EMC_VOLTAGE_MAINS = 0x3100U,
    /** 0x32xx, Voltage inside the device */
    CO_EMC_VOLTAGE_INSIDE = 0x3200U,
    /** 0x33xx, Output Voltage */
    CO_EMC_VOLTAGE_OUTPUT = 0x3300U,
    /** 0x40xx, Temperature */
    CO_EMC_TEMPERATURE = 0x4000U,
    /** 0x41xx, Ambient Temperature */
    CO_EMC_TEMP_AMBIENT = 0x4100U,
    /** 0x42xx, Device Temperature */
    CO_EMC_TEMP_DEVICE = 0x4200U,
    /** 0x50xx, Device Hardware */
    CO_EMC_HARDWARE = 0x5000U,
    /** 0x60xx, Device Software */
    CO_EMC_SOFTWARE_DEVICE = 0x6000U,
    /** 0x61xx, Internal Software */
    CO_EMC_SOFTWARE_INTERNAL = 0x6100U,
    /** 0x62xx, User Software */
    CO_EMC_SOFTWARE_USER = 0x6200U,
    /** 0x63xx, Data Set */
    CO_EMC_DATA_SET = 0x6300U,
    /** 0x70xx, Additional Modules */
    CO_EMC_ADDITIONAL_MODUL = 0x7000U,
    /** 0x80xx, Monitoring */
    CO_EMC_MONITORING = 0x8000U,
    /** 0x81xx, Communication */
    CO_EMC_COMMUNICATION = 0x8100U,
    /** 0x8110, CAN Overrun (Objects lost) */
    CO_EMC_CAN_OVERRUN = 0x8110U,
    /** 0x8120, CAN in Error Passive Mode */
    CO_EMC_CAN_PASSIVE = 0x8120U,
    /** 0x8130, Life Guard Error or Heartbeat Error */
    CO_EMC_HEARTBEAT = 0x8130U,
    /** 0x8140, recovered from bus off */
    CO_EMC_BUS_OFF_RECOVERED = 0x8140U,
    /** 0x8150, CAN-ID collision */
    CO_EMC_CAN_ID_COLLISION = 0x8150U,
    /** 0x82xx, Protocol Error */
    CO_EMC_PROTOCOL_ERROR = 0x8200U,
    /** 0x8210, PDO not processed due to length error */
    CO_EMC_PDO_LENGTH = 0x8210U,
    /** 0x8220, PDO length exceeded */
    CO_EMC_PDO_LENGTH_EXC = 0x8220U,
    /** 0x8230, DAM MPDO not processed, destination object not available */
    CO_EMC_DAM_MPDO = 0x8230U,
    /** 0x8240, Unexpected SYNC data length */
    CO_EMC_SYNC_DATA_LENGTH = 0x8240U,
    /** 0x8250, RPDO timeout */
    CO_EMC_RPDO_TIMEOUT = 0x8250U,
    /** 0x90xx, External Error */
    CO_EMC_EXTERNAL_ERROR = 0x9000U,
    /** 0xF0xx, Additional Functions */
    CO_EMC_ADDITIONAL_FUNC = 0xF000U,
    /** 0xFFxx, Device specific */
    CO_EMC_DEVICE_SPECIFIC = 0xFF00U,

    /** 0x2310, DS401, Current at outputs too high (overload) */
    CO_EMC401_OUT_CUR_HI = 0x2310U,
    /** 0x2320, DS401, Short circuit at outputs */
    CO_EMC401_OUT_SHORTED = 0x2320U,
    /** 0x2330, DS401, Load dump at outputs */
    CO_EMC401_OUT_LOAD_DUMP = 0x2330U,
    /** 0x3110, DS401, Input voltage too high */
    CO_EMC401_IN_VOLT_HI = 0x3110U,
    /** 0x3120, DS401, Input voltage too low */
    CO_EMC401_IN_VOLT_LOW = 0x3120U,
    /** 0x3210, DS401, Internal voltage too high */
    CO_EMC401_INTERN_VOLT_HI = 0x3210U,
    /** 0x3220, DS401, Internal voltage too low */
    CO_EMC401_INTERN_VOLT_LO = 0x3220U,
    /** 0x3310, DS401, Output voltage too high */
    CO_EMC401_OUT_VOLT_HIGH = 0x3310U,
    /** 0x3320, DS401, Output voltage too low */
    CO_EMC401_OUT_VOLT_LOW = 0x3320U,
} CO_EM_errorCode_t;


/**
 * Error status bits
 *
 * Bits for internal indication of the error condition. Each error condition is
 * specified by unique index from 0x00 up to 0xFF.
 *
 * If specific error occurs in the stack or in the application, CO_errorReport()
 * sets specific bit in the _errorStatusBit_ variable from @ref CO_EM_t. If bit
 * was already set, function returns without any action. Otherwise it prepares
 * emergency message.
 *
 * Maximum size (in bits) of the _errorStatusBit_ variable is specified by
 * @ref CO_CONFIG_EM_ERR_STATUS_BITS_COUNT (set to 10*8 bits by default). Stack
 * uses first 6 bytes. Additional 4 bytes are pre-defined for manufacturer
 * or device specific error indications, by default.
 */
typedef enum {
    /** 0x00, Error Reset or No Error */
    CO_EM_NO_ERROR                  = 0x00U,
    /** 0x01, communication, info, CAN bus warning limit reached */
    CO_EM_CAN_BUS_WARNING           = 0x01U,
    /** 0x02, communication, info, Wrong data length of the received CAN
     * message */
    CO_EM_RXMSG_WRONG_LENGTH        = 0x02U,
    /** 0x03, communication, info, Previous received CAN message wasn't
     * processed yet */
    CO_EM_RXMSG_OVERFLOW            = 0x03U,
    /** 0x04, communication, info, Wrong data length of received PDO */
    CO_EM_RPDO_WRONG_LENGTH         = 0x04U,
    /** 0x05, communication, info, Previous received PDO wasn't processed yet */
    CO_EM_RPDO_OVERFLOW             = 0x05U,
    /** 0x06, communication, info, CAN receive bus is passive */
    CO_EM_CAN_RX_BUS_PASSIVE        = 0x06U,
    /** 0x07, communication, info, CAN transmit bus is passive */
    CO_EM_CAN_TX_BUS_PASSIVE        = 0x07U,
    /** 0x08, communication, info, Wrong NMT command received */
    CO_EM_NMT_WRONG_COMMAND         = 0x08U,
    /** 0x09, communication, info, TIME message timeout */
    CO_EM_TIME_TIMEOUT              = 0x09U,
    /** 0x0A, communication, info, (unused) */
    CO_EM_0A_unused                 = 0x0AU,
    /** 0x0B, communication, info, (unused) */
    CO_EM_0B_unused                 = 0x0BU,
    /** 0x0C, communication, info, (unused) */
    CO_EM_0C_unused                 = 0x0CU,
    /** 0x0D, communication, info, (unused) */
    CO_EM_0D_unused                 = 0x0DU,
    /** 0x0E, communication, info, (unused) */
    CO_EM_0E_unused                 = 0x0EU,
    /** 0x0F, communication, info, (unused) */
    CO_EM_0F_unused                 = 0x0FU,

    /** 0x10, communication, critical, (unused) */
    CO_EM_10_unused                 = 0x10U,
    /** 0x11, communication, critical, (unused) */
    CO_EM_11_unused                 = 0x11U,
    /** 0x12, communication, critical, CAN transmit bus is off */
    CO_EM_CAN_TX_BUS_OFF            = 0x12U,
    /** 0x13, communication, critical, CAN module receive buffer has
     * overflowed */
    CO_EM_CAN_RXB_OVERFLOW          = 0x13U,
    /** 0x14, communication, critical, CAN transmit buffer has overflowed */
    CO_EM_CAN_TX_OVERFLOW           = 0x14U,
    /** 0x15, communication, critical, TPDO is outside SYNC window */
    CO_EM_TPDO_OUTSIDE_WINDOW       = 0x15U,
    /** 0x16, communication, critical, (unused) */
    CO_EM_16_unused                 = 0x16U,
    /** 0x17, communication, critical, RPDO message timeout */
    CO_EM_RPDO_TIME_OUT             = 0x17U,
    /** 0x18, communication, critical, SYNC message timeout */
    CO_EM_SYNC_TIME_OUT             = 0x18U,
    /** 0x19, communication, critical, Unexpected SYNC data length */
    CO_EM_SYNC_LENGTH               = 0x19U,
    /** 0x1A, communication, critical, Error with PDO mapping */
    CO_EM_PDO_WRONG_MAPPING         = 0x1AU,
    /** 0x1B, communication, critical, Heartbeat consumer timeout */
    CO_EM_HEARTBEAT_CONSUMER        = 0x1BU,
    /** 0x1C, communication, critical, Heartbeat consumer detected remote node
     * reset */
    CO_EM_HB_CONSUMER_REMOTE_RESET  = 0x1CU,
    /** 0x1D, communication, critical, (unused) */
    CO_EM_1D_unused                 = 0x1DU,
    /** 0x1E, communication, critical, (unused) */
    CO_EM_1E_unused                 = 0x1EU,
    /** 0x1F, communication, critical, (unused) */
    CO_EM_1F_unused                 = 0x1FU,

    /** 0x20, generic, info, Emergency buffer is full, Emergency message wasn't
     * sent */
    CO_EM_EMERGENCY_BUFFER_FULL     = 0x20U,
    /** 0x21, generic, info, (unused) */
    CO_EM_21_unused                 = 0x21U,
    /** 0x22, generic, info, Microcontroller has just started */
    CO_EM_MICROCONTROLLER_RESET     = 0x22U,
    /** 0x23, generic, info, (unused) */
    CO_EM_23_unused                 = 0x23U,
    /** 0x24, generic, info, (unused) */
    CO_EM_24_unused                 = 0x24U,
    /** 0x25, generic, info, (unused) */
    CO_EM_25_unused                 = 0x25U,
    /** 0x26, generic, info, (unused) */
    CO_EM_26_unused                 = 0x26U,
    /** 0x27, generic, info, Automatic store to non-volatile memory failed */
    CO_EM_NON_VOLATILE_AUTO_SAVE    = 0x27U,

    /** 0x28, generic, critical, Wrong parameters to CO_errorReport() function*/
    CO_EM_WRONG_ERROR_REPORT        = 0x28U,
    /** 0x29, generic, critical, Timer task has overflowed */
    CO_EM_ISR_TIMER_OVERFLOW        = 0x29U,
    /** 0x2A, generic, critical, Unable to allocate memory for objects */
    CO_EM_MEMORY_ALLOCATION_ERROR   = 0x2AU,
    /** 0x2B, generic, critical, Generic error, test usage */
    CO_EM_GENERIC_ERROR             = 0x2BU,
    /** 0x2C, generic, critical, Software error */
    CO_EM_GENERIC_SOFTWARE_ERROR    = 0x2CU,
    /** 0x2D, generic, critical, Object dictionary does not match the software*/
    CO_EM_INCONSISTENT_OBJECT_DICT  = 0x2DU,
    /** 0x2E, generic, critical, Error in calculation of device parameters */
    CO_EM_CALCULATION_OF_PARAMETERS = 0x2EU,
    /** 0x2F, generic, critical, Error with access to non volatile device memory
     */
    CO_EM_NON_VOLATILE_MEMORY       = 0x2FU,

    /** 0x30+, manufacturer, info or critical, Error status buts, free to use by
     * manufacturer. By default bits 0x30..0x3F are set as informational and
     * bits 0x40..0x4F are set as critical. Manufacturer critical bits sets the
     * error register, as specified by @ref CO_CONFIG_ERR_CONDITION_MANUFACTURER
     */
    CO_EM_MANUFACTURER_START        = 0x30U,
    /** (@ref CO_CONFIG_EM_ERR_STATUS_BITS_COUNT - 1), largest value of the
     * Error status bit. */
    CO_EM_MANUFACTURER_END          = CO_CONFIG_EM_ERR_STATUS_BITS_COUNT - 1
} CO_EM_errorStatusBits_t;



#if ((CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)) \
    || defined CO_DOXYGEN
/**
 * Fifo buffer for emergency producer and error history
 */
typedef struct {
    uint32_t msg;
#if ((CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER) || defined CO_DOXYGEN
    uint32_t info;
#endif
} CO_EM_fifo_t;
#endif


/**
 * Emergency object.
 */
typedef struct {
    /** Bitfield for the internal indication of the error condition. */
    uint8_t errorStatusBits[CO_CONFIG_EM_ERR_STATUS_BITS_COUNT / 8];
    /** Pointer to error register in object dictionary at 0x1001,00. */
    uint8_t *errorRegister;
    /** Old CAN error status bitfield */
    uint16_t CANerrorStatusOld;
    /** From CO_EM_init() */
    CO_CANmodule_t *CANdevTx;

#if ((CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)) \
    || defined CO_DOXYGEN
    /** Internal circular FIFO buffer for storing pre-processed emergency
     * messages. Messages are added by @ref CO_error() function. All messages
     * are later post-processed by @ref CO_EM_process() function. In case of
     * overflow, error is indicated but emergency message is not sent. Fifo is
     * also used for error history, OD object 0x1003, "Pre-defined error field".
     * Buffer is defined by @ref CO_EM_init(). */
    CO_EM_fifo_t *fifo;
    /** Size of the above buffer, specified by @ref CO_EM_init(). */
    uint8_t fifoSize;
    /** Pointer for the fifo buffer, where next emergency message will be
     * written by @ref CO_error() function. */
    uint8_t fifoWrPtr;
    /** Pointer for the fifo, where next emergency message has to be
     * post-processed by @ref CO_EM_process() function. If equal to bufWrPtr,
     * then all messages has been post-processed. */
    uint8_t fifoPpPtr;
    /** Indication of overflow - messages in buffer are not post-processed */
    uint8_t fifoOverflow;
    /** Count of emergency messages in fifo, used for OD object 0x1003 */
    uint8_t fifoCount;
#endif /* (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY) */

#if ((CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER) || defined CO_DOXYGEN
    /** True, if emergency producer is enabled, from Object dictionary */
    bool_t producerEnabled;
    /** Copy of CANopen node ID, from CO_EM_init() */
    uint8_t nodeId;
    /** CAN transmit buffer */
    CO_CANtx_t *CANtxBuff;
    /** Extension for OD object */
    OD_extension_t OD_1014_extension;
 #if ((CO_CONFIG_EM) & CO_CONFIG_EM_PROD_CONFIGURABLE) || defined CO_DOXYGEN
    /** COB ID of emergency message, from Object dictionary */
    uint16_t producerCanId;
    /** From CO_EM_init() */
    uint16_t CANdevTxIdx;
 #endif
 #if ((CO_CONFIG_EM) & CO_CONFIG_EM_PROD_INHIBIT) || defined CO_DOXYGEN
     /** Inhibit time for emergency message, from Object dictionary */
    uint32_t inhibitEmTime_us;
    /**< Internal timer for inhibit time */
    uint32_t inhibitEmTimer;
    /** Extension for OD object */
    OD_extension_t OD_1015_extension;
 #endif
#endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER */

#if ((CO_CONFIG_EM) & CO_CONFIG_EM_HISTORY) || defined CO_DOXYGEN
    /** Extension for OD object */
    OD_extension_t OD_1003_extension;
#endif

#if ((CO_CONFIG_EM) & CO_CONFIG_EM_STATUS_BITS) || defined CO_DOXYGEN
    /** Extension for OD object */
    OD_extension_t OD_statusBits_extension;
#endif

#if ((CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER) || defined CO_DOXYGEN
    /** From CO_EM_initCallbackRx() or NULL */
    void (*pFunctSignalRx)(const uint16_t ident,
                           const uint16_t errorCode,
                           const uint8_t errorRegister,
                           const uint8_t errorBit,
                           const uint32_t infoCode);
#endif

#if ((CO_CONFIG_EM) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_EM_initCallbackPre() or NULL */
    void (*pFunctSignalPre)(void *object);
    /** From CO_EM_initCallbackPre() or NULL */
    void *functSignalObjectPre;
#endif
} CO_EM_t;


/**
 * Initialize Emergency object.
 *
 * Function must be called in the communication reset section.
 *
 * @param em This object will be initialized.
 * @param fifo Fifo buffer for emergency producer and error history. It must be
 * defined externally. Its size must be capacity+1. See also @ref CO_EM_t, fifo.
 * @param fifoSize Size of the above fifo buffer. It is usually equal to the
 * length of the OD array 0x1003 + 1. If fifoSize is smaller than 2, then
 * emergency producer and error history will not work and 'fifo' may be NULL.
 * @param CANdevTx CAN device for Emergency transmission.
 * @param OD_1001_errReg OD entry for 0x1001 - "Error register", entry is
 * required, without IO extension.
 * @param OD_1014_cobIdEm OD entry for 0x1014 - "COB-ID EMCY", entry is
 * required, IO extension is required.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 * @param OD_1015_InhTime OD entry for 0x1015 - "Inhibit time EMCY", entry is
 * optional (can be NULL), IO extension is optional for runtime configuration.
 * @param OD_1003_preDefErr OD entry for 0x1003 - "Pre-defined error field".
 * Emergency object has own memory buffer for this entry. Entry is optional,
 * IO extension is required.
 * @param OD_statusBits Custom OD entry for accessing errorStatusBits from
 * @ref CO_EM_t. Entry must have variable of size
 * (CO_CONFIG_EM_ERR_STATUS_BITS_COUNT/8) bytes available for read/write access
 * on subindex 0. Emergency object has own memory buffer for this entry. Entry
 * is optional, IO extension is required.
 * @param CANdevRx CAN device for Emergency consumer reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param nodeId CANopen node ID of this device (for default emergency producer)
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return @ref CO_ReturnError_t CO_ERROR_NO in case of success.
 */
CO_ReturnError_t CO_EM_init(CO_EM_t *em,
                            CO_CANmodule_t *CANdevTx,
                            const OD_entry_t *OD_1001_errReg,
#if ((CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)) \
    || defined CO_DOXYGEN
                            CO_EM_fifo_t *fifo,
                            uint8_t fifoSize,
#endif
#if ((CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER) || defined CO_DOXYGEN
                            OD_entry_t *OD_1014_cobIdEm,
                            uint16_t CANdevTxIdx,
 #if ((CO_CONFIG_EM) & CO_CONFIG_EM_PROD_INHIBIT) || defined CO_DOXYGEN
                            OD_entry_t *OD_1015_InhTime,
 #endif
#endif
#if ((CO_CONFIG_EM) & CO_CONFIG_EM_HISTORY) || defined CO_DOXYGEN
                            OD_entry_t *OD_1003_preDefErr,
#endif
#if ((CO_CONFIG_EM) & CO_CONFIG_EM_STATUS_BITS) || defined CO_DOXYGEN
                            OD_entry_t *OD_statusBits,
#endif
#if ((CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER) || defined CO_DOXYGEN
                            CO_CANmodule_t *CANdevRx,
                            uint16_t CANdevRxIdx,
#endif
                            const uint8_t nodeId,
                            uint32_t *errInfo);


#if ((CO_CONFIG_EM) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize Emergency callback function.
 *
 * Function initializes optional callback function, which should immediately
 * start processing of CO_EM_process() function.
 * Callback is called from CO_errorReport() or CO_errorReset() function. Those
 * functions are fast and may be called from any thread. Callback should
 * immediately start mainline thread, which calls CO_EM_process() function.
 *
 * @param em This object.
 * @param object Pointer to object, which will be passed to pFunctSignal(). Can
 * be NULL
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_EM_initCallbackPre(CO_EM_t *em,
                           void *object,
                           void (*pFunctSignal)(void *object));
#endif


#if ((CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER) || defined CO_DOXYGEN
/**
 * Initialize Emergency received callback function.
 *
 * Function initializes optional callback function, which executes after
 * error condition is received.
 *
 * _ident_ argument from callback contains CAN-ID of the emergency message. If
 * _ident_ == 0, then emergency message was sent from this device.
 *
 * @remark Depending on the CAN driver implementation, this function is called
 * inside an ISR or inside a mainline. Must be thread safe.
 *
 * @param em This object.
 * @param pFunctSignalRx Pointer to the callback function. Not called if NULL.
 */
void CO_EM_initCallbackRx(CO_EM_t *em,
                          void (*pFunctSignalRx)(const uint16_t ident,
                                                 const uint16_t errorCode,
                                                 const uint8_t errorRegister,
                                                 const uint8_t errorBit,
                                                 const uint32_t infoCode));
#endif


/**
 * Process Error control and Emergency object.
 *
 * Function must be called cyclically. It verifies some communication errors,
 * calculates OD object 0x1001 - "Error register" and sends emergency message
 * if necessary.
 *
 * @param em This object.
 * @param NMTisPreOrOperational True if this node is NMT_PRE_OPERATIONAL or
 * NMT_OPERATIONAL state.
 * @param timeDifference_us Time difference from previous function call in
 * [microseconds].
 * @param [out] timerNext_us info to OS - see CO_process().
 */
void CO_EM_process(CO_EM_t *em,
                   bool_t NMTisPreOrOperational,
                   uint32_t timeDifference_us,
                   uint32_t *timerNext_us);


/**
 * Set or reset error condition.
 *
 * Function can be called on any error condition inside CANopen stack or
 * application. Function first checks change of error condition (setError is
 * true and error bit wasn't set or setError is false and error bit was set
 * before). If changed, then Emergency message is prepared and record in history
 * is added. Emergency message is later sent by CO_EM_process() function.
 *
 * Function is short and thread safe.
 *
 * @param em Emergency object.
 * @param setError True if error occurred or false if error resolved.
 * @param errorBit from @ref CO_EM_errorStatusBits_t.
 * @param errorCode from @ref CO_EM_errorCode_t.
 * @param infoCode 32 bit value is passed to bytes 4...7 of the Emergency
 * message. It contains optional additional information.
 */
void CO_error(CO_EM_t *em, bool_t setError, const uint8_t errorBit,
              uint16_t errorCode, uint32_t infoCode);


/**
 * Report error condition, for description of parameters see @ref CO_error.
 */
#define CO_errorReport(em, errorBit, errorCode, infoCode) \
    CO_error(em, true, errorBit, errorCode, infoCode)


/**
 * Reset error condition, for description of parameters see @ref CO_error.
 */
#define CO_errorReset(em, errorBit, infoCode) \
    CO_error(em, false, errorBit, CO_EMC_NO_ERROR, infoCode)


/**
 * Check specific error condition.
 *
 * Function returns true, if specific internal error is present.
 *
 * @param em Emergency object.
 * @param errorBit from @ref CO_EM_errorStatusBits_t.
 *
 * @return true if Error is present.
 */
static inline bool_t CO_isError(CO_EM_t *em, const uint8_t errorBit) {
    uint8_t index = errorBit >> 3;
    uint8_t bitmask = 1 << (errorBit & 0x7);

    return (em == NULL || index >= (CO_CONFIG_EM_ERR_STATUS_BITS_COUNT / 8)
            || (em->errorStatusBits[index] & bitmask) != 0) ? true : false;
}

/**
 * Get error register
 *
 * @param em Emergency object.
 *
 * @return Error register or 0 if doesn't exist.
 */
static inline uint8_t CO_getErrorRegister(CO_EM_t *em) {
    return (em == NULL || em->errorRegister == NULL) ? 0 : *em->errorRegister;
}

/** @} */ /* CO_Emergency */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_EMERGENCY_H */
