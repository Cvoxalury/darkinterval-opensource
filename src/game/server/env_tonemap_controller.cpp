//========================================================================//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "baseentity.h"
#include "entityoutput.h"
#include "convar.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar mat_hdr_tonemapscale( "mat_hdr_tonemapscale", "1.0", FCVAR_NONE, "The HDR tonemap scale. 1 = Use autoexposure, 0 = eyes fully closed, 16 = eyes wide open." );

// 0 - eyes fully closed / fully black
// 1 - nominal 
// 16 - eyes wide open / fully white

//-----------------------------------------------------------------------------
// Purpose: Entity that controls player's tonemap
//-----------------------------------------------------------------------------
class CEnvTonemapController : public CPointEntity
{
	DECLARE_CLASS( CEnvTonemapController, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void	Spawn( void );
	int		UpdateTransmitState( void );
	void	UpdateTonemapScaleBlend( void );

	// Inputs
	void	InputSetTonemapScale( inputdata_t &inputdata );
	void	InputBlendTonemapScale( inputdata_t &inputdata );
	void	InputSetTonemapRate( inputdata_t &inputdata );
	void	InputSetAutoExposureMin( inputdata_t &inputdata );
	void	InputSetAutoExposureMax( inputdata_t &inputdata );
	void	InputUseDefaultAutoExposure( inputdata_t &inputdata );
	void	InputSetBloomScale( inputdata_t &inputdata );
	void	InputUseDefaultBloomScale( inputdata_t &inputdata );
	void	InputSetBloomScaleRange( inputdata_t &inputdata );
#ifdef DARKINTERVAL // todo - this doesn't work, doesn't print out correct values
	void	InputGetAutoExposureMin(inputdata_t &inputdata);
	void	InputGetAutoExposureMax(inputdata_t &inputdata);
	void	InputGetBloomScale(inputdata_t &inputdata);
#endif
private:
	float	m_flBlendTonemapStart;		// HDR Tonemap at the start of the blend
	float	m_flBlendTonemapEnd;		// Target HDR Tonemap at the end of the blend
	float	m_flBlendEndTime;			// Time at which the blend ends
	float	m_flBlendStartTime;			// Time at which the blend started
#ifdef DARKINTERVAL 
	float	m_flKeyAutoExposureMax;
	float	m_flKeyAutoExposureMin;
	float	m_flKeyBloomScale;
	float	m_flKeyBlendTonemapRate;
#endif
	CNetworkVar( bool, m_bUseCustomAutoExposureMin );
	CNetworkVar( bool, m_bUseCustomAutoExposureMax );
	CNetworkVar( bool, m_bUseCustomBloomScale );
	CNetworkVar( float, m_flCustomAutoExposureMin );
	CNetworkVar( float, m_flCustomAutoExposureMax );
	CNetworkVar( float, m_flCustomBloomScale);
	CNetworkVar( float, m_flCustomBloomScaleMinimum);
};

LINK_ENTITY_TO_CLASS( env_tonemap_controller, CEnvTonemapController );

BEGIN_DATADESC( CEnvTonemapController )
	DEFINE_FIELD( m_flBlendTonemapStart, FIELD_FLOAT ),
	DEFINE_FIELD( m_flBlendTonemapEnd, FIELD_FLOAT ),
	DEFINE_FIELD( m_flBlendEndTime, FIELD_TIME ),
	DEFINE_FIELD( m_flBlendStartTime, FIELD_TIME ),
	DEFINE_FIELD( m_bUseCustomAutoExposureMin, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bUseCustomAutoExposureMax, FIELD_BOOLEAN ),
	DEFINE_FIELD(m_flCustomAutoExposureMin, FIELD_FLOAT),
	DEFINE_FIELD(m_flCustomAutoExposureMax, FIELD_FLOAT),
	DEFINE_FIELD(m_flCustomBloomScale, FIELD_FLOAT),
#ifdef DARKINTERVAL // to be able to set default tonemap controls without logic auto
	DEFINE_KEYFIELD( m_flKeyAutoExposureMin, FIELD_FLOAT,	"DefaultAutoExposureMin" ),
	DEFINE_KEYFIELD( m_flKeyAutoExposureMax, FIELD_FLOAT,	"DefaultAutoExposureMax"),
	DEFINE_KEYFIELD( m_flKeyBloomScale, FIELD_FLOAT,		"DefaultBloomScale"),
	DEFINE_KEYFIELD( m_flKeyBlendTonemapRate, FIELD_FLOAT,	"DefaultTonemapRate"),
#endif
	DEFINE_FIELD( m_flCustomBloomScaleMinimum, FIELD_FLOAT ),
	DEFINE_FIELD( m_bUseCustomBloomScale, FIELD_BOOLEAN ),

