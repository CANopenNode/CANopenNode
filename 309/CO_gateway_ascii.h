/**
 * CANopen access from other networks - ASCII mapping (CiA 309-3 DS v3.0.0)
 *
 * @file        CO_gateway_ascii.h
 * @ingroup     CO_CANopen_309_3
 * @author      Janez Paternoster
 * @author      Martin Wagner
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

#ifndef CO_GATEWAY_ASCII_H
#define CO_GATEWAY_ASCII_H

#include "301/CO_driver.h"
#include "301/CO_fifo.h"
#include "301/CO_SDOclient.h"
#include "301/CO_NMT_Heartbeat.h"
#include "305/CO_LSSmaster.h"
#include "303/CO_LEDs.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_GTW
#define CO_CONFIG_GTW (0)
#endif

#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_CANopen_309_3 Gateway ASCII mapping
 * CANopen access from other networks - ASCII mapping (CiA 309-3 DSP v3.0.0)
 *
 * @ingroup CO_CANopen_309
 * @{
 * This module enables ascii command interface (CAN gateway), which can be used
 * for master interaction with CANopen network. Some sort of string input/output
 * stream can be used, for example serial port + terminal on microcontroller or
 * stdio in OS or sockets, etc.
 *
 * For example, one wants to read 'Heartbeat producer time' parameter (0x1017,0)
 * on remote node (with id=4). Parameter is 16-bit integer. He can can enter
 * command string: `[1] 4 read 0x1017 0 i16`. CANopenNode will use SDO client,
 * send request to remote node via CAN, wait for response via CAN and prints
 * `[1] OK` to output stream on success.
 *
 * This module is usually initialized and processed in CANopen.c file.
 * Application should register own callback function for reading the output
 * stream. Application writes new commands with CO_GTWA_write().
 */

/**
 * @defgroup CO_CANopen_309_3_Syntax Command syntax
 * ASCII command syntax.
 *
 * @{
 *
 * @code{.unparsed}
Command strings start with '"["<sequence>"]"' followed by:
[[<net>] <node>] r[ead] <index> <subindex> [<datatype>]        # SDO upload.
[[<net>] <node>] w[rite] <index> <subindex> <datatype> <value> # SDO download.

[[<net>] <node>] start                   # NMT Start node.
[[<net>] <node>] stop                    # NMT Stop node.
[[<net>] <node>] preop[erational]        # NMT Set node to pre-operational.
[[<net>] <node>] reset node              # NMT Reset node.
[[<net>] <node>] reset comm[unication]   # NMT Reset communication.

[<net>] set network <value>              # Set default net.
[<net>] set node <value>                 # Set default node.
[<net>] set sdo_timeout <value>          # Configure SDO time-out.
[<net>] set sdo_block <value>            # Enable/disable SDO block transfer.

help [datatype|lss]                      # Print this or datatype or lss help.
led                                      # Print status LED diodes.
log                                      # Print message log.

Response:
"["<sequence>"]" OK | <value> |
                 ERROR:<SDO-abort-code> | ERROR:<internal-error-code>

* Every command must be terminated with <CR><LF> ('\\r\\n'). characters. Same
  is response. String is not null terminated, <CR> is optional in command.
* Comments started with '#' are ignored. They may be on the beginning of the
  line or after the command string.
* 'sdo_timeout' is in milliseconds, 500 by default. Block transfer is
  disabled by default.
* If '<net>' or '<node>' is not specified within commands, then value defined
  by 'set network' or 'set node' command is used.

Datatypes:
b                  # Boolean.
i8, i16, i32, i64  # Signed integers.
u8, u16, u32, u64  # Unsigned integers.
x8, x16, x32, x64  # Unsigned integers, displayed as hexadecimal, non-standard.
r32, r64           # Real numbers.
t, td              # Time of day, time difference.
vs                 # Visible string (between double quotes if multi-word).
os, us             # Octet, unicode string, (mime-base64 (RFC2045) based, line).
d                  # domain (mime-base64 (RFC2045) based, one line).
hex                # Hexagonal data, optionally space separated, non-standard.

LSS commands:
lss_switch_glob <0|1>                  # Switch state global command.
lss_switch_sel <vendorID> <product code> \\
               <revisionNo> <serialNo> #Switch state selective.
lss_set_node <node>                    # Configure node-ID.
lss_conf_bitrate <table_selector=0> \\
                 <table_index>         # Configure bit-rate.
lss_activate_bitrate <switch_delay_ms> # Activate new bit-rate.
lss_store                              # LSS store configuration.
lss_inquire_addr [<LSSSUB=0..3>]       # Inquire LSS address.
lss_get_node                           # Inquire node-ID.
_lss_fastscan [<timeout_ms>]           # Identify fastscan, non-standard.
lss_allnodes [<timeout_ms> [<nodeStart=1..127> <store=0|1>\\
                [<scanType0> <vendorId> <scanType1> <productCode>\\
                 <scanType2> <revisionNo> <scanType3> <serialNo>]]]
                                       # Node-ID configuration of all nodes.

* All LSS commands start with '\"[\"<sequence>\"]\" [<net>]'.
* <table_index>: 0=1000 kbit/s, 1=800 kbit/s, 2=500 kbit/s, 3=250 kbit/s,
                 4=125 kbit/s, 6=50 kbit/s, 7=20 kbit/s, 8=10 kbit/s, 9=auto
* <scanType>: 0=fastscan, 1=ignore, 2=match value in next parameter
 * @endcode
 *
 * This help text is the same as variable contents in CO_GTWA_helpString.
 * @}
 */


