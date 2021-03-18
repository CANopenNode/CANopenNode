/*
 * Helper functions for Linux epoll interface to CANopenNode.
 *
 * @file        CO_epoll_interface.c
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

/* following macro is necessary for accept4() function call (sockets) */
#define _GNU_SOURCE

#include "CO_epoll_interface.h"

#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <time.h>
#include <fcntl.h>

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <signal.h>

#ifndef LISTEN_BACKLOG
#define LISTEN_BACKLOG 50
#endif
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII */

/* delay for recall CANsend(), if CAN TX buffer is full */
#ifndef CANSEND_DELAY_US
#define CANSEND_DELAY_US 100
#endif


/* EPOLL **********************************************************************/
/* Helper function - get monotonic clock time in microseconds */
static inline uint64_t clock_gettime_us(void) {
    struct timespec ts;

    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

CO_ReturnError_t CO_epoll_create(CO_epoll_t *ep, uint32_t timerInterval_us) {
    int ret;
    struct epoll_event ev;

    if (ep == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure epoll for mainline */
    ep->epoll_new = false;
    ep->epoll_fd = epoll_create(1);
    if (ep->epoll_fd < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "epoll_create()");
        return CO_ERROR_SYSCALL;
    }

    /* Configure eventfd for notifications and add it to epoll */
    ep->event_fd = eventfd(0, EFD_NONBLOCK);
    if (ep->event_fd < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "eventfd()");
        return CO_ERROR_SYSCALL;
    }
    ev.events = EPOLLIN;
    ev.data.fd = ep->event_fd;
    ret = epoll_ctl(ep->epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if (ret < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(event_fd)");
        return CO_ERROR_SYSCALL;
    }

    /* Configure timer for timerInterval_us and add it to epoll */
    ep->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (ep->timer_fd < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "timerfd_create()");
        return CO_ERROR_SYSCALL;
    }
    ep->tm.it_interval.tv_sec = timerInterval_us / 1000000;
    ep->tm.it_interval.tv_nsec = (timerInterval_us % 1000000) * 1000;
    ep->tm.it_value.tv_sec = 0;
    ep->tm.it_value.tv_nsec = 1;
    ret = timerfd_settime(ep->timer_fd, 0, &ep->tm, NULL);
    if (ret < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "timerfd_settime");
        return CO_ERROR_SYSCALL;
    }
    ev.events = EPOLLIN;
    ev.data.fd = ep->timer_fd;
    ret = epoll_ctl(ep->epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if (ret < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(timer_fd)");
        return CO_ERROR_SYSCALL;
    }
    ep->timerInterval_us = timerInterval_us;
    ep->previousTime_us = clock_gettime_us();
    ep->timeDifference_us = 0;

    return CO_ERROR_NO;
}

void CO_epoll_close(CO_epoll_t *ep) {
    if (ep == NULL) {
        return;
    }

    close(ep->epoll_fd);
    ep->epoll_fd = -1;

    close(ep->event_fd);
    ep->event_fd = -1;

    close(ep->timer_fd);
    ep->timer_fd = -1;
}


void CO_epoll_wait(CO_epoll_t *ep) {
    if (ep == NULL) {
        return;
    }

    /* wait for an event */
    int ready = epoll_wait(ep->epoll_fd, &ep->ev, 1, -1);
    ep->epoll_new = true;
    ep->timerEvent = false;

    /* calculate time difference since last call */
    uint64_t now = clock_gettime_us();
    ep->timeDifference_us = (uint32_t)(now - ep->previousTime_us);
    ep->previousTime_us = now;
    /* application may will lower this */
    ep->timerNext_us = ep->timerInterval_us;

    /* process event */
    if (ready != 1 && errno == EINTR) {
        /* event from interrupt or signal, nothing to process, continue */
        ep->epoll_new = false;
    }
    else if (ready != 1) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "epoll_wait");
        ep->epoll_new = false;
    }
    else if ((ep->ev.events & EPOLLIN) != 0
             && ep->ev.data.fd == ep->event_fd
    ) {
        uint64_t val;
        ssize_t s = read(ep->event_fd, &val, sizeof(uint64_t));
        if (s != sizeof(uint64_t)) {
            log_printf(LOG_DEBUG, DBG_ERRNO, "read(event_fd)");
        }
        ep->epoll_new = false;
    }
    else if ((ep->ev.events & EPOLLIN) != 0
             && ep->ev.data.fd == ep->timer_fd
    ) {
        uint64_t val;
        ssize_t s = read(ep->timer_fd, &val, sizeof(uint64_t));
        if (s != sizeof(uint64_t) && errno != EAGAIN) {
            log_printf(LOG_DEBUG, DBG_ERRNO, "read(timer_fd)");
        }
        ep->epoll_new = false;
        ep->timerEvent = true;
    }
}

void CO_epoll_processLast(CO_epoll_t *ep) {
    if (ep == NULL) {
        return;
    }

    if (ep->epoll_new) {
        log_printf(LOG_DEBUG, DBG_EPOLL_UNKNOWN,
                   ep->ev.events, ep->ev.data.fd);
        ep->epoll_new = false;
    }

    /* lower next timer interval if changed by application */
    if (ep->timerNext_us < ep->timerInterval_us) {
        /* add one microsecond extra delay and make sure it is not zero */
        ep->timerNext_us += 1;
        if (ep->timerInterval_us < 1000000) {
            ep->tm.it_value.tv_nsec = ep->timerNext_us * 1000;
        }
        else {
            ep->tm.it_value.tv_sec = ep->timerNext_us / 1000000;
            ep->tm.it_value.tv_nsec =
                                    (ep->timerNext_us % 1000000) * 1000;
        }
        int ret = timerfd_settime(ep->timer_fd, 0, &ep->tm, NULL);
        if (ret < 0) {
            log_printf(LOG_DEBUG, DBG_ERRNO, "timerfd_settime");
        }
    }
}


/* MAINLINE *******************************************************************/
#ifndef CO_SINGLE_THREAD
/* Send event to wake CO_epoll_processMain() */
static void wakeupCallback(void *object) {
    CO_epoll_t *ep = (CO_epoll_t *)object;
    uint64_t u = 1;
    ssize_t s;
    s = write(ep->event_fd, &u, sizeof(uint64_t));
    if (s != sizeof(uint64_t)) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "write()");
    }
}
#endif

void CO_epoll_initCANopenMain(CO_epoll_t *ep, CO_t *co) {
    if (ep == NULL || co == NULL) {
        return;
    }

#ifndef CO_SINGLE_THREAD

    /* Configure LSS slave callback function */
 #if (CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE
  #if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
    CO_LSSslave_initCallbackPre(co->LSSslave,
                                (void *)ep, wakeupCallback);
  #endif
 #endif

    if (co->nodeIdUnconfigured) {
        return;
    }

    /* Configure callback functions */
 #if (CO_CONFIG_NMT) & CO_CONFIG_FLAG_CALLBACK_PRE
    CO_NMT_initCallbackPre(co->NMT,
                           (void *)ep, wakeupCallback);
 #endif
 #if (CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_CALLBACK_PRE
    CO_HBconsumer_initCallbackPre(co->HBcons,
                                  (void *)ep, wakeupCallback);
 #endif
 #if (CO_CONFIG_EM) & CO_CONFIG_FLAG_CALLBACK_PRE
    CO_EM_initCallbackPre(co->em,
                          (void *)ep, wakeupCallback);
 #endif
 #if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_CALLBACK_PRE
    CO_SDOserver_initCallbackPre(&co->SDOserver[0],
                                 (void *)ep, wakeupCallback);
 #endif
 #if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_CALLBACK_PRE
    CO_SDOclient_initCallbackPre(&co->SDOclient[0],
                                 (void *)ep, wakeupCallback);
 #endif
 #if (CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE
    CO_TIME_initCallbackPre(co->TIME,
                            (void *)ep, wakeupCallback);
 #endif
 #if (CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE
  #if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER
    CO_LSSmaster_initCallbackPre(co->LSSmaster,
                                 (void *)ep, wakeupCallback);
  #endif
 #endif

#endif /* CO_SINGLE_THREAD */
}

void CO_epoll_processMain(CO_epoll_t *ep,
                          CO_t *co,
                          bool_t enableGateway,
                          CO_NMT_reset_cmd_t *reset)
{
    if (ep == NULL || co == NULL || reset == NULL) {
        return;
    }

    /* process CANopen objects */
    *reset = CO_process(co,
                        enableGateway,
                        ep->timeDifference_us,
                        &ep->timerNext_us);

    /* If there are unsent CAN messages, call CO_CANmodule_process() earlier */
    if (co->CANmodule->CANtxCount > 0 && ep->timerNext_us > CANSEND_DELAY_US) {
        ep->timerNext_us = CANSEND_DELAY_US;
    }
}


