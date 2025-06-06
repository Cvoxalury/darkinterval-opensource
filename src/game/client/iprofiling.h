//========================================================================//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( IPROFILING_H )
#define IPROFILING_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/vgui.h>

namespace vgui
{
	class Panel;
}

abstract_class IProfiling
{
public:
	virtual void		Create( vgui::VPANEL parent ) = 0;
	virtual void		Destroy( void ) = 0;
};

extern IProfiling *profiling;

#endif // IPROFILING_H