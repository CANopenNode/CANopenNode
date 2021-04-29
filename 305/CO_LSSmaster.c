/*
 * CANopen LSS Master protocol.
 *
 * @file        CO_LSSmaster.c
 * @ingroup     CO_LSS
 * @author      Martin Wagner
 * @copyright   2017 - 2020 Neuberger Gebaeudeautomation GmbH
 *
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

#include "305/CO_LSSmaster.h"

#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER

#include <string.h>

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
  CO_LSSmaster_COMMAND_INQUIRE,
  CO_LSSmaster_COMMAND_IDENTIFY_FASTSCAN,
} CO_LSSmaster_command_t;

/*
 * LSS master fastscan state machine
 */
typedef enum {
  CO_LSSmaster_FS_STATE_CHECK,
  CO_LSSmaster_FS_STATE_SCAN,
  CO_LSSmaster_FS_STATE_VERIFY
} CO_LSSmaster_fs_t;

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_LSSmaster_receive(void *object, void *msg)
{
    CO_LSSmaster_t *LSSmaster;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);

    LSSmaster = (CO_LSSmaster_t*)object;   /* this is the correct pointer type of the first argument */

    /* verify message length and message overflow (previous message was not processed yet) */
    if(DLC==8 && !CO_FLAG_READ(LSSmaster->CANrxNew) &&
       LSSmaster->command!=CO_LSSmaster_COMMAND_WAITING){

        /* copy data and set 'new message' flag */
        memcpy(LSSmaster->CANrxData, data, sizeof(LSSmaster->CANrxData));

        CO_FLAG_SET(LSSmaster->CANrxNew);

#if (CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE
        /* Optional signal to RTOS, which can resume task, which handles further processing. */
        if(LSSmaster->pFunctSignal != NULL) {
            LSSmaster->pFunctSignal(LSSmaster->functSignalObject);
        }
#endif
    }
}

/*
 * Check LSS timeout.
 *
 * Generally, we do not really care if the message has been received before
 * or after the timeout expired. Only if no message has been received we have
 * to check for timeouts
 */