/* CANrx and REALTIME *********************************************************/
void CO_epoll_processRT(CO_epoll_t *ep,
                        CO_t *co,
                        bool_t realtime)
{
    if (co == NULL || ep == NULL) {
        return;
    }

    /* Verify for epoll events */
    if (ep->epoll_new) {
        if (CO_CANrxFromEpoll(co->CANmodule, &ep->ev, NULL, NULL)) {
            ep->epoll_new = false;
        }
    }

    if (!realtime || ep->timerEvent) {
        uint32_t *pTimerNext_us = realtime ? NULL : &ep->timerNext_us;

        CO_LOCK_OD(co->CANmodule);
        if (!co->nodeIdUnconfigured && co->CANmodule->CANnormal) {
            bool_t syncWas = false;

#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
            syncWas = CO_process_SYNC(co, ep->timeDifference_us,
                                      pTimerNext_us);
#endif
#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE
            CO_process_RPDO(co, syncWas, ep->timeDifference_us,
                            pTimerNext_us);
#endif
#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE
            CO_process_TPDO(co, syncWas, ep->timeDifference_us,
                            pTimerNext_us);
#endif
            (void) syncWas; (void) pTimerNext_us;
        }
        CO_UNLOCK_OD(co->CANmodule);
    }
}

/* GATEWAY ********************************************************************/
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
/* write response string from gateway-ascii object */
static size_t gtwa_write_response(void *object,
                                  const char *buf,
                                  size_t count,
                                  uint8_t *connectionOK)
{
    int* fd = (int *)object;
    /* nWritten = count -> in case of error (non-existing fd) data are purged */
    size_t nWritten = count;

    if (fd != NULL && *fd >= 0) {
        ssize_t n = write(*fd, (const void *)buf, count);
        if (n >= 0) {
            nWritten = (size_t)n;
        }
        else {
            /* probably EAGAIN - "Resource temporarily unavailable". Retry. */
            log_printf(LOG_DEBUG, DBG_ERRNO, "write(gtwa_response)");
            nWritten = 0;
        }
    }
    else {
        *connectionOK = 0;
    }
    return nWritten;
}

static inline void socetAcceptEnableForEpoll(CO_epoll_gtw_t *epGtw) {
    struct epoll_event ev;
    int ret;

    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.fd = epGtw->gtwa_fdSocket;
    ret = epoll_ctl(epGtw->epoll_fd, EPOLL_CTL_MOD, ev.data.fd, &ev);
    if (ret < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(gtwa_fdSocket)");
    }
}

CO_ReturnError_t CO_epoll_createGtw(CO_epoll_gtw_t *epGtw,
                                    int epoll_fd,
                                    int32_t commandInterface,
                                    uint32_t socketTimeout_ms,
                                    char *localSocketPath)
{
    int ret;
    struct epoll_event ev;

    if (epGtw == NULL || epoll_fd < 0) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }


    epGtw->epoll_fd = epoll_fd;
    epGtw->commandInterface = commandInterface;

    epGtw->socketTimeout_us = (socketTimeout_ms < (UINT_MAX / 1000 - 1000000)) ?
                              socketTimeout_ms * 1000 : (UINT_MAX - 1000000);
    epGtw->gtwa_fdSocket = -1;
    epGtw->gtwa_fd = -1;

    if (commandInterface == CO_COMMAND_IF_STDIO) {
        epGtw->gtwa_fd = STDIN_FILENO;
        log_printf(LOG_INFO, DBG_COMMAND_STDIO_INFO);
    }
    else if (commandInterface == CO_COMMAND_IF_LOCAL_SOCKET) {
        struct sockaddr_un addr;
        epGtw->localSocketPath = localSocketPath;

        /* Create, bind and listen local socket */
        epGtw->gtwa_fdSocket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if(epGtw->gtwa_fdSocket < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "socket(local)");
            return CO_ERROR_SYSCALL;
        }

        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, localSocketPath, sizeof(addr.sun_path) - 1);
        ret = bind(epGtw->gtwa_fdSocket, (struct sockaddr *) &addr,
                   sizeof(struct sockaddr_un));
        if(ret < 0) {
            log_printf(LOG_CRIT, DBG_COMMAND_LOCAL_BIND, localSocketPath);
            return CO_ERROR_SYSCALL;
        }

        ret = listen(epGtw->gtwa_fdSocket, LISTEN_BACKLOG);
        if(ret < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "listen(local)");
            return CO_ERROR_SYSCALL;
        }

        /* Ignore the SIGPIPE signal, which may happen, if remote client broke
         * the connection. Signal may be triggered by write call inside
         * gtwa_write_response() function */
        if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
            log_printf(LOG_CRIT, DBG_ERRNO, "signal");
            return CO_ERROR_SYSCALL;
        }

        log_printf(LOG_INFO, DBG_COMMAND_LOCAL_INFO, localSocketPath);
    }
    else if (commandInterface >= CO_COMMAND_IF_TCP_SOCKET_MIN &&
             commandInterface <= CO_COMMAND_IF_TCP_SOCKET_MAX
    ) {
        struct sockaddr_in addr;
        const int yes = 1;

        /* Create, bind and listen socket */
        epGtw->gtwa_fdSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if(epGtw->gtwa_fdSocket < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "socket(tcp)");
            return CO_ERROR_SYSCALL;
        }

        setsockopt(epGtw->gtwa_fdSocket, SOL_SOCKET, SO_REUSEADDR,
                   &yes, sizeof(int));

        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(commandInterface);
        addr.sin_addr.s_addr = INADDR_ANY;

        ret = bind(epGtw->gtwa_fdSocket, (struct sockaddr *) &addr,
                   sizeof(struct sockaddr_in));
        if(ret < 0) {
            log_printf(LOG_CRIT, DBG_COMMAND_TCP_BIND, commandInterface);
            return CO_ERROR_SYSCALL;
        }

        ret = listen(epGtw->gtwa_fdSocket, LISTEN_BACKLOG);
        if(ret < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "listen(tcp)");
            return CO_ERROR_SYSCALL;
        }

        /* Ignore the SIGPIPE signal, which may happen, if remote client broke
         * the connection. Signal may be triggered by write call inside
         * gtwa_write_response() function */
        if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
            log_printf(LOG_CRIT, DBG_ERRNO, "signal");
            return CO_ERROR_SYSCALL;
        }

        log_printf(LOG_INFO, DBG_COMMAND_TCP_INFO, commandInterface);
    }
    else {
        epGtw->commandInterface = CO_COMMAND_IF_DISABLED;
    }

    if (epGtw->gtwa_fd >= 0) {
        ev.events = EPOLLIN;
        ev.data.fd = epGtw->gtwa_fd;
        ret = epoll_ctl(epGtw->epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
        if (ret < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(gtwa_fd)");
            return CO_ERROR_SYSCALL;
        }
    }
    if (epGtw->gtwa_fdSocket >= 0) {
        /* prepare epoll for listening for new socket connection. After
         * connection will be accepted, fd for io operation will be defined. */
        ev.events = EPOLLIN | EPOLLONESHOT;
        ev.data.fd = epGtw->gtwa_fdSocket;
        ret = epoll_ctl(epGtw->epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
        if (ret < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(gtwa_fdSocket)");
            return CO_ERROR_SYSCALL;
        }
    }

    return CO_ERROR_NO;
}

void CO_epoll_closeGtw(CO_epoll_gtw_t *epGtw) {
    if (epGtw == NULL) {
        return;
    }

    if (epGtw->commandInterface == CO_COMMAND_IF_LOCAL_SOCKET) {
        if (epGtw->gtwa_fd > 0) {
            close(epGtw->gtwa_fd);
        }
        close(epGtw->gtwa_fdSocket);
        /* Remove local socket file from filesystem. */
        if(remove(epGtw->localSocketPath) < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "remove(local)");
        }
    }
    else if (epGtw->commandInterface >= CO_COMMAND_IF_TCP_SOCKET_MIN) {
        if (epGtw->gtwa_fd > 0) {
            close(epGtw->gtwa_fd);
        }
        close(epGtw->gtwa_fdSocket);
    }
    epGtw->gtwa_fd = -1;
    epGtw->gtwa_fdSocket = -1;
}

void CO_epoll_initCANopenGtw(CO_epoll_gtw_t *epGtw, CO_t *co) {
    if (epGtw == NULL || co == NULL || co->nodeIdUnconfigured) {
        return;
    }

    CO_GTWA_initRead(co->gtwa, gtwa_write_response, (void *)&epGtw->gtwa_fd);
    epGtw->freshCommand = true;
}

void CO_epoll_processGtw(CO_epoll_gtw_t *epGtw,
                         CO_t *co,
                         CO_epoll_t *ep)
{
    if (epGtw == NULL || co == NULL || ep == NULL) {
        return;
    }

    /* Verify for epoll events */
    if (ep->epoll_new
        && (ep->ev.data.fd == epGtw->gtwa_fdSocket
            || ep->ev.data.fd == epGtw->gtwa_fd)
    ) {
        if ((ep->ev.events & EPOLLIN) != 0
             && ep->ev.data.fd == epGtw->gtwa_fdSocket
        ) {
            bool_t fail = false;

            epGtw->gtwa_fd = accept4(epGtw->gtwa_fdSocket,
                                     NULL, NULL, SOCK_NONBLOCK);
            if (epGtw->gtwa_fd < 0) {
                fail = true;
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    log_printf(LOG_CRIT, DBG_ERRNO, "accept(gtwa_fdSocket)");
                }
            }
            else {
                /* add fd to epoll */
                struct epoll_event ev2;
                ev2.events = EPOLLIN;
                ev2.data.fd = epGtw->gtwa_fd;
                int ret = epoll_ctl(ep->epoll_fd,
                                    EPOLL_CTL_ADD, ev2.data.fd, &ev2);
                if (ret < 0) {
                    fail = true;
                    log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(add, gtwa_fd)");
                }
                epGtw->socketTimeoutTmr_us = 0;
            }

            if (fail) {
                socetAcceptEnableForEpoll(epGtw);
            }
            ep->epoll_new = false;
        }
        else if ((ep->ev.events & EPOLLIN) != 0
             && ep->ev.data.fd == epGtw->gtwa_fd
        ) {
            char buf[CO_CONFIG_GTWA_COMM_BUF_SIZE];
            size_t space = co->nodeIdUnconfigured ?
                        CO_CONFIG_GTWA_COMM_BUF_SIZE :
                        CO_GTWA_write_getSpace(co->gtwa);

            ssize_t s = read(epGtw->gtwa_fd, buf, space);

            if (space == 0 || co->nodeIdUnconfigured) {
                /* continue or purge data */
            }
            else if (s < 0 &&  errno != EAGAIN) {
                log_printf(LOG_DEBUG, DBG_ERRNO, "read(gtwa_fd)");
            }
            else if (s >= 0) {
                if (epGtw->commandInterface == CO_COMMAND_IF_STDIO) {
                    /* simplify command interface on stdio, make hard to type
                    * sequence optional, prepend "[0] " to string, if missing */
                    const char sequence[] = "[0] ";
                    bool_t closed = (buf[s-1] == '\n'); /* is command closed? */

                    if (buf[0] != '[' && (space - s) >= strlen(sequence)
                        && isgraph(buf[0]) && buf[0] != '#'
                        && closed && epGtw->freshCommand
                    ) {
                        CO_GTWA_write(co->gtwa, sequence, strlen(sequence));
                    }
                    epGtw->freshCommand = closed;
                    CO_GTWA_write(co->gtwa, buf, s);
                }
                else { /* socket, local or tcp */
                    if (s == 0) {
                        /* EOF received, close connection and enable socket
                         * accepting */
                        int ret = epoll_ctl(ep->epoll_fd, EPOLL_CTL_DEL,
                                            epGtw->gtwa_fd, NULL);
                        if (ret < 0) {
                            log_printf(LOG_CRIT, DBG_ERRNO,
                                    "epoll_ctl(del, gtwa_fd)");
                        }
                        if (close(epGtw->gtwa_fd) < 0) {
                            log_printf(LOG_CRIT, DBG_ERRNO, "close(gtwa_fd)");
                        }
                        epGtw->gtwa_fd = -1;
                        socetAcceptEnableForEpoll(epGtw);
                    }
                    else {
                        CO_GTWA_write(co->gtwa, buf, s);
                    }
                }
            }
            epGtw->socketTimeoutTmr_us = 0;

            ep->epoll_new = false;
        }
        else if ((ep->ev.events & (EPOLLERR | EPOLLHUP)) != 0) {
            log_printf(LOG_DEBUG, DBG_GENERAL,
                       "socket error or hangup, event=", ep->ev.events);
            if (close(epGtw->gtwa_fd) < 0) {
                log_printf(LOG_CRIT, DBG_ERRNO, "close(gtwa_fd, hangup)");
            }
        }
    } /* if (ep->epoll_new) */

    /* if socket connection is established, verify timeout */
    if (epGtw->socketTimeout_us > 0
        && epGtw->gtwa_fdSocket > 0 && epGtw->gtwa_fd > 0)
    {
        if (epGtw->socketTimeoutTmr_us > epGtw->socketTimeout_us) {
            /* timout expired, close current connection and accept next */
            int ret = epoll_ctl(ep->epoll_fd,
                                EPOLL_CTL_DEL, epGtw->gtwa_fd, NULL);
            if (ret < 0) {
                log_printf(LOG_CRIT, DBG_ERRNO,
                           "epoll_ctl(del, gtwa_fd), tmo");
            }
            if (close(epGtw->gtwa_fd) < 0) {
                log_printf(LOG_CRIT, DBG_ERRNO, "close(gtwa_fd), tmo");
            }
            epGtw->gtwa_fd = -1;
            socetAcceptEnableForEpoll(epGtw);
        }
        else {
            epGtw->socketTimeoutTmr_us += ep->timeDifference_us;
        }
    }
}
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII */
