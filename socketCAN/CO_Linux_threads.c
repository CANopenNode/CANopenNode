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

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <fcntl.h>

#include "CANopen.h"


/* Helper function - get monotonic clock time in microseconds */
static uint64_t CO_LinuxThreads_clock_gettime_us(void)
{
    struct timespec ts;

    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}


/* Mainline thread - basic (threadMain) ***************************************/
static struct
{
    uint64_t start;   /* time value CO_process() was called last time in us */
} threadMain;

void threadMain_init(void (*callback)(void*), void *object)
{
    threadMain.start = CO_LinuxThreads_clock_gettime_us();

    CO_SDO_initCallbackPre(CO->SDO[0], object, callback);
    CO_EM_initCallbackPre(CO->em, object, callback);
    CO_HBconsumer_initCallbackPre(CO->HBcons, object, callback);
}

void threadMain_close(void)
{
    CO_SDO_initCallbackPre(CO->SDO[0], NULL, NULL);
    CO_EM_initCallbackPre(CO->em, NULL, NULL);
    CO_HBconsumer_initCallbackPre(CO->HBcons, NULL, NULL);
}

void threadMain_process(CO_NMT_reset_cmd_t *reset)
{
    uint32_t finished;
    uint32_t diff;
    uint64_t now;

    now = CO_LinuxThreads_clock_gettime_us();
    diff = (uint32_t)(now - threadMain.start);
    threadMain.start = now;

    /* we use timerNext_us in CO_process() as indication if processing is
    * finished. We ignore any calculated values for maximum delay times. */
    do {
        finished = 1;
        *reset = CO_process(CO, diff, &finished);
        diff = 0;
    } while ((*reset == CO_RESET_NOT) && (finished == 0));
}


/* Mainline thread - Blocking (threadMainWait) ********************************/
static struct
{
    uint64_t start;   /* time value CO_process() was called last time in us */
    int epoll_fd;         /* epoll file descriptor */
    int event_fd;         /* notification event file descriptor */
    int timer_fd;         /* interval timer file descriptor */
    uint32_t interval_us; /* interval for threadMainWait_process */
    struct itimerspec tm; /* structure for timer configuration */
} threadMainWait;

static void threadMainWait_callback(void *object)
{
    /* send event to wake threadMainWait_process() */
    uint64_t u = 1;
    ssize_t s;
    s = write(threadMainWait.event_fd, &u, sizeof(uint64_t));
    if (s != sizeof(uint64_t)) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "write()");
    }
}

void threadMainWait_init(uint32_t interval_us)
{
    int32_t ret;
    struct epoll_event ev;

    /* Configure callback functions */
    CO_SDO_initCallbackPre(CO->SDO[0], NULL, threadMainWait_callback);
    CO_EM_initCallbackPre(CO->em, NULL, threadMainWait_callback);
    CO_HBconsumer_initCallbackPre(CO->HBcons, NULL, threadMainWait_callback);

    /* Initial value for time calculation */
    threadMainWait.start = CO_LinuxThreads_clock_gettime_us();

    /* Configure epoll for mainline */
    threadMainWait.epoll_fd = epoll_create(1);
    if (threadMainWait.epoll_fd < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "epoll_create()");
        exit(EXIT_FAILURE);
    }

    /* Configure eventfd for notifications and add it to epoll */
    threadMainWait.event_fd = eventfd(0, EFD_NONBLOCK);
    if (threadMainWait.event_fd < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "eventfd()");
        exit(EXIT_FAILURE);
    }
    ev.events = EPOLLIN;
    ev.data.fd = threadMainWait.event_fd;
    ret = epoll_ctl(threadMainWait.epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if (ret < 0){
        log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(event_fd)");
        exit(EXIT_FAILURE);
    }

    /* Configure timer for interval_us and add it to epoll */
    threadMainWait.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (threadMainWait.timer_fd < 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "timerfd_create()");
        exit(EXIT_FAILURE);
    }
    threadMainWait.interval_us = interval_us;
    threadMainWait.tm.it_interval.tv_sec = interval_us / 1000000;
    threadMainWait.tm.it_interval.tv_nsec = (interval_us % 1000000) * 1000;
    threadMainWait.tm.it_value.tv_sec = 0;
    threadMainWait.tm.it_value.tv_nsec = 1;
    ret = timerfd_settime(threadMainWait.timer_fd, 0, &threadMainWait.tm, NULL);
    if (ret < 0){
        log_printf(LOG_CRIT, DBG_ERRNO, "timerfd_settime");
        exit(EXIT_FAILURE);
    }
    ev.events = EPOLLIN;
    ev.data.fd = threadMainWait.timer_fd;
    ret = epoll_ctl(threadMainWait.epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if (ret < 0){
        log_printf(LOG_CRIT, DBG_ERRNO, "epoll_ctl(timer_fd)");
        exit(EXIT_FAILURE);
    }
}

void threadMainWait_close(void)
{
    CO_SDO_initCallbackPre(CO->SDO[0], NULL, NULL);
    CO_EM_initCallbackPre(CO->em, NULL, NULL);
    CO_HBconsumer_initCallbackPre(CO->HBcons, NULL, NULL);

    close(threadMainWait.epoll_fd);
    close(threadMainWait.event_fd);
    close(threadMainWait.timer_fd);
    threadMainWait.epoll_fd = -1;
    threadMainWait.event_fd = -1;
    threadMainWait.timer_fd = -1;
}

uint32_t threadMainWait_process(CO_NMT_reset_cmd_t *reset)
{
    int ready, ret;
    struct epoll_event ev;
    uint64_t ull;
    ssize_t s;
    uint32_t diff, timerNext_us;

    /* wait for event or timer expiration and read data from file descriptors */
    ready = epoll_wait(threadMainWait.epoll_fd, &ev, 1, -1);
    if (ready != 1 && errno != EINTR) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "epoll_wait");
    }
    else if (ev.data.fd == threadMainWait.event_fd) {
        s = read(threadMainWait.event_fd, &ull, sizeof(uint64_t));
        if (s != sizeof(uint64_t)) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "read(event_fd)");
        }
    }
    else if (ev.data.fd == threadMainWait.timer_fd) {
        s = read(threadMainWait.timer_fd, &ull, sizeof(uint64_t));
        if (s != sizeof(uint64_t) && errno != EAGAIN) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "read(timer_fd)");
        }
    }

    /* calculate time difference since last call */
    ull = CO_LinuxThreads_clock_gettime_us();
    diff = (uint32_t)(ull - threadMainWait.start);
    threadMainWait.start = ull;

    /* stack will lower this, if necessary */
    timerNext_us = threadMainWait.interval_us;

    /* process CANopen objects */
    *reset = CO_process(CO, diff, &timerNext_us);

    /* lower next timer interval if necessary */
    if (timerNext_us < threadMainWait.interval_us) {
        /* add one microsecond extra delay and make sure it is not zero */
        timerNext_us += 1;
        if (threadMainWait.interval_us < 1000000) {
        threadMainWait.tm.it_value.tv_nsec = timerNext_us * 1000;
        } else {
        threadMainWait.tm.it_value.tv_sec = timerNext_us / 1000000;
        threadMainWait.tm.it_value.tv_nsec = (timerNext_us % 1000000) * 1000;
        }
        ret = timerfd_settime(threadMainWait.timer_fd, 0,
                            &threadMainWait.tm, NULL);
        if (ret < 0){
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
        /* at least one timer interval occured */
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
