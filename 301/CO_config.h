/**
 * Configuration macros for CANopenNode.
 *
 * @file        CO_config.h
 * @ingroup     CO_driver
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


#ifndef CO_CONFIG_FLAGS_H
#define CO_CONFIG_FLAGS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_STACK_CONFIG Stack configuration
 * @ingroup CO_driver
 *
 * Stack configuration macros specify, which parts of the stack will be enabled.
 *
 * Default values for stack configuration macros are set in corresponding
 * header files. The same default values are also provided in this file, but
 * only for documentation generator. Default values can be overridden by
 * CO_driver_target.h file. If specified so, they can further be overridden by
 * CO_driver_custom.h file.
 *
 * Stack configuration macro is specified as bits, where each bit
 * enables/disables some part of the configurable CANopenNode object. Flags are
 * used for enabling or checking specific bit. Multiple flags can be ORed
 * together.
 *
 * Some functionalities of CANopenNode objects, enabled by configuration macros,
 * requires some objects from Object Dictionary to exist. Object Dictionary
 * configuration must match @ref CO_STACK_CONFIG.
 * @{
 */


/**
 * @defgroup CO_STACK_CONFIG_COMMON Common definitions
 * @{
 */
/**
 * Enable custom callback after CAN receive
 *
 * Flag enables optional callback functions, which are part of some CANopenNode
 * objects. Callbacks can optionally be registered by application, which
 * configures threads in operating system. Callbacks are called after something
 * has been preprocessed by higher priority thread and must be further
 * processed by lower priority thread. For example when CAN message is received
 * and preprocessed, callback should wake up mainline processing function.
 * See also @ref CO_process() function.
 *
 * If callback functions are used, they must be initialized separately, after
 * the object initialization.
 *
 * This flag is common to multiple configuration macros.
 */
#define CO_CONFIG_FLAG_CALLBACK_PRE 0x1000

/**
 * Enable calculation of timerNext_us variable.
 *
 * Calculation of the timerNext_us variable is useful for smooth operation on
 * operating system. See also @ref CO_process() function.
 *
 * This flag is common to multiple configuration macros.
 */
#define CO_CONFIG_FLAG_TIMERNEXT 0x2000

/**
 * Enable dynamic behaviour of Object Dictionary variables
 *
 * Some CANopen objects uses Object Dictionary variables as arguments to
 * initialization functions, which are processed in communication reset section.
 * If this flag is set, then writing to OD variable will reconfigure
 * corresponding CANopen object also during CANopen normal operation.
 *
 * This flag is common to multiple configuration macros.
 */
#define CO_CONFIG_FLAG_OD_DYNAMIC 0x4000
/** @} */ /* CO_STACK_CONFIG_COMMON */


/**
 * @defgroup CO_STACK_CONFIG_NMT_HB NMT master/slave and HB producer/consumer
 * @{
 */