static inline CO_LSSmaster_return_t CO_LSSmaster_check_timeout(
        CO_LSSmaster_t         *LSSmaster,
        uint32_t                timeDifference_us)
{
    CO_LSSmaster_return_t ret = CO_LSSmaster_WAIT_SLAVE;

    LSSmaster->timeoutTimer += timeDifference_us;
    if (LSSmaster->timeoutTimer >= LSSmaster->timeout_us) {
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
        uint16_t                CANidLssSlave,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx,
        uint16_t                CANidLssMaster)
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if (LSSmaster==NULL || CANdevRx==NULL || CANdevTx==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    LSSmaster->timeout_us = (uint32_t)timeout_ms * 1000;
    LSSmaster->state = CO_LSSmaster_STATE_WAITING;
    LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    LSSmaster->timeoutTimer = 0;
    CO_FLAG_CLEAR(LSSmaster->CANrxNew);
    memset(LSSmaster->CANrxData, 0, sizeof(LSSmaster->CANrxData));
#if (CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE
    LSSmaster->pFunctSignal = NULL;
    LSSmaster->functSignalObject = NULL;
#endif

    /* configure LSS CAN Slave response message reception */
    ret = CO_CANrxBufferInit(
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

    if (LSSmaster->TXbuff == NULL) {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}


/******************************************************************************/
void CO_LSSmaster_changeTimeout(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeout_ms)
{
    if (LSSmaster != NULL) {
        LSSmaster->timeout_us = (uint32_t)timeout_ms * 1000;
    }
}


#if (CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE
/******************************************************************************/
void CO_LSSmaster_initCallbackPre(
        CO_LSSmaster_t         *LSSmaster,
        void                   *object,
        void                  (*pFunctSignal)(void *object))
{
    if(LSSmaster != NULL){
        LSSmaster->functSignalObject = object;
        LSSmaster->pFunctSignal = pFunctSignal;
    }
}
#endif


/*
 * Helper function - initiate switch state
 */
static CO_LSSmaster_return_t CO_LSSmaster_switchStateSelectInitiate(
        CO_LSSmaster_t         *LSSmaster,
        CO_LSS_address_t       *lssAddress)
{
  CO_LSSmaster_return_t ret;

  if (lssAddress != NULL) {
      /* switch state select specific using LSS address */
      LSSmaster->state = CO_LSSmaster_STATE_CFG_SLECTIVE;
      LSSmaster->command = CO_LSSmaster_COMMAND_SWITCH_STATE;
      LSSmaster->timeoutTimer = 0;

      CO_FLAG_CLEAR(LSSmaster->CANrxNew);
      memset(&LSSmaster->TXbuff->data[6], 0, sizeof(LSSmaster->TXbuff->data) - 6);
      LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_SEL_VENDOR;
      CO_setUint32(&LSSmaster->TXbuff->data[1], lssAddress->identity.vendorID);
      CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);
      LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_SEL_PRODUCT;
      CO_setUint32(&LSSmaster->TXbuff->data[1], lssAddress->identity.productCode);
      CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);
      LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_SEL_REV;
      CO_setUint32(&LSSmaster->TXbuff->data[1], lssAddress->identity.revisionNumber);
      CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);
      LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_SEL_SERIAL;
      CO_setUint32(&LSSmaster->TXbuff->data[1], lssAddress->identity.serialNumber);
      CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

      ret = CO_LSSmaster_WAIT_SLAVE;
  }
  else {
      /* switch state global */
      LSSmaster->state = CO_LSSmaster_STATE_CFG_GLOBAL;

      CO_FLAG_CLEAR(LSSmaster->CANrxNew);
      LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_GLOBAL;
      LSSmaster->TXbuff->data[1] = CO_LSS_STATE_CONFIGURATION;
      memset(&LSSmaster->TXbuff->data[2], 0, sizeof(LSSmaster->TXbuff->data) - 2);
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
        uint32_t                timeDifference_us)
{
    CO_LSSmaster_return_t ret;

    if (CO_FLAG_READ(LSSmaster->CANrxNew)) {
        uint8_t cs = LSSmaster->CANrxData[0];
        CO_FLAG_CLEAR(LSSmaster->CANrxNew);

        if (cs == CO_LSS_SWITCH_STATE_SEL) {
            /* confirmation received */
            ret = CO_LSSmaster_OK;
        }
        else {
            ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_us);
        }
    }
    else {
        ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_us);
    }

    return ret;
}

/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_switchStateSelect(
        CO_LSSmaster_t         *LSSmaster,
        uint32_t                timeDifference_us,
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
        ret = CO_LSSmaster_switchStateSelectWait(LSSmaster, timeDifference_us);
    }

    if (ret!=CO_LSSmaster_INVALID_STATE && ret!=CO_LSSmaster_WAIT_SLAVE) {
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
    LSSmaster->state = CO_LSSmaster_STATE_WAITING;
    LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    LSSmaster->timeoutTimer = 0;

    /* switch state global */
    CO_FLAG_CLEAR(LSSmaster->CANrxNew);
    LSSmaster->TXbuff->data[0] = CO_LSS_SWITCH_STATE_GLOBAL;
    LSSmaster->TXbuff->data[1] = CO_LSS_STATE_WAITING;
    memset(&LSSmaster->TXbuff->data[2], 0, sizeof(LSSmaster->TXbuff->data) - 2);
    CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

    /* This is non-confirmed service! */
    ret = CO_LSSmaster_OK;

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
        uint32_t                timeDifference_us,
        uint8_t                 csWait)
{
    CO_LSSmaster_return_t ret;

    if (CO_FLAG_READ(LSSmaster->CANrxNew)) {
        uint8_t cs = LSSmaster->CANrxData[0];
        uint8_t errorCode = LSSmaster->CANrxData[1];
        CO_FLAG_CLEAR(LSSmaster->CANrxNew);

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
            ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_us);
        }
    }
    else {
        ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_us);
    }

    if (ret!=CO_LSSmaster_INVALID_STATE && ret!=CO_LSSmaster_WAIT_SLAVE) {
        /* finished */
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    return ret;
}