	DEFINE_THINKFUNC( UpdateTonemapScaleBlend ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTonemapScale", InputSetTonemapScale ),
	DEFINE_INPUTFUNC( FIELD_STRING, "BlendTonemapScale", InputBlendTonemapScale ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTonemapRate", InputSetTonemapRate ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetAutoExposureMin", InputSetAutoExposureMin ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetAutoExposureMax", InputSetAutoExposureMax ),
	DEFINE_INPUTFUNC( FIELD_VOID, "UseDefaultAutoExposure", InputUseDefaultAutoExposure ),
	DEFINE_INPUTFUNC( FIELD_VOID, "UseDefaultBloomScale", InputUseDefaultBloomScale ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetBloomScale", InputSetBloomScale ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetBloomScaleRange", InputSetBloomScaleRange ),
#ifdef DARKINTERVAL // todo - this doesn't work, doesn't print out correct values
	DEFINE_INPUTFUNC(FIELD_VOID, "GetAutoExposureMin", InputGetAutoExposureMin),
	DEFINE_INPUTFUNC(FIELD_VOID, "GetAutoExposureMax", InputGetAutoExposureMax),
	DEFINE_INPUTFUNC(FIELD_VOID, "GetBloomScale", InputGetBloomScale),
#endif
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CEnvTonemapController, DT_EnvTonemapController )
	SendPropInt( SENDINFO(m_bUseCustomAutoExposureMin), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_bUseCustomAutoExposureMax), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_bUseCustomBloomScale), 1, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO(m_flCustomAutoExposureMin), 0, SPROP_NOSCALE),
	SendPropFloat( SENDINFO(m_flCustomAutoExposureMax), 0, SPROP_NOSCALE),
	SendPropFloat( SENDINFO(m_flCustomBloomScale), 0, SPROP_NOSCALE),
	SendPropFloat( SENDINFO(m_flCustomBloomScaleMinimum), 0, SPROP_NOSCALE),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