/**
 * Configuration of @ref CO_NMT_Heartbeat.
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_NMT_CALLBACK_CHANGE - Enable custom callback after NMT
 *   state changes. Callback is configured by
 *   CO_NMT_initCallbackChanged().
 * - CO_CONFIG_NMT_MASTER - Enable simple NMT master
 * - #CO_CONFIG_FLAG_CALLBACK_PRE - Enable custom callback after preprocessing
 *   received NMT CAN message.
 *   Callback is configured by CO_NMT_initCallbackPre().
 * - #CO_CONFIG_FLAG_TIMERNEXT - Enable calculation of timerNext_us variable
 *   inside CO_NMT_process().
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_NMT (0)
#endif
#define CO_CONFIG_NMT_CALLBACK_CHANGE 0x01
#define CO_CONFIG_NMT_MASTER 0x02

/**
 * Configuration of @ref CO_HBconsumer
 *
 * Possible flags, can be ORed:
 * - #CO_CONFIG_FLAG_CALLBACK_PRE - Enable custom callback after preprocessing
 *   received heartbeat CAN message.
 *   Callback is configured by CO_HBconsumer_initCallbackPre().
 * - #CO_CONFIG_FLAG_TIMERNEXT - Enable calculation of timerNext_us variable
 *   inside CO_HBconsumer_process().
 * - CO_CONFIG_HB_CONS_ENABLE - Enable heartbeat consumer.
 * - CO_CONFIG_HB_CONS_CALLBACK_CHANGE - Enable custom common callback after NMT
 *   state of the monitored node changes. Callback is configured by
 *   CO_HBconsumer_initCallbackNmtChanged().
 * - CO_CONFIG_HB_CONS_CALLBACK_MULTI - Enable multiple custom callbacks, which
 *   can be configured individually for each monitored node. Callbacks are
 *   configured by CO_HBconsumer_initCallbackNmtChanged(),
 *   CO_HBconsumer_initCallbackHeartbeatStarted(),
 *   CO_HBconsumer_initCallbackTimeout() and
 *   CO_HBconsumer_initCallbackRemoteReset() functions.
 * - CO_CONFIG_HB_CONS_QUERY_FUNCT - Enable functions for query HB state or
 *   NMT state of the specific monitored node.
 * Note that CO_CONFIG_HB_CONS_CALLBACK_CHANGE and
 * CO_CONFIG_HB_CONS_CALLBACK_MULTI cannot be set simultaneously.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_HB_CONS (CO_CONFIG_HB_CONS_ENABLE)
#endif
#define CO_CONFIG_HB_CONS_ENABLE 0x01
#define CO_CONFIG_HB_CONS_CALLBACK_CHANGE 0x02
#define CO_CONFIG_HB_CONS_CALLBACK_MULTI 0x04
#define CO_CONFIG_HB_CONS_QUERY_FUNCT 0x08

/**
 * Number of heartbeat consumer objects, where each object corresponds to one
 * sub-index in OD object 0x1016, "Consumer heartbeat time".
 *
 * If heartbeat consumer is enabled, then valid values are 1 to 127.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_HB_CONS_SIZE 8
#endif
/** @} */ /* CO_STACK_CONFIG_NMT_HB */


/**
 * @defgroup CO_STACK_CONFIG_EMERGENCY Emergency producer/consumer
 * @{
 */
/**
 * Configuration of @ref CO_Emergency
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_EM_PRODUCER - Enable emergency producer.
 * - CO_CONFIG_EM_PROD_CONFIGURABLE - Emergency producer COB-ID is configurable,
 *   OD object 0x1014. If not configurable, then 0x1014 is read-only, COB_ID
 *   is set to CO_CAN_ID_EMERGENCY + nodeId and write is not verified.
 * - CO_CONFIG_EM_PROD_INHIBIT - Enable inhibit timer on emergency producer,
 *   OD object 0x1015.
 * - CO_CONFIG_EM_HISTORY - Enable error history, OD object 0x1003,
 *   "Pre-defined error field"
 * - CO_CONFIG_EM_CONSUMER - Enable simple emergency consumer with callback.
 * - CO_CONFIG_EM_STATUS_BITS - Access @ref CO_EM_errorStatusBits_t from OD.
 * - #CO_CONFIG_FLAG_CALLBACK_PRE - Enable custom callback after preprocessing
 *   emergency condition by CO_errorReport() or CO_errorReset() call.
 *   Callback is configured by CO_EM_initCallbackPre().
 * - #CO_CONFIG_FLAG_TIMERNEXT - Enable calculation of timerNext_us variable
 *   inside CO_EM_process().
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_EM (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)
#endif
#define CO_CONFIG_EM_PRODUCER 0x01
#define CO_CONFIG_EM_PROD_CONFIGURABLE 0x02
#define CO_CONFIG_EM_PROD_INHIBIT 0x04
#define CO_CONFIG_EM_HISTORY 0x08
#define CO_CONFIG_EM_STATUS_BITS 0x10
#define CO_CONFIG_EM_CONSUMER 0x20

/**
 * Maximum number of @ref CO_EM_errorStatusBits_t
 *
 * Stack uses 6*8 = 48 @ref CO_EM_errorStatusBits_t, others are free to use by
 * manufacturer. Allowable value range is from 48 to 256 bits in steps of 8.
 * Default is 80.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_EM_ERR_STATUS_BITS_COUNT (10*8)
#endif

/**
 * Size of the internal buffer, where emergencies are stored after error
 * indication with @ref CO_error() function. Each emergency has to be post-
 * processed by the @ref CO_EM_process() function. In case of overflow, error is
 * indicated but emergency message is not sent.
 *
 * The same buffer is also used for OD object 0x1003, "Pre-defined error field".
 *
 * Each buffer element consumes 8 bytes. Valid values are 1 to 254.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_EM_BUFFER_SIZE 16
#endif

/**
 * Condition for calculating CANopen Error register, "generic" error bit.
 *
 * Condition must observe suitable @ref CO_EM_errorStatusBits_t and use
 * corresponding member of errorStatusBits array from CO_EM_t to calculate the
 * condition. See also @ref CO_errorRegister_t.
 *
 * @warning Size of @ref CO_CONFIG_EM_ERR_STATUS_BITS_COUNT must be large
 * enough. (CO_CONFIG_EM_ERR_STATUS_BITS_COUNT/8) must be larger than index of
 * array member in em->errorStatusBits[index].
 *
 * em->errorStatusBits[5] should be included in the condition, because they are
 * used by the stack.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_ERR_CONDITION_GENERIC (em->errorStatusBits[5] != 0)
#endif

/**
 * Condition for calculating CANopen Error register, "current" error bit.
 * See @ref CO_CONFIG_ERR_CONDITION_GENERIC for description.
 * Macro is not defined by default, so no error is verified.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_ERR_CONDITION_CURRENT
#endif

/**
 * Condition for calculating CANopen Error register, "voltage" error bit.
 * See @ref CO_CONFIG_ERR_CONDITION_GENERIC for description.
 * Macro is not defined by default, so no error is verified.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_ERR_CONDITION_VOLTAGE
#endif

/**
 * Condition for calculating CANopen Error register, "temperature" error bit.
 * See @ref CO_CONFIG_ERR_CONDITION_GENERIC for description.
 * Macro is not defined by default, so no error is verified.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_ERR_CONDITION_TEMPERATURE
#endif

/**
 * Condition for calculating CANopen Error register, "communication" error bit.
 * See @ref CO_CONFIG_ERR_CONDITION_GENERIC for description.
 *
 * em->errorStatusBits[2] and em->errorStatusBits[3] must be included in the
 * condition, because they are used by the stack.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_ERR_CONDITION_COMMUNICATION (em->errorStatusBits[2] || em->errorStatusBits[3])
#endif

/**
 * Condition for calculating CANopen Error register, "device profile" error bit.
 * See @ref CO_CONFIG_ERR_CONDITION_GENERIC for description.
 * Macro is not defined by default, so no error is verified.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_ERR_CONDITION_DEV_PROFILE
#endif

/**
 * Condition for calculating CANopen Error register, "manufacturer" error bit.
 * See @ref CO_CONFIG_ERR_CONDITION_GENERIC for description.
 *
 * em->errorStatusBits[8] and em->errorStatusBits[8] are pre-defined, but can
 * be changed.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_ERR_CONDITION_MANUFACTURER (em->errorStatusBits[8] || em->errorStatusBits[9])
#endif
/** @} */ /* CO_STACK_CONFIG_EMERGENCY */


/**
 * @defgroup CO_STACK_CONFIG_SDO SDO server/client
 * @{
 */
