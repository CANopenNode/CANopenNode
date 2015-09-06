/*
 * Different utilities for using CANopenNode tasks from mainline.
 *
 * @file        main_utils.c
 * @author      Janez Paternoster
 * @copyright   2015 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <http://canopennode.sourceforge.net>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "main_utils.h"


/* Utilities for 'Timer interval task'. ***************************************/
int taskTmr_init(taskTmr_t *tt, long intervalns, uint16_t *maxTime){
    int ret;

    ret = tt->fd = timerfd_create(CLOCK_MONOTONIC, 0);

    /* Prepare timer (one shot, each time calculate new expiration time) It is
     * necessary not to use tt->tmrSpec.it_interval, because it is sliding. */
    tt->tmrSpec.it_interval.tv_sec = 0;
    tt->tmrSpec.it_interval.tv_nsec = 0;

    tt->tmrVal = &tt->tmrSpec.it_value;
    if(ret != -1){
        ret = clock_gettime(CLOCK_MONOTONIC, tt->tmrVal);
    }

    if(ret != -1){
        ret = timerfd_settime(tt->fd, TFD_TIMER_ABSTIME, &tt->tmrSpec, NULL);
    }

    tt->intervalns = intervalns;
    tt->intervalus = (int16_t)(intervalns / 1000);
    tt->maxTime = maxTime;

    return ret;
}

int taskTmr_wait(taskTmr_t *tt, struct timespec *sync){
    int ret = 0;
    uint64_t tmrExp;

    /* Wait for timer to expire */
    if(read(tt->fd, &tmrExp, sizeof(tmrExp)) != sizeof(uint64_t)) ret = -1;

    /* Calculate maximum interval in microseconds (informative) */
    if(tt->maxTime != NULL){
        struct timespec tmrMeasure;
        if(clock_gettime(CLOCK_MONOTONIC, &tmrMeasure) == -1) ret = -1;
        if(tmrMeasure.tv_sec == tt->tmrVal->tv_sec){
            long dt = tmrMeasure.tv_nsec - tt->tmrVal->tv_nsec;
            dt /= 1000;
            dt += tt->intervalus;
            if(dt > 0xFFFF){
                *tt->maxTime = 0xFFFF;
            }else if(dt > *tt->maxTime){
                *tt->maxTime = (uint16_t) dt;
            }
        }
    }

    /* Calculate next shot for the timer */
    tt->tmrVal->tv_nsec += tt->intervalns;
    if(tt->tmrVal->tv_nsec >= NSEC_PER_SEC){
        tt->tmrVal->tv_nsec -= NSEC_PER_SEC;
        tt->tmrVal->tv_sec++;
    }
    if(timerfd_settime(tt->fd, TFD_TIMER_ABSTIME, &tt->tmrSpec, NULL) == -1) ret = -1;

    return ret;
}


/* Utilities for 'Mainline task'. *********************************************/
int taskMain_init(taskMain_t *tt, volatile uint16_t *tmr1ms, uint16_t *maxTime){
    int ret;

    ret = tt->fd = timerfd_create(CLOCK_MONOTONIC, 0);

    tt->tmrSpec.it_interval.tv_sec = 0;
    tt->tmrSpec.it_interval.tv_nsec = 0;

    tt->tmrSpec.it_value.tv_sec = 0;
    tt->tmrSpec.it_value.tv_nsec = 1;

    tt->tmr1ms = tmr1ms;
    tt->tmr1msPrev = *tmr1ms;
    tt->maxTime = maxTime;

    if(ret != -1){
        ret = timerfd_settime(tt->fd, 0, &tt->tmrSpec, NULL);
    }

    return ret;
}

int taskMain_wait(taskMain_t *tt, uint16_t *timeDiff){
    int ret = 0;
    uint64_t tmrExp;
    uint16_t tmr1msCopy;

    /* Wait for timer to expire */
    if(read(tt->fd, &tmrExp, sizeof(tmrExp)) != sizeof(uint64_t)) ret = -1;

    /* Calculate time difference */
    tmr1msCopy = *tt->tmr1ms;
    *timeDiff = tmr1msCopy - tt->tmr1msPrev;
    tt->tmr1msPrev = tmr1msCopy;

    /* Calculate maximum interval in milliseconds (informative) */
    if(tt->maxTime != NULL){
        if(*timeDiff > *tt->maxTime){
            *tt->maxTime = *timeDiff;
        }
    }

    return ret;
}

int taskMain_setDelay(taskMain_t *tt, uint16_t delay){
    int ret = 0;

    /* Set next shot for the timer */
    tt->tmrSpec.it_value.tv_nsec = (long)(++delay) * NSEC_PER_MSEC;
    ret = timerfd_settime(tt->fd, 0, &tt->tmrSpec, NULL);

    return ret;
}


/* Helpers ********************************************************************/
void printSocketCanOptions(int fdSocket){
    struct can_filter rfilter[150];
    can_err_mask_t err_mask;
    int loopback, recv_own_msgs, enable_can_fd, i, nfilters;

    socklen_t len1 = sizeof(rfilter);
    socklen_t len2 = sizeof(err_mask);
    socklen_t len3 = sizeof(loopback);
    socklen_t len4 = sizeof(recv_own_msgs);
    socklen_t len5 = sizeof(enable_can_fd);

    getsockopt(fdSocket, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, &len1);
    getsockopt(fdSocket, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, &len2);
    getsockopt(fdSocket, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &loopback, &len3);
    getsockopt(fdSocket, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &recv_own_msgs, &len4);
    getsockopt(fdSocket, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable_can_fd, &len5);

    nfilters = len1 / sizeof(struct can_filter);
    for(i=0; i<nfilters; i++){
        printf("filter[%02d]: id=0x%08X, mask=0x%08X\n", i, rfilter[i].can_id, rfilter[i].can_mask);
    }

    printf("err_filter_mask=0x%08X, loopback=%d, recv_own_msgs=%d, enable_can_fd=%d\n",
           err_mask, loopback, recv_own_msgs, enable_can_fd);
}
