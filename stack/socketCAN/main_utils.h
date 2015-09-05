/*
 * Different utilities for using CANopenNode tasks from mainline.
 *
 * @file        main_utils.h
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


#ifndef MAIN_UTILS_H
#define MAIN_UTILS_H


#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sys/timerfd.h>


#define NSEC_PER_SEC        (1000000000)        /* The number of nanoseconds per second. */
#define NSEC_PER_MSEC       (1000000)           /* The number of nanoseconds per millisecond. */


/* Utilities for 'Timer interval task'. ***************************************/
/* It is used for short, realtime processing of CANopen SYNC and PDO objects,
 * triggered in constant intervals.*/
typedef struct{
    int                 fd;         /* file descriptor */
    struct itimerspec   tmrSpec;
    struct timespec    *tmrVal;
    long                intervalns;
    uint16_t            intervalus;
    uint16_t           *maxTime;
}taskTmr_t;


/* Create Linux timerfd and configures tt object.
 *
 * @param tt This object.
 * @param intervalns Interval in nanoseconds.
 * @param maxTime Pointer to value, which will indicate longest interval in
 *        microseconds. If NULL, calculations won't be made.
 *
 * @return 0 on success, -1 on error, info in errno.
 */
int taskTmr_init(taskTmr_t *tt, long intervalns, uint16_t *maxTime);


/* Block in equal intervals.
 *
 * @param tt This object.
 * @param sync Time of CANopen SYNC signal. If not NULL, taskTmr will
 *        synchronize with SYNC signal. Not yet implemented.
 *
 * @return 0 on success, -1 on error, info in errno.
 */
int taskTmr_wait(taskTmr_t *tt, struct timespec *sync);


#endif
