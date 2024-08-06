/**
 * CANopen Layer Setting Services protocol (common).
 *
 * @file        CO_LSS.h
 * @ingroup     CO_LSS
 * @author      Martin Wagner
 * @copyright   2017 - 2020 Neuberger Gebaeudeautomation GmbH
 *
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

#ifndef CO_LSS_H
#define CO_LSS_H

#include "301/CO_driver.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_LSS
#define CO_CONFIG_LSS (CO_CONFIG_LSS_SLAVE | CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE)
#endif

#if (((CO_CONFIG_LSS) & (CO_CONFIG_LSS_SLAVE | CO_CONFIG_LSS_MASTER)) != 0) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_LSS LSS
 * CANopen Layer Setting Services protocol (common).
 *
 * @ingroup CO_CANopen_305
 * @{
 * LSS protocol is according to CiA DSP 305 V3.0.0.
 *
 * LSS services and protocols are used to inquire or to change the settings of three parameters of the physical layer,
 * data link layer, and application layer on a CANopen device with LSS slave capability by a CANopen device with LSS
 * master capability via the CAN network.
 *
 * The following parameters may be inquired or changed:
 * - Node-ID of the CANopen device
 * - Bit timing parameters of the physical layer (bit rate)
 * - LSS address compliant to the identity object (1018h)
 *
 * The connection is established in one of two ways:
 * - addressing a node by it's 128 bit LSS address. This requires that the master already knows the node's LSS address.
 * - scanning the network for unknown nodes (Fastscan). Using this method, unknown devices can be found and configured
 *   one by one.
 *
 * Be aware that changing the bit rate is a critical step for the network. A failure will render the network unusable!
 *
 * Using this implementation, only master or slave can be included in one node at a time.
 *
 * For CAN identifiers see @ref CO_Default_CAN_ID_t
 */

/**
 * @defgroup CO_LSS_command_specifiers CO_LSS command specifiers
 * @{
 *
 * The LSS protocols are executed between the LSS master device and the LSS slave device(s) to implement the LSS
 * services. Some LSS protocols require a sequence of CAN messages.
 *
 * As identifying method only "LSS fastscan" is supported.
 */
#define CO_LSS_SWITCH_STATE_GLOBAL         0x04U /**< Switch state global protocol */
#define CO_LSS_SWITCH_STATE_SEL_VENDOR     0x40U /**< Switch state selective protocol - Vendor ID */
#define CO_LSS_SWITCH_STATE_SEL_PRODUCT    0x41U /**< Switch state selective protocol - Product code */
#define CO_LSS_SWITCH_STATE_SEL_REV        0x42U /**< Switch state selective protocol - Revision number */
#define CO_LSS_SWITCH_STATE_SEL_SERIAL     0x43U /**< Switch state selective protocol - Serial number */
#define CO_LSS_SWITCH_STATE_SEL            0x44U /**< Switch state selective protocol - Slave response */
#define CO_LSS_CFG_NODE_ID                 0x11U /**< Configure node ID protocol */
#define CO_LSS_CFG_BIT_TIMING              0x13U /**< Configure bit timing parameter protocol */
#define CO_LSS_CFG_ACTIVATE_BIT_TIMING     0x15U /**< Activate bit timing parameter protocol */
#define CO_LSS_CFG_STORE                   0x17U /**< Store configuration protocol */
#define CO_LSS_IDENT_SLAVE                 0x4FU /**< LSS Fastscan response */
#define CO_LSS_IDENT_FASTSCAN              0x51U /**< LSS Fastscan protocol */
#define CO_LSS_INQUIRE_VENDOR              0x5AU /**< Inquire identity vendor-ID protocol */
#define CO_LSS_INQUIRE_PRODUCT             0x5BU /**< Inquire identity product-code protocol */
#define CO_LSS_INQUIRE_REV                 0x5CU /**< Inquire identity revision-number protocol */
#define CO_LSS_INQUIRE_SERIAL              0x5DU /**< Inquire identity serial-number protocol */
#define CO_LSS_INQUIRE_NODE_ID             0x5EU /**< Inquire node-ID protocol */
/** @} */

/**
 * @defgroup CO_LSS_CFG_NODE_ID_status CO_LSS_CFG_NODE_ID status
 * @{
 * Error codes for Configure node ID protocol
 */
#define CO_LSS_CFG_NODE_ID_OK              0x00U /**< Protocol successfully completed */
#define CO_LSS_CFG_NODE_ID_OUT_OF_RANGE    0x01U /**< NID out of range */
#define CO_LSS_CFG_NODE_ID_MANUFACTURER    0xFFU /**< Manufacturer specific error. No further support */
/** @} */

/**
 * @defgroup CO_LSS_CFG_BIT_TIMING_status CO_LSS_CFG_BIT_TIMING status
 * @{
 * Error codes for Configure bit timing parameters protocol
 */
#define CO_LSS_CFG_BIT_TIMING_OK           0x00U /**< Protocol successfully completed */
#define CO_LSS_CFG_BIT_TIMING_OUT_OF_RANGE 0x01U /**< Bit timing / Bit rate not supported */
#define CO_LSS_CFG_BIT_TIMING_MANUFACTURER 0xFFU /**< Manufacturer specific error. No further support */
/** @} */

/**
 * @defgroup CO_LSS_CFG_STORE_status CO_LSS_CFG_STORE status
 * @{
 * Error codes for Store configuration protocol
 */
#define CO_LSS_CFG_STORE_OK                0x00U /**< Protocol successfully completed */
#define CO_LSS_CFG_STORE_NOT_SUPPORTED     0x01U /**< Store configuration not supported */
#define CO_LSS_CFG_STORE_FAILED            0x02U /**< Storage media access error */
#define CO_LSS_CFG_STORE_MANUFACTURER      0xFFU /**< Manufacturer specific error. No further support */
/** @} */

/**
 * @defgroup CO_LSS_FASTSCAN_bitcheck CO_LSS_FASTSCAN bitcheck
 * @{
 * Fastscan BitCheck. BIT0 means all bits are checked for equality by slave
 */
#define CO_LSS_FASTSCAN_BIT0               0x00U /**< Least significant bit of IDnumbners bit area to be checked */
/* ... */
#define CO_LSS_FASTSCAN_BIT31              0x1FU /**< dito */
#define CO_LSS_FASTSCAN_CONFIRM            0x80U /**< All LSS slaves waiting for scan respond and previous scan is reset */
/** @} */

/**
 * @defgroup CO_LSS_FASTSCAN_lssSub_lssNext CO_LSS_FASTSCAN lssSub lssNext
 * @{
 */
#define CO_LSS_FASTSCAN_VENDOR_ID          0x00U /**< Vendor ID */
#define CO_LSS_FASTSCAN_PRODUCT            0x01U /**< Product code */
#define CO_LSS_FASTSCAN_REV                0x02U /**< Revision number */
#define CO_LSS_FASTSCAN_SERIAL             0x03U /**< Serial number */
/** @} */

/**
 * The LSS address is a 128 bit number, uniquely identifying each node. It consists of the values in object 0x1018.
 */
typedef union {
    uint32_t addr[4];

    struct {
        uint32_t vendorID;
        uint32_t productCode;
        uint32_t revisionNumber;
        uint32_t serialNumber;
    } identity;
} CO_LSS_address_t;

/**
 * @defgroup CO_LSS_STATE_state CO_LSS_STATE state
 * @{
 *
 * The LSS FSA shall provide the following states:
 * - Initial: Pseudo state, indicating the activation of the FSA.
 * - LSS waiting: In this state, the LSS slave device waits for requests.
 * - LSS configuration: In this state variables may be configured in the LSS slave.
 * - Final: Pseudo state, indicating the deactivation of the FSA.
 */
#define CO_LSS_STATE_WAITING       0x00U /**< LSS FSA waiting for requests */
#define CO_LSS_STATE_CONFIGURATION 0x01U /**< LSS FSA waiting for configuration */
/** @} */

/**
 * @defgroup CO_LSS_BIT_TIMING_table CO_LSS_BIT_TIMING table
 * @{
 * Definition of table_index for /CiA301/ bit timing table
 */
#define CO_LSS_BIT_TIMING_1000     0U /**< 1000kbit/s */
#define CO_LSS_BIT_TIMING_800      1U /**< 800kbit/s */
#define CO_LSS_BIT_TIMING_500      2U /**< 500kbit/s */
#define CO_LSS_BIT_TIMING_250      3U /**< 250kbit/s */
#define CO_LSS_BIT_TIMING_125      4U /**< 125kbit/s */
                                      /* 5U - reserved */
#define CO_LSS_BIT_TIMING_50       6U /**< 50kbit/s */
#define CO_LSS_BIT_TIMING_20       7U /**< 20kbit/s */
#define CO_LSS_BIT_TIMING_10       8U /**< 10kbit/s */
#define CO_LSS_BIT_TIMING_AUTO     9U /**< Automatic bit rate detection */
/** @} */

/**
 * Lookup table for conversion between bit timing table and numerical bit rate
 */
static const uint16_t CO_LSS_bitTimingTableLookup[] = {1000, 800, 500, 250, 125, 0, 50, 20, 10, 0};

/**
 * Invalid node ID triggers node ID assignment
 */
#define CO_LSS_NODE_ID_ASSIGNMENT 0xFFU

/**
 * Macro to check if node id is valid
 */
#define CO_LSS_NODE_ID_VALID(nid) (((nid >= 1U) && (nid <= 0x7FU)) || (nid == CO_LSS_NODE_ID_ASSIGNMENT))

/**
 * Macro to check if two LSS addresses are equal
 */
#define CO_LSS_ADDRESS_EQUAL(/* CO_LSS_address_t */ a1, /* CO_LSS_address_t */ a2)                                     \
    ((a1.identity.productCode == a2.identity.productCode)                                                              \
     && (a1.identity.revisionNumber == a2.identity.revisionNumber)                                                     \
     && (a1.identity.serialNumber == a2.identity.serialNumber) && (a1.identity.vendorID == a2.identity.vendorID))

/** @} */ /* @defgroup CO_LSS */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* (CO_CONFIG_LSS) & (CO_CONFIG_LSS_SLAVE | CO_CONFIG_LSS_MASTER) */

#endif /* CO_LSS_H */
