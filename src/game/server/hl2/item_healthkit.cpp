//========================================================================//
//
// Purpose: Implements health kits and wall mounted health chargers.
// DI NEW: partially depletable kits and vials are implemented via StudioFrameAdvance and SetCycle in Spawn.
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "gamerules.h"
#include "player.h"
#include "items.h"
#include "in_buttons.h"
#include "engine/ienginesound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#ifdef DARKINTERVAL
ConVar	sk_healthkit( "sk_healthkit","25" );		
ConVar	sk_healthvial( "sk_healthvial","10" );	
#else
ConVar	sk_healthkit("sk_healthkit", "0");
ConVar	sk_healthvial("sk_healthvial", "0");
#endif
ConVar	sk_healthcharger( "sk_healthcharger","0" );		

//-----------------------------------------------------------------------------
// Small health kit. Heals the player when picked up.
//-----------------------------------------------------------------------------
class CHealthKit : public CItem
{
public:
	DECLARE_CLASS( CHealthKit, CItem );

	void Spawn( void );
	void Precache( void );
	bool MyTouch( CBasePlayer *pPlayer );
#ifdef DARKINTERVAL
	void StudioFrameAdvance(void);
	void InputSetHealth(inputdata_t &data);

	virtual bool ConsumedInstantlyByTouch(void) { return false; } // so that it can be depleted in parts

	DECLARE_DATADESC();
#endif
};