/**
 * Configuration of @ref CO_SDOserver
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_SDO_SRV_SEGMENTED - Enable SDO server segmented transfer.
 * - CO_CONFIG_SDO_SRV_BLOCK - Enable SDO server block transfer. If set, then
 *   CO_CONFIG_SDO_SRV_SEGMENTED must also be set.
 * - #CO_CONFIG_FLAG_CALLBACK_PRE - Enable custom callback after preprocessing
 *   received SDO CAN message.
 *   Callback is configured by CO_SDOserver_initCallbackPre().
 * - #CO_CONFIG_FLAG_TIMERNEXT - Enable calculation of timerNext_us variable
 *   inside CO_SDOserver_process().
 * - #CO_CONFIG_FLAG_OD_DYNAMIC - Enable dynamic configuration of additional SDO
 *   servers (Writing to object 0x1201+ re-configures the additional server).
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_SDO_SRV (CO_CONFIG_SDO_SRV_SEGMENTED)
#endif
#define CO_CONFIG_SDO_SRV_SEGMENTED 0x02
#define CO_CONFIG_SDO_SRV_BLOCK 0x04

/**
 * Size of the internal data buffer for the SDO server.
 *
 * If size is less than size of some variables in Object Dictionary, then data
 * will be transferred to internal buffer in several segments. Minimum size is
 * 8 or 899 (127*7) for block transfer.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_SDO_SRV_BUFFER_SIZE 32
#endif

/**
 * Configuration of @ref CO_SDOclient
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_SDO_CLI_ENABLE - Enable SDO client.
 * - CO_CONFIG_SDO_CLI_SEGMENTED - Enable SDO client segmented transfer.
 * - CO_CONFIG_SDO_CLI_BLOCK - Enable SDO client block transfer. If set, then
 *   CO_CONFIG_SDO_CLI_SEGMENTED, CO_CONFIG_FIFO_ALT_READ and
 *   CO_CONFIG_FIFO_CRC16_CCITT must also be set.
 * - CO_CONFIG_SDO_CLI_LOCAL - Enable local transfer, if Node-ID of the SDO
 *   server is the same as node-ID of the SDO client. (SDO client is the same
 *   device as SDO server.) Transfer data directly without communication on CAN.
 * - #CO_CONFIG_FLAG_CALLBACK_PRE - Enable custom callback after preprocessing
 *   received SDO CAN message.
 *   Callback is configured by CO_SDOclient_initCallbackPre().
 * - #CO_CONFIG_FLAG_TIMERNEXT - Enable calculation of timerNext_us variable
 *   inside CO_SDOclientDownloadInitiate(), CO_SDOclientDownload(),
 *   CO_SDOclientUploadInitiate(), CO_SDOclientUpload().
 * - #CO_CONFIG_FLAG_OD_DYNAMIC - Enable dynamic configuration of SDO clients
 *   (Writing to object 0x1280+ re-configures the client).
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_SDO_CLI (0)
#endif
#define CO_CONFIG_SDO_CLI_ENABLE 0x01
#define CO_CONFIG_SDO_CLI_SEGMENTED 0x02
#define CO_CONFIG_SDO_CLI_BLOCK 0x04
#define CO_CONFIG_SDO_CLI_LOCAL 0x08

/**
 * Size of the internal data buffer for the SDO client.
 *
 * Circular buffer is used for SDO communication. it can be read or written
 * between successive SDO calls. So size of the buffer can be lower than size of
 * the actual size of data transferred. If only segmented transfer is used, then
 * buffer size can be as low as 7 bytes, if data are read/written each cycle. If
 * block transfer is used, buffer size should be set to at least 889 bytes, so
 * maximum blksize can be used. In that case data should be read/written proper
 * time between cycles.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_SDO_CLI_BUFFER_SIZE 32
#endif
/** @} */ /* CO_STACK_CONFIG_SDO */


/**
 * @defgroup CO_STACK_CONFIG_TIME Time producer/consumer
 * @{
 */
/**
 * Configuration of @ref CO_TIME
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_TIME_ENABLE - Enable TIME object and TIME consumer.
 * - CO_CONFIG_TIME_PRODUCER - Enable TIME producer.
 * - #CO_CONFIG_FLAG_CALLBACK_PRE - Enable custom callback after preprocessing
 *   received TIME CAN message.
 *   Callback is configured by CO_TIME_initCallbackPre().
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_TIME (0)
#endif
#define CO_CONFIG_TIME_ENABLE 0x01
#define CO_CONFIG_TIME_PRODUCER 0x02
/** @} */ /* CO_STACK_CONFIG_TIME */


/**
 * @defgroup CO_STACK_CONFIG_SYNC_PDO SYNC and PDO producer/consumer
 * @{
 */
