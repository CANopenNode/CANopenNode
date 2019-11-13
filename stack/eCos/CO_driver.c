/*
 * CAN module object for eCos CAN framework
 *
 * @file        CO_driver_eCos.c
 * @ingroup     CO_driver
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


//===========================================================================
//                                INCLUDES
//===========================================================================
#include "CO_driver.h"
#include "CO_Emergency.h"
#include "ecos_helper.h"

#include <string.h>
#include <cyg/io/canio.h>
#include <cyg/io/io.h>
#include <cyg/infra/diag.h>
#include <cyg/infra/cyg_ass.h>
#include <cyg/infra/cyg_trac.h>

// Package option requirements
#if defined(CYGFUN_KERNEL_API_C)
#if defined(CYGPKG_IO_CAN_DEVICES)
#if defined(CYGOPT_IO_CAN_RUNTIME_MBOX_CFG)
#if defined(CYGOPT_IO_CAN_STD_CAN_ID)
#if defined(CYGOPT_IO_CAN_SUPPORT_NONBLOCKING)
#if defined(CYGOPT_IO_CAN_SUPPORT_TIMEOUTS)


//===========================================================================
//                                DEFINES
//===========================================================================
#define UNUSED_ENTRY 0xFFFF // indicates unused entry in rx buffer array


//===========================================================================
//                               DATA TYPES
//===========================================================================
/**
 * Stores thread data of a single thread object
 */
typedef struct st_thread_data
{
    cyg_thread   obj;
    long         stack[CYGNUM_HAL_STACK_SIZE_TYPICAL];
    cyg_handle_t hdl;
} thread_data_t;


//===========================================================================
//                             LOCAL DATA
//===========================================================================
static cyg_thread_entry_t can_rx_thread;///< thread entry for RX thread
static thread_data_t can_rx_thread_data = {
    hdl : 0
};
CO_CANmodule_t* can_module = 0;///< local CAN module object - if it is 0 then the module is not initialized


//===========================================================================
/**
 * This is a thin wrapper around CO_errorReport to enable some diagnostic output
 */
void CO_eCos_errorReport(CO_EM_t *em, const uint8_t errorBit,
	const uint16_t errorCode, const uint32_t infoCode)
{
	CO_DBG_PRINT("CO_eCos_errorReport: errorBit %x errorCode %x infoCode %x\n",
		errorBit, errorCode, infoCode);
	CO_errorReport(em, errorBit, errorCode, infoCode);
}



//===========================================================================
/**
 * This is a thin wrapper around CO_errorReset to enable some diagnostic output
 */
void CO_eCos_errorReset(CO_EM_t *em, const uint8_t errorBit, const uint32_t infoCode)
{
	CO_DBG_PRINT("CO_errorReset: errorBit %x infoCode %x\n", errorBit, infoCode);
	CO_errorReset(em, errorBit, infoCode);
}


//=============================================================================
/**
 * Call this function if you have a error return code != ENOERR. This function
 * reports the error as an emergency message and prints an optional debug
 * message
 * \param[in] ErrCode Error code returned by eCos CAN function
 * \param[in] CANmodule The CAN module object that caused the error
 * \param[in] DebugMessage Optional debug message. If this is not 0, then a
 *                         diagnostic message is printed with CO_DBG_PRINT
 */
static void reportErrorReturnCode(Cyg_ErrNo ErrCode, CO_CANmodule_t *CANmodule,
	const char* DebugMessage)
{
	CO_EM_t* EM = (CO_EM_t*)CANmodule->em;
	if (ENOERR == ErrCode)
	{
		return;
	}

	CO_errorReport(EM, CO_EM_GENERIC_SOFTWARE_ERROR,
		CO_EMC_SOFTWARE_DEVICE, ErrCode);

	if (DebugMessage)
	{
		CO_DBG_PRINT(DebugMessage, ErrCode);
	}
}


//=============================================================================
/**
 * Set mode of CAN controller (configuration, active...)
 * This function properly handles errors when setting mode
 */
