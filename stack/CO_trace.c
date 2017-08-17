/*
 * CANopen trace interface.
 *
 * @file        CO_trace.c
 * @author      Janez Paternoster
 * @copyright   2016 Janez Paternoster
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


#include "CO_trace.h"
#include <stdio.h>


/* Different functions for processing value for different data types. */
static int32_t getValueI8 (void *OD_variable) { return (int32_t) *((int8_t*)   OD_variable);}
static int32_t getValueI16(void *OD_variable) { return (int32_t) *((int16_t*)  OD_variable);}
static int32_t getValueI32(void *OD_variable) { return           *((int32_t*)  OD_variable);}
static int32_t getValueU8 (void *OD_variable) { return (int32_t) *((uint8_t*)  OD_variable);}
static int32_t getValueU16(void *OD_variable) { return (int32_t) *((uint16_t*) OD_variable);}
static int32_t getValueU32(void *OD_variable) { return           *((int32_t*)  OD_variable);}


/* Different functions for printing points for different data types. */
static uint32_t printPointCsv(char *s, uint32_t size, uint32_t timeStamp, int32_t value) {
    return snprintf(s, size, "%lu;%ld\n", timeStamp,             value);
}
static uint32_t printPointCsvUnsigned(char *s, uint32_t size, uint32_t timeStamp, int32_t value) {
    return snprintf(s, size, "%lu;%lu\n", timeStamp, (uint32_t)  value);
}
static uint32_t printPointBinary(char *s, uint32_t size, uint32_t timeStamp, int32_t value) {
    if(size < 8) return 0;
    CO_memcpySwap4(s, &timeStamp);
    CO_memcpySwap4(s+4, &value);
    return 8;
}
static uint32_t printPointSvgStart(char *s, uint32_t size, uint32_t timeStamp, int32_t value) {
    return snprintf(s, size, "M%lu,%ld", timeStamp,             value);
}
static uint32_t printPointSvgStartUnsigned(char *s, uint32_t size, uint32_t timeStamp, int32_t value) {
    return snprintf(s, size, "M%lu,%lu", timeStamp, (uint32_t)  value);
}
static uint32_t printPointSvg(char *s, uint32_t size, uint32_t timeStamp, int32_t value) {
    return snprintf(s, size, "H%luV%ld", timeStamp,             value);
}
static uint32_t printPointSvgUnsigned(char *s, uint32_t size, uint32_t timeStamp, int32_t value) {
    return snprintf(s, size, "H%luV%lu", timeStamp, (uint32_t)  value);
}


/* Collection of function pointers for fast processing based on specific data type. */
/* Rules for the array: There must be groups of six members (I8, I16, I32, U8, U16, U32)
 * in correct order and sequence, so findVariable() finds correct member. */
static const CO_trace_dataType_t dataTypes[] = {
    {getValueI8,  printPointCsv,              printPointCsv,         printPointCsv},
    {getValueI16, printPointCsv,              printPointCsv,         printPointCsv},
    {getValueI32, printPointCsv,              printPointCsv,         printPointCsv},
    {getValueU8,  printPointCsvUnsigned,      printPointCsvUnsigned, printPointCsvUnsigned},
    {getValueU16, printPointCsvUnsigned,      printPointCsvUnsigned, printPointCsvUnsigned},
    {getValueU32, printPointCsvUnsigned,      printPointCsvUnsigned, printPointCsvUnsigned},
    {getValueI8,  printPointBinary,           printPointBinary,      printPointBinary},
    {getValueI16, printPointBinary,           printPointBinary,      printPointBinary},
    {getValueI32, printPointBinary,           printPointBinary,      printPointBinary},
    {getValueU8,  printPointBinary,           printPointBinary,      printPointBinary},
    {getValueU16, printPointBinary,           printPointBinary,      printPointBinary},
    {getValueU32, printPointBinary,           printPointBinary,      printPointBinary},
    {getValueI8,  printPointSvgStart,         printPointSvg,         printPointSvg},
    {getValueI16, printPointSvgStart,         printPointSvg,         printPointSvg},
    {getValueI32, printPointSvgStart,         printPointSvg,         printPointSvg},
    {getValueU8,  printPointSvgStartUnsigned, printPointSvgUnsigned, printPointSvgUnsigned},
    {getValueU16, printPointSvgStartUnsigned, printPointSvgUnsigned, printPointSvgUnsigned},
    {getValueU32, printPointSvgStartUnsigned, printPointSvgUnsigned, printPointSvgUnsigned}
};


/* Find variable in Object Dictionary *****************************************/
static void findVariable(CO_trace_t *trace) {
    bool_t err = false;
    uint16_t index;
    uint8_t subIndex;
    uint8_t dataLen;
    void *OdDataPtr = NULL;
    int dtIndex = 0;

    /* parse mapping */
    index = (uint16_t) ((*trace->map) >> 16);
    subIndex = (uint8_t) ((*trace->map) >> 8);
    dataLen = (uint8_t) (*trace->map);
    if((dataLen & 0x07) != 0) { /* data length must be byte aligned */
        err = true;
    }
    dataLen >>= 3;   /* in bytes now */
    if(dataLen == 0) {
        dataLen = 4;
    }

    /* find mapped variable, if map available */
    if(!err && (index != 0 || subIndex != 0)) {
        uint16_t entryNo = CO_OD_find(trace->SDO, index);

        if(index >= 0x1000 && entryNo != 0xFFFF && subIndex <= trace->SDO->OD[entryNo].maxSubIndex) {
            OdDataPtr = CO_OD_getDataPointer(trace->SDO, entryNo, subIndex);
        }

        if(OdDataPtr != NULL) {
            uint16_t len = CO_OD_getLength(trace->SDO, entryNo, subIndex);

            if(len < dataLen) {
                dataLen = len;
            }
        }
        else {
            err = true;
        }
    }

    /* Get function pointers for correct data type */
    if(!err) {
        /* first sequence: data length */
        switch(dataLen) {
            case 1: dtIndex = 0; break;
            case 2: dtIndex = 1; break;
            case 4: dtIndex = 2; break;
            default: err = true; break;
        }
        /* second sequence: signed or unsigned */
        if(((*trace->format) & 1) == 1) {
            dtIndex += 3;
        }
        /* third sequence: Output type */
        dtIndex += ((*trace->format) >> 1) * 6;

        if(dtIndex > (sizeof(dataTypes) / sizeof(CO_trace_dataType_t))) {
            err = true;
        }
    }

    /* set output variables */
    if(!err) {
        if(OdDataPtr != NULL) {
            trace->OD_variable = OdDataPtr;
        }
        else {
            trace->OD_variable = trace->value;
        }
        trace->dt = &dataTypes[dtIndex];
    }
    else  {
        trace->OD_variable = NULL;
        trace->dt = NULL;
    }
}


/* OD function for accessing _OD_traceConfig_ (index 0x2300+) from SDO server.
 * For more information see file CO_SDO.h. */
static CO_SDO_abortCode_t CO_ODF_traceConfig(CO_ODF_arg_t *ODF_arg) {
    CO_trace_t *trace;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    trace = (CO_trace_t*) ODF_arg->object;

    switch(ODF_arg->subIndex) {
    case 1:     /* size */
        if(ODF_arg->reading) {
            uint32_t *value = (uint32_t*) ODF_arg->data;
            *value = trace->bufferSize;
        }
        break;

    case 2:     /* axisNo (trace enabled if nonzero) */
        if(ODF_arg->reading) {
            uint8_t *value = (uint8_t*) ODF_arg->data;
            if(!trace->enabled) {
                *value = 0;
            }
        }
        else {
            uint8_t *value = (uint8_t*) ODF_arg->data;

            if(*value == 0) {
                trace->enabled = false;
            }
            else if(!trace->enabled) {
                if(trace->bufferSize == 0) {
                    ret = CO_SDO_AB_OUT_OF_MEM;
                }
                else {
                    /* set trace->OD_variable and trace->dt, based on 'map' and 'format' */
                    findVariable(trace);

                    if(trace->OD_variable != NULL) {
                        *trace->value = 0;
                        *trace->minValue = 0;
                        *trace->maxValue = 0;
                        *trace->triggerTime = 0;
                        trace->valuePrev = 0;
                        trace->readPtr = 0;
                        trace->writePtr = 0;
                        trace->enabled = true;
                    }
                    else {
                        ret = CO_SDO_AB_NO_MAP;
                    }
                }
            }
        }
        break;

    case 5:     /* map */
    case 6:     /* format */
        if(!ODF_arg->reading) {
            if(trace->enabled) {
                ret = CO_SDO_AB_INVALID_VALUE;
            }
        }
        break;
    }

    return ret;
}


