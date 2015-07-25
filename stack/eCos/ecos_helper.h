#ifndef ecos_helperH
#define ecos_helperH
//============================================================================
/// \file   ecos_helper.h
/// \author Uwe Kindler
/// \date   05.09.2013
/// \brief  Declaration of ecos_helper
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include <cyg/kernel/kapi.h>
#include <cyg/kernel/ktypes.h>


/**
 * Converts milliseconds to eCos clock ticks
 */
cyg_tick_count convertMsToTicks(cyg_tick_count Milliseconds);

/**
 * Converts eCos clock ticks to milliseconds
 */
cyg_tick_count convertTicksToMs(cyg_tick_count ClockTicks);


//---------------------------------------------------------------------------
#endif // ecos_helperH
