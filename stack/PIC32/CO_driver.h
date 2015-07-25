/*
 * CAN module object for Microchip PIC32MX microcontroller.
 *
 * @file        CO_driver.h
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <http://canopennode.sourceforge.net>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef CO_DRIVER_H
#define CO_DRIVER_H


/* For documentation see file drvTemplate/CO_driver.h */


#include <p32xxxx.h>        /* processor header file */
#include <plib.h>
#include <stddef.h>         /* for 'NULL' */
#include <stdint.h>         /* for 'int8_t' to 'uint64_t' */


/* CAN module base address */
#define ADDR_CAN1    0
#define ADDR_CAN2    (_CAN2_BASE_ADDRESS - _CAN1_BASE_ADDRESS)


/* Disabling interrupts */
extern unsigned int CO_interruptStatus;
#define CO_DISABLE_INTERRUPTS() CO_interruptStatus = INTDisableInterrupts()
#define CO_ENABLE_INTERRUPTS()  INTRestoreInterrupts(CO_interruptStatus)


/* Data types */
    typedef unsigned char CO_bool_t;
    typedef enum{
        CO_false = 0,
        CO_true = 1
    }CO_boolval_t;
    /* int8_t to uint64_t are defined in stdint.h */
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
 * Application must declare following global variable depending on CO_FSYS used:
 * const CO_CANbitRateData_t  CO_CANbitRateData[8] = {CO_CANbitRateDataInitializers};
 *
 * There are initializers for eight objects, which corresponds to following
 * CAN bit rates (in kbps): 10, 20, 50, 125, 250, 500, 800, 1000.
 *
 * CO_FSYS is internal instruction cycle clock frequency in kHz units. See
 * PIC32MX documentation for more information on FSYS.
 *
 * Available values for FSYS:
 *    kbps = | 10 | 20 | 50 | 125 | 250 | 500 | 800 | 1000
 *    -------+----+----+----+-----+-----+-----+-----+-----
 *     4 Mhz |  O |  O |  O |  O  |  p  |  -  |  -  |  -
 *     8 Mhz |  O |  O |  O |  O  |  O  |  p  |  -  |  -
 *    12 Mhz |  O |  O |  O |  O  |  p  |  p  |  -  |  -
 *    16 Mhz |  O |  O |  O |  O  |  O  |  O  |  p  |  p
 *    20 Mhz |  O |  O |  O |  O  |  O  |  O  |  -  |  p
 *    24 Mhz |  O |  O |  O |  O  |  O  |  p  |  O  |  p
 *    32 Mhz |  p |  O |  O |  O  |  O  |  O  |  p  |  O
 *    36 Mhz |  - |  O |  O |  O  |  O  |  O  |  -  |  O
 *    40 Mhz |  - |  O |  O |  O  |  O  |  O  |  p  |  O
 *    48 Mhz |  - |  O |  O |  O  |  O  |  O  |  O  |  p
 *    56 Mhz |  - |  p |  O |  O  |  O  |  p  | (p) |  p
 *    64 Mhz |  - |  p |  O |  O  |  O  |  O  |  O  |  O
 *    72 Mhz |  - |  - |  O |  O  |  O  |  O  |  O  |  O
 *    80 Mhz |  - |  - |  O |  O  |  O  |  O  |  p  |  O
 *    ----------------------------------------------------
 *    (O=optimal; p=possible; -=not possible)
 */
#ifdef CO_FSYS
    /* Macros, which divides K into (SJW + PROP + PhSeg1 + PhSeg2) */
    #define TQ_x_7    1, 2, 3, 1
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
    #define TQ_x_22   1, 8, 8, 5
    #define TQ_x_23   1, 8, 8, 6
    #define TQ_x_24   1, 8, 8, 7
    #define TQ_x_25   1, 8, 8, 8

    #if CO_FSYS == 4000
        #define CO_CANbitRateDataInitializers  \
        {10,  TQ_x_20},   /*CAN=10kbps*/       \
        {5,   TQ_x_20},   /*CAN=20kbps*/       \
        {2,   TQ_x_20},   /*CAN=50kbps*/       \
        {1,   TQ_x_16},   /*CAN=125kbps*/      \
        {1,   TQ_x_8 },   /*CAN=250kbps*/      \
        {1,   TQ_x_8 },   /*Not possible*/     \
        {1,   TQ_x_8 },   /*Not possible*/     \
        {1,   TQ_x_8 }    /*Not possible*/
    #elif CO_FSYS == 8000
        #define CO_CANbitRateDataInitializers  \
        {25,  TQ_x_16},   /*CAN=10kbps*/       \
        {10,  TQ_x_20},   /*CAN=20kbps*/       \
        {5,   TQ_x_16},   /*CAN=50kbps*/       \
        {2,   TQ_x_16},   /*CAN=125kbps*/      \
        {1,   TQ_x_16},   /*CAN=250kbps*/      \
        {1,   TQ_x_8 },   /*CAN=500kbps*/      \
        {1,   TQ_x_8 },   /*Not possible*/     \
        {1,   TQ_x_8 }    /*Not possible*/
    #elif CO_FSYS == 12000
        #define CO_CANbitRateDataInitializers  \
        {40,  TQ_x_15},   /*CAN=10kbps*/       \
        {20,  TQ_x_15},   /*CAN=20kbps*/       \
        {8,   TQ_x_15},   /*CAN=50kbps*/       \
        {3,   TQ_x_16},   /*CAN=125kbps*/      \
        {2,   TQ_x_12},   /*CAN=250kbps*/      \
        {1,   TQ_x_12},   /*CAN=500kbps*/      \
        {1,   TQ_x_12},   /*Not possible*/     \
        {1,   TQ_x_12}    /*Not possible*/
    #elif CO_FSYS == 16000
        #define CO_CANbitRateDataInitializers  \
        {50,  TQ_x_16},   /*CAN=10kbps*/       \
        {25,  TQ_x_16},   /*CAN=20kbps*/       \
        {10,  TQ_x_16},   /*CAN=50kbps*/       \
        {4,   TQ_x_16},   /*CAN=125kbps*/      \
        {2,   TQ_x_16},   /*CAN=250kbps*/      \
        {1,   TQ_x_16},   /*CAN=500kbps*/      \
        {1,   TQ_x_10},   /*CAN=800kbps*/      \
        {1,   TQ_x_8 }    /*CAN=1000kbps*/
    #elif CO_FSYS == 20000
        #define CO_CANbitRateDataInitializers  \
        {50,  TQ_x_20},   /*CAN=10kbps*/       \
        {25,  TQ_x_20},   /*CAN=20kbps*/       \
        {10,  TQ_x_20},   /*CAN=50kbps*/       \
        {5,   TQ_x_16},   /*CAN=125kbps*/      \
        {2,   TQ_x_20},   /*CAN=250kbps*/      \
        {1,   TQ_x_20},   /*CAN=500kbps*/      \
        {1,   TQ_x_20},   /*Not possible*/     \
        {1,   TQ_x_10}    /*CAN=1000kbps*/
    #elif CO_FSYS == 24000
        #define CO_CANbitRateDataInitializers  \
        {63,  TQ_x_19},   /*CAN=10kbps*/       \
        {40,  TQ_x_15},   /*CAN=20kbps*/       \
        {15,  TQ_x_16},   /*CAN=50kbps*/       \
        {6,   TQ_x_16},   /*CAN=125kbps*/      \
        {3,   TQ_x_16},   /*CAN=250kbps*/      \
        {2,   TQ_x_12},   /*CAN=500kbps*/      \
        {1,   TQ_x_15},   /*CAN=800kbps*/      \
        {1,   TQ_x_12}    /*CAN=1000kbps*/
    #elif CO_FSYS == 32000
        #define CO_CANbitRateDataInitializers  \
        {64,  TQ_x_25},   /*CAN=10kbps*/       \
        {50,  TQ_x_16},   /*CAN=20kbps*/       \
        {20,  TQ_x_16},   /*CAN=50kbps*/       \
        {8,   TQ_x_16},   /*CAN=125kbps*/      \
        {4,   TQ_x_16},   /*CAN=250kbps*/      \
        {2,   TQ_x_16},   /*CAN=500kbps*/      \
        {2,   TQ_x_10},   /*CAN=800kbps*/      \
        {1,   TQ_x_16}    /*CAN=1000kbps*/
    #elif CO_FSYS == 36000
        #define CO_CANbitRateDataInitializers  \
        {50,  TQ_x_18},   /*CAN=10kbps*/       \
        {50,  TQ_x_18},   /*CAN=20kbps*/       \
        {20,  TQ_x_18},   /*CAN=50kbps*/       \
        {8,   TQ_x_18},   /*CAN=125kbps*/      \
        {4,   TQ_x_18},   /*CAN=250kbps*/      \
        {2,   TQ_x_18},   /*CAN=500kbps*/      \
        {2,   TQ_x_18},   /*Not possible*/     \
        {1,   TQ_x_18}    /*CAN=1000kbps*/
    #elif CO_FSYS == 40000
        #define CO_CANbitRateDataInitializers  \
        {50,  TQ_x_20},   /*Not possible*/     \
        {50,  TQ_x_20},   /*CAN=20kbps*/       \
        {25,  TQ_x_16},   /*CAN=50kbps*/       \
        {10,  TQ_x_16},   /*CAN=125kbps*/      \
        {5,   TQ_x_16},   /*CAN=250kbps*/      \
        {2,   TQ_x_20},   /*CAN=500kbps*/      \
        {1,   TQ_x_25},   /*CAN=800kbps*/      \
        {1,   TQ_x_20}    /*CAN=1000kbps*/
    #elif CO_FSYS == 48000
        #define CO_CANbitRateDataInitializers  \
        {63,  TQ_x_19},   /*Not possible*/     \
        {63,  TQ_x_19},   /*CAN=20kbps*/       \
        {30,  TQ_x_16},   /*CAN=50kbps*/       \
        {12,  TQ_x_16},   /*CAN=125kbps*/      \
        {6,   TQ_x_16},   /*CAN=250kbps*/      \
        {3,   TQ_x_16},   /*CAN=500kbps*/      \
        {2,   TQ_x_15},   /*CAN=800kbps*/      \
        {2,   TQ_x_12}    /*CAN=1000kbps*/
    #elif CO_FSYS == 56000
        #define CO_CANbitRateDataInitializers  \
        {61,  TQ_x_23},   /*Not possible*/     \
        {61,  TQ_x_23},   /*CAN=20kbps*/       \
        {35,  TQ_x_16},   /*CAN=50kbps*/       \
        {14,  TQ_x_16},   /*CAN=125kbps*/      \
        {7,   TQ_x_16},   /*CAN=250kbps*/      \
        {4,   TQ_x_14},   /*CAN=500kbps*/      \
        {5,   TQ_x_7 },   /*CAN=800kbps*/      \
        {2,   TQ_x_14}    /*CAN=1000kbps*/
    #elif CO_FSYS == 64000
        #define CO_CANbitRateDataInitializers  \
        {64,  TQ_x_25},   /*Not possible*/     \
        {64,  TQ_x_25},   /*CAN=20kbps*/       \
        {40,  TQ_x_16},   /*CAN=50kbps*/       \
        {16,  TQ_x_16},   /*CAN=125kbps*/      \
        {8,   TQ_x_16},   /*CAN=250kbps*/      \
        {4,   TQ_x_16},   /*CAN=500kbps*/      \
        {2,   TQ_x_20},   /*CAN=800kbps*/      \
        {2,   TQ_x_16}    /*CAN=1000kbps*/
    #elif CO_FSYS == 72000
        #define CO_CANbitRateDataInitializers  \
        {40,  TQ_x_18},   /*Not possible*/     \
        {40,  TQ_x_18},   /*Not possible*/     \
        {40,  TQ_x_18},   /*CAN=50kbps*/       \
        {16,  TQ_x_18},   /*CAN=125kbps*/      \
        {8,   TQ_x_18},   /*CAN=250kbps*/      \
        {4,   TQ_x_18},   /*CAN=500kbps*/      \
        {3,   TQ_x_15},   /*CAN=800kbps*/      \
        {2,   TQ_x_18}    /*CAN=1000kbps*/
    #elif CO_FSYS == 80000
        #define CO_CANbitRateDataInitializers  \
        {40,  TQ_x_20},   /*Not possible*/     \
        {40,  TQ_x_20},   /*Not possible*/     \
        {40,  TQ_x_20},   /*CAN=50kbps*/       \
        {16,  TQ_x_20},   /*CAN=125kbps*/      \
        {8,   TQ_x_20},   /*CAN=250kbps*/      \
        {4,   TQ_x_20},   /*CAN=500kbps*/      \
        {2,   TQ_x_25},   /*CAN=800kbps*/      \
        {2,   TQ_x_20}    /*CAN=1000kbps*/
    #else
        #error define_CO_FSYS CO_FSYS not supported
    #endif
#endif


/* Structure contains timing coefficients for CAN module.
 *
 * CAN baud rate is calculated from following equations:
 * Fsys                                - System clock (MAX 80MHz for PIC32MX)
 * TQ = 2 * BRP / Fsys                 - Time Quanta
 * BaudRate = 1 / (TQ * K)             - Can bus Baud Rate
 * K = SJW + PROP + PhSeg1 + PhSeg2    - Number of Time Quantas
 */
typedef struct{
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
    unsigned    ident    :11;   /* Standard Identifier */
    unsigned    FILHIT   :5;    /* Filter hit, see PIC32MX documentation */
    unsigned    CMSGTS   :16;   /* CAN message timestamp, see PIC32MX documentation */
    unsigned    DLC      :4;    /* Data length code (bits 0...3) */
    unsigned             :5;
    unsigned    RTR      :1;    /* Remote Transmission Request bit */
    unsigned             :22;
    uint8_t     data[8];        /* 8 data bytes */
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
    uint32_t            CMSGSID;     /* Equal to register in transmit message buffer. Includes standard Identifier */
    uint32_t            CMSGEID;     /* Equal to register in transmit message buffer. Includes data length code and RTR */
    uint8_t             data[8];
    volatile CO_bool_t  bufferFull;
    volatile CO_bool_t  syncFlag;
}CO_CANtx_t;


