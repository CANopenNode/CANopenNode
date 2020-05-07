/*
 * CANopen LSS Slave protocol.
 *
 * @file        CO_LSSslave.c
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

#include <string.h>

#include "301/CO_driver.h"
#include "301/CO_SDOserver.h" /* for helper functions */
#include "305/CO_LSSslave.h"

/*
 * Helper function - Handle service "switch state global"
 */
static void CO_LSSslave_serviceSwitchStateGlobal(
    CO_LSSslave_t *LSSslave,
    CO_LSS_cs_t service,
    void *msg)
{
    (void)service;  /* unused */

    uint8_t *data = CO_CANrxMsg_readData(msg);
    uint8_t mode = data[1];

    switch (mode) {
        case CO_LSS_STATE_WAITING:
            LSSslave->lssState = CO_LSS_STATE_WAITING;
            memset(&LSSslave->lssSelect, 0, sizeof(LSSslave->lssSelect));
            break;
        case CO_LSS_STATE_CONFIGURATION:
            LSSslave->lssState = CO_LSS_STATE_CONFIGURATION;
            break;
        default:
            break;
    }
}

/*
 * Helper function - Handle service "switch state selective"
 */
static void CO_LSSslave_serviceSwitchStateSelective(
    CO_LSSslave_t *LSSslave,
    CO_LSS_cs_t service,
    void *msg)
{
    uint32_t value;
    uint8_t *data = CO_CANrxMsg_readData(msg);
    CO_memcpySwap4(&value, &data[1]);

    if(LSSslave->lssState != CO_LSS_STATE_WAITING) {
        return;
    }

    switch (service) {
        case CO_LSS_SWITCH_STATE_SEL_VENDOR:
            LSSslave->lssSelect.identity.vendorID = value;
            break;
        case CO_LSS_SWITCH_STATE_SEL_PRODUCT:
            LSSslave->lssSelect.identity.productCode = value;
            break;
        case CO_LSS_SWITCH_STATE_SEL_REV:
            LSSslave->lssSelect.identity.revisionNumber = value;
            break;
        case CO_LSS_SWITCH_STATE_SEL_SERIAL:
            LSSslave->lssSelect.identity.serialNumber = value;

            if (CO_LSS_ADDRESS_EQUAL(LSSslave->lssAddress, LSSslave->lssSelect)) {
                LSSslave->lssState = CO_LSS_STATE_CONFIGURATION;

                /* send confirmation */
                LSSslave->TXbuff->data[0] = CO_LSS_SWITCH_STATE_SEL;
                memset(&LSSslave->TXbuff->data[1], 0, sizeof(LSSslave->TXbuff->data) - 1);
                CO_CANsend(LSSslave->CANdevTx, LSSslave->TXbuff);
            }
            break;
        default:
            break;
    }
}

/*
 * Helper function - Handle service "configure"
 *
 * values inside message have different meaning, depending on the selected
 * configuration type
 */
