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
 * Default values for stack configuration macros are set in CO_driver.h file.
 * They can be overridden by CO_driver_target.h file. If specified so, they can
 * further be overridden by CO_driver_custom.h file.
 *
 * Stack configuration macro is specified as bits, where each bit
 * enables/disables some part of the configurable CANopenNode object. Flags are
 * used for enabling or checking specific bit. Multiple flags can be ORed
 * together.
 *
 * Configuration macros may be in relation with @ref CO_NO_OBJ macros from
 * CANopen.h file. Former enables/disables stack functionalities, latter
 * enables/disables objects in object dictionary. Note that some objects in
 * object dictionary may need some configuration macros to be enabled.
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
#define CO_CONFIG_FLAG_CANRX_CALLBACK 0x0100


/**
 * Configuration of NMT
 *
 * Possible flags, can be ORed:
 * - CO_CONFIG_NMT_LEDS - Calculate CANopen blinking variables, which can
 * be used for LEDs.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_NMT CO_CONFIG_NMT_LEDS
#endif
#define CO_CONFIG_NMT_LEDS 0x01

/**
 * Configuration of Heartbeat Consumer
 *
 * Possible flags, can be ORed:
 * - #CO_CONFIG_FLAG_CANRX_CALLBACK - Enable custom callback after CAN receive.
 *   Callback is configured by CO_HBconsumer_initCallbackPre().
 * - CO_CONFIG_HB_CONS_CHANGE_CALLBACK - Enable custom callback after NMT
 *   state of the monitored node changes. Callback is configured by
 *   CO_HBconsumer_initCallbackNmtChanged().
 * - CO_CONFIG_HB_CONS_MULTI_CALLBACK - Enable multiple custom callbacks, which
 *   can be configured for each monitored node. Callback are configured by
 *   CO_HBconsumer_initCallbackHeartbeatStarted(),
 *   CO_HBconsumer_initCallbackTimeout() and
 *   CO_HBconsumer_initCallbackRemoteReset() functions.
 * - CO_CONFIG_HB_CONS_QUERY_FUNCT - Enable functions for query HB state or
 *   NMT state of the specific monitored node.
 */
#ifdef CO_DOXYGEN
#define CO_CONFIG_HB_CONS CO_CONFIG_FLAG_CANRX_CALLBACK | CO_CONFIG_HB_CONS_CHANGE_CALLBACK | CO_CONFIG_HB_CONS_MULTI_CALLBACK | CO_CONFIG_HB_CONS_QUERY_FUNCT
#endif
#define CO_CONFIG_HB_CONS_CHANGE_CALLBACK 0x01
#define CO_CONFIG_HB_CONS_MULTI_CALLBACK 0x02
#define CO_CONFIG_HB_CONS_QUERY_FUNCT 0x04


/**
 * Size of the internal SDO buffer.
 *
 * Size must be at least equal to size of largest variable in
 * @ref CO_SDO_objectDictionary. If data type is domain, data length is not
 * limited to SDO buffer size. If block transfer is implemented, value should be
 * set to 889.
 *
 * Value can be in range from 7 to 889 bytes.
 */
#ifndef CO_CONFIG_SDO_BUFFER_SIZE
#define CO_CONFIG_SDO_BUFFER_SIZE 32
#endif

/**
 * Configuration of Standard CiA 309 usage.
 *
 * CiA 309 standard covers CANopen access from other networks. It enables
 * usage of the NMT master, SDO client and LSS master as a gateway device.
 *
 * Value can be one of the following:
 * - 0: Disabled.
 * - 1: Interface enabled
 * - 2: Modbus/TCP mapping (Not implemented)
 * - 3: ASCII mapping
 * - 4: Profinet (Not implemented)
 */
#ifndef CO_CONFIG_309
#define CO_CONFIG_309 0
#endif

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_CONFIG_FLAGS_H */
