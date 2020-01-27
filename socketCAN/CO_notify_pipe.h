/**
 * CAN module object for Linux socketCAN.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_notify_pipe.h
 * @ingroup     CO_driver
 * @author      Martin Wagner
 * @copyright   2017 - 2020 Neuberger Gebaeudeautomation GmbH
 *
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

#ifndef CO_NOTIFY_PIPE_H_
#define CO_NOTIFY_PIPE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_pipe Pipe
 * @ingroup CO_driver
 * @{
 *
 * This is needed to wake up the can socket when blocking in select
 */

/**
 * Object
 */
typedef struct CO_NotifyPipe CO_NotifyPipe_t;

/**
 * Create Pipe
 *
 * @return != null if successfully created
 */
CO_NotifyPipe_t *CO_NotifyPipeCreate(void);

/**
 * Delete Pipe
 *
 * @param p pointer to object
 */
void CO_NotifyPipeFree(CO_NotifyPipe_t *p);

/**
 * Get file descriptor for select
 *
 * @param p pointer to object
 */
int CO_NotifyPipeGetFd(CO_NotifyPipe_t *p);

/**
 * Send event
 *
 * @param p pointer to object
 */
void CO_NotifyPipeSend(CO_NotifyPipe_t *p);

/** @} */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_NOTIFY_PIPE_H_ */

/**
* @} @}
**/
