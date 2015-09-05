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


/******************************************************************************/
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
    uint64_t tmrExp;
    int ret = 0;

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

