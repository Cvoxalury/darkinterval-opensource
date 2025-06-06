//========================================================================//
//
// Purpose: 
// DI todo - this is a WIP rework of the RPG. Will be moved to
// _weapons_darkinterval.cpp/h once it's done.
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_RPG_H
#define WEAPON_RPG_H

#ifdef _WIN32
#pragma once
#endif

#include "basehlcombatweapon.h"
#include "sprite.h"
#include "npcevent.h"
#include "beam_shared.h"

class CWeaponRPG;
class CLaserDot;
#ifdef DARKINTERVAL // it was decided it's less unwieldy to separate different cases into their own classes
class CLaserDotRPG;
class CLaserDotAPC;
class CLaserDotAPCNPC;
#endif
class RocketTrail;
 
//###########################################################################
//	>> CMissile		(missile launcher class is below this one!)
//###########################################################################
class CMissile : public CBaseCombatCharacter
{
	DECLARE_CLASS( CMissile, CBaseCombatCharacter );

public:
	static const int EXPLOSION_RADIUS = 200;

	CMissile();
	~CMissile();

#ifdef HL1_DLL
	Class_T Classify( void ) { return CLASS_NONE; }
#else
	Class_T Classify( void ) { return CLASS_MISSILE; }
#endif
	
	void	Spawn( void );
	void	Precache( void );
	void	MissileTouch( CBaseEntity *pOther );
	void	Explode( void );
	void	ShotDown( void );
	void	AccelerateThink( void );
	void	AugerThink( void );
	void	IgniteThink( void );
	void	SeekThink( void );
	void	DumbFire( void );
	void	SetGracePeriod( float flGracePeriod );

	int		OnTakeDamage_Alive( const CTakeDamageInfo &info );
	void	Event_Killed( const CTakeDamageInfo &info );
	
	virtual float	GetDamage() { return m_flDamage; }
	virtual void	SetDamage(float flDamage) { m_flDamage = flDamage; }

	unsigned int PhysicsSolidMaskForEntity( void ) const;

	CHandle<CWeaponRPG>		m_hOwner;

	static CMissile *Create( const Vector &vecOrigin, const QAngle &vecAngles, edict_t *pentOwner );

	void CreateDangerSounds( bool bState ){ m_bCreateDangerSounds = bState; }

	static void AddCustomDetonator( CBaseEntity *pEntity, float radius, float height = -1 );
	static void RemoveCustomDetonator( CBaseEntity *pEntity );

protected:
	virtual void DoExplosion();	
	virtual void ComputeActualDotPosition( CLaserDotRPG *pLaserDot, Vector *pActualDotPosition, float *pHomingSpeed );
	virtual int AugerHealth() { return m_iMaxHealth - 20; }

	// Creates the smoke trail
	void CreateSmokeTrail( void );

	// Gets the shooting position 
	void GetShootPosition( CLaserDotRPG *pLaserDot, Vector *pShootPosition );

	CHandle<RocketTrail>	m_hRocketTrail;
	float					m_flAugerTime;		// Amount of time to auger before blowing up anyway
	float					m_flMarkDeadTime;
	float					m_flDamage;

	struct CustomDetonator_t
	{
		EHANDLE hEntity;
		float radiusSq;
		float halfHeight;
	};

	static CUtlVector<CustomDetonator_t> gm_CustomDetonators;

private:
	float					m_flGracePeriodEndsAt;
	bool					m_bCreateDangerSounds;

	DECLARE_DATADESC();
};


//-----------------------------------------------------------------------------
// Laser dot control
//-----------------------------------------------------------------------------
CBaseEntity *CreateLaserDot( const Vector &origin, CBaseEntity *pOwner, bool bVisibleDot );
void SetLaserDotTarget( CBaseEntity *pLaserDot, CBaseEntity *pTarget );
void EnableLaserDot( CBaseEntity *pLaserDot, bool bEnable );
#ifdef DARKINTERVAL
CBaseEntity *CreateLaserDotAPC(const Vector &origin, CBaseEntity *pOwner, bool bVisibleDot);
void SetLaserDotAPCTarget(CBaseEntity *pLaserDot, CBaseEntity *pTarget);
void EnableLaserDotAPC(CBaseEntity *pLaserDot, bool bEnable);

CBaseEntity *CreateLaserDotAPCNPC(const Vector &origin, CBaseEntity *pOwner, bool bVisibleDot);
void SetLaserDotAPCNPCTarget(CBaseEntity *pLaserDot, CBaseEntity *pTarget);
void EnableLaserDotAPCNPC(CBaseEntity *pLaserDot, bool bEnable);

CBaseEntity *CreateLaserDotRPG(const Vector &origin, CBaseEntity *pOwner, bool bVisibleDot);
void SetLaserDotRPGTarget(CBaseEntity *pLaserDot, CBaseEntity *pTarget);
void EnableLaserDotRPG(CBaseEntity *pLaserDot, bool bEnable);
#endif // DARKINTERVAL
//-----------------------------------------------------------------------------
// Specialized missile fired by player 
//-----------------------------------------------------------------------------
class CAPCMissile : public CMissile
{
#ifdef DARKINTERVAL
	DECLARE_CLASS( CAPCMissile, CMissile );
#else
	DECLARE_CLASS(CMissile, CMissile);
#endif
	DECLARE_DATADESC();

public:
	static CAPCMissile *Create( const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, CBaseEntity *pOwner );

