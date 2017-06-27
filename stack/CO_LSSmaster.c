/*
 * CANopen LSS Master protocol.
 *
 * @file        CO_LSSmaster.c
 * @ingroup     CO_LSS
 * @author      Martin Wagner
 * @copyright   2017 Neuberger Gebäudeautomation GmbH
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

#include "CANopen.h"
#include "CO_LSSmaster.h"

#if CO_NO_LSS_CLIENT == 1

/*
 * LSS master slave select state machine. Compared to #CO_LSS_state_t this
 * has information if we currently have selected one or all slaves. This
 * allows for some basic error checking.
 */
typedef enum {
  CO_LSSmaster_STATE_WAITING = 0,
  CO_LSSmaster_STATE_CFG_SLECTIVE,
  CO_LSSmaster_STATE_CFG_GLOBAL,
} CO_LSSmaster_state_t;

/*
 * LSS master slave command state machine
 */
typedef enum {
  CO_LSSmaster_COMMAND_WAITING = 0,
  CO_LSSmaster_COMMAND_SWITCH_STATE,
  CO_LSSmaster_COMMAND_CFG_BIT_TIMING,
  CO_LSSmaster_COMMAND_CFG_NODE_ID,
  CO_LSSmaster_COMMAND_CFG_STORE,
  CO_LSSmaster_COMMAND_INQUIRE_VENDOR,
  CO_LSSmaster_COMMAND_INQUIRE_PRODUCT,
  CO_LSSmaster_COMMAND_INQUIRE_REV,
  CO_LSSmaster_COMMAND_INQUIRE_SERIAL,
  CO_LSSmaster_COMMAND_INQUIRE_NODE_ID,
  CO_LSSmaster_COMMAND_IDENTIFY_FASTSCAN
} CO_LSSmaster_command_t;

/*
 * macros for receive -> processing function message transfer
 */
#ifndef __GNUC__
/* this GCC builtin function ensures memory barriers. It might be needed when
 * the compiler or cpu re-orders memory access */
#define __sync_synchronize
#endif
#define IS_CANrxNew() (LSSmaster->CANrxNew)
#define SET_CANrxNew() __sync_synchronize(); LSSmaster->CANrxNew = true;
#define CLEAR_CANrxNew() __sync_synchronize(); LSSmaster->CANrxNew = false;

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_LSSmaster_receive(void *object, const CO_CANrxMsg_t *msg)
{
    CO_LSSmaster_t *LSSmaster;

    LSSmaster = (CO_LSSmaster_t*)object;   /* this is the correct pointer type of the first argument */

    /* verify message length and message overflow (previous message was not processed yet) */
    if(msg->DLC==8 && !IS_CANrxNew() &&
       LSSmaster->command!=CO_LSSmaster_COMMAND_WAITING){

        /* copy data and set 'new message' flag */
        LSSmaster->CANrxData[0] = msg->data[0];
        LSSmaster->CANrxData[1] = msg->data[1];
        LSSmaster->CANrxData[2] = msg->data[2];
        LSSmaster->CANrxData[3] = msg->data[3];
        LSSmaster->CANrxData[4] = msg->data[4];
        LSSmaster->CANrxData[5] = msg->data[5];
        LSSmaster->CANrxData[6] = msg->data[6];
        LSSmaster->CANrxData[7] = msg->data[7];

        SET_CANrxNew();

        /* Optional signal to RTOS, which can resume task, which handles SDO client. */
        if(LSSmaster->pFunctSignal != NULL) {
            LSSmaster->pFunctSignal(LSSmaster->functSignalObject);
        }
    }
}

/*
 * Check LSS timeout.
 *
 * Generally, we do not really care if the message has been received before
 * or after the timeout expired. Only if no message has been received we have
 * to check for timeouts
 */
