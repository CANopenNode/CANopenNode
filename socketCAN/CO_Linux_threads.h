/**
 * Helper functions for implementing CANopen threads in Linux.
 *
 * @file        CO_Linux_threads.h
 * @ingroup     CO_socketCAN
 * @author      Janez Paternoster
 * @author      Martin Wagner
 * @copyright   2004 - 2015 Janez Paternoster
 * @copyright   2018 - 2020 Neuberger Gebaeudeautomation GmbH
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

#ifndef CO_LINUX_THREADS_H
#define CO_LINUX_THREADS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_socketCAN socketCAN
 * @{
 *
 * Linux specific interface to CANopenNode
 *
 * CANopenNode runs in two threads: timer based realtime thread for CAN receive,
 * SYNC and PDO and mainline thread for other processing.
 *
 * The "threads" specified here do not fork threads themselves, but require
 * that two threads are provided by the calling application.
 * It uses the global CO object and has one thread-local struct for variables.
 */

/**
 * Initialize mainline thread.
 *
 * threadMain is non-realtime thread for CANopenNode processing. It is nonblocking
 * and should be called cyclically in 50 ms intervals or less if necessary. This
 * is indicated by the callback function.
 * This thread processes CO_process() function from CANopen.c file.
 *
 * @param callback this function is called to indicate #threadMain_process() has
 * work to do
 * @param object this pointer is given to _callback()_
 */
extern void threadMain_init(void (*callback)(void*), void *object);

/**
 * Cleanup mainline thread.
 */
extern void threadMain_close(void);

/**
 * Process mainline thread.
 *
 * Function must be called cyclically and after callback
 *
 * @param reset return value from CO_process() function.
 */
extern void threadMain_process(CO_NMT_reset_cmd_t *reset);

/**
 * Initialize realtime thread.
 *
 * CANrx_threadTmr is realtime thread for CANopenNode processing. It is nonblocking
 * and must be executed at CAN message receive or periodically in 1ms (or something)
 * intervals. Inside interval is processed CANopen SYNC message, RPDOs(inputs)
 * and TPDOs(outputs).
 * CANrx_threadTmr uses CAN socket from CO_driver.c
 *
 * @remark If realtime is required, this thread must be registred as such in the Linux
 * kernel.
 *
 * @param interval_us Interval of periodic timer in microseconds, recommended
 *                    value for realtime response: 1000 us
 */
extern void CANrx_threadTmr_init(uint32_t interval_us);

/**
 * Terminate realtime thread.
 */
extern void CANrx_threadTmr_close(void);

/**
 * Process realtime thread.
 *
 * This function must be called inside an infinite loop. It blocks until either
 * some event happens or a timer runs out.
 */
extern void CANrx_threadTmr_process();

/** @} */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_LINUX_THREADS_H */