static void setCAN_Mode(cyg_can_mode mode, void *CANdriverState)
{
	if (!can_module)
	{
		return;
	}

	cyg_uint32 len = sizeof(mode);
	Cyg_ErrNo Result = cyg_io_set_config(can_module->ioHandle,
		CYG_IO_SET_CONFIG_CAN_MODE ,&mode, &len);
	if (ENOERR != Result)
	{
        reportErrorReturnCode(Result, can_module, 0);
        CO_DBG_PRINT("Set CAN mode %d returned error %x\n",
        	mode, Result);
	}
}


//=============================================================================
void CO_CANsetConfigurationMode(void *CANdriverState)
{
	setCAN_Mode(CYGNUM_CAN_MODE_CONFIG, CANdriverState);
}


//=============================================================================
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule)
{
	setCAN_Mode(CYGNUM_CAN_MODE_START, CANmodule->CANdriverState);

    CANmodule->CANnormal = true;
}


//=============================================================================
/**
 * Translates CANopen node bitrate into eCos bitrate identifier
 */
cyg_can_baud_rate_t translateBaudRate(uint16_t CANbitRate)
{
	switch (CANbitRate)
	{
	case 10:  return CYGNUM_CAN_KBAUD_10;
	case 20:  return CYGNUM_CAN_KBAUD_20;
	case 50:  return CYGNUM_CAN_KBAUD_50;
	case 100: return CYGNUM_CAN_KBAUD_100;
	case 125: return CYGNUM_CAN_KBAUD_125;
	case 250: return CYGNUM_CAN_KBAUD_250;
	case 500: return CYGNUM_CAN_KBAUD_500;
	case 800: return CYGNUM_CAN_KBAUD_800;
	case 1000: return CYGNUM_CAN_KBAUD_1000;
	}

	return CYGNUM_CAN_KBAUD_1000;
}



//===========================================================================
/**
 * Prints CAN event via diagnostic output channel
 */
static void print_can_msg(cyg_can_message *pmsg, char *pMsg)
{
    char *pmsg_str;
    static char* msg_tbl[] =
    {
        "%s [ID:%03X] [RTR:%d] [EXT:%d] [DATA:]\n",
        "%s [ID:%03X] [RTR:%d] [EXT:%d] [DATA:%02X]\n",
        "%s [ID:%03X] [RTR:%d] [EXT:%d] [DATA:%02X %02X]\n",
        "%s [ID:%03X] [RTR:%d] [EXT:%d] [DATA:%02X %02X %02X]\n",
        "%s [ID:%03X] [RTR:%d] [EXT:%d] [DATA:%02X %02X %02X %02X]\n",
        "%s [ID:%03X] [RTR:%d] [EXT:%d] [DATA:%02X %02X %02X %02X %02X]\n",
        "%s [ID:%03X] [RTR:%d] [EXT:%d] [DATA:%02X %02X %02X %02X %02X %02X]\n",
        "%s [ID:%03X] [RTR:%d] [EXT:%d] [DATA:%02X %02X %02X %02X %02X %02X %02X]\n",
        "%s [ID:%03X] [RTR:%d] [EXT:%d] [DATA:%02X %02X %02X %02X %02X %02X %02X %02X]\n"
    };

    if (pmsg->rtr)
    {
        CO_DBG_PRINT("%s [ID:%03X] [RTR:%d] [EXT:%d] [DLC:%d]\n",
                    pMsg,
                    pmsg->id,
                    pmsg->rtr,
                    pmsg->ext,
                    pmsg->dlc);

        return;
    }

    if (pmsg->dlc > 8)
    {
        pmsg_str = msg_tbl[8];
    }
    else
    {
        pmsg_str = msg_tbl[pmsg->dlc];
    }

    CO_DBG_PRINT(pmsg_str,
                pMsg,
                pmsg->id,
                pmsg->rtr,
                pmsg->ext,
                pmsg->data.bytes[0],
                pmsg->data.bytes[1],
                pmsg->data.bytes[2],
                pmsg->data.bytes[3],
                pmsg->data.bytes[4],
                pmsg->data.bytes[5],
                pmsg->data.bytes[6],
                pmsg->data.bytes[7]);
}


