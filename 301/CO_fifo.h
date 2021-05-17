/**
 * FIFO circular buffer
 *
 * @file        CO_fifo.h
 * @ingroup     CO_CANopen_301_fifo
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

#ifndef CO_FIFO_H
#define CO_FIFO_H

#include "301/CO_driver.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_FIFO
#define CO_CONFIG_FIFO (0)
#endif

#if ((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ENABLE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_CANopen_301_fifo FIFO circular buffer
 * FIFO circular buffer for continuous data flow.
 *
 * @ingroup CO_CANopen_301
 * @{
 *
 * FIFO is organized as circular buffer with predefined capacity. It must be
 * initialized by CO_fifo_init(). Functions are not not thread safe.
 *
 * It can be used as general purpose FIFO circular buffer for any data. Data can
 * be written by CO_fifo_write() and read by CO_fifo_read() functions.
 *
 * Buffer has additional functions for usage with CiA309-3 standard. It acts as
 * circular buffer for storing ascii commands and fetching tokens from them.
 */

/**
 * Fifo object
 */
typedef struct {
    /** Buffer of size bufSize. Initialized by CO_fifo_init() */
    uint8_t *buf;
    /** Initialized by CO_fifo_init() */
    size_t bufSize;
    /** Location in the buffer, which will be next written. */
    size_t writePtr;
    /** Location in the buffer, which will be next read. */
    size_t readPtr;
#if ((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ALT_READ) || defined CO_DOXYGEN
    /** Location in the buffer, which will be next read. */
    size_t altReadPtr;
#endif
#if ((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_DATATYPES) || defined CO_DOXYGEN
    /** helper variable, set to false in CO_fifo_reset(), used in some
     * functions. */
    bool_t started;
    /** auxiliary variable, used in some functions. */
    uint32_t aux;
#endif
} CO_fifo_t;



/**
 * Initialize fifo object
 *
 * @param fifo This object will be initialized
 * @param buf Pointer to externally defined buffer
 * @param bufSize Size of the above buffer. Usable size of the buffer will be
 * one byte less than bufSize, it is used for operation of the circular buffer.
 */
void CO_fifo_init(CO_fifo_t *fifo, uint8_t *buf, size_t bufSize);


/**
 * Reset fifo object, make it empty
 *
 * @param fifo This object
 */
static inline void CO_fifo_reset(CO_fifo_t *fifo) {
    if (fifo != NULL) {
        fifo->readPtr = 0;
        fifo->writePtr = 0;
#if (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_DATATYPES
        fifo->started = false;
#endif
    }

    return;
}


/**
 * Purge all data in fifo object, keep other properties
 *
 * @param fifo This object
 *
 * @return true, if data were purged or false, if fifo were already empty
 */
static inline bool_t CO_fifo_purge(CO_fifo_t *fifo) {
    if (fifo != NULL && fifo->readPtr != fifo->writePtr) {
        fifo->readPtr = 0;
        fifo->writePtr = 0;
        return true;
    }

    return false;
}


/**
 * Get free buffer space in CO_fifo_t object
 *
 * @param fifo This object
 *
 * @return number of available bytes
 */
static inline size_t CO_fifo_getSpace(CO_fifo_t *fifo) {
    int sizeLeft = (int)fifo->readPtr - (int)fifo->writePtr - 1;
    if (sizeLeft < 0) {
        sizeLeft += (int)fifo->bufSize;
    }

    return (size_t) sizeLeft;
}


/**
 * Get size of data inside CO_fifo_t buffer object
 *
 * @param fifo This object
 *
 * @return number of occupied bytes
 */
static inline size_t CO_fifo_getOccupied(CO_fifo_t *fifo) {
    int sizeOccupied = (int)fifo->writePtr - (int)fifo->readPtr;
    if (sizeOccupied < 0) {
        sizeOccupied += (int)fifo->bufSize;
    }

    return (size_t) sizeOccupied;
}


/**
 * Put one character into CO_fifo_t buffer object
 *
 * @param fifo This object
 * @param c Character to put
 *
 * @return true, if write was successful (enough space in fifo buffer)
 */
static inline bool_t CO_fifo_putc(CO_fifo_t *fifo, const uint8_t c) {
    if (fifo != NULL && fifo->buf != NULL) {
        size_t writePtrNext = fifo->writePtr + 1;
        if (writePtrNext != fifo->readPtr &&
            !(writePtrNext == fifo->bufSize && fifo->readPtr == 0))
        {
            fifo->buf[fifo->writePtr] = c;
            fifo->writePtr = (writePtrNext == fifo->bufSize) ? 0 : writePtrNext;
            return true;
        }
    }
    return false;
}


