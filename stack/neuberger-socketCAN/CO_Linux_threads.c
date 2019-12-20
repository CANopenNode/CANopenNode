/*
 * Helper functions for implementing CANopen threads in Linux
 *
 * @file        Linux_threads.c
 * @ingroup     CO_driver
 * @author      Janez Paternoster, Martin Wagner
 * @copyright   2004 - 2015 Janez Paternoster, 2017 Neuberger Gebaeudeautomation GmbH
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
 *
 * Following clarification and special exception to the GNU General Public
 * License is included to the distribution terms of CANopenNode:
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library. Thus, the terms and
 * conditions of the GNU General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this library give
 * you permission to link this library with independent modules to
 * produce an executable, regardless of the license terms of these
 * independent modules, and to copy and distribute the resulting
 * executable under terms of your choice, provided that you also meet,
 * for each linked independent module, the terms and conditions of the
 * license of that module. An independent module is a module which is
 * not derived from or based on this library. If you modify this
 * library, you may extend this exception to your version of the
 * library, but you are not obliged to do so. If you do not wish
 * to do so, delete this exception statement from your version.
 */

#include <sys/timerfd.h>
#include <fcntl.h>
#include <unistd.h>

#include "CO_driver.h"
#include "CANopen.h"

/* Helper function - get monotonic clock time in ms */
static uint64_t CO_LinuxThreads_clock_gettime_ms(void)
{
  struct timespec ts;

  (void)clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Mainline thread (threadMain) ***************************************************/
static struct
{
  uint64_t  start;                  /* time value CO_process() was called last time in ms */
  void    (*pFunct)(void* object);  /* Callback function */
  void     *object;
} threadMain;

/**
 * This function notifies the user application after an event happened
 *
 * This is necessary because not all stack callbacks support object pointers.
 * It is not used for those callbacks that have this pointer!
 */
static void threadMain_resumeCallback(void)
{
  if (threadMain.pFunct != NULL) {
    threadMain.pFunct(threadMain.object);
  }
}

void threadMain_init(void (*callback)(void*), void *object)
{
  threadMain.start = CO_LinuxThreads_clock_gettime_ms();
  threadMain.pFunct = callback;
  threadMain.object = object;

  CO_SDO_initCallback(CO->SDO[0], threadMain_resumeCallback);
  CO_EM_initCallback(CO->em, threadMain_resumeCallback);
#if CO_NO_LSS_CLIENT == 1
  CO_LSSmaster_initCallback(CO->LSSmaster, threadMain.object, threadMain.pFunct);
#endif
#if CO_NO_SDO_CLIENT != 0
  for (int i = 0; i < CO_NO_SDO_CLIENT; i++) {
    CO_SDOclient_initCallback(CO->SDOclient[i], threadMain_resumeCallback);
  }
#endif
}

void threadMain_close(void)
{
  threadMain.pFunct = NULL;
  threadMain.object = NULL;
}

void threadMain_process(CO_NMT_reset_cmd_t *reset)
{
  uint16_t finished;
  uint16_t diff;
  uint64_t now;

  now = CO_LinuxThreads_clock_gettime_ms();
  diff = (uint16_t)(now - threadMain.start);

  /* we use timerNext_ms in CO_process() as indication if processing is
   * finished. We ignore any calculated values for maximum delay times. */
  do {
    finished = 1;
    *reset = CO_process(CO, diff, &finished);
    diff = 0;
  } while ((*reset == CO_RESET_NOT) && (finished == 0));

  /* prepare next call */
  threadMain.start = now;
}

/* Realtime thread (threadRT) *****************************************************/
static struct {
  uint32_t us_interval;         /* configured interval in us */
  int interval_fd;              /* timer fd */
} threadRT;

void CANrx_threadTmr_init(uint16_t interval)
{
  struct itimerspec itval;

  threadRT.us_interval = interval * 1000;
  /* set up non-blocking interval timer */
  threadRT.interval_fd = timerfd_create(CLOCK_MONOTONIC, 0);
  (void)fcntl(threadRT.interval_fd, F_SETFL, O_NONBLOCK);
  itval.it_interval.tv_sec = 0;
  itval.it_interval.tv_nsec = interval * 1000000;
  itval.it_value = itval.it_interval;
  (void)timerfd_settime(threadRT.interval_fd, 0, &itval, NULL);
}

void CANrx_threadTmr_close(void)
{
  (void)close(threadRT.interval_fd);
  threadRT.interval_fd = -1;
}

void CANrx_threadTmr_process(void)
{
  int32_t result;
  int32_t i;
  bool_t syncWas;
  unsigned long long missed;

  result = CO_CANrxWait(CO->CANmodule[0], threadRT.interval_fd, NULL);
  if (result < 0) {
    result = read(threadRT.interval_fd, &missed, sizeof(missed));
    if (result > 0) {
      /* at least one timer interval occured */
      CO_LOCK_OD();

      if(CO->CANmodule[0]->CANnormal) {

        for (i = 0; i <= missed; i++) {
          /* Process Sync */
          syncWas = CO_process_SYNC(CO, threadRT.us_interval);

          /* Read inputs */
          CO_process_RPDO(CO, syncWas);

          /* Write outputs */
          CO_process_TPDO(CO, syncWas, threadRT.us_interval);
        }
      }

      CO_UNLOCK_OD();
    }
  }
}
