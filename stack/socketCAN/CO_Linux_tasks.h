/**
 * Helper functions for implementing CANopen tasks in Linux using epoll.
 *
 * @file        Linux_tasks.h
 * @author      Janez Paternoster
 * @copyright   2015 - 2020 Janez Paternoster
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


#ifndef CO_LINUX_TASKS_H
#define CO_LINUX_TASKS_H


/**
 * Initialize mainline task.
 *
 * taskMain is non-realtime task for CANopenNode processing. It is nonblocking
 * and is executing cyclically in 50 ms intervals or less if necessary.
 * It uses Linux epoll, timerfd for interval and pipe for task triggering.
 * This task processes CO_process() function from CANopen.c file.
 *
 * @param fdEpoll File descriptor for Linux epoll API.
 * @param maxTime Pointer to variable, where longest interval will be written
 * [in milliseconds]. If NULL, calculations won't be made.
 */
void taskMain_init(int fdEpoll, uint16_t *maxTime);

/**
 * Cleanup mainline task.
 */
void taskMain_close(void);

/**
 * Process mainline task.
 *
 * Function must be called after epoll.
 *
 * @param fd Available file descriptor from epoll().
 * @param reset return value from CO_process() function.
 * @param timer1ms variable, which must increment each millisecond.
 *
 * @return True, if fd was matched.
 */
bool_t taskMain_process(int fd, CO_NMT_reset_cmd_t *reset, uint16_t timer1ms);

/**
 * Signal function, which triggers mainline task.
 *
 * It is used from some CANopenNode objects as callback.
 */
void taskMain_cbSignal(void);


/**
 * Initialize realtime task.
 *
 * CANrx_taskTmr is realtime task for CANopenNode processing. It is nonblocking
 * and is executing on CAN message receive or periodically in 1ms (or something)
 * intervals. Inside interval is processed CANopen SYNC message, RPDOs(inputs)
 * and TPDOs(outputs). Between inputs and outputs can also be executed some
 * realtime application code.
 * CANrx_taskTmr uses Linux epoll, CAN socket form CO_driver.c and timerfd for
 * interval.
 *
 *
 * @param fdEpoll File descriptor for Linux epoll API.
 * @param intervalns Interval of periodic timer in nanoseconds.
 * @param maxTime Pointer to variable, where longest interval will be written
 * [in milliseconds]. If NULL, calculations won't be made.
 */
void CANrx_taskTmr_init(int fdEpoll, long intervalns, uint16_t *maxTime);

/**
 * Cleanup realtime task.
 */
void CANrx_taskTmr_close(void);

/**
 * Process realtime task.
 *
 * Function must be called after epoll.
 *
 * @param fd Available file descriptor from epoll().
 *
 * @return True, if fd was matched.
 */
bool_t CANrx_taskTmr_process(int fd);

/**
 * Disable CAN receive thread temporary.
 *
 * Function is called at SYNC message on CAN bus.
 * It disables CAN receive thread until RPDOs are processed.
 */
void CANrx_lockCbSync(bool_t syncReceived);

#endif