/**
 * Put one character into CO_fifo_t buffer object
 *
 * Overwrite old characters, if run out of space
 *
 * @param fifo This object
 * @param c Character to put
 */
static inline void CO_fifo_putc_ov(CO_fifo_t *fifo, const uint8_t c) {
    if (fifo != NULL && fifo->buf != NULL) {
        fifo->buf[fifo->writePtr] = c;

        if (++fifo->writePtr == fifo->bufSize) fifo->writePtr = 0;
        if (fifo->readPtr == fifo->writePtr) {
            if (++fifo->readPtr == fifo->bufSize) fifo->readPtr = 0;
        }
    }
}


/**
 * Get one character from CO_fifo_t buffer object
 *
 * @param fifo This object
 * @param buf Buffer of length one byte, where character will be copied
 *
 * @return true, if read was successful (non-empty fifo buffer)
 */
static inline bool_t CO_fifo_getc(CO_fifo_t *fifo, uint8_t *buf) {
    if (fifo != NULL && buf != NULL && fifo->readPtr != fifo->writePtr) {
        *buf = fifo->buf[fifo->readPtr];
        if (++fifo->readPtr == fifo->bufSize) {
            fifo->readPtr = 0;
        }
        return true;
    }
    return false;
}


/**
 * Write data into CO_fifo_t object.
 *
 * This function copies data from buf into internal buffer of CO_fifo_t
 * object. Function returns number of bytes successfully copied.
 * If there is not enough space in destination, not all bytes will be copied.
 *
 * @param fifo This object
 * @param buf Buffer which will be copied
 * @param count Number of bytes in buf
 * @param [in,out] crc Externally defined variable for CRC checksum, ignored if
 * NULL
 *
 * @return number of bytes actually written.
 */
size_t CO_fifo_write(CO_fifo_t *fifo,
                     const uint8_t *buf,
                     size_t count,
                     uint16_t *crc);


/**
 * Read data from CO_fifo_t object.
 *
 * This function copies data from internal buffer of CO_fifo_t object into
 * buf. Function returns number of bytes successfully copied. Function also
 * writes true into eof argument, if command delimiter character is reached.
 *
 * @param fifo This object
 * @param buf Buffer into which data will be copied
 * @param count Copy up to count bytes into buffer
 * @param [out] eof If different than NULL, then search for delimiter character.
 * If found, then read up to (including) that character and set *eof to true.
 * Otherwise set *eof to false.
 *
 * @return number of bytes actually read.
 */
size_t CO_fifo_read(CO_fifo_t *fifo, uint8_t *buf, size_t count, bool_t *eof);


#if ((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ALT_READ) || defined CO_DOXYGEN
/**
 * Initializes alternate read with #CO_fifo_altRead
 *
 * @param fifo This object
 * @param offset Offset in bytes from original read pointer
 *
 * @return same as offset or lower, if there is not enough data.
 */
size_t CO_fifo_altBegin(CO_fifo_t *fifo, size_t offset);


/**
 * Ends alternate read with #CO_fifo_altRead and calculate crc checksum
 *
 * @param fifo This object
 * @param [in,out] crc Externally defined variable for CRC checksum, ignored if
 * NULL
 */
void CO_fifo_altFinish(CO_fifo_t *fifo, uint16_t *crc);


/**
 * Get alternate size of remaining data, see #CO_fifo_altRead
 *
 * @param fifo This object
 *
 * @return number of occupied bytes.
 */
static inline size_t CO_fifo_altGetOccupied(CO_fifo_t *fifo) {
    int sizeOccupied = (int)fifo->writePtr - (int)fifo->altReadPtr;
    if (sizeOccupied < 0) {
        sizeOccupied += (int)fifo->bufSize;
    }

    return (size_t) sizeOccupied;
}


/**
 * Alternate read data from CO_fifo_t object.
 *
 * This function is similar as CO_fifo_read(), but uses alternate read pointer
 * inside circular buffer. It reads data from the buffer and data remains in it.
 * This function uses alternate read pointer and keeps original read pointer
 * unchanged. Before using this function, alternate read pointer must be
 * initialized with CO_fifo_altBegin(). CO_fifo_altFinish() sets original read
 * pointer to alternate read pointer and so empties the buffer.
 *
 * @param fifo This object
 * @param buf Buffer into which data will be copied
 * @param count Copy up to count bytes into buffer
 *
 * @return number of bytes actually read.
 */
size_t CO_fifo_altRead(CO_fifo_t *fifo, uint8_t *buf, size_t count);
#endif /* #if (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ALT_READ */


#if ((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_COMMANDS) || defined CO_DOXYGEN
/**
 * Search command inside FIFO
 *
 * If there are some data inside the FIFO, then search for command delimiter.
 *
 * If command is long, then in the buffer may not be enough space for it.
 * In that case buffer is full and no delimiter is present. Function then
 * returns true and command should be processed for the starting tokens.
 * Buffer can later be refilled multiple times, until command is closed by
 * command delimiter.
 *
 * If this function returns different than 0, then buffer is usually read
 * by multiple CO_fifo_readToken() calls. If reads was successful, then
 * delimiter is reached and fifo->readPtr is set after the command. If any
 * CO_fifo_readToken() returns nonzero *err, then there is an error and command
 * should be cleared. All this procedure must be implemented inside single
 * function call.
 *
 * @param fifo This object.
 * @param clear If true, then command will be cleared from the
 * buffer. If there is no delimiter, buffer will be cleared entirely.
 *
 * @return true if command with delimiter found or buffer full.
 */
bool_t CO_fifo_CommSearch(CO_fifo_t *fifo, bool_t clear);


/**
 * Trim spaces inside FIFO
 *
 * Function removes all non graphical characters and comments from fifo
 * buffer. It stops on first graphical character or on command delimiter (later
 * is also removed).
 *
 * @param fifo This object.
 * @param [in, out] insideComment if set to true as input, it skips all
 * characters and searches only for delimiter. As output it is set to true, if
 * fifo is empty, is inside comment and command delimiter is not found.
 *
 * @return true if command delimiter was found.
 */
bool_t CO_fifo_trimSpaces(CO_fifo_t *fifo, bool_t *insideComment);


/**
 * Get token from FIFO buffer
 *
 * Function search FIFO buffer for token. Token is string of only graphical
 * characters. Graphical character is any printable character except space with
 * acsii code within limits: 0x20 < code < 0x7F (see isgraph() function).
 *
 * If token is found, then copy it to the buf, if count is large enough. On
 * success also set readPtr to point to the next graphical character.
 *
 * Each token must have at least one empty space after it (space, command
 * delimiter, '\0', etc.). Delimiter must not be graphical character.
 *
 * If comment delimiter (delimComment, see #CO_fifo_init) character is found,
 * then all string till command delimiter (delimCommand, see #CO_fifo_init) will
 * be erased from the buffer.
 *
 * See also #CO_fifo_CommSearch().
 *
 * @param fifo This object.
 * @param buf Buffer into which data will be copied. Will be terminated by '\0'.
 * @param count Copy up to count bytes into buffer
 * @param [in,out] closed This is input/output variable. Not used if NULL.
 * - As output variable it is set to 1, if command delimiter (delimCommand,
 *   see #CO_fifo_init) is found after the token and set to 0 otherwise.
 * - As input variable it is used for verifying error condition:
 *  - *closed = 0: Set *err to true if token is empty or command delimiter
 *                 is found.
 *  - *closed = 1: Set *err to true if token is empty or command delimiter
 *                 is NOT found.
 *  - *closed = any other value: No checking of token size or command delimiter.
 * @param [out] err If not NULL, it is set to true if token is larger than buf
 * or in matching combination in 'closed' argument. If it is already true, then
 * function returns immediately.
 *
 * @return Number of bytes read.
 */
size_t CO_fifo_readToken(CO_fifo_t *fifo,
                         char *buf,
                         size_t count,
                         int8_t *closed,
                         bool_t *err);
#endif /* (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_COMMANDS */


#if ((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_DATATYPES) || defined CO_DOXYGEN
/**
 * Read uint8_t variable from fifo and output as ascii string.
 *
 * @param fifo This object.
 * @param buf Buffer into which ascii string will be copied.
 * @param count Available count of bytes inside the buf.
 * @param end True indicates, that fifo contains last bytes of data.
 *
 * @return Number of ascii bytes written into buf.
 */
