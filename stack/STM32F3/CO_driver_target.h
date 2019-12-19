/*
 * CAN module object for ST STM32F334 microcontroller.
 *
 * @file        CO_driver_target.h
 * @author      Janez Paternoster
 * @author      Ondrej Netik
 * @author      Vijayendra
 * @author      Jan van Lienden
 * @author      Petteri Mustonen
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
#include <stdbool.h>
#include <stddef.h>         /* for 'NULL' */
#include <stdint.h>         /* for 'int8_t' to 'uint64_t' */
#include "stm32f30x.h"

#define bool_t bool
#define CO_LITTLE_ENDIAN

/* Exported define -----------------------------------------------------------*/
#define PACKED_STRUCT               __attribute__((packed))
#define ALIGN_STRUCT_DWORD          __attribute__((aligned(4)))

/* Peripheral addresses */
#define ADDR_CAN1                   CAN1

/* Critical sections */
#define CO_LOCK_CAN_SEND()          __set_PRIMASK(1);
#define CO_UNLOCK_CAN_SEND()        __set_PRIMASK(0);

#define CO_LOCK_EMCY()              __set_PRIMASK(1);
#define CO_UNLOCK_EMCY()            __set_PRIMASK(0);

#define CO_LOCK_OD()                __set_PRIMASK(1);
#define CO_UNLOCK_OD()              __set_PRIMASK(0);

#define CLOCK_CAN                   RCC_APB1Periph_CAN1

#define CAN_REMAP_1                 /* Select CAN1 remap 1 */
#ifdef CAN1_NO_REMAP                /* CAN1 not remapped */
#define CLOCK_GPIO_CAN              RCC_APB2Periph_GPIOA
#define GPIO_Remapping_CAN          (0)
#define GPIO_CAN                    GPIOA
#define GPIO_Pin_CAN_RX             GPIO_Pin_11
#define GPIO_Pin_CAN_TX             GPIO_Pin_12
#define GPIO_PinSource_CAN_RX       GPIO_PinSource11
#define GPIO_PinSource_CAN_TX       GPIO_PinSource12
#define GPIO_CAN_Remap_State        DISABLE
#endif
#ifdef CAN_REMAP_1                  /* CAN1 remap 1 */
#define CLOCK_GPIO_CAN              RCC_AHBPeriph_GPIOB
#define GPIO_Remapping_CAN          GPIO_Remap1_CAN1
#define GPIO_CAN                    GPIOB
#define GPIO_Pin_CAN_RX             GPIO_Pin_8
#define GPIO_Pin_CAN_TX             GPIO_Pin_9
#define GPIO_PinSource_CAN_RX       GPIO_PinSource8
#define GPIO_PinSource_CAN_TX       GPIO_PinSource9
#define GPIO_CAN_Remap_State        ENABLE
#endif

#define CAN1_TX_INTERRUPTS          CAN1_TX_IRQn
#define CAN1_RX0_INTERRUPTS         CAN1_RX0_IRQn

#define CO_CAN_TXMAILBOX   ((uint8_t)0x00)

/* Timeout for initialization */

#define INAK_TIMEOUT        ((uint32_t)0x0000FFFF)
/* Data types */
typedef float                   float32_t;
typedef long double             float64_t;
typedef char                    char_t;
typedef unsigned char           oChar_t;
typedef unsigned char           domain_t;


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
    uint16_t           ident;
    uint16_t           mask;
    void               *object;
    void              (*pFunct)(void *object, const CO_CANrxMsg_t *message); // Changed by VJ
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
    CAN_TypeDef        *CANdriverState;         /* STM32F4xx specific */
    CO_CANrx_t         *rxArray;
    uint16_t            rxSize;
    CO_CANtx_t         *txArray;
    uint16_t            txSize;
    volatile bool     CANnormal;
    volatile bool     useCANrxFilters;
    //volatile uint8_t    useCANrxFilters;
    volatile uint8_t    bufferInhibitFlag;
    volatile uint8_t    firstCANtxMessage;
    volatile uint16_t   CANtxCount;
    uint32_t            errOld;
    void               *em;
}CO_CANmodule_t;


/* CAN interrupts receives and transmits CAN messages. */
void CO_CANinterrupt_Rx(CO_CANmodule_t *CANmodule);
void CO_CANinterrupt_Tx(CO_CANmodule_t *CANmodule);

#endif /* CO_DRIVER_TARGET_H */
