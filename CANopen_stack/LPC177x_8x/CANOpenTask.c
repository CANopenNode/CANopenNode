/*--------------------------------------------------------------*/
/*            GENESYS ELECTRONICS DESIGN PTY LTD                */
/*--------------------------------------------------------------*/
/*                                                              */
/* FILE NAME :  CANOpenTask.h                                   */
/*                                                              */
/* REVISION NUMBER: 1.0                                         */
/*                                                              */
/* PURPOSE : This is the implementation of the CANOpen main task*/
/*                                                              */
/*                                                              */
/*--------------------------------------------------------------*/
/*                                                              */
/* AUTHOR:                                                      */
/*  Harshah Sagar                                               */
/*  GENESYS ELECTRONICS DESIGN Pty Ltd                          */
/*  Unit 5/33 Ryde Road                                         */
/*  Pymble NSW 2073                                             */
/*  Australia.                                                  */
/*  Telephone # 61-02-9496 8900                                 */
/*--------------------------------------------------------------*/
/* Revision Number :                                            */
/* Revision By     : Amit Hergass                               */
/* Date            : 30-Jun-2014                                */
/* Description     : Original Issue                             */
/*                                                              */
/* !--------------------------------------------------------!   */
/* ! Revisions to be entered in reverse chronological order !   */
/* !--------------------------------------------------------!   */
/*                                                              */
/*--------------------------------------------------------------*/

#include "FreeRTOS.h"
#include "task.h"

#include "board.h"
#include "Watchdog.h"
#include "CANopen.h"
#include "application.h"
#include "CANOpenTask.h"

/* Global variables and objects */
volatile uint16_t CO_timer1ms = 0U; /* variable increments each millisecond */
static bool canStackEnabled = true;
void CO_TimerProcess(void);

#ifdef TASK_WATCHDOG_ENABLED
static hWatchdog			CanOpenWdHandler;
#endif
/* main ***********************************************************************/
void CANOpenTask ( void *pvParameters )
{
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;

    /* Configure microcontroller. */
#ifdef TASK_WATCHDOG_ENABLED								
		/* wait number of the task iteration delay time (#seconds until WD expires * (1000ms/WD task frequency))
		 Give the task 20 seconds to recover*/
		CanOpenWdHandler = watchogTaskRegister( "CANOpen", (20 * (1000 / WATCHDOG_TASK_FREQUENCY_MS))); 
#endif	
    while(true)
    {
        reset = CO_RESET_NOT;
        /* TODO: initialize EEPROM */

            /*  Todo: Loading COD */
        DEBUGOUT("CANOpenTask() Loading COD\n\r");

        /* Verify, if OD structures have proper alignment of initial values */
        DEBUGOUT("Checking COD in RAM (size=%d)\n\r", &CO_OD_RAM.LastWord - &CO_OD_RAM.FirstWord);
        if (CO_OD_RAM.FirstWord != CO_OD_RAM.LastWord)
            DEBUGOUT("Err COD in RAM\n\r");
        DEBUGOUT("Checking COD in EEPROM (size=%d)\n\r", &CO_OD_EEPROM.LastWord - &CO_OD_EEPROM.FirstWord);
        if (CO_OD_EEPROM.FirstWord != CO_OD_EEPROM.LastWord)
            DEBUGOUT("Err COD in EEPROM\n\r");
        DEBUGOUT("Checking COD in ROM (size=%d)\n\r", &CO_OD_ROM.LastWord - &CO_OD_ROM.FirstWord);
        if (CO_OD_ROM.FirstWord != CO_OD_ROM.LastWord)
            DEBUGOUT("Err COD in ROM\n\r");

        /* Application interface */
        programStart();


        /* increase variable each startup. Variable is stored in EEPROM. */
        OD_powerOnCounter++;


        DEBUGOUT("CO power-on (BTR=%dk Node=0x%x)\n\r", CO_OD_ROM.CANBitRate, CO_OD_ROM.CANNodeID);
        
        while((reset != CO_RESET_APP)&& (true == canStackEnabled)){
			/* CANopen communication reset - initialize CANopen objects *******************/
            CO_ReturnError_t err;
            uint16_t timer1msPrevious=0;

            /* disable timer and CAN interrupts */
            NVIC_DisableIRQ(CAN_IRQn);
            
            /* initialize CANopen */
            err = CO_init();
            if(err != CO_ERROR_NO){
                DEBUGOUT("CANOpenTask CO_init() Failed!!!\n\r");
                CO_errorReport(CO->em, CO_EM_MEMORY_ALLOCATION_ERROR, CO_EMC_SOFTWARE_INTERNAL, err); 
                while(1)
                {
                    vTaskDelay(1);
                };
            }
            /* initialize variables */
            timer1msPrevious = CO_timer1ms;
            reset = CO_RESET_NOT;

            /* Application interface */
            communicationReset();

            /* start CAN and enable interrupts */
            CO_CANsetNormalMode(ADDR_CAN1);


            while((reset == CO_RESET_NOT) && (true == canStackEnabled)){
    /* loop for normal program execution ******************************************/
                uint16_t timer1msDiff;

                CO_DISABLE_INTERRUPTS();
				/*execution every 1 millisecond */
                timer1msDiff = CO_timer1ms - timer1msPrevious;
                if(0 < timer1msDiff)
                {
                    CO_TimerProcess();
                }
                else
                {
                    vTaskDelay(1);
                }
                timer1msPrevious = CO_timer1ms;
                CO_ENABLE_INTERRUPTS();

                /* Application interface */
                programAsync(timer1msDiff);

                /* CANopen process */
                reset = CO_process(CO, timer1msDiff);
                /* Process EEPROM */
				
#ifdef TASK_WATCHDOG_ENABLED
				watchogTaskFeed(CanOpenWdHandler);
#endif	
            }
    }

    DEBUGOUT("CANOpenTask Terminated reset= %d ,canStackEnabled= %d !!!\n\r", reset,canStackEnabled);
    
    /* program exit ***************************************************************/

    /* Application interface */
    programEnd();

    /* delete objects from memory */
    CO_delete();
    }

    //Exit the task
    vTaskDelete( NULL );

}


/* timer interrupt every millisecond ************************/
/*void vApplicationTickHook(void) since the tick is 1ms we can use the FreeRTOS tick hook*/
void /* interrupt */ CO_TimerInterruptHandler(void){

    /* clear interrupt flag */
    CO_timer1ms++;
}

/* function executes every millisecond ************************/
void CO_TimerProcess(void){

    CO_process_RPDO(CO);

    /* Application interface */
    program1ms();

    CO_process_TPDO(CO);

    /* verify timer overflow (is flag set again?) */
    if(0){
        CO_errorReport(CO->em, CO_EM_ISR_TIMER_OVERFLOW, CO_EMC_SOFTWARE_INTERNAL, 0U);
    }
}


/* CAN interrupt function *****************************************************/
void /* interrupt */ CAN_IRQHandler(void){ /* CO_CAN1InterruptHandler(void){*/
    CO_CANinterrupt(CO->CANmodule[0]);
    /* clear interrupt flag */
}

/* CAN_StackEnable() CAN Task Enable/Disable *****************************************************/
/*enableSwitch: true- Task enables, false- Task disabled */
void CAN_StackEnable(bool enableSwitch){
   /* CAN Task can only be terminated, not restarted at this stage*/
   if(canStackEnabled)
   {
    canStackEnabled = enableSwitch;
   }
}
/* CAN_GetStackEnable *****************************************************/
/*Return: true- Task enables, false- Task disabled */
bool CAN_GetStackEnable(void){
   return canStackEnabled;
}