static CO_LSSmaster_return_t CO_LSSmaster_check_timeout(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms)
{
    CO_LSSmaster_return_t ret = CO_LSSmaster_WAIT_SLAVE;

    LSSmaster->timeoutTimer += timeDifference_ms;
    if (LSSmaster->timeoutTimer >= LSSmaster->timeout) {
        LSSmaster->timeoutTimer = 0;
        ret = CO_LSSmaster_TIMEOUT;
    }

    return ret;
}


/******************************************************************************/
CO_ReturnError_t CO_LSSmaster_init(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeout_ms,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        uint32_t                CANidLssSlave,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx,
        uint32_t                CANidLssMaster)
{
    /* verify arguments */
    if (LSSmaster==NULL || CANdevRx==NULL || CANdevTx==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    LSSmaster->timeout = timeout_ms;
    LSSmaster->state = CO_LSSmaster_STATE_WAITING;
    LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    LSSmaster->timeoutTimer = 0;
    CLEAR_CANrxNew();
    CO_memset(LSSmaster->CANrxData, 0, sizeof(LSSmaster->CANrxData));
    LSSmaster->pFunctSignal = NULL;
    LSSmaster->functSignalObject = NULL;

    /* configure LSS CAN Slave response message reception */
    CO_CANrxBufferInit(
            CANdevRx,             /* CAN device */
            CANdevRxIdx,          /* rx buffer index */
            CANidLssSlave,        /* CAN identifier */
            0x7FF,                /* mask */
            0,                    /* rtr */
            (void*)LSSmaster,     /* object passed to receive function */
            CO_LSSmaster_receive);/* this function will process received message */

    /* configure LSS CAN Master message transmission */
    LSSmaster->CANdevTx = CANdevTx;
    LSSmaster->TXbuff = CO_CANtxBufferInit(
            CANdevTx,             /* CAN device */
            CANdevTxIdx,          /* index of specific buffer inside CAN module */
            CANidLssMaster,       /* CAN identifier */
            0,                    /* rtr */
            8,                    /* number of data bytes */
            0);                   /* synchronous message flag bit */

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_LSSmaster_changeTimeout(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeout_ms)
{
    if (LSSmaster != NULL) {
        LSSmaster->timeout = timeout_ms;
    }
}


/******************************************************************************/
void CO_LSSmaster_initCallback(
        CO_LSSmaster_t         *LSSmaster,
        void                   *object,
        void                  (*pFunctSignal)(void *object))
{
    if(LSSmaster != NULL){
        LSSmaster->functSignalObject = object;
        LSSmaster->pFunctSignal = pFunctSignal;
    }
}

/*
 * Helper function - initiate switch state
 */
static CO_LSSmaster_return_t CO_LSSmaster_switchStateSelectInitiate(
        CO_LSSmaster_t         *LSSmaster,
        CO_LSS_address_t       *lssAddress)
{
  CO_LSSmaster_return_t ret;

  if (LSSmaster == NULL){
      return CO_LSSmaster_ILLEGAL_ARGUMENT;
  }

  if (lssAddress != NULL) {
      /* switch state select specific using LSS address */
      LSSmaster->state = CO_LSSmaster_STATE_CFG_SLECTIVE;
      LSSmaster->command = CO_LSSmaster_COMMAND_SWITCH_STATE;
      LSSmaster->timeoutTimer = 0;

      CLEAR_CANrxNew();
      CO_memset(&LSSmaster->TXbuff->data[6], 0, 3);
      LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_SEL_VENDOR;
      CO_setUint32(&LSSmaster->TXbuff->data[1], lssAddress->vendorID);
      CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);
      LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_SEL_PRODUCT;
      CO_setUint32(&LSSmaster->TXbuff->data[1], lssAddress->productCode);
      CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);
      LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_SEL_REV;
      CO_setUint32(&LSSmaster->TXbuff->data[1], lssAddress->revisionNumber);
      CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);
      LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_SEL_SERIAL;
      CO_setUint32(&LSSmaster->TXbuff->data[1], lssAddress->serialNumber);
      CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

      ret = CO_LSSmaster_WAIT_SLAVE;
  }
  else {
      /* switch state global */
      LSSmaster->state = CO_LSSmaster_STATE_CFG_GLOBAL;

      CLEAR_CANrxNew();
      LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_GLOBAL;
      LSSmaster->TXbuff->data[1] = CO_LSS_STATE_CONFIGURATION;
      CO_memset(&LSSmaster->TXbuff->data[2], 0, 6);
      CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

      /* This is non-confirmed service! */
      ret = CO_LSSmaster_OK;
  }
  return ret;
}

/*
 * Helper function - wait for confirmation
 */
static CO_LSSmaster_return_t CO_LSSmaster_switchStateSelectWait(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms)
{
    CO_LSSmaster_return_t ret;

    if (LSSmaster == NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    if (IS_CANrxNew()) {
        uint8_t cs = LSSmaster->CANrxData[0];
        CLEAR_CANrxNew();

        if (cs == CO_LSS_SWITCH_STATE_SEL) {
            /* confirmation received */
            ret = CO_LSSmaster_OK;
        }
        else {
            ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_ms);
        }
    }
    else {
        ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_ms);
    }

    return ret;
}

/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_switchStateSelect(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        CO_LSS_address_t       *lssAddress)
{
    CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;

    if (LSSmaster == NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    /* Initiate select */
    if (LSSmaster->state==CO_LSSmaster_STATE_WAITING &&
        LSSmaster->command==CO_LSSmaster_COMMAND_WAITING){

        ret = CO_LSSmaster_switchStateSelectInitiate(LSSmaster, lssAddress);
    }
    /* Wait for confirmation */
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_SWITCH_STATE) {
        ret = CO_LSSmaster_switchStateSelectWait(LSSmaster, timeDifference_ms);
    }

    if (ret != CO_LSSmaster_WAIT_SLAVE) {
        /* finished */
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    if (ret < CO_LSSmaster_OK) {
        /* switching failed, go back to waiting */
        LSSmaster->state=CO_LSSmaster_STATE_WAITING;
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    return ret;
}


/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_switchStateDeselect(
        CO_LSSmaster_t         *LSSmaster)
{
    CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;

    if (LSSmaster == NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    /* We can always send this command to get into a clean state on the network.
     * If no slave is selected, this command is ignored. */
    if (LSSmaster->command == CO_LSSmaster_COMMAND_WAITING){

        /* switch state global */
        LSSmaster->state = CO_LSSmaster_STATE_WAITING;

        CLEAR_CANrxNew();
        LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_GLOBAL;
        LSSmaster->TXbuff->data[1] = CO_LSS_STATE_WAITING;
        CO_memset(&LSSmaster->TXbuff->data[2], 0, 6);
        CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

        /* This is non-confirmed service! */
        ret = CO_LSSmaster_OK;
    }

    return ret;
}


/*
 * Helper function - wait for confirmation, check for returned error code
 *
 * This uses the nature of the configure confirmation message design:
 * - byte 0 -> cs
 * - byte 1 -> Error Code, where
 *    - 0  = OK
 *    - 1 .. FE = Values defined by CiA. All currently defined values
 *                are slave rejects. No further distinction on why the
 *                slave did reject the request.
 *    - FF = Manufacturer Error Code in byte 2
 * - byte 2 -> Manufacturer Error, currently not used
 *
 * enums for the errorCode are
 * - CO_LSS_cfgNodeId_t
 * - CO_LSS_cfgBitTiming_t
 * - CO_LSS_cfgStore_t
 */
static CO_LSSmaster_return_t CO_LSSmaster_configureCheckWait(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        uint8_t                 csWait)
{
    CO_LSSmaster_return_t ret;

    if (LSSmaster == NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    if (IS_CANrxNew()) {
        uint8_t cs = LSSmaster->CANrxData[0];
        uint8_t errorCode = LSSmaster->CANrxData[1];
        CLEAR_CANrxNew();

        if (cs == csWait) {
            if (errorCode == 0) {
                ret = CO_LSSmaster_OK;
            }
            else if (errorCode == 0xff) {
                ret = CO_LSSmaster_OK_MANUFACTURER;
            }
            else {
                ret = CO_LSSmaster_OK_ILLEGAL_ARGUMENT;
            }
        }
        else {
            ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_ms);
        }
    }
    else {
        ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_ms);
    }

    if (ret != CO_LSSmaster_WAIT_SLAVE) {
        /* finished */
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    return ret;
}


/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_configureBitTiming(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        uint16_t                bit)
{
    CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;
    uint8_t bitTiming;

    if (LSSmaster == NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    switch (bit) {
        case 1000:  bitTiming = CO_LSS_BIT_TIMING_1000; break;
        case 800:   bitTiming = CO_LSS_BIT_TIMING_800;  break;
        case 500:   bitTiming = CO_LSS_BIT_TIMING_500;  break;
        case 250:   bitTiming = CO_LSS_BIT_TIMING_250;  break;
        case 125:   bitTiming = CO_LSS_BIT_TIMING_125;  break;
        case 50:    bitTiming = CO_LSS_BIT_TIMING_50;   break;
        case 20:    bitTiming = CO_LSS_BIT_TIMING_20;   break;
        case 10:    bitTiming = CO_LSS_BIT_TIMING_10;   break;
        case 0:     bitTiming = CO_LSS_BIT_TIMING_AUTO; break;
        default:    return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    /* Initiate config bit */
    if (LSSmaster->state==CO_LSSmaster_STATE_CFG_SLECTIVE &&
        LSSmaster->command==CO_LSSmaster_COMMAND_WAITING){

        LSSmaster->command = CO_LSSmaster_COMMAND_CFG_BIT_TIMING;
        LSSmaster->timeoutTimer = 0;

        CLEAR_CANrxNew();
        LSSmaster->TXbuff->data[0] = CO_LSS_CFG_BIT_TIMING;
        LSSmaster->TXbuff->data[1] = 0;
        LSSmaster->TXbuff->data[2] = bitTiming;
        CO_memset(&LSSmaster->TXbuff->data[3], 0, 5);
        CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

        ret = CO_LSSmaster_WAIT_SLAVE;
    }
    /* Wait for confirmation */
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_CFG_BIT_TIMING) {

        ret = CO_LSSmaster_configureCheckWait(LSSmaster, timeDifference_ms,
                CO_LSS_CFG_BIT_TIMING);
    }

    if (ret != CO_LSSmaster_WAIT_SLAVE) {
        /* finished */
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    return ret;
}


/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_configureNodeId(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        uint8_t                 nodeId)
{
    CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;

    if (LSSmaster==NULL || !CO_LSS_NODE_ID_VALID(nodeId)){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    /* Initiate config node ID */
    if (LSSmaster->state==CO_LSSmaster_STATE_CFG_SLECTIVE &&
        LSSmaster->command==CO_LSSmaster_COMMAND_WAITING){

        LSSmaster->command = CO_LSSmaster_COMMAND_CFG_NODE_ID;
        LSSmaster->timeoutTimer = 0;

        CLEAR_CANrxNew();
        LSSmaster->TXbuff->data[0] = CO_LSS_CFG_NODE_ID;
        LSSmaster->TXbuff->data[1] = nodeId;
        CO_memset(&LSSmaster->TXbuff->data[2], 0, 6);
        CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

        ret = CO_LSSmaster_WAIT_SLAVE;
    }
    /* Wait for confirmation */
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_CFG_NODE_ID) {

      ret = CO_LSSmaster_configureCheckWait(LSSmaster, timeDifference_ms,
              CO_LSS_CFG_NODE_ID);
    }

    if (ret != CO_LSSmaster_WAIT_SLAVE) {
        /* finished */
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    return ret;
}


/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_configureStore(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms)
{
    CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;

    if (LSSmaster == NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    /* Initiate config store */
    if (LSSmaster->state==CO_LSSmaster_STATE_CFG_SLECTIVE &&
        LSSmaster->command==CO_LSSmaster_COMMAND_WAITING){

        LSSmaster->command = CO_LSSmaster_COMMAND_CFG_STORE;
        LSSmaster->timeoutTimer = 0;

        CLEAR_CANrxNew();
        LSSmaster->TXbuff->data[0] = CO_LSS_CFG_STORE;
        CO_memset(&LSSmaster->TXbuff->data[1], 0, 7);
        CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

        ret = CO_LSSmaster_WAIT_SLAVE;
    }
    /* Wait for confirmation */
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_CFG_STORE) {

      ret = CO_LSSmaster_configureCheckWait(LSSmaster, timeDifference_ms,
              CO_LSS_CFG_STORE);
    }

    if (ret != CO_LSSmaster_WAIT_SLAVE) {
        /* finished */
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    return ret;
}


/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_ActivateBit(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                switchDelay_ms)
{
    CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;

    if (LSSmaster == NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    /* for activating bit timing, we need to have all slaves set to config
     * state. This check makes it a bit harder to shoot ourselves in the foot */
    if (LSSmaster->state==CO_LSSmaster_STATE_CFG_GLOBAL &&
        LSSmaster->command==CO_LSSmaster_COMMAND_WAITING){

        CLEAR_CANrxNew();
        LSSmaster->TXbuff->data[0] = CO_LSS_CFG_ACTIVATE_BIT_TIMING;
        CO_setUint16(&LSSmaster->TXbuff->data[1], switchDelay_ms);
        CO_memset(&LSSmaster->TXbuff->data[3], 0, 5);
        CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

        /* This is non-confirmed service! */
        ret = CO_LSSmaster_OK;
    }

    return ret;
}

/*
 * Helper function - send request
 */
static CO_LSSmaster_return_t CO_LSSmaster_inquireInitiate(
        CO_LSSmaster_t         *LSSmaster,
        uint8_t                 cs)
{
    if (LSSmaster == NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    CLEAR_CANrxNew();
    LSSmaster->TXbuff->data[0] = cs;
    CO_memset(&LSSmaster->TXbuff->data[1], 0, 7);
    CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

    return CO_LSSmaster_WAIT_SLAVE;
}

/*
 * Helper function - wait for confirmation
 */
static CO_LSSmaster_return_t CO_LSSmaster_inquireCheckWait(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        uint8_t                 csWait,
        uint32_t               *value)
{
    CO_LSSmaster_return_t ret;

    if (LSSmaster == NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    if (IS_CANrxNew()) {
        uint8_t cs = LSSmaster->CANrxData[0];
        *value = CO_getUint32(&LSSmaster->CANrxData[1]);
        CLEAR_CANrxNew();

        if (cs == csWait) {
            ret = CO_LSSmaster_OK;
        }
        else {
            ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_ms);
        }
    }
    else {
        ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_ms);
    }

    return ret;
}

/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_InquireLssAddress(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        CO_LSS_address_t       *lssAddress)
{
    CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;
    CO_LSSmaster_command_t next = CO_LSSmaster_COMMAND_WAITING;

    if (LSSmaster==NULL || lssAddress==NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    /* Check for reply */
    if (LSSmaster->command == CO_LSSmaster_COMMAND_INQUIRE_VENDOR) {

        ret = CO_LSSmaster_inquireCheckWait(LSSmaster, timeDifference_ms,
                CO_LSS_INQUIRE_VENDOR, &lssAddress->vendorID);
        if (ret == CO_LSSmaster_OK) {
            /* Start next request */
            next = CO_LSSmaster_COMMAND_INQUIRE_PRODUCT;
            ret = CO_LSSmaster_WAIT_SLAVE;
        }
    }
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_INQUIRE_PRODUCT) {

        ret = CO_LSSmaster_inquireCheckWait(LSSmaster, timeDifference_ms,
                CO_LSS_INQUIRE_PRODUCT, &lssAddress->productCode);
        if (ret == CO_LSSmaster_OK) {
            /* Start next request */
            next = CO_LSSmaster_COMMAND_INQUIRE_REV;
            ret = CO_LSSmaster_WAIT_SLAVE;
        }
    }
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_INQUIRE_REV) {

        ret = CO_LSSmaster_inquireCheckWait(LSSmaster, timeDifference_ms,
                CO_LSS_INQUIRE_REV, &lssAddress->revisionNumber);
        if (ret == CO_LSSmaster_OK) {
            /* Start next request */
            next = CO_LSSmaster_COMMAND_INQUIRE_SERIAL;
            ret = CO_LSSmaster_WAIT_SLAVE;
        }
    }
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_INQUIRE_SERIAL) {

        ret = CO_LSSmaster_inquireCheckWait(LSSmaster, timeDifference_ms,
                CO_LSS_INQUIRE_SERIAL, &lssAddress->serialNumber);
    }
    /* Check for next request */
    if (LSSmaster->state == CO_LSSmaster_STATE_CFG_SLECTIVE) {
        if (LSSmaster->command == CO_LSSmaster_COMMAND_WAITING) {

            LSSmaster->command = CO_LSSmaster_COMMAND_INQUIRE_VENDOR;
            LSSmaster->timeoutTimer = 0;

            ret = CO_LSSmaster_inquireInitiate(LSSmaster, CO_LSS_INQUIRE_VENDOR);
        }
        else if (next == CO_LSSmaster_COMMAND_INQUIRE_PRODUCT) {
            LSSmaster->command = CO_LSSmaster_COMMAND_INQUIRE_PRODUCT;
            LSSmaster->timeoutTimer = 0;

            ret = CO_LSSmaster_inquireInitiate(LSSmaster, CO_LSS_INQUIRE_PRODUCT);
        }
        else if (next == CO_LSSmaster_COMMAND_INQUIRE_REV) {
            LSSmaster->command = CO_LSSmaster_COMMAND_INQUIRE_REV;
            LSSmaster->timeoutTimer = 0;

            ret = CO_LSSmaster_inquireInitiate(LSSmaster, CO_LSS_INQUIRE_REV);
        }
        else if (next == CO_LSSmaster_COMMAND_INQUIRE_SERIAL) {
            LSSmaster->command = CO_LSSmaster_COMMAND_INQUIRE_SERIAL;
            LSSmaster->timeoutTimer = 0;

            ret = CO_LSSmaster_inquireInitiate(LSSmaster, CO_LSS_INQUIRE_SERIAL);
        }
    }

    if (ret != CO_LSSmaster_WAIT_SLAVE) {
        /* finished */
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    return ret;
}


/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_InquireNodeId(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        uint8_t                *nodeId)
{
  CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;

  if (LSSmaster==NULL || nodeId==NULL){
      return CO_LSSmaster_ILLEGAL_ARGUMENT;
  }

  /* send request */
  if (LSSmaster->state==CO_LSSmaster_STATE_CFG_SLECTIVE &&
      LSSmaster->command == CO_LSSmaster_COMMAND_WAITING) {

      LSSmaster->command = CO_LSSmaster_COMMAND_INQUIRE_NODE_ID;
      LSSmaster->timeoutTimer = 0;

      ret = CO_LSSmaster_inquireInitiate(LSSmaster, CO_LSS_INQUIRE_NODE_ID);
  }
  /* Check for reply */
  else if (LSSmaster->command == CO_LSSmaster_COMMAND_INQUIRE_NODE_ID) {
      uint32_t tmp;

      ret = CO_LSSmaster_inquireCheckWait(LSSmaster, timeDifference_ms,
              CO_LSS_INQUIRE_NODE_ID, &tmp);

      *nodeId = tmp & 0xff;
  }

  if (ret != CO_LSSmaster_WAIT_SLAVE) {
      LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
  }
  return ret;
}


/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_IdentifyFastscan(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        CO_LSS_address_t       *lssAddressScanStart,
        CO_LSS_address_t       *lssAddressScanMatch,
        CO_LSS_address_t       *lssAddressFound)
{
    if (LSSmaster == NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

}


#endif
