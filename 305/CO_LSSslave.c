/*
 * CANopen LSS Slave protocol.
 *
 * @file        CO_LSSslave.c
 * @ingroup     CO_LSS
 * @author      Martin Wagner
 * @author      Janez Paternoster
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

#include "305/CO_LSSslave.h"

#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE

#include <string.h>

/* 'bit' must be unsigned or additional range check must be added: bit>=CO_LSS_FASTSCAN_BIT0 */
#define CO_LSS_FASTSCAN_BITCHECK_VALID(bit) (bit<=CO_LSS_FASTSCAN_BIT31 || bit==CO_LSS_FASTSCAN_CONFIRM)
/* 'index' must be unsigned or additional range check must be added: index>=CO_LSS_FASTSCAN_VENDOR_ID */
#define CO_LSS_FASTSCAN_LSS_SUB_NEXT_VALID(index) (index<=CO_LSS_FASTSCAN_SERIAL)
/* 'index' must be unsigned or additional range check must be added: index>=CO_LSS_BIT_TIMING_1000 */
#define CO_LSS_BIT_TIMING_VALID(index) (index != 5 && index <= CO_LSS_BIT_TIMING_AUTO)

#if CO_LSS_FASTSCAN_BIT0!=0 || CO_LSS_FASTSCAN_VENDOR_ID!=0 || CO_LSS_BIT_TIMING_1000!=0
#error Inconsistency in LSS macros
#endif

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_LSSslave_receive(void *object, void *msg)
{
    CO_LSSslave_t *LSSslave = (CO_LSSslave_t*)object;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);

    if(DLC == 8U && !CO_FLAG_READ(LSSslave->sendResponse)) {
        bool_t request_LSSslave_process = false;
        uint8_t *data = CO_CANrxMsg_readData(msg);
        CO_LSS_cs_t cs = (CO_LSS_cs_t) data[0];

        if (cs == CO_LSS_SWITCH_STATE_GLOBAL) {
            uint8_t mode = data[1];

            switch (mode) {
                case CO_LSS_STATE_WAITING:
                    if (LSSslave->lssState == CO_LSS_STATE_CONFIGURATION &&
                        LSSslave->activeNodeID == CO_LSS_NODE_ID_ASSIGNMENT &&
                        *LSSslave->pendingNodeID != CO_LSS_NODE_ID_ASSIGNMENT)
                    {
                        /* Slave process function will request NMT Reset comm.*/
                        LSSslave->service = cs;
                        request_LSSslave_process = true;
                    }
                    LSSslave->lssState = CO_LSS_STATE_WAITING;
                    memset(&LSSslave->lssSelect, 0,
                           sizeof(LSSslave->lssSelect));
                    break;
                case CO_LSS_STATE_CONFIGURATION:
                    LSSslave->lssState = CO_LSS_STATE_CONFIGURATION;
                    break;
                default:
                    break;
            }
        }
        else if(LSSslave->lssState == CO_LSS_STATE_WAITING) {
            switch (cs) {
            case CO_LSS_SWITCH_STATE_SEL_VENDOR: {
                uint32_t valSw;
                memcpy(&valSw, &data[1], sizeof(valSw));
                LSSslave->lssSelect.identity.vendorID = CO_SWAP_32(valSw);
                break;
            }
            case CO_LSS_SWITCH_STATE_SEL_PRODUCT: {
                uint32_t valSw;
                memcpy(&valSw, &data[1], sizeof(valSw));
                LSSslave->lssSelect.identity.productCode = CO_SWAP_32(valSw);
                break;
            }
            case CO_LSS_SWITCH_STATE_SEL_REV: {
                uint32_t valSw;
                memcpy(&valSw, &data[1], sizeof(valSw));
                LSSslave->lssSelect.identity.revisionNumber = CO_SWAP_32(valSw);
                break;
            }
            case CO_LSS_SWITCH_STATE_SEL_SERIAL: {
                uint32_t valSw;
                memcpy(&valSw, &data[1], sizeof(valSw));
                LSSslave->lssSelect.identity.serialNumber = CO_SWAP_32(valSw);

                if (CO_LSS_ADDRESS_EQUAL(LSSslave->lssAddress,
                                         LSSslave->lssSelect)
                ) {
                    LSSslave->lssState = CO_LSS_STATE_CONFIGURATION;
                    LSSslave->service = cs;
                    request_LSSslave_process = true;
                }
                break;
            }
            case CO_LSS_IDENT_FASTSCAN: {
                /* fastscan is only active on unconfigured nodes */
                if (*LSSslave->pendingNodeID == CO_LSS_NODE_ID_ASSIGNMENT &&
                    LSSslave->activeNodeID == CO_LSS_NODE_ID_ASSIGNMENT)
                {
                    uint8_t bitCheck = data[5];
                    uint8_t lssSub = data[6];
                    uint8_t lssNext = data[7];
                    uint32_t valSw;
                    uint32_t idNumber;
                    bool_t ack;

                    if (!CO_LSS_FASTSCAN_BITCHECK_VALID(bitCheck) ||
                        !CO_LSS_FASTSCAN_LSS_SUB_NEXT_VALID(lssSub) ||
                        !CO_LSS_FASTSCAN_LSS_SUB_NEXT_VALID(lssNext)) {
                        /* Invalid request */
                        break;
                    }

                    memcpy(&valSw, &data[1], sizeof(valSw));
                    idNumber = CO_SWAP_32(valSw);
                    ack = false;

                    if (bitCheck == CO_LSS_FASTSCAN_CONFIRM) {
                        /* Confirm, Reset */
                        ack = true;
                        LSSslave->fastscanPos = CO_LSS_FASTSCAN_VENDOR_ID;
                        memset(&LSSslave->lssFastscan, 0,
                                sizeof(LSSslave->lssFastscan));
                    }
                    else if (LSSslave->fastscanPos == lssSub) {
                        uint32_t mask = 0xFFFFFFFF << bitCheck;

                        if ((LSSslave->lssAddress.addr[lssSub] & mask)
                            == (idNumber & mask))
                        {
                            /* all requested bits match */
                            ack = true;
                            LSSslave->fastscanPos = lssNext;

                            if (bitCheck == 0 && lssNext < lssSub) {
                                /* complete match, enter configuration state */
                                LSSslave->lssState = CO_LSS_STATE_CONFIGURATION;
                            }
                        }
                    }
                    if (ack) {
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE_FASTSCAN_DIRECT_RESPOND
                        LSSslave->TXbuff->data[0] = CO_LSS_IDENT_SLAVE;
                        memset(&LSSslave->TXbuff->data[1], 0,
                               sizeof(LSSslave->TXbuff->data) - 1);
                        CO_CANsend(LSSslave->CANdevTx, LSSslave->TXbuff);
#else
                        LSSslave->service = cs;
                        request_LSSslave_process = true;
#endif
                    }
                }
                break;
            }
            default: {
                break;
            }
            }
        }
        else { /* LSSslave->lssState == CO_LSS_STATE_CONFIGURATION */
            memcpy(&LSSslave->CANdata, &data[0], sizeof(LSSslave->CANdata));
            LSSslave->service = cs;
            request_LSSslave_process = true;
        }

        if (request_LSSslave_process) {
            CO_FLAG_SET(LSSslave->sendResponse);
#if (CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE
            /* Optional signal to RTOS, which can resume task,
             * which handles further processing. */
            if (LSSslave->pFunctSignalPre != NULL) {
                LSSslave->pFunctSignalPre(LSSslave->functSignalObjectPre);
            }
#endif
        }
    }
}