/**
 * Configuration of @ref CO_SYNC
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_SYNC_ENABLE - Enable SYNC object and SYNC consumer.
 * - CO_CONFIG_SYNC_PRODUCER - Enable SYNC producer.
 * - #CO_CONFIG_FLAG_CALLBACK_PRE - Enable custom callback after preprocessing
 *   received SYNC CAN message.
 *   Callback is configured by CO_SYNC_initCallbackPre().
 * - #CO_CONFIG_FLAG_TIMERNEXT - Enable calculation of timerNext_us variable
 *   inside CO_SYNC_process().
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_SYNC (CO_CONFIG_SYNC_ENABLE | CO_CONFIG_SYNC_PRODUCER)
#endif
#define CO_CONFIG_SYNC_ENABLE 0x01
#define CO_CONFIG_SYNC_PRODUCER 0x02

/**
 * Configuration of @ref CO_PDO
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_RPDO_ENABLE - Enable receive PDO objects.
 * - CO_CONFIG_TPDO_ENABLE - Enable transmit PDO objects.
 * - CO_CONFIG_PDO_SYNC_ENABLE - Enable SYNC in PDO objects.
 * - CO_CONFIG_RPDO_CALLS_EXTENSION - Enable calling configured extension
 *   callbacks when received RPDO CAN message modifies OD entries.
 * - CO_CONFIG_TPDO_CALLS_EXTENSION - Enable calling configured extension
 *   callbacks before TPDO CAN message is sent.
 * - #CO_CONFIG_FLAG_CALLBACK_PRE - Enable custom callback after preprocessing
 *   received RPDO CAN message.
 *   Callback is configured by CO_RPDO_initCallbackPre().
 * - #CO_CONFIG_FLAG_TIMERNEXT - Enable calculation of timerNext_us variable
 *   inside CO_TPDO_process().
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_PDO (CO_CONFIG_RPDO_ENABLE | CO_CONFIG_TPDO_ENABLE | CO_CONFIG_PDO_SYNC_ENABLE)
#endif
#define CO_CONFIG_RPDO_ENABLE 0x01
#define CO_CONFIG_TPDO_ENABLE 0x02
#define CO_CONFIG_PDO_SYNC_ENABLE 0x04
#define CO_CONFIG_RPDO_CALLS_EXTENSION 0x08
#define CO_CONFIG_TPDO_CALLS_EXTENSION 0x10
/** @} */ /* CO_STACK_CONFIG_SYNC_PDO */


/**
 * @defgroup CO_STACK_CONFIG_LEDS CANopen LED diodes
 * Specified in standard CiA 303-3
 * @{
 */
/**
 * Configuration of @ref CO_LEDs
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_LEDS_ENABLE - Enable calculation of the CANopen LED indicators.
 * - #CO_CONFIG_FLAG_TIMERNEXT - Enable calculation of timerNext_us variable
 *   inside CO_NMT_process().
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_LEDS (CO_CONFIG_LEDS_ENABLE)
#endif
#define CO_CONFIG_LEDS_ENABLE 0x01
/** @} */ /* CO_STACK_CONFIG_LEDS */


/**
 * @defgroup CO_STACK_CONFIG_SRDO Safety Related Data Objects (SRDO)
 * Specified in standard EN 50325-5 (CiA 304)
 * @{
 */
/**
 * Configuration of @ref CO_GFC
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_GFC_ENABLE - Enable the GFC object
 * - CO_CONFIG_GFC_CONSUMER - Enable the GFC consumer
 * - CO_CONFIG_GFC_PRODUCER - Enable the GFC producer
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_GFC (0)
#endif
#define CO_CONFIG_GFC_ENABLE 0x01
#define CO_CONFIG_GFC_CONSUMER 0x02
#define CO_CONFIG_GFC_PRODUCER 0x04

/**
 * Configuration of @ref CO_SRDO
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_SRDO_ENABLE - Enable the SRDO object.
 * - CO_CONFIG_SRDO_CHECK_TX - Enable checking data before sending.
 * - CO_CONFIG_RSRDO_CALLS_EXTENSION - Enable calling configured extension
 *   callbacks when received RSRDO CAN message modifies OD entries.
 * - CO_CONFIG_TRSRDO_CALLS_EXTENSION - Enable calling configured extension
 *   callbacks before TSRDO CAN message is sent.
 * - #CO_CONFIG_FLAG_CALLBACK_PRE - Enable custom callback after preprocessing
 *   received RSRDO CAN message.
 *   Callback is configured by CO_SRDO_initCallbackPre().
 * - #CO_CONFIG_FLAG_TIMERNEXT - Enable calculation of timerNext_us variable
 *   inside CO_SRDO_process() (Tx SRDO only).
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_SRDO (0)
#endif
#define CO_CONFIG_SRDO_ENABLE 0x01
#define CO_CONFIG_SRDO_CHECK_TX 0x02
#define CO_CONFIG_RSRDO_CALLS_EXTENSION 0x04
#define CO_CONFIG_TSRDO_CALLS_EXTENSION 0x08

/**
 * SRDO Tx time delay
 *
 * minimum time between the first and second SRDO (Tx) message
 * in us
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_SRDO_MINIMUM_DELAY 0
#endif
/** @} */ /* CO_STACK_CONFIG_SRDO */


/**
 * @defgroup CO_STACK_CONFIG_LSS LSS master/slave
 * Specified in standard CiA 305
 * @{
 */
/**
 * Configuration of @ref CO_LSS
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_LSS_SLAVE - Enable LSS slave
 * - CO_CONFIG_LSS_SLAVE_FASTSCAN_DIRECT_RESPOND - Send LSS fastscan respond
 *   directly from CO_LSSslave_receive() function.
 * - CO_CONFIG_LSS_MASTER - Enable LSS master
 * - #CO_CONFIG_FLAG_CALLBACK_PRE - Enable custom callback after preprocessing
 *   received CAN message.
 *   Callback is configured by CO_LSSmaster_initCallbackPre().
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_LSS (CO_CONFIG_LSS_SLAVE)
#endif
#define CO_CONFIG_LSS_SLAVE 0x01
#define CO_CONFIG_LSS_SLAVE_FASTSCAN_DIRECT_RESPOND 0x02
#define CO_CONFIG_LSS_MASTER 0x10
/** @} */ /* CO_STACK_CONFIG_LSS */


/**
 * @defgroup CO_STACK_CONFIG_GATEWAY CANopen gateway
 * Specified in standard CiA 309
 * @{
 */
/**
 * Configuration of @ref CO_CANopen_309_3
 *
 * Gateway object is covered by standard CiA 309 - CANopen access from other
 * networks. It enables usage of the NMT master, SDO client and LSS master as a
 * gateway device.
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_GTW_MULTI_NET - Enable multiple network interfaces in gateway
 *   device. This functionality is currently not implemented.
 * - CO_CONFIG_GTW_ASCII - Enable gateway device with ASCII mapping (CiA 309-3)
 *   If set, then CO_CONFIG_FIFO_ASCII_COMMANDS must also be set.
 * - CO_CONFIG_GTW_ASCII_SDO - Enable SDO client. If set, then
 *   CO_CONFIG_FIFO_ASCII_DATATYPES must also be set.
 * - CO_CONFIG_GTW_ASCII_NMT - Enable NMT master
 * - CO_CONFIG_GTW_ASCII_LSS - Enable LSS master
 * - CO_CONFIG_GTW_ASCII_LOG - Enable non-standard message log read
 * - CO_CONFIG_GTW_ASCII_ERROR_DESC - Print error description as additional
 *   comments in gateway-ascii device for SDO and gateway errors.
 * - CO_CONFIG_GTW_ASCII_PRINT_HELP - use non-standard command "help" to print
 *   help usage.
 * - CO_CONFIG_GTW_ASCII_PRINT_LEDS - Display "red" and "green" CANopen status
 *   LED diodes on terminal.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_GTW (0)
#endif
#define CO_CONFIG_GTW_MULTI_NET 0x01
#define CO_CONFIG_GTW_ASCII 0x02
#define CO_CONFIG_GTW_ASCII_SDO 0x04
#define CO_CONFIG_GTW_ASCII_NMT 0x08
#define CO_CONFIG_GTW_ASCII_LSS 0x10
#define CO_CONFIG_GTW_ASCII_LOG 0x20
#define CO_CONFIG_GTW_ASCII_ERROR_DESC 0x40
#define CO_CONFIG_GTW_ASCII_PRINT_HELP 0x80
#define CO_CONFIG_GTW_ASCII_PRINT_LEDS 0x100

/**
 * Number of loops of #CO_SDOclientDownload() in case of block download
 *
 * If SDO clint has block download in progress and OS has buffer for CAN tx
 * messages, then #CO_SDOclientDownload() functionion can be called multiple
 * times within own loop (up to 127). This can speed-up SDO block transfer.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_GTW_BLOCK_DL_LOOP 1
#endif

/**
 * Size of command buffer in ASCII gateway object.
 *
 * If large amount of data is transferred (block transfer), then this should be
 * increased to 1000 or more. Buffer may be refilled between the block transfer.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_GTWA_COMM_BUF_SIZE 200
#endif

/**
 * Size of message log buffer in ASCII gateway object.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_GTWA_LOG_BUF_SIZE 2000
#endif
/** @} */ /* CO_STACK_CONFIG_GATEWAY */


