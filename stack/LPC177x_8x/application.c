/*
 * Header for application interface for CANopenNode stack.
 *
 * @file        application.c
 * @ingroup     application
 * @version     SVN: \$Id: application.c 31 2013-03-08 17:57:40Z jani22 $
 * @author      Janez Paternoster
 * @copyright   2012 - 2013 Janez Paternoster
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
/*System includes------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <time.h>
/*Board includes-------------------------------------------------------------*/
#include "board.h"

/*App includes---------------------------------------------------------------*/
#include "COMPSW.H"
#include "CANopen.h"


#define CAN_RUN_LED_ON()  Chip_GPIO_WritePortBit(LPC_GPIO, CAN_RUN_LED_PORT, CAN_RUN_LED_PIN,false )
#define CAN_RUN_LED_OFF() Chip_GPIO_WritePortBit(LPC_GPIO, CAN_RUN_LED_PORT, CAN_RUN_LED_PIN,true)
// No HW allocation for Error LED
#define CAN_ERROR_LED_ON() 
#define CAN_ERROR_LED_OFF() 


/*******************************************************************************/
void programStart(void){

    // turn OFF the LEDs
    CAN_RUN_LED_OFF();
    CAN_ERROR_LED_OFF();
}


/*******************************************************************************/
void communicationReset(void){
    
    // turn OFF the LEDs
    CAN_RUN_LED_OFF();
    CAN_ERROR_LED_OFF();
}


/*******************************************************************************/
void programEnd(void){
    
    // turn OFF the LEDs
    CAN_RUN_LED_OFF();
    CAN_ERROR_LED_OFF();
}


/*******************************************************************************/
void programAsync(uint16_t timer1msDiff){
    
    if(LED_GREEN_RUN(CO->NMT))CAN_RUN_LED_ON();   else CAN_RUN_LED_OFF();
    if(LED_RED_ERROR(CO->NMT))CAN_ERROR_LED_ON(); else CAN_ERROR_LED_OFF();
}


/*******************************************************************************/
void program1ms(void){

}