static void CO_LSSslave_serviceConfig(
    CO_LSSslave_t *LSSslave,
    CO_LSS_cs_t service,
    void *msg)
{
    uint8_t nid;
    uint8_t tableSelector;
    uint8_t tableIndex;
    uint8_t errorCode;
    uint8_t *data;

    if(LSSslave->lssState != CO_LSS_STATE_CONFIGURATION) {
        return;
    }

    data = CO_CANrxMsg_readData(msg);

    switch (service) {
        case CO_LSS_CFG_NODE_ID:
            nid = data[1];
            errorCode = CO_LSS_CFG_NODE_ID_OK;

            if (CO_LSS_NODE_ID_VALID(nid)) {
                LSSslave->pendingNodeID = nid;
            }
            else {
                errorCode = CO_LSS_CFG_NODE_ID_OUT_OF_RANGE;
            }

            /* send confirmation */
            LSSslave->TXbuff->data[0] = CO_LSS_CFG_NODE_ID;
            LSSslave->TXbuff->data[1] = errorCode;
            /* we do not use spec-error, always 0 */
            memset(&LSSslave->TXbuff->data[2], 0, sizeof(LSSslave->TXbuff->data) - 2);
            CO_CANsend(LSSslave->CANdevTx, LSSslave->TXbuff);
            break;
        case CO_LSS_CFG_BIT_TIMING:
            if (LSSslave->pFunctLSScheckBitRate == NULL) {
                /* setting bit timing is not supported. Drop request */
                break;
            }

            tableSelector = data[1];
            tableIndex = data[2];
            errorCode = CO_LSS_CFG_BIT_TIMING_OK;

            if (tableSelector==0 && CO_LSS_BIT_TIMING_VALID(tableIndex)) {
                uint16_t bit = CO_LSS_bitTimingTableLookup[tableIndex];
                bool_t bit_rate_supported  = LSSslave->pFunctLSScheckBitRate(
                    LSSslave->functLSScheckBitRateObject, bit);

                if (bit_rate_supported) {
                    LSSslave->pendingBitRate = bit;
                }
                else {
                    errorCode = CO_LSS_CFG_BIT_TIMING_OUT_OF_RANGE;
                }
            }
            else {
                /* we currently only support CiA301 bit timing table */
                errorCode = CO_LSS_CFG_BIT_TIMING_OUT_OF_RANGE;
            }

            /* send confirmation */
            LSSslave->TXbuff->data[0] = CO_LSS_CFG_BIT_TIMING;
            LSSslave->TXbuff->data[1] = errorCode;
            /* we do not use spec-error, always 0 */
            memset(&LSSslave->TXbuff->data[2], 0, sizeof(LSSslave->TXbuff->data) - 2);
            CO_CANsend(LSSslave->CANdevTx, LSSslave->TXbuff);
            break;
        case CO_LSS_CFG_ACTIVATE_BIT_TIMING:
            if (LSSslave->pFunctLSScheckBitRate == NULL) {
                /* setting bit timing is not supported. Drop request */
                break;
            }

            /* notify application */
            if (LSSslave->pFunctLSSactivateBitRate != NULL) {
              uint16_t delay;
              CO_memcpySwap2(&delay, &data[1]);
              LSSslave->pFunctLSSactivateBitRate(
                  LSSslave->functLSSactivateBitRateObject, delay);
            }
            break;
        case CO_LSS_CFG_STORE:
            errorCode = CO_LSS_CFG_STORE_OK;

            if (LSSslave->pFunctLSScfgStore == NULL) {
                /* storing is not supported. Reply error */
                errorCode = CO_LSS_CFG_STORE_NOT_SUPPORTED;
            }
            else {
                bool_t result;
                /* Store "pending" to "persistent" */
                result = LSSslave->pFunctLSScfgStore(LSSslave->functLSScfgStore,
                    LSSslave->pendingNodeID, LSSslave->pendingBitRate);
                if (!result) {
                    errorCode = CO_LSS_CFG_STORE_FAILED;
                }
            }

            /* send confirmation */
            LSSslave->TXbuff->data[0] = CO_LSS_CFG_STORE;
            LSSslave->TXbuff->data[1] = errorCode;
            /* we do not use spec-error, always 0 */
            memset(&LSSslave->TXbuff->data[2], 0, sizeof(LSSslave->TXbuff->data) - 2);
            CO_CANsend(LSSslave->CANdevTx, LSSslave->TXbuff);
            break;
        default:
            break;
    }
}

/*
 * Helper function - Handle service "inquire"
 */
static void CO_LSSslave_serviceInquire(
    CO_LSSslave_t *LSSslave,
    CO_LSS_cs_t service,
    void *msg)
{
    (void)msg;  /* unused */

    uint32_t value;

    if(LSSslave->lssState != CO_LSS_STATE_CONFIGURATION) {
        return;
    }

    switch (service) {
        case CO_LSS_INQUIRE_VENDOR:
            value = LSSslave->lssAddress.identity.vendorID;
            break;
        case CO_LSS_INQUIRE_PRODUCT:
            value = LSSslave->lssAddress.identity.productCode;
            break;
        case CO_LSS_INQUIRE_REV:
            value = LSSslave->lssAddress.identity.revisionNumber;
            break;
        case CO_LSS_INQUIRE_SERIAL:
            value = LSSslave->lssAddress.identity.serialNumber;
            break;
        case CO_LSS_INQUIRE_NODE_ID:
            value = (uint32_t)LSSslave->activeNodeID;
            break;
        default:
            return;
    }
    /* send response */
    LSSslave->TXbuff->data[0] = service;
    CO_memcpySwap4(&LSSslave->TXbuff->data[1], &value);
    memset(&LSSslave->TXbuff->data[5], 0, sizeof(LSSslave->TXbuff->data) - 5);
    CO_CANsend(LSSslave->CANdevTx, LSSslave->TXbuff);
}

/*
 * Helper function - Handle service "identify"
 */
static void CO_LSSslave_serviceIdent(
    CO_LSSslave_t *LSSslave,
    CO_LSS_cs_t service,
    void *msg)
{
    uint32_t idNumber;
    uint8_t bitCheck;
    uint8_t lssSub;
    uint8_t lssNext;
    bool_t ack;
    uint8_t *data = CO_CANrxMsg_readData(msg);

    if (LSSslave->lssState != CO_LSS_STATE_WAITING) {
        /* fastscan is only allowed in waiting state */
        return;
    }
    if (service != CO_LSS_IDENT_FASTSCAN) {
        /* we only support "fastscan" identification */
        return;
    }
    if (LSSslave->pendingNodeID!=CO_LSS_NODE_ID_ASSIGNMENT ||
        LSSslave->activeNodeID!=CO_LSS_NODE_ID_ASSIGNMENT) {
        /* fastscan is only active on unconfigured nodes */
        return;
    }

    CO_memcpySwap4(&idNumber, &data[1]);
    bitCheck = data[5];
    lssSub = data[6];
    lssNext = data[7];

    if (!CO_LSS_FASTSCAN_BITCHECK_VALID(bitCheck) ||
        !CO_LSS_FASTSCAN_LSS_SUB_NEXT_VALID(lssSub) ||
        !CO_LSS_FASTSCAN_LSS_SUB_NEXT_VALID(lssNext)) {
        /* Invalid request */
        return;
    }

    ack = false;
    if (bitCheck == CO_LSS_FASTSCAN_CONFIRM) {
        /* Confirm, Reset */
        ack = true;
        LSSslave->fastscanPos = CO_LSS_FASTSCAN_VENDOR_ID;
        memset(&LSSslave->lssFastscan, 0, sizeof(LSSslave->lssFastscan));
    }
    else if (LSSslave->fastscanPos == lssSub) {
        uint32_t mask = 0xFFFFFFFF << bitCheck;

        if ((LSSslave->lssAddress.addr[lssSub] & mask) == (idNumber & mask)) {
            /* all requested bits match */
            ack = true;
            LSSslave->fastscanPos = lssNext;

            if (bitCheck==0 && lssNext<lssSub) {
                /* complete match, enter configuration state */
                LSSslave->lssState = CO_LSS_STATE_CONFIGURATION;
            }
        }
    }
    if (ack) {
        LSSslave->TXbuff->data[0] = CO_LSS_IDENT_SLAVE;
        memset(&LSSslave->TXbuff->data[1], 0, sizeof(LSSslave->TXbuff->data) - 1);
        CO_CANsend(LSSslave->CANdevTx, LSSslave->TXbuff);
    }
}


/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_LSSslave_receive(void *object, void *msg)
{
    CO_LSSslave_t *LSSslave;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);

    LSSslave = (CO_LSSslave_t*)object;   /* this is the correct pointer type of the first argument */

    if(DLC == 8){
        CO_LSS_cs_t cs = (CO_LSS_cs_t) data[0];

        if (CO_LSS_CS_SERVICE_IS_SWITCH_GLOBAL(cs)) {
            CO_LSSslave_serviceSwitchStateGlobal(LSSslave, cs, msg);
        }
        else if (CO_LSS_CS_SERVICE_IS_SWITCH_STATE_SELECTIVE(cs)) {
            CO_LSSslave_serviceSwitchStateSelective(LSSslave, cs, msg);
        }
        else if (CO_LSS_CS_SERVICE_IS_CONFIG(cs)) {
            CO_LSSslave_serviceConfig(LSSslave, cs, msg);
        }
        else if (CO_LSS_CS_SERVICE_IS_INQUIRE(cs)) {
            CO_LSSslave_serviceInquire(LSSslave, cs, msg);
        }
        else if (CO_LSS_CS_SERVICE_IS_IDENT(cs)) {
            CO_LSSslave_serviceIdent(LSSslave, cs, msg);
        }
        else {
            /* No Ack -> Unsupported commands are dropped */
        }
    }
}


