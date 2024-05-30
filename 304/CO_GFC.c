/**
 * CANopen Global fail-safe command protocol.
 *
 * @file        CO_GFC.c
 * @ingroup     CO_GFC
 * @author      Robert Grüning
 * @copyright   2020 Robert Grüning
 * @copyright   2024 Janez Paternoster
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

#include "304/CO_GFC.h"

#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE

/*
 * Custom function for reading or writing OD object.
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t
OD_write_1300(OD_stream_t* stream, const void* buf, OD_size_t count, OD_size_t* countWritten) {
    if ((stream == NULL) || (stream->subIndex != 0U) || (buf == NULL) || (countWritten == NULL)) {
        return ODR_DEV_INCOMPAT;
    }

    CO_GFC_t* GFC = stream->object;

    uint8_t value = CO_getUint8(buf);
    if (value > 1) {
        return ODR_INVALID_VALUE;
    }

    GFC->valid = (value == 1);

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}

#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_CONSUMER
static void
CO_GFC_receive(void* object, void* msg) {
    CO_GFC_t* GFC;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);

    GFC = (CO_GFC_t*)object; /* this is the correct pointer type of the first argument */

    if (GFC->valid && (DLC == 0)) {

        /* Callback signals Global Failsafe Command */
        if (GFC->pFunctSignalSafe != NULL) {
            GFC->pFunctSignalSafe(GFC->functSignalObjectSafe);
        }
    }
}

void
CO_GFC_initCallbackEnterSafeState(CO_GFC_t* GFC, void* object, void (*pFunctSignalSafe)(void* object)) {
    if (GFC != NULL) {
        GFC->functSignalObjectSafe = object;
        GFC->pFunctSignalSafe = pFunctSignalSafe;
    }
}
#endif


CO_ReturnError_t
CO_GFC_init(CO_GFC_t* GFC, OD_entry_t* OD_1300_gfcParameter, CO_CANmodule_t* GFC_CANdevRx, uint16_t GFC_rxIdx, uint16_t CANidRxGFC,
            CO_CANmodule_t* GFC_CANdevTx, uint16_t GFC_txIdx, uint16_t CANidTxGFC) {
    if (GFC == NULL || OD_1300_gfcParameter == NULL || GFC_CANdevRx == NULL || GFC_CANdevTx == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    uint8_t valid = 0;
    if (OD_get_u8(OD_1300_gfcParameter, 0, &valid, true) != ODR_OK) {
        return CO_ERROR_OD_PARAMETERS;
    }
    GFC->valid = (valid == 1);

    /* Configure Object dictionary entry at index 0x1300+ */
    GFC->OD_gfcParam_ext.object = GFC;
    GFC->OD_gfcParam_ext.read = OD_readOriginal;
    GFC->OD_gfcParam_ext.write = OD_write_1300;
    OD_extension_init(OD_1300_gfcParameter, &GFC->OD_gfcParam_ext);

#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_PRODUCER
    GFC->CANdevTx = GFC_CANdevTx;
    GFC->CANtxBuff = CO_CANtxBufferInit(GFC->CANdevTx, /* CAN device */
                                        GFC_txIdx,     /* index of specific buffer inside CAN module */
                                        CANidTxGFC,    /* CAN identifier */
                                        0,             /* rtr */
                                        0,             /* number of data bytes */
                                        0);            /* synchronous message flag bit */

    if (GFC->CANtxBuff == NULL) {
        return CO_ERROR_TX_UNCONFIGURED;
    }
#else
    (void)GFC_txIdx;  /* unused */
    (void)CANidTxGFC; /* unused */
#endif

#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_CONSUMER
    GFC->functSignalObjectSafe = NULL;
    GFC->pFunctSignalSafe = NULL;
    const CO_ReturnError_t r = CO_CANrxBufferInit(GFC_CANdevRx,    /* CAN device */
                                                  GFC_rxIdx,       /* rx buffer index */
                                                  CANidRxGFC,      /* CAN identifier */
                                                  0x7FF,           /* mask */
                                                  0,               /* rtr */
                                                  (void*)GFC,      /* object passed to receive function */
                                                  CO_GFC_receive); /* this function will process received message */
    if (r != CO_ERROR_NO) {
        return r;
    }
#else
    (void)GFC_rxIdx;  /* unused */
    (void)CANidRxGFC; /* unused */
#endif

    return CO_ERROR_NO;
}

#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_PRODUCER
CO_ReturnError_t
CO_GFCsend(CO_GFC_t* GFC) {
    if (GFC->valid) {
        return CO_CANsend(GFC->CANdevTx, GFC->CANtxBuff);
    }
    return CO_ERROR_NO;
}
#endif

#endif /* (CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE */
