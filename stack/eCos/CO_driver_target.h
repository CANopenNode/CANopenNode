/*
 * CAN module object for eCos RTOS CAN layer
 *
 * @file        CO_driver_target.h
 * @author      Uwe Kindler
 * @copyright   2013 Uwe Kindler
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


#ifndef CO_DRIVER_TARGET_H
#define CO_DRIVER_TARGET_H


/* For documentation see file drvTemplate/CO_driver.h */


//===========================================================================
//                                INCLUDES
//===========================================================================
#include <cyg/infra/cyg_type.h>
#include <cyg/io/canio.h>
#include <cyg/io/io.h>
#include <cyg/kernel/kapi.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

//===========================================================================
//                              CONFIGURATION
//===========================================================================
#define CYGDBG_IO_CANOPEN_DEBUG 1 // define this if you want debug output via diagnostic channel


//===========================================================================
//                                DEFINES
//===========================================================================
#if (CYG_BYTEORDER == CYG_MSBFIRST)
#define CO_BIG_ENDIAN 1
#else
#define CO_LITTLE_ENDIAN 1
#endif

//
// Support debug output if this option is enabled in CDL file
//
#ifdef CYGDBG_IO_CANOPEN_DEBUG
#define CO_DBG_PRINT diag_printf
#else
#define CO_DBG_PRINT( fmt, ... )
#endif

/* CAN module base address */
// we don't really care about the addresses here because the eCos port
// uses I/O handles for accessing its CAN devices
#define ADDR_CAN1               0
#define ADDR_CAN2               1


/* Critical sections */
// shared data is accessed only from thread level code (not from ISR or DSR)
// so we simply do a scheduler lock here to prevent access from different
// threads
#define CO_LOCK_CAN_SEND()      cyg_scheduler_lock()
#define CO_UNLOCK_CAN_SEND()    cyg_scheduler_unlock()

#define CO_LOCK_EMCY()          cyg_scheduler_lock()
#define CO_UNLOCK_EMCY()        cyg_scheduler_unlock()

#define CO_LOCK_OD()            cyg_scheduler_lock()
#define CO_UNLOCK_OD()          cyg_scheduler_unlock()



/* Data types */
typedef unsigned char           bool_t;
typedef cyg_uint8               uint8_t;
typedef cyg_uint16              uint16_t;
typedef cyg_uint32              uint32_t;
typedef cyg_uint64              uint64_t;
typedef cyg_int8                int8_t;
typedef cyg_int16               int16_t;
typedef cyg_int32               int32_t;
typedef cyg_int64               int64_t;
typedef float                   float32_t;
typedef long double             float64_t;
typedef char                    char_t;
typedef unsigned char           oChar_t;
typedef unsigned char           domain_t;


/* CAN receive message structure as aligned in CAN module. */
typedef struct{
    uint32_t            ID;
    uint8_t             DLC; /* Length of CAN message */
    uint8_t             RTR;
    uint8_t             data[8]; /* 8 data bytes */
}CO_CANrxMsg_t;


/* Received message object */
typedef struct{
    uint16_t            ident;
    void               *object;
    void              (*pFunct)(void *object, const CO_CANrxMsg_t *message);
}CO_CANrx_t;


/* Transmit message object. */
typedef struct{
	uint16_t            ID;
	uint8_t             DLC;
	uint8_t             RTR;
    uint8_t             data[8];
    volatile uint8_t    bufferFull;
    volatile uint8_t    syncFlag;
}CO_CANtx_t;


/* CAN module object. */
typedef struct{
    CO_CANrx_t         *rxArray;
    uint16_t            rxSize;
    CO_CANtx_t         *txArray;
    uint16_t            txSize;
    volatile bool_t     CANnormal;
    volatile uint8_t   *curentSyncTimeIsInsideWindow;
    volatile uint8_t    useCANrxFilters;
    volatile uint8_t    bufferInhibitFlag;
    volatile uint8_t    firstCANtxMessage;
    volatile uint16_t   CANtxCount;
    uint32_t            errOld;
    void               *em;
    void               *driverPrivate;
    uint16_t            rxBufferIndexArray[0x800]; ///< Array of pointers to rx buffers
    cyg_io_handle_t     ioHandle;
}CO_CANmodule_t;

#ifdef __cplusplus
} //extern "C"
#endif /* __cplusplus */
#endif /* CO_DRIVER_TARGET_H */
