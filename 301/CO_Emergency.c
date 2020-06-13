/*
 * CANopen Emergency object.
 *
 * @file        CO_Emergency.c
 * @ingroup     CO_Emergency
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


#include <string.h>

#include "301/CO_driver.h"
#include "301/CO_SDOserver.h"
#include "301/CO_Emergency.h"


#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_EM_receive(void *object, void *msg) {
    CO_EM_t *em;

    em = (CO_EM_t*)object;

    if(em!=NULL && em->pFunctSignalRx!=NULL){
        uint16_t ident = CO_CANrxMsg_readIdent(msg);
        if (ident != 0x80) {
            /* ignore sync messages (necessary if sync object is not used) */
            uint8_t *data = CO_CANrxMsg_readData(msg);
            uint16_t errorCode;
            uint32_t infoCode;

            CO_memcpySwap2(&errorCode, &data[0]);
            CO_memcpySwap4(&infoCode, &data[4]);
            em->pFunctSignalRx(ident,
                            errorCode,
                            data[2],
                            data[3],
                            infoCode);
        }
    }
}
#endif

/*
 * Function for accessing _Pre-Defined Error Field_ (index 0x1003) from SDO server.
 *
 * For more information see file CO_SDOserver.h.
 */
static CO_SDO_abortCode_t CO_ODF_1003(CO_ODF_arg_t *ODF_arg);
static CO_SDO_abortCode_t CO_ODF_1003(CO_ODF_arg_t *ODF_arg){
    CO_EMpr_t *emPr;
    uint8_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    emPr = (CO_EMpr_t*) ODF_arg->object;
    value = ODF_arg->data[0];

    if(ODF_arg->reading){
        uint8_t noOfErrors;
        noOfErrors = emPr->preDefErrNoOfErrors;

        if(ODF_arg->subIndex == 0U){
            ODF_arg->data[0] = noOfErrors;
        }
        else if(ODF_arg->subIndex > noOfErrors){
            ret = CO_SDO_AB_NO_DATA;
        }
        else{
            ret = CO_SDO_AB_NONE;
        }
    }
    else{
        /* only '0' may be written to subIndex 0 */
        if(ODF_arg->subIndex == 0U){
            if(value == 0U){
                emPr->preDefErrNoOfErrors = 0U;
            }
            else{
                ret = CO_SDO_AB_INVALID_VALUE;
            }
        }
        else{
            ret = CO_SDO_AB_READONLY;
        }
    }

    return ret;
}


/*
 * Function for accessing _COB ID EMCY_ (index 0x1014) from SDO server.
 *
 * For more information see file CO_SDOserver.h.
 */
static CO_SDO_abortCode_t CO_ODF_1014(CO_ODF_arg_t *ODF_arg);
static CO_SDO_abortCode_t CO_ODF_1014(CO_ODF_arg_t *ODF_arg){
    uint8_t *nodeId;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    nodeId = (uint8_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    /* add nodeId to the value */
    if(ODF_arg->reading){
        CO_setUint32(ODF_arg->data, value + *nodeId);
    }

    return ret;
}


/******************************************************************************/
CO_ReturnError_t CO_EM_init(
        CO_EM_t                *em,
        CO_EMpr_t              *emPr,
        CO_SDO_t               *SDO,
        uint8_t                *errorStatusBits,
        uint8_t                 errorStatusBitsSize,
        uint8_t                *errorRegister,
        uint32_t               *preDefErr,
        uint8_t                 preDefErrSize,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx,
        uint16_t                CANidTxEM)
{
    uint8_t i;
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if(em==NULL || emPr==NULL || SDO==NULL || errorStatusBits==NULL || errorStatusBitsSize<6U ||
       errorRegister==NULL || preDefErr==NULL || CANdevTx==NULL
#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
       || CANdevRx==NULL
#endif
    ){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    em->errorStatusBits         = errorStatusBits;
    em->errorStatusBitsSize     = errorStatusBitsSize;
    em->bufEnd                  = em->buf + (CO_EM_INTERNAL_BUFFER_SIZE * 8);
    em->bufWritePtr             = em->buf;
    em->bufReadPtr              = em->buf;
    em->bufFull                 = 0U;
    em->wrongErrorReport        = 0U;
#if (CO_CONFIG_EM) & CO_CONFIG_FLAG_CALLBACK_PRE
    em->pFunctSignalPre         = NULL;
    em->functSignalObjectPre    = NULL;
#endif
#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
    em->pFunctSignalRx          = NULL;
#endif
    emPr->em                    = em;
    emPr->errorRegister         = errorRegister;
    emPr->preDefErr             = preDefErr;
    emPr->preDefErrSize         = preDefErrSize;
    emPr->preDefErrNoOfErrors   = 0U;
    emPr->inhibitEmTimer        = 0U;
    emPr->CANerrorStatusOld     = 0U;

    /* clear error status bits */
    for(i=0U; i<errorStatusBitsSize; i++){
        em->errorStatusBits[i] = 0U;
    }

    /* Configure Object dictionary entry at index 0x1003 and 0x1014 */
    CO_OD_configure(SDO, OD_H1003_PREDEF_ERR_FIELD, CO_ODF_1003, (void*)emPr, 0, 0U);
    CO_OD_configure(SDO, OD_H1014_COBID_EMERGENCY, CO_ODF_1014, (void*)&SDO->nodeId, 0, 0U);

#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
    /* configure SDO server CAN reception */
    ret = CO_CANrxBufferInit(
            CANdevRx,               /* CAN device */
            CANdevRxIdx,            /* rx buffer index */
            CO_CAN_ID_EMERGENCY,    /* CAN identifier */
            0x780,                  /* mask */
            0,                      /* rtr */
            (void*)em,              /* object passed to receive function */
            CO_EM_receive);         /* this function will process received message */
#endif

    /* configure emergency message CAN transmission */
    emPr->CANdev = CANdevTx;
    emPr->CANtxBuff = CO_CANtxBufferInit(
            CANdevTx,            /* CAN device */
            CANdevTxIdx,        /* index of specific buffer inside CAN module */
            CANidTxEM,          /* CAN identifier */
            0,                  /* rtr */
            8U,                 /* number of data bytes */
            0);                 /* synchronous message flag bit */

    if (emPr->CANtxBuff == NULL) {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}


#if (CO_CONFIG_EM) & CO_CONFIG_FLAG_CALLBACK_PRE
/******************************************************************************/
void CO_EM_initCallbackPre(
        CO_EM_t                *em,
        void                   *object,
        void                  (*pFunctSignal)(void *object))
{
    if(em != NULL){
        em->functSignalObjectPre = object;
        em->pFunctSignalPre = pFunctSignal;
    }
}
#endif


#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
/******************************************************************************/
void CO_EM_initCallbackRx(
        CO_EM_t                *em,
        void                  (*pFunctSignalRx)(const uint16_t ident,
                                                const uint16_t errorCode,
                                                const uint8_t errorRegister,
                                                const uint8_t errorBit,
                                                const uint32_t infoCode))
{
    if(em != NULL){
        em->pFunctSignalRx = pFunctSignalRx;
    }
}
#endif


/******************************************************************************/
void CO_EM_process(
        CO_EMpr_t              *emPr,
        bool_t                  NMTisPreOrOperational,
        uint32_t                timeDifference_us,
        uint16_t                emInhTime_100us,
        uint32_t               *timerNext_us)
{

    CO_EM_t *em = emPr->em;
    uint8_t errorRegister;
    uint8_t errorMask;
    uint8_t i;
    uint32_t emInhTime_us = (uint32_t)emInhTime_100us * 100;
    uint16_t CANerrSt = emPr->CANdev->CANerrorStatus;

    /* verify errors from driver */
    if (CANerrSt != emPr->CANerrorStatusOld) {
        uint16_t CANerrStChanged = CANerrSt ^ emPr->CANerrorStatusOld;
        emPr->CANerrorStatusOld = CANerrSt;

        if (CANerrStChanged & (CO_CAN_ERRTX_WARNING | CO_CAN_ERRRX_WARNING)) {
            if (CANerrSt & (CO_CAN_ERRTX_WARNING | CO_CAN_ERRRX_WARNING))
                CO_errorReport(em, CO_EM_CAN_BUS_WARNING, CO_EMC_NO_ERROR, 0);
            else
                CO_errorReset(em, CO_EM_CAN_BUS_WARNING, 0);
        }

        if (CANerrStChanged & CO_CAN_ERRTX_PASSIVE) {
            if (CANerrSt & CO_CAN_ERRTX_PASSIVE)
                CO_errorReport(em, CO_EM_CAN_TX_BUS_PASSIVE,
                               CO_EMC_CAN_PASSIVE, 0);
            else
                CO_errorReset(em, CO_EM_CAN_TX_BUS_PASSIVE, 0);
        }

        if (CANerrStChanged & CO_CAN_ERRTX_BUS_OFF) {
            if (CANerrSt & CO_CAN_ERRTX_BUS_OFF)
                CO_errorReport(em, CO_EM_CAN_TX_BUS_OFF,
                               CO_EMC_BUS_OFF_RECOVERED, 0);
            else
                CO_errorReset(em, CO_EM_CAN_TX_BUS_OFF, 0);
        }

        if (CANerrStChanged & CO_CAN_ERRTX_OVERFLOW) {
            if (CANerrSt & CO_CAN_ERRTX_OVERFLOW)
                CO_errorReport(em, CO_EM_CAN_TX_OVERFLOW,
                               CO_EMC_CAN_OVERRUN, 0);
            else
                CO_errorReset(em, CO_EM_CAN_TX_OVERFLOW, 0);
        }

        if (CANerrStChanged & CO_CAN_ERRTX_PDO_LATE) {
            if (CANerrSt & CO_CAN_ERRTX_PDO_LATE)
                CO_errorReport(em, CO_EM_TPDO_OUTSIDE_WINDOW,
                               CO_EMC_COMMUNICATION, 0);
            else
                CO_errorReset(em, CO_EM_TPDO_OUTSIDE_WINDOW, 0);
        }

        if (CANerrStChanged & CO_CAN_ERRRX_PASSIVE) {
            if (CANerrSt & CO_CAN_ERRRX_PASSIVE)
                CO_errorReport(em, CO_EM_CAN_RX_BUS_PASSIVE,
                               CO_EMC_CAN_PASSIVE, 0);
            else
                CO_errorReset(em, CO_EM_CAN_RX_BUS_PASSIVE, 0);
        }

        if (CANerrStChanged & CO_CAN_ERRRX_OVERFLOW) {
            if (CANerrSt & CO_CAN_ERRRX_OVERFLOW)
                CO_errorReport(em, CO_EM_CAN_RXB_OVERFLOW,
                               CO_EMC_CAN_OVERRUN, 0);
            else
                CO_errorReset(em, CO_EM_CAN_RXB_OVERFLOW, 0);
        }
    }

    /* verify other errors */
    if(em->wrongErrorReport != 0U){
        CO_errorReport(em, CO_EM_WRONG_ERROR_REPORT, CO_EMC_SOFTWARE_INTERNAL, (uint32_t)em->wrongErrorReport);
        em->wrongErrorReport = 0U;
    }


    /* calculate Error register */
    errorRegister = 0U;
    errorMask = (uint8_t)~(CO_ERR_REG_GENERIC_ERR | CO_ERR_REG_COMM_ERR | CO_ERR_REG_MANUFACTURER);
    /* generic error */
    if(em->errorStatusBits[5]){
        errorRegister |= CO_ERR_REG_GENERIC_ERR;
    }
    /* communication error (overrun, error state) */
    if(em->errorStatusBits[2] || em->errorStatusBits[3]){
        errorRegister |= CO_ERR_REG_COMM_ERR;
    }
    /* Manufacturer */
    for(i=6; i<em->errorStatusBitsSize; i++) {
        if (em->errorStatusBits[i]) {
            errorRegister |= CO_ERR_REG_MANUFACTURER;
        }
    }
    *emPr->errorRegister = (*emPr->errorRegister & errorMask) | errorRegister;

    /* inhibit time */
    if (emPr->inhibitEmTimer < emInhTime_us) {
        emPr->inhibitEmTimer += timeDifference_us;
    }

    /* send Emergency message. */
    if(     NMTisPreOrOperational &&
            !emPr->CANtxBuff->bufferFull &&
            (em->bufReadPtr != em->bufWritePtr || em->bufFull))
    {
        uint32_t preDEF;    /* preDefinedErrorField */

        if (emPr->inhibitEmTimer >= emInhTime_us) {
            /* inhibit time elapsed, send message */

            /* add error register */
            em->bufReadPtr[2] = *emPr->errorRegister;

#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
            /* report also own emergency messages */
            if (em->pFunctSignalRx != NULL) {
                uint16_t errorCode;
                uint32_t infoCode;
                CO_memcpySwap2(&errorCode, &em->bufReadPtr[0]);
                CO_memcpySwap4(&infoCode, &em->bufReadPtr[4]);
                em->pFunctSignalRx(0,
                                   errorCode,
                                   em->bufReadPtr[2],
                                   em->bufReadPtr[3],
                                   infoCode);
            }
#endif

            /* copy data to CAN emergency message */
            memcpy(emPr->CANtxBuff->data, em->bufReadPtr, sizeof(emPr->CANtxBuff->data));
            memcpy(&preDEF, em->bufReadPtr, sizeof(preDEF));
            em->bufReadPtr += 8;

            /* Update read buffer pointer and reset inhibit timer */
            if(em->bufReadPtr == em->bufEnd){
                em->bufReadPtr = em->buf;
            }
            emPr->inhibitEmTimer = 0U;

            /* verify message buffer overflow, then clear full flag */
            if(em->bufFull == 2U){
                em->bufFull = 0U;    /* will be updated below */
                CO_errorReport(em, CO_EM_EMERGENCY_BUFFER_FULL, CO_EMC_GENERIC, 0U);
            }
            else{
                em->bufFull = 0;
                CO_errorReset(em, CO_EM_EMERGENCY_BUFFER_FULL, 0);
            }

            /* write to 'pre-defined error field' (object dictionary, index 0x1003) */
            if(emPr->preDefErr){
                uint8_t j;

                if(emPr->preDefErrNoOfErrors < emPr->preDefErrSize)
                    emPr->preDefErrNoOfErrors++;
                for(j=emPr->preDefErrNoOfErrors-1; j>0; j--)
                    emPr->preDefErr[j] = emPr->preDefErr[j-1];
                emPr->preDefErr[0] = preDEF;
            }

            /* send CAN message */
            CO_CANsend(emPr->CANdev, emPr->CANtxBuff);

        }
#if (CO_CONFIG_EM) & CO_CONFIG_FLAG_TIMERNEXT
        else if (timerNext_us != NULL) {
            uint32_t diff;
            /* check again after inhibit time elapsed */
            diff = emInhTime_us - emPr->inhibitEmTimer;
            if (*timerNext_us > diff) {
                *timerNext_us = diff;
            }
        }
#endif
    }

    return;
}


/******************************************************************************/
void CO_errorReport(CO_EM_t *em, const uint8_t errorBit, const uint16_t errorCode, const uint32_t infoCode){
    uint8_t index = errorBit >> 3;
    uint8_t bitmask = 1 << (errorBit & 0x7);
    uint8_t *errorStatusBits = 0;
    bool_t sendEmergency = true;

    if(em == NULL){
        sendEmergency = false;
    }
    else if(index >= em->errorStatusBitsSize){
        /* if errorBit value not supported, send emergency 'CO_EM_WRONG_ERROR_REPORT' */
        em->wrongErrorReport = errorBit;
        sendEmergency = false;
    }
    else{
        errorStatusBits = &em->errorStatusBits[index];
        /* if error was already reported, do nothing */
        if((*errorStatusBits & bitmask) != 0){
            sendEmergency = false;
        }
    }

    if(sendEmergency){
        /* set error bit */
        if(errorBit){
            /* any error except NO_ERROR */
            *errorStatusBits |= bitmask;
        }

        /* verify buffer full, set overflow */
        if(em->bufFull){
            em->bufFull = 2;
        }
        else{
            uint8_t bufCopy[8];

            /* prepare data for emergency message */
            CO_memcpySwap2(&bufCopy[0], &errorCode);
            bufCopy[2] = 0; /* error register will be set later */
            bufCopy[3] = errorBit;
            CO_memcpySwap4(&bufCopy[4], &infoCode);

            /* copy data to the buffer, increment writePtr and verify buffer full */
            CO_LOCK_EMCY();
            memcpy(em->bufWritePtr, bufCopy, sizeof(bufCopy));
            em->bufWritePtr += 8;

            if(em->bufWritePtr == em->bufEnd) em->bufWritePtr = em->buf;
            if(em->bufWritePtr == em->bufReadPtr) em->bufFull = 1;
            CO_UNLOCK_EMCY();

#if (CO_CONFIG_EM) & CO_CONFIG_FLAG_CALLBACK_PRE
            /* Optional signal to RTOS, which can resume task, which handles CO_EM_process */
            if(em->pFunctSignalPre != NULL) {
                em->pFunctSignalPre(em->functSignalObjectPre);
            }
#endif
        }
    }
}


/******************************************************************************/
void CO_errorReset(CO_EM_t *em, const uint8_t errorBit, const uint32_t infoCode){
    uint8_t index = errorBit >> 3;
    uint8_t bitmask = 1 << (errorBit & 0x7);
    uint8_t *errorStatusBits = 0;
    bool_t sendEmergency = true;

    if(em == NULL){
        sendEmergency = false;
    }
    else if(index >= em->errorStatusBitsSize){
        /* if errorBit value not supported, send emergency 'CO_EM_WRONG_ERROR_REPORT' */
        em->wrongErrorReport = errorBit;
        sendEmergency = false;
    }
    else{
        errorStatusBits = &em->errorStatusBits[index];
        /* if error was allready cleared, do nothing */
        if((*errorStatusBits & bitmask) == 0){
            sendEmergency = false;
        }
    }

    if(sendEmergency){
        /* erase error bit */
        *errorStatusBits &= ~bitmask;

        /* verify buffer full */
        if(em->bufFull){
            em->bufFull = 2;
        }
        else{
            uint8_t bufCopy[8];

            /* prepare data for emergency message */
            bufCopy[0] = 0;
            bufCopy[1] = 0;
            bufCopy[2] = 0; /* error register will be set later */
            bufCopy[3] = errorBit;
            CO_memcpySwap4(&bufCopy[4], &infoCode);

            /* copy data to the buffer, increment writePtr and verify buffer full */
            CO_LOCK_EMCY();
            memcpy(em->bufWritePtr, bufCopy, sizeof(bufCopy));
            em->bufWritePtr += 8;

            if(em->bufWritePtr == em->bufEnd) em->bufWritePtr = em->buf;
            if(em->bufWritePtr == em->bufReadPtr) em->bufFull = 1;
            CO_UNLOCK_EMCY();

#if (CO_CONFIG_EM) & CO_CONFIG_FLAG_CALLBACK_PRE
            /* Optional signal to RTOS, which can resume task, which handles CO_EM_process */
            if(em->pFunctSignalPre != NULL) {
                em->pFunctSignalPre(em->functSignalObjectPre);
            }
#endif
        }
    }
}


/******************************************************************************/
bool_t CO_isError(CO_EM_t *em, const uint8_t errorBit){
    uint8_t index = errorBit >> 3;
    uint8_t bitmask = 1 << (errorBit & 0x7);
    bool_t ret = false;

    if(em != NULL && index < em->errorStatusBitsSize){
        if((em->errorStatusBits[index] & bitmask) != 0){
            ret = true;
        }
    }

    return ret;
}