	CAPCMissile();
	~CAPCMissile();
	void	IgniteDelay( void );
	void	AugerDelay( float flDelayTime );
	void	ExplodeDelay( float flDelayTime );
#ifdef DARKINTERVAL
	void	EnableGuiding();
#endif
	void	DisableGuiding();
#if defined( HL2_DLL )
	virtual Class_T Classify ( void ) { return CLASS_COMBINE; }
#endif

	void	AimAtSpecificTarget( CBaseEntity *pTarget );
	void	SetGuidanceHint( const char *pHintName );

	void	APCSeekThink( void );

	CAPCMissile			*m_pNext;

protected:
	virtual void DoExplosion();	
#ifdef DARKINTERVAL
	virtual void ComputeActualDotPosition( CLaserDotAPC *pLaserDot, Vector *pActualDotPosition, float *pHomingSpeed );
#else
	virtual void ComputeActualDotPosition(CLaserDot *pLaserDot, Vector *pActualDotPosition, float *pHomingSpeed);
#endif
	virtual int AugerHealth();
#ifdef DARKINTERVAL
protected:
#else
private:
#endif
	void Init();
	void ComputeLeadingPosition( const Vector &vecShootPosition, CBaseEntity *pTarget, Vector *pLeadPosition );
	void BeginSeekThink();
	void AugerStartThink();
	void ExplodeThink();
	void APCMissileTouch( CBaseEntity *pOther );

	float	m_flReachedTargetTime;
	float	m_flIgnitionTime;
	bool	m_bGuidingDisabled;
	float   m_flLastHomingSpeed;
	EHANDLE m_hSpecificTarget;
	string_t m_strHint;
};
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Specialized missile fired only by NPC APC Drivers
//-----------------------------------------------------------------------------
class CAPCMissileNPC : public CAPCMissile
{
	DECLARE_CLASS(CAPCMissileNPC, CAPCMissile);
	DECLARE_DATADESC();

public:
	static CAPCMissileNPC *Create(const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, CBaseEntity *pOwner);
	CAPCMissileNPC();
	~CAPCMissileNPC();
	void	APCSeekThink(void);
	CAPCMissileNPC *m_pNext;
	virtual void ComputeActualDotPosition(CLaserDotAPCNPC *pLaserDot, Vector *pActualDotPosition, float *pHomingSpeed);
};
#endif
//-----------------------------------------------------------------------------
// Finds apc missiles in cone
//-----------------------------------------------------------------------------
#ifdef DARKINTERVAL
CAPCMissileNPC *FindAPCMissileInCone(const Vector &vecOrigin, const Vector &vecDirection, float flAngle);
#else
CAPCMissile *FindAPCMissileInCone(const Vector &vecOrigin, const Vector &vecDirection, float flAngle);
#endif
//-----------------------------------------------------------------------------
// RPG
//-----------------------------------------------------------------------------
class CWeaponRPG : public CBaseHLCombatWeapon
{
	DECLARE_CLASS(CWeaponRPG, CBaseHLCombatWeapon);
public:

	CWeaponRPG();
	~CWeaponRPG();

	DECLARE_SERVERCLASS();

	void	Precache(void);

	void	PrimaryAttack(void);
	virtual float GetFireRate(void) { return 1; };
	void	ItemPostFrame(void);

	void	Activate(void);
	void	DecrementAmmo(CBaseCombatCharacter *pOwner);

	bool	Deploy(void);
	bool	Holster(CBaseCombatWeapon *pSwitchingTo = NULL);
	bool	Reload(void);
	bool	WeaponShouldBeLowered(void);
	bool	Lower(void);

	virtual void Drop(const Vector &vecVelocity);

	int		GetMinBurst() { return 1; }
	int		GetMaxBurst() { return 1; }
	float	GetMinRestTime() { return 4.0; }
	float	GetMaxRestTime() { return 4.0; }

	bool	WeaponLOSCondition(const Vector &ownerPos, const Vector &targetPos, bool bSetConditions);
	int		WeaponRangeAttack1Condition(float flDot, float flDist);

	void	Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);
	void	StartGuiding(void);
	void	StopGuiding(void);
	void	ToggleGuiding(void);
	bool	IsGuiding(void);

	void	NotifyRocketDied(void);

	bool	HasAnyAmmo(void);

	void	SuppressGuiding(bool state = true);

	void	CreateLaserPointer(void);
	void	UpdateLaserPosition(Vector vecMuzzlePos = vec3_origin, Vector vecEndPos = vec3_origin);
	Vector	GetLaserPosition(void);
	void	StartLaserEffects(void);
	void	StopLaserEffects(void);
	void	UpdateLaserEffects(void);

	// NPC RPG users cheat and directly set the laser pointer's origin
	void	UpdateNPCLaserPosition(const Vector &vecTarget);
	void	SetNPCLaserPosition(const Vector &vecTarget);
	const Vector &GetNPCLaserPosition(void);

	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread(void)
	{
		static Vector cone = VECTOR_CONE_3DEGREES;
		return cone;
	}

	CBaseEntity *GetMissile(void) { return m_hMissile; }

	DECLARE_ACTTABLE();
	DECLARE_DATADESC();

protected:

	bool				m_bInitialStateUpdate;
	bool				m_bGuiding;
	bool				m_bHideGuiding;		//User to override the player's wish to guide under certain circumstances
	Vector				m_vecNPCLaserDot;
	CHandle<CLaserDot>	m_hLaserDot;
	CHandle<CMissile>	m_hMissile;
	CHandle<CSprite>	m_hLaserMuzzleSprite;
	CHandle<CBeam>		m_hLaserBeam;
};
#endif // WEAPON_RPG_H