/**
 * @defgroup CO_STACK_CONFIG_CRC16 CRC 16 calculation
 * Helper object
 * @{
 */
/**
 * Configuration of @ref CO_crc16_ccitt calculation
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_CRC16_ENABLE - Enable CRC16 calculation
 * - CO_CONFIG_CRC16_EXTERNAL - CRC functions are defined externally
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_CRC16 (0)
#endif
#define CO_CONFIG_CRC16_ENABLE 0x01
#define CO_CONFIG_CRC16_EXTERNAL 0x02
/** @} */ /* CO_STACK_CONFIG_CRC16 */


/**
 * @defgroup CO_STACK_CONFIG_FIFO FIFO buffer
 * Helper object
 * @{
 */
/**
 * Configuration of @ref CO_CANopen_301_fifo
 *
 * FIFO buffer is basically a simple first-in first-out circular data buffer. It
 * is used by the SDO client and by the CANopen gateway. It has additional
 * advanced functions for data passed to FIFO.
 *
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_FIFO_ENABLE - Enable FIFO buffer
 * - CO_CONFIG_FIFO_ALT_READ - This must be enabled, when SDO client has
 *   CO_CONFIG_SDO_CLI_BLOCK enabled. See @ref CO_fifo_altRead().
 * - CO_CONFIG_FIFO_CRC16_CCITT - This must be enabled, when SDO client has
 *   CO_CONFIG_SDO_CLI_BLOCK enabled. It enables CRC calculation on data.
 * - CO_CONFIG_FIFO_ASCII_COMMANDS - This must be enabled, when CANopen gateway
 *   has CO_CONFIG_GTW_ASCII enabled. It adds command handling functions.
 * - CO_CONFIG_FIFO_ASCII_DATATYPES - This must be enabled, when CANopen gateway
 *   has CO_CONFIG_GTW_ASCII and CO_CONFIG_GTW_ASCII_SDO enabled. It adds
 *   datatype transform functions between binary and ascii, which are necessary
 *   for SDO client.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_FIFO (0)
#endif
#define CO_CONFIG_FIFO_ENABLE 0x01
#define CO_CONFIG_FIFO_ALT_READ 0x02
#define CO_CONFIG_FIFO_CRC16_CCITT 0x04
#define CO_CONFIG_FIFO_ASCII_COMMANDS 0x08
#define CO_CONFIG_FIFO_ASCII_DATATYPES 0x10
/** @} */ /* CO_STACK_CONFIG_FIFO */


/**
 * @defgroup CO_STACK_CONFIG_TRACE Trace recorder
 * Non standard object
 * @{
 */
/**
 * Configuration of @ref CO_trace for recording variables over time.
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_TRACE_ENABLE - Enable Trace recorder
 * - CO_CONFIG_TRACE_OWN_INTTYPES - If set, then macros PRIu32("u" or "lu")
 *   and PRId32("d" or "ld") must be set. (File inttypes.h can not be included).
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_TRACE (0)
#endif
#define CO_CONFIG_TRACE_ENABLE 0x01
#define CO_CONFIG_TRACE_OWN_INTTYPES 0x02
/** @} */ /* CO_STACK_CONFIG_TRACE */

/** @} */ /* CO_STACK_CONFIG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_CONFIG_FLAGS_H */
