/*
 * CAN module object for Linux SocketCAN.
 *
 * @file        CO_driver.h
 * @author      Janez Paternoster
 * @copyright   2015 Janez Paternoster
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
 */


#ifndef CO_DRIVER_H
#define CO_DRIVER_H


/* For documentation see file drvTemplate/CO_driver.h */


#include <stddef.h>         /* for 'NULL' */
#include <stdint.h>         /* for 'int8_t' to 'uint64_t' */
#include <stdbool.h>        /* for 'true', 'false' */
#include <unistd.h>
#include <endian.h>

#ifndef CO_SINGLE_THREAD
#include <pthread.h>
#endif

#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h>


/* general configuration */
//    #define CO_LOG_CAN_MESSAGES   /* Call external function for each received or transmitted CAN message. */
    #define CO_SDO_BUFFER_SIZE           889    /* Override default SDO buffer size. */


/* Critical sections */
#ifdef CO_SINGLE_THREAD
    #define CO_LOCK_CAN_SEND()
    #define CO_UNLOCK_CAN_SEND()

    #define CO_LOCK_EMCY()
    #define CO_UNLOCK_EMCY()

    #define CO_LOCK_OD()
    #define CO_UNLOCK_OD()
#else
    #define CO_LOCK_CAN_SEND()      /* not needed */
    #define CO_UNLOCK_CAN_SEND()

    extern pthread_mutex_t CO_EMCY_mtx;
    #define CO_LOCK_EMCY()          {if(pthread_mutex_lock(&CO_EMCY_mtx) != 0) CO_errExit("Mutex lock CO_EMCY_mtx failed");}
    #define CO_UNLOCK_EMCY()        {if(pthread_mutex_unlock(&CO_EMCY_mtx) != 0) CO_errExit("Mutex unlock CO_EMCY_mtx failed");}

    extern pthread_mutex_t CO_OD_mtx;
    #define CO_LOCK_OD()            {if(pthread_mutex_lock(&CO_OD_mtx) != 0) CO_errExit("Mutex lock CO_OD_mtx failed");}
    #define CO_UNLOCK_OD()          {if(pthread_mutex_unlock(&CO_OD_mtx) != 0) CO_errExit("Mutex unlock CO_OD_mtx failed");}
#endif


/* Data types */
    /* int8_t to uint64_t are defined in stdint.h */
    typedef _Bool                   bool_t;
    typedef float                   float32_t;
    typedef double                  float64_t;
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


/* CAN receive message structure as aligned in CAN module. */
typedef struct{
    uint32_t        ident;
    uint8_t         DLC;
    uint8_t         data[8] __attribute__((aligned(8)));
}CO_CANrxMsg_t;


/* Received message object */
typedef struct{
    uint32_t            ident;
    uint32_t            mask;
    void               *object;
    void              (*pFunct)(void *object, const CO_CANrxMsg_t *message);
}CO_CANrx_t;


/* Transmit message object as aligned in CAN module. */
typedef struct{
    uint32_t            ident;
    uint8_t             DLC;
    uint8_t             data[8] __attribute__((aligned(8)));
    volatile bool_t     bufferFull;
    volatile bool_t     syncFlag;
}CO_CANtx_t;


/* CAN module object. */
typedef struct{
    int32_t             CANbaseAddress;
#ifdef CO_LOG_CAN_MESSAGES
    CO_CANtx_t          txRecord;
#endif
    CO_CANrx_t         *rxArray;
    uint16_t            rxSize;
    CO_CANtx_t         *txArray;
    uint16_t            txSize;
    uint16_t            wasConfigured;/* Zero only on first run of CO_CANmodule_init */
    int                 fd;         /* CAN_RAW socket file descriptor */
    struct can_filter  *filter;     /* array of CAN filters of size rxSize */
    volatile bool_t     CANnormal;
    volatile bool_t     useCANrxFilters;
    volatile bool_t     bufferInhibitFlag;
    volatile bool_t     firstCANtxMessage;
    volatile uint8_t    error;
    volatile uint16_t   CANtxCount;
    uint32_t            errOld;
    void               *em;
}CO_CANmodule_t;


/* Endianes */
#ifdef BYTE_ORDER
#if BYTE_ORDER == LITTLE_ENDIAN
    #define CO_LITTLE_ENDIAN
#else
    #define CO_BIG_ENDIAN
#endif
#endif


/* Helper function, must be defined externally. */
void CO_errExit(char* msg);


/* Request CAN configuration or normal mode */
void CO_CANsetConfigurationMode(int32_t fdSocket);
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule);


/* Initialize CAN module object. */
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        int32_t                 CANbaseAddress,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate); /* not used */


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


/* Functions receives CAN messages. It is blocking.
 *
 * @param CANmodule This object.
 */
void CO_CANrxWait(CO_CANmodule_t *CANmodule);


#endif
