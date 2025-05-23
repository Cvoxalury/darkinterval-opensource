//========================================================================//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_hl2_playerlocaldata.h"
#include "dt_recv.h"
#ifdef DARKINTERVAL
#include "hud_locator.h"
#include "usermessages.h"
#endif
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#ifdef DARKINTERVAL
C_HL2PlayerLocalData *g_pPlayerLocalData = NULL;
#endif
BEGIN_RECV_TABLE_NOBASE( C_HL2PlayerLocalData, DT_HL2Local )
	RecvPropFloat( RECVINFO(m_flSuitPower) ),
	RecvPropInt( RECVINFO(m_bZooming) ),
	RecvPropInt( RECVINFO(m_bitsActiveDevices) ),
	RecvPropInt( RECVINFO(m_iSquadMemberCount) ),
	RecvPropInt( RECVINFO(m_iSquadMedicCount) ),
	RecvPropBool( RECVINFO(m_fSquadInFollowMode) ),
	RecvPropBool( RECVINFO(m_bWeaponLowered) ),
	RecvPropEHandle( RECVINFO(m_hAutoAimTarget) ),
	RecvPropVector( RECVINFO(m_vecAutoAimPoint) ),
	RecvPropEHandle( RECVINFO(m_hLadder) ),
	RecvPropBool( RECVINFO(m_bDisplayReticle) ),
	RecvPropBool( RECVINFO(m_bStickyAutoAim) ),
	RecvPropBool( RECVINFO(m_bAutoAimTarget) ),
#ifdef HL2_EPISODIC
	RecvPropFloat( RECVINFO(m_flFlashBattery) ),
	RecvPropVector( RECVINFO(m_vecLocatorOrigin) ),
#ifdef DARKINTERVAL
	RecvPropInt(RECVINFO(m_iNumLocatorContacts)),
	RecvPropArray(RecvPropEHandle(RECVINFO(m_locatorEnt[0])), m_locatorEnt),
	RecvPropArray(RecvPropInt(RECVINFO(m_iLocatorContactType[0])), m_iLocatorContactType),
#endif
#endif
#ifdef DARKINTERVAL
	// DI NEW
	RecvPropInt(RECVINFO(m_nZoomType)),
	RecvPropBool(RECVINFO(m_flashlight_force_flicker_bool)),
#endif
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_HL2PlayerLocalData )
	DEFINE_PRED_FIELD( m_hLadder, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#ifdef DARKINTERVAL
void __MsgFunc_UpdatePlayerLocator(bf_read &msg)
{
#ifdef HL2_EPISODIC
	// Radar code here!
	if (!GetHudLocator())
		return;

	for (int i = 0; i < g_pPlayerLocalData->m_iNumLocatorContacts; i++)
	{
		GetHudLocator()->AddLocatorContact(g_pPlayerLocalData->m_locatorEnt[i], g_pPlayerLocalData->m_iLocatorContactType[i]);
	}
#endif
}
#endif
C_HL2PlayerLocalData::C_HL2PlayerLocalData()
{
	m_flSuitPower = 0.0;
	m_bZooming = false;
	m_iSquadMemberCount = 0;
	m_iSquadMedicCount = 0;
	m_fSquadInFollowMode = false;
	m_bWeaponLowered = false;
	m_hLadder = NULL;
#ifdef HL2_EPISODIC
	m_flFlashBattery = 0.0f;
	m_vecLocatorOrigin = vec3_origin;
#ifdef DARKINTERVAL
	m_iNumLocatorContacts = 0;
#endif
#endif
#ifdef DARKINTERVAL
	if (g_pPlayerLocalData == NULL)
		usermessages->HookMessage("UpdatePlayerLocator", __MsgFunc_UpdatePlayerLocator);
	g_pPlayerLocalData = this;
#endif
}

