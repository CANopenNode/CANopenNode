/**
 * Helper functions for Linux epoll interface to CANopenNode.
 *
 * @file        CO_epoll_interface.h
 * @ingroup     CO_epoll_interface
 * @author      Janez Paternoster
 * @author      Martin Wagner
 * @copyright   2004 - 2020 Janez Paternoster
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

#ifndef CO_EPOLL_INTERFACE_H
#define CO_EPOLL_INTERFACE_H

#include "CANopen.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_socketCAN socketCAN
 * @{
 *
 * Linux specific interface to CANopenNode.
 *
 * Linux includes CAN interface inside its kernel, so called SocketCAN. It
 * operates as a network device. For more information on Linux SocketCAN see
 * https://www.kernel.org/doc/html/latest/networking/can.html
 *
 * Linux specific files for interfacing with Linux SocketCAN are located inside
 * "CANopenNode/socketCAN" directory.
 *
 * CANopenNode runs as a set of non-blocking functions. It can run in single or
 * multiple threads. Best approach for RT IO device can be with two threads:
 * - timer based real-time thread for CAN receive, SYNC and PDO, see
 *   @ref CO_epoll_processRT()
 * - mainline thread for other processing, see @ref CO_epoll_processMain()
 *
 * Main references for Linux functions used here are Linux man pages and the
 * book: The Linux Programming Interface by Michael Kerrisk.
 * @}
 */

/**
 * @defgroup CO_epoll_interface Epoll interface
 * @ingroup CO_socketCAN
 * @{
 *
 * Linux epoll interface to CANopenNode.
 *
 * The Linux epoll API performs a monitoring multiple file descriptors to see
 * if I/O is possible on any of them.
 *
 * CANopenNode uses epoll interface to provide an event based mechanism. Epoll
 * waits for multiple different events, such as: interval timer event,
 * notification event, CAN receive event or socket based event for gateway.
 * CANopenNode non-blocking functions are processed after each event.
 *
 * CANopenNode itself offers functionality for calculation of time, when next
 * interval timer event should trigger the processing. It can also trigger
 * notification events in case of multi-thread operation.
 */

/**
 * Object for epoll, timer and event API.
 */
typedef struct {
    /** Epoll file descriptor */
    int epoll_fd;
    /** Notification event file descriptor */
    int event_fd;
    /** Interval timer file descriptor */
    int timer_fd;
    /** Interval of the timer in microseconds, from @ref CO_epoll_create() */
    uint32_t timerInterval_us;
    /** Time difference since last @ref CO_epoll_wait() execution in
     * microseconds */
    uint32_t timeDifference_us;
    /** Timer value in microseconds, which can be changed by application and can
     * shorten time of next @ref CO_epoll_wait() execution */
    uint32_t timerNext_us;
    /** True,if timer event is inside @ref CO_epoll_wait() */
    bool_t timerEvent;
    /** time value from the last process call in microseconds */
    uint64_t previousTime_us;
    /** Structure for timerfd */
    struct itimerspec tm;
    /** Structure for epoll_wait */
    struct epoll_event ev;
    /** true, if new epoll event is necessary to process */
    bool_t epoll_new;
} CO_epoll_t;

/**
 * Create Linux epoll, timerfd and eventfd
 *
 * Create and configure multiple Linux notification facilities, which trigger
 * execution of the task. Epoll blocks and monitors multiple file descriptors,
 * timerfd triggers in constant timer intervals and eventfd triggers on external
 * signal.
 *
 * @param ep This object
 * @param timerInterval_us Timer interval in microseconds
 *
 * @return @ref CO_ReturnError_t CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT or
 * CO_ERROR_SYSCALL.
 */
CO_ReturnError_t CO_epoll_create(CO_epoll_t *ep, uint32_t timerInterval_us);

/**
 * Close epoll, timerfd and eventfd
 *
 * @param ep This object
 */
void CO_epoll_close(CO_epoll_t *ep);

/**
 * Wait for an epoll event
 *
 * This function blocks until event registered on epoll: timerfd, eventfd, or
 * application specified event. Function also calculates timeDifference_us since
 * last call and prepares timerNext_us.
 *
 * @param ep This object
 */
void CO_epoll_wait(CO_epoll_t *ep);

/**
 * Closing function for an epoll event
 *
 * This function must be called after @ref CO_epoll_wait(). Between them
 * should be application specified processing functions, which can check for
 * own events and do own processing. Application may also lower timerNext_us
 * variable. If lowered, then interval timer will be reconfigured and
 * @ref CO_epoll_wait() will be triggered earlier.
 *
 * @param ep This object
 */
void CO_epoll_processLast(CO_epoll_t *ep);

/**
 * Initialization of functions in CANopen reset-communication section
 *
 * Configure callbacks for CANopen objects.
 *
 * @param ep This object
 * @param co CANopen object
 */
void CO_epoll_initCANopenMain(CO_epoll_t *ep, CO_t *co);

/**
 * Process CANopen mainline functions
 *
 * This function calls @ref CO_process(). It is non-blocking and should execute
 * cyclically. It should be between @ref CO_epoll_wait() and
 * @ref CO_epoll_processLast() functions.
 *
 * @param ep This object
 * @param co CANopen object
 * @param [out] reset Return from @ref CO_process().
 */
void CO_epoll_processMain(CO_epoll_t *ep,
                          CO_t *co,
                          CO_NMT_reset_cmd_t *reset);


/**
 * Process CAN receive and realtime functions
 *
 * This function checks epoll for CAN receive event and processes CANopen
 * realtime functions: @ref CO_process_SYNC(), @ref CO_process_RPDO() and
 * @ref CO_process_TPDO().  It is non-blocking and should execute cyclically.
 * It should be between @ref CO_epoll_wait() and @ref CO_epoll_processLast()
 * functions.
 *
 * Function can be used in the mainline thread or in own realtime thread.
 *
 * Processing of CANopen realtime functions is protected with @ref CO_LOCK_OD.
 * Also Node-Id must be configured and CANmodule must be in CANnormal for
 * processing.
 *
 * @param ep Pointer to @ref CO_epoll_t object.
 * @param co CANopen object
 * @param realtime Set to true, if function is called from the own realtime
 * thread, and is executed at short constant interval.
 */
void CO_epoll_processRT(CO_epoll_t *ep,
                        CO_t *co,
                        bool_t realtime);


#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII) || defined CO_DOXYGEN
/**
 * Command interface type for gateway-ascii
 */
typedef enum {
    CO_COMMAND_IF_DISABLED = -100,
    CO_COMMAND_IF_STDIO = -2,
    CO_COMMAND_IF_LOCAL_SOCKET = -1,
    CO_COMMAND_IF_TCP_SOCKET_MIN = 0,
    CO_COMMAND_IF_TCP_SOCKET_MAX = 0xFFFF
} CO_commandInterface_t;

/**
 * Object for gateway
 */
typedef struct {
    /** Epoll file descriptor, from @ref CO_epoll_createGtw() */
    int epoll_fd;
    /** Command interface type or tcp port number, see
     * @ref CO_commandInterface_t */
    int32_t commandInterface;
    /** Socket timeout in microseconds */
    uint32_t socketTimeout_us;
    /** Socket timeout timer in microseconds */
    uint32_t socketTimeoutTmr_us;
    /** Path in case of local socket */
    char *localSocketPath;
    /** Gateway socket file descriptor */
    int gtwa_fdSocket;
    /** Gateway io stream file descriptor */
    int gtwa_fd;
    /** Indication of fresh command */
    bool_t freshCommand;
} CO_epoll_gtw_t;

/**
 * Create socket for gateway-ascii command interface and add it to epoll
 *
 * Depending on arguments function configures stdio interface or local socket
 * or IP socket.
 *
 * @param epGtw This object
 * @param epoll_fd Already configured epoll file descriptor
 * @param commandInterface Command interface type from CO_commandInterface_t
 * @param socketTimeout_ms Timeout for established socket connection in [ms]
 * @param localSocketPath File path, if commandInterface is local socket
 *
 * @return @ref CO_ReturnError_t CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT or
 * CO_ERROR_SYSCALL.
 */
CO_ReturnError_t CO_epoll_createGtw(CO_epoll_gtw_t *epGtw,
                                    int epoll_fd,
                                    int32_t commandInterface,
                                    uint32_t socketTimeout_ms,
                                    char *localSocketPath);

/**
 * Close gateway-ascii sockets
 *
 * @param epGtw This object
 */
void CO_epoll_closeGtw(CO_epoll_gtw_t *epGtw);

/**
 * Initialization of gateway functions  in CANopen reset-communication section
 *
 * @param epGtw This object
 * @param co CANopen object
 */
void CO_epoll_initCANopenGtw(CO_epoll_gtw_t *epGtw, CO_t *co);

/**
 * Process CANopen gateway functions
 *
 * This function checks for epoll events and verifies socket connection timeout.
 * It is non-blocking and should execute cyclically. It should be between
 * @ref CO_epoll_wait() and @ref CO_epoll_processLast() functions.
 *
 * @param epGtw This object
 * @param co CANopen object
 * @param ep Pointer to @ref CO_epoll_t object.
 */
void CO_epoll_processGtw(CO_epoll_gtw_t *epGtw,
                         CO_t *co,
                         CO_epoll_t *ep);
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII */

/** @} */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_EPOLL_INTERFACE_H */
