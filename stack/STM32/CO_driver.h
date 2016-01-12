/*
 * CAN module object for ST STM32F103 microcontroller.
 *
 * @file        CO_driver.h
 * @author      Janez Paternoster
 * @author      Ondrej Netik
 * @author      Vijayendra
 * @author      Jan van Lienden
 * @copyright   2013 Janez Paternoster
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


#ifndef CO_DRIVER_H
#define CO_DRIVER_H


/* For documentation see file drvTemplate/CO_driver.h */


/* Includes ------------------------------------------------------------------*/
#include "common.h"
#include "stm32f10x_conf.h"

/* Exported define -----------------------------------------------------------*/
#define PACKED_STRUCT               __attribute__((packed))
#define ALIGN_STRUCT_DWORD          __attribute__((aligned(4)))

/* Peripheral addresses */
    #define ADDR_CAN1               CAN1
    #define TMIDxR_TXRQ  ((uint32_t)0x00000001) /* Transmit mailbox request */

/* Critical sections */
    #define CO_LOCK_CAN_SEND()      __set_PRIMASK(1);
    #define CO_UNLOCK_CAN_SEND()    __set_PRIMASK(0);

    #define CO_LOCK_EMCY()          __set_PRIMASK(1);
    #define CO_UNLOCK_EMCY()        __set_PRIMASK(0);

    #define CO_LOCK_OD()            __set_PRIMASK(1);
    #define CO_UNLOCK_OD()          __set_PRIMASK(0);

    
#define CLOCK_CAN                   RCC_APB1Periph_CAN1

#define CAN_REMAP_2                 /* Select CAN1 remap 2 */
#ifdef CAN1_NO_REMAP                /* CAN1 not remapped */
#define CLOCK_GPIO_CAN              RCC_APB2Periph_GPIOA
#define GPIO_Remapping_CAN          (0)
#define GPIO_CAN                    GPIOA
#define GPIO_Pin_CAN_RX             GPIO_Pin_11
#define GPIO_Pin_CAN_TX             GPIO_Pin_12
#define GPIO_CAN_Remap_State        DISABLE
#endif
#ifdef CAN_REMAP1                  /* CAN1 remap 1 */
#define CLOCK_GPIO_CAN              RCC_APB2Periph_GPIOB
#define GPIO_Remapping_CAN          GPIO_Remap1_CAN1
#define GPIO_CAN                    GPIOB
#define GPIO_Pin_CAN_RX             GPIO_Pin_8
#define GPIO_Pin_CAN_TX             GPIO_Pin_9
#define GPIO_CAN_Remap_State        ENABLE
#endif
#ifdef CAN_REMAP_2                 /* CAN1 remap 2 */
#define CLOCK_GPIO_CAN              RCC_APB2Periph_GPIOD
#define GPIO_Remapping_CAN          GPIO_Remap2_CAN1
#define GPIO_CAN                    GPIOD
#define GPIO_Pin_CAN_RX             GPIO_Pin_0
#define GPIO_Pin_CAN_TX             GPIO_Pin_1
#define GPIO_CAN_Remap_State        ENABLE
#endif

#ifdef STM32F10X_CL
#define CAN1_TX_INTERRUPTS          CAN1_TX_IRQn
#define CAN1_RX0_INTERRUPTS         CAN1_RX0_IRQn
#else
#define CAN1_TX_INTERRUPTS          USB_HP_CAN1_TX_IRQn
#define CAN1_RX0_INTERRUPTS         USB_LP_CAN1_RX0_IRQn
#endif

#define CAN_TXMAILBOX_0   ((uint8_t)0x00)
#define CAN_TXMAILBOX_1   ((uint8_t)0x01)
#define CAN_TXMAILBOX_2   ((uint8_t)0x02)

/* Timeout for initialization */

#define INAK_TIMEOUT        ((uint32_t)0x0000FFFF)
/* Data types */
    typedef float                   float32_t;
    typedef long double             float64_t;
    typedef char                    char_t;
    typedef unsigned char           oChar_t;
    typedef unsigned char           domain_t;

/* Return values */
typedef enum{
    CO_ERROR_NO                 = 0,
    CO_ERROR_ILLEGAL_ARGUMENT   = -1,
    CO_ERROR_OUT_OF_MEMORY      = -2,
    CO_ERROR_TIMEOUT            = -3,
    CO_ERROR_ILLEGAL_BAUDRATE   = -4,
    CO_ERROR_RX_OVERFLOW        = -5,
    CO_ERROR_RX_PDO_OVERFLOW    = -6,
    CO_ERROR_RX_MSG_LENGTH      = -7,
    CO_ERROR_RX_PDO_LENGTH      = -8,
    CO_ERROR_TX_OVERFLOW        = -9,
    CO_ERROR_TX_PDO_WINDOW      = -10,
    CO_ERROR_TX_UNCONFIGURED    = -11,
    CO_ERROR_PARAMETERS         = -12,
    CO_ERROR_DATA_CORRUPT       = -13,
    CO_ERROR_CRC                = -14
}CO_ReturnError_t;

/* CAN receive message structure as aligned in CAN module.
 * prevzato z stm32f10_can.h - velikostne polozky a poradi odpovidaji. */
typedef struct{
    uint32_t    ident;          /* Standard Identifier */
    uint32_t    ExtId;          /* Specifies the extended identifier */
    uint8_t     IDE;            /* Specifies the type of identifier for the
                                   message that will be received */
    uint8_t     RTR;            /* Remote Transmission Request bit */
    uint8_t     DLC;            /* Data length code (bits 0...3) */
    uint8_t     data[8];        /* 8 data bytes */
    uint8_t     FMI;            /* Specifies the index of the filter the message
                                   stored in the mailbox passes through */
}CO_CANrxMsg_t;


/* Received message object */
typedef struct{
    uint16_t            ident;
    uint16_t            mask;
    void               *object;
    void              (*pFunct)(void *object, CanRxMsg *message); // Changed by VJ
}CO_CANrx_t;


/* Transmit message object. */
typedef struct{
    uint32_t            ident;
    uint8_t             DLC;
    uint8_t             data[8];
    volatile uint8_t    bufferFull;
    volatile uint8_t    syncFlag;
}CO_CANtx_t;/* ALIGN_STRUCT_DWORD; */


/* CAN module object. */
typedef struct{
    CAN_TypeDef        *CANbaseAddress;         /* STM32F4xx specific */
    CO_CANrx_t         *rxArray;
    uint16_t            rxSize;
    CO_CANtx_t         *txArray;
    uint16_t            txSize;
    volatile bool_t     CANnormal;
    volatile bool_t     useCANrxFilters;
    volatile uint8_t    useCANrxFilters;
    volatile uint8_t    bufferInhibitFlag;
    volatile uint8_t    firstCANtxMessage;
    volatile uint16_t   CANtxCount;
    uint32_t            errOld;
    void               *em;
}CO_CANmodule_t;

/* Init CAN Led Interface */
typedef enum {
    eCoLed_None = 0,
    eCoLed_Green = 1,
    eCoLed_Red = 2,
} eCoLeds;

/* Exported variables -----------------------------------------------------------*/

/* Exported functions -----------------------------------------------------------*/
void InitCanLeds(void);
void CanLedsOn(eCoLeds led);
void CanLedsOff(eCoLeds led);
void CanLedsSet(eCoLeds led);


/* Request CAN configuration or normal mode */
//void CO_CANsetConfigurationMode(CAN_TypeDef *CANbaseAddress);
//void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule);

/* Initialize CAN module object. */
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        CAN_TypeDef            *CANbaseAddress,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate);


/* Switch off CANmodule. */
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule);


/* Read CAN identifier */
//uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg);


/* Configure CAN message receive buffer. */
CO_ReturnError_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint16_t                mask,
        int8_t                  rtr,
        void                   *object,
        void                  (*pFunct)(void *object, CO_CANrxMsg_t *message));


/* Configure CAN message transmit buffer. */
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        int8_t                  rtr,
        uint8_t                 noOfBytes,
        int8_t                  syncFlag);

/* Send CAN message. */
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer);


/* Clear all synchronous TPDOs from CAN module transmit buffers. */
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule);


/* Verify all errors of CAN module. */
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule);


/* CAN interrupts receives and transmits CAN messages. */
void CO_CANinterrupt_Rx(CO_CANmodule_t *CANmodule);
void CO_CANinterrupt_Tx(CO_CANmodule_t *CANmodule);
void CO_CANinterrupt_Status(CO_CANmodule_t *CANmodule);


#endif