LINK_ENTITY_TO_CLASS( item_healthkit, CHealthKit );
#ifdef DARKINTERVAL // convenient reserve names
LINK_ENTITY_TO_CLASS( item_health, CHealthKit );
#endif
PRECACHE_REGISTER(item_healthkit);
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHealthKit::Spawn( void )
{
	Precache();
#ifdef DARKINTERVAL
	SetModel( "models/_items/healthkit.mdl" );
#else
	SetModel("models/items/healthkit.mdl");
#endif
	BaseClass::Spawn();
	m_iHealth = m_iMaxHealth = sk_healthkit.GetInt();
	SetCycle(1.0f - float((float)m_iHealth / (float)m_iMaxHealth));
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHealthKit::Precache( void )
{
#ifdef DARKINTERVAL
	PrecacheModel("models/_items/healthkit.mdl");
#else
	PrecacheModel("models/items/healthkit.mdl");
#endif
	PrecacheScriptSound( "HealthKit.Touch" );
}
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: Allows the healthkit liquid level to be adjusted based on HP amount
// Input  :
// Output : 
//-----------------------------------------------------------------------------
void CHealthKit::StudioFrameAdvance(void)
{
	m_flPlaybackRate = 0;
	
	SetCycle(1.0f - float((float)m_iHealth / (float)m_iMaxHealth));

	if (!m_flPrevAnimTime)
	{
		m_flPrevAnimTime = CURTIME;
	}

	if (m_iHealth <= 0) RemoveEffects(EF_ITEM_BLINK);

	// Latch prev
	m_flPrevAnimTime = m_flAnimTime;
	// Set current
	m_flAnimTime = CURTIME;
}
#endif
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
// Output : 
//-----------------------------------------------------------------------------
bool CHealthKit::MyTouch( CBasePlayer *pPlayer )
{
#ifdef DARKINTERVAL
//	if (pPlayer->GetHealth() <= 0)
//	{
//		pPlayer->Revive();
//	}

	if (m_iHealth <= 0) return false;

	int healthToGive = min((pPlayer->GetMaxHealth() - pPlayer->GetHealth()), sk_healthkit.GetInt());
	// heal how much is missing from player's full health, but not more than 25
	if ( pPlayer->TakeHealth( healthToGive, DMG_GENERIC ) )
#else
	if ( pPlayer->TakeHealth(sk_healthkit.GetFloat(), DMG_GENERIC) )
#endif
	{
		CSingleUserRecipientFilter user( pPlayer );
		user.MakeReliable();

		UserMessageBegin( user, "ItemPickup" );
			WRITE_STRING( GetClassname() );
		MessageEnd();

		CPASAttenuationFilter filter( pPlayer, "HealthKit.Touch" );
		EmitSound( filter, pPlayer->entindex(), "HealthKit.Touch" );

#ifndef DARKINTERVAL
		if ( g_pGameRules->ItemShouldRespawn( this ) )
		{
			Respawn();
		}
		else
		{
			UTIL_Remove(this);	
		}
#else
		m_iHealth -= healthToGive; // deplete this healthkit's own health reserve by same amount
		StudioFrameAdvance();
		if (m_iHealth <= 0)
		{
		//	if (g_pGameRules->ItemShouldRespawn(this))
		//	{
		//		Respawn();
		//	}
		//	else
			{
				UTIL_Remove(this);	
			}
		}
#endif
		return true;
	}

	return false;
}

#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Used to set health level after spawning the kit, for partially depleted
// kits in the world
//-----------------------------------------------------------------------------
void CHealthKit::InputSetHealth(inputdata_t &data)
{
	m_iHealth = data.value.Int();
	StudioFrameAdvance();
}

BEGIN_DATADESC(CHealthKit)
DEFINE_INPUTFUNC(FIELD_INTEGER, "SetHealth", InputSetHealth),
END_DATADESC()
#endif
//-----------------------------------------------------------------------------
// Small dynamically dropped health kit
//-----------------------------------------------------------------------------

class CHealthVial : public CItem
{
public:
	DECLARE_CLASS( CHealthVial, CItem );
#ifdef DARKINTERVAL
	DECLARE_DATADESC();

	virtual bool ConsumedInstantlyByTouch(void) { return false; } // so that it can be depleted in parts
#endif
	void Spawn( void )
	{
		Precache();
#ifdef DARKINTERVAL
		SetModel( "models/_Items/healthvial.mdl" );
		m_iHealth = m_iMaxHealth = sk_healthvial.GetInt();
		SetCycle(1.0f - float((float)m_iHealth / (float)m_iMaxHealth));
#else
		SetModel("models/healthvial.mdl");
#endif
		BaseClass::Spawn();
	}

	void Precache( void )
	{
#ifdef DARKINTERVAL
		PrecacheModel("models/_Items/healthvial.mdl");
#else
		PrecacheModel("models/healthvial.mdl");
#endif
		PrecacheScriptSound( "HealthVial.Touch" );
	}
#ifdef DARKINTERVAL
	void StudioFrameAdvance(void)
	{
		m_flPlaybackRate = 0;

		SetCycle(1.0f - float((float)m_iHealth / (float)m_iMaxHealth));

		if (!m_flPrevAnimTime)
		{
			m_flPrevAnimTime = CURTIME;
		}

		if (m_iHealth <= 0) RemoveEffects(EF_ITEM_BLINK);

		// Latch prev
		m_flPrevAnimTime = m_flAnimTime;
		// Set current
		m_flAnimTime = CURTIME;
	}
#endif
	bool MyTouch( CBasePlayer *pPlayer )
	{
#ifdef DARKINTERVAL
		if (m_iHealth <= 0) return false;

		int healthToGive = min((pPlayer->GetMaxHealth() - pPlayer->GetHealth()), sk_healthvial.GetInt());
		// heal how much is missing from player's full health, but not more than 10
		if (pPlayer->TakeHealth(healthToGive, DMG_GENERIC))
#else
		if ( pPlayer->TakeHealth(sk_healthvial.GetFloat(), DMG_GENERIC) )
#endif
		{
			CSingleUserRecipientFilter user( pPlayer );
			user.MakeReliable();

			UserMessageBegin( user, "ItemPickup" );
				WRITE_STRING( GetClassname() );
			MessageEnd();

			CPASAttenuationFilter filter( pPlayer, "HealthVial.Touch" );
			EmitSound( filter, pPlayer->entindex(), "HealthVial.Touch" );
#ifdef DARKINTERVAL
		//	if ( g_pGameRules->ItemShouldRespawn( this ) )
		//	{
		//		Respawn();
		//	}
		//	else
		//	{
		//		UTIL_Remove(this);	
		//	}
			
			m_iHealth -= healthToGive; // deplete this healthkit's own health reserve by same amount
			StudioFrameAdvance();
			if (m_iHealth <= 0)
			{
			//	if (g_pGameRules->ItemShouldRespawn(this))
			//	{
			//		Respawn();
			//	}
			//	else
				{
					UTIL_Remove(this);
				}
			}
#else
			if ( g_pGameRules->ItemShouldRespawn(this) )
			{
				Respawn();
			} else
			{
				UTIL_Remove(this);
			}
#endif // DARKINTERVAL
			return true;
		}

		return false;
	}
#ifdef DARKINTERVAL
	void InputSetHealth(inputdata_t &data)
	{
		m_iHealth = data.value.Int();
		StudioFrameAdvance();
	}
#endif
};

LINK_ENTITY_TO_CLASS( item_healthvial, CHealthVial );
PRECACHE_REGISTER( item_healthvial );
#ifdef DARKINTERVAL
BEGIN_DATADESC(CHealthVial)
DEFINE_INPUTFUNC(FIELD_INTEGER, "SetHealth", InputSetHealth),
END_DATADESC()
#endif
//-----------------------------------------------------------------------------
// Wall mounted health kit. Heals the player when used.
//-----------------------------------------------------------------------------
class CWallHealth : public CBaseToggle
{
public:
	DECLARE_CLASS( CWallHealth, CBaseToggle );

	void Spawn( );
	void Precache( void );
	int  DrawDebugTextOverlays(void);
	bool CreateVPhysics(void);
	void Off(void);
	void Recharge(void);
	bool KeyValue(  const char *szKeyName, const char *szValue );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() | m_iCaps; }

	float m_flNextCharge; 
	int		m_iReactivate ; // DeathMatch Delay until reactvated
	int		m_iJuice;
	int		m_iOn;			// 0 = off, 1 = startup, 2 = going
	float   m_flSoundTime;

	int		m_nState;
	int		m_iCaps;

	COutputFloat m_OutRemainingHealth;
	COutputEvent m_OnPlayerUse;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(func_healthcharger, CWallHealth);

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pkvd - 
//-----------------------------------------------------------------------------
bool CWallHealth::KeyValue(  const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "style") ||
		FStrEq(szKeyName, "height") ||
		FStrEq(szKeyName, "value1") ||
		FStrEq(szKeyName, "value2") ||
		FStrEq(szKeyName, "value3"))
	{
		return(true);
	}
	else if (FStrEq(szKeyName, "dmdelay"))
	{
		m_iReactivate = atoi(szValue);
		return(true);
	}

	return(BaseClass::KeyValue( szKeyName, szValue ));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWallHealth::Spawn(void)
{
	Precache( );

	SetSolid( SOLID_BSP );
	SetMoveType( MOVETYPE_PUSH );

	SetModel( STRING( GetModelName() ) );

	m_iJuice = sk_healthcharger.GetFloat();

	m_nState = 0;	
	
	m_iCaps	= FCAP_CONTINUOUS_USE;

	CreateVPhysics();
}

int CWallHealth::DrawDebugTextOverlays(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];
		Q_snprintf(tempstr,sizeof(tempstr),"Charge left: %i", m_iJuice );
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------

bool CWallHealth::CreateVPhysics(void)
{
	VPhysicsInitStatic();
	return true;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWallHealth::Precache(void)
{
	PrecacheScriptSound( "WallHealth.Deny" );
	PrecacheScriptSound( "WallHealth.Start" );
	PrecacheScriptSound( "WallHealth.LoopingContinueCharge" );
	PrecacheScriptSound( "WallHealth.Recharge" );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pActivator - 
//			*pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CWallHealth::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	// Make sure that we have a caller
	if (!pActivator)
		return;

	// if it's not a player, ignore
	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = dynamic_cast<CBasePlayer *>(pActivator);

	// Reset to a state of continuous use.
	m_iCaps = FCAP_CONTINUOUS_USE;

	// if there is no juice left, turn it off
	if (m_iJuice <= 0)
	{
		m_nState = 1;			
		Off();
	}

	// if the player doesn't have the suit, or there is no juice left, make the deny noise.
	// disabled HEV suit dependency for now.
	//if ((m_iJuice <= 0) || (!(pActivator->m_bWearingSuit)))
	if (m_iJuice <= 0)
	{
		if (m_flSoundTime <= CURTIME)
		{
			m_flSoundTime = CURTIME + 0.62;
			EmitSound( "WallHealth.Deny" );
		}
		return;
	}

	if( pActivator->GetHealth() >= pActivator->GetMaxHealth() )
	{
		if( pPlayer )
		{
			pPlayer->m_afButtonPressed &= ~IN_USE;
		}

		// Make the user re-use me to get started drawing health.
		m_iCaps = FCAP_IMPULSE_USE;

		return;
	}

	SetNextThink( CURTIME + 0.25f );
	SetThink(&CWallHealth::Off);

	// Time to recharge yet?

	if (m_flNextCharge >= CURTIME)
		return;

	// Play the on sound or the looping charging sound
	if (!m_iOn)
	{
		m_iOn++;
		EmitSound( "WallHealth.Start" );
		m_flSoundTime = 0.56 + CURTIME;

		m_OnPlayerUse.FireOutput( pActivator, this );
	}
	if ((m_iOn == 1) && (m_flSoundTime <= CURTIME))
	{
		m_iOn++;
		CPASAttenuationFilter filter( this, "WallHealth.LoopingContinueCharge" );
		filter.MakeReliable();
		EmitSound( filter, entindex(), "WallHealth.LoopingContinueCharge" );
	}

	// charge the player
	if ( pActivator->TakeHealth( 1, DMG_GENERIC ) )
	{
		m_iJuice--;
	}

	// Send the output.
	float flRemaining = m_iJuice / sk_healthcharger.GetFloat();
	m_OutRemainingHealth.Set(flRemaining, pActivator, this);

	// govern the rate of charge
	m_flNextCharge = CURTIME + 0.1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWallHealth::Recharge(void)
{
	EmitSound( "WallHealth.Recharge" );
	m_iJuice = sk_healthcharger.GetFloat();
	m_nState = 0;			
	SetThink( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWallHealth::Off(void)
{
	// Stop looping sound.
	if (m_iOn > 1)
		StopSound( "WallHealth.LoopingContinueCharge" );

	m_iOn = 0;

	if ((!m_iJuice) &&  ( ( m_iReactivate = g_pGameRules->FlHealthChargerRechargeTime() ) > 0) )
	{
		SetNextThink( CURTIME + m_iReactivate );
		SetThink(&CWallHealth::Recharge);
	}
	else
		SetThink( NULL );
}

BEGIN_DATADESC(CWallHealth)

DEFINE_FIELD(m_flNextCharge, FIELD_TIME),
DEFINE_FIELD(m_iReactivate, FIELD_INTEGER),
DEFINE_FIELD(m_iJuice, FIELD_INTEGER),
DEFINE_FIELD(m_iOn, FIELD_INTEGER),
DEFINE_FIELD(m_flSoundTime, FIELD_TIME),
DEFINE_FIELD(m_nState, FIELD_INTEGER),
DEFINE_FIELD(m_iCaps, FIELD_INTEGER),

// Function Pointers
DEFINE_FUNCTION(Off),
DEFINE_FUNCTION(Recharge),

DEFINE_OUTPUT(m_OnPlayerUse, "OnPlayerUse"),
DEFINE_OUTPUT(m_OutRemainingHealth, "OutRemainingHealth"),

END_DATADESC()

//-----------------------------------------------------------------------------
// Wall mounted health kit. Heals the player when used.
//-----------------------------------------------------------------------------
class CNewWallHealth : public CBaseAnimating
{
public:
	DECLARE_CLASS( CNewWallHealth, CBaseAnimating );

	void Spawn( );
	void Precache( void );
	int  DrawDebugTextOverlays(void);
	bool CreateVPhysics(void);
	void Off(void);
	void Recharge(void);
	bool KeyValue(  const char *szKeyName, const char *szValue );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() | m_iCaps; }
#ifdef DARKINTERVAL	
	void InputRecharge(inputdata_t &inputdata);
	void InputSetCharge(inputdata_t &inputdata);
	void InputDisable(inputdata_t &inputdata);
	void InputEnable(inputdata_t &inputdata);
	bool	m_charger_disabled_bool;
#endif
	float m_flNextCharge; 
	int		m_iReactivate ; // DeathMatch Delay until reactvated
	int		m_iJuice;
	int		m_iOn;			// 0 = off, 1 = startup, 2 = going
	float   m_flSoundTime;

	int		m_nState;
	int		m_iCaps;

	COutputFloat m_OutRemainingHealth;
	COutputEvent m_OnPlayerUse;
#ifdef DARKINTERVAL	
	COutputEvent m_OnHalfEmpty;
	COutputEvent m_OnEmpty;
	COutputEvent m_OnFull;
#endif
	void StudioFrameAdvance ( void );

	float m_flJuice;

	DECLARE_DATADESC();


};

LINK_ENTITY_TO_CLASS( item_healthcharger, CNewWallHealth);

#define HEALTH_CHARGER_MODEL_NAME "models/props_combine/health_charger001.mdl"
#define CHARGE_RATE 0.25f
#define CHARGES_PER_SECOND 1.0f / CHARGE_RATE
#define CALLS_PER_SECOND 7.0f * CHARGES_PER_SECOND

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pkvd - 
//-----------------------------------------------------------------------------
bool CNewWallHealth::KeyValue(  const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "style") ||
		FStrEq(szKeyName, "height") ||
		FStrEq(szKeyName, "value1") ||
		FStrEq(szKeyName, "value2") ||
		FStrEq(szKeyName, "value3"))
	{
		return(true);
	}
	else if (FStrEq(szKeyName, "dmdelay"))
	{
		m_iReactivate = atoi(szValue);
		return(true);
	}

	return(BaseClass::KeyValue( szKeyName, szValue ));
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNewWallHealth::Spawn(void)
{
	Precache( );

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );
	CreateVPhysics();

	SetModel( HEALTH_CHARGER_MODEL_NAME );
	AddEffects( EF_NOSHADOW );

	ResetSequence( LookupSequence( "idle" ) );

	m_iJuice = sk_healthcharger.GetFloat();

	m_nState = 0;	
	
	m_iReactivate = 0;
	m_iCaps	= FCAP_CONTINUOUS_USE;

	CreateVPhysics();

	m_flJuice = m_iJuice;
	SetCycle( 1.0f - ( m_flJuice /  sk_healthcharger.GetFloat() ) );
#ifdef DARKINTERVAL	
	if (m_charger_disabled_bool)
		m_nSkin = 1;
#endif
}

int CNewWallHealth::DrawDebugTextOverlays(void) 
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		char tempstr[512];
		Q_snprintf(tempstr,sizeof(tempstr),"Charge left: %i", m_iJuice );
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}
	return text_offset;
}

//-----------------------------------------------------------------------------

bool CNewWallHealth::CreateVPhysics(void)
{
	VPhysicsInitStatic();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNewWallHealth::Precache(void)
{
	PrecacheModel( HEALTH_CHARGER_MODEL_NAME );

	PrecacheScriptSound( "WallHealth.Deny" );
	PrecacheScriptSound( "WallHealth.Start" );
	PrecacheScriptSound( "WallHealth.LoopingContinueCharge" );
	PrecacheScriptSound( "WallHealth.Recharge" );
#ifdef DARKINTERVAL	
	PrecacheScriptSound("SuitRecharge.Disable");
	PrecacheScriptSound("SuitRecharge.Enable");
#endif
}

void CNewWallHealth::StudioFrameAdvance( void )
{
	m_flPlaybackRate = 0;

	float flMaxJuice = sk_healthcharger.GetFloat();
	
	SetCycle( 1.0f - (float)( m_flJuice / flMaxJuice ) );
//	Msg( "Cycle: %f - Juice: %d - m_flJuice :%f - Interval: %f\n", (float)GetCycle(), (int)m_iJuice, (float)m_flJuice, GetAnimTimeInterval() );

	if ( !m_flPrevAnimTime )
	{
		m_flPrevAnimTime = CURTIME;
	}

	// Latch prev
	m_flPrevAnimTime = m_flAnimTime;
	// Set current
	m_flAnimTime = CURTIME;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pActivator - 
//			*pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CNewWallHealth::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	// Make sure that we have a caller
	if (!pActivator)
		return;

	// if it's not a player, ignore
	if ( !pActivator->IsPlayer() )
		return;
	CBasePlayer *pPlayer = dynamic_cast<CBasePlayer *>(pActivator);

	// Reset to a state of continuous use.
	m_iCaps = FCAP_CONTINUOUS_USE;

	if ( m_iOn )
	{
		float flCharges = CHARGES_PER_SECOND;
		float flCalls = CALLS_PER_SECOND;

		m_flJuice -= flCharges / flCalls;
		StudioFrameAdvance();
	}

	// if there is no juice left, turn it off
	if (m_iJuice <= 0)
	{
		ResetSequence( LookupSequence( "emptyclick" ) );
		m_nState = 1;			
		Off();
	}

	// if the player doesn't have the suit, or there is no juice left, make the deny noise.
	// disabled HEV suit dependency for now.
	//if ((m_iJuice <= 0) || (!(pActivator->m_bWearingSuit)))
	if (m_iJuice <= 0)
	{
		if (m_flSoundTime <= CURTIME)
		{
			m_flSoundTime = CURTIME + 0.62;
			EmitSound( "WallHealth.Deny" );
		}
		return;
	}

	if( pActivator->GetHealth() >= pActivator->GetMaxHealth() )
	{
		if( pPlayer )
		{
			pPlayer->m_afButtonPressed &= ~IN_USE;
		}

		// Make the user re-use me to get started drawing health.
		m_iCaps = FCAP_IMPULSE_USE;
		
		EmitSound( "WallHealth.Deny" );
		return;
	}

	SetNextThink( CURTIME + CHARGE_RATE );
	SetThink( &CNewWallHealth::Off );

	// Time to recharge yet?

	if (m_flNextCharge >= CURTIME)
		return;

	// Play the on sound or the looping charging sound
	if (!m_iOn)
	{
		m_iOn++;
		EmitSound( "WallHealth.Start" );
		m_flSoundTime = 0.56 + CURTIME;

		m_OnPlayerUse.FireOutput( pActivator, this );
	}
	if ((m_iOn == 1) && (m_flSoundTime <= CURTIME))
	{
		m_iOn++;
		CPASAttenuationFilter filter( this, "WallHealth.LoopingContinueCharge" );
		filter.MakeReliable();
		EmitSound( filter, entindex(), "WallHealth.LoopingContinueCharge" );
	}

	// charge the player
	if ( pActivator->TakeHealth( 1, DMG_GENERIC ) )
	{
		m_iJuice--;
	}

	// Send the output.
	float flRemaining = m_iJuice / sk_healthcharger.GetFloat();
	m_OutRemainingHealth.Set(flRemaining, pActivator, this);

	// govern the rate of charge
	m_flNextCharge = CURTIME + 0.1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNewWallHealth::Recharge(void)
{
	EmitSound( "WallHealth.Recharge" );
	m_flJuice = m_iJuice = sk_healthcharger.GetFloat();
	m_nState = 0;

	ResetSequence( LookupSequence( "idle" ) );
	StudioFrameAdvance();

	m_iReactivate = 0;

	SetThink( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNewWallHealth::Off(void)
{
	// Stop looping sound.
	if (m_iOn > 1)
		StopSound( "WallHealth.LoopingContinueCharge" );

	if ( m_nState == 1 )
	{
		SetCycle( 1.0f );
	}

	m_iOn = 0;
	m_flJuice = m_iJuice;

	if ( m_iReactivate == 0 )
	{
		if ((!m_iJuice) && g_pGameRules->FlHealthChargerRechargeTime() > 0 )
		{
			m_iReactivate = g_pGameRules->FlHealthChargerRechargeTime();
			SetNextThink( CURTIME + m_iReactivate );
			SetThink(&CNewWallHealth::Recharge);
		}
		else
			SetThink( NULL );
	}
}
#ifdef DARKINTERVAL	
void CNewWallHealth::InputRecharge(inputdata_t &inputdata)
{
	Recharge();
}

void CNewWallHealth::InputSetCharge(inputdata_t &inputdata)
{
	ResetSequence(LookupSequence("idle"));

	if ((m_iOn == 1) && (m_flSoundTime <= CURTIME))
	{
		m_iOn++;
		CPASAttenuationFilter filter(this, "WallHealth.LoopingContinueCharge");
		filter.MakeReliable();
		EmitSound(filter, entindex(), "WallHealth.LoopingContinueCharge");
	}

	int iJuice = min(inputdata.value.Int(), (int)sk_healthcharger.GetFloat());

	m_flJuice = /*m_iMaxJuice =*/ m_iJuice = iJuice;

	Off();
	StudioFrameAdvance();
}

void CNewWallHealth::InputDisable(inputdata_t &inputdata)
{
	EmitSound("SuitRecharge.Disable");
	m_charger_disabled_bool = true;
	m_nSkin = 1;
}

void CNewWallHealth::InputEnable(inputdata_t &inputdata)
{
	EmitSound("SuitRecharge.Enable");
	m_charger_disabled_bool = false;
	m_nSkin = 0;
}
#endif // DARKINTERVAL

BEGIN_DATADESC(CNewWallHealth)
	DEFINE_FIELD(m_flNextCharge, FIELD_TIME),
	DEFINE_FIELD(m_iReactivate, FIELD_INTEGER),
	DEFINE_FIELD(m_iJuice, FIELD_INTEGER),
	DEFINE_FIELD(m_iOn, FIELD_INTEGER),
	DEFINE_FIELD(m_flSoundTime, FIELD_TIME),
	DEFINE_FIELD(m_nState, FIELD_INTEGER),
	DEFINE_FIELD(m_iCaps, FIELD_INTEGER),
	DEFINE_FIELD(m_flJuice, FIELD_FLOAT),
#ifdef DARKINTERVAL	
	DEFINE_FIELD(m_charger_disabled_bool, FIELD_BOOLEAN),
#endif
	// Function Pointers
	DEFINE_FUNCTION(Off),
	DEFINE_FUNCTION(Recharge),

	DEFINE_OUTPUT(m_OnPlayerUse, "OnPlayerUse"),
	DEFINE_OUTPUT(m_OutRemainingHealth, "OutRemainingHealth"),
#ifdef DARKINTERVAL	
	DEFINE_OUTPUT(m_OnHalfEmpty, "OnHalfEmpty"),
	DEFINE_OUTPUT(m_OnEmpty, "OnEmpty"),
	DEFINE_OUTPUT(m_OnFull, "OnFull"),

	DEFINE_INPUTFUNC(FIELD_VOID, "Recharge", InputRecharge),
	DEFINE_INPUTFUNC(FIELD_INTEGER, "SetCharge", InputSetCharge),
	DEFINE_INPUTFUNC(FIELD_VOID, "Disable", InputDisable),
	DEFINE_INPUTFUNC(FIELD_VOID, "Enable", InputEnable),
#endif
END_DATADESC()