//===========================================================================
/**
 * Prints CAN event flags via diagnostic output channel
 */
static void print_can_flags(cyg_uint16 flags, char *pMsg)
{
    char      *pmsg_str;
    cyg_uint8  i ;
    static char* msg_tbl[] =
    {
        "RX  ",
        "TX  ",
        "WRX  ",
        "WTX  ",
        "ERRP  ",
        "BOFF  ",
        "OVRX  ",
        "OVTX  ",
        "CERR  ",
        "LSTY  ",
        "ESTY  ",
        "ALOS  ",
        "DEVC  ",
        "PHYF  ",
        "PHYH  ",
        "PHYL  ",
        "ERRA  ",
        "OVRXHW  "
    };

    i = 0;
    while (flags && (i < 16))
    {
        if (flags & 0x0001)
        {
            pmsg_str = msg_tbl[i];
            CO_DBG_PRINT(pmsg_str);
        }
        flags >>=1;
        i++;
    }

    CO_DBG_PRINT("\n");
}


//===========================================================================
/**
 * The receive thread the reads the messages from the eCos CAN driver and
 * calls the receive buffer callback functions to process the received messages
 */
void can_rx_thread(cyg_addrword_t data)
{
    cyg_uint32    len;
    cyg_can_event rx_event;
    CO_CANmodule_t* CANmodule = (CO_CANmodule_t*)data;
    CO_EM_t* EM = (CO_EM_t*)CANmodule->em;
    Cyg_ErrNo Result;
    CO_DBG_PRINT("can_rx_thread started\n");

    while (1)
    {
        //
        // First receive CAN event from real CAN hardware and report an error
    	// if something goes wrong
        //
        len = sizeof(rx_event);
        Result = cyg_io_read(CANmodule->ioHandle, &rx_event, &len);
        if (ENOERR != Result)
        {
        	reportErrorReturnCode(Result, CANmodule,
        		"cyg_io_read() returned error %x\n");
        	continue;
        }

        print_can_flags(rx_event.flags, "Received event");
        //
        // Check if we received an RX event - if we have an RX event then we
        // copy the received message data and call the receive buffer callback
        // function
        //
        if (rx_event.flags & CYGNUM_CAN_EVENT_RX)
        {
			uint16_t BufferIndex = CANmodule->rxBufferIndexArray[rx_event.msg.id];
			if (UNUSED_ENTRY != BufferIndex)
			{
				CO_CANrx_t *msgBuff = &CANmodule->rxArray[BufferIndex];
				CO_CANrxMsg_t rcvMsg;
				rcvMsg.ID = rx_event.msg.id;
				rcvMsg.DLC = rx_event.msg.dlc;
				rcvMsg.RTR = rx_event.msg.rtr;
				memcpy(rcvMsg.data, rx_event.msg.data.bytes, rx_event.msg.dlc);
				print_can_msg(&rx_event.msg, "Rx: ");
				msgBuff->pFunct(msgBuff->object, &rcvMsg);
			}
        } // if (rx_event.flags & CYGNUM_CAN_EVENT_RX)

        if (rx_event.flags & CYGNUM_CAN_EVENT_OVERRUN_RX)
        {
        	CO_eCos_errorReport(EM, CO_EM_RXMSG_OVERFLOW, CO_EMC_CAN_OVERRUN, 0);
        }

        if (rx_event.flags & CYGNUM_CAN_EVENT_OVERRUN_RX_HW)
        {
        	CO_eCos_errorReport(EM, CO_EM_RXMSG_OVERFLOW, CO_EMC_CAN_OVERRUN, 0);
        }
    	CO_DBG_PRINT("processing can_rx_thread\n");
    } // while (1)
}


