/*
 * CAN module object for Microchip dsPIC30F microcontroller.
 *
 * @file        CO_driver.h
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
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


#include <p30Fxxxx.h>       /* processor header file */
#include <stddef.h>         /* for 'NULL' */
#include <stdint.h>         /* for 'int8_t' to 'uint64_t' */
#include <stdbool.h>        /* for true and false */


/* CAN module base address */
    #define ADDR_CAN1               0x300
    #define ADDR_CAN2               0x3C0


/* Critical sections */
    #define CO_LOCK_CAN_SEND()      asm volatile ("disi #0x3FFF")
    #define CO_UNLOCK_CAN_SEND()    asm volatile ("disi #0x0000")

    #define CO_LOCK_EMCY()          asm volatile ("disi #0x3FFF")
    #define CO_UNLOCK_EMCY()        asm volatile ("disi #0x0000")

    #define CO_LOCK_OD()            asm volatile ("disi #0x3FFF")
    #define CO_UNLOCK_OD()          asm volatile ("disi #0x0000")


/* Data types */
    /* int8_t to uint64_t are defined in stdint.h */
    typedef unsigned char           bool_t;
    typedef float                   float32_t;
    typedef long double             float64_t;
    typedef char                    char_t;
    typedef unsigned char           oChar_t;
    typedef unsigned char           domain_t;


/* CAN bit rates
 *
 * CAN bit rates are initializers for array of eight CO_CANbitRateData_t
 * objects.
 *
 * Macros are not used by driver itself, they may be used by application with
 * combination with object CO_CANbitRateData_t.
 * Application must declare following global variable depending on CO_FCY used:
 * const CO_CANbitRateData_t  CO_CANbitRateData[8] = {CO_CANbitRateDataInitializers};
 *
 * There are initializers for eight objects, which corresponds to following
 * CAN bit rates (in kbps): 10, 20, 50, 125, 250, 500, 800, 1000.
 *
 * CO_FCY is internal instruction cycle clock frequency in kHz units. It is
 * calculated from oscillator frequency (FOSC [in kHz]) and PLL mode:
 *     - If PLL is not used -> FCY = FOSC / 4,
 *     - If PLL x 4 is used -> FCY = FOSC,
 *     - If PLL x 16 is used -> FCY = FOSC * 4
 *
 * Possible values for FCY are (in three groups):
 *     - Optimal CAN bit timing on all Baud Rates: 4000, 6000, 16000, 24000.
 *     - Not so optimal CAN bit timing on all Baud Rates: 2000, 8000.
 *     - not all CANopen Baud Rates possible: 1000, 1500, 2500, 3000, 5000,
 *       10000, 12000, 20000, 28000, 30000, 1843 (internal FRC, no PLL),
 *       7372 (internal FRC + 4*PLL).
 */
