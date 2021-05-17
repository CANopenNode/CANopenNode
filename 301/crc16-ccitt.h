/**
 * Calculation of CRC 16 CCITT polynomial.
 *
 * @file        crc16-ccitt.h
 * @ingroup     CO_crc16_ccitt
 * @author      Lammert Bies
 * @author      Janez Paternoster
 * @copyright   2012 - 2020 Janez Paternoster
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

#ifndef CRC16_CCITT_H
#define CRC16_CCITT_H

#include "301/CO_driver.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_CRC16
#define CO_CONFIG_CRC16 (0)
#endif

#if ((CO_CONFIG_CRC16) & CO_CONFIG_CRC16_ENABLE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_crc16_ccitt CRC 16 CCITT
 * Calculation of CRC 16 CCITT polynomial.
 *
 * @ingroup CO_CANopen_301
 * @{
 *
 * Equation:
 *
 * `x^16 + x^12 + x^5 + 1`
 */


/**
 * Update crc16_ccitt variable with one data byte
 *
 * This function updates crc variable for one data byte using crc16 ccitt
 * algorithm.
 *
 * @param [in,out] crc Externally defined variable for CRC checksum. Before
 * start of new CRC calculation, variable must be initialized (zero for xmodem).
 * @param chr One byte of data
 */
void crc16_ccitt_single(uint16_t *crc, const uint8_t chr);


/**
 * Calculate CRC sum on block of data.
 *
 * @param block Pointer to block of data.
 * @param blockLength Length of data in bytes;
 * @param crc Initial value (zero for xmodem). If block is split into
 * multiple segments, previous CRC is used as initial.
 *
 * @return Calculated CRC.
 */
uint16_t crc16_ccitt(const uint8_t block[],
                     size_t blockLength,
                     uint16_t crc);


/** @} */ /* CO_crc16_ccitt */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* (CO_CONFIG_CRC16) & CO_CONFIG_CRC16_ENABLE */

#endif /* CRC16_CCITT_H */
