//========================================================================//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef AI_NAVTYPE_H
#define AI_NAVTYPE_H

#if defined( _WIN32 )
#pragma once
#endif

// ---------------------------
//  Navigation Type Bits
// ---------------------------
enum Navigation_t 
{
	NAV_NONE = -1,	// error condition
	NAV_GROUND = 0,	// walk/run
	NAV_JUMP,		// jump/leap
	NAV_FLY,		// can fly, move all around
	NAV_CLIMB,		// climb ladders
#ifdef DARKINTERVAL
	NAV_SWIM,		// DI NEW - water nodes
#endif
};


#endif // AI_NAVTYPE_H