#ifdef CO_FCY
    /* Macros, which divides K into (SJW + PROP + PhSeg1 + PhSeg2) */
    #define TQ_x_4    1, 1, 1, 1
    #define TQ_x_5    1, 1, 2, 1
    #define TQ_x_6    1, 1, 3, 1
    #define TQ_x_8    1, 2, 3, 2
    #define TQ_x_9    1, 2, 4, 2
    #define TQ_x_10   1, 3, 4, 2
    #define TQ_x_12   1, 3, 6, 2
    #define TQ_x_14   1, 4, 7, 2
    #define TQ_x_15   1, 4, 8, 2  /* good timing */
    #define TQ_x_16   1, 5, 8, 2  /* good timing */
    #define TQ_x_17   1, 6, 8, 2  /* good timing */
    #define TQ_x_18   1, 7, 8, 2  /* good timing */
    #define TQ_x_19   1, 8, 8, 2  /* good timing */
    #define TQ_x_20   1, 8, 8, 3  /* good timing */
    #define TQ_x_21   1, 8, 8, 4
    #define TQ_x_25   1, 8, 8, 8

    #if CO_FCY == 1000
        #define CO_CANbitRateDataInitializers  \
        {4, 10,  TQ_x_20},   /*CAN=10kbps*/    \
        {4, 5,   TQ_x_20},   /*CAN=20kbps*/    \
        {4, 2,   TQ_x_20},   /*CAN=50kbps*/    \
        {4, 1,   TQ_x_16},   /*CAN=125kbps*/   \
        {4, 1,   TQ_x_8 },   /*CAN=250kbps*/   \
        {4, 1,   TQ_x_4 },   /*CAN=500kbps*/   \
        {4, 1,   TQ_x_4 },   /*Not possible*/  \
        {4, 1,   TQ_x_4 }    /*Not possible*/
    #elif CO_FCY == 1500
        #define CO_CANbitRateDataInitializers  \
        {4, 15,  TQ_x_20},   /*CAN=10kbps*/    \
        {4, 10,  TQ_x_15},   /*CAN=20kbps*/    \
        {4, 4,   TQ_x_15},   /*CAN=50kbps*/    \
        {4, 2,   TQ_x_12},   /*CAN=125kbps*/   \
        {4, 1,   TQ_x_12},   /*CAN=250kbps*/   \
        {4, 1,   TQ_x_6 },   /*CAN=500kbps*/   \
        {4, 1,   TQ_x_6 },   /*Not possible*/  \
        {4, 1,   TQ_x_6 }    /*Not possible*/
    #elif CO_FCY == 1843                /* internal FRC, no PLL */
        #define CO_CANbitRateDataInitializers  \
        {4, 23,  TQ_x_16},   /*CAN=10kbps*/    \
        {4, 23,  TQ_x_8 },   /*CAN=20kbps*/    \
        {4, 23,  TQ_x_8 },   /*Not possible*/  \
        {4, 23,  TQ_x_8 },   /*Not possible*/  \
        {4, 23,  TQ_x_8 },   /*Not possible*/  \
        {4, 23,  TQ_x_8 },   /*Not possible*/  \
        {4, 23,  TQ_x_8 },   /*Not possible*/  \
        {4, 23,  TQ_x_8 }    /*Not possible*/
    #elif CO_FCY == 2000
        #define CO_CANbitRateDataInitializers  \
        {4, 25,  TQ_x_16},   /*CAN=10kbps*/    \
        {4, 10,  TQ_x_20},   /*CAN=20kbps*/    \
        {4, 5,   TQ_x_16},   /*CAN=50kbps*/    \
        {4, 2,   TQ_x_16},   /*CAN=125kbps*/   \
        {4, 1,   TQ_x_16},   /*CAN=250kbps*/   \
        {4, 1,   TQ_x_8 },   /*CAN=500kbps*/   \
        {4, 1,   TQ_x_5 },   /*CAN=800kbps*/   \
        {4, 1,   TQ_x_4 }    /*CAN=1000kbps*/
    #elif CO_FCY == 2500
        #define CO_CANbitRateDataInitializers  \
        {4, 25,  TQ_x_20},   /*CAN=10kbps*/    \
        {4, 10,  TQ_x_25},   /*CAN=20kbps*/    \
        {4, 5,   TQ_x_20},   /*CAN=50kbps*/    \
        {4, 2,   TQ_x_20},   /*CAN=125kbps*/   \
        {4, 1,   TQ_x_20},   /*CAN=250kbps*/   \
        {4, 1,   TQ_x_10},   /*CAN=500kbps*/   \
        {4, 1,   TQ_x_10},   /*Not possible*/  \
        {4, 1,   TQ_x_5 }    /*CAN=1000kbps*/
    #elif CO_FCY == 3000
        #define CO_CANbitRateDataInitializers  \
        {4, 40,  TQ_x_15},   /*CAN=10kbps*/    \
        {4, 20,  TQ_x_15},   /*CAN=20kbps*/    \
        {4, 8,   TQ_x_15},   /*CAN=50kbps*/    \
        {4, 3,   TQ_x_16},   /*CAN=125kbps*/   \
        {4, 2,   TQ_x_12},   /*CAN=250kbps*/   \
        {4, 1,   TQ_x_12},   /*CAN=500kbps*/   \
        {4, 1,   TQ_x_12},   /*Not possible*/  \
        {4, 1,   TQ_x_6 }    /*CAN=1000kbps*/
    #elif CO_FCY == 4000
        #define CO_CANbitRateDataInitializers  \
        {4, 50,  TQ_x_16},   /*CAN=10kbps*/    \
        {4, 25,  TQ_x_16},   /*CAN=20kbps*/    \
        {4, 10,  TQ_x_16},   /*CAN=50kbps*/    \
        {4, 4,   TQ_x_16},   /*CAN=125kbps*/   \
        {4, 2,   TQ_x_16},   /*CAN=250kbps*/   \
        {4, 1,   TQ_x_16},   /*CAN=500kbps*/   \
        {4, 1,   TQ_x_10},   /*CAN=800kbps*/   \
        {4, 1,   TQ_x_8 }    /*CAN=1000kbps*/
    #elif CO_FCY == 5000
        #define CO_CANbitRateDataInitializers  \
        {4, 50,  TQ_x_20},   /*CAN=10kbps*/    \
        {4, 25,  TQ_x_20},   /*CAN=20kbps*/    \
        {4, 10,  TQ_x_20},   /*CAN=50kbps*/    \
        {4, 5,   TQ_x_16},   /*CAN=125kbps*/   \
        {4, 2,   TQ_x_20},   /*CAN=250kbps*/   \
        {4, 1,   TQ_x_20},   /*CAN=500kbps*/   \
        {4, 1,   TQ_x_20},   /*Not possible*/  \
        {4, 1,   TQ_x_10}    /*CAN=1000kbps*/
    #elif CO_FCY == 6000
        #define CO_CANbitRateDataInitializers  \
        {4, 63,  TQ_x_19},   /*CAN=10kbps*/    \
        {4, 40,  TQ_x_15},   /*CAN=20kbps*/    \
        {4, 15,  TQ_x_16},   /*CAN=50kbps*/    \
        {4, 6,   TQ_x_16},   /*CAN=125kbps*/   \
        {4, 3,   TQ_x_16},   /*CAN=250kbps*/   \
        {4, 2,   TQ_x_12},   /*CAN=500kbps*/   \
        {4, 1,   TQ_x_15},   /*CAN=800kbps*/   \
        {4, 1,   TQ_x_12}    /*CAN=1000kbps*/
    #elif CO_FCY == 7372                /* internal FRC + 4*PLL */
        #define CO_CANbitRateDataInitializers  \
        {1, 23,  TQ_x_16},   /*CAN=10kbps*/    \
        {4, 46,  TQ_x_16},   /*CAN=20kbps*/    \
        {4, 14,  TQ_x_21},   /*CAN=50kbps*/    \
        {4, 13,  TQ_x_9 },   /*CAN=125kbps*/   \
        {4, 13,  TQ_x_9 },   /*Not possible*/  \
        {4, 13,  TQ_x_9 },   /*Not possible*/  \
        {4, 13,  TQ_x_9 },   /*Not possible*/  \
        {4, 13,  TQ_x_9 }    /*Not possible*/
    #elif CO_FCY == 8000
        #define CO_CANbitRateDataInitializers  \
        {1, 25,  TQ_x_16},   /*CAN=10kbps*/    \
        {1, 10,  TQ_x_20},   /*CAN=20kbps*/    \
        {1, 5,   TQ_x_16},   /*CAN=50kbps*/    \
        {1, 2,   TQ_x_16},   /*CAN=125kbps*/   \
        {1, 1,   TQ_x_16},   /*CAN=250kbps*/   \
        {1, 1,   TQ_x_8 },   /*CAN=500kbps*/   \
        {1, 1,   TQ_x_5 },   /*CAN=800kbps*/   \
        {1, 1,   TQ_x_4 }    /*CAN=1000kbps*/
    #elif CO_FCY == 10000
        #define CO_CANbitRateDataInitializers  \
        {1, 25,  TQ_x_20},   /*CAN=10kbps*/    \
        {1, 10,  TQ_x_25},   /*CAN=20kbps*/    \
        {1, 5,   TQ_x_20},   /*CAN=50kbps*/    \
        {1, 2,   TQ_x_20},   /*CAN=125kbps*/   \
        {1, 1,   TQ_x_20},   /*CAN=250kbps*/   \
        {1, 1,   TQ_x_10},   /*CAN=500kbps*/   \
        {1, 1,   TQ_x_10},   /*Not possible*/  \
        {1, 1,   TQ_x_5 }    /*CAN=1000kbps*/
    #elif CO_FCY == 12000
        #define CO_CANbitRateDataInitializers  \
        {1, 40,  TQ_x_15},   /*CAN=10kbps*/    \
        {1, 20,  TQ_x_15},   /*CAN=20kbps*/    \
        {1, 8,   TQ_x_15},   /*CAN=50kbps*/    \
        {1, 3,   TQ_x_16},   /*CAN=125kbps*/   \
        {1, 2,   TQ_x_12},   /*CAN=250kbps*/   \
        {1, 1,   TQ_x_12},   /*CAN=500kbps*/   \
        {1, 1,   TQ_x_12},   /*Not possible*/  \
        {1, 1,   TQ_x_6 }    /*CAN=1000kbps*/
    #elif CO_FCY == 16000
        #define CO_CANbitRateDataInitializers  \
        {1, 50,  TQ_x_16},   /*CAN=10kbps*/    \
        {1, 25,  TQ_x_16},   /*CAN=20kbps*/    \
        {1, 10,  TQ_x_16},   /*CAN=50kbps*/    \
        {1, 4,   TQ_x_16},   /*CAN=125kbps*/   \
        {1, 2,   TQ_x_16},   /*CAN=250kbps*/   \
        {1, 1,   TQ_x_16},   /*CAN=500kbps*/   \
        {1, 1,   TQ_x_10},   /*CAN=800kbps*/   \
        {1, 1,   TQ_x_8 }    /*CAN=1000kbps*/
    #elif CO_FCY == 20000
        #define CO_CANbitRateDataInitializers  \
        {1, 50,  TQ_x_20},   /*CAN=10kbps*/    \
        {1, 25,  TQ_x_20},   /*CAN=20kbps*/    \
        {1, 10,  TQ_x_20},   /*CAN=50kbps*/    \
        {1, 5,   TQ_x_16},   /*CAN=125kbps*/   \
        {1, 2,   TQ_x_20},   /*CAN=250kbps*/   \
        {1, 1,   TQ_x_20},   /*CAN=500kbps*/   \
        {1, 1,   TQ_x_20},   /*Not possible*/  \
        {1, 1,   TQ_x_10}    /*CAN=1000kbps*/
    #elif CO_FCY == 24000
        #define CO_CANbitRateDataInitializers  \
        {1, 63,  TQ_x_19},   /*CAN=10kbps*/    \
        {1, 40,  TQ_x_15},   /*CAN=20kbps*/    \
        {1, 15,  TQ_x_16},   /*CAN=50kbps*/    \
        {1, 6,   TQ_x_16},   /*CAN=125kbps*/   \
        {1, 3,   TQ_x_16},   /*CAN=250kbps*/   \
        {1, 2,   TQ_x_12},   /*CAN=500kbps*/   \
        {1, 1,   TQ_x_15},   /*CAN=800kbps*/   \
        {1, 1,   TQ_x_12}    /*CAN=1000kbps*/
    #elif CO_FCY == 28000
        #define CO_CANbitRateDataInitializers  \
        {1, 56,  TQ_x_25},   /*CAN=10kbps*/    \
        {1, 35,  TQ_x_20},   /*CAN=20kbps*/    \
        {1, 14,  TQ_x_20},   /*CAN=50kbps*/    \
        {1, 7,   TQ_x_16},   /*CAN=125kbps*/   \
        {1, 4,   TQ_x_14},   /*CAN=250kbps*/   \
        {1, 2,   TQ_x_14},   /*CAN=500kbps*/   \
        {1, 2,   TQ_x_14},   /*Not possible*/  \
        {1, 1,   TQ_x_14}    /*CAN=1000kbps*/
    #elif CO_FCY == 30000
        #define CO_CANbitRateDataInitializers  \
        {1, 60,  TQ_x_25},   /*CAN=10kbps*/    \
        {1, 50,  TQ_x_15},   /*CAN=20kbps*/    \
        {1, 20,  TQ_x_15},   /*CAN=50kbps*/    \
        {1, 8,   TQ_x_15},   /*CAN=125kbps*/   \
        {1, 4,   TQ_x_15},   /*CAN=250kbps*/   \
        {1, 2,   TQ_x_15},   /*CAN=500kbps*/   \
        {1, 2,   TQ_x_15},   /*Not possible*/  \
        {1, 1,   TQ_x_15}    /*CAN=1000kbps*/
    #else
        #error define_CO_FCY CO_FCY not supported
    #endif
