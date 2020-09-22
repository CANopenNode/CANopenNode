/*
 * CANopen Indicator specification (CiA 303-3 v1.4.0)
 *
 * @file        CO_LEDs.h
 * @ingroup     CO_LEDs
 * @author      Janez Paternoster
 * @copyright   2020 Janez Paternoster
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

#include "303/CO_LEDs.h"

#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE

/******************************************************************************/
CO_ReturnError_t CO_LEDs_init(CO_LEDs_t *LEDs) {
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if (LEDs == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* clear the object */
    memset(LEDs, 0, sizeof(CO_LEDs_t));

    return ret;
}


/******************************************************************************/
void CO_LEDs_process(CO_LEDs_t *LEDs,
                     uint32_t timeDifference_us,
                     CO_NMT_internalState_t NMTstate,
                     bool_t LSSconfig,
                     bool_t ErrCANbusOff,
                     bool_t ErrCANbusWarn,
                     bool_t ErrRpdo,
                     bool_t ErrSync,
                     bool_t ErrHbCons,
                     bool_t ErrOther,
                     bool_t firmwareDownload,
                     uint32_t *timerNext_us)
{
    (void)timerNext_us; /* may be unused */

    uint8_t rd = 0;
    uint8_t gr = 0;
    bool_t tick = false;

    LEDs->LEDtmr50ms += timeDifference_us;
    while (LEDs->LEDtmr50ms >= 50000) {
        bool_t rdFlickerNext = (LEDs->LEDred & CO_LED_flicker) == 0;

        tick = true;
        LEDs->LEDtmr50ms -= 50000;

        if (++LEDs->LEDtmr200ms > 3) {
            /* calculate 2,5Hz blinking and flashing */
            LEDs->LEDtmr200ms = 0;
            rd = gr = 0;

            if ((LEDs->LEDred & CO_LED_blink) == 0) rd |= CO_LED_blink;
            else                                    gr |= CO_LED_blink;

            switch (++LEDs->LEDtmrflash_1) {
                case 1: rd |= CO_LED_flash_1; break;
                case 2: gr |= CO_LED_flash_1; break;
                case 6: LEDs->LEDtmrflash_1 = 0; break;
                default: break;
            }
            switch (++LEDs->LEDtmrflash_2) {
                case 1: case 3: rd |= CO_LED_flash_2; break;
                case 2: case 4: gr |= CO_LED_flash_2; break;
                case 8: LEDs->LEDtmrflash_2 = 0; break;
                default: break;
            }
            switch (++LEDs->LEDtmrflash_3) {
                case 1: case 3: case 5: rd |= CO_LED_flash_3; break;
                case 2: case 4: case 6: gr |= CO_LED_flash_3; break;
                case 10: LEDs->LEDtmrflash_3 = 0; break;
                default: break;
            }
            switch (++LEDs->LEDtmrflash_4) {
                case 1: case 3: case 5: case 7: rd |= CO_LED_flash_4; break;
                case 2: case 4: case 6: case 8: gr |= CO_LED_flash_4; break;
                case 12: LEDs->LEDtmrflash_4 = 0; break;
                default: break;
            }
        }
        else {
            /* clear flicker and CANopen bits, keep others */
            rd = LEDs->LEDred & (0xFF ^ (CO_LED_flicker | CO_LED_CANopen));
            gr = LEDs->LEDgreen & (0xFF ^ (CO_LED_flicker | CO_LED_CANopen));
        }

        /* calculate 10Hz flickering */
        if (rdFlickerNext) rd |= CO_LED_flicker;
        else               gr |= CO_LED_flicker;

    } /* while (LEDs->LEDtmr50ms >= 50000) */

    if (tick) {
        uint8_t rd_co, gr_co;

        /* CANopen red ERROR LED */
        if      (ErrCANbusOff)                      rd_co = 1;
        else if (NMTstate == CO_NMT_INITIALIZING)   rd_co = rd & CO_LED_flicker;
        else if (ErrRpdo)                           rd_co = rd & CO_LED_flash_4;
        else if (ErrSync)                           rd_co = rd & CO_LED_flash_3;
        else if (ErrHbCons)                         rd_co = rd & CO_LED_flash_2;
        else if (ErrCANbusWarn)                     rd_co = rd & CO_LED_flash_1;
        else if (ErrOther)                          rd_co = rd & CO_LED_blink;
        else                                        rd_co = 0;

        /* CANopen green RUN LED */
        if      (LSSconfig)                         gr_co = gr & CO_LED_flicker;
        else if (firmwareDownload)                  gr_co = gr & CO_LED_flash_3;
        else if (NMTstate == CO_NMT_STOPPED)        gr_co = gr & CO_LED_flash_1;
        else if (NMTstate == CO_NMT_PRE_OPERATIONAL)gr_co = gr & CO_LED_blink;
        else if (NMTstate == CO_NMT_OPERATIONAL)    gr_co = 1;
        else                                        gr_co = 0;

        if (rd_co != 0) rd |= CO_LED_CANopen;
        if (gr_co != 0) gr |= CO_LED_CANopen;
        LEDs->LEDred = rd;
        LEDs->LEDgreen = gr;
    } /* if (tick) */

#if (CO_CONFIG_LEDS) & CO_CONFIG_FLAG_TIMERNEXT
    if (timerNext_us != NULL) {
        uint32_t diff = 50000 - LEDs->LEDtmr50ms;
        if (*timerNext_us > diff) {
            *timerNext_us = diff;
        }
    }
#endif
}

#endif /* (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE */
