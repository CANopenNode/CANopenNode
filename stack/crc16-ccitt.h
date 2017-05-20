/**
 * Calculation of CRC 16 CCITT polynomial.
 *
 * @file        crc16-ccitt.h
 * @ingroup     CO_crc16_ccitt
 * @author      Lammert Bies
 * @author      Janez Paternoster
 * @copyright   2012 - 2013 Janez Paternoster
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
