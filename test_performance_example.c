#include "can_controller_factory.h"
#include "canopen_controller_factory.h"
#include "database_factory.h"
#include "debug.h"
#include "rp2040_regs.h"
#include "timer_controller_factory.h"

int main (){
    CanOpenInterface* canopen = getCanOpenFactory();
    CanInterface* can = getCanFactory();
    TimerInterface* timer = getTimerControllerFactory();

    TEST_ASSERT_NOT_NULL(canopen);
    TEST_ASSERT_NOT_NULL(can);

    TEST_ASSERT_TRUE_MESSAGE(canopen->configure(can, 500, 1), "configure failed");
    CO_ReturnError_t err = canopen->appConfigLoop();
    TEST_ASSERT_EQUAL_MESSAGE(CO_ERROR_NO, err, "appConfigLoop failed");

    // uint64_t iterations = 1'000;
    uint64_t iterations = 1;
    const uint64_t start_ms = timer->millis();
    uint8_t statusLed = 0;
    uint8_t errorLed = 0;
    CO_NMT_reset_cmd_t reset = CO_RESET_COMM;
    uint64_t init_us = timer->micros();
    uint64_t total_time_us = 0;

    for (uint64_t i = 0; i < iterations; i++) {
        uint64_t iter_start_us = timer->micros();

        if (reset == CO_RESET_APP) {
            canopen->reset();
            canopen->end();
            LOG_ERROR("reset");
            break;
        } else if (reset != CO_RESET_NOT) {
            timer->delaySec(1);
            CO_ReturnError_t err = canopen->appConfigLoop();
            if (err != CO_ERROR_NO) {
                LOG_ERROR("appConfigLoop failed");
                // return err;
            }
            reset = CO_RESET_NOT;
            // continue;
        } else if (reset == CO_RESET_NOT) {
            reset = canopen->appExecLoop(timer->micros(), statusLed, errorLed);
            // continue;
        }

        uint64_t iter_end_us = timer->micros();
        uint64_t iter_time_us = iter_end_us - iter_start_us;
        total_time_us += iter_time_us;

        if (i % 1000000 == 0) {
            uint64_t end_us = timer->micros();
            LOG_INFO("Iteration: %lu - %lu us", i, end_us - init_us);
            init_us = timer->micros();
        }
    }

    const uint64_t end_ms = timer->millis();
    const uint64_t duration_ms = end_ms - start_ms;
    LOG_INFO("Duration: %lu ms\n", duration_ms);
    LOG_INFO("Average time per iteration: %.2f us\n", static_cast<double>(total_time_us) / iterations);

}