//========================================================================//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "model_types.h"
#include "vcollide.h"
#include "vcollide_parse.h"
#include "solidsetdefaults.h"
#include "bone_setup.h"
#include "engine/ivmodelinfo.h"
#include "physics.h"
#include "view.h"
#include "clienteffectprecachesystem.h"
#include "c_physicsprop.h"
#include "tier0/vprof.h"
#include "ivrenderview.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_PhysicsProp, DT_PhysicsProp, CPhysicsProp)
	RecvPropBool(RECVINFO(m_bAwake)),
#ifdef DARKINTERVAL
//	RecvPropInt(RECVINFO(m_stainCount_int)), // an experiment, nothing at the moment
#endif
END_RECV_TABLE()

#ifndef DARKINTERVAL // reducing amount of convars
ConVar r_PhysPropStaticLighting( "r_PhysPropStaticLighting", "1" );
#endif
#ifdef DARKINTERVAL
ConVar mat_object_motion_blur_scale_props("mat_object_motion_blur_scale_props", "0.5");
#endif
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PhysicsProp::C_PhysicsProp( void )
#ifdef DARKINTERVAL // object motion blur
	: m_MotionBlurObject(this, mat_object_motion_blur_scale_props.GetFloat())
#endif
{
	m_pPhysicsObject = NULL;
	m_takedamage = DAMAGE_YES;

	// default true so static lighting will get recomputed when we go to sleep
	m_bAwakeLastTime = true;
#ifdef DARKINTERVAL
//	m_stainCount_int = 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PhysicsProp::~C_PhysicsProp( void )
{
#ifdef DARKINTERVAL
//	m_stainCount_int = 0;
#endif
}

// @MULTICORE (toml 9/18/2006): this visualization will need to be implemented elsewhere
#ifdef _DEBUG 
ConVar r_visualizeproplightcaching( "r_visualizeproplightcaching", "0" );
#endif
//-----------------------------------------------------------------------------
// Purpose: Draws the object
// Input  : flags - 
//-----------------------------------------------------------------------------
bool C_PhysicsProp::OnInternalDrawModel( ClientModelRenderInfo_t *pInfo )
{
	CreateModelInstance();
#ifndef DARKINTERVAL
	if ( r_PhysPropStaticLighting.GetBool() && m_bAwakeLastTime != m_bAwake )
	{
		if (m_bAwakeLastTime && !m_bAwake)
		{
			// transition to sleep, bake lighting now, once
			if (!modelrender->RecomputeStaticLighting(GetModelInstance()))
			{
				// not valid for drawing
				return false;
			}

#ifdef _DEBUG 
			if (r_visualizeproplightcaching.GetBool())
			{
				float color[] = { 0.0f, 1.0f, 0.0f, 1.0f };
				render->SetColorModulation(color);
			}
		}
		else if (r_visualizeproplightcaching.GetBool())
		{
			float color[] = { 1.0f, 0.0f, 0.0f, 1.0f };
			render->SetColorModulation(color);
		}
#else
		}
#endif
	}

	if ( !m_bAwake && r_PhysPropStaticLighting.GetBool() )
	{
		// going to sleep, have static lighting
		pInfo->flags |= STUDIO_STATIC_LIGHTING;
	}
#endif // DARKINTERVAL
	// track state
	m_bAwakeLastTime = m_bAwake;

	return true;
}
