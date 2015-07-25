//===========================================================================
/// \file    CO_PollingTimer.c
/// \author  Uwe Kindler (UK)
/// \date    2013/08/28
/// \brief   Implementation of simple polling timer
//===========================================================================


//===========================================================================
//                                  INCLUDES
//===========================================================================
#include <cyg/kernel/kapi.h>
#include "CO_PollingTimer.h"
#include "ecos_helper.h"



//===========================================================================
cyg_uint64 CO_TmrGetMilliSec(void)
{
	return convertTicksToMs(cyg_current_time());
}


//===========================================================================
cyg_uint64 CO_TmrGetElapsedMsecs(cyg_uint64 LastTimeStamp)
{
	return CO_TmrGetMilliSec() - LastTimeStamp;
}


//===========================================================================
bool CO_TmrIsExpired(cyg_uint64 LastTimeStamp)
{
    cyg_uint64 dwTimeNow   = CO_TmrGetMilliSec();
    cyg_uint64 dwTimestamp = (LastTimeStamp + 1);

    if (dwTimeNow > dwTimestamp)
    {
        if ((dwTimeNow - dwTimestamp) < 0x8000000000000000)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else // if (dwTimeNow  <= dwTimestamp)
    {
        if ((dwTimestamp - dwTimeNow) > 0x8000000000000000)
        {
            return true;
        }
        else
        {
            return false;
        }
    } // if (time_now <= timestamp)
}


//===========================================================================
cyg_uint64 CO_TmrStartFrom(cyg_uint64 dwStartTime, cyg_uint64 dwPeriod)
{
	cyg_uint64 TimeStamp = dwStartTime + dwPeriod;
	// if resulting time stamp is in the past, then we fix the time stamp value
	// and move it to now
	if (TimeStamp < CO_TmrGetMilliSec())
	{
		return CO_TmrGetMilliSec();
	}
	else
	{
		return TimeStamp;
	}
}

//===========================================================================
cyg_uint64 CO_TmrStartFromNow(cyg_uint64 dwPeriod)
{
	return  CO_TmrGetMilliSec() + dwPeriod;
}


//----------------------------------------------------------------------------
// end of CO_PollingTimer.c

