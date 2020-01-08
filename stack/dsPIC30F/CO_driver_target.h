/*
 * CAN module object for Microchip dsPIC30F microcontroller.
 *
 * @file        CO_driver_target.h
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
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


#ifndef CO_DRIVER_TARGET_H
#define CO_DRIVER_TARGET_H


#include <p30Fxxxx.h>       /* processor header file */
#include <stddef.h>         /* for 'NULL' */
#include <stdint.h>         /* for 'int8_t' to 'uint64_t' */
#include <stdbool.h>        /* for true and false */

/* Endianness */
#define CO_LITTLE_ENDIAN


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
    void               *CANdriverState;
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


/* CAN interrupt receives and transmits CAN messages.
 *
 * Function must be called directly from _C1Interrupt or _C2Interrupt with
 * high priority. dsPIC30F uses two receive buffers and one transmit buffer.
 */
void CO_CANinterrupt(CO_CANmodule_t *CANmodule);


#endif /* CO_DRIVER_TARGET_H */
