/*
 * CAN module object for Linux SocketCAN.
 *
 * @file        CO_driver.h
 * @author      Janez Paternoster
 * @copyright   2015 - 2020 Janez Paternoster
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


/* Endianness */
#ifdef BYTE_ORDER
#if BYTE_ORDER == LITTLE_ENDIAN
    #define CO_LITTLE_ENDIAN
#else
    #define CO_BIG_ENDIAN
#endif /* BYTE_ORDER == LITTLE_ENDIAN */
#endif /* BYTE_ORDER */


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

    #define CANrxMemoryBarrier()
#else
    #define CO_LOCK_CAN_SEND()      /* not needed */
    #define CO_UNLOCK_CAN_SEND()

    extern pthread_mutex_t CO_EMCY_mtx;
    #define CO_LOCK_EMCY()          {if(pthread_mutex_lock(&CO_EMCY_mtx) != 0) CO_errExit("Mutex lock CO_EMCY_mtx failed");}
    #define CO_UNLOCK_EMCY()        {if(pthread_mutex_unlock(&CO_EMCY_mtx) != 0) CO_errExit("Mutex unlock CO_EMCY_mtx failed");}

    extern pthread_mutex_t CO_OD_mtx;
    #define CO_LOCK_OD()            {if(pthread_mutex_lock(&CO_OD_mtx) != 0) CO_errExit("Mutex lock CO_OD_mtx failed");}
    #define CO_UNLOCK_OD()          {if(pthread_mutex_unlock(&CO_OD_mtx) != 0) CO_errExit("Mutex unlock CO_OD_mtx failed");}

    #define CANrxMemoryBarrier()    {__sync_synchronize();}
#endif /* CO_SINGLE_THREAD */

/* Syncronisation functions */
#define IS_CANrxNew(rxNew) ((uintptr_t)rxNew)
#define SET_CANrxNew(rxNew) {CANrxMemoryBarrier(); rxNew = (void*)1L;}
#define CLEAR_CANrxNew(rxNew) {CANrxMemoryBarrier(); rxNew = (void*)0L;}


/* Data types */
/* int8_t to uint64_t are defined in stdint.h */
typedef _Bool                   bool_t;
typedef float                   float32_t;
typedef double                  float64_t;
typedef char                    char_t;
typedef unsigned char           oChar_t;
typedef unsigned char           domain_t;


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
    void               *CANdriverState;
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

/* Helper function, must be defined externally. */
void CO_errExit(char* msg);


/* Functions receives CAN messages. It is blocking.
 *
 * @param CANmodule This object.
 */
void CO_CANrxWait(CO_CANmodule_t *CANmodule);


#endif /* CO_DRIVER_TARGET_H */
