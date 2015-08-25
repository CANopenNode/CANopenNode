/*
 * CANopen main program file.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        main_generic.c
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
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


#include "CANopen.h"


#define TMR_TASK_INTERVAL   (1000)          /* Interval of tmrTask thread in microseconds */
#define INCREMENT_1MS(var)  (var++)         /* Increment 1ms variable in tmrTask */


/* Global variables and objects */
    volatile uint16_t   CO_timer1ms = 0U;   /* variable increments each millisecond */
    volatile bool_t     CO_CAN_OK = false;  /* CAN in normal mode indicator */


/* main ***********************************************************************/
int main (void){
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;

    /* Configure microcontroller. */


    /* initialize EEPROM */


    /* increase variable each startup. Variable is stored in EEPROM. */
    OD_powerOnCounter++;


    while(reset != CO_RESET_APP){
/* CANopen communication reset - initialize CANopen objects *******************/
        CO_ReturnError_t err;
        uint16_t timer1msPrevious;

        /* disable timer and CAN interrupts */
        CO_CAN_OK = false;


        /* initialize CANopen */
        err = CO_init();
        if(err != CO_ERROR_NO){
            while(1);
            /* CO_errorReport(CO->em, CO_EM_MEMORY_ALLOCATION_ERROR, CO_EMC_SOFTWARE_INTERNAL, err); */
        }


        /* Configure Timer interrupt function for execution every 1 millisecond */


        /* Configure CAN transmit and receive interrupt */


        /* start CAN */
        CO_CANsetNormalMode(ADDR_CAN1);
        CO_CAN_OK = true;

        reset = CO_RESET_NOT;
        timer1msPrevious = CO_timer1ms;

        while(reset == CO_RESET_NOT){
/* loop for normal program execution ******************************************/
            uint16_t timer1msCopy, timer1msDiff;

            timer1msCopy = CO_timer1ms;
            timer1msDiff = timer1msCopy - timer1msPrevious;
            timer1msPrevious = timer1msCopy;


            /* CANopen process */
            reset = CO_process(CO, timer1msDiff);

            /* Nonblocking application code may go here. */

            /* Process EEPROM */
        }
    }


/* program exit ***************************************************************/
    CO_DISABLE_INTERRUPTS();


    /* delete objects from memory */
    CO_delete();


    /* reset */
    return 0;
}


/* timer thread executes in constant intervals ********************************/
static void tmrTask_thread(void){

    for(;;) {

        /* sleep for interval */

        INCREMENT_1MS(CO_timer1ms);

        if(CO_CAN_OK) {
            int16_t i;
            bool_t syncWas = false;

            /* Process SYNC */
            switch(CO_SYNC_process(CO->SYNC, TMR_TASK_INTERVAL, OD_synchronousWindowLength)){
                case 1: syncWas = true; break;  //immediatelly after SYNC message
                case 2: CO_CANclearPendingSyncPDOs(CO->CANmodule[0]); break; //outside SYNC window
            }

            /* Process RPDOs */
            for(i=0; i<CO_NO_RPDO; i++){
                CO_RPDO_process(CO->RPDO[i], syncWas);
            }

            /* Reenable CANrx, if it was disabled by SYNC callback */

            /* Further I/O or nonblocking application code may go here. */

            /* Verify PDO Change Of State and process PDOs */
            for(i=0; i<CO_NO_TPDO; i++){
                if(!CO->TPDO[i]->sendRequest) CO->TPDO[i]->sendRequest = CO_TPDOisCOS(CO->TPDO[i]);
                CO_TPDO_process(CO->TPDO[i], CO->SYNC, syncWas, TMR_TASK_INTERVAL);
            }
        }

    }

    /* verify timer overflow */
    if(0){
        CO_errorReport(CO->em, CO_EM_ISR_TIMER_OVERFLOW, CO_EMC_SOFTWARE_INTERNAL, 0U);
    }
}


/* CAN interrupt function *****************************************************/
void /* interrupt */ CO_CAN1InterruptHandler(void){
    CO_CANinterrupt(CO->CANmodule[0]);


    /* clear interrupt flag */
}