size_t CO_fifo_readU82a (CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read uint16_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readU162a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read uint32_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readU322a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read uint64_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readU642a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read uint8_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readX82a (CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read uint16_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readX162a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read uint32_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readX322a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read uint64_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readX642a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read int8_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readI82a (CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read int16_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readI162a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read int32_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readI322a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read int64_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readI642a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read float32_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readR322a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read float64_t variable from fifo as ascii string, see CO_fifo_readU82a */
size_t CO_fifo_readR642a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read data from fifo and output as space separated two digit ascii string,
 * see also CO_fifo_readU82a */
size_t CO_fifo_readHex2a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read data from fifo and output as visible string. A visible string is
 * enclosed with double quotes. If a double quote is used within the string,
 * the quotes are escaped by a second quotes, e.g. “Hello “”World””, CANopen
 * is great”. UTF-8 characters and also line breaks works with this function.
 * Function removes all NULL and CR characters from output string.
 * See also CO_fifo_readU82a */
size_t CO_fifo_readVs2a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);
/** Read data from fifo and output as mime-base64 encoded string. Encoding is as
 * specified in RFC 2045, without CR-LF, but one long string. See also
 * CO_fifo_readU82a */
size_t CO_fifo_readB642a(CO_fifo_t *fifo, char *buf, size_t count, bool_t end);


/** Bitfields for status argument from CO_fifo_cpyTok2U8 function and similar */
typedef enum {
    /** Bit is set, if command delimiter is reached in src */
    CO_fifo_st_closed  = 0x01U,
    /** Bit is set, if copy was partial and more data are available. If unset
     * and no error, then all data was successfully copied. */
    CO_fifo_st_partial = 0x02U,
    /** Bit is set, if no valid token found */
    CO_fifo_st_errTok  = 0x10U,
    /** Bit is set, if value is not valid or out of limits */
    CO_fifo_st_errVal  = 0x20U,
    /** Bit is set, if destination buffer is to small */
    CO_fifo_st_errBuf  = 0x40U,
    /** Bit is set, if internal error */
    CO_fifo_st_errInt  = 0x80U,
    /** Bitmask for error bits */
    CO_fifo_st_errMask = 0xF0U
} CO_fifo_st;

/**
 * Read ascii string from src fifo and copy as uint8_t variable to dest fifo.
 *
 * @param dest destination fifo buffer object.
 * @param src source fifo buffer object.
 * @param [out] status bitfield of the CO_fifo_st type.
 *
 * @return Number of bytes written into dest.
 */
size_t CO_fifo_cpyTok2U8 (CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Copy ascii string to uint16_t variable, see CO_fifo_cpyTok2U8 */
size_t CO_fifo_cpyTok2U16(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Copy ascii string to uint32_t variable, see CO_fifo_cpyTok2U8 */
size_t CO_fifo_cpyTok2U32(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Copy ascii string to uint64_t variable, see CO_fifo_cpyTok2U8 */
size_t CO_fifo_cpyTok2U64(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Copy ascii string to int8_t variable, see CO_fifo_cpyTok2U8 */
size_t CO_fifo_cpyTok2I8 (CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Copy ascii string to int16_t variable, see CO_fifo_cpyTok2U8 */
size_t CO_fifo_cpyTok2I16(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Copy ascii string to int32_t variable, see CO_fifo_cpyTok2U8 */
size_t CO_fifo_cpyTok2I32(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Copy ascii string to int64_t variable, see CO_fifo_cpyTok2U8 */
size_t CO_fifo_cpyTok2I64(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Copy ascii string to float32_t variable, see CO_fifo_cpyTok2U8 */
size_t CO_fifo_cpyTok2R32(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Copy ascii string to float64_t variable, see CO_fifo_cpyTok2U8 */
size_t CO_fifo_cpyTok2R64(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Copy bytes written as two hex digits into to data. Bytes may be space
 * separated. See CO_fifo_cpyTok2U8 for parameters. */
size_t CO_fifo_cpyTok2Hex(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Copy visible string to data. A visible string must be enclosed with double
 * quotes, if it contains space. If a double quote is used within the string,
 * the quotes are escaped by a second quotes. Input string can not contain
 * newline characters. See CO_fifo_cpyTok2U8 */
size_t CO_fifo_cpyTok2Vs(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);
/** Read ascii mime-base64 encoded string from src fifo and copy as binary data
 * to dest fifo. Encoding is as specified in RFC 2045, without CR-LF, but one
 * long string in single line. See also CO_fifo_readU82a */
size_t CO_fifo_cpyTok2B64(CO_fifo_t *dest, CO_fifo_t *src, CO_fifo_st *status);

#endif /* (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_DATATYPES */

/** @} */ /* CO_CANopen_301_fifo */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* (CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ENABLE */

#endif /* CO_FIFO_H */