/******************************************************************************/
CO_ReturnError_t CO_LSSslave_init(
        CO_LSSslave_t          *LSSslave,
        CO_LSS_address_t       *lssAddress,
        uint16_t               *pendingBitRate,
        uint8_t                *pendingNodeID,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        uint16_t                CANidLssMaster,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx,
        uint16_t                CANidLssSlave)
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if (LSSslave==NULL || pendingBitRate == NULL || pendingNodeID == NULL ||
        CANdevRx==NULL || CANdevTx==NULL ||
        !CO_LSS_NODE_ID_VALID(*pendingNodeID)
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Application must make sure that lssAddress is filled with data. */

    /* clear the object */
    memset(LSSslave, 0, sizeof(CO_LSSslave_t));

    /* Configure object variables */
    memcpy(&LSSslave->lssAddress, lssAddress, sizeof(LSSslave->lssAddress));
    LSSslave->lssState = CO_LSS_STATE_WAITING;
    LSSslave->fastscanPos = CO_LSS_FASTSCAN_VENDOR_ID;

    LSSslave->pendingBitRate = pendingBitRate;
    LSSslave->pendingNodeID = pendingNodeID;
    LSSslave->activeNodeID = *pendingNodeID;
    CO_FLAG_CLEAR(LSSslave->sendResponse);

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


#if (CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE
/******************************************************************************/
void CO_LSSslave_initCallbackPre(
        CO_LSSslave_t          *LSSslave,
        void                   *object,
        void                  (*pFunctSignalPre)(void *object))
{
    if(LSSslave != NULL){
        LSSslave->functSignalObjectPre = object;
        LSSslave->pFunctSignalPre = pFunctSignalPre;
    }
}
#endif


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
        LSSslave->functLSScfgStoreObject = object;
        LSSslave->pFunctLSScfgStore = pFunctLSScfgStore;
    }
}


