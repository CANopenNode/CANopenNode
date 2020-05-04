/*
 * Device and application specific definitions for CANopenNode.
 *
 * @file        CO_driver_target.h
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


#ifndef CO_DRIVER_TARGET
#define CO_DRIVER_TARGET

/* This file contains device and application specific definitions.
 * It is included from CO_driver.h, which contains documentation
 * for definitions below. */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef CO_DRIVER_CUSTOM
#include "CO_driver_custom.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Stack configuration override from CO_driver.h. Compile full stack.
 * For more information see file CO_config.h. */
#ifndef CO_CONFIG_NMT
#define CO_CONFIG_NMT (CO_CONFIG_FLAG_CALLBACK_PRE | \
                       CO_CONFIG_FLAG_TIMERNEXT | \
                       CO_CONFIG_NMT_CALLBACK_CHANGE | \
                       CO_CONFIG_NMT_MASTER | \
                       CO_CONFIG_NMT_LEDS)
#endif

#ifndef CO_CONFIG_SDO
#define CO_CONFIG_SDO (CO_CONFIG_FLAG_CALLBACK_PRE | \
                       CO_CONFIG_FLAG_TIMERNEXT | \
                       CO_CONFIG_SDO_SEGMENTED | \
                       CO_CONFIG_SDO_BLOCK)
#endif

#ifndef CO_CONFIG_SDO_BUFFER_SIZE
#define CO_CONFIG_SDO_BUFFER_SIZE 1800
#endif

#ifndef CO_CONFIG_EM
#define CO_CONFIG_EM (CO_CONFIG_FLAG_CALLBACK_PRE | \
                      CO_CONFIG_FLAG_TIMERNEXT | \
                      CO_CONFIG_EM_CONSUMER)
#endif

#ifndef CO_CONFIG_HB_CONS
#define CO_CONFIG_HB_CONS (CO_CONFIG_FLAG_CALLBACK_PRE | \
                           CO_CONFIG_FLAG_TIMERNEXT | \
                           CO_CONFIG_HB_CONS_CALLBACK_CHANGE | \
                           CO_CONFIG_HB_CONS_CALLBACK_MULTI | \
                           CO_CONFIG_HB_CONS_QUERY_FUNCT)
#endif

#ifndef CO_CONFIG_PDO
#define CO_CONFIG_PDO (CO_CONFIG_FLAG_CALLBACK_PRE | \
                       CO_CONFIG_FLAG_TIMERNEXT | \
                       CO_CONFIG_PDO_SYNC_ENABLE)
#endif

#ifndef CO_CONFIG_SYNC
#define CO_CONFIG_SYNC (CO_CONFIG_FLAG_CALLBACK_PRE | \
                        CO_CONFIG_FLAG_TIMERNEXT)
#endif

#ifndef CO_CONFIG_SDO_CLI
#define CO_CONFIG_SDO_CLI (CO_CONFIG_FLAG_CALLBACK_PRE | \
                           CO_CONFIG_FLAG_TIMERNEXT | \
                           CO_CONFIG_SDO_CLI_SEGMENTED | \
                           CO_CONFIG_SDO_CLI_BLOCK | \
                           CO_CONFIG_SDO_CLI_LOCAL)
#endif

#ifndef CO_CONFIG_SDO_CLI_BUFFER_SIZE
#define CO_CONFIG_SDO_CLI_BUFFER_SIZE 1000
#endif

#ifndef CO_CONFIG_LSS_MST
#define CO_CONFIG_LSS_MST (CO_CONFIG_FLAG_CALLBACK_PRE)
#endif

#ifndef CO_CONFIG_GTW
#define CO_CONFIG_GTW (CO_CONFIG_GTW_MULTI_NET | \
                       CO_CONFIG_GTW_ASCII)
#define CO_CONFIG_GTWA_COMM_BUF_SIZE 2000
#endif


/* Basic definitions */
#define CO_LITTLE_ENDIAN
/* NULL is defined in stddef.h */
/* true and false are defined in stdbool.h */
/* int8_t to uint64_t are defined in stdint.h */
typedef unsigned char           bool_t;
typedef float                   float32_t;
typedef long double             float64_t;
typedef char                    char_t;
typedef unsigned char           oChar_t;
typedef unsigned char           domain_t;


/* Access to received CAN message */
#define CO_CANrxMsg_readIdent(msg) ((uint16_t)0)
#define CO_CANrxMsg_readDLC(msg)   ((uint8_t)0)
#define CO_CANrxMsg_readData(msg)  ((uint8_t *)NULL)

/* Received message object */
typedef struct {
    uint16_t ident;
    uint16_t mask;
    void *object;
    void (*CANrx_callback)(void *object, void *message);
} CO_CANrx_t;

/* Transmit message object */
typedef struct {
    uint32_t ident;
    uint8_t DLC;
    uint8_t data[8];
    volatile bool_t bufferFull;
    volatile bool_t syncFlag;
} CO_CANtx_t;

/* CAN module object */
typedef struct {
    void *CANptr;
    CO_CANrx_t *rxArray;
    uint16_t rxSize;
    CO_CANtx_t *txArray;
    uint16_t txSize;
    volatile bool_t CANnormal;
    volatile bool_t useCANrxFilters;
    volatile bool_t bufferInhibitFlag;
    volatile bool_t firstCANtxMessage;
    volatile uint16_t CANtxCount;
    uint32_t errOld;
    void *em;
} CO_CANmodule_t;


/* (un)lock critical section in CO_CANsend() */
#define CO_LOCK_CAN_SEND()
#define CO_UNLOCK_CAN_SEND()

/* (un)lock critical section in CO_errorReport() or CO_errorReset() */
#define CO_LOCK_EMCY()
#define CO_UNLOCK_EMCY()

/* (un)lock critical section when accessing Object Dictionary */
#define CO_LOCK_OD()
#define CO_UNLOCK_OD()

/* Synchronization between CAN receive and message processing threads. */
#define CO_MemoryBarrier()
#define CO_FLAG_READ(rxNew) ((rxNew) != NULL)
#define CO_FLAG_SET(rxNew) {CO_MemoryBarrier(); rxNew = (void*)1L;}
#define CO_FLAG_CLEAR(rxNew) {CO_MemoryBarrier(); rxNew = NULL;}


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DRIVER_TARGET */
