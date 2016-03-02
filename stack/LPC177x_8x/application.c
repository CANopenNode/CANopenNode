/*
 * Header for application interface for CANopenNode stack.
 *
 * @file        application.c
 * @ingroup     application
 * @author      Janez Paternoster
 * @copyright   2012 - 2013 Janez Paternoster
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
