//============================================================================
/// \file   ecos_helper.cpp
/// \author Uwe Kindler
/// \date   05.09.2013
/// \brief  Implementation of ecos_helper
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include <cyg/kernel/kernel.hxx>


//============================================================================
extern "C" cyg_tick_count convertMsToTicks(cyg_tick_count Milliseconds)
{
	static struct Cyg_Clock::converter ms_converter;
	static volatile cyg_atomic conv_init = 0;
	if (!conv_init)
	{
		struct Cyg_Clock::converter temp_ms_converter;
		Cyg_Clock::real_time_clock->get_other_to_clock_converter(
			1000000, &temp_ms_converter);
		Cyg_Scheduler::lock();
		ms_converter = temp_ms_converter;
		conv_init = 1;
		Cyg_Scheduler::unlock();
	}

	return Cyg_Clock::convert(Milliseconds, &ms_converter);
}


//============================================================================
extern "C" cyg_tick_count convertTicksToMs(cyg_tick_count ClockTicks)
{
	static struct Cyg_Clock::converter ms_converter;
	static volatile cyg_atomic conv_init = 0;
	if (!conv_init)
	{
		struct Cyg_Clock::converter temp_ms_converter;
		Cyg_Clock::real_time_clock->get_clock_to_other_converter(
			1000000, &temp_ms_converter);
		Cyg_Scheduler::lock();
		ms_converter = temp_ms_converter;
		conv_init = 1;
		Cyg_Scheduler::unlock();
	}

	return Cyg_Clock::convert(ClockTicks, &ms_converter);
}



//---------------------------------------------------------------------------
// EOF ecos_helper.cpp
