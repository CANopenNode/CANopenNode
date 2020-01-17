/**
 * Application interface for CANopenNode stack.
 *
 * @file        application.h
 * @ingroup     CO_application
 * @author      Janez Paternoster
 * @copyright   2012 - 2020 Janez Paternoster
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


#ifndef CO_APPLICATION_H
#define CO_APPLICATION_H


/**
 * @defgroup CO_application Application interface
 * @ingroup CO_CANopen
 * @{
 *
 * Application interface for CANopenNode stack. Function is called
 * from file main_xxx.c (if implemented).
 *
 * ###Main program flow chart
 *
 * @code
               (Program Start)
                      |
                      V
    +------------------------------------+
    |           programStart()           |
    +------------------------------------+
                      |
                      |<-------------------------+
                      |                          |
                      V                          |
             (Initialze CANopen)                 |
                      |                          |
                      V                          |
    +------------------------------------+       |
    |        communicationReset()        |       |
    +------------------------------------+       |
                      |                          |
                      V                          |
         (Enable CAN and interrupts)             |
                      |                          |
                      |<----------------------+  |
                      |                       |  |
                      V                       |  |
    +------------------------------------+    |  |
    |           programAsync()           |    |  |
    +------------------------------------+    |  |
                      |                       |  |
                      V                       |  |
        (Process CANopen asynchronous)        |  |
                      |                       |  |
                      +- infinite loop -------+  |
                      |                          |
                      +- reset communication ----+
                      |
                      V
    +------------------------------------+
    |            programEnd()            |
    +------------------------------------+
                      |
                      V
              (delete CANopen)
                      |
                      V
                (Program end)
   @endcode
 *
 *
 * ###Timer program flow chart
 *
 * @code
        (Timer interrupt 1 millisecond)
                      |
                      V
              (CANopen read RPDOs)
                      |
                      V
    +------------------------------------+
    |           program1ms()             |
    +------------------------------------+
                      |
                      V
              (CANopen write TPDOs)
   @endcode
 *
 *
 * ###Receive and transmit high priority interrupt flow chart
 *
 * @code
           (CAN receive event or)
      (CAN transmit buffer empty event)
                      |
                      V
       (Process received CAN message or)
   (copy next message to CAN transmit buffer)
   @endcode
 */


/**
 * Called after microcontroller reset.
 */
void programStart(void);


/**
 * Called after communication reset.
 */
void communicationReset(void);


/**
 * Called before program end.
 */
void programEnd(void);


/**
 * Called cyclically from main.
 *
 * @param timer1msDiff Time difference since last call
 */
void programAsync(uint16_t timer1msDiff);


/**
 * Called cyclically from 1ms timer task.
 */
void program1ms(void);


/** @} */
#endif