#ifdef DARKINTERVAL
	inputdata_t i;
	if (m_flKeyAutoExposureMax > 0)
	{
		i.value.SetFloat( m_flKeyAutoExposureMax);
		InputSetAutoExposureMax(i);
	}
	if (m_flKeyAutoExposureMin > 0)
	{
		i.value.SetFloat(m_flKeyAutoExposureMin);
		InputSetAutoExposureMin(i);
	}
	if (m_flKeyBloomScale >= 0)
	{
		i.value.SetFloat(m_flKeyBloomScale);
		InputSetBloomScale(i);
	}
	if (m_flKeyBlendTonemapRate >= 0)
	{
		i.value.SetFloat(m_flKeyBlendTonemapRate);
		InputSetTonemapRate(i);
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEnvTonemapController::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Set the tonemap scale to the specified value
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapScale( inputdata_t &inputdata )
{
	float flRemapped = inputdata.value.Float();
	mat_hdr_tonemapscale.SetValue( flRemapped );
}

//-----------------------------------------------------------------------------
// Purpose: Blend the tonemap scale to the specified value
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputBlendTonemapScale( inputdata_t &inputdata )
{
	char parseString[255];
	Q_strncpy(parseString, inputdata.value.String(), sizeof(parseString));

	// Get the target tonemap scale
	char *pszParam = strtok(parseString," ");
	if ( !pszParam || !pszParam[0] )
	{
		Warning("%s (%s) received BlendTonemapScale input without a target tonemap scale. Syntax: <target tonemap scale> <blend time>\n", GetClassname(), GetDebugName() );
		return;
	}
	m_flBlendTonemapEnd = atof( pszParam );

	// Get the blend time
	pszParam = strtok(NULL," ");
	if ( !pszParam || !pszParam[0] )
	{
		Warning("%s (%s) received BlendTonemapScale input without a blend time. Syntax: <target tonemap scale> <blend time>\n", GetClassname(), GetDebugName() );
		return;
	}
	m_flBlendEndTime = CURTIME + atof( pszParam );

	m_flBlendStartTime = CURTIME;
	m_flBlendTonemapStart = mat_hdr_tonemapscale.GetFloat();

	// Start thinking
	SetNextThink( CURTIME + 0.1 );
	SetThink( &CEnvTonemapController::UpdateTonemapScaleBlend );
}

//-----------------------------------------------------------------------------
// Purpose: set a base and minimum bloom scale
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomScaleRange( inputdata_t &inputdata )
{
	float bloom_max=1, bloom_min=1;
	int nargs=sscanf("%f %f",inputdata.value.String(), bloom_max, bloom_min );
	if (nargs != 2)
	{
		Warning("%s (%s) received SetBloomScaleRange input without 2 arguments. Syntax: <max bloom> <min bloom>\n", GetClassname(), GetDebugName() );
		return;
	}
	m_flCustomBloomScale=bloom_max;
	m_flCustomBloomScaleMinimum=bloom_min;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapRate( inputdata_t &inputdata )
{
	// TODO: There should be a better way to do this.
	ConVarRef mat_hdr_manual_tonemap_rate( "mat_hdr_manual_tonemap_rate" );
	if ( mat_hdr_manual_tonemap_rate.IsValid() )
	{
		float flTonemapRate = inputdata.value.Float();
		mat_hdr_manual_tonemap_rate.SetValue( flTonemapRate );
	}
}
#ifdef DARKINTERVAL // todo - this doesn't work, doesn't print out correct values
//-----------------------------------------------------------------------------
// Purpose: Print out the current value for convenient in-game tweaking
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputGetAutoExposureMin(inputdata_t &inputdata)
{
	// TODO: There should be a better way to do this.
	Msg("Tonemap's autoexposuremin value is %.1f\n", m_flCustomAutoExposureMin);
}
void CEnvTonemapController::InputGetAutoExposureMax(inputdata_t &inputdata)
{
	// TODO: There should be a better way to do this.
	Msg("Tonemap's autoexposuremax value is %.1f\n", m_flCustomAutoExposureMax);
}
void CEnvTonemapController::InputGetBloomScale(inputdata_t &inputdata)
{
	// TODO: There should be a better way to do this.
	Msg("Tonemap's bloomscale value is %.1f\n", m_flCustomBloomScale);
}
#endif
//-----------------------------------------------------------------------------
// Purpose: Blend the tonemap scale to the specified value
//-----------------------------------------------------------------------------
void CEnvTonemapController::UpdateTonemapScaleBlend( void )
{ 
	float flRemapped = RemapValClamped( CURTIME, m_flBlendStartTime, m_flBlendEndTime, m_flBlendTonemapStart, m_flBlendTonemapEnd );
	mat_hdr_tonemapscale.SetValue( flRemapped );

	//Msg("Setting tonemap scale to %f (curtime %f, %f -> %f)\n", flRemapped, CURTIME, m_flBlendStartTime, m_flBlendEndTime ); 

	// Stop when we're out of the blend range
	if ( CURTIME >= m_flBlendEndTime )
		return;

	SetNextThink( CURTIME + 0.1 );
}

//-----------------------------------------------------------------------------
// Purpose: Set the auto exposure min to the specified value
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetAutoExposureMin( inputdata_t &inputdata )
{
	m_flCustomAutoExposureMin = inputdata.value.Float();
	m_bUseCustomAutoExposureMin = true;
}

//-----------------------------------------------------------------------------
// Purpose: Set the auto exposure max to the specified value
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetAutoExposureMax( inputdata_t &inputdata )
{
	m_flCustomAutoExposureMax = inputdata.value.Float();
	m_bUseCustomAutoExposureMax = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputUseDefaultAutoExposure( inputdata_t &inputdata )
{
	m_bUseCustomAutoExposureMin = false;
	m_bUseCustomAutoExposureMax = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomScale( inputdata_t &inputdata )
{
	m_flCustomBloomScale = inputdata.value.Float();
	m_flCustomBloomScaleMinimum = m_flCustomBloomScale;
	m_bUseCustomBloomScale = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputUseDefaultBloomScale( inputdata_t &inputdata )
{
	m_bUseCustomBloomScale = false;
}