/** Size of response string buffer. This is intermediate buffer. If there is
 * larger amount of data to transfer, then multiple transfers will occur. */
#ifndef CO_GTWA_RESP_BUF_SIZE
#define CO_GTWA_RESP_BUF_SIZE 200
#endif


/** Timeout time in microseconds for some internal states. */
#ifndef CO_GTWA_STATE_TIMEOUT_TIME_US
#define CO_GTWA_STATE_TIMEOUT_TIME_US 1200000
#endif


/**
 * Response error codes as specified by CiA 309-3. Values less or equal to 0
 * are used for control for some functions and are not part of the standard.
 */
typedef enum {
    /** 0 - No error or idle */
    CO_GTWA_respErrorNone                        = 0,
    /** 100 - Request not supported */
    CO_GTWA_respErrorReqNotSupported             = 100,
    /** 101 - Syntax error */
    CO_GTWA_respErrorSyntax                      = 101,
    /** 102 - Request not processed due to internal state */
    CO_GTWA_respErrorInternalState               = 102,
    /** 103 - Time-out (where applicable) */
    CO_GTWA_respErrorTimeOut                     = 103,
    /** 104 - No default net set */
    CO_GTWA_respErrorNoDefaultNetSet             = 104,
    /** 105 - No default node set */
    CO_GTWA_respErrorNoDefaultNodeSet            = 105,
    /** 106 - Unsupported net */
    CO_GTWA_respErrorUnsupportedNet              = 106,
    /** 107 - Unsupported node */
    CO_GTWA_respErrorUnsupportedNode             = 107,
    /** 200 - Lost guarding message */
    CO_GTWA_respErrorLostGuardingMessage         = 200,
    /** 201 - Lost connection */
    CO_GTWA_respErrorLostConnection              = 201,
    /** 202 - Heartbeat started */
    CO_GTWA_respErrorHeartbeatStarted            = 202,
    /** 203 - Heartbeat lost */
    CO_GTWA_respErrorHeartbeatLost               = 203,
    /** 204 - Wrong NMT state */
    CO_GTWA_respErrorWrongNMTstate               = 204,
    /** 205 - Boot-up */
    CO_GTWA_respErrorBootUp                      = 205,
    /** 300 - Error passive */
    CO_GTWA_respErrorErrorPassive                = 300,
    /** 301 - Bus off */
    CO_GTWA_respErrorBusOff                      = 301,
    /** 303 - CAN buffer overflow */
    CO_GTWA_respErrorCANbufferOverflow           = 303,
    /** 304 - CAN init */
    CO_GTWA_respErrorCANinit                     = 304,
    /** 305 - CAN active (at init or start-up) */
    CO_GTWA_respErrorCANactive                   = 305,
    /** 400 - PDO already used */
    CO_GTWA_respErrorPDOalreadyUsed              = 400,
    /** 401 - PDO length exceeded */
    CO_GTWA_respErrorPDOlengthExceeded           = 401,
    /** 501 - LSS implementation- / manufacturer-specific error */
    CO_GTWA_respErrorLSSmanufacturer             = 501,
    /** 502 - LSS node-ID not supported */
    CO_GTWA_respErrorLSSnodeIdNotSupported       = 502,
    /** 503 - LSS bit-rate not supported */
    CO_GTWA_respErrorLSSbitRateNotSupported      = 503,
    /** 504 - LSS parameter storing failed */
    CO_GTWA_respErrorLSSparameterStoringFailed   = 504,
    /** 505 - LSS command failed because of media error */
    CO_GTWA_respErrorLSSmediaError               = 505,
    /** 600 - Running out of memory */
    CO_GTWA_respErrorRunningOutOfMemory          = 600
} CO_GTWA_respErrorCode_t;


/**
 * Internal states of the Gateway-ascii state machine.
 */
typedef enum {
    /** Gateway is idle, no command is processing. This state is starting point
     * for new commands, which are parsed here. */
    CO_GTWA_ST_IDLE = 0x00U,
    /** SDO 'read' (upload) */
    CO_GTWA_ST_READ = 0x10U,
    /** SDO 'write' (download) */
    CO_GTWA_ST_WRITE = 0x11U,
    /** SDO 'write' (download) - aborted, purging remaining data */
    CO_GTWA_ST_WRITE_ABORTED = 0x12U,
    /** LSS 'lss_switch_glob' */
    CO_GTWA_ST_LSS_SWITCH_GLOB = 0x20U,
    /** LSS 'lss_switch_sel' */
    CO_GTWA_ST_LSS_SWITCH_SEL = 0x21U,
    /** LSS 'lss_set_node' */
    CO_GTWA_ST_LSS_SET_NODE = 0x22U,
    /** LSS 'lss_conf_bitrate' */
    CO_GTWA_ST_LSS_CONF_BITRATE = 0x23U,
    /** LSS 'lss_store' */
    CO_GTWA_ST_LSS_STORE = 0x24U,
    /** LSS 'lss_inquire_addr' or 'lss_get_node' */
    CO_GTWA_ST_LSS_INQUIRE = 0x25U,
    /** LSS 'lss_inquire_addr', all parameters */
    CO_GTWA_ST_LSS_INQUIRE_ADDR_ALL = 0x26U,
    /** LSS '_lss_fastscan' */
    CO_GTWA_ST__LSS_FASTSCAN = 0x30U,
    /** LSS 'lss_allnodes' */
    CO_GTWA_ST_LSS_ALLNODES = 0x31U,
    /** print message 'log' */
    CO_GTWA_ST_LOG = 0x80U,
    /** print 'help' text */
    CO_GTWA_ST_HELP = 0x81U,
    /** print 'status' of the node */
    CO_GTWA_ST_LED = 0x82U
} CO_GTWA_state_t;


#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) || defined CO_DOXYGEN
/*
 * CANopen Gateway-ascii data types structure
 */
typedef struct {
    /** Data type syntax, as defined in CiA309-3 */
    char* syntax;
    /** Data type length in bytes, 0 if size is not known */
    size_t length;
    /** Function, which reads data of specific data type from fifo buffer and
     * writes them as corresponding ascii string. It is a pointer to
     * #CO_fifo_readU82a function or similar and is used with SDO upload. For
     * description of parameters see #CO_fifo_readU82a */
    size_t (*dataTypePrint)(CO_fifo_t *fifo,
                            char *buf,
                            size_t count,
                            bool_t end);
    /** Function, which reads ascii-data of specific data type from fifo buffer
     * and copies them to another fifo buffer as binary data. It is a pointer to
     * #CO_fifo_cpyTok2U8 function or similar and is used with SDO download. For
     * description of parameters see #CO_fifo_cpyTok2U8 */
    size_t (*dataTypeScan)(CO_fifo_t *dest,
                           CO_fifo_t *src,
                           CO_fifo_st *status);
} CO_GTWA_dataType_t;
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO */


/**
 * CANopen Gateway-ascii object
 */
