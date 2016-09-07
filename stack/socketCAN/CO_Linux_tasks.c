/*
 * Helper functions for implementing CANopen tasks in Linux using epoll.
 *
 * @file        Linux_tasks.c
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


#include "CANopen.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>


#define NSEC_PER_SEC            (1000000000)    /* The number of nanoseconds per second. */
#define NSEC_PER_MSEC           (1000000)       /* The number of nanoseconds per millisecond. */


/* External helper function ***************************************************/
void CO_errExit(char* msg);
void CO_error(const uint32_t info);


/* Mainline task (taskMain) ***************************************************/
static struct {
    int                 fdTmr;          /* file descriptor for taskTmr */
    int                 fdPipe[2];      /* file descriptors for pipe [0]=read, [1]=write */
    struct itimerspec   tmrSpec;
    uint16_t            tmr1msPrev;
    uint16_t           *maxTime;
} taskMain;


void taskMain_init(int fdEpoll, uint16_t *maxTime) {
    struct epoll_event ev;
    int flags;

    /* Prepare pipe for triggering events. For example, if new SDO request
     * arrives from CAN network, CANrx callback writes a byte into the pipe.
     * This immediately triggers (via epoll) processing of SDO server, which
     * generates response. Read and write ends of pipe are nonblocking.
     * (See 'self pipe trick'.) */
    if(pipe(taskMain.fdPipe) == -1)
        CO_errExit("taskMain_init - pipe failed");

    flags = fcntl(taskMain.fdPipe[0], F_GETFL);
    if(flags == -1)
        CO_errExit("taskMain_init - fcntl-F_GETFL[0] failed");
    flags |= O_NONBLOCK;
    if(fcntl(taskMain.fdPipe[0], F_SETFL, flags) == -1)
        CO_errExit("taskMain_init - fcntl-F_SETFL[0] failed");

    flags = fcntl(taskMain.fdPipe[1], F_GETFL);
    if(flags == -1)
        CO_errExit("taskMain_init - fcntl-F_GETFL[1] failed");
    flags |= O_NONBLOCK;
    if(fcntl(taskMain.fdPipe[1], F_SETFL, flags) == -1)
        CO_errExit("taskMain_init - fcntl-F_SETFL[1] failed");

    /* get file descriptor for timer */
    taskMain.fdTmr = timerfd_create(CLOCK_MONOTONIC, 0);
    if(taskMain.fdTmr == -1)
        CO_errExit("taskMain_init - timerfd_create failed");

    /* add events for epoll */
    ev.events = EPOLLIN;
    ev.data.fd = taskMain.fdPipe[0];
    if(epoll_ctl(fdEpoll, EPOLL_CTL_ADD, taskMain.fdPipe[0], &ev) == -1)
        CO_errExit("taskMain_init - epoll_ctl CANrx failed");

    ev.events = EPOLLIN;
    ev.data.fd = taskMain.fdTmr;
    if(epoll_ctl(fdEpoll, EPOLL_CTL_ADD, taskMain.fdTmr, &ev) == -1)
        CO_errExit("taskMain_init - epoll_ctl taskTmr failed");

    /* Prepare timer, use no interval, delay time will be set each cycle. */
    taskMain.tmrSpec.it_interval.tv_sec = 0;
    taskMain.tmrSpec.it_interval.tv_nsec = 0;

    taskMain.tmrSpec.it_value.tv_sec = 0;
    taskMain.tmrSpec.it_value.tv_nsec = 1;

    if(timerfd_settime(taskMain.fdTmr, 0, &taskMain.tmrSpec, NULL) != 0)
        CO_errExit("taskMain_init - timerfd_settime failed");

    taskMain.tmr1msPrev = 0;
    taskMain.maxTime = maxTime;
}


void taskMain_close(void) {
    close(taskMain.fdPipe[0]);
    close(taskMain.fdPipe[1]);
    close(taskMain.fdTmr);
}


bool_t taskMain_process(int fd, CO_NMT_reset_cmd_t *reset, uint16_t timer1ms) {
    bool_t wasProcessed = true;

    /* Signal from pipe, consume all bytes. */
    if(fd == taskMain.fdPipe[0]) {
        for(;;) {
            char ch;
            if(read(taskMain.fdPipe[0], &ch, 1) == -1) {
                if (errno == EAGAIN)
                    break;  /* No more bytes. */
                else
                    CO_error(0x21100000L + errno);
            }
        }
    }

    /* Timer expired. */
    else if(fd == taskMain.fdTmr) {
        uint64_t tmrExp;
        if(read(taskMain.fdTmr, &tmrExp, sizeof(tmrExp)) != sizeof(uint64_t))
            CO_error(0x21200000L + errno);
    }
    else {
        wasProcessed = false;
    }

    /* Process mainline. */
    if(wasProcessed) {
        uint16_t timer1msDiff;
        uint16_t timerNext = 50;

        /* Calculate time difference */
        timer1msDiff = timer1ms - taskMain.tmr1msPrev;
        taskMain.tmr1msPrev = timer1ms;

        /* Calculate maximum interval in milliseconds (informative) */
        if(taskMain.maxTime != NULL) {
            if(timer1msDiff > *taskMain.maxTime) {
                *taskMain.maxTime = timer1msDiff;
            }
        }


        /* CANopen process */
        *reset = CO_process(CO, timer1msDiff, &timerNext);


        /* Set delay for next sleep. */
        taskMain.tmrSpec.it_value.tv_nsec = (long)(++timerNext) * NSEC_PER_MSEC;
        if(timerfd_settime(taskMain.fdTmr, 0, &taskMain.tmrSpec, NULL) == -1)
            CO_error(0x21500000L + errno);

    }

    return wasProcessed;
}


void taskMain_cbSignal(void) {
    if(write(taskMain.fdPipe[1], "x", 1) == -1)
        CO_error(0x23100000L + errno);
}


/* Realtime task (taskRT) *****************************************************/
static struct {
    int                 fdRx0;          /* file descriptor for CANrx */
    int                 fdTmr;          /* file descriptor for taskTmr */
    struct itimerspec   tmrSpec;
    struct timespec    *tmrVal;
    long                intervalns;
    long                intervalus;
    uint16_t           *maxTime;
} taskRT;


void CANrx_taskTmr_init(int fdEpoll, long intervalns, uint16_t *maxTime) {
    struct epoll_event ev;

    /* get file descriptors */
    taskRT.fdRx0 = CO->CANmodule[0]->fd;

    taskRT.fdTmr = timerfd_create(CLOCK_MONOTONIC, 0);
    if(taskRT.fdTmr == -1)
        CO_errExit("CANrx_taskTmr_init - timerfd_create failed");

    /* add events for epoll */
    ev.events = EPOLLIN;
    ev.data.fd = taskRT.fdRx0;
    if(epoll_ctl(fdEpoll, EPOLL_CTL_ADD, taskRT.fdRx0, &ev) == -1)
        CO_errExit("CANrx_taskTmr_init - epoll_ctl CANrx failed");

    ev.events = EPOLLIN;
    ev.data.fd = taskRT.fdTmr;
    if(epoll_ctl(fdEpoll, EPOLL_CTL_ADD, taskRT.fdTmr, &ev) == -1)
        CO_errExit("CANrx_taskTmr_init - epoll_ctl taskTmr failed");

    /* Prepare timer (one shot, each time calculate new expiration time) It is
     * necessary not to use taskRT.tmrSpec.it_interval, because it is sliding. */
    taskRT.tmrSpec.it_interval.tv_sec = 0;
    taskRT.tmrSpec.it_interval.tv_nsec = 0;

    taskRT.tmrVal = &taskRT.tmrSpec.it_value;
    if(clock_gettime(CLOCK_MONOTONIC, taskRT.tmrVal) != 0)
        CO_errExit("CANrx_taskTmr_init - clock_gettime failed");

    if(timerfd_settime(taskRT.fdTmr, TFD_TIMER_ABSTIME, &taskRT.tmrSpec, NULL) != 0)
        CO_errExit("CANrx_taskTmr_init - timerfd_settime failed");

    taskRT.intervalns = intervalns;
    taskRT.intervalus = intervalns / 1000;
    taskRT.maxTime = maxTime;
}


void CANrx_taskTmr_close(void) {
    close(taskRT.fdTmr);
}


bool_t CANrx_taskTmr_process(int fd) {
    bool_t wasProcessed = true;

    /* Get received CAN message. */
    if(fd == taskRT.fdRx0) {
        CO_CANrxWait(CO->CANmodule[0]);
    }

    /* Execute taskTmr */
    else if(fd == taskRT.fdTmr) {
        uint64_t tmrExp;

        /* Wait for timer to expire */
        if(read(taskRT.fdTmr, &tmrExp, sizeof(tmrExp)) != sizeof(uint64_t))
            CO_error(0x22100000L + errno);

        /* Calculate maximum interval in microseconds (informative) */
        if(taskRT.maxTime != NULL) {
            struct timespec tmrMeasure;
            if(clock_gettime(CLOCK_MONOTONIC, &tmrMeasure) == -1)
                CO_error(0x22200000L + errno);
            if(tmrMeasure.tv_sec == taskRT.tmrVal->tv_sec) {
                long dt = tmrMeasure.tv_nsec - taskRT.tmrVal->tv_nsec;
                dt /= 1000;
                dt += taskRT.intervalus;
                if(dt > 0xFFFF) {
                    *taskRT.maxTime = 0xFFFF;
                }else if(dt > *taskRT.maxTime) {
                    *taskRT.maxTime = (uint16_t) dt;
                }
            }
        }

        /* Calculate next shot for the timer */
        taskRT.tmrVal->tv_nsec += taskRT.intervalns;
        if(taskRT.tmrVal->tv_nsec >= NSEC_PER_SEC) {
            taskRT.tmrVal->tv_nsec -= NSEC_PER_SEC;
            taskRT.tmrVal->tv_sec++;
        }
        if(timerfd_settime(taskRT.fdTmr, TFD_TIMER_ABSTIME, &taskRT.tmrSpec, NULL) == -1)
            CO_error(0x22300000L + errno);


        /* Lock PDOs and OD */
        CO_LOCK_OD();

        if(CO->CANmodule[0]->CANnormal) {
            bool_t syncWas;

            /* Process Sync and read inputs */
            syncWas = CO_process_SYNC_RPDO(CO, taskRT.intervalus);

            /* Further I/O or nonblocking application code may go here. */

            /* Write outputs */
            CO_process_TPDO(CO, syncWas, taskRT.intervalus);
        }

        /* Unlock */
        CO_UNLOCK_OD();
    }

    else {
        wasProcessed = false;
    }

    return wasProcessed;
}
