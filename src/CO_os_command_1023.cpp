/*
 * os_command_1023.cpp
 *
 *  Created on: 5 Dec 2016 г.
 *      Author: a.smirnov
 *
 *      Function for accessing "OS command" (index 0x1023) from SDO server
 */

/** Includes *****************************************************************/
#include "CO_os_command_1023.h"
#include "CO_OD.h"

/** Exported functions *******************************************************/

CO_SDO_abortCode_t CO_ODF_1023(CO_ODF_arg_t* ODF_arg)   {

  // processing write attempt to OD_OSCommand_t.command
  if ( (ODF_arg->subIndex == 1) && (!ODF_arg->reading) )  {

    uint8_t* SDObuffer = ODF_arg->data;                     // pointer to SDO buffer
    uint8_t* ODdata = (uint8_t*) ODF_arg->ODdataStorage;    // pointer to OD_OSCommand_t.command
    uint16_t length = ODF_arg->dataLength;

    // copy data from SDO buffer to OD
    while (length--)
      *(ODdata++) = *(SDObuffer++);
    *(ODdata++) = 0;                    // string end

    // execute command
    OD_OSCommand.status = nExecuteOSCommand();
  }//( (ODF_arg->subIndex == 1) && (!ODF_arg->reading) )

  return CO_SDO_AB_NONE;

}//CO_ODF_1023 ----------------------------------------------------------------