typedef struct {
    /** Pointer to external function for reading response from Gateway-ascii
     * object. Pointer is initialized in CO_GTWA_initRead().
     *
     * @param object Void pointer to custom object
     * @param buf Buffer from which data can be read
     * @param count Count of bytes available inside buffer
     * @param [out] connectionOK different than 0 indicates connection is OK.
     *
     * @return Count of bytes actually transferred.
     */
    size_t (*readCallback)(void *object,
                           const char *buf,
                           size_t count,
                           uint8_t *connectionOK);
    /** Pointer to object, which will be used inside readCallback, from
     * CO_GTWA_init() */
    void *readCallbackObject;
    /** Sequence number of the command */
    uint32_t sequence;
    /** Default CANopen Net number is undefined (-1) at startup */
    int32_t net_default;
    /** Default CANopen Node ID number is undefined (-1) at startup */
    int16_t node_default;
    /** Current CANopen Net number */
    uint16_t net;
    /** Current CANopen Node ID */
    uint8_t node;
    /** CO_fifo_t object for command (not pointer) */
    CO_fifo_t commFifo;
    /** Command buffer of usable size @ref CO_CONFIG_GTWA_COMM_BUF_SIZE */
    uint8_t commBuf[CO_CONFIG_GTWA_COMM_BUF_SIZE + 1];
    /** Response buffer of usable size @ref CO_GTWA_RESP_BUF_SIZE */
    char respBuf[CO_GTWA_RESP_BUF_SIZE];
    /** Actual size of data in respBuf */
    size_t respBufCount;
    /** If only part of data has been successfully written into external
     * application (with readCallback()), then Gateway-ascii object will stay
     * in current state. This situation is indicated with respHold variable and
     * respBufOffset indicates offset to untransferred data inside respBuf. */
    size_t respBufOffset;
    /** See respBufOffset above */
    bool_t respHold;
    /** Sum of time difference from CO_GTWA_process() in case of respHold */
    uint32_t timeDifference_us_cumulative;
    /** Current state of the gateway object */
    CO_GTWA_state_t state;
    /** Timeout timer for the current state */
    uint32_t stateTimeoutTmr;
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) || defined CO_DOXYGEN
    /** SDO client object from CO_GTWA_init() */
    CO_SDOclient_t *SDO_C;
    /** Timeout time for SDO transfer in milliseconds, if no response */
    uint16_t SDOtimeoutTime;
    /** SDO block transfer enabled? */
    bool_t SDOblockTransferEnable;
    /** Indicate status of data copy from / to SDO buffer. If reading, true
     * indicates, that response has started. If writing, true indicates, that
     * SDO buffer contains only part of data and more data will follow. */
    bool_t SDOdataCopyStatus;
    /** Data type of variable in current SDO communication */
    const CO_GTWA_dataType_t *SDOdataType;
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_NMT) || defined CO_DOXYGEN
    /** NMT object from CO_GTWA_init() */
    CO_NMT_t *NMT;
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS) || defined CO_DOXYGEN
    /** LSSmaster object from CO_GTWA_init() */
    CO_LSSmaster_t *LSSmaster;
    /** 128 bit number, uniquely identifying each node */
    CO_LSS_address_t lssAddress;
    /** LSS Node-ID parameter */
    uint8_t lssNID;
    /** LSS bitrate parameter */
    uint16_t lssBitrate;
    /** LSS inquire parameter */
    CO_LSS_cs_t lssInquireCs;
    /** LSS fastscan parameter */
    CO_LSSmaster_fastscan_t lssFastscan;
    /** LSS allnodes sub state parameter */
    uint8_t lssSubState;
    /** LSS allnodes node count parameter */
    uint8_t lssNodeCount;
    /** LSS allnodes store parameter */
    bool_t lssStore;
    /** LSS allnodes timeout parameter */
    uint16_t lssTimeout_ms;
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG) || defined CO_DOXYGEN
    /** Message log buffer of usable size @ref CO_CONFIG_GTWA_LOG_BUF_SIZE */
    uint8_t logBuf[CO_CONFIG_GTWA_LOG_BUF_SIZE + 1];
    /** CO_fifo_t object for message log (not pointer) */
    CO_fifo_t logFifo;
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_HELP) || defined CO_DOXYGEN
    /** Offset, when printing help text */
    const char *helpString;
    size_t helpStringOffset;
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS) || defined CO_DOXYGEN
    /** CO_LEDs_t object for CANopen status LEDs imitation from CO_GTWA_init()*/
    CO_LEDs_t *LEDs;
    uint8_t ledStringPreviousIndex;