//============================================================================
Cyg_ErrNo canInit(CO_CANmodule_t* CANmodule, uint16_t CANbitRate)
{
	cyg_uint32 len;
	cyg_can_info_t can_info;
	cyg_can_msgbuf_cfg msgbox_cfg;

	//
	// Get a valid device handle for CAN device 0
	//
	Cyg_ErrNo Result = cyg_io_lookup("/dev/can0", &CANmodule->ioHandle);
	if (ENOERR != Result)
	{
		reportErrorReturnCode(Result, CANmodule,
			"cyg_io_lookup(/dev/can0) returned error: %x\n");
		return Result;
	}

	//
	// Set sending CAN messages to non blocking mode because it happens in
	// main thread and main thread should never be blocked
	//
	cyg_uint32 blocking = 0;
	len = sizeof(blocking);
	Result = cyg_io_set_config(CANmodule->ioHandle,
		CYG_IO_SET_CONFIG_WRITE_BLOCKING, &blocking, &len);
	if (ENOERR != Result)
	{
		reportErrorReturnCode(Result, CANmodule,
			"CYG_IO_SET_CONFIG_WRITE_BLOCKING %x\n");
		return Result;
	}

	//
	// Set receiving CAN events to blocking mode because it happens in
	// dedicated receive thread
	//
	blocking = 1;
	len = sizeof(blocking);
	Result = cyg_io_set_config(CANmodule->ioHandle,
		CYG_IO_SET_CONFIG_READ_BLOCKING, &blocking, &len);
	if (ENOERR != Result)
	{
		reportErrorReturnCode(Result, CANmodule,
			"CYG_IO_SET_CONFIG_READ_BLOCKING %x\n");
		return Result;
	}

	//
	// Set TX timeout to 0 because function shall return immediately if
	// send queue is full
	//
	cyg_can_timeout_info_t timeouts;
	timeouts.rx_timeout = convertMsToTicks(1000);// 1000 ms converted to clock ticks
	timeouts.tx_timeout = 0;
	len = sizeof(timeouts);
	Result = cyg_io_set_config(CANmodule->ioHandle,
		CYG_IO_SET_CONFIG_CAN_TIMEOUT, &timeouts, &len);
	if (ENOERR != Result)
	{
		reportErrorReturnCode(Result, CANmodule,
			"CYG_IO_SET_CONFIG_CAN_TIMEOUT %x\n");
		return Result;
	}

	//
	// Flush output - this is required in case of reset
	//
	Result = cyg_io_set_config(CANmodule->ioHandle,
		CYG_IO_SET_CONFIG_CAN_OUTPUT_FLUSH, 0, 0);
	if (ENOERR != Result)
	{
		reportErrorReturnCode(Result, CANmodule,
			"CYG_IO_SET_CONFIG_CAN_OUTPUT_FLUSH returned error: %x\n");
		return Result;
	}

	//
	// Set baudrate
	//
	can_info.baud = translateBaudRate(CANbitRate);
	len = sizeof(can_info);
    Result = cyg_io_set_config(CANmodule->ioHandle, CYG_IO_SET_CONFIG_CAN_INFO, &can_info, &len);
    if (ENOERR != Result)
    {
		reportErrorReturnCode(Result, CANmodule,
			"Setting baudrate returned error: %x\n");
    	return Result;
    }

    //
    // Now reset message buffer configuration - this is mandatory before starting
    // message buffer runtime configuration
    //
    msgbox_cfg.cfg_id = CYGNUM_CAN_MSGBUF_RESET_ALL;
    len = sizeof(msgbox_cfg);
    Result = cyg_io_set_config(CANmodule->ioHandle, CYG_IO_SET_CONFIG_CAN_MSGBUF ,&msgbox_cfg, &len);
    if (ENOERR != Result)
    {
		reportErrorReturnCode(Result, CANmodule,
			"CYGNUM_CAN_MSGBUF_RESET_ALL returned error: %x\n");
    	return Result;
    }

    if (!can_rx_thread_data.hdl)
    {
		//
		// create receive thread if it has not been created yet
		//
		cyg_thread_create(4, can_rx_thread,
							 (cyg_addrword_t)CANmodule,
							 "can_rx_thread",
							 (void *) can_rx_thread_data.stack,
							 CYGNUM_HAL_STACK_SIZE_TYPICAL * sizeof(long),
							 &can_rx_thread_data.hdl,
							 &can_rx_thread_data.obj);

		cyg_thread_resume(can_rx_thread_data.hdl);
    }
    CO_DBG_PRINT("CAN driver initialised\n");
    return ENOERR;
}