/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_configureBitTiming(
        CO_LSSmaster_t         *LSSmaster,
        uint32_t                timeDifference_us,
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

        CO_FLAG_CLEAR(LSSmaster->CANrxNew);
        LSSmaster->TXbuff->data[0] = CO_LSS_CFG_BIT_TIMING;
        LSSmaster->TXbuff->data[1] = 0;
        LSSmaster->TXbuff->data[2] = bitTiming;
        memset(&LSSmaster->TXbuff->data[3], 0, sizeof(LSSmaster->TXbuff->data) - 3);
        CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

        ret = CO_LSSmaster_WAIT_SLAVE;
    }
    /* Wait for confirmation */
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_CFG_BIT_TIMING) {

        ret = CO_LSSmaster_configureCheckWait(LSSmaster, timeDifference_us,
                CO_LSS_CFG_BIT_TIMING);
    }

    if (ret!=CO_LSSmaster_INVALID_STATE && ret!=CO_LSSmaster_WAIT_SLAVE) {
        /* finished */
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    return ret;
}


/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_configureNodeId(
        CO_LSSmaster_t         *LSSmaster,
        uint32_t                timeDifference_us,
        uint8_t                 nodeId)
{
    CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;

    if (LSSmaster==NULL || !CO_LSS_NODE_ID_VALID(nodeId)){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    /* Initiate config node ID */
    if ((LSSmaster->state==CO_LSSmaster_STATE_CFG_SLECTIVE ||
        /* Let un-config node ID also be run in global mode for unconfiguring all nodes */
        (LSSmaster->state==CO_LSSmaster_STATE_CFG_GLOBAL &&
         nodeId == CO_LSS_NODE_ID_ASSIGNMENT)) &&
         LSSmaster->command==CO_LSSmaster_COMMAND_WAITING) {

        LSSmaster->command = CO_LSSmaster_COMMAND_CFG_NODE_ID;
        LSSmaster->timeoutTimer = 0;

        CO_FLAG_CLEAR(LSSmaster->CANrxNew);
        LSSmaster->TXbuff->data[0] = CO_LSS_CFG_NODE_ID;
        LSSmaster->TXbuff->data[1] = nodeId;
        memset(&LSSmaster->TXbuff->data[2], 0, sizeof(LSSmaster->TXbuff->data) - 2);
        CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

        ret = CO_LSSmaster_WAIT_SLAVE;
    }
    /* Wait for confirmation */
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_CFG_NODE_ID) {

        ret = CO_LSSmaster_configureCheckWait(LSSmaster, timeDifference_us,
                CO_LSS_CFG_NODE_ID);
    }

    if (ret!=CO_LSSmaster_INVALID_STATE && ret!=CO_LSSmaster_WAIT_SLAVE) {
        /* finished */
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    return ret;
}


/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_configureStore(
        CO_LSSmaster_t         *LSSmaster,
        uint32_t                timeDifference_us)
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

        CO_FLAG_CLEAR(LSSmaster->CANrxNew);
        LSSmaster->TXbuff->data[0] = CO_LSS_CFG_STORE;
        memset(&LSSmaster->TXbuff->data[1], 0, sizeof(LSSmaster->TXbuff->data) - 1);
        CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

        ret = CO_LSSmaster_WAIT_SLAVE;
    }
    /* Wait for confirmation */
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_CFG_STORE) {

        ret = CO_LSSmaster_configureCheckWait(LSSmaster, timeDifference_us,
                CO_LSS_CFG_STORE);
    }

    if (ret!=CO_LSSmaster_INVALID_STATE && ret!=CO_LSSmaster_WAIT_SLAVE) {
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

        CO_FLAG_CLEAR(LSSmaster->CANrxNew);
        LSSmaster->TXbuff->data[0] = CO_LSS_CFG_ACTIVATE_BIT_TIMING;
        CO_setUint16(&LSSmaster->TXbuff->data[1], switchDelay_ms);
        memset(&LSSmaster->TXbuff->data[3], 0, sizeof(LSSmaster->TXbuff->data) - 3);
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
    CO_FLAG_CLEAR(LSSmaster->CANrxNew);
    LSSmaster->TXbuff->data[0] = cs;
    memset(&LSSmaster->TXbuff->data[1], 0, sizeof(LSSmaster->TXbuff->data) - 1);
    CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);

    return CO_LSSmaster_WAIT_SLAVE;
}