#endif
} CO_GTWA_t;


/**
 * Initialize Gateway-ascii object
 *
 * @param gtwa This object will be initialized
 * @param SDO_C SDO client object
 * @param SDOclientTimeoutTime_ms Default timeout in milliseconds, 500 typically
 * @param SDOclientBlockTransfer If true, block transfer will be set by default
 * @param NMT NMT object
 * @param LSSmaster LSS master object
 * @param LEDs LEDs object
 * @param dummy dummy argument, set to 0
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT
 */
CO_ReturnError_t CO_GTWA_init(CO_GTWA_t* gtwa,
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) || defined CO_DOXYGEN
                              CO_SDOclient_t* SDO_C,
                              uint16_t SDOclientTimeoutTime_ms,
                              bool_t SDOclientBlockTransfer,
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_NMT) || defined CO_DOXYGEN
                              CO_NMT_t *NMT,
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS) || defined CO_DOXYGEN
                              CO_LSSmaster_t *LSSmaster,
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS) || defined CO_DOXYGEN
                              CO_LEDs_t *LEDs,
#endif
                              uint8_t dummy);


/**
 * Initialize read callback in Gateway-ascii object
 *
 * Callback will be used for transfer data to output stream of the application.
 * It will be called from CO_GTWA_process() zero or multiple times, depending on
 * the data available. If readCallback is uninitialized or NULL, then output
 * data will be purged.
 *
 * @param gtwa This object will be initialized
 * @param readCallback Pointer to external function for reading response from
 * Gateway-ascii object. See #CO_GTWA_t for parameters.
 * @param readCallbackObject Pointer to object, which will be used inside
 * readCallback
 */
void CO_GTWA_initRead(CO_GTWA_t* gtwa,
                      size_t (*readCallback)(void *object,
                                             const char *buf,
                                             size_t count,
                                             uint8_t *connectionOK),
                      void *readCallbackObject);


/**
 * Get free write buffer space
 *
 * @param gtwa This object
 *
 * @return number of available bytes
 */
static inline size_t CO_GTWA_write_getSpace(CO_GTWA_t* gtwa) {
    return CO_fifo_getSpace(&gtwa->commFifo);
}


/**
 * Write command into CO_GTWA_t object.
 *
 * This function copies ascii command from buf into internal fifo buffer.
 * Command must be closed with '\n' character. Function returns number of bytes
 * successfully copied. If there is not enough space in destination, not all
 * bytes will be copied and data can be refilled later (in case of large SDO
 * download).
 *
 * @param gtwa This object
 * @param buf Buffer which will be copied
 * @param count Number of bytes in buf
 *
 * @return number of bytes actually written.
 */
static inline size_t CO_GTWA_write(CO_GTWA_t* gtwa,
                                   const char *buf,
                                   size_t count)
{
    return CO_fifo_write(&gtwa->commFifo, (const uint8_t *)buf, count, NULL);
}


#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG) || defined CO_DOXYGEN
/**
 * Print message log string into fifo buffer
 *
 * This function enables recording of system log messages including CANopen
 * events. Function can be called by application for recording any message.
 * Message is copied to internal fifo buffer. In case fifo is full, old messages
 * will be owerwritten. Message log fifo can be read with non-standard command
 * "log". After log is read, it is emptied.
 *
 * @param gtwa This object
 * @param message Null terminated string
 */
void CO_GTWA_log_print(CO_GTWA_t* gtwa, const char *message);
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG */


/**
 * Process Gateway-ascii object
 *
 * This is non-blocking function and must be called cyclically
 *
 * @param gtwa This object will be initialized.
 * @param enable If true, gateway operates normally. If false, gateway is
 * completely disabled and no command interaction is possible. Can be connected
 * to hardware switch, for example.
 * @param timeDifference_us Time difference from previous function call in
 * [microseconds].
 * @param [out] timerNext_us info to OS - see CO_process().
 *
 * @return CO_ReturnError_t: CO_ERROR_NO on success or CO_ERROR_ILLEGAL_ARGUMENT
 */
void CO_GTWA_process(CO_GTWA_t *gtwa,
                     bool_t enable,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us);

/** @} */ /* CO_CANopen_309_3 */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif  /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII */

#endif /* CO_GATEWAY_ASCII_H */
