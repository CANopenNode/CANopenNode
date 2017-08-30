/*
 * os_command_1023.h
 *
 *  Created on: 5 Dec 2016 г.
 *      Author: a.smirnov
 *
 *      Function for accessing "OS command" (index 0x1023) from SDO server
 */

#ifndef OS_COMMAND_1023_H_
#define OS_COMMAND_1023_H_

/** Includes *****************************************************************/
#include "CANopen.h"

/** Defines ******************************************************************/
/** Typedef, enumeration, class **********************************************/


/**
 *  OS command states
 *  For additional info see CiA 301, subclause 7.4.8.6
 */
typedef enum  {
  OS_COMMAND_NO_ERROR_NO_REPLY    = 0,        // Command completed – no error – no reply
  OS_COMMAND_NO_ERROR_REPLY       = 1,        // Command completed – no error – reply
  OS_COMMAND_ERROR_NO_REPLY       = 2,        // Command completed – error – no reply
  OS_COMMAND_ERROR_REPLY          = 3,        // Command completed – error – reply
  OS_COMMAND_EXECUTING            = 0xFF,     // Command executing
} OS_Command_State_t;

/** Exported functions prototypes ********************************************/

/**
 *  @brief  Execute command from OD_OSCommand_t.command
 *  @note   Implementation is user-depended
 *
 *  @param  none
 *  @return Command state
 */
OS_Command_State_t nExecuteOSCommand(void);

/**
 * Function for accessing "OS command" (index 0x1023) from SDO server
 */
CO_SDO_abortCode_t CO_ODF_1023(CO_ODF_arg_t *ODF_arg);

#endif /* OS_COMMAND_1023_H_ */