/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        void                   *CANdriverState,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate)
{
    uint16_t i;

    /* verify arguments */
    if(CANmodule==NULL || rxArray==NULL || txArray==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->curentSyncTimeIsInsideWindow = 0;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = 1;// we can make this configurable
    CANmodule->bufferInhibitFlag = 0;
    CANmodule->firstCANtxMessage = 1;
    CANmodule->CANtxCount = 0;
    CANmodule->errOld = 0;
    CANmodule->em = 0;
    memset(CANmodule->rxBufferIndexArray, UNUSED_ENTRY, sizeof(CANmodule->rxBufferIndexArray));

    for(i=0; i<rxSize; i++){
        CANmodule->rxArray[i].ident = 0;
        CANmodule->rxArray[i].pFunct = 0;
    }
    for(i=0; i<txSize; i++){
        CANmodule->txArray[i].bufferFull = 0;
    }

    Cyg_ErrNo Result = canInit(CANmodule, CANbitRate);
    if (ENOERR != Result)
    {
    	return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return CO_ERROR_NO;
}


//=============================================================================
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
	setCAN_Mode(CYGNUM_CAN_MODE_STOP, ADDR_CAN1);
}


//=============================================================================
uint16_t CO_CANrxMsg_readIdent(CO_CANrxMsg_t *rxMsg)
{
    return rxMsg->ID;
}


//=============================================================================
Cyg_ErrNo hwCANrxBufferInit(CO_CANmodule_t *CANmodule, uint16_t ident)
{
	CO_DBG_PRINT("hwCANrxBufferInit %x\n", ident);
    cyg_can_filter rx_filter;
    cyg_uint32 len;
    rx_filter.cfg_id  = CYGNUM_CAN_MSGBUF_RX_FILTER_ADD;
    rx_filter.msg.id  = ident & 0x07FF;
    rx_filter.msg.ext = CYGNUM_CAN_ID_STD;
    len = sizeof(rx_filter);
    return cyg_io_set_config(CANmodule->ioHandle,
    	CYG_IO_SET_CONFIG_CAN_MSGBUF ,&rx_filter, &len);
}


//=============================================================================
void updateHardwareFilters(CO_CANmodule_t *CANmodule)
{
    cyg_uint32             len;
    cyg_can_msgbuf_cfg     msgbox_cfg;
    uint16_t i = 0;

	CO_DBG_PRINT("updateHardwareFilters()\n");
    //
    // Now reset message buffer configuration - this is mandatory before starting
    // message buffer runtime configuration
    //
    msgbox_cfg.cfg_id = CYGNUM_CAN_MSGBUF_RESET_ALL;
    len = sizeof(msgbox_cfg);
    Cyg_ErrNo Result = cyg_io_set_config(CANmodule->ioHandle,
    	CYG_IO_SET_CONFIG_CAN_MSGBUF ,&msgbox_cfg, &len);
    if (ENOERR != Result)
    {
		reportErrorReturnCode(Result, CANmodule,
			"CYGNUM_CAN_MSGBUF_RESET_ALL returned error: %x\n");
		return;
    }

    memset(CANmodule->rxBufferIndexArray, UNUSED_ENTRY, sizeof(CANmodule->rxBufferIndexArray));
    // new iterate through all buffers and set all filters
    for (i = 0; i < CANmodule->rxSize; ++i)
    {
    	CO_CANrx_t* rxBuffer = &CANmodule->rxArray[i];
    	if (!rxBuffer->pFunct)
    	{
    		continue;
    	}

    	uint16_t CAN_ID = rxBuffer->ident &= ~0x800;
    	Result = hwCANrxBufferInit(CANmodule, CAN_ID);
    	if (ENOERR != Result)
    	{
    		reportErrorReturnCode(Result, CANmodule,
    			"CYGNUM_CAN_MSGBUF_RX_FILTER_ADD returned error: %x\n");
    		return;
    	}
    	CANmodule->rxBufferIndexArray[CAN_ID] = i;
    }
}


//=============================================================================
int16_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint16_t                mask,
        uint8_t                 rtr,
        void                   *object,
        void                  (*pFunct)(void *object, const CO_CANrxMsg_t *message))
{
    CO_CANrx_t *rxBuffer;
    int16_t ret = CO_ERROR_NO;
    bool enable_buffer = true;

    /* safety */
    if(!CANmodule || !object || !pFunct || index >= CANmodule->rxSize){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* buffer, which will be configured */
    rxBuffer = &CANmodule->rxArray[index];

    /* Configure object variables */
    rxBuffer->object = object;

    /* CAN identifier and CAN mask, bit aligned with CAN module. Different on different microcontrollers. */
    rxBuffer->ident = ident & 0x07FF;
    if(rtr) rxBuffer->ident |= 0x0800;

    if(!CANmodule->useCANrxFilters)
    {
    	return ret;
    }

    enable_buffer = (0 != ident) || (index == 0);

    if (!enable_buffer)
    {
    	if (!rxBuffer->pFunct)
    	{
    		return ret;
    	}

    	rxBuffer->pFunct = 0;
    	updateHardwareFilters(CANmodule);
    	return ret;
    }

    rxBuffer->pFunct = pFunct;
    CO_DBG_PRINT("Setting hardware filter ID: %x Mask: %x  Buffer: %d\n", ident, mask,index);
    CYG_ASSERT(ident < 0x800, "Illegal CAN identifier > 0x800");
    if (UNUSED_ENTRY != CANmodule->rxBufferIndexArray[ident])
    {
    	return CO_ERROR_NO;
    }

    CANmodule->rxBufferIndexArray[ident] = index;
    Cyg_ErrNo Result = hwCANrxBufferInit(CANmodule, ident);
    if (ENOERR != Result)
    {
    	ret = CO_ERROR_OUT_OF_MEMORY;
    }
    can_module = CANmodule;

    return ret;
}


//=============================================================================
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint8_t                 rtr,
        uint8_t                 noOfBytes,
        uint8_t                 syncFlag)
{
    /* safety */
    if(!CANmodule || CANmodule->txSize <= index) return 0;

    /* get specific buffer */
    CO_CANtx_t *buffer = &CANmodule->txArray[index];

    buffer->ID = ident;
    buffer->DLC = noOfBytes;
    buffer->RTR = rtr;
    buffer->bufferFull = 0;
    buffer->syncFlag = syncFlag ? 1 : 0;

    return buffer;
}


//=============================================================================
int16_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
	int16_t Ret = CO_ERROR_NO;

    // messages with syncFlag set (synchronous PDOs) must be transmited inside preset time window
    if(CANmodule->curentSyncTimeIsInsideWindow && buffer->syncFlag && !(*CANmodule->curentSyncTimeIsInsideWindow))
    {
        CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_TPDO_OUTSIDE_WINDOW, CO_EMC_COMMUNICATION, 0);
        return CO_ERROR_TX_PDO_WINDOW;
    }

	cyg_uint32 len;
	cyg_can_message tx_msg;

	len = sizeof(tx_msg);
	tx_msg.id = buffer->ID;
	tx_msg.dlc = buffer->DLC;
	tx_msg.rtr = buffer->RTR ? CYGNUM_CAN_FRAME_RTR : CYGNUM_CAN_FRAME_DATA;
	tx_msg.ext = CYGNUM_CAN_ID_STD;
	memcpy(tx_msg.data.bytes, buffer->data, buffer->DLC);
	Cyg_ErrNo Result = cyg_io_write(CANmodule->ioHandle, &tx_msg, &len);
	if (ENOERR == Result)
	{
		CANmodule->firstCANtxMessage = 0;
		buffer->bufferFull = 0;
	}
	else
	{
		CO_eCos_errorReport((CO_EM_t*)CANmodule->em,
			CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, 0);
		CO_DBG_PRINT("cyg_io_write() returned error %x\n", Result);
		Ret = CO_ERROR_TIMEOUT;
	}

    return Ret;
}


//=============================================================================
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
	// we do nothing here. eCos has a transmit queue and if the messages
	// are in the queue then we can't do anything - the message will get
	// transmitted
}



//=============================================================================
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule){
    uint16_t rxErrors = 0;
    uint16_t txErrors = 0;
    CO_EM_t* EM = (CO_EM_t*)CANmodule->em;
    Cyg_ErrNo Result;
    cyg_uint32 len;
    cyg_can_err_count_info err_info;

    len = sizeof(err_info);
    Result = cyg_io_get_config(CANmodule->ioHandle, CYG_IO_GET_CONFIG_CAN_ERR_COUNTERS,
    	&err_info, &len);
    if (ENOERR != Result)
    {
        reportErrorReturnCode(Result, CANmodule,
        	"CYG_IO_GET_CONFIG_CAN_ERR_COUNTERS returned error: %x\n");
        return;
    }

    txErrors = err_info.tx_err_count;
    rxErrors = err_info.rx_err_count;

    cyg_can_state can_state;
    len = sizeof(cyg_can_state);
    Result = cyg_io_get_config(CANmodule->ioHandle, CYG_IO_GET_CONFIG_CAN_STATE,
    	&can_state, &len);
    if (ENOERR != Result)
    {
        reportErrorReturnCode(Result, CANmodule,
        	"CYG_IO_GET_CONFIG_CAN_STATE returned error: %x\n");
        return;
    }

    if (CYGNUM_CAN_STATE_BUS_OFF == can_state)
    {
    	txErrors = 256;
    }

    uint32_t err = txErrors << 8 | rxErrors;
    if (CANmodule->errOld == err)
    {
    	return;
    }

    CANmodule->errOld = err;

    // Bus off
	if(txErrors >= 256)
	{
		CO_eCos_errorReport(EM, CO_EM_CAN_TX_BUS_OFF, CO_EMC_BUS_OFF_RECOVERED, err);
		return;
	}
		CO_eCos_errorReset(EM, CO_EM_CAN_TX_BUS_OFF, err);

	// bus warning
	if ((rxErrors >= 96) || (txErrors >= 96))
	{
		CO_eCos_errorReport(EM, CO_EM_CAN_BUS_WARNING, CO_EMC_NO_ERROR, err);
	}

	// rx bus passive
	if (rxErrors >= 128)
	{
		CO_eCos_errorReport(EM, CO_EM_CAN_RX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
	}
	else
	{
		CO_eCos_errorReset(EM, CO_EM_CAN_RX_BUS_PASSIVE, err);
	}

	// tx bus passive
	if (txErrors >= 128)
	{
		CO_eCos_errorReport(EM, CO_EM_CAN_TX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
	}
	else
	{
		CO_eCos_errorReset(EM, CO_EM_CAN_TX_BUS_PASSIVE, err);
	}

	// no error
	if ((rxErrors < 96) && (txErrors < 96))
	{
		CO_eCos_errorReset(EM, CO_EM_CAN_BUS_WARNING, err);
	}
}

#else // CYGOPT_IO_CAN_SUPPORT_TIMEOUTS
#error "Needs support for CAN timeouts - CYGOPT_IO_CAN_SUPPORT_TIMEOUTS"
#endif

#else // CYGOPT_IO_CAN_SUPPORT_NONBLOCKING
#error "Needs support for non blocking read / write calls - CYGOPT_IO_CAN_SUPPORT_NONBLOCKING"
#endif

#else // CYGOPT_IO_CAN_STD_CAN_ID
#error "Needs support for CAN standard 11 Bit identifiers - CYGOPT_IO_CAN_STD_CAN_ID"
#endif

#else // CYGOPT_IO_CAN_RUNTIME_MBOX_CFG
#error "Needs support for CAN message buffer runtime configuration - CYGOPT_IO_CAN_RUNTIME_MBOX_CFG"
#endif

#else // CYGPKG_IO_CAN_DEVICES
#error "Needs CAN hardware device drivers - CYGPKG_IO_CAN_DEVICES"
#endif

#else // CYGFUN_KERNEL_API_C)
#error "Needs kernel C API"
#endif
