/*
 * CANopen main program file.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        main_generic.c
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
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


#include <stdio.h>

#include "CANopen.h"


#define TMR_TASK_INTERVAL   (1000)          /* Interval of tmrTask thread in microseconds */
#define INCREMENT_1MS(var)  (var++)         /* Increment 1ms variable in tmrTask */

#define log_printf(macropar_message, ...) \
        printf(macropar_message, ##__VA_ARGS__)


/* Global variables and objects */
    volatile static bool_t CANopenConfiguredOK = false; /* Indication if CANopen modules are configured */
    volatile uint16_t   CO_timer1ms = 0U;   /* variable increments each millisecond */
    uint8_t LED_red, LED_green;



/* main ***********************************************************************/
int main (void){
    CO_ReturnError_t err;
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    uint32_t heapMemoryUsed;
    void *CANmoduleAddress = NULL; /* CAN module address */
    uint8_t pendingNodeId = 10; /* read from dip switches or nonvolatile memory, configurable by LSS slave */
    uint8_t activeNodeId = 10; /* Copied from CO_pendingNodeId in the communication reset section */
    uint16_t pendingBitRate = 125;  /* read from dip switches or nonvolatile memory, configurable by LSS slave */

    /* Configure microcontroller. */


    /* Allocate memory */
    err = CO_new(&heapMemoryUsed);
    if (err != CO_ERROR_NO) {
        log_printf("Error: Can't allocate memory\n");
        return 0;
    }
    else {
        log_printf("Allocated %d bytes for CANopen objects\n", heapMemoryUsed);
    }

    /* initialize EEPROM */


    /* increase variable each startup. Variable is stored in EEPROM. */
    OD_powerOnCounter++;

    log_printf("CANopenNode - Reset application, count = %d\n", OD_powerOnCounter);


    while(reset != CO_RESET_APP){
/* CANopen communication reset - initialize CANopen objects *******************/
        uint16_t timer1msPrevious;

        log_printf("CANopenNode - Reset communication...\n");

        /* disable CAN and CAN interrupts */
        CANopenConfiguredOK = false;

        /* initialize CANopen */
        err = CO_CANinit(CANmoduleAddress, pendingBitRate);
        if (err != CO_ERROR_NO) {
            log_printf("Error: CAN initialization failed: %d\n", err);
            return 0;
        }
        err = CO_LSSinit(&pendingNodeId, &pendingBitRate);
        if(err != CO_ERROR_NO) {
            log_printf("Error: LSS slave initialization failed: %d\n", err);
            return 0;
        }
        activeNodeId = pendingNodeId;
        err = CO_CANopenInit(activeNodeId);
        if(err == CO_ERROR_NO) {
            CANopenConfiguredOK = true;
        }
        else if(err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) {
            log_printf("Error: CANopen initialization failed: %d\n", err);
            return 0;
        }

        /* Configure Timer interrupt function for execution every 1 millisecond */


        /* Configure CAN transmit and receive interrupt */


        /* Configure CANopen callbacks, etc */
        if(CANopenConfiguredOK) {

        }


        /* start CAN */
        CO_CANsetNormalMode(CO->CANmodule[0]);

        reset = CO_RESET_NOT;
        timer1msPrevious = CO_timer1ms;

        log_printf("CANopenNode - Running...\n");
        fflush(stdout);

        while(reset == CO_RESET_NOT){
/* loop for normal program execution ******************************************/
            uint16_t timer1msCopy, timer1msDiff;

            timer1msCopy = CO_timer1ms;
            timer1msDiff = timer1msCopy - timer1msPrevious;
            timer1msPrevious = timer1msCopy;


            /* CANopen process */
            reset = CO_process(CO, (uint32_t)timer1msDiff*1000, NULL);
            LED_red = CO_LED_RED(CO->LEDs, CO_LED_CANopen);
            LED_green = CO_LED_GREEN(CO->LEDs, CO_LED_CANopen);

            /* Nonblocking application code may go here. */

            /* Process EEPROM */

            /* optional sleep for short time */
        }
    }


/* program exit ***************************************************************/
    /* stop threads */


    /* delete objects from memory */
    CO_delete((void*) 0/* CAN module address */);

    log_printf("CANopenNode finished\n");

    /* reset */
    return 0;
}


/* timer thread executes in constant intervals ********************************/
void tmrTask_thread(void){

    for(;;) {

        /* sleep for interval */

        INCREMENT_1MS(CO_timer1ms);

        if(CO->CANmodule[0]->CANnormal) {
            bool_t syncWas;

            /* Process Sync */
            syncWas = CO_process_SYNC(CO, TMR_TASK_INTERVAL, NULL);

            /* Read inputs */
            CO_process_RPDO(CO, syncWas);

            /* Further I/O or nonblocking application code may go here. */

            /* Write outputs */
            CO_process_TPDO(CO, syncWas, TMR_TASK_INTERVAL, NULL);

            /* verify timer overflow */
            if(0) {
                CO_errorReport(CO->em, CO_EM_ISR_TIMER_OVERFLOW, CO_EMC_SOFTWARE_INTERNAL, 0U);
            }
        }
    }
}


/* CAN interrupt function executes on received CAN message ********************/
void /* interrupt */ CO_CAN1InterruptHandler(void){

    /* clear interrupt flag */
}
