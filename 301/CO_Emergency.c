/*
 * CANopen Emergency object.
 *
 * @file        CO_Emergency.c
 * @ingroup     CO_Emergency
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

#include <string.h>

#include "301/CO_Emergency.h"

/* verify configuration */
#if CO_CONFIG_EM_ERR_STATUS_BITS_COUNT < (6*8) \
    || CO_CONFIG_EM_ERR_STATUS_BITS_COUNT > 256 \
    || (CO_CONFIG_EM_ERR_STATUS_BITS_COUNT % 8) != 0
 #error CO_CONFIG_EM_ERR_STATUS_BITS_COUNT is not correct
#endif

/* fifo buffer example for fifoSize = 7 (actual capacity = 6)                 *
 *                                                                            *
 *   0      *            *             *            *                         *
 *   1    pp==wp     fifoPpPtr     fifoWrPtr        *                         *
 *   2      *            *             *            *                         *
 *   3      *            *             *        fifoWrPtr                     *
 *   4      *        fifoWrPtr     fifoPpPtr    fifoPpPtr                     *
 *   5      *            *             *            *                         *
 *   6      *            *             *            *                         *
 *                                                                            *
 *       nothing      3 bytes       4 bytes       buffer                      *
 *     to process   to process    to process      full                        *
 ******************************************************************************/

#if (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PROD_CONFIGURABLE
/*
 * Custom functions for read/write OD object "COB-ID EMCY"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_read_1014(OD_stream_t *stream, void *buf,
                          OD_size_t count, OD_size_t *countRead)
{
    if (stream == NULL || stream->subIndex != 0 || buf == NULL
        || count < sizeof(uint32_t) || countRead == NULL
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_EM_t *em = (CO_EM_t *)stream->object;

    uint16_t canId = em->producerCanId == CO_CAN_ID_EMERGENCY ?
                     CO_CAN_ID_EMERGENCY + em->nodeId : em->producerCanId;
    uint32_t COB_IDEmergency32 = em->producerEnabled ? 0 : 0x80000000;
    COB_IDEmergency32 |= canId;
    CO_setUint32(buf, COB_IDEmergency32);


    *countRead = sizeof(uint32_t);
    return ODR_OK;
}

static ODR_t OD_write_1014(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    if (stream == NULL || stream->subIndex != 0 || buf == NULL
        || count != sizeof(uint32_t) || countWritten == NULL
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_EM_t *em = (CO_EM_t *)stream->object;

    /* Verify written value. COB ID must not change, if emergency is enabled */
    uint32_t COB_IDEmergency32 = CO_getUint32(buf);
    uint16_t newCanId = (uint16_t)(COB_IDEmergency32 & 0x7FF);
    uint16_t curCanId = em->producerCanId == CO_CAN_ID_EMERGENCY ?
                        CO_CAN_ID_EMERGENCY + em->nodeId : em->producerCanId;
    bool_t newEnabled = (COB_IDEmergency32 & 0x80000000) == 0 && newCanId != 0;
    if ((COB_IDEmergency32 & 0x7FFFF800)!=0 || CO_IS_RESTRICTED_CAN_ID(newCanId)
        || (em->producerEnabled && newEnabled && newCanId != curCanId)
    ) {
        return ODR_INVALID_VALUE;
    }

    /* store values. If default CAN-ID is used, then store only value of
     * CO_CAN_ID_EMERGENCY without node id. */
    em->producerEnabled = newEnabled;
    em->producerCanId = newCanId == (CO_CAN_ID_EMERGENCY + em->nodeId) ?
                        CO_CAN_ID_EMERGENCY : newCanId;

    /* configure emergency message CAN transmission */
    if (newEnabled) {
        em->CANtxBuff = CO_CANtxBufferInit(
            em->CANdevTx,       /* CAN device */
            em->CANdevTxIdx,    /* index of specific buffer inside CAN module */
            newCanId,           /* CAN identifier */
            0,                  /* rtr */
            8U,                 /* number of data bytes */
            0);                 /* synchronous message flag bit */
    }

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}
 #else
/*
 * Custom functions for read/write OD object "COB-ID EMCY"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_read_1014_default(OD_stream_t *stream, void *buf,
                                  OD_size_t count, OD_size_t *countRead)
{
    if (stream == NULL || stream->subIndex != 0 || buf == NULL
        || count < sizeof(uint32_t) || countRead == NULL
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_EM_t *em = (CO_EM_t *)stream->object;

    uint32_t COB_IDEmergency32 = em->producerEnabled ? 0 : 0x80000000;
    COB_IDEmergency32 |= CO_CAN_ID_EMERGENCY + em->nodeId;
    CO_setUint32(buf, COB_IDEmergency32);

    *countRead = sizeof(uint32_t);
    return ODR_OK;
}
 #endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_PROD_CONFIGURABLE */

 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PROD_INHIBIT
/*
 * Custom function for writing OD object "Inhibit time EMCY"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_write_1015(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    if (stream == NULL || stream->subIndex != 0 || buf == NULL
        || count != sizeof(uint16_t) || countWritten == NULL
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_EM_t *em = (CO_EM_t *)stream->object;

    /* update object */
    em->inhibitEmTime_us = (uint32_t)CO_getUint16(buf) * 100;
    em->inhibitEmTimer = 0;

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}
 #endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_PROD_INHIBIT */
#endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER */

#if (CO_CONFIG_EM) & CO_CONFIG_EM_HISTORY
/*
 * Custom functions for read/write OD object _OD_statusBits_, optional
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_read_1003(OD_stream_t *stream, void *buf,
                          OD_size_t count, OD_size_t *countRead)
{
    if (stream == NULL || buf == NULL || countRead == NULL
        || (count < 4 && stream->subIndex > 0) || count < 1
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_EM_t *em = (CO_EM_t *)stream->object;

    if (em->fifoSize < 2) {
        return ODR_DEV_INCOMPAT;
    }
    if (stream->subIndex == 0) {
        CO_setUint8(buf, em->fifoCount);

        *countRead = sizeof(uint8_t);
        return ODR_OK;
    }
    else if (stream->subIndex <= em->fifoCount) {
        /* newest error is reported on subIndex 1 and is stored just behind
         * fifoWrPtr. Get correct index in FIFO buffer. */
        int16_t index = (int16_t)em->fifoWrPtr - stream->subIndex;
        if (index < 0) {
            index += em->fifoSize;
        }
        else if (index >= (em->fifoSize)) {
            return ODR_DEV_INCOMPAT;
        }
        CO_setUint32(buf, em->fifo[index].msg);

        *countRead = sizeof(uint32_t);
        return ODR_OK;
    }
    else {
        return ODR_NO_DATA;
    }
}

static ODR_t OD_write_1003(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    if (stream == NULL || stream->subIndex != 0 || buf == NULL || count != 1
        || countWritten == NULL)
    {
        return ODR_DEV_INCOMPAT;
    }

    if (CO_getUint8(buf) != 0) {
        return ODR_INVALID_VALUE;
    }

    CO_EM_t *em = (CO_EM_t *)stream->object;

    /* clear error history */
    em->fifoCount = 0;

    *countWritten = sizeof(uint8_t);
    return ODR_OK;
}
#endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_HISTORY */

#if (CO_CONFIG_EM) & CO_CONFIG_EM_STATUS_BITS
/*
 * Custom functions for read/write OD object _OD_statusBits_, optional
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_read_statusBits(OD_stream_t *stream, void *buf,
                                OD_size_t count, OD_size_t *countRead)
{
    if (stream == NULL || stream->subIndex != 0
        || buf == NULL || countRead == NULL)
    {
        return ODR_DEV_INCOMPAT;
    }

    CO_EM_t *em = (CO_EM_t *)stream->object;

    /* get MAX(errorStatusBitsSize, bufSize, ODsizeIndication) */
    OD_size_t countReadLocal = CO_CONFIG_EM_ERR_STATUS_BITS_COUNT / 8;
    if (countReadLocal > count) {
        countReadLocal = count;
    }
    if (stream->dataLength != 0 && countReadLocal > stream->dataLength) {
        countReadLocal = stream->dataLength;
    }
    else {
        stream->dataLength = countReadLocal;
    }

    memcpy (buf, &em->errorStatusBits[0], countReadLocal);

    *countRead = countReadLocal;
    return ODR_OK;
}