/* OD function for accessing _OD_trace_ (index 0x2400+) from SDO server.
 * For more information see file CO_SDO.h. */
static CO_SDO_abortCode_t CO_ODF_trace(CO_ODF_arg_t *ODF_arg) {
    CO_trace_t *trace;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    trace = (CO_trace_t*) ODF_arg->object;

    switch(ODF_arg->subIndex) {
    case 1:     /* size */
        if(ODF_arg->reading) {
            uint32_t *value = (uint32_t*) ODF_arg->data;
            uint32_t size = trace->bufferSize;
            uint32_t wp = trace->writePtr;
            uint32_t rp = trace->readPtr;

            if(wp >= rp) {
                *value = wp - rp;
            }
            else {
                *value = size - rp + wp;
            }
        }
        else {
            uint32_t *value = (uint32_t*) ODF_arg->data;

            if(*value == 0) {
                /* clear buffer, handle race conditions */
                while(trace->readPtr != 0 || trace->writePtr != 0) {
                    trace->readPtr = 0;
                    trace->writePtr = 0;
                    *trace->triggerTime = 0;
                }
            }
            else {
                ret = CO_SDO_AB_INVALID_VALUE;
            }
        }
        break;

    case 5:     /* plot */
        if(ODF_arg->reading) {
            /* This plot will be transmitted as domain data type. String data
             * will be printed directly to SDO buffer. If there is more data
             * to print, than is the size of SDO buffer, then this function
             * will be called multiple times until internal trace buffer is
             * empty. Internal trace buffer is circular buffer. It is accessed
             * by this function and by higher priority thread. If this buffer
             * is full, there is a danger for race condition. First records
             * from trace buffer may be overwritten somewhere between. If this
             * is detected, then do{}while() loop tries printing again. */
            if(trace->bufferSize == 0 || ODF_arg->dataLength < 100) {
                ret = CO_SDO_AB_OUT_OF_MEM;
            }
            else if(trace->readPtr == trace->writePtr) {
                ret = CO_SDO_AB_NO_DATA;
            }
            else {
                uint32_t rp, t, v, len, freeLen;
                char *s;
                bool_t readPtrOverflowed; /* for handling race conditions */

                /* repeat everything, if trace->readPtr was overflowed in CO_trace_process */
                do {
                    readPtrOverflowed = false;
                    s = (char*) ODF_arg->data;
                    freeLen = ODF_arg->dataLength;

                    rp = trace->readPtr;

                    /* start plot, increment variables, verify overflow */
                    if(ODF_arg->firstSegment) {
                        t = trace->timeBuffer[rp];
                        v = trace->valueBuffer[rp];
                        rp ++;
                        if(++trace->readPtr == trace->bufferSize) {
                            trace->readPtr = 0;
                            if(rp != trace->bufferSize) {
                                readPtrOverflowed = true;
                                continue;
                            }
                            rp = 0;
                        }
                        if(rp != trace->readPtr) {
                            readPtrOverflowed = true;
                            continue;
                        }
                        len = trace->dt->printPointStart(s, freeLen, t, v);
                        s += len;
                        freeLen -= len;
                    }

                    /* print other points */
                    if(rp != trace->writePtr) {
                    for(;;) {
                        t = trace->timeBuffer[rp];
                        v = trace->valueBuffer[rp];
                        rp ++;
                        if(++trace->readPtr == trace->bufferSize) {
                            trace->readPtr = 0;
                            if(rp != trace->bufferSize && ODF_arg->firstSegment) {
                                readPtrOverflowed = true;
                                break;
                            }
                            rp = 0;
                        }
                        if(rp != trace->readPtr && ODF_arg->firstSegment) {
                            readPtrOverflowed = true;
                            break;
                        }

                        /* If internal buffer is empty, end transfer */
                        if(rp == trace->writePtr) {
                            /* If there is last time stamp, point will be printed at the end */
                            if(t != trace->lastTimeStamp) {
                                len = trace->dt->printPoint(s, freeLen, t, v);
                                s += len;
                                freeLen -= len;
                            }
                            ODF_arg->lastSegment = true;
                            break;
                        }
                        len = trace->dt->printPoint(s, freeLen, t, v);
                        s += len;
                        freeLen -= len;

                        /* if output buffer is full, next data will be sent later */
                        if(freeLen < 50) {
                            ODF_arg->lastSegment = false;
                            break;
                        }
                    }
                    }

                    /* print last point */
                    if(!readPtrOverflowed && ODF_arg->lastSegment) {
                        v = trace->valuePrev;
                        t = trace->lastTimeStamp;
                        len = trace->dt->printPointEnd(s, freeLen, t, v);
                        s += len;
                        freeLen -= len;
                    }
                } while(readPtrOverflowed);

                ODF_arg->dataLength -= freeLen;
            }
        }
        break;
    }

    return ret;
}


