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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_crc16_ccitt CRC 16 CCITT
 * @ingroup CO_CANopen
 * @{
 *
 * Calculation of CRC 16 CCITT polynomial.
 *
 * Equation:
 *
 * `x^16 + x^12 + x^5 + 1`
 */


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
#ifdef CO_USE_OWN_CRC16
extern
#endif
unsigned short crc16_ccitt(
        const unsigned char     block[],
        unsigned int            blockLength,
        unsigned short          crc);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