static ODR_t OD_write_statusBits(OD_stream_t *stream, const void *buf,
                                 OD_size_t count, OD_size_t *countWritten)
{
    if (stream == NULL || stream->subIndex != 0
        || buf == NULL || countWritten == NULL
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_EM_t *em = (CO_EM_t *)stream->object;

    /* get MAX(errorStatusBitsSize, bufSize, ODsizeIndication) */
    OD_size_t countWrite = CO_CONFIG_EM_ERR_STATUS_BITS_COUNT / 8;
    if (countWrite > count) {
        countWrite = count;
    }
    if (stream->dataLength != 0 && countWrite > stream->dataLength) {
        countWrite = stream->dataLength;
    }
    else {
        stream->dataLength = countWrite;
    }

    memcpy (&em->errorStatusBits[0], buf, countWrite);

    *countWritten = countWrite;
    return ODR_OK;
}
#endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_STATUS_BITS */

#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_EM_receive(void *object, void *msg) {
    CO_EM_t *em = (CO_EM_t*)object;

    if (em != NULL && em->pFunctSignalRx != NULL) {
        uint16_t ident = CO_CANrxMsg_readIdent(msg);

        /* ignore sync messages (necessary if sync object is not used) */
        if (ident != 0x80) {
            uint8_t *data = CO_CANrxMsg_readData(msg);
            uint16_t errorCode;
            uint32_t infoCode;

            memcpy(&errorCode, &data[0], sizeof(errorCode));
            memcpy(&infoCode, &data[4], sizeof(infoCode));
            em->pFunctSignalRx(ident,
                               CO_SWAP_16(errorCode),
                               data[2],
                               data[3],
                               CO_SWAP_32(infoCode));
        }
    }
}
#endif


/******************************************************************************/
CO_ReturnError_t CO_EM_init(CO_EM_t *em,
                            CO_CANmodule_t *CANdevTx,
                            const OD_entry_t *OD_1001_errReg,
#if (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)
                            CO_EM_fifo_t *fifo,
                            uint8_t fifoSize,
#endif
#if (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER
                            OD_entry_t *OD_1014_cobIdEm,
                            uint16_t CANdevTxIdx,
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PROD_INHIBIT
                            OD_entry_t *OD_1015_InhTime,
 #endif
#endif
#if (CO_CONFIG_EM) & CO_CONFIG_EM_HISTORY
                            OD_entry_t *OD_1003_preDefErr,
#endif
#if (CO_CONFIG_EM) & CO_CONFIG_EM_STATUS_BITS
                            OD_entry_t *OD_statusBits,
#endif
#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
                            CO_CANmodule_t *CANdevRx,
                            uint16_t CANdevRxIdx,
#endif
                            const uint8_t nodeId,
                            uint32_t *errInfo)
{
    (void) nodeId; /* may be unused */
    CO_ReturnError_t ret = CO_ERROR_NO;
    ODR_t odRet;

    /* verify arguments */
    if (em == NULL || OD_1001_errReg == NULL
#if (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)
        || (fifo == NULL && fifoSize >= 2)
#endif
#if (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER
        || OD_1014_cobIdEm == NULL || CANdevTx == NULL
        || nodeId < 1 || nodeId > 127
#endif
#if (CO_CONFIG_EM) & CO_CONFIG_EM_HISTORY
       || OD_1003_preDefErr == NULL
#endif
#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
       || CANdevRx == NULL
#endif
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* clear the object */
    memset(em, 0, sizeof(CO_EM_t));

    /* set object variables */
    em->CANdevTx = CANdevTx;

    /* get and verify "Error register" from Object Dictionary */
    em->errorRegister = OD_getPtr(OD_1001_errReg, 0, sizeof(uint8_t), NULL);
    if (em->errorRegister == NULL) {
        if (errInfo != NULL) *errInfo = OD_getIndex(OD_1001_errReg);
        return CO_ERROR_OD_PARAMETERS;
    }
    *em->errorRegister = 0;

#if (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)
    em->fifo = fifo;
    em->fifoSize = fifoSize;
#endif
#if (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER
    /* get initial and verify "COB-ID EMCY" from Object Dictionary */
    uint32_t COB_IDEmergency32;
    odRet = OD_get_u32(OD_1014_cobIdEm, 0, &COB_IDEmergency32, true);
    if (odRet != ODR_OK || (COB_IDEmergency32 & 0x7FFFF800) != 0) {
        if (errInfo != NULL) *errInfo = OD_getIndex(OD_1014_cobIdEm);
         /* don't break a program, if only value of a parameter is wrong */
        if (odRet != ODR_OK)
            return CO_ERROR_OD_PARAMETERS;
    }

 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PROD_CONFIGURABLE
    uint16_t producerCanId = (uint16_t)(COB_IDEmergency32 & 0x7FF);
    em->producerEnabled = (COB_IDEmergency32 & 0x80000000) == 0
                          && producerCanId != 0;

    em->OD_1014_extension.object = em;
    em->OD_1014_extension.read = OD_read_1014;
    em->OD_1014_extension.write = OD_write_1014;
    odRet = OD_extension_init(OD_1014_cobIdEm, &em->OD_1014_extension);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) *errInfo = OD_getIndex(OD_1014_cobIdEm);
        return CO_ERROR_OD_PARAMETERS;
    }
    /* following two variables are used inside OD_read_1014 and OD_write_1014 */
    em->producerCanId = producerCanId;
    em->CANdevTxIdx = CANdevTxIdx;
    /* if default producerCanId is used, then value of CO_CAN_ID_EMERGENCY
     * (0x80) is stored into non-volatile memory. In that case it is necessary
     * to add nodeId of this node to the stored value. */
    if (producerCanId == CO_CAN_ID_EMERGENCY) producerCanId += nodeId;
 #else
    uint16_t producerCanId = CO_CAN_ID_EMERGENCY + nodeId;
    em->producerEnabled = (COB_IDEmergency32 & 0x80000000) == 0;

    em->OD_1014_extension.object = em;
    em->OD_1014_extension.read = OD_read_1014_default;
    em->OD_1014_extension.write = OD_writeOriginal;
    odRet = OD_extension_init(OD_1014_cobIdEm, &em->OD_1014_extension);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) *errInfo = OD_getIndex(OD_1014_cobIdEm);
        return CO_ERROR_OD_PARAMETERS;
    }
 #endif

    /* configure parameters and emergency message CAN transmission */
    em->nodeId = nodeId;

    em->CANtxBuff = CO_CANtxBufferInit(
            CANdevTx,           /* CAN device */
            CANdevTxIdx,        /* index of specific buffer inside CAN module */
            producerCanId,      /* CAN identifier */
            0,                  /* rtr */
            8U,                 /* number of data bytes */
            0);                 /* synchronous message flag bit */

    if (em->CANtxBuff == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PROD_INHIBIT
    /* get and verify optional "Inhibit time EMCY" from Object Dictionary */
    em->inhibitEmTime_us = 0;
    em->inhibitEmTimer = 0;
    uint16_t inhibitTime_100us;
    odRet = OD_get_u16(OD_1015_InhTime, 0, &inhibitTime_100us, true);
    if (odRet == ODR_OK) {
        em->inhibitEmTime_us = (uint32_t)inhibitTime_100us * 100;

        em->OD_1015_extension.object = em;
        em->OD_1015_extension.read = OD_readOriginal;
        em->OD_1015_extension.write = OD_write_1015;
        OD_extension_init(OD_1015_InhTime, &em->OD_1015_extension);
    }
 #endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_PROD_INHIBIT */
#endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER */


#if (CO_CONFIG_EM) & CO_CONFIG_EM_HISTORY
    /* If OD entry available, make access to em->preDefErr */
    em->OD_1003_extension.object = em;
    em->OD_1003_extension.read = OD_read_1003;
    em->OD_1003_extension.write = OD_write_1003;
    OD_extension_init(OD_1003_preDefErr, &em->OD_1003_extension);
#endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_HISTORY */


#if (CO_CONFIG_EM) & CO_CONFIG_EM_STATUS_BITS
    /* If OD entry available, make access to em->errorStatusBits */
    em->OD_statusBits_extension.object = em;
    em->OD_statusBits_extension.read = OD_read_statusBits;
    em->OD_statusBits_extension.write = OD_write_statusBits;
    OD_extension_init(OD_statusBits, &em->OD_statusBits_extension);
#endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_STATUS_BITS */


#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
    em->pFunctSignalRx = NULL;
    /* configure SDO server CAN reception */
    ret = CO_CANrxBufferInit(
        CANdevRx,               /* CAN device */
        CANdevRxIdx,            /* rx buffer index */
        CO_CAN_ID_EMERGENCY,    /* CAN identifier */
        0x780,                  /* mask */
        0,                      /* rtr */
        (void*)em,              /* object passed to receive function */
        CO_EM_receive);         /* this function will process received message*/
#endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER */

    return ret;
}


/******************************************************************************/
#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
void CO_EM_initCallbackRx(CO_EM_t *em,
                          void (*pFunctSignalRx)(const uint16_t ident,
                                                 const uint16_t errorCode,
                                                 const uint8_t errorRegister,
                                                 const uint8_t errorBit,
                                                 const uint32_t infoCode))
{
    if (em != NULL) {
        em->pFunctSignalRx = pFunctSignalRx;
    }
}
#endif

#if (CO_CONFIG_EM) & CO_CONFIG_FLAG_CALLBACK_PRE
void CO_EM_initCallbackPre(CO_EM_t *em,
                           void *object,
                           void (*pFunctSignal)(void *object))
{
    if (em != NULL) {
        em->functSignalObjectPre = object;
        em->pFunctSignalPre = pFunctSignal;
    }
}
#endif


/******************************************************************************/
void CO_EM_process(CO_EM_t *em,
                   bool_t NMTisPreOrOperational,
                   uint32_t timeDifference_us,
                   uint32_t *timerNext_us)
{
    (void)timerNext_us; /* may be unused */

    /* verify errors from driver */
    uint16_t CANerrSt = em->CANdevTx->CANerrorStatus;
    if (CANerrSt != em->CANerrorStatusOld) {
        uint16_t CANerrStChanged = CANerrSt ^ em->CANerrorStatusOld;
        em->CANerrorStatusOld = CANerrSt;

        if (CANerrStChanged & (CO_CAN_ERRTX_WARNING | CO_CAN_ERRRX_WARNING))
            CO_error(em,
                (CANerrSt & (CO_CAN_ERRTX_WARNING | CO_CAN_ERRRX_WARNING)) != 0,
                CO_EM_CAN_BUS_WARNING, CO_EMC_NO_ERROR, 0);

        if (CANerrStChanged & CO_CAN_ERRTX_PASSIVE)
            CO_error(em, (CANerrSt & CO_CAN_ERRTX_PASSIVE) != 0,
                     CO_EM_CAN_TX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, 0);

        if (CANerrStChanged & CO_CAN_ERRTX_BUS_OFF)
            CO_error(em, (CANerrSt & CO_CAN_ERRTX_BUS_OFF) != 0,
                     CO_EM_CAN_TX_BUS_OFF, CO_EMC_BUS_OFF_RECOVERED, 0);

        if (CANerrStChanged & CO_CAN_ERRTX_OVERFLOW)
            CO_error(em, (CANerrSt & CO_CAN_ERRTX_OVERFLOW) != 0,
                     CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, 0);

        if (CANerrStChanged & CO_CAN_ERRTX_PDO_LATE)
            CO_error(em, (CANerrSt & CO_CAN_ERRTX_PDO_LATE) != 0,
                     CO_EM_TPDO_OUTSIDE_WINDOW, CO_EMC_COMMUNICATION, 0);

        if (CANerrStChanged & CO_CAN_ERRRX_PASSIVE)
            CO_error(em, (CANerrSt & CO_CAN_ERRRX_PASSIVE) != 0,
                     CO_EM_CAN_RX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, 0);

        if (CANerrStChanged & CO_CAN_ERRRX_OVERFLOW)
            CO_error(em, (CANerrSt & CO_CAN_ERRRX_OVERFLOW) != 0,
                     CO_EM_CAN_RXB_OVERFLOW, CO_EM_CAN_RXB_OVERFLOW, 0);
    }

    /* calculate Error register */
    uint8_t errorRegister = 0U;
    if (CO_CONFIG_ERR_CONDITION_GENERIC)
        errorRegister |= CO_ERR_REG_GENERIC_ERR;
#ifdef CO_CONFIG_ERR_CONDITION_CURRENT
    if (CO_CONFIG_ERR_CONDITION_CURRENT)
        errorRegister |= CO_ERR_REG_CURRENT;
#endif
#ifdef CO_CONFIG_ERR_CONDITION_VOLTAGE
    if (CO_CONFIG_ERR_CONDITION_VOLTAGE)
        errorRegister |= CO_ERR_REG_VOLTAGE;
#endif
#ifdef CO_CONFIG_ERR_CONDITION_TEMPERATURE
    if (CO_CONFIG_ERR_CONDITION_TEMPERATURE)
        errorRegister |= CO_ERR_REG_TEMPERATURE;
#endif
    if (CO_CONFIG_ERR_CONDITION_COMMUNICATION)
        errorRegister |= CO_ERR_REG_COMMUNICATION;
#ifdef CO_CONFIG_ERR_CONDITION_DEV_PROFILE
    if (CO_CONFIG_ERR_CONDITION_DEV_PROFILE)
        errorRegister |= CO_ERR_REG_DEV_PROFILE;
#endif
    if (CO_CONFIG_ERR_CONDITION_MANUFACTURER)
        errorRegister |= CO_ERR_REG_MANUFACTURER;
    *em->errorRegister = errorRegister;

    if (!NMTisPreOrOperational) {
        return;
    }

    /* post-process Emergency message in fifo buffer. */
#if (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER
    if (em->fifoSize >= 2) {
        uint8_t fifoPpPtr = em->fifoPpPtr;

 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PROD_INHIBIT
        if (em->inhibitEmTimer < em->inhibitEmTime_us) {
            em->inhibitEmTimer += timeDifference_us;
        }

        if (fifoPpPtr != em->fifoWrPtr && !em->CANtxBuff->bufferFull
            && em->inhibitEmTimer >= em->inhibitEmTime_us
        ) {
            em->inhibitEmTimer = 0;
 #else
        if (fifoPpPtr != em->fifoWrPtr && !em->CANtxBuff->bufferFull) {
 #endif
            /* add error register to emergency message */
            em->fifo[fifoPpPtr].msg |= (uint32_t) errorRegister << 16;

            /* send emergency message */
            memcpy(em->CANtxBuff->data, &em->fifo[fifoPpPtr].msg,
                sizeof(em->CANtxBuff->data));
            CO_CANsend(em->CANdevTx, em->CANtxBuff);

 #if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
            /* report also own emergency messages */
            if (em->pFunctSignalRx != NULL) {
                uint32_t errMsg = em->fifo[fifoPpPtr].msg;
                em->pFunctSignalRx(0,
                                   CO_SWAP_16((uint16_t) errMsg),
                                   errorRegister,
                                   (uint8_t) (errMsg >> 24),
                                   CO_SWAP_32(em->fifo[fifoPpPtr].info));
            }
 #endif

            /* increment pointer */
            em->fifoPpPtr = (++fifoPpPtr < em->fifoSize) ? fifoPpPtr : 0;

            /* verify message buffer overflow. Clear error condition if all
             * messages from fifo buffer are processed */
            if (em->fifoOverflow == 1) {
                em->fifoOverflow = 2;
                CO_errorReport(em, CO_EM_EMERGENCY_BUFFER_FULL,
                               CO_EMC_GENERIC, 0);
            }
            else if (em->fifoOverflow == 2 && em->fifoPpPtr == em->fifoWrPtr) {
                em->fifoOverflow = 0;
                CO_errorReset(em, CO_EM_EMERGENCY_BUFFER_FULL, 0);
            }
        }
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PROD_INHIBIT
  #if (CO_CONFIG_EM) & CO_CONFIG_FLAG_TIMERNEXT
        else if (timerNext_us != NULL
                 && em->inhibitEmTimer < em->inhibitEmTime_us)
        {
            /* check again after inhibit time elapsed */
            uint32_t diff = em->inhibitEmTime_us - em->inhibitEmTimer;
            if (*timerNext_us > diff) {
                *timerNext_us = diff;
            }
        }
  #endif
 #endif
    }
#elif (CO_CONFIG_EM) & CO_CONFIG_EM_HISTORY
    if (em->fifoSize >= 2) {
        uint8_t fifoPpPtr = em->fifoPpPtr;
        while (fifoPpPtr != em->fifoWrPtr) {
            /* add error register to emergency message and increment pointers */
            em->fifo[fifoPpPtr].msg |= (uint32_t) errorRegister << 16;

            if (++fifoPpPtr >= em->fifoSize) {
                fifoPpPtr = 0;
            }
        }
        em->fifoPpPtr = fifoPpPtr;
    }
#endif /* (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER, #elif CO_CONFIG_EM_HISTORY */

    return;
}


/******************************************************************************/
void CO_error(CO_EM_t *em, bool_t setError, const uint8_t errorBit,
              uint16_t errorCode, uint32_t infoCode)
{
    if (em == NULL) return;

    uint8_t index = errorBit >> 3;
    uint8_t bitmask = 1 << (errorBit & 0x7);

    /* if unsupported errorBit, change to 'CO_EM_WRONG_ERROR_REPORT' */
    if (index >= (CO_CONFIG_EM_ERR_STATUS_BITS_COUNT / 8)) {
        index = CO_EM_WRONG_ERROR_REPORT >> 3;
        bitmask = 1 << (CO_EM_WRONG_ERROR_REPORT & 0x7);
        errorCode = CO_EMC_SOFTWARE_INTERNAL;
        infoCode = errorBit;
    }

    uint8_t *errorStatusBits = &em->errorStatusBits[index];
    uint8_t errorStatusBitMasked = *errorStatusBits & bitmask;

    /* If error is already set (or unset), return without further actions,
     * otherwise toggle bit and continue with error indication. */
    if (setError) {
        if (errorStatusBitMasked != 0) {
            return;
        }
    }
    else {
        if (errorStatusBitMasked == 0) {
            return;
        }
        errorCode = CO_EMC_NO_ERROR;
    }

#if (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)
    /* prepare emergency message. Error register will be added in post-process*/
    uint32_t errMsg = (uint32_t)errorBit << 24 | CO_SWAP_16(errorCode);
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER
    uint32_t infoCodeSwapped = CO_SWAP_32(infoCode);
 #endif
#endif

    /* safely write data, and increment pointers */
    CO_LOCK_EMCY(em->CANdevTx);
    if (setError) *errorStatusBits |= bitmask;
    else          *errorStatusBits &= ~bitmask;

#if (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)
    if (em->fifoSize >= 2) {
        uint8_t fifoWrPtr = em->fifoWrPtr;
        uint8_t fifoWrPtrNext = fifoWrPtr + 1;
        if (fifoWrPtrNext >= em->fifoSize) {
            fifoWrPtrNext = 0;
        }

        if (fifoWrPtrNext == em->fifoPpPtr) {
            em->fifoOverflow = 1;
        }
        else {
            em->fifo[fifoWrPtr].msg = errMsg;
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER
            em->fifo[fifoWrPtr].info = infoCodeSwapped;
 #endif
            em->fifoWrPtr = fifoWrPtrNext;
            if (em->fifoCount < (em->fifoSize - 1)) em->fifoCount++;
        }
    }
#endif /* (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY) */

    CO_UNLOCK_EMCY(em->CANdevTx);

#if (CO_CONFIG_EM) & CO_CONFIG_FLAG_CALLBACK_PRE
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER
    /* Optional signal to RTOS, which can resume task, which handles
     * CO_EM_process */
    if (em->pFunctSignalPre != NULL && em->producerEnabled) {
        em->pFunctSignalPre(em->functSignalObjectPre);
    }
 #endif
#endif
}
