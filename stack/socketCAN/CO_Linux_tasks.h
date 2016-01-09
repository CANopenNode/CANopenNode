/**
 * Helper functions for implementing CANopen tasks in Linux using epoll.
 *
 * @file        Linux_tasks.h
 * @author      Janez Paternoster
 * @copyright   2015 Janez Paternoster
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
