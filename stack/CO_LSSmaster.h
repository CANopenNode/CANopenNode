/**
 * CANopen LSS Master/Slave protocol.
 *
 * @file        CO_LSSmaster.h
 * @ingroup     CO_LSS
 * @author      Martin Wagner
 * @copyright   2017 Neuberger Gebäudeautomation GmbH
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


#ifndef CO_LSSmaster_H
#define CO_LSSmaster_H

#ifdef __cplusplus
extern "C" {
#endif

#include "CO_LSS.h"

/**
 * @addtogroup CO_LSS
 * @{
 */

/**
 * LSS master object.
 */
typedef struct{
    uint16_t              timeout;          /**< LSS response timeout in ms */

    uint8_t               lssState;         /**< #CO_LSS_state_t of the currently active slave */
    CO_LSS_address_t      lssAddress;       /**< #CO_LSS_address_t of the currently active slave */

}CO_LSSmaster_t;


/**
 * Initialize LSS object.
 *
 * Function must be called in the communication reset section. todo?
 * 
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT. todo
 */
CO_ReturnError_t CO_LSSmaster_init(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeout);

/**
 * Change LSS master timeout
 *
 * On LSS, a "negative ack" is signaled by the slave not answering. Because of
 * that, a low timeout value can significantly increase initialization speed in
 * some cases (e.g. fastscan). However, as soon as there is activity on the bus,
 * LSS messages can be delayed because of their high COB (see #CO_Default_CAN_ID_t).
 *
 * @param LSSmaster This object.
 * @param timeout
 */
void CO_LSSmaster_changeTimeout(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeout);

/**
 * Request LSS switch mode
 *
 * @param LSSmaster This object.
 * @param command switch mode command
 * @param lssAddress LSS target address
 * @return todo
 */
CO_ReturnError_t CO_LSSmaster_switchMode(
        CO_LSSmaster_t         *LSSmaster,
        CO_LSS_switch_t         command,
        CO_LSS_address_t        lssAddress);

/**
 * Request LSS configuration
 *
 * @param LSSmaster This object.
 * @param command config command
 * @return todo
 */
CO_ReturnError_t CO_LSSmaster_configure(
        CO_LSSmaster_t         *LSSmaster,
        CO_LSS_config_t         command);

/**
 * Request LSS inquiry
 *
 * @param LSSmaster This object.
 * @param command inquiry command
 * @return todo
 */
CO_ReturnError_t CO_LSSmaster_inquire(
        CO_LSSmaster_t         *LSSmaster,
        CO_LSS_config_t         command);

/**
 * Request LSS identification
 *
 * @param LSSmaster This object.
 * @param command identification command
 * @return todo
 */
CO_ReturnError_t CO_LSSmaster_identifify(
        CO_LSSmaster_t         *LSSmaster,
        CO_LSS_config_t         command);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
