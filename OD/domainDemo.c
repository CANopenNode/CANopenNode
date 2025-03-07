/**
 * Example access to the Object Dictionary variable of type domain.
 *
 * @file        domainDemo.c
 * @author      --
 * @copyright   2021 --
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

#include "domainDemo.h"

#ifndef DOMAIN_DEMO_LENGTH_INDICATE
#define DOMAIN_DEMO_LENGTH_INDICATE 1
#endif

/* Global variables are used here for simplicity. */
/* Data simulation for domain */
static uint8_t dataSimulated = 0;
/* Index of current data byte transferred. */
static OD_size_t dataIndex = 0;
/* Size of data to be transferred. It is used when reading domainDemo
 * and updated after writing domainDemo. */
static OD_size_t dataSize = 1024;
/* Extension for OD object OD_domainDemo */
static OD_extension_t domainDemo_extension;

/*
 * Custom function for reading OD object _domainDemo_
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_read_domainDemo(OD_stream_t* stream, void* buf, OD_size_t count, OD_size_t* countRead) {
    if (stream == NULL || buf == NULL || countRead == NULL || stream->subIndex != 0) {
        return ODR_DEV_INCOMPAT;
    }

    /* Data is simple repeating sequence of values 0..255. Data can be much
     * longer than count (available size of the buffer). So
     * OD_read_domainDemo() may be called multiple times. */
    if (stream->dataOffset == 0) {
        /* Data offset is 0, so this is the first call of this function
         * in current (SDO) communication. Initialize variables. */
        dataSimulated = 0;
        dataIndex = 0;
#if DOMAIN_DEMO_LENGTH_INDICATE > 0
        /* Indicate dataLength */
        size_t dataLen = dataSize;
        stream->dataLength = dataLen <= 0xFFFFFFFF ? dataLen : 0;
#else
        /* It is not required to indicate data length in SDO transfer */
        stream->dataLength = 0;
#endif
    }

    /* copy application data into buf */
    OD_size_t i;
    for (i = 0; i < count; i++) {
        uint8_t* bufU8 = (uint8_t*)buf;
        if (dataIndex >= dataSize) {
            break;
        }
        bufU8[i] = dataSimulated++;
        dataIndex++;
    }
    *countRead = i;

    /* finished? */
    if (dataIndex >= dataSize) {
        stream->dataOffset = 0;
        return ODR_OK;
    }

    /* indicate partial read, this function will be called again. */
    stream->dataOffset = dataIndex;
    return ODR_PARTIAL;
}

/*
 * Custom function for reading OD object _domainDemo_
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_write_domainDemo(OD_stream_t* stream, const void* buf, OD_size_t count, OD_size_t* countWritten) {
    if (stream == NULL || buf == NULL || countWritten == NULL || stream->subIndex != 0) {
        return ODR_DEV_INCOMPAT;
    }

    /* Data will be just verified for correct sequence in this example
     * (repeating sequence of values 0..255). Data can be much longer than
     * count (current size of data in the buffer). So OD_write_domainDemo() may
     * be called multiple times. */
    if (stream->dataOffset == 0) {
        /* Data offset is 0, so this is the first call of this function
         * in current SDO communication. Initialize variables. */
        dataSimulated = 0;
        dataIndex = 0;
    }

    /* copy received data into application */
    OD_size_t i;
    for (i = 0; i < count; i++) {
        uint8_t* bufU8 = (uint8_t*)buf;
        /* for simulation just verify, if received data is in sequence */
        if (bufU8[i] != dataSimulated++) {
            return ODR_INVALID_VALUE;
        }
        dataIndex++;
    }
    *countWritten = i;
    stream->dataOffset = dataIndex;

    /* determine, if file write finished or not (dataLength may not yet be
     * indicated) */
    if (stream->dataLength > 0 && stream->dataOffset >= stream->dataLength) {
        stream->dataOffset = 0;
        /* Simulation - set data size to data size currently written. */
        dataSize = dataIndex;
        return ODR_OK;
    }

    /* indicate partial write, this function will be called again. */
    return ODR_PARTIAL;
}

/******************************************************************************/
CO_ReturnError_t domainDemo_init(OD_entry_t* OD_domainDemo, uint32_t* errInfo) {
    if (OD_domainDemo == NULL || errInfo == NULL)
        return CO_ERROR_ILLEGAL_ARGUMENT;

    /* Initialize custom OD object "domainDemo" */
    domainDemo_extension.object = NULL;
    domainDemo_extension.read = OD_read_domainDemo;
    domainDemo_extension.write = OD_write_domainDemo;
    ODR_t odRet = OD_extension_init(OD_domainDemo, &domainDemo_extension);

    if (odRet != ODR_OK) {
        *errInfo = OD_getIndex(OD_domainDemo);
        return CO_ERROR_OD_PARAMETERS;
    }

    return CO_ERROR_NO;
}
