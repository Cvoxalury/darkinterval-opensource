//========================================================================//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basecombatweapon.h"
#include "env_explosion.h"
#include "eventqueue.h"
#include "gamerules.h"
#include "ammodef.h"
#include "in_buttons.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "engine/ienginesound.h"
#include "game.h"

#include "player.h"
#include "entitylist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Spawnflags
#define SF_MESSAGE_DISABLED		1

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMessageEntity : public CPointEntity
{
	DECLARE_CLASS( CMessageEntity, CPointEntity );

public:
	void	Spawn( void );
	void	Activate( void );
	void	Think( void );
	void	DrawOverlays(void);

	virtual void UpdateOnRemove();

	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();

protected:
	int				m_radius;
	string_t		m_messageText;
	bool			m_drawText;
	bool			m_bDeveloperOnly;
#ifdef DARKINTERVAL
	bool			m_bEditorOnly; // our Hammer can preview point messages, sometimes they're used for purely in-editor purposes
#endif
	bool			m_bEnabled;
};

LINK_ENTITY_TO_CLASS( point_message, CMessageEntity );

BEGIN_DATADESC( CMessageEntity )

	DEFINE_KEYFIELD( m_radius, FIELD_INTEGER, "radius" ),
	DEFINE_KEYFIELD( m_messageText, FIELD_STRING, "message" ),
	DEFINE_KEYFIELD( m_bDeveloperOnly, FIELD_BOOLEAN, "developeronly" ),
#ifdef DARKINTERVAL
	DEFINE_KEYFIELD(m_bEditorOnly, FIELD_BOOLEAN, "editoronly"),
#endif
	DEFINE_FIELD( m_drawText, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bEnabled, FIELD_BOOLEAN ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID,	 "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID,	 "Disable", InputDisable ),

END_DATADESC()

static CUtlVector< CHandle< CMessageEntity > >	g_MessageEntities;

//-----------------------------------------
// Spawn
//-----------------------------------------
void CMessageEntity::Spawn( void )
{
#ifdef DARKINTERVAL // don't even spawn in messages that are purely for in-editor view
	if (m_bEditorOnly)
	{
		UTIL_Remove(this);
		return;
	}
#endif
	SetNextThink( CURTIME + 0.1f );
	m_drawText = false;
	m_bDeveloperOnly = false;
	m_bEnabled = !HasSpawnFlags( SF_MESSAGE_DISABLED );
	//m_debugOverlays |= OVERLAY_TEXT_BIT;		// make sure we always show the text
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::Activate( void )
{
	BaseClass::Activate();

	CHandle< CMessageEntity > h;
	h = this;
	g_MessageEntities.AddToTail( h );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();

	CHandle< CMessageEntity > h;
	h = this;
	g_MessageEntities.FindAndRemove( h );

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------
// Think
//-----------------------------------------
void CMessageEntity::Think( void )
{
	SetNextThink( CURTIME + 0.1f );

	// check for player distance
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
#ifdef DARKINTERVAL // unsure why notarget used to disable point messages, but this is unwanted in DI
	if ( !pPlayer )
#else
	if ( !pPlayer || ( pPlayer->GetFlags() & FL_NOTARGET ) )
#endif
		return;

	Vector worldTargetPosition = pPlayer->EyePosition();

	// bail if player is too far away
	if ( (worldTargetPosition - GetAbsOrigin()).Length() > m_radius )
	{
		m_drawText = false;
		return;
	}

	// turn on text
	m_drawText = true;
}
	
//-------------------------------------------
//-------------------------------------------
void CMessageEntity::DrawOverlays(void) 
{
	if ( !m_drawText )
		return;

	if ( m_bDeveloperOnly && !g_pDeveloper->GetInt() )
		return;

	if ( !m_bEnabled )
		return;

	// display text if they are within range
	char tempstr[512];
	Q_snprintf( tempstr, sizeof(tempstr), "%s", STRING(m_messageText) );
	EntityText( 0, tempstr, 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMessageEntity::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

// This is a hack to make point_message stuff appear in developer 0 release builds
//  for now
void DrawMessageEntities()
{
	int c = g_MessageEntities.Count();
	for ( int i = c - 1; i >= 0; i-- )
	{
		CMessageEntity *me = g_MessageEntities[ i ];
		if ( !me )
		{
			g_MessageEntities.Remove( i );
			continue;
		}

		me->DrawOverlays();
	}
}