#endif


/* Structure contains timing coefficients for CAN module.
 *
 * CAN baud rate is calculated from following equations:
 * FCAN = FCY * Scale                  - Input frequency to CAN module (MAX 30MHz for dsPIC30F)
 * TQ = 2 * BRP / FCAN                 - Time Quanta
 * BaudRate = 1 / (TQ * K)             - Can bus Baud Rate
 * K = SJW + PROP + PhSeg1 + PhSeg2    - Number of Time Quantas
 */
typedef struct{
    uint8_t   scale;    /* (1 or 4) Scales FCY clock - dsPIC30F specific */
    uint8_t   BRP;      /* (1...64) Baud Rate Prescaler */
    uint8_t   SJW;      /* (1...4) SJW time */
    uint8_t   PROP;     /* (1...8) PROP time */
    uint8_t   phSeg1;   /* (1...8) Phase Segment 1 time */
    uint8_t   phSeg2;   /* (1...8) Phase Segment 2 time */
}CO_CANbitRateData_t;


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


/* CAN receive message structure as aligned in CAN module. */
typedef struct{
    uint16_t    ident;          /* Standard Identifier as aligned in CAN module. 16 bits:
                                  'UUUSSSSS SSSSSSRE' (U: unused; S: SID; R=SRR; E=IDE). */
    uint16_t    extIdent;       /* Extended identifier, not used here */
    uint16_t    DLC      :4;    /* Data length code (bits 0...3) */
    uint16_t    DLCrest  :12;   /* Not used here (bits 4..15) */
    uint8_t     data[8];        /* 8 data bytes */
    uint16_t    CON;            /* Control word */
}CO_CANrxMsg_t;