/*
 * Helper function - wait for confirmation
 */
static CO_LSSmaster_return_t CO_LSSmaster_inquireCheckWait(
        CO_LSSmaster_t         *LSSmaster,
        uint32_t                timeDifference_us,
        uint8_t                 csWait,
        uint32_t               *value)
{
    CO_LSSmaster_return_t ret;

    if (CO_FLAG_READ(LSSmaster->CANrxNew)) {
        uint8_t cs = LSSmaster->CANrxData[0];
        *value = CO_getUint32(&LSSmaster->CANrxData[1]);
        CO_FLAG_CLEAR(LSSmaster->CANrxNew);

        if (cs == csWait) {
            ret = CO_LSSmaster_OK;
        }
        else {
            ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_us);
        }
    }
    else {
        ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_us);
    }

    return ret;
}

/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_InquireLssAddress(
        CO_LSSmaster_t         *LSSmaster,
        uint32_t                timeDifference_us,
        CO_LSS_address_t       *lssAddress)
{
    CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;
    CO_LSSmaster_command_t next = CO_LSSmaster_COMMAND_WAITING;

    if (LSSmaster==NULL || lssAddress==NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }

    /* Check for reply */
    if (LSSmaster->command == CO_LSSmaster_COMMAND_INQUIRE_VENDOR) {

        ret = CO_LSSmaster_inquireCheckWait(LSSmaster, timeDifference_us,
                CO_LSS_INQUIRE_VENDOR, &lssAddress->identity.vendorID);
        if (ret == CO_LSSmaster_OK) {
            /* Start next request */
            next = CO_LSSmaster_COMMAND_INQUIRE_PRODUCT;
            ret = CO_LSSmaster_WAIT_SLAVE;
        }
    }
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_INQUIRE_PRODUCT) {

        ret = CO_LSSmaster_inquireCheckWait(LSSmaster, timeDifference_us,
                CO_LSS_INQUIRE_PRODUCT, &lssAddress->identity.productCode);
        if (ret == CO_LSSmaster_OK) {
            /* Start next request */
            next = CO_LSSmaster_COMMAND_INQUIRE_REV;
            ret = CO_LSSmaster_WAIT_SLAVE;
        }
    }
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_INQUIRE_REV) {

        ret = CO_LSSmaster_inquireCheckWait(LSSmaster, timeDifference_us,
                CO_LSS_INQUIRE_REV, &lssAddress->identity.revisionNumber);
        if (ret == CO_LSSmaster_OK) {
            /* Start next request */
            next = CO_LSSmaster_COMMAND_INQUIRE_SERIAL;
            ret = CO_LSSmaster_WAIT_SLAVE;
        }
    }
    else if (LSSmaster->command == CO_LSSmaster_COMMAND_INQUIRE_SERIAL) {

        ret = CO_LSSmaster_inquireCheckWait(LSSmaster, timeDifference_us,
                CO_LSS_INQUIRE_SERIAL, &lssAddress->identity.serialNumber);
    }
    /* Check for next request */
    if (LSSmaster->state == CO_LSSmaster_STATE_CFG_SLECTIVE ||
        LSSmaster->state == CO_LSSmaster_STATE_CFG_GLOBAL) {
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

    if (ret!=CO_LSSmaster_INVALID_STATE && ret!=CO_LSSmaster_WAIT_SLAVE) {
        /* finished */
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    return ret;
}


/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_Inquire(
        CO_LSSmaster_t         *LSSmaster,
        uint32_t                timeDifference_us,
        CO_LSS_cs_t             lssInquireCs,
        uint32_t               *value)
{
  CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;

  if (LSSmaster==NULL || value==NULL){
      return CO_LSSmaster_ILLEGAL_ARGUMENT;
  }

  /* send request */
  if ((LSSmaster->state==CO_LSSmaster_STATE_CFG_SLECTIVE ||
       LSSmaster->state==CO_LSSmaster_STATE_CFG_GLOBAL) &&
       LSSmaster->command == CO_LSSmaster_COMMAND_WAITING) {

      LSSmaster->command = CO_LSSmaster_COMMAND_INQUIRE;
      LSSmaster->timeoutTimer = 0;

      ret = CO_LSSmaster_inquireInitiate(LSSmaster, lssInquireCs);
  }
  /* Check for reply */
  else if (LSSmaster->command == CO_LSSmaster_COMMAND_INQUIRE) {
      ret = CO_LSSmaster_inquireCheckWait(LSSmaster, timeDifference_us,
                                          lssInquireCs, value);
  }

  if (ret != CO_LSSmaster_WAIT_SLAVE) {
      LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
  }
  return ret;
}

/*
 * Helper function - send request
 */
static void CO_LSSmaster_FsSendMsg(
        CO_LSSmaster_t         *LSSmaster,
        uint32_t                idNumber,
        uint8_t                 bitCheck,
        uint8_t                 lssSub,
        uint8_t                 lssNext)
{
    LSSmaster->timeoutTimer = 0;

    CO_FLAG_CLEAR(LSSmaster->CANrxNew);
    LSSmaster->TXbuff->data[0] = CO_LSS_IDENT_FASTSCAN;
    CO_setUint32(&LSSmaster->TXbuff->data[1], idNumber);
    LSSmaster->TXbuff->data[5] = bitCheck;
    LSSmaster->TXbuff->data[6] = lssSub;
    LSSmaster->TXbuff->data[7] = lssNext;

    CO_CANsend(LSSmaster->CANdevTx, LSSmaster->TXbuff);
}

/*
 * Helper function - wait for confirmation
 */
static CO_LSSmaster_return_t CO_LSSmaster_FsCheckWait(
        CO_LSSmaster_t         *LSSmaster,
        uint32_t                timeDifference_us)
{
    CO_LSSmaster_return_t ret;

    ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_us);
    if (ret == CO_LSSmaster_TIMEOUT) {
        ret = CO_LSSmaster_SCAN_NOACK;

        if (CO_FLAG_READ(LSSmaster->CANrxNew)) {
            uint8_t cs = LSSmaster->CANrxData[0];
            CO_FLAG_CLEAR(LSSmaster->CANrxNew);

            if (cs == CO_LSS_IDENT_SLAVE) {
                /* At least one node is waiting for fastscan */
                ret = CO_LSSmaster_SCAN_FINISHED;
            }
        }
    }

    return ret;
}

/*
 * Helper function - initiate scan for 32 bit part of LSS address
 */
static CO_LSSmaster_return_t CO_LSSmaster_FsScanInitiate(
        CO_LSSmaster_t                  *LSSmaster,
        uint32_t                         timeDifference_us,
        CO_LSSmaster_scantype_t          scan,
        CO_LSS_fastscan_lss_sub_next     lssSub)
{
    (void)timeDifference_us;    /* unused */

    LSSmaster->fsLssSub = lssSub;
    LSSmaster->fsIdNumber = 0;

    switch (scan) {
        case CO_LSSmaster_FS_SCAN:
            break;
        case CO_LSSmaster_FS_MATCH:
            /* No scanning requested */
            return CO_LSSmaster_SCAN_FINISHED;
        case CO_LSSmaster_FS_SKIP:
        default:
            return CO_LSSmaster_SCAN_FAILED;
    }

    LSSmaster->fsBitChecked = CO_LSS_FASTSCAN_BIT31;

    /* trigger scan procedure by sending first message */
    CO_LSSmaster_FsSendMsg(LSSmaster, LSSmaster->fsIdNumber,
        LSSmaster->fsBitChecked, LSSmaster->fsLssSub, LSSmaster->fsLssSub);

    return CO_LSSmaster_WAIT_SLAVE;
}