/******************************************************************************/
CO_ReturnError_t CO_LSSslave_init(
        CO_LSSslave_t          *LSSslave,
        CO_LSS_address_t        lssAddress,
        uint16_t                pendingBitRate,
        uint8_t                 pendingNodeID,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        uint32_t                CANidLssMaster,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx,
        uint32_t                CANidLssSlave)
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if (LSSslave==NULL || CANdevRx==NULL || CANdevTx==NULL ||
        !CO_LSS_NODE_ID_VALID(pendingNodeID)) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* check LSS address for plausibility. As a bare minimum, the vendor
     * ID and serial number must be set */
    if (lssAddress.identity.vendorID==0 || lssAddress.identity.serialNumber==0) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    memcpy(&LSSslave->lssAddress, &lssAddress, sizeof(LSSslave->lssAddress));
    LSSslave->lssState = CO_LSS_STATE_WAITING;
    memset(&LSSslave->lssSelect, 0, sizeof(LSSslave->lssSelect));

    memset(&LSSslave->lssFastscan, 0, sizeof(LSSslave->lssFastscan));
    LSSslave->fastscanPos = CO_LSS_FASTSCAN_VENDOR_ID;

    LSSslave->pendingBitRate = pendingBitRate;
    LSSslave->pendingNodeID = pendingNodeID;
    LSSslave->activeNodeID = CO_LSS_NODE_ID_ASSIGNMENT;
    LSSslave->pFunctLSScheckBitRate = NULL;
    LSSslave->functLSScheckBitRateObject = NULL;
    LSSslave->pFunctLSSactivateBitRate = NULL;
    LSSslave->functLSSactivateBitRateObject = NULL;
    LSSslave->pFunctLSScfgStore = NULL;
    LSSslave->functLSScfgStore = NULL;

    /* configure LSS CAN Master message reception */
    ret = CO_CANrxBufferInit(
            CANdevRx,             /* CAN device */
            CANdevRxIdx,          /* rx buffer index */
            CANidLssMaster,       /* CAN identifier */
            0x7FF,                /* mask */
            0,                    /* rtr */
            (void*)LSSslave,      /* object passed to receive function */
            CO_LSSslave_receive); /* this function will process received message */

    /* configure LSS CAN Slave response message transmission */
    LSSslave->CANdevTx = CANdevTx;
    LSSslave->TXbuff = CO_CANtxBufferInit(
            CANdevTx,             /* CAN device */
            CANdevTxIdx,          /* index of specific buffer inside CAN module */
            CANidLssSlave,        /* CAN identifier */
            0,                    /* rtr */
            8,                    /* number of data bytes */
            0);                   /* synchronous message flag bit */

    if (LSSslave->TXbuff == NULL) {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}


/******************************************************************************/
void CO_LSSslave_initCheckBitRateCallback(
        CO_LSSslave_t          *LSSslave,
        void                   *object,
        bool_t                (*pFunctLSScheckBitRate)(void *object, uint16_t bitRate))
{
    if(LSSslave != NULL){
        LSSslave->functLSScheckBitRateObject = object;
        LSSslave->pFunctLSScheckBitRate = pFunctLSScheckBitRate;
    }
}


/******************************************************************************/
void CO_LSSslave_initActivateBitRateCallback(
        CO_LSSslave_t          *LSSslave,
        void                   *object,
        void                  (*pFunctLSSactivateBitRate)(void *object, uint16_t delay))
{
    if(LSSslave != NULL){
        LSSslave->functLSSactivateBitRateObject = object;
        LSSslave->pFunctLSSactivateBitRate = pFunctLSSactivateBitRate;
    }
}


/******************************************************************************/
void CO_LSSslave_initCfgStoreCallback(
        CO_LSSslave_t          *LSSslave,
        void                   *object,
        bool_t                (*pFunctLSScfgStore)(void *object, uint8_t id, uint16_t bitRate))
{
    if(LSSslave != NULL){
        LSSslave->functLSScfgStore = object;
        LSSslave->pFunctLSScfgStore = pFunctLSScfgStore;
    }
}


/******************************************************************************/
void CO_LSSslave_process(
        CO_LSSslave_t          *LSSslave,
        uint16_t                activeBitRate,
        uint8_t                 activeNodeId,
        uint16_t               *pendingBitRate,
        uint8_t                *pendingNodeId)
{
    (void)activeBitRate;    /* unused */

    LSSslave->activeNodeID = activeNodeId;
    *pendingBitRate = LSSslave->pendingBitRate;
    *pendingNodeId = LSSslave->pendingNodeID;
}


/******************************************************************************/
CO_LSS_state_t CO_LSSslave_getState(
        CO_LSSslave_t          *LSSslave)
{
  if(LSSslave != NULL){
      return LSSslave->lssState;
  }
  return CO_LSS_STATE_WAITING;
}


/******************************************************************************/
bool_t CO_LSSslave_LEDprocess(
        CO_LSSslave_t          *LSSslave,
        uint32_t                timeDifference_us,
        bool_t *LEDon)
{
    static uint32_t ms50 = 0;
    static int8_t flash1, flash2;

    if (LSSslave == NULL || LEDon == NULL)
        return false;
    ms50 += timeDifference_us;
    if(ms50 >= 50000) {
        ms50 -= 50000;
        /* 4 cycles on, 50 cycles off */
        if(++flash1 >= 4) flash1 = -50;

        /* 4 cycles on, 4 cycles off, 4 cycles on, 50 cycles off */
        switch(++flash2){
            case    4:  flash2 = -104; break;
            case -100:  flash2 =  100; break;
            case  104:  flash2 =  -50; break;
        }
    }
    if (LSSslave->lssState == CO_LSS_STATE_CONFIGURATION)
    {
        *LEDon = (flash2 >= 0);
        return true;
    }
    else if (LSSslave->activeNodeID == CO_LSS_NODE_ID_ASSIGNMENT)
    {
        *LEDon = (flash1 >= 0);
        return true;
    }
    return false;
}