/* Received message object */
typedef struct{
    uint16_t            ident;
    uint16_t            mask;
    void               *object;
    void              (*pFunct)(void *object, const CO_CANrxMsg_t *message);
}CO_CANrx_t;


/* Transmit message object. */
typedef struct{
    uint16_t            ident;      /* Standard Identifier as aligned in CAN module. 16 bits:
                                      'SSSSSUUU SSSSSSRE' (U: unused; S: SID; R=SRR; E=IDE). */
    uint8_t             DLC;
    uint8_t             data[8];
    volatile bool_t     bufferFull;
    volatile bool_t     syncFlag;
}CO_CANtx_t;


/* CAN module object. */
typedef struct{
    uint16_t            CANbaseAddress;
    CO_CANrx_t         *rxArray;
    uint16_t            rxSize;
    CO_CANtx_t         *txArray;
    uint16_t            txSize;
    volatile bool_t     CANnormal;
    volatile bool_t     useCANrxFilters;
    volatile bool_t     bufferInhibitFlag;
    volatile bool_t     firstCANtxMessage;
    volatile uint16_t   CANtxCount;
    uint32_t            errOld;
    void               *em;
}CO_CANmodule_t;


/* Endianes */
#define CO_LITTLE_ENDIAN


/* Request CAN configuration or normal mode */
void CO_CANsetConfigurationMode(uint16_t CANbaseAddress);
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule);


/* Initialize CAN module object. */
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        uint16_t                CANbaseAddress,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate);


/* Switch off CANmodule. */
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule);


/* Read CAN identifier */
uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg);


/* Configure CAN message receive buffer. */
CO_ReturnError_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint16_t                mask,
        bool_t                  rtr,
        void                   *object,
        void                  (*pFunct)(void *object, const CO_CANrxMsg_t *message));


/* Configure CAN message transmit buffer. */
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        bool_t                  rtr,
        uint8_t                 noOfBytes,
        bool_t                  syncFlag);


/* Send CAN message. */
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer);


/* Clear all synchronous TPDOs from CAN module transmit buffers. */
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule);


/* Verify all errors of CAN module. */
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule);


/* CAN interrupt receives and transmits CAN messages.
 *
 * Function must be called directly from _C1Interrupt or _C2Interrupt with
 * high priority. dsPIC30F uses two receive buffers and one transmit buffer.
 */
void CO_CANinterrupt(CO_CANmodule_t *CANmodule);


#endif
