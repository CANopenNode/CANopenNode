#ifndef CO_PollingTimerH
#define CO_PollingTimerH
//===========================================================================
/// \file    PollingTimer.h
/// \author  Uwe Kindler (UK)
/// \date    2013/08/28
/// \brief   Declaration of simple polling timer class.
//===========================================================================


//===========================================================================
//                                  INCLUDES
//===========================================================================
#include <cyg/infra/cyg_type.h>


///
/// Query actual time in milliseconds.
/// \return Actual time in milliseconds since system start
/// An implementation for a certain platform needs to implement this
/// static function
///
cyg_uint64 CO_TmrGetMilliSec(void);

/**
 * Returns the expired time since given timestamp
 * \return getMilliSec() - LastTimeStamp
 */
cyg_uint64 CO_TmrGetElapsedMsecs(cyg_uint64 LastTimeStamp);

///
/// Returns true if timer is expired
///
bool CO_TmrIsExpired(cyg_uint64 LastTimeStamp);


///
/// Set timer expiration time starting from given dwStartTime.
/// \param[in] dwStartTime Time in milliseconds when timer starts
/// \param[in] dwPeriod    Timer period in milliseconds
///
cyg_uint64 CO_TmrStartFrom(cyg_uint64 dwStartTime, cyg_uint64 dwPeriod);

///
/// Set timer expiration time starting from now.
/// Function sets a new expiration time. Expiration time is calculated from
/// actual time + timer period.
/// \param[in] dwPeriod Timer period in milliseconds
///
cyg_uint64 CO_TmrStartFromNow(cyg_uint64 dwPeriod);


//----------------------------------------------------------------------------
#endif // CO_PollingTimerH