/*
 * Helper function - scan for 32 bits of LSS address, one by one
 */
static CO_LSSmaster_return_t CO_LSSmaster_FsScanWait(
        CO_LSSmaster_t                  *LSSmaster,
        uint32_t                         timeDifference_us,
        CO_LSSmaster_scantype_t          scan)
{
    CO_LSSmaster_return_t ret;

    switch (scan) {
        case CO_LSSmaster_FS_SCAN:
            break;
        case CO_LSSmaster_FS_MATCH:
            /* No scanning requested */
            return CO_LSSmaster_SCAN_FINISHED;
        case CO_LSSmaster_FS_SKIP:
        default:
            return CO_LSSmaster_SCAN_FAILED;
    }

    ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_us);
    if (ret == CO_LSSmaster_TIMEOUT) {

        ret = CO_LSSmaster_WAIT_SLAVE;

        if (CO_FLAG_READ(LSSmaster->CANrxNew)) {
            uint8_t cs = LSSmaster->CANrxData[0];
            CO_FLAG_CLEAR(LSSmaster->CANrxNew);

            if (cs != CO_LSS_IDENT_SLAVE) {
                /* wrong response received. Can not continue */
                return CO_LSSmaster_SCAN_FAILED;
            }
        }
        else {
            /* no response received, assumption is wrong */
            LSSmaster->fsIdNumber |= 1UL << LSSmaster->fsBitChecked;
        }

        if (LSSmaster->fsBitChecked == CO_LSS_FASTSCAN_BIT0) {
            /* Scanning cycle is finished, we now have 32 bit address data */
            ret = CO_LSSmaster_SCAN_FINISHED;
        }
        else {
            LSSmaster->fsBitChecked --;

            CO_LSSmaster_FsSendMsg(LSSmaster,
                LSSmaster->fsIdNumber, LSSmaster->fsBitChecked,
                LSSmaster->fsLssSub, LSSmaster->fsLssSub);
        }
    }

    return ret;
}

/*
 * Helper function - initiate check for 32 bit part of LSS address
 */
static CO_LSSmaster_return_t CO_LSSmaster_FsVerifyInitiate(
        CO_LSSmaster_t                  *LSSmaster,
        uint32_t                         timeDifference_us,
        CO_LSSmaster_scantype_t          scan,
        uint32_t                         idNumberCheck,
        CO_LSS_fastscan_lss_sub_next     lssNext)
{
    (void)timeDifference_us;    /* unused */

    switch (scan) {
        case CO_LSSmaster_FS_SCAN:
            /* ID obtained by scan */
            break;
        case CO_LSSmaster_FS_MATCH:
            /* ID given by user */
            LSSmaster->fsIdNumber = idNumberCheck;
            break;
        case CO_LSSmaster_FS_SKIP:
        default:
            return CO_LSSmaster_SCAN_FAILED;
    }

    LSSmaster->fsBitChecked = CO_LSS_FASTSCAN_BIT0;

    /* send request */
    CO_LSSmaster_FsSendMsg(LSSmaster, LSSmaster->fsIdNumber,
        LSSmaster->fsBitChecked, LSSmaster->fsLssSub, lssNext);

    return CO_LSSmaster_WAIT_SLAVE;
}

/*
 * Helper function - verify 32 bit LSS address, request node(s) to switch
 * their state machine to the next state
 */
static CO_LSSmaster_return_t CO_LSSmaster_FsVerifyWait(
        CO_LSSmaster_t                  *LSSmaster,
        uint32_t                         timeDifference_us,
        CO_LSSmaster_scantype_t          scan,
        uint32_t                        *idNumberRet)
{
    CO_LSSmaster_return_t ret;

    if (scan == CO_LSSmaster_FS_SKIP) {
        return CO_LSSmaster_SCAN_FAILED;
    }

    ret = CO_LSSmaster_check_timeout(LSSmaster, timeDifference_us);
    if (ret == CO_LSSmaster_TIMEOUT) {

        *idNumberRet = 0;
        ret = CO_LSSmaster_SCAN_NOACK;

        if (CO_FLAG_READ(LSSmaster->CANrxNew)) {
            uint8_t cs = LSSmaster->CANrxData[0];
            CO_FLAG_CLEAR(LSSmaster->CANrxNew);

            if (cs == CO_LSS_IDENT_SLAVE) {
                *idNumberRet = LSSmaster->fsIdNumber;
                ret = CO_LSSmaster_SCAN_FINISHED;
            } else {
                ret = CO_LSSmaster_SCAN_FAILED;
            }
        }
    }

    return ret;
}

/*
 * Helper function - check which 32 bit to scan for next, if any
 */
static CO_LSS_fastscan_lss_sub_next CO_LSSmaster_FsSearchNext(
        CO_LSSmaster_t                  *LSSmaster,
        const CO_LSSmaster_fastscan_t   *fastscan)
{
    int i;

    /* we search for the next LSS address part to scan for, beginning with the
     * one after the current one. If there is none remaining, scanning is
     * finished */
    for (i = LSSmaster->fsLssSub + 1; i <= CO_LSS_FASTSCAN_SERIAL; i++) {
        if (fastscan->scan[i] != CO_LSSmaster_FS_SKIP) {
            return (CO_LSS_fastscan_lss_sub_next)i;
        }
    }
    /* node selection is triggered by switching node state machine back
     * to initial state */
    return CO_LSS_FASTSCAN_VENDOR_ID;
}

/******************************************************************************/
CO_LSSmaster_return_t CO_LSSmaster_IdentifyFastscan(
        CO_LSSmaster_t          *LSSmaster,
        uint32_t                 timeDifference_us,
        CO_LSSmaster_fastscan_t *fastscan)
{
    uint8_t i;
    uint8_t count;
    CO_LSSmaster_return_t ret = CO_LSSmaster_INVALID_STATE;
    CO_LSS_fastscan_lss_sub_next next;

    /* parameter validation */
    if (LSSmaster==NULL || fastscan==NULL){
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }
    if (fastscan->scan[0] == CO_LSSmaster_FS_SKIP) {
        /* vendor ID scan cannot be skipped */
        return CO_LSSmaster_ILLEGAL_ARGUMENT;
    }
    count = 0;
    for (i = 0; i < (sizeof(fastscan->scan) / sizeof(fastscan->scan[0])); i++) {
        if (fastscan->scan[i] == CO_LSSmaster_FS_SKIP) {
            count ++;
        }
        if (count > 2) {
            /* Node selection needs the Vendor ID and at least one other value */
            return CO_LSSmaster_ILLEGAL_ARGUMENT;
        }
    }

    /* state machine validation */
    if (LSSmaster->state!=CO_LSSmaster_STATE_WAITING ||
        (LSSmaster->command!=CO_LSSmaster_COMMAND_WAITING &&
         LSSmaster->command!=CO_LSSmaster_COMMAND_IDENTIFY_FASTSCAN)) {
        /* state machine not ready, other command is already processed */
        return CO_LSSmaster_INVALID_STATE;
    }

    /* evaluate LSS state machine */
    switch (LSSmaster->command) {
        case CO_LSSmaster_COMMAND_WAITING:
            /* start fastscan */
            LSSmaster->command = CO_LSSmaster_COMMAND_IDENTIFY_FASTSCAN;

            /* check if any nodes are waiting, if yes fastscan is reset */
            LSSmaster->fsState = CO_LSSmaster_FS_STATE_CHECK;
            CO_LSSmaster_FsSendMsg(LSSmaster, 0, CO_LSS_FASTSCAN_CONFIRM, 0, 0);

            return CO_LSSmaster_WAIT_SLAVE;
        default:
            /* continue with evaluating fastscan state machine */
            break;
    }

    /* evaluate fastscan state machine. The state machine is evaluated as following
     * - check for non-configured nodes
     * - scan for vendor ID
     * - verify vendor ID, switch node state
     * - scan for product code
     * - verify product code, switch node state
     * - scan for revision number
     * - verify revision number, switch node state
     * - scan for serial number
     * - verify serial number, switch node to LSS configuration mode
     * Certain steps can be skipped as mentioned in the function description.
     * If one step is not ack'ed by a node, the scanning process is terminated
     * and the correspondign error is returned. */
    switch (LSSmaster->fsState) {
        case CO_LSSmaster_FS_STATE_CHECK:
            ret = CO_LSSmaster_FsCheckWait(LSSmaster, timeDifference_us);
            if (ret == CO_LSSmaster_SCAN_FINISHED) {
                memset(&fastscan->found, 0, sizeof(fastscan->found));

                /* start scanning procedure by triggering vendor ID scan */
                CO_LSSmaster_FsScanInitiate(LSSmaster, timeDifference_us,
                      fastscan->scan[CO_LSS_FASTSCAN_VENDOR_ID],
                      CO_LSS_FASTSCAN_VENDOR_ID);
                ret = CO_LSSmaster_WAIT_SLAVE;

                LSSmaster->fsState = CO_LSSmaster_FS_STATE_SCAN;
            }
            break;
        case CO_LSSmaster_FS_STATE_SCAN:
            ret = CO_LSSmaster_FsScanWait(LSSmaster, timeDifference_us,
                      fastscan->scan[LSSmaster->fsLssSub]);
            if (ret == CO_LSSmaster_SCAN_FINISHED) {
                /* scanning finished, initiate verifcation. The verification
                 * message also contains the node state machine "switch to
                 * next state" request */
                next = CO_LSSmaster_FsSearchNext(LSSmaster, fastscan);
                ret = CO_LSSmaster_FsVerifyInitiate(LSSmaster, timeDifference_us,
                          fastscan->scan[LSSmaster->fsLssSub],
                          fastscan->match.addr[LSSmaster->fsLssSub], next);

                LSSmaster->fsState = CO_LSSmaster_FS_STATE_VERIFY;
            }
            break;
        case CO_LSSmaster_FS_STATE_VERIFY:
            ret = CO_LSSmaster_FsVerifyWait(LSSmaster, timeDifference_us,
                      fastscan->scan[LSSmaster->fsLssSub],
                      &fastscan->found.addr[LSSmaster->fsLssSub]);
            if (ret == CO_LSSmaster_SCAN_FINISHED) {
                /* verification successful:
                 * - assumed node id is correct
                 * - node state machine has switched to the requested state,
                 *   mirror that in the local copy */
                next = CO_LSSmaster_FsSearchNext(LSSmaster, fastscan);
                if (next == CO_LSS_FASTSCAN_VENDOR_ID) {
                    /* fastscan finished, one node is now in LSS configuration
                     * mode */
                    LSSmaster->state = CO_LSSmaster_STATE_CFG_SLECTIVE;
                }
                else {
                    /* initiate scan for next part of LSS address */
                    ret = CO_LSSmaster_FsScanInitiate(LSSmaster,
                              timeDifference_us, fastscan->scan[next], next);
                    if (ret == CO_LSSmaster_SCAN_FINISHED) {
                        /* Scanning is not requested. Initiate verification
                         * step in next function call */
                        ret = CO_LSSmaster_WAIT_SLAVE;
                    }

                    LSSmaster->fsState = CO_LSSmaster_FS_STATE_SCAN;
                }
            }
            break;
        default:
            break;
    }

    if (ret != CO_LSSmaster_WAIT_SLAVE) {
        /* finished */
        LSSmaster->command = CO_LSSmaster_COMMAND_WAITING;
    }
    return ret;
}

#endif /* (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER */
