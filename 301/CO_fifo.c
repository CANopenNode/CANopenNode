/*
 * FIFO circular buffer
 *
 * @file        CO_fifo.c
 * @ingroup     CO_CANopen_309_fifo
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

#include "301/CO_fifo.h"

#if (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ENABLE

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "crc16-ccitt.h"

#if (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_COMMANDS
#include <stdio.h>
#include <inttypes.h>

/* Non-graphical character for command delimiter */
#define DELIM_COMMAND '\n'
/* Graphical character for comment delimiter */
#define DELIM_COMMENT '#'
#endif /* (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_COMMANDS */

/* verify configuration */
#if (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_CRC16_CCITT
 #if !((CO_CONFIG_CRC16) & CO_CONFIG_CRC16_ENABLE)
  #error CO_CONFIG_CRC16_ENABLE must be enabled.
 #endif
#endif

/******************************************************************************/
void CO_fifo_init(CO_fifo_t *fifo, char *buf, size_t bufSize) {

    if (fifo == NULL || buf == NULL || bufSize < 2) {
        return;
    }

    fifo->readPtr = 0;
    fifo->writePtr = 0;
    fifo->buf = buf;
    fifo->bufSize = bufSize;

    return;
}


/* Circular FIFO buffer example for fifo->bufSize = 7 (usable size = 6): ******
 *                                                                            *
 *   0      *            *             *            *                         *
 *   1    rp==wp      readPtr      writePtr         *                         *
 *   2      *            *             *            *                         *
 *   3      *            *             *        writePtr                      *
 *   4      *        writePtr       readPtr      readPtr                      *
 *   5      *            *             *            *                         *
 *   6      *            *             *            *                         *
 *                                                                            *
 *        empty       3 bytes       4 bytes       buffer                      *
 *        buffer      in buff       in buff       full                        *
 ******************************************************************************/
size_t CO_fifo_write(CO_fifo_t *fifo,
                     const char *buf,
                     size_t count,
                     uint16_t *crc)
{
    size_t i;
    char *bufDest;

    if (fifo == NULL || fifo->buf == NULL || buf == NULL) {
        return 0;
    }

    bufDest = &fifo->buf[fifo->writePtr];
    for (i = count; i > 0; i--) {
        size_t writePtrNext = fifo->writePtr + 1;

        /* is circular buffer full */
        if (writePtrNext == fifo->readPtr ||
            (writePtrNext == fifo->bufSize && fifo->readPtr == 0)) {
            break;
        }

        *bufDest = *buf;

#if (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_CRC16_CCITT
        if (crc != NULL) {
            crc16_ccitt_single(crc, (unsigned char)*buf);
        }
#endif

        /* increment variables */
        if (writePtrNext == fifo->bufSize) {
            fifo->writePtr = 0;
            bufDest = &fifo->buf[0];
        }
        else {
            fifo->writePtr++;
            bufDest++;
        }
        buf++;
    }

    return count - i;
}


/******************************************************************************/
size_t CO_fifo_read(CO_fifo_t *fifo, char *buf, size_t count, bool_t *eof) {
    size_t i;
    const char *bufSrc;

    if (eof != NULL) {
        *eof = false;
    }
    if (fifo == NULL || buf == NULL || fifo->readPtr == fifo->writePtr) {
        return 0;
    }

    bufSrc = &fifo->buf[fifo->readPtr];
    for (i = count; i > 0; ) {
        const char c = *bufSrc;

        /* is circular buffer empty */
        if (fifo->readPtr == fifo->writePtr) {
            break;
        }

        *(buf++) = c;

        /* increment variables */
        if (++fifo->readPtr == fifo->bufSize) {
            fifo->readPtr = 0;
            bufSrc = &fifo->buf[0];
        }
        else {
            bufSrc++;
        }
        i--;

#if (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_COMMANDS
        /* is delimiter? */
        if (eof != NULL && c == DELIM_COMMAND) {
            *eof = true;
            break;
        }
#endif
    }

    return count - i;
}


#if (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ALT_READ
/******************************************************************************/
size_t CO_fifo_altBegin(CO_fifo_t *fifo, size_t offset) {
    size_t i;

    if (fifo == NULL) {
        return 0;
    }

    fifo->altReadPtr = fifo->readPtr;
    for (i = offset; i > 0; i--) {
        /* is circular buffer empty */
        if (fifo->altReadPtr == fifo->writePtr) {
            break;
        }

        /* increment variable */
        if (++fifo->altReadPtr == fifo->bufSize) {
            fifo->altReadPtr = 0;
        }
    }

    return offset - i;
}

void CO_fifo_altFinish(CO_fifo_t *fifo, uint16_t *crc) {

    if (fifo == NULL) {
        return;
    }

    if (crc == NULL) {
        fifo->readPtr = fifo->altReadPtr;
    }
    else {
        const char *bufSrc = &fifo->buf[fifo->readPtr];
        while (fifo->readPtr != fifo->altReadPtr) {
#if (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_CRC16_CCITT
            crc16_ccitt_single(crc, (unsigned char)*bufSrc);
#endif
            /* increment variable */
            if (++fifo->readPtr == fifo->bufSize) {
                fifo->readPtr = 0;
                bufSrc = &fifo->buf[0];
            }
            else {
                bufSrc++;
            }
        }
    }
}

size_t CO_fifo_altRead(CO_fifo_t *fifo, char *buf, size_t count) {
    size_t i;
    const char *bufSrc;

    bufSrc = &fifo->buf[fifo->altReadPtr];
    for (i = count; i > 0; i--) {
        const char c = *bufSrc;

        /* is there no more data */
        if (fifo->altReadPtr == fifo->writePtr) {
            break;
        }

        *(buf++) = c;

        /* increment variables */
        if (++fifo->altReadPtr == fifo->bufSize) {
            fifo->altReadPtr = 0;
            bufSrc = &fifo->buf[0];
        }
        else {
            bufSrc++;
        }
    }

    return count - i;
}
#endif /* (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ALT_READ */


#if (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_COMMANDS
/******************************************************************************/
bool_t CO_fifo_CommSearch(CO_fifo_t *fifo, bool_t clear) {
    bool_t newCommand = false;
    size_t count;
    char *commandEnd;

    if (fifo == NULL || fifo->readPtr == fifo->writePtr) {
        return 0;
    }

    /* search delimiter until writePtr or until end of buffer */
    if (fifo->readPtr < fifo->writePtr) {
        count = fifo->writePtr - fifo->readPtr;
    } else {
        count = fifo->bufSize - fifo->readPtr;
    }
    commandEnd = (char *)memchr((const void *)&fifo->buf[fifo->readPtr],
                                DELIM_COMMAND,
                                count);
    if (commandEnd != NULL) {
        newCommand = true;
    }
    else if (fifo->readPtr > fifo->writePtr) {
        /* not found, search in the beginning of the circular buffer */
        commandEnd = (char *)memchr((const void *)&fifo->buf[0],
                                    DELIM_COMMAND,
                                    fifo->writePtr);
        if (commandEnd != NULL || fifo->readPtr == (fifo->writePtr + 1)) {
            /* command delimiter found or buffer full */
            newCommand = true;
        }
    }
    else if (fifo->readPtr == 0 && fifo->writePtr == (fifo->bufSize - 1)) {
        /* buffer full */
        newCommand = true;
    }

    /* Clear buffer if set so */
    if (clear) {
        if (commandEnd != NULL) {
            fifo->readPtr = (int)(commandEnd - fifo->buf) + 1;
            if (fifo->readPtr == fifo->bufSize) {
                fifo->readPtr = 0;
            }
        }
        else {
            fifo->readPtr = fifo->writePtr;
        }
    }

    return newCommand;
}


/******************************************************************************/
bool_t CO_fifo_trimSpaces(CO_fifo_t *fifo) {
    bool_t delimCommandFound = false;

    if (fifo != NULL) {
        while (fifo->readPtr != fifo->writePtr) {
            char c = fifo->buf[fifo->readPtr];

            if (c == DELIM_COMMENT) {
                /* comment found, clear all until next DELIM_COMMAND */
                while (fifo->readPtr != fifo->writePtr) {
                    if (++fifo->readPtr == fifo->bufSize) {
                        fifo->readPtr = 0;
                    }
                    if (c == DELIM_COMMAND) {
                        delimCommandFound = true;
                        break;
                    }
                }
                break;
            }
            else if (isgraph((int)c) != 0) {
                break;
            }
            if (++fifo->readPtr == fifo->bufSize) {
                fifo->readPtr = 0;
            }
            if (c == DELIM_COMMAND) {
                delimCommandFound = true;
                break;
            }
        }
    }
    return delimCommandFound;
}


/******************************************************************************/
size_t CO_fifo_readToken(CO_fifo_t *fifo,
                         char *buf,
                         size_t count,
                         char *closed,
                         bool_t *err)
{
    bool_t delimCommandFound = false;
    bool_t delimCommentFound = false;
    size_t tokenSize = 0;

    if (fifo != NULL && buf != NULL && count > 1 && (err == NULL || *err == 0)
        && fifo->readPtr != fifo->writePtr
    ) {
        bool_t finished = false;
        char step = 0;
        size_t ptr = fifo->readPtr; /* current pointer (integer, 0 based) */
        char *c = &fifo->buf[ptr];  /* current char */
        do {
            switch (step) {
            case 0: /* skip leading empty characters, stop on delimiter */
                if (isgraph((int)*c) != 0) {
                    if (*c == DELIM_COMMENT) {
                        delimCommentFound = true;
                    } else {
                        buf[tokenSize++] = *c;
                        step++;
                    }
                }
                else if (*c == DELIM_COMMAND) {
                    delimCommandFound = true;
                }
                break;
            case 1: /* search for end of the token */
                if (isgraph((int)*c) != 0) {
                    if (*c == DELIM_COMMENT) {
                        delimCommentFound = true;
                    } else if (tokenSize < count) {
                        buf[tokenSize++] = *c;
                    }
                }
                else {
                    if (*c == DELIM_COMMAND) {
                        delimCommandFound = true;
                    }
                    step++;
                }
                break;
            case 2: /* skip trailing empty characters */
                if (isgraph((int)*c) != 0) {
                    if (*c == DELIM_COMMENT) {
                        delimCommentFound = true;
                    } else {
                        fifo->readPtr = ptr;
                        finished = true;
                    }
                }
                else if (*c == DELIM_COMMAND) {
                    delimCommandFound = true;
                }
                break;
            }
            if (delimCommentFound == true) {
                /* Comment delimiter found, clear all till end of the line. */
                fifo->readPtr = ptr;
                delimCommandFound = CO_fifo_CommSearch(fifo, true);
                finished = true;
            }
            else if (delimCommandFound) {
                /* command delimiter found, set readPtr behind it. */
                if (++ptr == fifo->bufSize) ptr = 0;
                fifo->readPtr = ptr;
                finished = true;
            }
            else if (!finished) {
                /* find next character in the circular buffer */
                if (++ptr == fifo->bufSize) {
                    ptr = 0;
                    c = &fifo->buf[ptr];
                }
                else {
                    c++;
                }
                /* end, if buffer is now empty */
                if (ptr == fifo->writePtr) {
                    if (step == 2) {
                        fifo->readPtr = ptr;
                    }
                    else {
                        tokenSize = 0;
                    }
                    finished = true;
                }
            }
        } while (!finished);
    }

    /* set 'err' return value */
    if (err != NULL && *err == false) {
        if (tokenSize == count || (closed != NULL &&
            ((*closed == 1 && (!delimCommandFound || tokenSize == 0)) ||
             (*closed == 0 && (delimCommandFound || tokenSize == 0)))
        )) {
            *err = true;
        }
    }
    /* set 'closed' return value */
    if (closed != NULL) {
        *closed = delimCommandFound ? 1 : 0;
    }

    /* token was larger then size of the buffer, all was cleaned, return '' */
    if (tokenSize == count) {
        tokenSize = 0;
    }
    /* write string terminator character */
    if (buf != NULL && count > tokenSize) {
        buf[tokenSize] = '\0';
    }

    return tokenSize;
}
#endif /* (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_COMMANDS */


#if (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_DATATYPES
/******************************************************************************/
size_t CO_fifo_readU82a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    uint8_t n;

    if (fifo != NULL && count >= 6 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "%"PRIu8, n);
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readU162a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    uint16_t n;

    if (fifo != NULL && count >= 8 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "%"PRIu16, CO_SWAP_16(n));
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readU322a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    uint32_t n;

    if (fifo != NULL && count >= 12 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "%"PRIu32, CO_SWAP_32(n));
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readU642a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    uint64_t n;

    if (fifo != NULL && count >= 20 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "%"PRIu64, CO_SWAP_64(n));
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readX82a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    uint8_t n;

    if (fifo != NULL && count >= 6 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "0x%02"PRIX8, n);
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readX162a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    uint16_t n;

    if (fifo != NULL && count >= 8 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "0x%04"PRIX16, CO_SWAP_16(n));
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readX322a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    uint32_t n;

    if (fifo != NULL && count >= 12 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "0x%08"PRIX32, CO_SWAP_32(n));
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readX642a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    uint64_t n;

    if (fifo != NULL && count >= 20 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "0x%016"PRIX64, CO_SWAP_64(n));
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readI82a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    int8_t n;

    if (fifo != NULL && count >= 6 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "%"PRId8, n);
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readI162a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    int16_t n;

    if (fifo != NULL && count >= 8 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "%"PRId16, CO_SWAP_16(n));
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readI322a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    int32_t n;

    if (fifo != NULL && count >= 13 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "%"PRId32, CO_SWAP_32(n));
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readI642a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    int64_t n;

    if (fifo != NULL && count >= 23 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "%"PRId64, CO_SWAP_64(n));
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readR322a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    float32_t n;

    if (fifo != NULL && count >= 20 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "%g", CO_SWAP_32(n));
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readR642a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    float64_t n;

    if (fifo != NULL && count >= 30 && CO_fifo_getOccupied(fifo) == sizeof(n)) {
        CO_fifo_read(fifo, (char *)&n, sizeof(n), NULL);
        return sprintf(buf, "%g", CO_SWAP_64(n));
    }
    else {
        return CO_fifo_readHex2a(fifo, buf, count, end);
    }
}

size_t CO_fifo_readHex2a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    (void)end;    /* unused */

    size_t len = 0;

    if (fifo != NULL && count > 3) {
        /* Very first write is without leading space */
        if (!fifo->started) {
            char c;
            if(CO_fifo_getc(fifo, &c)) {
                len = sprintf(&buf[0], "%02"PRIX8, (uint8_t)c);
                fifo->started = true;
            }
        }

        while ((len + 3) < count) {
            char c;
            if(!CO_fifo_getc(fifo, &c)) {
                break;
            }
            len += sprintf(&buf[len], " %02"PRIX8, (uint8_t)c);
        }
    }

    return len;
}

size_t CO_fifo_readVs2a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end) {
    size_t len = 0;

    if (fifo != NULL && count > 3) {
        /* Start with '"' */
        if (!fifo->started) {
            buf[len++] = '"';
            fifo->started = true;
        }

        while ((len + 2) < count) {
            char c;
            if(!CO_fifo_getc(fifo, &c)) {
                if (end) {
                    buf[len++] = '"';
                }
                break;
            }
            if (c == '"') {
                buf[len++] = '"';
            }
            buf[len++] = c;
        }
    }

    return len;
}


/******************************************************************************/
size_t CO_fifo_cpyTok2U8(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    char buf[15];
    char closed = -1;
    bool_t err = 0;
    size_t nWr = 0;
    size_t nRd = CO_fifo_readToken(src, buf, sizeof(buf), &closed, &err);
    CO_fifo_st st = (uint8_t)closed;
    if (nRd == 0 || err) st |= CO_fifo_st_errTok;
    else {
        char *sRet;
        uint32_t u32 = strtoul(buf, &sRet, 0);
        if (sRet != strchr(buf, '\0') || u32 > UINT8_MAX) st |= CO_fifo_st_errVal;
        else {
            uint8_t num = (uint8_t) u32;
            nWr = CO_fifo_write(dest, (const char *)&num, sizeof(num), NULL);
            if (nWr != sizeof(num)) st |= CO_fifo_st_errBuf;
        }
    }
    if (status != NULL) *status = st;
    return nWr;
}

size_t CO_fifo_cpyTok2U16(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    char buf[15];
    char closed = -1;
    bool_t err = 0;
    size_t nWr = 0;
    size_t nRd = CO_fifo_readToken(src, buf, sizeof(buf), &closed, &err);
    CO_fifo_st st = (uint8_t)closed;
    if (nRd == 0 || err) st |= CO_fifo_st_errTok;
    else {
        char *sRet;
        uint32_t u32 = strtoul(buf, &sRet, 0);
        if (sRet != strchr(buf, '\0') || u32 > UINT16_MAX) st |= CO_fifo_st_errVal;
        else {
            uint16_t num = CO_SWAP_16((uint16_t) u32);
            nWr = CO_fifo_write(dest, (const char *)&num, sizeof(num), NULL);
            if (nWr != sizeof(num)) st |= CO_fifo_st_errBuf;
        }
    }
    if (status != NULL) *status = st;
    return nWr;
}

size_t CO_fifo_cpyTok2U32(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    char buf[15];
    char closed = -1;
    bool_t err = 0;
    size_t nWr = 0;
    size_t nRd = CO_fifo_readToken(src, buf, sizeof(buf), &closed, &err);
    CO_fifo_st st = (uint8_t)closed;
    if (nRd == 0 || err) st |= CO_fifo_st_errTok;
    else {
        char *sRet;
        uint32_t u32 = strtoul(buf, &sRet, 0);
        if (sRet != strchr(buf, '\0')) st |= CO_fifo_st_errVal;
        else {
            uint32_t num = CO_SWAP_32(u32);
            nWr = CO_fifo_write(dest, (const char *)&num, sizeof(num), NULL);
            if (nWr != sizeof(num)) st |= CO_fifo_st_errBuf;
        }
    }
    if (status != NULL) *status = st;
    return nWr;
}

size_t CO_fifo_cpyTok2U64(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    char buf[25];
    char closed = -1;
    bool_t err = 0;
    size_t nWr = 0;
    size_t nRd = CO_fifo_readToken(src, buf, sizeof(buf), &closed, &err);
    CO_fifo_st st = (uint8_t)closed;
    if (nRd == 0 || err) st |= CO_fifo_st_errTok;
    else {
        char *sRet;
        uint64_t u64 = strtoull(buf, &sRet, 0);
        if (sRet != strchr(buf, '\0')) st |= CO_fifo_st_errVal;
        else {
            uint64_t num = CO_SWAP_64(u64);
            nWr = CO_fifo_write(dest, (const char *)&num, sizeof(num), NULL);
            if (nWr != sizeof(num)) st |= CO_fifo_st_errBuf;
        }
    }
    if (status != NULL) *status = (uint8_t) st;
    return nWr;
}

size_t CO_fifo_cpyTok2I8(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    char buf[15];
    char closed = -1;
    bool_t err = 0;
    size_t nWr = 0;
    size_t nRd = CO_fifo_readToken(src, buf, sizeof(buf), &closed, &err);
    CO_fifo_st st = (uint8_t)closed;
    if (nRd == 0 || err) st |= CO_fifo_st_errTok;
    else {
        char *sRet;
        int32_t i32 = strtol(buf, &sRet, 0);
        if (sRet != strchr(buf, '\0') || i32 < INT8_MIN || i32 > INT8_MAX) {
            st |= CO_fifo_st_errVal;
        } else {
            int8_t num = (int8_t) i32;
            nWr = CO_fifo_write(dest, (const char *)&num, sizeof(num), NULL);
            if (nWr != sizeof(num)) st |= CO_fifo_st_errBuf;
        }
    }
    if (status != NULL) *status = st;
    return nWr;
}

size_t CO_fifo_cpyTok2I16(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    char buf[15];
    char closed = -1;
    bool_t err = 0;
    size_t nWr = 0;
    size_t nRd = CO_fifo_readToken(src, buf, sizeof(buf), &closed, &err);
    CO_fifo_st st = (uint8_t)closed;
    if (nRd == 0 || err) st |= CO_fifo_st_errTok;
    else {
        char *sRet;
        int32_t i32 = strtol(buf, &sRet, 0);
        if (sRet != strchr(buf, '\0') || i32 < INT16_MIN || i32 > INT16_MAX) {
            st |= CO_fifo_st_errVal;
        } else {
            int16_t num = CO_SWAP_16((int16_t) i32);
            nWr = CO_fifo_write(dest, (const char *)&num, sizeof(num), NULL);
            if (nWr != sizeof(num)) st |= CO_fifo_st_errBuf;
        }
    }
    if (status != NULL) *status = st;
    return nWr;
}

size_t CO_fifo_cpyTok2I32(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    char buf[15];
    char closed = -1;
    bool_t err = 0;
    size_t nWr = 0;
    size_t nRd = CO_fifo_readToken(src, buf, sizeof(buf), &closed, &err);
    CO_fifo_st st = (uint8_t)closed;
    if (nRd == 0 || err) st |= CO_fifo_st_errTok;
    else {
        char *sRet;
        int32_t i32 = strtol(buf, &sRet, 0);
        if (sRet != strchr(buf, '\0')) st |= CO_fifo_st_errVal;
        else {
            int32_t num = CO_SWAP_32(i32);
            nWr = CO_fifo_write(dest, (const char *)&num, sizeof(num), NULL);
            if (nWr != sizeof(num)) st |= CO_fifo_st_errBuf;
        }
    }
    if (status != NULL) *status = st;
    return nWr;
}

size_t CO_fifo_cpyTok2I64(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    char buf[25];
    char closed = -1;
    bool_t err = 0;
    size_t nWr = 0;
    size_t nRd = CO_fifo_readToken(src, buf, sizeof(buf), &closed, &err);
    CO_fifo_st st = (uint8_t)closed;
    if (nRd == 0 || err) st |= CO_fifo_st_errTok;
    else {
        char *sRet;
        int64_t i64 = strtoll(buf, &sRet, 0);
        if (sRet != strchr(buf, '\0')) st |= CO_fifo_st_errVal;
        else {
            int64_t num = CO_SWAP_64(i64);
            nWr = CO_fifo_write(dest, (const char *)&num, sizeof(num), NULL);
            if (nWr != sizeof(num)) st |= CO_fifo_st_errBuf;
        }
    }
    if (status != NULL) *status = (uint8_t) st;
    return nWr;
}

size_t CO_fifo_cpyTok2R32(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    char buf[30];
    char closed = -1;
    bool_t err = 0;
    size_t nWr = 0;
    size_t nRd = CO_fifo_readToken(src, buf, sizeof(buf), &closed, &err);
    CO_fifo_st st = (uint8_t)closed;
    if (nRd == 0 || err) st |= CO_fifo_st_errTok;
    else {
        char *sRet;
        float32_t f32 = strtof(buf, &sRet);
        if (sRet != strchr(buf, '\0')) st |= CO_fifo_st_errVal;
        else {
            float32_t num = CO_SWAP_32(f32);
            nWr = CO_fifo_write(dest, (const char *)&num, sizeof(num), NULL);
            if (nWr != sizeof(num)) st |= CO_fifo_st_errBuf;
        }
    }
    if (status != NULL) *status = st;
    return nWr;
}

size_t CO_fifo_cpyTok2R64(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    char buf[40];
    char closed = -1;
    bool_t err = 0;
    size_t nWr = 0;
    size_t nRd = CO_fifo_readToken(src, buf, sizeof(buf), &closed, &err);
    CO_fifo_st st = (uint8_t)closed;
    if (nRd == 0 || err) st |= CO_fifo_st_errTok;
    else {
        char *sRet;
        float64_t f64 = strtof(buf, &sRet);
        if (sRet != strchr(buf, '\0')) st |= CO_fifo_st_errVal;
        else {
            float64_t num = CO_SWAP_64(f64);
            nWr = CO_fifo_write(dest, (const char *)&num, sizeof(num), NULL);
            if (nWr != sizeof(num)) st |= CO_fifo_st_errBuf;
        }
    }
    if (status != NULL) *status = st;
    return nWr;
}

size_t CO_fifo_cpyTok2Hex(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    size_t destSpace, destSpaceStart;
    bool_t finished = false;
    CO_fifo_st st = 0;

    if (dest == NULL || src == NULL) {
        return 0;
    }

    /* get free space of the dest fifo */
    destSpaceStart = destSpace = CO_fifo_getSpace(dest);

    /* repeat until destination space available and no error and not finished
     * and source characters available */
    while (destSpace > 0 && (st & CO_fifo_st_errMask) == 0 && !finished) {
        char c;
        if (!CO_fifo_getc(src, &c)) {
            break;
        }

        if (dest->started == false) {
            /* search for first of two hex digits in token */
            if (isxdigit((int)c) != 0) {
                /* first hex digit is known */
                dest->aux = c;
                dest->started = true;
            }
            else if (c == DELIM_COMMAND) {
                /* newline found, finish */
                st |= CO_fifo_st_closed;
                finished = true;
            }
            else if (c == DELIM_COMMENT) {
                /* comment found, clear line and finish */
                while (CO_fifo_getc(src, &c)) {
                    if (c == DELIM_COMMAND) {
                        st |= CO_fifo_st_closed;
                        break;
                    }
                }
                finished = true;
            }
            else if (isgraph((int)c) != 0) {
                /* printable character, not hex digit, error */
                st |= CO_fifo_st_errTok;
            }
            /* else just skip empty space */
        }
        else {
            /* one hex digit is known, what is next */
            if (isxdigit((int)c) != 0) {
                /* two hex digits are known */
                char s[3];
                int32_t num;
                s[0] = (char) dest->aux; s[1] = c, s[2] = 0;
                num = strtol(s, NULL, 16);
                /* copy the character */
                CO_fifo_putc(dest, (char) num);
                destSpace--;
            }
            else if (isgraph((int)c) != 0 && c != DELIM_COMMENT) {
                /* printable character, not hex digit, error */
                st |= CO_fifo_st_errTok;
            }
            else {
                /* this is space or comment, single hex digit is known */
                char s[2];
                int32_t num;
                s[0] = (char) dest->aux; s[1] = 0;
                num = strtol(s, NULL, 16);
                /* copy the character */
                CO_fifo_putc(dest, (char) num);
                destSpace--;
                if (c == DELIM_COMMAND) {
                    /* newline found, finish */
                    st |= CO_fifo_st_closed;
                    finished = true;
                }
                else if (c == DELIM_COMMENT) {
                    /* comment found, clear line and finish */
                    while (CO_fifo_getc(src, &c)) {
                        if (c == DELIM_COMMAND) {
                            st |= CO_fifo_st_closed;
                            break;
                        }
                    }
                    finished = true;
                }
            }
            dest->started = false;
        }
    }

    if (!finished) {
        st |= CO_fifo_st_partial;
    }

    if (status != NULL) *status = st;

    return destSpaceStart - destSpace;
}

