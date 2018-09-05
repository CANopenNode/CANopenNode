/**
 * CANopen LSS Master/Slave protocol.
 *
 * @file        CO_LSS.h
 * @ingroup     CO_LSS
 * @author      Martin Wagner
 * @copyright   2017 Neuberger Gebäudeautomation GmbH
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free and open source software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Following clarification and special exception to the GNU General Public
 * License is included to the distribution terms of CANopenNode:
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library. Thus, the terms and
 * conditions of the GNU General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this library give
 * you permission to link this library with independent modules to
 * produce an executable, regardless of the license terms of these
 * independent modules, and to copy and distribute the resulting
 * executable under terms of your choice, provided that you also meet,
 * for each linked independent module, the terms and conditions of the
 * license of that module. An independent module is a module which is
 * not derived from or based on this library. If you modify this
 * library, you may extend this exception to your version of the
 * library, but you are not obliged to do so. If you do not wish
 * to do so, delete this exception statement from your version.
 */


#ifndef CO_LSS_H
#define CO_LSS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_LSS LSS
 * @ingroup CO_CANopen
 * @{
 *
 * CANopen Layer Setting Services protocol
 *
 * LSS protocol is according to CiA DSP 305 V3.0.0.
 *
 * LSS services and protocols are used to inquire or to change the settings
 * of three parameters of the physical layer, data link layer, and application
 * layer on a CANopen device with LSS slave capability by a CANopen device
 * with LSS master capability via the CAN network.
 *
 * The following parameters may be inquired or changed:
 * - Node-ID of the CANopen device
 * - Bit timing parameters of the physical layer (bit rate)
 * - LSS address compliant to the identity object (1018h)
 *
 * The connection is established in one of two ways:
 * - addressing a node by it's 128 bit LSS address. This requires that the
 *   master already knows the node's LSS address.
 * - scanning the network for unknown nodes (Fastscan). Using this method,
 *   unknown devices can be found and configured one by one.
 *
 * Be aware that changing the bit rate is a critical step for the network. A
 * failure will render the network unusable!
 *
 * Using this implementation, only master or slave can be included in one
 * node at a time.
 *
 * For CAN identifiers see #CO_Default_CAN_ID_t
 */

#if CO_NO_LSS_CLIENT == 1 || CO_NO_LSS_SERVER == 1

/**
 * LSS protocol command specifiers
 *
 * The LSS protocols are executed between the LSS master device and the LSS
 * slave device(s) to implement the LSS services. Some LSS protocols require
 * a sequence of CAN messages.
 *
 * As identifying method only "LSS fastscan" is supported.
 */
typedef enum {
    CO_LSS_SWITCH_STATE_GLOBAL      = 0x04U, /**< Switch state global protocol */
    CO_LSS_SWITCH_STATE_SEL_VENDOR  = 0x40U, /**< Switch state selective protocol - Vendor ID */
    CO_LSS_SWITCH_STATE_SEL_PRODUCT = 0x41U, /**< Switch state selective protocol - Product code */
    CO_LSS_SWITCH_STATE_SEL_REV     = 0x42U, /**< Switch state selective protocol - Revision number */
    CO_LSS_SWITCH_STATE_SEL_SERIAL  = 0x43U, /**< Switch state selective protocol - Serial number */
    CO_LSS_SWITCH_STATE_SEL         = 0x44U, /**< Switch state selective protocol - Slave response */
    CO_LSS_CFG_NODE_ID              = 0x11U, /**< Configure node ID protocol */
    CO_LSS_CFG_BIT_TIMING           = 0x13U, /**< Configure bit timing parameter protocol */
    CO_LSS_CFG_ACTIVATE_BIT_TIMING  = 0x15U, /**< Activate bit timing parameter protocol */
    CO_LSS_CFG_STORE                = 0x17U, /**< Store configuration protocol */
    CO_LSS_IDENT_SLAVE              = 0x4FU, /**< LSS Fastscan response */
    CO_LSS_IDENT_FASTSCAN           = 0x51U, /**< LSS Fastscan protocol */
    CO_LSS_INQUIRE_VENDOR           = 0x5AU, /**< Inquire identity vendor-ID protocol */
    CO_LSS_INQUIRE_PRODUCT          = 0x5BU, /**< Inquire identity product-code protocol */
    CO_LSS_INQUIRE_REV              = 0x5CU, /**< Inquire identity revision-number protocol */
    CO_LSS_INQUIRE_SERIAL           = 0x5DU, /**< Inquire identity serial-number protocol */
    CO_LSS_INQUIRE_NODE_ID          = 0x5EU, /**< Inquire node-ID protocol */
} CO_LSS_cs_t;

/**
 * Macro to get service type group from command specifier
 * @{*/
#define CO_LSS_CS_SERVICE_IS_SWITCH_GLOBAL(cs) (cs == CO_LSS_SWITCH_STATE_GLOBAL)
#define CO_LSS_CS_SERVICE_IS_SWITCH_STATE_SELECTIVE(cs) (cs >= CO_LSS_SWITCH_STATE_SEL_VENDOR && cs <= CO_LSS_SWITCH_STATE_SEL)
#define CO_LSS_CS_SERVICE_IS_CONFIG(cs) (cs >= CO_LSS_CFG_NODE_ID && cs <= CO_LSS_CFG_STORE)
#define CO_LSS_CS_SERVICE_IS_INQUIRE(cs) (cs >= CO_LSS_INQUIRE_VENDOR && cs <= CO_LSS_INQUIRE_NODE_ID)
#define CO_LSS_CS_SERVICE_IS_IDENT(cs) (cs==CO_LSS_IDENT_SLAVE || cs==CO_LSS_IDENT_FASTSCAN)
/**@}*/

/**
 * Error codes for Configure node ID protocol
 */
typedef enum {
    CO_LSS_CFG_NODE_ID_OK           = 0x00U,/**< Protocol successfully completed */
    CO_LSS_CFG_NODE_ID_OUT_OF_RANGE = 0x01U,/**< NID out of range */
    CO_LSS_CFG_NODE_ID_MANUFACTURER = 0xFFU /**< Manufacturer specific error. No further support */
} CO_LSS_cfgNodeId_t;

/**
 * Error codes for Configure bit timing parameters protocol
 */
typedef enum {
    CO_LSS_CFG_BIT_TIMING_OK           = 0x00U,/**< Protocol successfully completed */
    CO_LSS_CFG_BIT_TIMING_OUT_OF_RANGE = 0x01U,/**< Bit timing / Bit rate not supported */
    CO_LSS_CFG_BIT_TIMING_MANUFACTURER = 0xFFU /**< Manufacturer specific error. No further support */
} CO_LSS_cfgBitTiming_t;

/**
 * Error codes for Store configuration protocol
 */
typedef enum {
    CO_LSS_CFG_STORE_OK            = 0x00U, /**< Protocol successfully completed */
    CO_LSS_CFG_STORE_NOT_SUPPORTED = 0x01U, /**< Store configuration not supported */
    CO_LSS_CFG_STORE_FAILED        = 0x02U, /**< Storage media access error */
    CO_LSS_CFG_STORE_MANUFACTURER  = 0xFFU  /**< Manufacturer specific error. No further support */
} CO_LSS_cfgStore_t;

/**
 * Fastscan BitCheck. BIT0 means all bits are checked for equality by slave.
 */
typedef enum {
    CO_LSS_FASTSCAN_BIT0    = 0x00U, /**< Least significant bit of IDnumbners bit area to be checked */
    /* ... */
    CO_LSS_FASTSCAN_BIT31   = 0x1FU, /**< dito */
    CO_LSS_FASTSCAN_CONFIRM = 0x80U  /**< All LSS slaves waiting for scan respond and previous scan is reset */
} CO_LSS_fastscan_bitcheck;

#define CO_LSS_FASTSCAN_BITCHECK_VALID(bit) ((bit>=CO_LSS_FASTSCAN_BIT0 && bit<=CO_LSS_FASTSCAN_BIT31) || bit==CO_LSS_FASTSCAN_CONFIRM)

/**
 * Fastscan LSSsub, LSSnext
 */
typedef enum {
    CO_LSS_FASTSCAN_VENDOR_ID = 0, /**< Vendor ID */
    CO_LSS_FASTSCAN_PRODUCT   = 1, /**< Product code */
    CO_LSS_FASTSCAN_REV       = 2, /**< Revision number */
    CO_LSS_FASTSCAN_SERIAL    = 3  /**< Serial number */
} CO_LSS_fastscan_lss_sub_next;

#define CO_LSS_FASTSCAN_LSS_SUB_NEXT_VALID(index) (index>=CO_LSS_FASTSCAN_VENDOR_ID && index<=CO_LSS_FASTSCAN_SERIAL)

/**
 * The LSS address is a 128 bit number, uniquely identifying each node. It
 * consists of the values in object 0x1018.
 */
typedef union {
    uint32_t addr[4];
    struct {
        uint32_t vendorID;
        uint32_t productCode;
        uint32_t revisionNumber;
        uint32_t serialNumber;
    };
} CO_LSS_address_t;

/**
 * LSS finite state automaton
 *
 * The LSS FSA shall provide the following states:
 * - Initial: Pseudo state, indicating the activation of the FSA.
 * - LSS waiting: In this state, the LSS slave device waits for requests.
 * - LSS configuration: In this state variables may be configured in the LSS slave.
 * - Final: Pseudo state, indicating the deactivation of the FSA.
 */
typedef enum {
    CO_LSS_STATE_WAITING = 0,                /**< LSS FSA waiting for requests*/
    CO_LSS_STATE_CONFIGURATION = 1,          /**< LSS FSA waiting for configuration*/
} CO_LSS_state_t;

/**
 * Definition of table_index for /CiA301/ bit timing table
 */
typedef enum {
    CO_LSS_BIT_TIMING_1000 = 0,              /**< 1000kbit/s */
    CO_LSS_BIT_TIMING_800  = 1,              /**< 800kbit/s */
    CO_LSS_BIT_TIMING_500  = 2,              /**< 500kbit/s */
    CO_LSS_BIT_TIMING_250  = 3,              /**< 250kbit/s */
    CO_LSS_BIT_TIMING_125  = 4,              /**< 125kbit/s */
    /* reserved            = 5 */
    CO_LSS_BIT_TIMING_50   = 6,              /**< 50kbit/s */
    CO_LSS_BIT_TIMING_20   = 7,              /**< 20kbit/s */
    CO_LSS_BIT_TIMING_10   = 8,              /**< 10kbit/s */
    CO_LSS_BIT_TIMING_AUTO = 9,              /**< Automatic bit rate detection */
} CO_LSS_bitTimingTable_t;

/**
 * Lookup table for conversion between bit timing table and numerical
 * bit rate
 */
static const uint16_t CO_LSS_bitTimingTableLookup[]  = {
     1000,
     800,
     500,
     250,
     125,
     0,
     50,
     20,
     10,
     0
};

/**
 * Macro to check if index contains valid bit timing
 */
#define CO_LSS_BIT_TIMING_VALID(index) (index != 5 && (index >= CO_LSS_BIT_TIMING_1000 && index <= CO_LSS_BIT_TIMING_AUTO))

/**
 * Invalid node ID triggers node ID assignment
 */
#define CO_LSS_NODE_ID_ASSIGNMENT 0xFFU

/**
 * Macro to check if node id is valid
 */
#define CO_LSS_NODE_ID_VALID(nid) ((nid >= 1 && nid <= 0x7F) || nid == CO_LSS_NODE_ID_ASSIGNMENT)

/**
 * Macro to check if two LSS addresses are equal
 */
#define CO_LSS_ADDRESS_EQUAL(/*CO_LSS_address_t*/ a1, /*CO_LSS_address_t*/ a2) \
     (a1.productCode == a2.productCode &&         \
      a1.revisionNumber == a2.revisionNumber &&   \
      a1.serialNumber == a2.serialNumber &&       \
      a1.vendorID == a2.vendorID)

#endif /* CO_NO_LSS_CLIENT == 1 || CO_NO_LSS_SERVER == 1 */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