/******************************************************************************/
void CO_trace_init(
        CO_trace_t             *trace,
        CO_SDO_t               *SDO,
        uint8_t                 enabled,
        uint32_t               *timeBuffer,
        int32_t                *valueBuffer,
        uint32_t                bufferSize,
        uint32_t               *map,
        uint8_t                *format,
        uint8_t                *trigger,
        int32_t                *threshold,
        int32_t                *value,
        int32_t                *minValue,
        int32_t                *maxValue,
        uint32_t               *triggerTime,
        uint16_t                idx_OD_traceConfig,
        uint16_t                idx_OD_trace)
{
    trace->SDO = SDO;
    trace->enabled = (enabled != 0) ? true : false;
    trace->timeBuffer = timeBuffer;
    trace->valueBuffer = valueBuffer;
    trace->bufferSize = bufferSize;
    trace->writePtr = 0;
    trace->readPtr = 0;
    trace->lastTimeStamp = 0;
    trace->map = map;
    trace->format = format;
    trace->trigger = trigger;
    trace->threshold = threshold;
    trace->value = value;
    trace->minValue = minValue;
    trace->maxValue = maxValue;
    trace->triggerTime = triggerTime;
    *trace->value = 0;
    *trace->minValue = 0;
    *trace->maxValue = 0;
    *trace->triggerTime = 0;
    trace->valuePrev = 0;

    /* set trace->OD_variable and trace->dt, based on 'map' and 'format' */
    findVariable(trace);

    if(timeBuffer == NULL || valueBuffer == NULL) {
        trace->bufferSize = 0;
    }

    if( trace->bufferSize == 0 || trace->OD_variable == NULL) {
        trace->enabled = false;
    }

    CO_OD_configure(SDO, idx_OD_traceConfig, CO_ODF_traceConfig, (void*)trace, 0, 0);
    CO_OD_configure(SDO, idx_OD_trace, CO_ODF_trace, (void*)trace, 0, 0);
}


/******************************************************************************/
void CO_trace_process(CO_trace_t *trace, uint32_t timestamp) {
    if(trace->enabled) {

        int32_t val = trace->dt->pGetValue(trace->OD_variable);

        if(val != trace->valuePrev) {
            /* Verify, if value passed threshold */
            if((*trace->trigger & 1) != 0 && trace->valuePrev < *trace->threshold && val >= *trace->threshold) {
                *trace->triggerTime = timestamp;
            }
            if((*trace->trigger & 2) != 0 && trace->valuePrev < *trace->threshold && val >= *trace->threshold) {
                *trace->triggerTime = timestamp;
            }

            /* Write value and verify min/max */
            if(trace->value != trace->OD_variable) {
                *trace->value = val;
            }
            trace->valuePrev = val;
            if(*trace->minValue > val) {
                *trace->minValue = val;
            }
            if(*trace->maxValue < val) {
                *trace->maxValue = val;
            }

            /* write buffers and update pointers */
            trace->timeBuffer[trace->writePtr] = timestamp;
            trace->valueBuffer[trace->writePtr] = val;
            if(++trace->writePtr == trace->bufferSize) {
                trace->writePtr = 0;
            }
            if(trace->writePtr == trace->readPtr) {
                if(++trace->readPtr == trace->bufferSize) {
                    trace->readPtr = 0;
                }
            }
        }
        else {
            /* if buffer is empty, make first record */
            if(trace->writePtr == trace->readPtr) {
                /* write buffers and update pointers */
                trace->timeBuffer[trace->writePtr] = timestamp;
                trace->valueBuffer[trace->writePtr] = val;
                if(++trace->writePtr == trace->bufferSize) {
                    trace->writePtr = 0;
                }
            }
        }
        trace->lastTimeStamp = timestamp;
    }
}