size_t CO_fifo_cpyTok2Vs(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status) {
    size_t destSpace, destSpaceStart;
    bool_t finished = false;
    CO_fifo_st st = 0;

    if (dest == NULL || src == NULL) {
        return 0;
    }

    /* get free space of the dest fifo */
    destSpaceStart = destSpace = CO_fifo_getSpace(dest);

    /* is this the first write into dest? */
    if (!dest->started) {
        /* aux variable is used as state here */
        dest->aux = 0;
        dest->started = true;
    }

    /* repeat until destination space available and no error and not finished
     * and source characters available */
    while (destSpace > 0 && (st & CO_fifo_st_errMask) == 0 && !finished) {
        char c;
        if (!CO_fifo_getc(src, &c)) {
            break;
        }

        switch (dest->aux) {
        case 0: /* beginning of the string, first write into dest */
            if (isgraph((int)c) == 0 || c == DELIM_COMMENT) {
                if (CO_fifo_trimSpaces(src)) {
                    /* newline found without string, this is an error */
                    st |= CO_fifo_st_errTok;
                }
            }
            else if (c == '"') {
                /* Indicated beginning of the string, skip this character. */
                dest->aux = 1;
            }
            else {
                /* this must be a single word string without '"' */
                /* copy the character */
                CO_fifo_putc(dest, c);
                destSpace--;
                dest->aux = 2;
            }
            break;

        case 1: /* inside string, quoted string */
        case 2: /* inside string, single word, no quotes */
            if (c == '"') {
                /* double qoute found, this may be end of the string or escaped
                 * double quote (with two double quotes) */
                dest->aux += 2;
            }
            else if (isgraph((int)c) == 0 && dest->aux == 2) {
                /* end of single word string */
                if (c == DELIM_COMMAND) {
                    st |= CO_fifo_st_closed;
                }
                else if (CO_fifo_trimSpaces(src)) {
                    st |= CO_fifo_st_closed;
                }
                finished = true;
            }
            else if (c == DELIM_COMMAND) {
                /* no closing quote, error */
                st |= CO_fifo_st_errTok;
            }
            else {
                /* copy the character */
                CO_fifo_putc(dest, c);
                destSpace--;
            }
            break;

        case 3: /* previous was double quote, parsing quoted string */
        case 4: /* previous was double quote, parsing no quoted word */
            if (c == '"') {
                /* escaped double quote, copy the character and continue */
                CO_fifo_putc(dest, c);
                destSpace--;
                dest->aux -= 2;
            }
            else {
                /* previous character was closing double quote */
                if (dest->aux == 4) {
                    /* no opening double quote, syntax error */
                    st |= CO_fifo_st_errTok;
                }
                else {
                    if (isgraph((int)c) == 0) {
                        /* string finished */
                        if (c == DELIM_COMMAND) {
                            st |= CO_fifo_st_closed;
                        }
                        else if (CO_fifo_trimSpaces(src)) {
                            st |= CO_fifo_st_closed;
                        }
                        finished = true;
                    }
                    else {
                        /* space must follow closing double quote, error */
                        st |= CO_fifo_st_errTok;
                    }
                }
            }
            break;

        default: /* internal error */
            st |= CO_fifo_st_errInt;
            break;
        }
    }

    if (!finished) {
        st |= CO_fifo_st_partial;
    }

    if (status != NULL) *status = st;

    return destSpaceStart - destSpace;
}

#endif /* (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_DATATYPES */

#endif /* (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ENABLE */
