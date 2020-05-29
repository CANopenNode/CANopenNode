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

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#if CO_CONFIG_FIFO_ASCII_COMMANDS == 1
#include <stdio.h>
#include <inttypes.h>

/* Non-graphical character for command delimiter */
#define DELIM_COMMAND '\n'
/* Graphical character for comment delimiter */
#define DELIM_COMMENT '#'
#endif /* CO_CONFIG_FIFO_ASCII_COMMANDS == 1 */


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


#if CO_CONFIG_FIFO_CRC16_CCITT == 1
/* CRC calculation algorithm
 * Author of the original crc16 ccitt C code is Lammert Bies.
 *
 * CRC table is calculated by the following algorithm:
 *
 * void CO_fifo_crc16_ccitt_table_init(void) {
 *     uint16_t i, j;
 *     for (i = 0; i < 256; i++) {
 *         uint16_t crc = 0;
 *         uint16_t c = i << 8;
 *         for(j = 0; j < 8; j++) {
 *             if((crc ^ c) & 0x8000) crc = (crc << 1) ^ 0x1021;
 *             else                   crc = crc << 1;
 *             c = c << 1;
 *         }
 *         CO_fifo_crc16_ccitt_table[i] = crc;
 *     }
 * } */
static const uint16_t CO_fifo_crc16_ccitt_table[256] = {
    0x0000U, 0x1021U, 0x2042U, 0x3063U, 0x4084U, 0x50A5U, 0x60C6U, 0x70E7U,
    0x8108U, 0x9129U, 0xA14AU, 0xB16BU, 0xC18CU, 0xD1ADU, 0xE1CEU, 0xF1EFU,
    0x1231U, 0x0210U, 0x3273U, 0x2252U, 0x52B5U, 0x4294U, 0x72F7U, 0x62D6U,
    0x9339U, 0x8318U, 0xB37BU, 0xA35AU, 0xD3BDU, 0xC39CU, 0xF3FFU, 0xE3DEU,
    0x2462U, 0x3443U, 0x0420U, 0x1401U, 0x64E6U, 0x74C7U, 0x44A4U, 0x5485U,
    0xA56AU, 0xB54BU, 0x8528U, 0x9509U, 0xE5EEU, 0xF5CFU, 0xC5ACU, 0xD58DU,
    0x3653U, 0x2672U, 0x1611U, 0x0630U, 0x76D7U, 0x66F6U, 0x5695U, 0x46B4U,
    0xB75BU, 0xA77AU, 0x9719U, 0x8738U, 0xF7DFU, 0xE7FEU, 0xD79DU, 0xC7BCU,
    0x48C4U, 0x58E5U, 0x6886U, 0x78A7U, 0x0840U, 0x1861U, 0x2802U, 0x3823U,
    0xC9CCU, 0xD9EDU, 0xE98EU, 0xF9AFU, 0x8948U, 0x9969U, 0xA90AU, 0xB92BU,
    0x5AF5U, 0x4AD4U, 0x7AB7U, 0x6A96U, 0x1A71U, 0x0A50U, 0x3A33U, 0x2A12U,
    0xDBFDU, 0xCBDCU, 0xFBBFU, 0xEB9EU, 0x9B79U, 0x8B58U, 0xBB3BU, 0xAB1AU,
    0x6CA6U, 0x7C87U, 0x4CE4U, 0x5CC5U, 0x2C22U, 0x3C03U, 0x0C60U, 0x1C41U,
    0xEDAEU, 0xFD8FU, 0xCDECU, 0xDDCDU, 0xAD2AU, 0xBD0BU, 0x8D68U, 0x9D49U,
    0x7E97U, 0x6EB6U, 0x5ED5U, 0x4EF4U, 0x3E13U, 0x2E32U, 0x1E51U, 0x0E70U,
    0xFF9FU, 0xEFBEU, 0xDFDDU, 0xCFFCU, 0xBF1BU, 0xAF3AU, 0x9F59U, 0x8F78U,
    0x9188U, 0x81A9U, 0xB1CAU, 0xA1EBU, 0xD10CU, 0xC12DU, 0xF14EU, 0xE16FU,
    0x1080U, 0x00A1U, 0x30C2U, 0x20E3U, 0x5004U, 0x4025U, 0x7046U, 0x6067U,
    0x83B9U, 0x9398U, 0xA3FBU, 0xB3DAU, 0xC33DU, 0xD31CU, 0xE37FU, 0xF35EU,
    0x02B1U, 0x1290U, 0x22F3U, 0x32D2U, 0x4235U, 0x5214U, 0x6277U, 0x7256U,
    0xB5EAU, 0xA5CBU, 0x95A8U, 0x8589U, 0xF56EU, 0xE54FU, 0xD52CU, 0xC50DU,
    0x34E2U, 0x24C3U, 0x14A0U, 0x0481U, 0x7466U, 0x6447U, 0x5424U, 0x4405U,
    0xA7DBU, 0xB7FAU, 0x8799U, 0x97B8U, 0xE75FU, 0xF77EU, 0xC71DU, 0xD73CU,
    0x26D3U, 0x36F2U, 0x0691U, 0x16B0U, 0x6657U, 0x7676U, 0x4615U, 0x5634U,
    0xD94CU, 0xC96DU, 0xF90EU, 0xE92FU, 0x99C8U, 0x89E9U, 0xB98AU, 0xA9ABU,
    0x5844U, 0x4865U, 0x7806U, 0x6827U, 0x18C0U, 0x08E1U, 0x3882U, 0x28A3U,
    0xCB7DU, 0xDB5CU, 0xEB3FU, 0xFB1EU, 0x8BF9U, 0x9BD8U, 0xABBBU, 0xBB9AU,
    0x4A75U, 0x5A54U, 0x6A37U, 0x7A16U, 0x0AF1U, 0x1AD0U, 0x2AB3U, 0x3A92U,
    0xFD2EU, 0xED0FU, 0xDD6CU, 0xCD4DU, 0xBDAAU, 0xAD8BU, 0x9DE8U, 0x8DC9U,
    0x7C26U, 0x6C07U, 0x5C64U, 0x4C45U, 0x3CA2U, 0x2C83U, 0x1CE0U, 0x0CC1U,
    0xEF1FU, 0xFF3EU, 0xCF5DU, 0xDF7CU, 0xAF9BU, 0xBFBAU, 0x8FD9U, 0x9FF8U,
    0x6E17U, 0x7E36U, 0x4E55U, 0x5E74U, 0x2E93U, 0x3EB2U, 0x0ED1U, 0x1EF0U
};

/*
 * Update crc16_ccitt variable with one data byte
 *
 * This function updates crc variable for one data byte using crc16 ccitt
 * algorithm. Function is used inside CO_fifo_write() and CO_fifo_altRead().
 * User can also provide own function for crc calculation.
 *
 * @param [in,out] crc Externally defined variable for CRC checksum
 * @param chr One byte of data
 */
static inline void CO_fifo_crc16_ccitt(uint16_t *crc, const char chr) {
    uint8_t tmp = (uint8_t)(*crc >> 8) ^ (uint8_t)chr;
    *crc = (*crc << 8) ^ CO_fifo_crc16_ccitt_table[tmp];
}
#endif /* CO_CONFIG_FIFO_CRC16_CCITT == 1 */


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

#if CO_CONFIG_FIFO_CRC16_CCITT > 0
        if (crc != NULL) {
            CO_fifo_crc16_ccitt(crc, *buf);
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

#if CO_CONFIG_FIFO_ASCII_COMMANDS == 1
        /* is delimiter? */
        if (eof != NULL && c == DELIM_COMMAND) {
            *eof = true;
            break;
        }
#endif
    }

    return count - i;
}


#if CO_CONFIG_FIFO_ALT_READ == 1
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
#if CO_CONFIG_FIFO_CRC16_CCITT > 0
            CO_fifo_crc16_ccitt(crc, *bufSrc);
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
#endif /* CO_CONFIG_FIFO_ALT_READ == 1 */


#if CO_CONFIG_FIFO_ASCII_COMMANDS == 1
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
            else if (isgraph(c) != 0) {
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
                if (isgraph(*c) != 0) {
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
                if (isgraph(*c) != 0) {
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
                if (isgraph(*c) != 0) {
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
#endif /* #if CO_CONFIG_FIFO_ASCII_COMMANDS == 1 */


#if CO_CONFIG_FIFO_ASCII_DATATYPES == 1
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
            if (isxdigit(c) != 0) {
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
            else if (isgraph(c) != 0) {
                /* printable character, not hex digit, error */
                st |= CO_fifo_st_errTok;
            }
            /* else just skip empty space */
        }
        else {
            /* one hex digit is known, what is next */
            if (isxdigit(c) != 0) {
                /* two hex digits are known */
                char s[3];
                int32_t num;
                s[0] = (char) dest->aux; s[1] = c, s[2] = 0;
                num = strtol(s, NULL, 16);
                /* copy the character */
                CO_fifo_putc(dest, (char) num);
                destSpace--;
            }
            else if (isgraph(c) != 0 && c != DELIM_COMMENT) {
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
            if (isgraph(c) == 0 || c == DELIM_COMMENT) {
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
            else if (isgraph(c) == 0 && dest->aux == 2) {
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
                    if (isgraph(c) == 0) {
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

#endif /* CO_CONFIG_FIFO_ASCII_DATATYPES == 1 */