/* CAN module object. */
typedef struct{
    uint16_t            CANbaseAddress;
    CO_CANrxMsg_t       CANmsgBuff[33]; /* PIC32 specific: CAN message buffer for CAN module. 32 buffers for receive, 1 buffer for transmit */
    uint8_t             CANmsgBuffSize; /* PIC32 specific: Size of the above buffer == 33. Take care initial value! */
    CO_CANrx_t         *rxArray;
    uint16_t            rxSize;
    CO_CANtx_t         *txArray;
    uint16_t            txSize;
    volatile CO_bool_t  useCANrxFilters;
    volatile CO_bool_t  bufferInhibitFlag;
    volatile CO_bool_t  firstCANtxMessage;
    volatile uint16_t   CANtxCount;
    uint32_t            errOld;
    void               *em;
}CO_CANmodule_t;


/* Endianes */
#define CO_LITTLE_ENDIAN


/* Request CAN configuration or normal mode */
void CO_CANsetConfigurationMode(uint16_t CANbaseAddress);
void CO_CANsetNormalMode(uint16_t CANbaseAddress);


/* Initialize CAN module object.
 *
 * PIC32MX CAN FIFO configuration: Two FIFOs are used. First FIFO is 32 messages
 * long and is used for reception. Second is used for transmission and is 1
 * message long. Format of message in fifo is described by CO_CANrxMsg_t for
 * both: receiving and transmitting messages. However transmitting messages does
 * not use all structure members.
 */
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
        CO_bool_t               rtr,
        void                   *object,
        void                  (*pFunct)(void *object, const CO_CANrxMsg_t *message));


/* Configure CAN message transmit buffer. */
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        CO_bool_t               rtr,
        uint8_t                 noOfBytes,
        CO_bool_t               syncFlag);


/* Send CAN message. */
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer);


/* Clear all synchronous TPDOs from CAN module transmit buffers. */
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule);


/* Verify all errors of CAN module. */
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule);


/* CAN interrupt receives and transmits CAN messages.
 *
 * Function must be called directly from _C1Interrupt or _C2Interrupt with
 * high priority.
 */
void CO_CANinterrupt(CO_CANmodule_t *CANmodule);


#endif
