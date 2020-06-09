/*
 * Helper functions for implementing CANopen threads in Linux
 *
 * @file        Linux_threads.c
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

#include "CANopen.h"
#include "CO_Linux_threads.h"

#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <time.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <fcntl.h>
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII */


#ifndef LISTEN_BACKLOG
#define LISTEN_BACKLOG 50
#endif


/* Helper function - get monotonic clock time in microseconds */
static inline uint64_t CO_LinuxThreads_clock_gettime_us(void)
{
    struct timespec ts;

    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}


#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
/* write response string from gateway-ascii object */
static size_t gtwa_write_response(void *object, const char *buf, size_t count) {
    int* fd = (int *)object;
    /* nWritten = count -> in case of error (non-existing fd) data are purged */
    size_t nWritten = count;

    if (fd != NULL && *fd >= 0) {
        ssize_t n = write(*fd, (const void *)buf, count);
        if (n >= 0) {
            nWritten = (size_t)n;
        }
        else {
            log_printf(LOG_DEBUG, DBG_ERRNO, "write(gtwa_response)");
        }
    }
    return nWritten;
}
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII */


/* Mainline thread - Blocking (threadMainWait) ********************************/
static struct {
    uint64_t start;   /* time value CO_process() was called last time in us */
    int epoll_fd;         /* epoll file descriptor */
    int event_fd;         /* notification event file descriptor */
    int timer_fd;         /* interval timer file descriptor */
    uint32_t interval_us; /* interval for threadMainWait_process */
    struct itimerspec tm; /* structure for timer configuration */
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    int32_t commandInterface; /* command interface type or tcp port */
    uint32_t socketTimeout_us;
    uint32_t socketTimeoutTmr_us;
    char *localSocketPath;/* path in case of local socket */
    int gtwa_fdSocket;    /* gateway socket file descriptor */
    int gtwa_fd;          /* gateway io stream file descriptor */
    bool_t freshCommand;
#endif
} tmw;

static void threadMainWait_callback(void *object)
{
    (void)object;
    /* send event to wake threadMainWait_process() */
    uint64_t u = 1;
    ssize_t s;
    s = write(tmw.event_fd, &u, sizeof(uint64_t));
    if (s != sizeof(uint64_t)) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "write()");
    }
}

void threadMainWait_init(bool_t CANopenConfiguredOK)
{
    /* Configure LSS slave callback function */
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
    CO_LSSslave_initCallbackPre(CO->LSSslave, NULL, threadMainWait_callback);
#endif

    /* Initial value for time calculation */
    tmw.start = CO_LinuxThreads_clock_gettime_us();

    if (!CANopenConfiguredOK)
        return;

    /* Configure callback functions */
    CO_NMT_initCallbackPre(CO->NMT, NULL, threadMainWait_callback);
    CO_SDO_initCallbackPre(CO->SDO[0], NULL, threadMainWait_callback);
    CO_EM_initCallbackPre(CO->em, NULL, threadMainWait_callback);
    CO_HBconsumer_initCallbackPre(CO->HBcons, NULL, threadMainWait_callback);
#if CO_NO_SDO_CLIENT != 0
    CO_SDOclient_initCallbackPre(CO->SDOclient[0], NULL,
                                 threadMainWait_callback);
#endif
#if (CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE
    CO_TIME_initCallbackPre(CO->TIME, NULL, threadMainWait_callback);
#endif
#if (CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER
    CO_LSSmaster_initCallbackPre(CO->LSSmaster, NULL, threadMainWait_callback);
#endif
#endif

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    CO_GTWA_initRead(CO->gtwa, gtwa_write_response, (void *)&tmw.gtwa_fd);
    tmw.freshCommand = true;
#endif
}

void threadMainWait_initOnce(uint32_t interval_us,
                             int32_t commandInterface,
                             uint32_t socketTimeout_ms,
                             char *localSocketPath)
{
    int ret;
    struct epoll_event ev;

    /* Configure epoll for mainline */
    tmw.epoll_fd = epoll_create(1);
    if (tmw.epoll_fd < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "epoll_create()");
        exit(EXIT_FAILURE);
    }

    /* Configure eventfd for notifications and add it to epoll */
    tmw.event_fd = eventfd(0, EFD_NONBLOCK);
    if (tmw.event_fd < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "eventfd()");
        exit(EXIT_FAILURE);
    }
    ev.events = EPOLLIN;
    ev.data.fd = tmw.event_fd;
    ret = epoll_ctl(tmw.epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if (ret < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(event_fd)");
        exit(EXIT_FAILURE);
    }

    /* Configure timer for interval_us and add it to epoll */
    tmw.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (tmw.timer_fd < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "timerfd_create()");
        exit(EXIT_FAILURE);
    }
    tmw.interval_us = interval_us;
    tmw.tm.it_interval.tv_sec = interval_us / 1000000;
    tmw.tm.it_interval.tv_nsec = (interval_us % 1000000) * 1000;
    tmw.tm.it_value.tv_sec = 0;
    tmw.tm.it_value.tv_nsec = 1;
    ret = timerfd_settime(tmw.timer_fd, 0, &tmw.tm, NULL);
    if (ret < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "timerfd_settime");
        exit(EXIT_FAILURE);
    }
    ev.events = EPOLLIN;
    ev.data.fd = tmw.timer_fd;
    ret = epoll_ctl(tmw.epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if (ret < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(timer_fd)");
        exit(EXIT_FAILURE);
    }

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    /* Configure gateway-ascii command interface (CiA309-3) */
    tmw.commandInterface = commandInterface;
    tmw.socketTimeout_us = (socketTimeout_ms < (UINT_MAX / 1000 - 1000000)) ?
                           socketTimeout_ms * 1000 : (UINT_MAX - 1000000);
    tmw.gtwa_fdSocket = -1;
    tmw.gtwa_fd = -1;

    if (commandInterface == CO_COMMAND_IF_STDIO) {
        tmw.gtwa_fd = STDIN_FILENO;
        log_printf(LOG_INFO, DBG_COMMAND_STDIO_INFO);
    }
    else if (commandInterface == CO_COMMAND_IF_LOCAL_SOCKET) {
        struct sockaddr_un addr;
        tmw.localSocketPath = localSocketPath;

        /* Create, bind and listen local socket */
        tmw.gtwa_fdSocket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if(tmw.gtwa_fdSocket < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "socket(local)");
            exit(EXIT_FAILURE);
        }

        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, localSocketPath, sizeof(addr.sun_path) - 1);
        ret = bind(tmw.gtwa_fdSocket, (struct sockaddr *) &addr,
                   sizeof(struct sockaddr_un));
        if(ret < 0) {
            log_printf(LOG_CRIT, DBG_COMMAND_LOCAL_BIND, localSocketPath);
            exit(EXIT_FAILURE);
        }

        ret = listen(tmw.gtwa_fdSocket, LISTEN_BACKLOG);
        if(ret < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "listen(local)");
            exit(EXIT_FAILURE);
        }

        log_printf(LOG_INFO, DBG_COMMAND_LOCAL_INFO, localSocketPath);
    }
    else if (commandInterface >= CO_COMMAND_IF_TCP_SOCKET_MIN &&
             commandInterface <= CO_COMMAND_IF_TCP_SOCKET_MAX
    ) {
        struct sockaddr_in addr;
        const int yes = 1;

        /* Create, bind and listen socket */
        tmw.gtwa_fdSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if(tmw.gtwa_fdSocket < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "socket(tcp)");
            exit(EXIT_FAILURE);
        }

        setsockopt(tmw.gtwa_fdSocket, SOL_SOCKET, SO_REUSEADDR,
                   &yes, sizeof(int));

        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(commandInterface);
        addr.sin_addr.s_addr = INADDR_ANY;

        ret = bind(tmw.gtwa_fdSocket, (struct sockaddr *) &addr,
                   sizeof(struct sockaddr_in));
        if(ret < 0) {
            log_printf(LOG_CRIT, DBG_COMMAND_TCP_BIND, commandInterface);
            exit(EXIT_FAILURE);
        }

        ret = listen(tmw.gtwa_fdSocket, LISTEN_BACKLOG);
        if(ret < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "listen(tcp)");
            exit(EXIT_FAILURE);
        }

        log_printf(LOG_INFO, DBG_COMMAND_TCP_INFO, commandInterface);
    }
    else {
        tmw.commandInterface = CO_COMMAND_IF_DISABLED;
    }

    if (tmw.gtwa_fd >= 0) {
        ev.events = EPOLLIN;
        ev.data.fd = tmw.gtwa_fd;
        ret = epoll_ctl(tmw.epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
        if (ret < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(gtwa_fd)");
            exit(EXIT_FAILURE);
        }
    }
    if (tmw.gtwa_fdSocket >= 0) {
        /* prepare epool for listening for new socket connection. After
         * connection will be accepted, fd for io operation will be defined. */
        ev.events = EPOLLIN | EPOLLONESHOT;
        ev.data.fd = tmw.gtwa_fdSocket;
        ret = epoll_ctl(tmw.epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
        if (ret < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(gtwa_fdSocket)");
            exit(EXIT_FAILURE);
        }
    }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII */
}

void threadMainWait_close(void)
{
    close(tmw.epoll_fd);
    tmw.epoll_fd = -1;

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    if (tmw.commandInterface == CO_COMMAND_IF_LOCAL_SOCKET) {
        if (tmw.gtwa_fd > 0) {
            close(tmw.gtwa_fd);
        }
        close(tmw.gtwa_fdSocket);
        /* Remove local socket file from filesystem. */
        if(remove(tmw.localSocketPath) < 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "remove(local)");
        }
    }
    else if (tmw.commandInterface >= CO_COMMAND_IF_TCP_SOCKET_MIN) {
        if (tmw.gtwa_fd > 0) {
            close(tmw.gtwa_fd);
        }
        close(tmw.gtwa_fdSocket);
    }
    tmw.gtwa_fd = -1;
    tmw.gtwa_fdSocket = -1;
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII */

    close(tmw.event_fd);
    tmw.event_fd = -1;

    close(tmw.timer_fd);
    tmw.timer_fd = -1;
}

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
static inline void socetAcceptEnableForEpoll(void) {
    struct epoll_event ev;
    int ret;

    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.fd = tmw.gtwa_fdSocket;
    ret = epoll_ctl(tmw.epoll_fd, EPOLL_CTL_MOD, ev.data.fd, &ev);
    if (ret < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(gtwa_fdSocket)");
    }
}
#endif

uint32_t threadMainWait_process(CO_NMT_reset_cmd_t *reset)
{
    int ready;
    struct epoll_event ev;
    uint64_t ull;
    ssize_t s;
    uint32_t diff, timerNext_us;

    /* wait for event or timer expiration and read data from file descriptors */
    ready = epoll_wait(tmw.epoll_fd, &ev, 1, -1);
    if (ready != 1 && errno != EINTR) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "epoll_wait");
    }
    else if (ev.data.fd == tmw.event_fd) {
        s = read(tmw.event_fd, &ull, sizeof(uint64_t));
        if (s != sizeof(uint64_t)) {
            log_printf(LOG_DEBUG, DBG_ERRNO, "read(event_fd)");
        }
    }
    else if (ev.data.fd == tmw.timer_fd) {
        s = read(tmw.timer_fd, &ull, sizeof(uint64_t));
        if (s != sizeof(uint64_t) && errno != EAGAIN) {
            log_printf(LOG_DEBUG, DBG_ERRNO, "read(timer_fd)");
        }
    }
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    else if (ev.data.fd == tmw.gtwa_fdSocket) {
        bool_t fail = false;

        tmw.gtwa_fd = accept4(tmw.gtwa_fdSocket, NULL, NULL, SOCK_NONBLOCK);
        if (tmw.gtwa_fd < 0) {
            fail = true;
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                log_printf(LOG_CRIT, DBG_ERRNO, "accept(gtwa_fdSocket)");
            }
        }
        else {
            int ret;
            /* add fd to epoll */
            struct epoll_event ev2;
            ev2.events = EPOLLIN;
            ev2.data.fd = tmw.gtwa_fd;
            ret = epoll_ctl(tmw.epoll_fd, EPOLL_CTL_ADD, ev2.data.fd, &ev2);
            if (ret < 0) {
                fail = true;
                log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(add, gtwa_fd)");
            }
            tmw.socketTimeoutTmr_us = 0;
        }

        if (fail) {
            socetAcceptEnableForEpoll();
        }
    }
    else if (ev.data.fd == tmw.gtwa_fd) {
        char buf[CO_CONFIG_GTWA_COMM_BUF_SIZE];
        size_t space = CO->nodeIdUnconfigured ?
                       CO_CONFIG_GTWA_COMM_BUF_SIZE :
                       CO_GTWA_write_getSpace(CO->gtwa);

        s = read(tmw.gtwa_fd, buf, space);

        if (CO->nodeIdUnconfigured) {
            /* purge data */
        }
        else if (s < 0 &&  errno != EAGAIN) {
            log_printf(LOG_DEBUG, DBG_ERRNO, "read(gtwa_fd)");
        }
        else if (s >= 0) {
            if (tmw.commandInterface == CO_COMMAND_IF_STDIO) {
                /* simplify command interface on stdio, make hard to type
                 * sequence optional, prepend "[0] " to string, if missing */
                const char sequence[] = "[0] ";
                bool_t closed = (buf[s-1] == '\n'); /* is command closed? */

                if (buf[0] != '[' && (space - s) >= strlen(sequence)
                    && isgraph(buf[0]) && buf[0] != '#'
                    && closed && tmw.freshCommand
                ) {
                    CO_GTWA_write(CO->gtwa, sequence, strlen(sequence));
                }
                tmw.freshCommand = closed;
                CO_GTWA_write(CO->gtwa, buf, s);
            }
            else { /* socket, local or tcp */
                if (s == 0) {
                    int ret;
                    /* EOF received, close connection and enable socket accept*/
                    ret = epoll_ctl(tmw.epoll_fd, EPOLL_CTL_DEL,
                                    tmw.gtwa_fd, NULL);
                    if (ret < 0) {
                        log_printf(LOG_CRIT, DBG_ERRNO,
                                   "epoll_ctl(del, gtwa_fd)");
                    }
                    if (close(tmw.gtwa_fd) < 0) {
                        log_printf(LOG_CRIT, DBG_ERRNO, "close(gtwa_fd)");
                    }
                    tmw.gtwa_fd = -1;
                    socetAcceptEnableForEpoll();
                }
                else {
                    CO_GTWA_write(CO->gtwa, buf, s);
                }
            }
        }
        tmw.socketTimeoutTmr_us = 0;
    }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII */

    /* calculate time difference since last call */
    ull = CO_LinuxThreads_clock_gettime_us();
    diff = (uint32_t)(ull - tmw.start);
    tmw.start = ull;

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    /* if socket connection is established, verify timeout */
    if (tmw.socketTimeout_us > 0 && tmw.gtwa_fdSocket > 0 && tmw.gtwa_fd > 0) {
        if (tmw.socketTimeoutTmr_us > tmw.socketTimeout_us) {
            int ret;
            /* timout expired, close current connection and accept next */
            ret = epoll_ctl(tmw.epoll_fd, EPOLL_CTL_DEL, tmw.gtwa_fd, NULL);
            if (ret < 0) {
                log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(del, gtwa_fd), tmo");
            }
            if (close(tmw.gtwa_fd) < 0) {
                log_printf(LOG_CRIT, DBG_ERRNO, "close(gtwa_fd), tmo");
            }
            tmw.gtwa_fd = -1;
            socetAcceptEnableForEpoll();
        }
        else {
            tmw.socketTimeoutTmr_us += diff;
        }
    }
#endif

    /* stack will lower this, if necessary */
    timerNext_us = tmw.interval_us;

    /* process CANopen objects */
    *reset = CO_process(CO, diff, &timerNext_us);

    /* lower next timer interval if necessary */
    if (timerNext_us < tmw.interval_us) {
        int ret;
        /* add one microsecond extra delay and make sure it is not zero */
        timerNext_us += 1;
        if (tmw.interval_us < 1000000) {
            tmw.tm.it_value.tv_nsec = timerNext_us * 1000;
        }
        else {
            tmw.tm.it_value.tv_sec = timerNext_us / 1000000;
            tmw.tm.it_value.tv_nsec = (timerNext_us % 1000000) * 1000;
        }
        ret = timerfd_settime(tmw.timer_fd, 0, &tmw.tm, NULL);
        if (ret < 0) {
            log_printf(LOG_DEBUG, DBG_ERRNO, "timerfd_settime");
        }
    }

    return diff;
}


/* Realtime thread (threadRT) *************************************************/
static struct {
    uint32_t us_interval;         /* configured interval in us */
    int interval_fd;              /* timer fd */
} threadRT;

void CANrx_threadTmr_init(uint32_t interval_us)
{
    struct itimerspec itval;

    threadRT.us_interval = interval_us;
    /* set up non-blocking interval timer */
    threadRT.interval_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    (void)fcntl(threadRT.interval_fd, F_SETFL, O_NONBLOCK);
    itval.it_interval.tv_sec = 0;
    itval.it_interval.tv_nsec = interval_us * 1000;
    itval.it_value = itval.it_interval;
    (void)timerfd_settime(threadRT.interval_fd, 0, &itval, NULL);
}

void CANrx_threadTmr_close(void)
{
    (void)close(threadRT.interval_fd);
    threadRT.interval_fd = -1;
}

uint32_t CANrx_threadTmr_process(void)
{
    int32_t result;
    uint64_t i;
    bool_t syncWas;
    uint64_t missed = 0;

    result = CO_CANrxWait(CO->CANmodule[0], threadRT.interval_fd, NULL);
    if (result < 0) {
        result = read(threadRT.interval_fd, &missed, sizeof(missed));
        if (result > 0) {
        /* at least one timer interval occurred */
        CO_LOCK_OD();

        if(CO->CANmodule[0]->CANnormal) {

            for (i = 0; i <= missed; i++) {

#if CO_NO_SYNC == 1
            /* Process Sync */
            syncWas = CO_process_SYNC(CO, threadRT.us_interval, NULL);
#else
            syncWas = false;
#endif
            /* Read inputs */
            CO_process_RPDO(CO, syncWas);

            /* Write outputs */
            CO_process_TPDO(CO, syncWas, threadRT.us_interval, NULL);
            }
        }

        CO_UNLOCK_OD();
        }
    }

    return (uint32_t) missed;
}
