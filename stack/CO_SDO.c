/*
 * CANopen Service Data Object - server.
 *
 * @file        CO_SDO.c
 * @ingroup     CO_SDO
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
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


#include "CO_driver.h"
#include "CO_SDO.h"
#include "crc16-ccitt.h"


/* Client command specifier, see DS301 */
#define CCS_DOWNLOAD_INITIATE          1U
#define CCS_DOWNLOAD_SEGMENT           0U
#define CCS_UPLOAD_INITIATE            2U
#define CCS_UPLOAD_SEGMENT             3U
#define CCS_DOWNLOAD_BLOCK             6U
#define CCS_UPLOAD_BLOCK               5U
#define CCS_ABORT                      0x80U


#if CO_SDO_BUFFER_SIZE < 7
    #error CO_SDO_BUFFER_SIZE must be greater than 7
#endif


/* Helper functions. **********************************************************/
void CO_memcpy(uint8_t dest[], const uint8_t src[], const uint16_t size){
    uint16_t i;
    for(i = 0; i < size; i++){
        dest[i] = src[i];
    }
}

uint16_t CO_getUint16(const uint8_t data[]){
    CO_bytes_t b;
    b.u8[0] = data[0];
    b.u8[1] = data[1];
    return b.u16[0];
}

uint32_t CO_getUint32(const uint8_t data[]){
    CO_bytes_t b;
    b.u8[0] = data[0];
    b.u8[1] = data[1];
    b.u8[2] = data[2];
    b.u8[3] = data[3];
    return b.u32[0];
}

void CO_setUint16(uint8_t data[], const uint16_t value){
    CO_bytes_t b;
    b.u16[0] = value;
    data[0] = b.u8[0];
    data[1] = b.u8[1];
}

void CO_setUint32(uint8_t data[], const uint32_t value){
    CO_bytes_t b;
    b.u32[0] = value;
    data[0] = b.u8[0];
    data[1] = b.u8[1];
    data[2] = b.u8[2];
    data[3] = b.u8[3];
}

#ifdef CO_LITTLE_ENDIAN
void CO_memcpySwap2(void* dest, const void* src){
    char *cdest;
    char *csrc;
    cdest = (char *) dest;
    csrc = (char *) src;
    cdest[0] = csrc[0];
    cdest[1] = csrc[1];
}
void CO_memcpySwap4(void* dest, const void* src){
    char *cdest;
    char *csrc;
    cdest = (char *) dest;
    csrc = (char *) src;
    cdest[0] = csrc[0];
    cdest[1] = csrc[1];
    cdest[2] = csrc[2];
    cdest[3] = csrc[3];
}
void CO_memcpySwap8(void* dest, const void* src){
    char *cdest;
    char *csrc;
    cdest = (char *) dest;
    csrc = (char *) src;
    cdest[0] = csrc[0];
    cdest[1] = csrc[1];
    cdest[2] = csrc[2];
    cdest[3] = csrc[3];
    cdest[4] = csrc[4];
    cdest[5] = csrc[5];
    cdest[6] = csrc[6];
    cdest[7] = csrc[7];
}
#endif
#ifdef CO_BIG_ENDIAN
void CO_memcpySwap2(void* dest, const void* src){
    char *cdest;
    char *csrc;
    cdest = (char *) dest;
    csrc = (char *) src;
    cdest[0] = csrc[1];
    cdest[1] = csrc[0];
}
void CO_memcpySwap4(void* dest, const void* src){
    char *cdest;
    char *csrc;
    cdest = (char *) dest;
    csrc = (char *) src;
    cdest[0] = csrc[3];
    cdest[1] = csrc[2];
    cdest[2] = csrc[1];
    cdest[3] = csrc[0];
}
void CO_memcpySwap8(void* dest, const void* src){
    char *cdest;
    char *csrc;
    cdest = (char *) dest;
    csrc = (char *) src;
    cdest[0] = csrc[7];
    cdest[1] = csrc[6];
    cdest[2] = csrc[5];
    cdest[3] = csrc[4];
    cdest[4] = csrc[3];
    cdest[5] = csrc[2];
    cdest[6] = csrc[1];
    cdest[7] = csrc[0];
}
#endif


/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_SDO_receive(void *object, const CO_CANrxMsg_t *msg);
static void CO_SDO_receive(void *object, const CO_CANrxMsg_t *msg){
    CO_SDO_t *SDO;

    SDO = (CO_SDO_t*)object;   /* this is the correct pointer type of the first argument */

    /* WARNING: When doing a SDO block upload and immediately after that
     * starting another SDO request, this request is dropped. Especially if
     * processing function has slow response.
     * See: https://github.com/CANopenNode/CANopenNode/issues/39 */

    /* verify message length and message overflow (previous message was not processed yet) */
    if((msg->DLC == 8U) && (!SDO->CANrxNew)){
        if(SDO->state != CO_SDO_ST_DOWNLOAD_BL_SUBBLOCK) {
            /* copy data and set 'new message' flag */
            SDO->CANrxData[0] = msg->data[0];
            SDO->CANrxData[1] = msg->data[1];
            SDO->CANrxData[2] = msg->data[2];
            SDO->CANrxData[3] = msg->data[3];
            SDO->CANrxData[4] = msg->data[4];
            SDO->CANrxData[5] = msg->data[5];
            SDO->CANrxData[6] = msg->data[6];
            SDO->CANrxData[7] = msg->data[7];

            SDO->CANrxNew = true;
        }
        else {
            /* block download, copy data directly */
            uint8_t seqno;

            SDO->CANrxData[0] = msg->data[0];
            seqno = SDO->CANrxData[0] & 0x7fU;
            SDO->timeoutTimer = 0;

            /* check correct sequence number. */
            if(seqno == (SDO->sequence + 1U)) {
                /* sequence is correct */
                uint8_t i;

                SDO->sequence++;

                /* copy data */
                for(i=1; i<8; i++) {
                    SDO->ODF_arg.data[SDO->bufferOffset++] = msg->data[i]; //SDO->ODF_arg.data is equal as SDO->databuffer
                    if(SDO->bufferOffset >= CO_SDO_BUFFER_SIZE) {
                        /* buffer full, break reception */
                        SDO->state = CO_SDO_ST_DOWNLOAD_BL_SUB_RESP;
                        SDO->CANrxNew = true;
                        break;
                    }
                }

                /* break reception if last segment or block sequence is too large */
                if(((SDO->CANrxData[0] & 0x80U) == 0x80U) || (SDO->sequence >= SDO->blksize)) {
                    SDO->state = CO_SDO_ST_DOWNLOAD_BL_SUB_RESP;
                    SDO->CANrxNew = true;
                }
            }
            else if((seqno == SDO->sequence) || (SDO->sequence == 0U)){
                /* Ignore message, if it is duplicate or if sequence didn't started yet. */
            }
            else {
                /* seqno is totally wrong, break reception. */
                SDO->state = CO_SDO_ST_DOWNLOAD_BL_SUB_RESP;
                SDO->CANrxNew = true;
            }
        }

        /* Optional signal to RTOS, which can resume task, which handles SDO server. */
        if(SDO->CANrxNew && SDO->pFunctSignal != NULL) {
            SDO->pFunctSignal();
        }
    }
}


/*
 * Function for accessing _SDO server parameter_ for default SDO (index 0x1200)
 * from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static CO_SDO_abortCode_t CO_ODF_1200(CO_ODF_arg_t *ODF_arg);
static CO_SDO_abortCode_t CO_ODF_1200(CO_ODF_arg_t *ODF_arg){
    uint8_t *nodeId;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    nodeId = (uint8_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    /* if SDO reading Object dictionary 0x1200, add nodeId to the value */
    if((ODF_arg->reading) && (ODF_arg->subIndex > 0U)){
        CO_setUint32(ODF_arg->data, value + *nodeId);
    }

    return ret;
}


/******************************************************************************/
CO_ReturnError_t CO_SDO_init(
        CO_SDO_t               *SDO,
        uint32_t                COB_IDClientToServer,
        uint32_t                COB_IDServerToClient,
        uint16_t                ObjDictIndex_SDOServerParameter,
        CO_SDO_t               *parentSDO,
        const CO_OD_entry_t     OD[],
        uint16_t                ODSize,
        CO_OD_extension_t      *ODExtensions,
        uint8_t                 nodeId,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx)
{
    /* verify arguments */
    if(SDO==NULL || CANdevRx==NULL || CANdevTx==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* configure own object dictionary */
    if(parentSDO == NULL){
        uint16_t i;

        SDO->ownOD = true;
        SDO->OD = OD;
        SDO->ODSize = ODSize;
        SDO->ODExtensions = ODExtensions;

        /* clear pointers in ODExtensions */
        for(i=0U; i<ODSize; i++){
            SDO->ODExtensions[i].pODFunc = NULL;
            SDO->ODExtensions[i].object = NULL;
            SDO->ODExtensions[i].flags = NULL;
        }
    }
    /* copy object dictionary from parent */
    else{
        SDO->ownOD = false;
        SDO->OD = parentSDO->OD;
        SDO->ODSize = parentSDO->ODSize;
        SDO->ODExtensions = parentSDO->ODExtensions;
    }

    /* Configure object variables */
    SDO->nodeId = nodeId;
    SDO->state = CO_SDO_ST_IDLE;
    SDO->CANrxNew = false;
    SDO->pFunctSignal = NULL;


    /* Configure Object dictionary entry at index 0x1200 */
    if(ObjDictIndex_SDOServerParameter == OD_H1200_SDO_SERVER_PARAM){
        CO_OD_configure(SDO, ObjDictIndex_SDOServerParameter, CO_ODF_1200, (void*)&SDO->nodeId, 0U, 0U);
    }

    if((COB_IDClientToServer & 0x80000000) != 0 || (COB_IDServerToClient & 0x80000000) != 0 ){
        // SDO is invalid
        COB_IDClientToServer = 0;
        COB_IDServerToClient = 0;
    }
    /* configure SDO server CAN reception */
    CO_CANrxBufferInit(
            CANdevRx,               /* CAN device */
            CANdevRxIdx,            /* rx buffer index */
            COB_IDClientToServer,   /* CAN identifier */
            0x7FF,                  /* mask */
            0,                      /* rtr */
            (void*)SDO,             /* object passed to receive function */
            CO_SDO_receive);        /* this function will process received message */

    /* configure SDO server CAN transmission */
    SDO->CANdevTx = CANdevTx;
    SDO->CANtxBuff = CO_CANtxBufferInit(
            CANdevTx,               /* CAN device */
            CANdevTxIdx,            /* index of specific buffer inside CAN module */
            COB_IDServerToClient,   /* CAN identifier */
            0,                      /* rtr */
            8,                      /* number of data bytes */
            0);                     /* synchronous message flag bit */

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_SDO_initCallback(
        CO_SDO_t               *SDO,
        void                  (*pFunctSignal)(void))
{
    if(SDO != NULL){
        SDO->pFunctSignal = pFunctSignal;
    }
}


/******************************************************************************/
void CO_OD_configure(
        CO_SDO_t               *SDO,
        uint16_t                index,
        CO_SDO_abortCode_t    (*pODFunc)(CO_ODF_arg_t *ODF_arg),
        void                   *object,
        uint8_t                *flags,
        uint8_t                 flagsSize)
{
    uint16_t entryNo;

    entryNo = CO_OD_find(SDO, index);
    if(entryNo < 0xFFFFU){
        CO_OD_extension_t *ext = &SDO->ODExtensions[entryNo];
        uint8_t maxSubIndex = SDO->OD[entryNo].maxSubIndex;

        ext->pODFunc = pODFunc;
        ext->object = object;
        if((flags != NULL) && (flagsSize != 0U) && (flagsSize == maxSubIndex)){
            uint16_t i;
            ext->flags = flags;
            for(i=0U; i<=maxSubIndex; i++){
                ext->flags[i] = 0U;
            }
        }
        else{
            ext->flags = NULL;
        }
    }
}


/******************************************************************************/
uint16_t CO_OD_find(CO_SDO_t *SDO, uint16_t index){
    /* Fast search in ordered Object Dictionary. If indexes are mixed, this won't work. */
    /* If Object Dictionary has up to 2^N entries, then N is max number of loop passes. */
    uint16_t cur, min, max;
    const CO_OD_entry_t* object;

    min = 0U;
    max = SDO->ODSize - 1U;
    while(min < max){
        cur = (min + max) / 2;
        object = &SDO->OD[cur];
        /* Is object matched */
        if(index == object->index){
            return cur;
        }
        if(index < object->index){
            max = cur;
            if(max) max--;
        }
        else
            min = cur + 1U;
    }

    if(min == max){
        object = &SDO->OD[min];
        /* Is object matched */
        if(index == object->index){
            return min;
        }
    }

    return 0xFFFFU;  /* object does not exist in OD */
}


/******************************************************************************/
uint16_t CO_OD_getLength(CO_SDO_t *SDO, uint16_t entryNo, uint8_t subIndex){
    const CO_OD_entry_t* object = &SDO->OD[entryNo];

    if(entryNo == 0xFFFFU){
        return 0U;
    }

    if(object->maxSubIndex == 0U){    /* Object type is Var */
        if(object->pData == 0){ /* data type is domain */
            return CO_SDO_BUFFER_SIZE;
        }
        else{
            return object->length;
        }
    }
    else if(object->attribute != 0U){ /* Object type is Array */
        if(subIndex == 0U){
            return 1U;
        }
        else if(object->pData == 0){
            /* data type is domain */
            return CO_SDO_BUFFER_SIZE;
        }
        else{
            return object->length;
        }
    }
    else{                            /* Object type is Record */
        if(((const CO_OD_entryRecord_t*)(object->pData))[subIndex].pData == 0){
            /* data type is domain */
            return CO_SDO_BUFFER_SIZE;
        }
        else{
            return ((const CO_OD_entryRecord_t*)(object->pData))[subIndex].length;
        }
    }
}


/******************************************************************************/
uint16_t CO_OD_getAttribute(CO_SDO_t *SDO, uint16_t entryNo, uint8_t subIndex){
    const CO_OD_entry_t* object = &SDO->OD[entryNo];

    if(entryNo == 0xFFFFU){
        return 0U;
    }

    if(object->maxSubIndex == 0U){   /* Object type is Var */
        return object->attribute;
    }
    else if(object->attribute != 0U){/* Object type is Array */
        bool_t exception_1003 = false;
        uint16_t attr = object->attribute;

        /* Special exception: Object 1003,00 should be writable */
        if(object->index == 0x1003 && subIndex == 0) {
            exception_1003 = true;
            attr |= CO_ODA_WRITEABLE;
        }

        if(subIndex == 0U  && exception_1003 == false){
            /* First subIndex is readonly */
            attr &= ~(CO_ODA_WRITEABLE | CO_ODA_RPDO_MAPABLE);
            attr |= CO_ODA_READABLE;
        }
        return attr;
    }
    else{                            /* Object type is Record */
        return ((const CO_OD_entryRecord_t*)(object->pData))[subIndex].attribute;
    }
}


/******************************************************************************/
void* CO_OD_getDataPointer(CO_SDO_t *SDO, uint16_t entryNo, uint8_t subIndex){
    const CO_OD_entry_t* object = &SDO->OD[entryNo];

    if(entryNo == 0xFFFFU){
        return 0;
    }

    if(object->maxSubIndex == 0U){   /* Object type is Var */
        return object->pData;
    }
    else if(object->attribute != 0U){/* Object type is Array */
        if(subIndex==0){
            /* this is the data, for the subIndex 0 in the array */
            return (void*) &object->maxSubIndex;
        }
        else if(object->pData == 0){
            /* data type is domain */
            return 0;
        }
        else{
            return (void*)(((int8_t*)object->pData) + ((subIndex-1) * object->length));
        }
    }
    else{                            /* Object Type is Record */
        return ((const CO_OD_entryRecord_t*)(object->pData))[subIndex].pData;
    }
}


/******************************************************************************/
uint8_t* CO_OD_getFlagsPointer(CO_SDO_t *SDO, uint16_t entryNo, uint8_t subIndex){
    CO_OD_extension_t* ext;

    if((entryNo == 0xFFFFU) || (SDO->ODExtensions == 0)){
        return 0;
    }

    ext = &SDO->ODExtensions[entryNo];

    return &ext->flags[subIndex];
}


/******************************************************************************/
uint32_t CO_SDO_initTransfer(CO_SDO_t *SDO, uint16_t index, uint8_t subIndex){

    SDO->ODF_arg.index = index;
    SDO->ODF_arg.subIndex = subIndex;

    /* find object in Object Dictionary */
    SDO->entryNo = CO_OD_find(SDO, index);
    if(SDO->entryNo == 0xFFFFU){
        return CO_SDO_AB_NOT_EXIST ;     /* object does not exist in OD */
    }

    /* verify existance of subIndex */
    if(subIndex > SDO->OD[SDO->entryNo].maxSubIndex &&
            SDO->OD[SDO->entryNo].pData != NULL)
    {
        return CO_SDO_AB_SUB_UNKNOWN;     /* Sub-index does not exist. */
    }

    /* pointer to data in Object dictionary */
    SDO->ODF_arg.ODdataStorage = CO_OD_getDataPointer(SDO, SDO->entryNo, subIndex);

    /* fill ODF_arg */
    SDO->ODF_arg.object = NULL;
    if(SDO->ODExtensions){
        CO_OD_extension_t *ext = &SDO->ODExtensions[SDO->entryNo];
        SDO->ODF_arg.object = ext->object;
    }
    SDO->ODF_arg.data = SDO->databuffer;
    SDO->ODF_arg.dataLength = CO_OD_getLength(SDO, SDO->entryNo, subIndex);
    SDO->ODF_arg.attribute = CO_OD_getAttribute(SDO, SDO->entryNo, subIndex);
    SDO->ODF_arg.pFlags = CO_OD_getFlagsPointer(SDO, SDO->entryNo, subIndex);

    SDO->ODF_arg.firstSegment = true;
    SDO->ODF_arg.lastSegment = true;

    /* indicate total data length, if not domain */
    SDO->ODF_arg.dataLengthTotal = (SDO->ODF_arg.ODdataStorage) ? SDO->ODF_arg.dataLength : 0U;

    SDO->ODF_arg.offset = 0U;

    /* verify length */
    if(SDO->ODF_arg.dataLength > CO_SDO_BUFFER_SIZE){
        return CO_SDO_AB_DEVICE_INCOMPAT;     /* general internal incompatibility in the device */
    }

    return 0U;
}


/******************************************************************************/
uint32_t CO_SDO_readOD(CO_SDO_t *SDO, uint16_t SDOBufferSize){
    uint8_t *SDObuffer = SDO->ODF_arg.data;
    uint8_t *ODdata = (uint8_t*)SDO->ODF_arg.ODdataStorage;
    uint16_t length = SDO->ODF_arg.dataLength;
    CO_OD_extension_t *ext = 0;

    /* is object readable? */
    if((SDO->ODF_arg.attribute & CO_ODA_READABLE) == 0)
        return CO_SDO_AB_WRITEONLY;     /* attempt to read a write-only object */

    /* find extension */
    if(SDO->ODExtensions != NULL){
        ext = &SDO->ODExtensions[SDO->entryNo];
    }

    /* copy data from OD to SDO buffer if not domain */
    if(ODdata != NULL){
        CO_LOCK_OD();
        while(length--) *(SDObuffer++) = *(ODdata++);
        CO_UNLOCK_OD();
    }
    /* if domain, Object dictionary function MUST exist */
    else{
        if(ext->pODFunc == NULL){
            return CO_SDO_AB_DEVICE_INCOMPAT;     /* general internal incompatibility in the device */
        }
    }

    /* call Object dictionary function if registered */
    SDO->ODF_arg.reading = true;
    if(ext->pODFunc != NULL){
        uint32_t abortCode = ext->pODFunc(&SDO->ODF_arg);
        if(abortCode != 0U){
            return abortCode;
        }

        /* dataLength (upadted by pODFunc) must be inside limits */
        if((SDO->ODF_arg.dataLength == 0U) || (SDO->ODF_arg.dataLength > SDOBufferSize)){
            return CO_SDO_AB_DEVICE_INCOMPAT;     /* general internal incompatibility in the device */
        }
    }
    SDO->ODF_arg.offset += SDO->ODF_arg.dataLength;
    SDO->ODF_arg.firstSegment = false;

    /* swap data if processor is not little endian (CANopen is) */
#ifdef CO_BIG_ENDIAN
    if((SDO->ODF_arg.attribute & CO_ODA_MB_VALUE) != 0){
        uint16_t len = SDO->ODF_arg.dataLength;
        uint8_t *buf1 = SDO->ODF_arg.data;
        uint8_t *buf2 = buf1 + len - 1;

        len /= 2;
        while(len--){
            uint8_t b = *buf1;
            *(buf1++) = *buf2;
            *(buf2--) = b;
        }
    }
#endif

    return 0U;
}


/******************************************************************************/
uint32_t CO_SDO_writeOD(CO_SDO_t *SDO, uint16_t length){
    uint8_t *SDObuffer = SDO->ODF_arg.data;
    uint8_t *ODdata = (uint8_t*)SDO->ODF_arg.ODdataStorage;
    bool_t exception_1003 = false;

    /* is object writeable? */
    if((SDO->ODF_arg.attribute & CO_ODA_WRITEABLE) == 0){
        return CO_SDO_AB_READONLY;     /* attempt to write a read-only object */
    }

    /* length of domain data is application specific and not verified */
    if(ODdata == 0){
        SDO->ODF_arg.dataLength = length;
    }

    /* verify length except for domain data type */
    else if(SDO->ODF_arg.dataLength != length){
        return CO_SDO_AB_TYPE_MISMATCH;     /* Length of service parameter does not match */
    }

    /* swap data if processor is not little endian (CANopen is) */
#ifdef CO_BIG_ENDIAN
    if((SDO->ODF_arg.attribute & CO_ODA_MB_VALUE) != 0){
        uint16_t len = SDO->ODF_arg.dataLength;
        uint8_t *buf1 = SDO->ODF_arg.data;
        uint8_t *buf2 = buf1 + len - 1;

        len /= 2;
        while(len--){
            uint8_t b = *buf1;
            *(buf1++) = *buf2;
            *(buf2--) = b;
        }
    }
#endif

    /* call Object dictionary function if registered */
    SDO->ODF_arg.reading = false;
    if(SDO->ODExtensions != NULL){
        CO_OD_extension_t *ext = &SDO->ODExtensions[SDO->entryNo];

        if(ext->pODFunc != NULL){
            uint32_t abortCode = ext->pODFunc(&SDO->ODF_arg);
            if(abortCode != 0U){
                return abortCode;
            }
        }
    }
    SDO->ODF_arg.offset += SDO->ODF_arg.dataLength;
    SDO->ODF_arg.firstSegment = false;

    /* Special exception: 1003,00 is writable from network, but not in OD  */
    if(SDO->ODF_arg.index == 0x1003 && SDO->ODF_arg.subIndex == 0) {
        exception_1003 = true;
    }

    /* copy data from SDO buffer to OD if not domain */
    if(ODdata != NULL && exception_1003 == false){
        CO_LOCK_OD();
        while(length--){
            *(ODdata++) = *(SDObuffer++);
        }
        CO_UNLOCK_OD();
    }

    return 0;
}


/******************************************************************************/
static void CO_SDO_abort(CO_SDO_t *SDO, uint32_t code){
    SDO->CANtxBuff->data[0] = 0x80;
    SDO->CANtxBuff->data[1] = SDO->ODF_arg.index & 0xFF;
    SDO->CANtxBuff->data[2] = (SDO->ODF_arg.index>>8) & 0xFF;
    SDO->CANtxBuff->data[3] = SDO->ODF_arg.subIndex;
    CO_memcpySwap4(&SDO->CANtxBuff->data[4], &code);
    SDO->state = CO_SDO_ST_IDLE;
    SDO->CANrxNew = false;
    CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
}


/******************************************************************************/
int8_t CO_SDO_process(
        CO_SDO_t               *SDO,
        bool_t                  NMTisPreOrOperational,
        uint16_t                timeDifference_ms,
        uint16_t                SDOtimeoutTime,
        uint16_t               *timerNext_ms)
{
    CO_SDO_state_t state = CO_SDO_ST_IDLE;
    bool_t timeoutSubblockDownolad = false;
    bool_t sendResponse = false;

    /* return if idle */
    if((SDO->state == CO_SDO_ST_IDLE) && (!SDO->CANrxNew)){
        return 0;
    }

    /* SDO is allowed to work only in operational or pre-operational NMT state */
    if(!NMTisPreOrOperational){
        SDO->state = CO_SDO_ST_IDLE;
        SDO->CANrxNew = false;
        return 0;
    }

    /* Is something new to process? */
    if((!SDO->CANtxBuff->bufferFull) && ((SDO->CANrxNew) || (SDO->state == CO_SDO_ST_UPLOAD_BL_SUBBLOCK))){
        uint8_t CCS = SDO->CANrxData[0] >> 5;   /* Client command specifier */

        /* reset timeout */
        if(SDO->state != CO_SDO_ST_UPLOAD_BL_SUBBLOCK)
            SDO->timeoutTimer = 0;

        /* clear response buffer */
        SDO->CANtxBuff->data[0] = SDO->CANtxBuff->data[1] = SDO->CANtxBuff->data[2] = SDO->CANtxBuff->data[3] = 0;
        SDO->CANtxBuff->data[4] = SDO->CANtxBuff->data[5] = SDO->CANtxBuff->data[6] = SDO->CANtxBuff->data[7] = 0;

        /* Is abort from client? */
        if((SDO->CANrxNew) && (SDO->CANrxData[0] == CCS_ABORT)){
            SDO->state = CO_SDO_ST_IDLE;
            SDO->CANrxNew = false;
            return -1;
        }

        /* continue with previous SDO communication or start new */
        if(SDO->state != CO_SDO_ST_IDLE){
            state = SDO->state;
        }
        else{
            uint32_t abortCode;
            uint16_t index;

            /* Is client command specifier valid */
            if((CCS != CCS_DOWNLOAD_INITIATE) && (CCS != CCS_UPLOAD_INITIATE) &&
                (CCS != CCS_DOWNLOAD_BLOCK) && (CCS != CCS_UPLOAD_BLOCK)){
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* init ODF_arg */
            index = SDO->CANrxData[2];
            index = index << 8 | SDO->CANrxData[1];
            abortCode = CO_SDO_initTransfer(SDO, index, SDO->CANrxData[3]);
            if(abortCode != 0U){
                CO_SDO_abort(SDO, abortCode);
                return -1;
            }

            /* download */
            if((CCS == CCS_DOWNLOAD_INITIATE) || (CCS == CCS_DOWNLOAD_BLOCK)){
                if((SDO->ODF_arg.attribute & CO_ODA_WRITEABLE) == 0U){
                    CO_SDO_abort(SDO, CO_SDO_AB_READONLY); /* attempt to write a read-only object */
                    return -1;
                }

                /* set state machine to normal or block download */
                if(CCS == CCS_DOWNLOAD_INITIATE){
                    state = CO_SDO_ST_DOWNLOAD_INITIATE;
                }
                else{
                    state = CO_SDO_ST_DOWNLOAD_BL_INITIATE;
                }
            }

            /* upload */
            else{
                abortCode = CO_SDO_readOD(SDO, CO_SDO_BUFFER_SIZE);
                if(abortCode != 0U){
                    CO_SDO_abort(SDO, abortCode);
                    return -1;
                }

                /* if data size is large enough set state machine to block upload, otherwise set to normal transfer */
                if((CCS == CCS_UPLOAD_BLOCK) && (SDO->ODF_arg.dataLength > SDO->CANrxData[5])){
                    state = CO_SDO_ST_UPLOAD_BL_INITIATE;
                }
                else{
                    state = CO_SDO_ST_UPLOAD_INITIATE;
                }
            }
        }
    }

    /* verify SDO timeout */
    if(SDO->timeoutTimer < SDOtimeoutTime){
        SDO->timeoutTimer += timeDifference_ms;
    }
    if(SDO->timeoutTimer >= SDOtimeoutTime){
        if((SDO->state == CO_SDO_ST_DOWNLOAD_BL_SUBBLOCK) && (SDO->sequence != 0) && (!SDO->CANtxBuff->bufferFull)){
            timeoutSubblockDownolad = true;
            state = CO_SDO_ST_DOWNLOAD_BL_SUB_RESP;
        }
        else{
            CO_SDO_abort(SDO, CO_SDO_AB_TIMEOUT); /* SDO protocol timed out */
            return -1;
        }
    }

    /* return immediately if still idle */
    if(state == CO_SDO_ST_IDLE){
        return 0;
    }

    /* state machine (buffer is freed (SDO->CANrxNew = 0;) at the end) */
    switch(state){
        uint32_t abortCode;
        uint16_t len, i;
        bool_t lastSegmentInSubblock;

        case CO_SDO_ST_DOWNLOAD_INITIATE:{
            /* default response */
            SDO->CANtxBuff->data[0] = 0x60;
            SDO->CANtxBuff->data[1] = SDO->CANrxData[1];
            SDO->CANtxBuff->data[2] = SDO->CANrxData[2];
            SDO->CANtxBuff->data[3] = SDO->CANrxData[3];

            /* Expedited transfer */
            if((SDO->CANrxData[0] & 0x02U) != 0U){
                /* is size indicated? Get message length */
                if((SDO->CANrxData[0] & 0x01U) != 0U){
                    len = 4U - ((SDO->CANrxData[0] >> 2U) & 0x03U);
                }
                else{
                    len = SDO->ODF_arg.dataLength;
                }

                /* copy data to SDO buffer */
                SDO->ODF_arg.data[0] = SDO->CANrxData[4];
                SDO->ODF_arg.data[1] = SDO->CANrxData[5];
                SDO->ODF_arg.data[2] = SDO->CANrxData[6];
                SDO->ODF_arg.data[3] = SDO->CANrxData[7];

                /* write data to the Object dictionary */
                abortCode = CO_SDO_writeOD(SDO, len);
                if(abortCode != 0U){
                    CO_SDO_abort(SDO, abortCode);
                    return -1;
                }

                /* finish the communication */
                SDO->state = CO_SDO_ST_IDLE;
                sendResponse = true;
            }

            /* Segmented transfer */
            else{
                /* verify length if size is indicated */
                if((SDO->CANrxData[0]&0x01) != 0){
                    uint32_t lenRx;
                    CO_memcpySwap4(&lenRx, &SDO->CANrxData[4]);
                    SDO->ODF_arg.dataLengthTotal = lenRx;

                    /* verify length except for domain data type */
                    if((lenRx != SDO->ODF_arg.dataLength) && (SDO->ODF_arg.ODdataStorage != 0)){
                        CO_SDO_abort(SDO, CO_SDO_AB_TYPE_MISMATCH);  /* Length of service parameter does not match */
                        return -1;
                    }
                }
                SDO->bufferOffset = 0;
                SDO->sequence = 0;
                SDO->state = CO_SDO_ST_DOWNLOAD_SEGMENTED;
                sendResponse = true;
            }
            break;
        }

        case CO_SDO_ST_DOWNLOAD_SEGMENTED:{
            /* verify client command specifier */
            if((SDO->CANrxData[0]&0xE0) != 0x00U){
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* verify toggle bit */
            i = (SDO->CANrxData[0]&0x10U) ? 1U : 0U;
            if(i != SDO->sequence){
                CO_SDO_abort(SDO, CO_SDO_AB_TOGGLE_BIT);/* toggle bit not alternated */
                return -1;
            }

            /* get size of data in message */
            len = 7U - ((SDO->CANrxData[0] >> 1U) & 0x07U);

            /* verify length. Domain data type enables length larger than SDO buffer size */
            if((SDO->bufferOffset + len) > SDO->ODF_arg.dataLength){
                if(SDO->ODF_arg.ODdataStorage != 0){
                    CO_SDO_abort(SDO, CO_SDO_AB_DATA_LONG);  /* Length of service parameter too high */
                    return -1;
                }
                else{
                    /* empty buffer in domain data type */
                    SDO->ODF_arg.lastSegment = false;
                    abortCode = CO_SDO_writeOD(SDO, SDO->bufferOffset);
                    if(abortCode != 0U){
                        CO_SDO_abort(SDO, abortCode);
                        return -1;
                    }

                    SDO->ODF_arg.dataLength = CO_SDO_BUFFER_SIZE;
                    SDO->bufferOffset = 0;
                }
            }

            /* copy data to buffer */
            for(i=0U; i<len; i++)
                SDO->ODF_arg.data[SDO->bufferOffset++] = SDO->CANrxData[i+1];

            /* If no more segments to be downloaded, write data to the Object dictionary */
            if((SDO->CANrxData[0] & 0x01U) != 0U){
                SDO->ODF_arg.lastSegment = true;
                abortCode = CO_SDO_writeOD(SDO, SDO->bufferOffset);
                if(abortCode != 0U){
                    CO_SDO_abort(SDO, abortCode);
                    return -1;
                }

                /* finish */
                SDO->state = CO_SDO_ST_IDLE;
            }

            /* download segment response and alternate toggle bit */
            SDO->CANtxBuff->data[0] = 0x20 | (SDO->sequence ? 0x10 : 0x00);
            SDO->sequence = (SDO->sequence) ? 0 : 1;
            sendResponse = true;
            break;
        }

        case CO_SDO_ST_DOWNLOAD_BL_INITIATE:{
            /* verify client command specifier and subcommand */
            if((SDO->CANrxData[0]&0xE1U) != 0xC0U){
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* prepare response */
            SDO->CANtxBuff->data[0] = 0xA4;
            SDO->CANtxBuff->data[1] = SDO->CANrxData[1];
            SDO->CANtxBuff->data[2] = SDO->CANrxData[2];
            SDO->CANtxBuff->data[3] = SDO->CANrxData[3];

            /* blksize */
            SDO->blksize = (CO_SDO_BUFFER_SIZE > (7*127)) ? 127 : (CO_SDO_BUFFER_SIZE / 7);
            SDO->CANtxBuff->data[4] = SDO->blksize;

            /* is CRC enabled */
            SDO->crcEnabled = (SDO->CANrxData[0] & 0x04) ? true : false;
            SDO->crc = 0;

            /* verify length if size is indicated */
            if((SDO->CANrxData[0]&0x02) != 0U){
                uint32_t lenRx;
                CO_memcpySwap4(&lenRx, &SDO->CANrxData[4]);
                SDO->ODF_arg.dataLengthTotal = lenRx;

                /* verify length except for domain data type */
                if((lenRx != SDO->ODF_arg.dataLength) && (SDO->ODF_arg.ODdataStorage != 0)){
                    CO_SDO_abort(SDO, CO_SDO_AB_TYPE_MISMATCH);  /* Length of service parameter does not match */
                    return -1;
                }
            }

            SDO->bufferOffset = 0;
            SDO->sequence = 0;
            SDO->state = CO_SDO_ST_DOWNLOAD_BL_SUBBLOCK;

            /* send response */
            sendResponse = true;
            break;
        }

        case CO_SDO_ST_DOWNLOAD_BL_SUBBLOCK:{
            /* data are copied directly in receive function */
            break;
        }

        case CO_SDO_ST_DOWNLOAD_BL_SUB_RESP:{
            /* no new message received, SDO timeout occured, try to response */
            lastSegmentInSubblock = (!timeoutSubblockDownolad &&
                        ((SDO->CANrxData[0] & 0x80U) == 0x80U)) ? true : false;

            /* prepare response */
            SDO->CANtxBuff->data[0] = 0xA2;
            SDO->CANtxBuff->data[1] = SDO->sequence;
            SDO->sequence = 0;

            /* empty buffer in domain data type if not last segment */
            if((SDO->ODF_arg.ODdataStorage == 0) && (SDO->bufferOffset != 0) && !lastSegmentInSubblock){
                /* calculate CRC on next bytes, if enabled */
                if(SDO->crcEnabled){
                    SDO->crc = crc16_ccitt(SDO->ODF_arg.data, SDO->bufferOffset, SDO->crc);
                }

                /* write data to the Object dictionary */
                SDO->ODF_arg.lastSegment = false;
                abortCode = CO_SDO_writeOD(SDO, SDO->bufferOffset);
                if(abortCode != 0U){
                    CO_SDO_abort(SDO, abortCode);
                    return -1;
                }

                SDO->ODF_arg.dataLength = CO_SDO_BUFFER_SIZE;
                SDO->bufferOffset = 0;
            }

            /* blksize */
            len = CO_SDO_BUFFER_SIZE - SDO->bufferOffset;
            SDO->blksize = (len > (7*127)) ? 127 : (len / 7);
            SDO->CANtxBuff->data[2] = SDO->blksize;

            /* set next state */
            if(lastSegmentInSubblock) {
                SDO->state = CO_SDO_ST_DOWNLOAD_BL_END;
            }
            else if(SDO->bufferOffset >= CO_SDO_BUFFER_SIZE) {
                CO_SDO_abort(SDO, CO_SDO_AB_DEVICE_INCOMPAT);
                return -1;
            }
            else {
                SDO->state = CO_SDO_ST_DOWNLOAD_BL_SUBBLOCK;
            }

            /* send response */
            sendResponse = true;

            break;
        }

        case CO_SDO_ST_DOWNLOAD_BL_END:{
            /* verify client command specifier and subcommand */
            if((SDO->CANrxData[0]&0xE1U) != 0xC1U){
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* number of bytes in the last segment of the last block that do not contain data. */
            len = (SDO->CANrxData[0]>>2U) & 0x07U;
            SDO->bufferOffset -= len;

            /* calculate and verify CRC, if enabled */
            if(SDO->crcEnabled){
                uint16_t crc;
                SDO->crc = crc16_ccitt(SDO->ODF_arg.data, SDO->bufferOffset, SDO->crc);

                CO_memcpySwap2(&crc, &SDO->CANrxData[1]);

                if(SDO->crc != crc){
                    CO_SDO_abort(SDO, CO_SDO_AB_CRC);   /* CRC error (block mode only). */
                    return -1;
                }
            }

            /* write data to the Object dictionary */
            SDO->ODF_arg.lastSegment = true;
            abortCode = CO_SDO_writeOD(SDO, SDO->bufferOffset);
            if(abortCode != 0U){
                CO_SDO_abort(SDO, abortCode);
                return -1;
            }

            /* send response */
            SDO->CANtxBuff->data[0] = 0xA1;
            SDO->state = CO_SDO_ST_IDLE;
            sendResponse = true;
            break;
        }

        case CO_SDO_ST_UPLOAD_INITIATE:{
            /* default response */
            SDO->CANtxBuff->data[1] = SDO->CANrxData[1];
            SDO->CANtxBuff->data[2] = SDO->CANrxData[2];
            SDO->CANtxBuff->data[3] = SDO->CANrxData[3];

            /* Expedited transfer */
            if(SDO->ODF_arg.dataLength <= 4U){
                for(i=0U; i<SDO->ODF_arg.dataLength; i++)
                    SDO->CANtxBuff->data[4U+i] = SDO->ODF_arg.data[i];

                SDO->CANtxBuff->data[0] = 0x43U | ((4U-SDO->ODF_arg.dataLength) << 2U);
                SDO->state = CO_SDO_ST_IDLE;

                sendResponse = true;
            }

            /* Segmented transfer */
            else{
                SDO->bufferOffset = 0U;
                SDO->sequence = 0U;
                SDO->state = CO_SDO_ST_UPLOAD_SEGMENTED;

                /* indicate data size, if known */
                if(SDO->ODF_arg.dataLengthTotal != 0U){
                    uint32_t len = SDO->ODF_arg.dataLengthTotal;
                    CO_memcpySwap4(&SDO->CANtxBuff->data[4], &len);
                    SDO->CANtxBuff->data[0] = 0x41U;
                }
                else{
                    SDO->CANtxBuff->data[0] = 0x40U;
                }

                /* send response */
                sendResponse = true;
            }
            break;
        }

        case CO_SDO_ST_UPLOAD_SEGMENTED:{
            /* verify client command specifier */
            if((SDO->CANrxData[0]&0xE0U) != 0x60U){
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* verify toggle bit */
            i = ((SDO->CANrxData[0]&0x10U) != 0) ? 1U : 0U;
            if(i != SDO->sequence){
                CO_SDO_abort(SDO, CO_SDO_AB_TOGGLE_BIT);/* toggle bit not alternated */
                return -1;
            }

            /* calculate length to be sent */
            len = SDO->ODF_arg.dataLength - SDO->bufferOffset;
            if(len > 7U) len = 7U;

            /* If data type is domain, re-fill the data buffer if neccessary and indicated so. */
            if((SDO->ODF_arg.ODdataStorage == 0) && (len < 7U) && (!SDO->ODF_arg.lastSegment)){
                /* copy previous data to the beginning */
                for(i=0U; i<len; i++){
                    SDO->ODF_arg.data[i] = SDO->ODF_arg.data[SDO->bufferOffset+i];
                }

                /* move the beginning of the data buffer */
                SDO->ODF_arg.data += len;
                SDO->ODF_arg.dataLength = CO_OD_getLength(SDO, SDO->entryNo, SDO->ODF_arg.subIndex) - len;

                /* read next data from Object dictionary function */
                abortCode = CO_SDO_readOD(SDO, CO_SDO_BUFFER_SIZE);
                if(abortCode != 0U){
                    CO_SDO_abort(SDO, abortCode);
                    return -1;
                }

                /* return to the original data buffer */
                SDO->ODF_arg.data -= len;
                SDO->ODF_arg.dataLength +=  len;
                SDO->bufferOffset = 0;

                /* re-calculate the length */
                len = SDO->ODF_arg.dataLength;
                if(len > 7U) len = 7U;
            }

            /* fill response data bytes */
            for(i=0U; i<len; i++)
                SDO->CANtxBuff->data[i+1] = SDO->ODF_arg.data[SDO->bufferOffset++];

            /* first response byte */
            SDO->CANtxBuff->data[0] = 0x00 | (SDO->sequence ? 0x10 : 0x00) | ((7-len)<<1);
            SDO->sequence = (SDO->sequence) ? 0 : 1;

            /* verify end of transfer */
            if((SDO->bufferOffset == SDO->ODF_arg.dataLength) && (SDO->ODF_arg.lastSegment)){
                SDO->CANtxBuff->data[0] |= 0x01;
                SDO->state = CO_SDO_ST_IDLE;
            }

            /* send response */
            sendResponse = true;
            break;
        }

        case CO_SDO_ST_UPLOAD_BL_INITIATE:{
            /* default response */
            SDO->CANtxBuff->data[1] = SDO->CANrxData[1];
            SDO->CANtxBuff->data[2] = SDO->CANrxData[2];
            SDO->CANtxBuff->data[3] = SDO->CANrxData[3];

            /* calculate CRC, if enabled */
            if((SDO->CANrxData[0] & 0x04U) != 0U){
                SDO->crcEnabled = true;
                SDO->crc = crc16_ccitt(SDO->ODF_arg.data, SDO->ODF_arg.dataLength, 0);
            }
            else{
                SDO->crcEnabled = false;
                SDO->crc = 0;
            }

            /* Number of segments per block */
            SDO->blksize = SDO->CANrxData[4];

            /* verify client subcommand */
            if((SDO->CANrxData[0]&0x03U) != 0x00U){
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            /* verify blksize and if SDO data buffer is large enough */
            if((SDO->blksize < 1U) || (SDO->blksize > 127U) ||
               (((SDO->blksize*7U) > SDO->ODF_arg.dataLength) && (!SDO->ODF_arg.lastSegment))){
                CO_SDO_abort(SDO, CO_SDO_AB_BLOCK_SIZE); /* Invalid block size (block mode only). */
                return -1;
            }

            /* indicate data size, if known */
            if(SDO->ODF_arg.dataLengthTotal != 0U){
                uint32_t len = SDO->ODF_arg.dataLengthTotal;
                CO_memcpySwap4(&SDO->CANtxBuff->data[4], &len);
                SDO->CANtxBuff->data[0] = 0xC6U;
            }
            else{
                SDO->CANtxBuff->data[0] = 0xC4U;
            }

            /* send response */
            SDO->state = CO_SDO_ST_UPLOAD_BL_INITIATE_2;
            sendResponse = true;
            break;
        }

        case CO_SDO_ST_UPLOAD_BL_INITIATE_2:{
            /* verify client command specifier and subcommand */
            if((SDO->CANrxData[0]&0xE3U) != 0xA3U){
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            SDO->bufferOffset = 0;
            SDO->sequence = 0;
            SDO->endOfTransfer = false;
            SDO->CANrxNew = false;
            SDO->state = CO_SDO_ST_UPLOAD_BL_SUBBLOCK;
            /* continue in next case */
        }

        case CO_SDO_ST_UPLOAD_BL_SUBBLOCK:{
            /* is block confirmation received */
            if(SDO->CANrxNew){
                uint8_t ackseq;
                uint16_t j;

                /* verify client command specifier and subcommand */
                if((SDO->CANrxData[0]&0xE3U) != 0xA2U){
                    CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                    return -1;
                }

                ackseq = SDO->CANrxData[1];   /* sequence number of the last segment, that was received correctly. */

                /* verify if response is too early */
                if(ackseq > SDO->sequence){
                    CO_SDO_abort(SDO, CO_SDO_AB_SEQ_NUM); /* Invalid sequence */
                    return -1;
                }

                /* end of transfer */
                if((SDO->endOfTransfer) && (ackseq == SDO->blksize)){
                    /* first response byte */
                    SDO->CANtxBuff->data[0] = 0xC1 | ((7 - SDO->lastLen) << 2);

                    /* CRC */
                    if(SDO->crcEnabled)
                        CO_memcpySwap2(&SDO->CANtxBuff->data[1], &SDO->crc);

                    SDO->state = CO_SDO_ST_UPLOAD_BL_END;

                    /* send response */
                    sendResponse = true;
                    break;
                }

                /* move remaining data to the beginning */
                for(i=ackseq*7, j=0; i<SDO->ODF_arg.dataLength; i++, j++)
                    SDO->ODF_arg.data[j] = SDO->ODF_arg.data[i];

                /* set remaining data length in buffer */
                SDO->ODF_arg.dataLength -= ackseq * 7U;

                /* new block size */
                SDO->blksize = SDO->CANrxData[2];

                /* If data type is domain, re-fill the data buffer if necessary and indicated so. */
                if((SDO->ODF_arg.ODdataStorage == 0) && (SDO->ODF_arg.dataLength < (SDO->blksize*7U)) && (!SDO->ODF_arg.lastSegment)){
                    /* move the beginning of the data buffer */
                    len = SDO->ODF_arg.dataLength; /* length of valid data in buffer */
                    SDO->ODF_arg.data += len;
                    SDO->ODF_arg.dataLength = CO_OD_getLength(SDO, SDO->entryNo, SDO->ODF_arg.subIndex) - len;

                    /* read next data from Object dictionary function */
                    abortCode = CO_SDO_readOD(SDO, CO_SDO_BUFFER_SIZE);
                    if(abortCode != 0U){
                        CO_SDO_abort(SDO, abortCode);
                        return -1;
                    }

                    /* calculate CRC on next bytes, if enabled */
                    if(SDO->crcEnabled){
                        SDO->crc = crc16_ccitt(SDO->ODF_arg.data, SDO->ODF_arg.dataLength, SDO->crc);
                    }

                  /* return to the original data buffer */
                    SDO->ODF_arg.data -= len;
                    SDO->ODF_arg.dataLength +=  len;
                }

                /* verify if SDO data buffer is large enough */
                if(((SDO->blksize*7U) > SDO->ODF_arg.dataLength) && (!SDO->ODF_arg.lastSegment)){
                    CO_SDO_abort(SDO, CO_SDO_AB_BLOCK_SIZE); /* Invalid block size (block mode only). */
                    return -1;
                }

                SDO->bufferOffset = 0U;
                SDO->sequence = 0U;
                SDO->endOfTransfer = false;

                /* clear flag here */
                SDO->CANrxNew = false;
            }

            /* return, if all segments was already transfered or on end of transfer */
            if((SDO->sequence == SDO->blksize) || (SDO->endOfTransfer)){
                return 1;/* don't clear the SDO->CANrxNew flag, so return directly */
            }

            /* reset timeout */
            SDO->timeoutTimer = 0;

            /* calculate length to be sent */
            len = SDO->ODF_arg.dataLength - SDO->bufferOffset;
            if(len > 7U){
                len = 7U;
            }

            /* fill response data bytes */
            for(i=0U; i<len; i++){
                SDO->CANtxBuff->data[i+1] = SDO->ODF_arg.data[SDO->bufferOffset++];
            }

            /* first response byte */
            SDO->CANtxBuff->data[0] = ++SDO->sequence;

            /* verify end of transfer */
            if((SDO->bufferOffset == SDO->ODF_arg.dataLength) && (SDO->ODF_arg.lastSegment)){
                SDO->CANtxBuff->data[0] |= 0x80;
                SDO->lastLen = len;
                SDO->blksize = SDO->sequence;
                SDO->endOfTransfer = true;
            }

            /* send response */
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);

            /* Set timerNext_ms to 0 to inform OS to call this function again without delay. */
            if(timerNext_ms != NULL){
                *timerNext_ms = 0;
            }

            /* don't clear the SDO->CANrxNew flag, so return directly */
            return 1;
        }

        case CO_SDO_ST_UPLOAD_BL_END:{
            /* verify client command specifier */
            if((SDO->CANrxData[0]&0xE1U) != 0xA1U){
                CO_SDO_abort(SDO, CO_SDO_AB_CMD);/* Client command specifier not valid or unknown. */
                return -1;
            }

            SDO->state = CO_SDO_ST_IDLE;
            break;
        }

        default:{
            CO_SDO_abort(SDO, CO_SDO_AB_DEVICE_INCOMPAT);/* general internal incompatibility in the device */
            return -1;
        }
    }

    /* free buffer and send message */
    SDO->CANrxNew = false;
    if(sendResponse) {
        CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
    }

    if(SDO->state != CO_SDO_ST_IDLE){
        return 1;
    }

    return 0;
}
