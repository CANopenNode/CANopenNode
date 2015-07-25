//===========================================================================
//                                INCLUDES
//===========================================================================
#include "CANopen.h"
#include <cyg/infra/diag.h>


//===========================================================================
extern "C" void programStart(void)
{
	diag_printf("programStart\n");
}


//===========================================================================
extern "C" void communicationReset(void)
{
	diag_printf("communicationReset\n");
}


//===========================================================================
extern "C" void programEnd(void)
{
	diag_printf("programEnd\n");
}


//===========================================================================
extern "C" void programAsync(uint16_t timer1msDiff)
{
	static uint32_t MsCount = 0;
	static uint8_t SecCount = 0;
	static uint8_t Output0 = OD_writeOutput8Bit[0];

	MsCount += timer1msDiff;
	OD_readInput8Bit[1] = MsCount;
	if (MsCount >= 1000)
	{
		diag_printf("programAsync\n");
		MsCount -= 1000;
		SecCount++;
	}

	OD_readInput8Bit[0] = SecCount;
	if (Output0 != OD_writeOutput8Bit[0])
	{
		Output0 = OD_writeOutput8Bit[0];
		diag_printf("Output0 changed: %x\n", Output0);
	}
}


//===========================================================================
extern "C" void program1ms(void)
{
	static uint32_t MsCount = 0;
	MsCount ++;
	if (MsCount >= 1000)
	{
		diag_printf("program1ms\n");
		MsCount -= 1000;
	}
}


//---------------------------------------------------------------------------
// EOF application.cpp