/******************************************************************************/
bool_t CO_LSSslave_process(CO_LSSslave_t *LSSslave) {
    bool_t resetCommunication = false;

    if (CO_FLAG_READ(LSSslave->sendResponse)) {
        uint8_t nid;
        uint8_t errorCode;
        uint8_t errorCodeManuf;
        uint8_t tableSelector;
        uint8_t tableIndex;
        bool_t CANsend = false;
        uint32_t valSw;

        memset(&LSSslave->TXbuff->data[0], 0, sizeof(LSSslave->TXbuff->data));

        switch (LSSslave->service) {
        case CO_LSS_SWITCH_STATE_GLOBAL: {
            /* Node-Id was unconfigured before, now it is configured,
             * enter the NMT Reset communication autonomously. */
            resetCommunication = true;
            break;
        }
        case CO_LSS_SWITCH_STATE_SEL_SERIAL: {
            LSSslave->TXbuff->data[0] = CO_LSS_SWITCH_STATE_SEL;
            CANsend = true;
            break;
        }
        case CO_LSS_CFG_NODE_ID: {
            nid = LSSslave->CANdata[1];
            errorCode = CO_LSS_CFG_NODE_ID_OK;

            if (CO_LSS_NODE_ID_VALID(nid)) {
                *LSSslave->pendingNodeID = nid;
            }
            else {
                errorCode = CO_LSS_CFG_NODE_ID_OUT_OF_RANGE;
            }

            /* send confirmation */
            LSSslave->TXbuff->data[0] = LSSslave->service;
            LSSslave->TXbuff->data[1] = errorCode;
            /* we do not use spec-error, always 0 */
            CANsend = true;
            break;
        }
        case CO_LSS_CFG_BIT_TIMING: {
            if (LSSslave->pFunctLSScheckBitRate == NULL) {
                /* setting bit timing is not supported. Drop request */
                break;
            }

            tableSelector = LSSslave->CANdata[1];
            tableIndex = LSSslave->CANdata[2];
            errorCode = CO_LSS_CFG_BIT_TIMING_OK;
            errorCodeManuf = CO_LSS_CFG_BIT_TIMING_OK;

            if (tableSelector == 0 && CO_LSS_BIT_TIMING_VALID(tableIndex)) {
                uint16_t bit = CO_LSS_bitTimingTableLookup[tableIndex];
                bool_t bit_rate_supported = LSSslave->pFunctLSScheckBitRate(
                    LSSslave->functLSScheckBitRateObject, bit);

                if (bit_rate_supported) {
                    *LSSslave->pendingBitRate = bit;
                }
                else {
                    errorCode = CO_LSS_CFG_BIT_TIMING_MANUFACTURER;
                    errorCodeManuf = CO_LSS_CFG_BIT_TIMING_OUT_OF_RANGE;
                }
            }
            else {
                /* we currently only support CiA301 bit timing table */
                errorCode = CO_LSS_CFG_BIT_TIMING_OUT_OF_RANGE;
            }

            /* send confirmation */
            LSSslave->TXbuff->data[0] = LSSslave->service;
            LSSslave->TXbuff->data[1] = errorCode;
            LSSslave->TXbuff->data[2] = errorCodeManuf;
            CANsend = true;
            break;
        }
        case CO_LSS_CFG_ACTIVATE_BIT_TIMING: {
            if (LSSslave->pFunctLSScheckBitRate == NULL) {
                /* setting bit timing is not supported. Drop request */
                break;
            }

            /* notify application */
            if (LSSslave->pFunctLSSactivateBitRate != NULL) {
                uint16_t delay = ((uint16_t) LSSslave->CANdata[2]) << 8;
                delay |= LSSslave->CANdata[1];
                LSSslave->pFunctLSSactivateBitRate(
                    LSSslave->functLSSactivateBitRateObject, delay);
            }
            break;
        }
        case CO_LSS_CFG_STORE: {
            errorCode = CO_LSS_CFG_STORE_OK;

            if (LSSslave->pFunctLSScfgStore == NULL) {
                /* storing is not supported. Reply error */
                errorCode = CO_LSS_CFG_STORE_NOT_SUPPORTED;
            }
            else {
                bool_t result;
                /* Store "pending" to "persistent" */
                result =
                   LSSslave->pFunctLSScfgStore(LSSslave->functLSScfgStoreObject,
                                               *LSSslave->pendingNodeID,
                                               *LSSslave->pendingBitRate);
                if (!result) {
                    errorCode = CO_LSS_CFG_STORE_FAILED;
                }
            }

            /* send confirmation */
            LSSslave->TXbuff->data[0] = LSSslave->service;
            LSSslave->TXbuff->data[1] = errorCode;
            /* we do not use spec-error, always 0 */
            CANsend = true;
            break;
        }
        case CO_LSS_INQUIRE_VENDOR: {
            LSSslave->TXbuff->data[0] = LSSslave->service;
            valSw = CO_SWAP_32(LSSslave->lssAddress.identity.vendorID);
            memcpy(&LSSslave->TXbuff->data[1], &valSw, sizeof(valSw));
            CANsend = true;
            break;
        }
        case CO_LSS_INQUIRE_PRODUCT: {
            LSSslave->TXbuff->data[0] = LSSslave->service;
            valSw = CO_SWAP_32(LSSslave->lssAddress.identity.productCode);
            memcpy(&LSSslave->TXbuff->data[1], &valSw, sizeof(valSw));
            CANsend = true;
            break;
        }
        case CO_LSS_INQUIRE_REV: {
            LSSslave->TXbuff->data[0] = LSSslave->service;
            valSw = CO_SWAP_32(LSSslave->lssAddress.identity.revisionNumber);
            memcpy(&LSSslave->TXbuff->data[1], &valSw, sizeof(valSw));
            CANsend = true;
            break;
        }
        case CO_LSS_INQUIRE_SERIAL: {
            LSSslave->TXbuff->data[0] = LSSslave->service;
            valSw = CO_SWAP_32(LSSslave->lssAddress.identity.serialNumber);
            memcpy(&LSSslave->TXbuff->data[1], &valSw, sizeof(valSw));
            CANsend = true;
            break;
        }
        case CO_LSS_INQUIRE_NODE_ID: {
            LSSslave->TXbuff->data[0] = LSSslave->service;
            LSSslave->TXbuff->data[1] = LSSslave->activeNodeID;
            CANsend = true;
            break;
        }
        case CO_LSS_IDENT_FASTSCAN: {
            LSSslave->TXbuff->data[0] = CO_LSS_IDENT_SLAVE;
            CANsend = true;
            break;
        }
        default: {
            break;
        }
        }

        if(CANsend) {
            CO_CANsend(LSSslave->CANdevTx, LSSslave->TXbuff);
        }

        CO_FLAG_CLEAR(LSSslave->sendResponse);
    }

    return resetCommunication;
}

#endif /* (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE */
