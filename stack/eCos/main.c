//============================================================================
// Name        : CANopenNodeEcos.cpp
// Author      : Uwe Kindler
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================


//============================================================================
//                                   INCLUDES
//============================================================================
#include <cyg/kernel/kapi.h> // kernel API
#include <cyg/infra/diag.h>
#include <cyg/infra/cyg_trac.h>

#include "CANopen.h"
#include "application.h"
#include "CO_PollingTimer.h"
#include "CO_Flash.h"


//============================================================================
// Global variables and objects
//============================================================================
static cyg_uint64 CANopenPollingTimer = 1;
extern struct sCO_OD_ROM CO_OD_ROM;
static uint8_t reset = CO_RESET_NOT;
extern void CO_eCos_errorReport(CO_EM_t *em, const uint8_t errorBit,
	const uint16_t errorCode, const uint32_t infoCode);


//===========================================================================
// CANopen stack main entry point
//===========================================================================
void CO_main(void)
{
	CO_FlashInit();
	while (1)
	{
		reset = CO_RESET_NOT;
		// Application interface
		programStart();

		// increase variable each startup. Variable is stored in EEPROM.
		OD_powerOnCounter++;
		while (reset < CO_RESET_APP)
		{
			// CANopen communication reset - initialize CANopen objects
			int16_t err;
			cyg_uint64 timer1msPrevious = CO_TmrGetMilliSec();

			// initialize CANopen
			err = CO_init();
			if(err)
			{
				CO_eCos_errorReport(CO->em, CO_EM_MEMORY_ALLOCATION_ERROR,
					CO_EMC_SOFTWARE_INTERNAL, err);
				while(1);
			}
			// register object dictionary functions to support store and restore
			// of parameters via objects 0x1010 and 0x1011
			CO_FlashRegisterODFunctions(CO);

			// initialize variables
			reset = CO_RESET_NOT;

			// Application interface
			communicationReset();

			// start CAN and enable interrupts
			CO_CANsetNormalMode(ADDR_CAN1);

			while (CO_RESET_NOT == reset)
			{
				//diag_print_reg("CAN GSR", CAN_CTRL_1_REG_BASE + CANREG_GSR);
				// loop for normal program execution
				cyg_uint64 timer1ms = CO_TmrGetMilliSec();
				uint16_t timer1msDiff = timer1ms - timer1msPrevious;
				timer1msPrevious = timer1ms;

				// Application interface
				programAsync(timer1msDiff);

				// CANopen process
				reset = CO_process(CO, timer1msDiff);
				if (CO_TmrIsExpired(CANopenPollingTimer))
				{
					CANopenPollingTimer = CO_TmrStartFrom(CANopenPollingTimer, 1);
					CO_process_RPDO(CO);
					program1ms();
					CO_process_TPDO(CO);
				}
			}
		}
	}
}


//===========================================================================
// Application main entry point
//===========================================================================
int main( int argc, char *argv[])
{
	CO_main();// this function will never return
	return 0;
}

//---------------------------------------------------------------------------
