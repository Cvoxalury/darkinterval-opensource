#include "cbase.h"
#include "vehicle_apc.h"
#include "ai_basenpc.h"
#include "ammodef.h"
#include "effect_dispatch_data.h"
#include "engine/ienginesound.h"
#include "entityflame.h"
#include "eventqueue.h"
#include "env_explosion.h"
#include "gib.h"
#include "globalstate.h"
#include "grenade_homer.h"
#include "ieffects.h"
#include "in_buttons.h"
#include "ndebugoverlay.h"
#include "particle_parse.h"
#include "particle_system.h"
#include "smoke_trail.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "weapon_rpg.h"
#include "npc_vehicledriver.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#ifdef DARKINTERVAL
static string_t s_iszAPC_Classname; // currently unused
static string_t s_iszHopper_Classname; // currently unused; would've been used for oulining hostile hopper mines

//bool CPropAPC::m_sbStaticPoseParamsLoaded = false;

int CPropAPC::m_poseparam_door = 0;
int CPropAPC::m_poseparam_headlight = 0;
int CPropAPC::m_poseparam_handbrake = 0;
int CPropAPC::m_poseparam_thruster = 0;
int CPropAPC::m_poseparam_gun_pitch = 0;
int CPropAPC::m_poseparam_gun_yaw = 0;
#endif
#ifdef DARKINTERVAL
#define ROCKET_ATTACK_RANGE_MAX			sk_apc_rocket_max_range.GetFloat() // 5500
#define ROCKET_ATTACK_RANGE_MIN			sk_apc_rocket_min_range.GetFloat() //1250

#define MACHINE_GUN_ATTACK_RANGE_MAX	sk_apc_gun_max_range.GetFloat() //1250
#else
#define ROCKET_ATTACK_RANGE_MAX			5500.0f
#define ROCKET_ATTACK_RANGE_MIN			1250.0f

#define MACHINE_GUN_ATTACK_RANGE_MAX	1250.0f
#endif
#define MACHINE_GUN_ATTACK_RANGE_MIN	0.0f

#define MACHINE_GUN_MAX_UP_PITCH		30
#define MACHINE_GUN_MAX_DOWN_PITCH		10
#define MACHINE_GUN_MAX_LEFT_YAW		30
#define MACHINE_GUN_MAX_RIGHT_YAW		30
#ifdef DARKINTERVAL
#define MACHINE_GUN_BURST_SIZE_NPC		sk_apc_gun_burst_size.GetInt()
#define MACHINE_GUN_BURST_TIME			sk_apc_gun_refire_delay.GetFloat()
#define MACHINE_GUN_BURST_PAUSE_TIME	sk_apc_gun_reload_delay.GetFloat()
#define MACHINE_GUN_BURST_PAUSE_TIME_NPC	sk_apc_gun_npc_reload_delay.GetFloat()
#else
#define MACHINE_GUN_BURST_SIZE			10
#define MACHINE_GUN_BURST_TIME			0.075f
#define MACHINE_GUN_BURST_PAUSE_TIME	2.0f
#endif
#define ROCKET_SALVO_SIZE				5
#define ROCKET_DELAY_TIME				1.5
#define ROCKET_MIN_BURST_PAUSE_TIME		3
#define ROCKET_MAX_BURST_PAUSE_TIME		4
#ifdef DARKINTERVAL
#define ROCKET_SPEED					sk_apc_rocket_speed.GetFloat() //800
#else
#define ROCKET_SPEED					800
#endif
#define DEATH_VOLLEY_ROCKET_COUNT		4
#define DEATH_VOLLEY_MIN_FIRE_TIME		0.333
#define DEATH_VOLLEY_MAX_FIRE_TIME		0.166

#define BODYGROUP_HOLO_HULL	1			// turned on if hull integrity is critical
#define BODYGROUP_HOLO_ROCKETS 2		// turned on if rockets are in cooldown
#define BODYGROUP_HOLO_JUMP	3			// turned on if driver is in place, off if vacant

extern short g_sModelIndexFireball; // Echh...

static ConVar sk_apc_health( "sk_apc_health", "750" );
#ifdef DARKINTERVAL
static ConVar sk_apc_rocket_max_range("sk_apc_rocket_max_range", "6000");
static ConVar sk_apc_rocket_min_range("sk_apc_rocket_min_range", "256");
static ConVar sk_apc_rocket_speed("sk_apc_rocket_speed", "2000");
static ConVar sk_apc_gun_max_range("sk_apc_gun_max_range", "2000");
static ConVar sk_apc_gun_burst_size("sk_apc_gun_npc_burst_size", "10");
static ConVar sk_apc_gun_refire_delay("sk_apc_gun_refire_delay", "0.18");
static ConVar sk_apc_gun_reload_delay("sk_apc_gun_reload_delay", "4");
static ConVar sk_apc_gun_npc_reload_delay("sk_apc_gun_npc_reload_delay", "8");
static ConVar sk_apc_auto_repair_max_amount("sk_apc_auto_repair_max_amount", "300"); // self repair kicks in below this value, stops if this value is reached
static ConVar sk_apc_auto_repair_increment("sk_apc_auto_repair_increment", "2"); // self repair adds this much hull points on each think when active
static ConVar sk_apc_auto_repair_combat_delay("sk_apc_auto_repair_combat_delay", "10"); // must not take damage for this many seconds for self repair to activate
static ConVar sk_apc_auto_repair_think_delay("sk_apc_auto_repair_think_delay", "2.0"); // self repair adds hull points once in this many seconds when active
static ConVar sk_apc_auto_repair_enable_on_npcs("sk_apc_auto_repair_enable_on_npcs", "1"); // whether or not APCs operated by NPC drivers have self-repair too

ConVar	hud_apchint_numentries("hud_apchint_numentries", "5", FCVAR_NONE);

extern ConVar sv_vehicle_autoaim_scale;
extern ConVar autoaim_max_dist;
#endif
#define APC_MAX_CHUNKS	3
static const char *s_pChunkModelName[APC_MAX_CHUNKS] = 
{
	"models/gibs/helicopter_brokenpiece_01.mdl",
	"models/gibs/helicopter_brokenpiece_02.mdl",
	"models/gibs/helicopter_brokenpiece_03.mdl",
};

#define APC_MAX_GIBS	6
static const char *s_pGibModelName[APC_MAX_GIBS] = 
{
	"models/combine_apc_destroyed_gib01.mdl",
	"models/combine_apc_destroyed_gib02.mdl",
	"models/combine_apc_destroyed_gib03.mdl",
	"models/combine_apc_destroyed_gib04.mdl",
	"models/combine_apc_destroyed_gib05.mdl",
	"models/combine_apc_destroyed_gib06.mdl",
};

LINK_ENTITY_TO_CLASS( prop_vehicle_apc, CPropAPC );

BEGIN_DATADESC( CPropAPC )
#ifdef DARKINTERVAL
	DEFINE_FIELD(m_flDoorCloseTime,			FIELD_TIME),
#endif
	DEFINE_FIELD( m_flDangerSoundTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flHandbrakeTime,		FIELD_TIME ),
	DEFINE_FIELD( m_bInitialHandbrake,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nSmokeTrailCount,		FIELD_INTEGER ),
	DEFINE_FIELD( m_flMachineGunTime,		FIELD_TIME ),
	DEFINE_FIELD( m_iMachineGunBurstLeft,	FIELD_INTEGER ),
//	DEFINE_FIELD( m_nMachineGunMuzzleAttachment,	FIELD_INTEGER ),
//	DEFINE_FIELD( m_nMachineGunBaseAttachment,	FIELD_INTEGER ),
//	DEFINE_FIELD( m_vecBarrelPos,			FIELD_VECTOR ),
#ifdef DARKINTERVAL
	DEFINE_FIELD( m_aimYaw,					FIELD_FLOAT),
	DEFINE_FIELD( m_aimPitch,				FIELD_FLOAT),
	DEFINE_FIELD( m_flRechargeTime,			FIELD_TIME),
	DEFINE_FIELD( m_flChargeRemainder,		FIELD_FLOAT),
	DEFINE_FIELD( m_flDrainRemainder,		FIELD_FLOAT),
	DEFINE_FIELD( m_nGunState,				FIELD_INTEGER),
	DEFINE_FIELD( m_flRocketAimingTime,		FIELD_TIME),
	DEFINE_FIELD( m_flRocketReloadTime,		FIELD_TIME),
	DEFINE_FIELD( m_flRocketStartAimTime,	FIELD_FLOAT),
	DEFINE_FIELD( m_nRocketState,			FIELD_INTEGER),
	DEFINE_FIELD(m_flThrustersTime,			FIELD_TIME),
#endif
	DEFINE_FIELD( m_bInFiringCone,			FIELD_BOOLEAN ),
#ifdef DARKINTERVAL
	DEFINE_FIELD( m_hLaserDot,				FIELD_EHANDLE ),
	DEFINE_FIELD( m_flLaserTargetTime,		FIELD_TIME),
#endif
	DEFINE_FIELD( m_hRocketTarget,			FIELD_EHANDLE ),
	DEFINE_FIELD( m_iRocketSalvoLeft,		FIELD_INTEGER ),
	DEFINE_FIELD( m_flRocketTime,			FIELD_TIME ),
//	DEFINE_FIELD( m_nRocketAttachment,		FIELD_INTEGER ),
	DEFINE_FIELD( m_nRocketSide,			FIELD_INTEGER ),
	DEFINE_FIELD( m_hSpecificRocketTarget,  FIELD_EHANDLE ),
	DEFINE_KEYFIELD(m_strMissileHint,		FIELD_STRING, "missilehint"),
#ifdef DARKINTERVAL
	DEFINE_FIELD( m_nAmmoCount,				FIELD_INTEGER),
	DEFINE_FIELD( m_nAmmo2Count,			FIELD_INTEGER),
	DEFINE_FIELD( m_bHeadlightIsOn,			FIELD_BOOLEAN),
	DEFINE_FIELD( m_iNumberOfEntries,		FIELD_INTEGER),
	DEFINE_FIELD( m_turretAmmoSprite,		FIELD_CLASSPTR),
	DEFINE_FIELD( m_repairStateSprite,		FIELD_CLASSPTR),
	DEFINE_FIELD( m_cabinStandbySprite,		FIELD_CLASSPTR),
	DEFINE_EMBEDDED(m_hullBlink_timer),
	// no need to save this - if player wants
	// to bother with saving & reloading during firing,
	// hen be my guest.
//	DEFINE_FIELD( m_accuracy_penalty_float,		FIELD_FLOAT), 
														
	DEFINE_EMBEDDED(m_NextDenyRocketSoundTime),
	DEFINE_EMBEDDED(m_NextDenyTurretSoundTime),

	DEFINE_ARRAY(m_WheelFxTime, FIELD_TIME, NUM_WHEEL_EFFECTS),
	DEFINE_ARRAY(m_hWheelDust, FIELD_EHANDLE, NUM_WHEEL_EFFECTS),
	DEFINE_ARRAY(m_hWheelWater, FIELD_EHANDLE, NUM_WHEEL_EFFECTS),

	// Give or take health (hull integrity). // DI TODO: promote for all practical vehicles?
	DEFINE_INPUTFUNC(FIELD_INTEGER, "SetHealth", InputSetHealth),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetHealthFraction", InputSetHealthFraction),
	DEFINE_INPUTFUNC(FIELD_INTEGER, "AddHealth", InputAddHealth),
	DEFINE_INPUTFUNC(FIELD_INTEGER, "RemoveHealth", InputRemoveHealth),
	DEFINE_INPUTFUNC(FIELD_VOID, "GetHealth", InputRequestHealth),
#endif // DARKINTERVAL
	DEFINE_INPUTFUNC( FIELD_VOID, "Destroy", InputDestroy ),
	DEFINE_INPUTFUNC( FIELD_STRING, "FireMissileAt", InputFireMissileAt ),
#ifdef DARKINTERVAL
	DEFINE_INPUTFUNC(FIELD_VOID, "ShowHudHint", InputShowHudHint),
#endif
	DEFINE_OUTPUT( m_OnDeath,				"OnDeath" ),
	DEFINE_OUTPUT( m_OnFiredMissile,		"OnFiredMissile" ),
	DEFINE_OUTPUT( m_OnDamaged,				"OnDamaged" ),
	DEFINE_OUTPUT( m_OnDamagedByPlayer,		"OnDamagedByPlayer" ),
#ifdef DARKINTERVAL
	DEFINE_OUTPUT( m_OnFullHealth,			"OnFullHealth" ),
	DEFINE_OUTPUT( m_outputHealth_int,		"CurrentHealth"),

	DEFINE_THINKFUNC(AutoRepairThink),
	DEFINE_FIELD(m_auto_repair_goal_hull_amount_int, FIELD_INTEGER),
#endif
END_DATADESC()
#ifdef DARKINTERVAL
IMPLEMENT_SERVERCLASS_ST(CPropAPC, DT_PropAPC)
	SendPropInt(SENDINFO(m_nAmmoCount), 9),
	SendPropInt(SENDINFO(m_nAmmo2Count), 10),
	SendPropBool(SENDINFO(m_bHeadlightIsOn)),
END_SEND_TABLE();
#endif
#ifdef DARKINTERVAL
CPropAPC::CPropAPC() :
	m_NextDenyRocketSoundTime(0.5),
	m_NextDenyTurretSoundTime(0.5),
	m_flDoorCloseTime(0),
	m_accuracy_penalty_float(0),
	m_auto_repair_goal_hull_amount_int(0),
	m_turretAmmoSprite(NULL),
	m_repairStateSprite(NULL),
	m_cabinStandbySprite(NULL),
	m_iNumberOfEntries(0)
{
}

CPropAPC::~CPropAPC()
{
}

#define APC_TURRET_SPRITE "sprites/grubflare1.vmt"
#define APC_AUTOREPAIR_SPRITE "sprites/grubflare1.vmt"
#endif
void CPropAPC::Precache( void )
{
	BaseClass::Precache();

	int i;
	for ( i = 0; i < APC_MAX_CHUNKS; ++i )
	{
		PrecacheModel( s_pChunkModelName[i] );
	}

	for ( i = 0; i < APC_MAX_GIBS; ++i )
	{
		PrecacheModel( s_pGibModelName[i] );
	}
#ifdef DARKINTERVAL
	PrecacheScriptSound("PropAPC.Headlight_on");
	PrecacheScriptSound("PropAPC.Headlight_off");
	PrecacheScriptSound("PropAPC.DenyTurret");
	PrecacheScriptSound("PropAPC.DenyRocket");
	PrecacheScriptSound("PropAPC.DryFireRocket");
	PrecacheScriptSound("PropAPC.StopFiringTurret");
	PrecacheScriptSound("PropAPC.FireTurret");
	PrecacheScriptSound("PropAPC.FireTurretNPC");
	PrecacheScriptSound("PropAPC.FireRocket");
	PrecacheScriptSound("PropAPC.Thrusters");
	PrecacheScriptSound("PropAPC.DoorOpen");
	PrecacheScriptSound("PropAPC.DoorClose");
	PrecacheScriptSound("PropAPC.StartEngine");
	PrecacheScriptSound("PropAPC.StopEngine");
	PrecacheScriptSound("combine.door_lock");

	// APC repair mechanism sounds
	PrecacheScriptSound("APC_Repair_Mech.StandBy");
	PrecacheScriptSound("APC_Repair_Mech.Servos1");
	PrecacheScriptSound("APC_Repair_Mech.Servos2");
	PrecacheScriptSound("APC_Repair_Mech.Servos3");
	PrecacheScriptSound("APC_Repair_Mech.Servos4");
	PrecacheScriptSound("APC_Repair_Mech.Sparks1");
	PrecacheScriptSound("APC_Repair_Mech.Sparks2");
	PrecacheScriptSound("APC_Repair_Mech.Sparks3");
	PrecacheScriptSound("APC_Repair_Mech.Sparks4");
	PrecacheScriptSound("APC_Repair_Mech.Sparks5");
	PrecacheScriptSound("APC_Repair_Mech.Sparks6");
	PrecacheScriptSound("APC_Repair_Mech.Sparks7");
	PrecacheScriptSound("APC_Repair_Mech.Sparks8");
	PrecacheScriptSound("APC_Repair_Mech.Weld");
	PrecacheScriptSound("APC_Repair_Mech.Clamp1");
	PrecacheScriptSound("APC_Repair_Mech.Clamp2");

	PrecacheModel("models/weapons/w_missile_launch.mdl");
	UTIL_PrecacheOther("grenade_homer"); // for NPC rockets

	PrecacheModel(APC_TURRET_SPRITE);
	PrecacheModel("models/combine_apc_driveable.mdl"); // always precache the driveable model

	PrecacheParticleSystem("impact_concrete");
	PrecacheParticleSystem("apc_muzzleflash");
	PrecacheParticleSystem("WheelDust");
	PrecacheParticleSystem("WheelSplash");
#else
	PrecacheScriptSound("Weapon_AR2.Single");
	PrecacheScriptSound("PropAPC.FireRocket");
	PrecacheScriptSound("combine.door_lock");
#endif
}

void CPropAPC::Spawn( void )
{
#ifdef DARKINTERVAL
	m_nAmmoCount = m_bHasGun ? 0 : -1;
	m_nAmmo2Count = m_bHasGun ? 0 : -1;

	// Setup vehicle as a real-wheels car.
	SetVehicleType(VEHICLE_TYPE_CAR_WHEELS);
#endif
	BaseClass::Spawn();

	SetBlocksLOS( true );
	m_iHealth = m_iMaxHealth = sk_apc_health.GetFloat();
	SetCycle( 0 );
	m_iMachineGunBurstLeft = MACHINE_GUN_BURST_SIZE_NPC;
#ifdef DARKINTERVAL
	if (GetDriver() && GetDriver()->IsPlayer())
		m_iMachineGunBurstLeft = 100;
#endif
	m_iRocketSalvoLeft = ROCKET_SALVO_SIZE;
	m_nRocketSide = 0;
	m_lifeState = LIFE_ALIVE;
	m_bInFiringCone = false;

	m_flHandbrakeTime = CURTIME + 0.1;
	m_bInitialHandbrake = false;
#ifdef DARKINTERVAL
	m_nGunState = GUN_STATE_IDLE;
	m_nRocketState = ROCKET_STATE_READY;
	m_flRocketAimingTime = 0;
	m_flRocketReloadTime = 0;
	m_flRocketStartAimTime = 0;
	m_flThrustersTime = 0;

	// Reset the gun to a default pose.
	SetPoseParameter( m_poseparam_gun_pitch, 0 );
	SetPoseParameter( m_poseparam_gun_yaw, 0 );
	m_aimYaw = 0;
	m_aimPitch = 0;

	// Reset the misc poseparameters
	SetPoseParameter(m_poseparam_headlight, 0);
	SetPoseParameter(m_poseparam_handbrake, 0);
	SetPoseParameter(m_poseparam_thruster, 0);

	// Reset bodygroups
	SetBodygroup(BODYGROUP_HOLO_HULL, 0);
	SetBodygroup(BODYGROUP_HOLO_ROCKETS, 0);
	SetBodygroup(BODYGROUP_HOLO_JUMP, 0);

	m_hullBlink_timer.Set(0.5f);

	// created on spawn instead of EnterVehicle, because we want it blinking on standby, empty
	// driveable APCs
	int iCabinAttachment = LookupAttachment("cabin_sprite_left");
	if (!m_cabinStandbySprite && iCabinAttachment > 0) m_cabinStandbySprite = CSprite::SpriteCreate(APC_AUTOREPAIR_SPRITE, GetAbsOrigin(), false);
	if (m_cabinStandbySprite != NULL) {
		m_cabinStandbySprite->FollowEntity(this);
		m_cabinStandbySprite->SetAttachment(this, iCabinAttachment);
		m_cabinStandbySprite->SetTransparency(kRenderWorldGlow, 0, 0, 0, 255, kRenderFxStrobeSlow);
		m_cabinStandbySprite->SetScale(0.05f);
	}
#else
	// Reset the gun to a default pose.
	SetPoseParameter("vehicle_weapon_pitch", 0);
	SetPoseParameter("vehicle_weapon_yaw", 90);
#endif // DARKINTERVAL
	CreateAPCLaserDot();

	if( g_pGameRules->GetAutoAimMode() == AUTOAIM_ON_CONSOLE )
	{
		AddFlag( FL_AIMTARGET );
	}
#ifdef DARKINTERVAL
	for (int iWheel = 0; iWheel < NUM_WHEEL_EFFECTS; ++iWheel)
	{
		m_WheelFxTime[iWheel] = 0;
	}

	m_bHasGun = true;

	RegisterThinkContext("AutoRepairContext"); // when outside of combat, the vehicle is allowed to slowly repair itself

	s_iszAPC_Classname = AllocPooledString("prop_vehicle_apc");
	s_iszHopper_Classname = AllocPooledString("combine_mine");
#endif
}

void CPropAPC::CreateAPCLaserDot( void )
{
#ifdef DARKINTERVAL
	CBaseEntity *pOwner = this;
	if (GetDriver() && GetDriver()->IsPlayer())
	{
		pOwner = GetDriver();
	}
	// Create a laser if we don't have one
	if ( m_hLaserDot == NULL )
	{
		m_hLaserDot = CreateLaserDotAPC( GetAbsOrigin(), pOwner, false );
	}
#else
	if ( m_hLaserDot == NULL )
	{
		m_hLaserDot = CreateLaserDot(GetAbsOrigin(), this, false);
	}
#endif
}

bool CPropAPC::ShouldAttractAutoAim( CBaseEntity *pAimingEnt )
{
#ifdef DARKINTERVAL // DI todo - autoaim doesn't work the way it should (it's a general issue)
	if( pAimingEnt->IsPlayer() && GetDriver() )
#else
	if ( g_pGameRules->GetAutoAimMode() == AUTOAIM_ON_CONSOLE && pAimingEnt->IsPlayer() && GetDriver() )
#endif
	{
		return true;
	}

	return BaseClass::ShouldAttractAutoAim( pAimingEnt );
}

void CPropAPC::Activate()
{
	BaseClass::Activate();
#ifdef DARKINTERVAL
	CBaseServerVehicle *pServerVehicle = dynamic_cast<CBaseServerVehicle *>(GetServerVehicle());
	if (pServerVehicle)
	{
		if (pServerVehicle->GetPassenger())
		{
			// If a jeep comes back from a save game with a driver, make sure the engine rumble starts up.
			pServerVehicle->StartEngineRumble();
		}
	}
#endif
	m_nRocketAttachment = LookupAttachment( "cannon_muzzle" );
	m_nMachineGunMuzzleAttachment = LookupAttachment( "muzzle" );
	m_nMachineGunBaseAttachment = LookupAttachment( "gun_base" );

	// NOTE: gun_ref must have the same position as gun_base, but rotates with the gun
	int nMachineGunRefAttachment = LookupAttachment( "gun_def" );

	Vector vecWorldBarrelPos;
	matrix3x4_t matRefToWorld;
	GetAttachment( m_nMachineGunMuzzleAttachment, vecWorldBarrelPos );
	GetAttachment( nMachineGunRefAttachment, matRefToWorld );
	VectorITransform( vecWorldBarrelPos, matRefToWorld, m_vecBarrelPos );
}
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: Cache whatever pose parameters we intend to use
//-----------------------------------------------------------------------------
void	CPropAPC::PopulatePoseParameters(void)
{
	if (!m_sbStaticPoseParamsLoaded)
	{
		m_poseparam_door = LookupPoseParameter("vehicle_door_pos");
		m_poseparam_headlight = LookupPoseParameter("vehicle_headlight_tumbler");
		m_poseparam_handbrake = LookupPoseParameter("vehicle_handbrake");
		m_poseparam_thruster = LookupPoseParameter("vehicle_holo_jump");
		m_poseparam_gun_pitch = LookupPoseParameter("vehicle_weapon_pitch");
		m_poseparam_gun_yaw = LookupPoseParameter("vehicle_weapon_yaw");

		m_sbStaticPoseParamsLoaded = true;
	}

	BaseClass::PopulatePoseParameters();
}
#endif
void CPropAPC::UpdateOnRemove( void )
{
	if ( m_hLaserDot )
	{
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}
#ifdef DARKINTERVAL
	// Kill our wheel effects
	for (int i = 0; i < NUM_WHEEL_EFFECTS; i++)
	{
		if (m_hWheelDust[i] != NULL)
		{
			UTIL_Remove(m_hWheelDust[i]);
		}

		if (m_hWheelWater[i] != NULL)
		{
			UTIL_Remove(m_hWheelWater[i]);
		}
	}
#endif
	BaseClass::UpdateOnRemove();
}

void CPropAPC::CreateServerVehicle( void )
{
	// Create our armed server vehicle
	m_pServerVehicle = new CAPCFourWheelServerVehicle();
	m_pServerVehicle->SetVehicle( this );
}

Class_T	CPropAPC::ClassifyPassenger( CBaseCombatCharacter *pPassenger, Class_T defaultClassification )
{ 
	return CLASS_COMBINE; // this has no effect on the player driving APCs
}

float CPropAPC::PassengerDamageModifier( const CTakeDamageInfo &inputInfo)
{ 
	CTakeDamageInfo DmgInfo = inputInfo;

	// bullets, slashing and headbutts don't hurt us in the apc, neither do rockets
#ifdef DARKINTERVAL
	if( (DmgInfo.GetDamageType() & DMG_BULLET) || (DmgInfo.GetDamageType() & DMG_SLASH) ||
		(DmgInfo.GetDamageType() & DMG_CLUB))
		return (0);

	if (DmgInfo.GetDamageType() && DMG_CRUSH) return (0.0f);

	if (DmgInfo.GetDamageType() & DMG_BLAST) return (0.2f);

	if (DmgInfo.GetDamageType() & DMG_ACID) return (0.02f);

	if (DmgInfo.GetDamageType() & DMG_BURN) return (0.02f);
#else
	if ( ( DmgInfo.GetDamageType() & DMG_BULLET ) || ( DmgInfo.GetDamageType() & DMG_SLASH ) ||
		( DmgInfo.GetDamageType() & DMG_CLUB ) || ( DmgInfo.GetDamageType() & DMG_BLAST ) )
		return ( 0 );
#endif
	// Accept everything else by default
	return 1.0; 
}

Vector CPropAPC::EyePosition( )
{
	Vector vecEyePosition;
	CollisionProp()->NormalizedToWorldSpace( Vector( 0.5, 0.5, 1.0 ), &vecEyePosition );
	return vecEyePosition;
}

Vector CPropAPC::BodyTarget( const Vector &posSrc, bool bNoisy ) 
{
#ifndef DARKINTERVAL
	if( g_pGameRules->GetAutoAimMode() == AUTOAIM_ON_CONSOLE )
	{
		return WorldSpaceCenter();
	}

	return BaseClass::BodyTarget( posSrc, bNoisy );
#else
	Vector	shotPos;
	matrix3x4_t	matrix;

	int eyeAttachmentIndex = LookupAttachment("cannon_muzzle"); // FIXME
	GetAttachment(eyeAttachmentIndex, matrix);
	MatrixGetColumn(matrix, 3, shotPos);

	if (bNoisy)
	{
		shotPos[0] += random->RandomFloat(-8.0f, 8.0f);
		shotPos[1] += random->RandomFloat(-8.0f, 8.0f);
		shotPos[2] += random->RandomFloat(-8.0f, 8.0f);
	}

	return shotPos;
#endif
}
#ifdef DARKINTERVAL
// Wheel effects
#define	MIN_WHEEL_DUST_SPEED	5

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAPC::UpdateWheelFx(void)
{
	// See if this wheel should emit dust
	const vehicleparams_t *vehicleData = m_pServerVehicle->GetVehicleParams();
	const vehicle_operatingparams_t *carState = m_pServerVehicle->GetVehicleOperatingParams();
	bool bAllowDust = vehicleData->steering.dustCloud;

	// Car must be active
	bool bCarOn = m_VehiclePhysics.IsOn();

	// Must be moving quickly enough or skidding along the ground
	bool bCreateDust = (bCarOn &&
		bAllowDust &&
		(m_VehiclePhysics.GetSpeed() >= MIN_WHEEL_DUST_SPEED || carState->skidSpeed > DEFAULT_SKID_THRESHOLD));

	// Update our wheel dust
	Vector	vecPos;
	for (int i = 0; i < NUM_WHEEL_EFFECTS; i++)
	{
		m_pServerVehicle->GetWheelContactPoint(i, vecPos);

		// Make sure the effect is created
		if (m_hWheelDust[i] == NULL)
		{
			// Create the dust effect in place
			m_hWheelDust[i] = (CParticleSystem *)CreateEntityByName("info_particle_system");
			if (m_hWheelDust[i] == NULL)
				continue;

			// Setup our basic parameters
			m_hWheelDust[i]->KeyValue("start_active", "0");
			m_hWheelDust[i]->KeyValue("effect_name", "WheelDust");
			m_hWheelDust[i]->SetParent(this);
			m_hWheelDust[i]->SetLocalOrigin(vec3_origin);
			DispatchSpawn(m_hWheelDust[i]);
			if (CURTIME > 0.5f)
				m_hWheelDust[i]->Activate();
		}

		// Turn the dust on or off
		if (bCreateDust)
		{
			// Angle the dust out away from the wheels
			Vector vecForward, vecRight, vecUp;
			GetVectors(&vecForward, &vecRight, &vecUp);

			const vehicle_controlparams_t *vehicleControls = m_pServerVehicle->GetVehicleControlParams();
			float flWheelDir = (i & 1) ? 1.0f : -1.0f;
			QAngle vecAngles;
			vecForward += vecRight * flWheelDir;
			vecForward += vecRight * (vehicleControls->steering*0.5f) * flWheelDir;
			vecForward += vecUp;
			VectorAngles(vecForward, vecAngles);

			m_hWheelDust[i]->StartParticleSystem();
			m_hWheelDust[i]->SetAbsAngles(vecAngles);
			m_hWheelDust[i]->SetAbsOrigin(vecPos + Vector(0, 0, 8));

			if (m_WheelFxTime[i] < CURTIME)
			{
				// Stagger ripple times
				m_WheelFxTime[i] = CURTIME + RandomFloat(0.15, 0.2);
				Vector wheelWaterPos = vecPos;
				if (UTIL_PointContents(vecPos) & CONTENTS_WATER || UTIL_PointContents(vecPos) & CONTENTS_SLIME)
				{
					bool slime = false;
					if (UTIL_PointContents(vecPos) & CONTENTS_SLIME)
					{
						slime = true;
					}
					float wheelWaterHeight = UTIL_FindWaterSurface(vecPos, -8192, 8192);
					vecPos.z = wheelWaterHeight;

					CEffectData	data;
					data.m_fFlags = 0;
					data.m_vOrigin = vecPos + vecRight * (vehicleControls->steering*0.5f) * flWheelDir;

					if (slime)
					{
						data.m_fFlags |= FX_WATER_IN_SLIME;
					}

					data.m_fFlags |= FX_NO_SPLASH_SOUND;

					Vector vecSplashDir;
					vecSplashDir = (vecForward + vecUp) * 0.5f;
					VectorNormalize(vecSplashDir);
					data.m_vNormal = vecSplashDir;
					data.m_flScale = 10.0f + random->RandomFloat(0, 10.0f * 0.25);
					data.m_flMagnitude = 1; // hack. In the splash/ripple code, magnitude of 1 disables bumpmapped splashes (they're expensive).
					DispatchEffect("watersplash", data);
				}
			}
		}
		else
		{
			// Stop emitting
			m_hWheelDust[i]->StopParticleSystem();
		}
	}
}
#endif // DARKINTERVAL
void CPropAPC::AddSmokeTrail( const Vector &vecPos )
{
	// Start this trail out with a bang!
#ifdef DARKINTERVAL
	ExplosionCreate( vecPos, vec3_angle, this, 1000, 500.0f, SF_ENVEXPLOSION_NODAMAGE | 
		SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NOSMOKE | 
		SF_ENVEXPLOSION_NOFIREBALLSMOKE, 0 );
#else
	ExplosionCreate(vecPos, vec3_angle, this, 1000, 500.0f, SF_ENVEXPLOSION_NODAMAGE |
		SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE |
		SF_ENVEXPLOSION_NOFIREBALLSMOKE, 0 );

#endif
	UTIL_ScreenShake( vecPos, 25.0, 150.0, 1.0, 750.0f, SHAKE_START );

	if ( m_nSmokeTrailCount == MAX_SMOKE_TRAILS )
		return;

	SmokeTrail *pSmokeTrail =  SmokeTrail::CreateSmokeTrail();
	if( !pSmokeTrail )
		return;

	// See if there's an attachment for this smoke trail
	char buf[32];
	Q_snprintf( buf, 32, "damage%d", m_nSmokeTrailCount );
	int nAttachment = LookupAttachment( buf );

	++m_nSmokeTrailCount;

	pSmokeTrail->m_SpawnRate = 4;
	pSmokeTrail->m_ParticleLifetime = 5.0f;
	pSmokeTrail->m_StartColor.Init( 0.7f, 0.7f, 0.7f );
	pSmokeTrail->m_EndColor.Init( 0.6, 0.6, 0.6 );
	pSmokeTrail->m_StartSize = 32;
	pSmokeTrail->m_EndSize = 64;
	pSmokeTrail->m_SpawnRadius = 4;
	pSmokeTrail->m_Opacity = 0.5f;
	pSmokeTrail->m_MinSpeed = 16;
	pSmokeTrail->m_MaxSpeed = 16;
	pSmokeTrail->m_MinDirectedSpeed	= 16.0f;
	pSmokeTrail->m_MaxDirectedSpeed	= 16.0f;
	pSmokeTrail->SetLifetime( 5 );
	pSmokeTrail->SetParent( this, nAttachment );

	Vector vecForward( 0, 0, 1 );
	QAngle angles;
	VectorAngles( vecForward, angles );

	if ( nAttachment == 0 )
	{
		pSmokeTrail->SetAbsOrigin( vecPos );
		pSmokeTrail->SetAbsAngles( angles );
	}
	else
	{
		pSmokeTrail->SetLocalOrigin( vec3_origin );
		pSmokeTrail->SetLocalAngles( angles );
	}

	pSmokeTrail->SetMoveType( MOVETYPE_NONE );
}

void CPropAPC::ExplodeAndThrowChunk( const Vector &vecExplosionPos )
{
#ifdef DARKINTERVAL
	ExplosionCreate( vecExplosionPos, vec3_angle, this, 1000, 500.0f, 
		SF_ENVEXPLOSION_NODAMAGE | SF_ENVEXPLOSION_NOSPARKS |
		SF_ENVEXPLOSION_NOSMOKE  | SF_ENVEXPLOSION_NOFIREBALLSMOKE, 0 );
#else
	ExplosionCreate(vecExplosionPos, vec3_angle, this, 1000, 500.0f,
		SF_ENVEXPLOSION_NODAMAGE | SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS |
		SF_ENVEXPLOSION_NOSMOKE | SF_ENVEXPLOSION_NOFIREBALLSMOKE, 0 );
#endif
	UTIL_ScreenShake( vecExplosionPos, 25.0, 150.0, 1.0, 750.0f, SHAKE_START );

	// Drop a flaming, smoking chunk.
	CGib *pChunk = CREATE_ENTITY( CGib, "gib" );
	pChunk->Spawn( "models/gibs/hgibs.mdl" );
	pChunk->SetBloodColor( DONT_BLEED );

	QAngle vecSpawnAngles;
	vecSpawnAngles.Random( -90, 90 );
	pChunk->SetAbsOrigin( vecExplosionPos );
	pChunk->SetAbsAngles( vecSpawnAngles );

	int nGib = random->RandomInt( 0, APC_MAX_CHUNKS - 1 );
	pChunk->Spawn( s_pChunkModelName[nGib] );
	pChunk->SetOwnerEntity( this );
	pChunk->m_lifeTime = random->RandomFloat( 6.0f, 8.0f );
	pChunk->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	IPhysicsObject *pPhysicsObject = pChunk->VPhysicsInitNormal( SOLID_VPHYSICS, pChunk->GetSolidFlags(), false );
	
	// Set the velocity
	if ( pPhysicsObject )
	{
		pPhysicsObject->EnableMotion( true );
		Vector vecVelocity;

		QAngle angles;
		angles.x = random->RandomFloat( -40, 0 );
		angles.y = random->RandomFloat( 0, 360 );
		angles.z = 0.0f;
		AngleVectors( angles, &vecVelocity );
		
		vecVelocity *= random->RandomFloat( 300, 900 );
		vecVelocity += GetAbsVelocity();

		AngularImpulse angImpulse;
		angImpulse = RandomAngularImpulse( -180, 180 );

		pChunk->SetAbsVelocity( vecVelocity );
		pPhysicsObject->SetVelocity(&vecVelocity, &angImpulse );
	}

	CEntityFlame *pFlame = CEntityFlame::Create( pChunk, false );
	if ( pFlame != NULL )
	{
		pFlame->SetLifetime( pChunk->m_lifeTime );
	}
}

inline bool CPropAPC::ShouldTriggerDamageEffect( int nPrevHealth, int nEffectCount, const CTakeDamageInfo &info) const
{
	if (info.GetDamageType() & DMG_ACID) return false;
	int nPrevRange = (int)( ((float)nPrevHealth / (float)GetMaxHealth()) * nEffectCount );
	int nRange = (int)( ((float)GetHealth() / (float)GetMaxHealth()) * nEffectCount );
	return ( nRange != nPrevRange );
}

void CPropAPC::Event_Killed( const CTakeDamageInfo &info )
{
	m_OnDeath.FireOutput( info.GetAttacker(), this );
#ifdef DARKINTERVAL // get rid of cabin things
	if (m_turretAmmoSprite) UTIL_Remove(m_turretAmmoSprite);
	if (m_repairStateSprite) UTIL_Remove(m_repairStateSprite);
	if (m_cabinStandbySprite) UTIL_Remove(m_cabinStandbySprite);
	SetBodygroup(BODYGROUP_HOLO_HULL, 0);
	SetBodygroup(BODYGROUP_HOLO_JUMP, 0);
	SetBodygroup(BODYGROUP_HOLO_ROCKETS, 0);
#endif
	Vector vecAbsMins, vecAbsMaxs;
	CollisionProp()->WorldSpaceAABB( &vecAbsMins, &vecAbsMaxs );

	Vector vecNormalizedMins, vecNormalizedMaxs;
	CollisionProp()->WorldToNormalizedSpace( vecAbsMins, &vecNormalizedMins );
	CollisionProp()->WorldToNormalizedSpace( vecAbsMaxs, &vecNormalizedMaxs );

	Vector vecAbsPoint;
	CPASFilter filter( GetAbsOrigin() );
	for (int i = 0; i < 5; i++)
	{
		CollisionProp()->RandomPointInBounds( vecNormalizedMins, vecNormalizedMaxs, &vecAbsPoint );
		te->Explosion( filter, random->RandomFloat( 0.0, 1.0 ),	&vecAbsPoint, 
			g_sModelIndexFireball, random->RandomInt( 4, 10 ), 
			random->RandomInt( 8, 15 ), 
			( i < 2 ) ? TE_EXPLFLAG_NODLIGHTS : TE_EXPLFLAG_NOPARTICLES | TE_EXPLFLAG_NOFIREBALLSMOKE | TE_EXPLFLAG_NODLIGHTS,
			100, 0 );
	}

	// TODO: make the gibs spawn in sync with the delayed explosions
	int nGibs = random->RandomInt( 1, 4 );
	for ( int i = 0; i < nGibs; i++)
	{
		// Throw a flaming, smoking chunk.
		CGib *pChunk = CREATE_ENTITY( CGib, "gib" );
		pChunk->Spawn( "models/gibs/hgibs.mdl" );
		pChunk->SetBloodColor( DONT_BLEED );

		QAngle vecSpawnAngles;
		vecSpawnAngles.Random( -90, 90 );
		pChunk->SetAbsOrigin( vecAbsPoint );
		pChunk->SetAbsAngles( vecSpawnAngles );

		int nGib = random->RandomInt( 0, APC_MAX_CHUNKS - 1 );
		pChunk->Spawn( s_pChunkModelName[nGib] );
		pChunk->SetOwnerEntity( this );
		pChunk->m_lifeTime = random->RandomFloat( 6.0f, 8.0f );
		pChunk->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		IPhysicsObject *pPhysicsObject = pChunk->VPhysicsInitNormal( SOLID_VPHYSICS, pChunk->GetSolidFlags(), false );
		
		// Set the velocity
		if ( pPhysicsObject )
		{
			pPhysicsObject->EnableMotion( true );
			Vector vecVelocity;

			QAngle angles;
			angles.x = random->RandomFloat( -20, 20 );
			angles.y = random->RandomFloat( 0, 360 );
			angles.z = 0.0f;
			AngleVectors( angles, &vecVelocity );
			
			vecVelocity *= random->RandomFloat( 300, 900 );
			vecVelocity += GetAbsVelocity();

			AngularImpulse angImpulse;
			angImpulse = RandomAngularImpulse( -180, 180 );

			pChunk->SetAbsVelocity( vecVelocity );
			pPhysicsObject->SetVelocity(&vecVelocity, &angImpulse );
		}

		CEntityFlame *pFlame = CEntityFlame::Create( pChunk, false );
		if ( pFlame != NULL )
		{
			pFlame->SetLifetime( pChunk->m_lifeTime );
		}
	}

	UTIL_ScreenShake( vecAbsPoint, 25.0, 150.0, 1.0, 750.0f, SHAKE_START );
#ifndef DARKINTERVAL
	if ( hl2_episodic.GetBool() )
	{
		// EP1 perf hit
		Ignite(6, false);
	} 
	else
#endif
	Ignite( 60, false );
#ifdef DARKINTERVAL // kill the driver immediately if they didn't escape in time
	if (GetDriver() && GetDriver()->IsPlayer())
	{
		CTakeDamageInfo killDriver = info;
		killDriver.SetDamage(500);
		killDriver.SetDamageType(DMG_VEHICLE);
		killDriver.ScaleDamageForce(0);
		GetDriver()->TakeDamage(killDriver);
	}
#endif
	m_lifeState = LIFE_DYING;

	// Spawn a lesser amount if the player is close
	m_iRocketSalvoLeft = DEATH_VOLLEY_ROCKET_COUNT;
	m_flRocketTime = CURTIME;
}
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: Input handler for adding to the breakable's health.
// Input  : Integer health points to add.
//-----------------------------------------------------------------------------
void CPropAPC::InputAddHealth(inputdata_t &inputdata)
{
	if (m_iHealth > 0 && inputdata.value.Int() >= 0) // don't try and heal already dead APCs, and don't do damage
	{
		m_iHealth += inputdata.value.Int();
		if (m_iHealth > m_iMaxHealth) m_iHealth = m_iMaxHealth;
		RemoveAllDecals(); // clear bullet holes, explosion marks, etc
	}

	if (m_iHealth >= m_iMaxHealth)
	{
		m_OnFullHealth.FireOutput(this, this);
	}

	m_outputHealth_int.Set(m_iHealth, this, inputdata.pCaller);
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for removing health from the breakable.
// Input  : Integer health points to remove.
//-----------------------------------------------------------------------------
void CPropAPC::InputRemoveHealth(inputdata_t &inputdata)
{
	if (m_iHealth > 0) // don't try and kill already dead APCs
	{
		// Behave like it's real damage coming from somewhere.
		CTakeDamageInfo healthChange;
		healthChange.SetDamage((float)inputdata.value.Int());
		healthChange.SetAttacker(inputdata.pActivator);
		healthChange.SetInflictor(inputdata.pActivator);
		healthChange.SetDamageType(DMG_BLAST);
		healthChange.SetDamagePosition(WorldSpaceCenter());
		healthChange.SetDamageForce(Vector(0, 0, 1));
		TakeDamage(healthChange);
	}

	if (m_iHealth >= m_iMaxHealth)
	{
		m_OnFullHealth.FireOutput(this, this);
	}

	m_outputHealth_int.Set(m_iHealth, this, inputdata.pCaller);
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for requesting health from the vehicle.
// Makes it output its current health (armour) as an integer.
//-----------------------------------------------------------------------------
void CPropAPC::InputRequestHealth(inputdata_t &inputdata)
{
	m_outputHealth_int.Set(m_iHealth, this, inputdata.pCaller);
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the breakable's health.
//-----------------------------------------------------------------------------
void CPropAPC::InputSetHealth(inputdata_t &inputdata)
{
	if (inputdata.value.Int() > m_iHealth)
	{
		RemoveAllDecals(); // clear bullet holes, explosion marks, etc
	}
	else
	{
		// Don't do damage effects in this input. We may want to have already-damaged APCs on maps,
		// but we don't want them to produce explosion noises and gibs at map start. So do it "covertly".
	}

	if (inputdata.value.Int() <= 0) // however, this should work as actual destruction of the vehicle.
	{
		CTakeDamageInfo info(this, this, m_iHealth + 1000, DMG_BLAST);
		info.SetDamagePosition(WorldSpaceCenter());
		info.SetDamageForce(Vector(0, 0, 1));
		if (GetDriver() && GetDriver()->IsPlayer())
		{
			GetDriver()->TakeDamage(info);
		}
		TakeDamage(info);
	}

	m_iHealth = inputdata.value.Int();
	if (m_iHealth > m_iMaxHealth) m_iHealth = m_iMaxHealth;

	if (m_iHealth >= m_iMaxHealth)
	{
		m_OnFullHealth.FireOutput(this, this);
	}

	m_outputHealth_int.Set(m_iHealth, this, inputdata.pCaller);
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for setting the breakable's health.
//-----------------------------------------------------------------------------
void CPropAPC::InputSetHealthFraction(inputdata_t &inputdata)
{
	float fh = inputdata.value.Float();
	if (fh > 1) fh = 1.0f; // don't allow weirdness
	if (fh <= 0)
	{
		// This will just mean that the vehicle should die. 
		CTakeDamageInfo info(this, this, m_iHealth + 1000, DMG_BLAST);
		info.SetDamagePosition(WorldSpaceCenter());
		info.SetDamageForce(Vector(0, 0, 1));
		if (GetDriver() && GetDriver()->IsPlayer())
		{
			GetDriver()->TakeDamage(info);
		}
		TakeDamage(info);
	}

	if ((float)(m_iHealth / m_iMaxHealth) < fh) // health was originally lower %. Treat as "healing"
	{
		RemoveAllDecals(); // clear bullet holes, explosion marks, etc
	}

	// Again, don't do damage fx, same reason as in InputSetHealth.
	m_iHealth = (int)(m_iMaxHealth * fh);
	if (m_iHealth > m_iMaxHealth) m_iHealth = m_iMaxHealth;

	if (m_iHealth >= m_iMaxHealth)
	{
		m_OnFullHealth.FireOutput(this, this);
	}

	m_outputHealth_int.Set(m_iHealth, this, inputdata.pCaller);
}
#endif // DARKINTERVAL
void CPropAPC::InputDestroy( inputdata_t &inputdata )
{
	CTakeDamageInfo info( this, this, m_iHealth + 1000, DMG_BLAST );
	info.SetDamagePosition( WorldSpaceCenter() );
	info.SetDamageForce( Vector( 0, 0, 1 ) );
	if (GetDriver() && GetDriver()->IsPlayer())
	{
		GetDriver()->TakeDamage(info);
	}
	TakeDamage( info );
}

void CPropAPC::InputFireMissileAt( inputdata_t &inputdata )
{
	string_t strMissileTarget = MAKE_STRING( inputdata.value.String() );
	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, strMissileTarget, NULL, inputdata.pActivator, inputdata.pCaller );
	if ( pTarget == NULL )
	{
		DevWarning( "%s: Could not find target '%s'! The APC will fire at its current enemy instead!\n", GetClassname(), STRING( strMissileTarget ) );
		m_flRocketTime = CURTIME;
		FireNPCRocket();
		return;
	}

	m_flRocketTime = CURTIME;
	m_hSpecificRocketTarget = pTarget; 
	FireNPCRocket();
}

int CPropAPC::OnTakeDamage( const CTakeDamageInfo &inputInfo)
{
#ifdef DARKINTERVAL
	if (!GetDriver())
		return 0; // prevent players from blowing up spare APCs

	if (GetDriver() && GetDriver()->GetFlags() & FL_GODMODE)
		return 0; // prevent players with godmode from crashing the game by getting their APC destroyed
#endif
	if ( m_iHealth == 0 )
		return 0;
	
	CTakeDamageInfo info = inputInfo;
#ifdef DARKINTERVAL
	// HACKHACK: Scale up grenades until we get a better explosion/pressure damage system
	if (inputInfo.GetDamageType() & DMG_BLAST)
	{
		info.SetDamageForce(inputInfo.GetDamageForce() * 4);
	}

	VPhysicsTakeDamage(info);

	// reset the damage
	info.SetDamage(inputInfo.GetDamage());
#endif
	m_OnDamaged.FireOutput( info.GetAttacker(), this );
#ifdef DARKINTERVAL
	if (info.GetDamage() >= GetHealth())
	{
		inputdata_t input;
		input.pActivator = this; input.pCaller = this;
		InputLock(input); // prevent entering blown-up APCs
	}
#endif
	if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() )
	{
		m_OnDamagedByPlayer.FireOutput( info.GetAttacker(), this );
	}
	
	CTakeDamageInfo dmgInfo = info;
#ifdef DARKINTERVAL
	bool takesDamage = false;

	if (dmgInfo.GetInflictor()->ClassMatches("npc_cremator")) takesDamage = true;
	if (dmgInfo.GetAttacker()->ClassMatches("toxic_cloud"))	takesDamage = false;

	// allow squid acid to damage the vehicle slightly, but not the driver
	if (dmgInfo.GetInflictor()->ClassMatches("npc_squid")) takesDamage = true;
	if (dmgInfo.GetInflictor()->ClassMatches("npc_bullsquid")) takesDamage = true;
	if (dmgInfo.GetInflictor()->ClassMatches("monster_bullsquid")) takesDamage = true;
	if (dmgInfo.GetInflictor()->ClassMatches("monster_bullchicken")) takesDamage = true;
	if (dmgInfo.GetInflictor()->ClassMatches("shared_projectile")) takesDamage = true;

	if (dmgInfo.GetDamageType() & (DMG_BLAST | DMG_AIRBOAT)
		|| dmgInfo.GetDamageType() & DMG_ACID 
		|| dmgInfo.GetAmmoType() == GetAmmoDef()->Index("HelicopterGun")
		|| dmgInfo.GetAmmoType() == GetAmmoDef()->Index("SniperRound")
		|| dmgInfo.GetAmmoType() == GetAmmoDef()->Index("CombineCannon")
		|| dmgInfo.GetAmmoType() == GetAmmoDef()->Index("AR2")
		|| dmgInfo.GetAmmoType() == GetAmmoDef()->Index("OICW"))
	{
		takesDamage = true;
	}

	if (takesDamage)
	{
		int nPrevHealth = GetHealth();

		if (dmgInfo.GetAmmoType() == GetAmmoDef()->Index("SniperRound"))
		{
			dmgInfo.ScaleDamage(0.1f); // do little damage to vehicle, but bigger (still reduced) dmg to driver
		}

		if (dmgInfo.GetDamageType() & DMG_PLASMA) // cremator
		{
			dmgInfo.ScaleDamage(0.5f);
		}

		if (dmgInfo.GetDamageType() & DMG_ACID) // bullsquid
		{
			dmgInfo.ScaleDamage(2.0f); // it needs a raise, because it's too low otherwise
			dmgInfo.SetDamage(round(dmgInfo.GetDamage()));
		}

		m_iHealth -= dmgInfo.GetDamage();
		if ( m_iHealth <= 0 )
		{
			m_iHealth = 0;
			Event_Killed( dmgInfo );
			return 0;
		}

		//Check to do damage to driver
		if (GetDriver())
		{
			if (GetDriver()->IsPlayer())
			{
				if (dmgInfo.GetDamageType() & DMG_BURN && dmgInfo.GetInflictor()->IsNPC()) // don't stop damage from explosions
				{
					dmgInfo.SetDamage(0);
				}
				if (dmgInfo.GetDamageType() & DMG_PLASMA) // cremator
				{
					dmgInfo.SetDamage(0);
				}
				if (dmgInfo.GetDamageType() & DMG_NERVEGAS) // old polyp
				{
					dmgInfo.SetDamage(0);
				}
				if (dmgInfo.GetAmmoType() == GetAmmoDef()->Index("SniperRound"))
				{
					dmgInfo.ScaleDamage(0.5); // we previously scaled it to 10%, bring it back to 50%
				}
				if (dmgInfo.GetAmmoType() == GetAmmoDef()->Index("HelicopterGun"))
				{
					dmgInfo.ScaleDamage(0.05); // these types harm the vehicle, but not the driver.
				}
				if (dmgInfo.GetAmmoType() == GetAmmoDef()->Index("APCTurret"))
				{
					dmgInfo.ScaleDamage(0.03); // these types harm the vehicle, but not the driver.
				}
				if (dmgInfo.GetAmmoType() == GetAmmoDef()->Index("OICW"))
				{
					dmgInfo.ScaleDamage(0.1); // these types harm the vehicle, but not the driver.
				}
				if (dmgInfo.GetAmmoType() == GetAmmoDef()->Index("AR2"))
				{
					dmgInfo.ScaleDamage(0.03); // these types harm the vehicle, but not the driver.
				}
				if (dmgInfo.GetDamageType() & (DMG_BURN))
				{
					dmgInfo.ScaleDamage(0.3); // reduced damage to driver - most of it is absorbed by vehicle armor
				}
				if (dmgInfo.GetDamageType() & (DMG_BLAST))
				{
					dmgInfo.ScaleDamage(0.5); // reduced damage to driver - most of it is absorbed by vehicle armor
				}
				// these can't hurt us at all in the APC.
				if (dmgInfo.GetDamageType() & (DMG_ACID))
				{
					dmgInfo.ScaleDamage(0.0);
				}
				if (dmgInfo.GetDamageType() & (DMG_SLASH))
				{
					dmgInfo.ScaleDamage(0.0);
				}
				if (dmgInfo.GetDamageType() & (DMG_CRUSH))
				{
					dmgInfo.ScaleDamage(0.0);
				}

				// lastly, world impacts while driving should be much weaker in this vehicle
				if (dmgInfo.GetDamageType() & (DMG_VEHICLE))
				{
					dmgInfo.ScaleDamage(0.1);
				}

				dmgInfo.SetDamageType(info.GetDamageType() | DMG_VEHICLE);

				// Deal the damage to the passenger
				GetDriver()->TakeDamage(dmgInfo);

				// make player suit speak "Armour compromised"
				if (GetHealth() <= GetMaxHealth() * 0.66)
				{
					CBasePlayer *player = ToBasePlayer(GetDriver());
					player->SetSuitUpdate("!HEVVOXNEW_ARMORCOMPROMISED0", false, SUIT_NEXT_IN_5MIN);
				}
			}
		}

		m_auto_repair_goal_hull_amount_int = (int)(100 * (ceil((float)m_iHealth / 100)));
		SetContextThink(&CPropAPC::AutoRepairThink, CURTIME + sk_apc_auto_repair_combat_delay.GetFloat(), "AutoRepairContext");

		// Chain
//		BaseClass::OnTakeDamage( dmgInfo );

		// Spawn damage effects
		if ( nPrevHealth != GetHealth() )
		{
			int i = RandomInt(0, 3);
			Vector vecFx = WorldSpaceCenter();
			GetAttachment(i, vecFx, NULL);
			
			if ( ShouldTriggerDamageEffect( nPrevHealth, MAX_SMOKE_TRAILS, dmgInfo ) )
			{
				AddSmokeTrail(vecFx);
			}

			if ( ShouldTriggerDamageEffect( nPrevHealth, MAX_EXPLOSIONS, dmgInfo) )
			{
				ExplodeAndThrowChunk(vecFx);
			}
		}
	}
#else
	if ( dmgInfo.GetDamageType() & ( DMG_BLAST | DMG_AIRBOAT ) )
	{
		int nPrevHealth = GetHealth();

		m_iHealth -= dmgInfo.GetDamage();
		if ( m_iHealth <= 0 )
		{
			m_iHealth = 0;
			Event_Killed(dmgInfo);
			return 0;
		}

		// Chain
		//		BaseClass::OnTakeDamage( dmgInfo );

		// Spawn damage effects
		if ( nPrevHealth != GetHealth() )
		{
			if ( ShouldTriggerDamageEffect(nPrevHealth, MAX_SMOKE_TRAILS) )
			{
				AddSmokeTrail(dmgInfo.GetDamagePosition());
			}

			if ( ShouldTriggerDamageEffect(nPrevHealth, MAX_EXPLOSIONS) )
			{
				ExplodeAndThrowChunk(dmgInfo.GetDamagePosition());
			}
		}
	}
#endif // DARKINTERVAL
	return 1;
}

void CPropAPC::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData )
{
	BaseClass::ProcessMovement( pPlayer, pMoveData );

	if ( m_flDangerSoundTime > CURTIME )
		return;

	QAngle vehicleAngles = GetLocalAngles();
	Vector vecStart = GetAbsOrigin();
	Vector vecDir, vecRight;

	GetVectors(&vecDir, &vecRight, NULL);

	GetVectors( &vecDir, NULL, NULL );
#ifdef DARKINTERVAL // reworked danger sounds
	const float soundDuration = 0.25;
	float speed = m_VehiclePhysics.GetHLSpeed();
	
	// Make danger sounds ahead of the jeep
	if (fabs(speed) > 120)
	{
		Vector	vecSpot;

		float steering = m_VehiclePhysics.GetSteering();
		if (steering != 0)
		{
			if (speed > 0)
			{
				vecDir += vecRight * steering * 0.5;
			}
			else
			{
				vecDir -= vecRight * steering * 0.5;
			}
			VectorNormalize(vecDir);
		}
		const float radius = speed * 0.4;

		// 0.3 seconds ahead of the jeep
		vecSpot = vecStart + vecDir * (speed * 1.1f);
		CSoundEnt::InsertSound(SOUND_DANGER | SOUND_CONTEXT_PLAYER_VEHICLE, vecSpot, radius, soundDuration, this, 0);
		CSoundEnt::InsertSound(SOUND_PHYSICS_DANGER | SOUND_CONTEXT_PLAYER_VEHICLE, vecSpot, radius, soundDuration, this, 1);
	}

	// Make engine sounds even when we're not going fast.
	CSoundEnt::InsertSound(SOUND_PLAYER | SOUND_CONTEXT_PLAYER_VEHICLE, GetAbsOrigin(), 800, soundDuration, this, 0);
	m_flDangerSoundTime = CURTIME + 0.1f;
#else
	// Make danger sounds ahead of the APC
	trace_t	tr;
	Vector	vecSpot, vecLeftDir, vecRightDir;

	// lay down sound path
	vecSpot = vecStart + vecDir * 600;
	CSoundEnt::InsertSound(SOUND_DANGER, vecSpot, 400, 0.1, this);

	// put sounds a bit to left and right but slightly closer to APC to make a "cone" of sound 
	// in front of it
	QAngle leftAngles = vehicleAngles;
	leftAngles[ YAW ] += 20;
	VehicleAngleVectors(leftAngles, &vecLeftDir, NULL, NULL);
	vecSpot = vecStart + vecLeftDir * 400;
	UTIL_TraceLine(vecStart, vecSpot, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
	CSoundEnt::InsertSound(SOUND_DANGER, vecSpot, 400, 0.1, this);

	QAngle rightAngles = vehicleAngles;
	rightAngles[ YAW ] -= 20;
	VehicleAngleVectors(rightAngles, &vecRightDir, NULL, NULL);
	vecSpot = vecStart + vecRightDir * 400;
	UTIL_TraceLine(vecStart, vecSpot, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
	CSoundEnt::InsertSound(SOUND_DANGER, vecSpot, 400, 0.1, this);

	m_flDangerSoundTime = CURTIME + 0.3;
#endif // DARKINTERVAL
}

void CPropAPC::Think( void )
{
	BaseClass::Think();
#ifdef DARKINTERVAL
	SetSimulationTime(CURTIME);
#endif
	SetNextThink(CURTIME);
#ifdef DARKINTERVAL
	SetAnimatedEveryTick(true);
	
	if (GetDriver() && GetDriver()->IsNPC()) // optimisation
	{
		if (m_cabinStandbySprite) m_cabinStandbySprite->SetThink(&CBaseEntity::SUB_Remove);
		if (m_repairStateSprite) m_repairStateSprite->SetThink(&CBaseEntity::SUB_Remove);
		if (m_turretAmmoSprite) m_turretAmmoSprite->SetThink(&CBaseEntity::SUB_Remove);
	}
#endif
	if ( !m_bInitialHandbrake )	// after initial timer expires, set the handbrake
	{
		m_bInitialHandbrake = true;
		m_VehiclePhysics.SetHandbrake( true );
		m_VehiclePhysics.Think();
	}
#ifdef DARKINTERVAL
	// FIXME: Slam the crosshair every think -- if we don't do this it disappears randomly, never to return.
	if ((m_hPlayer.Get() != NULL && GetDriver() && GetDriver()->IsPlayer()) && !(m_bEnterAnimOn || m_bExitAnimOn))
	{
		m_hPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_VEHICLE_CROSSHAIR;
	}

	// Aim gun based on the player view direction.
	if (m_hPlayer && !m_bExitAnimOn && !m_bEnterAnimOn)
	{
		Vector vecEyeDir, vecEyePos;
		m_hPlayer->EyePositionAndVectors(&vecEyePos, &vecEyeDir, NULL, NULL);

		if (g_pGameRules->GetAutoAimMode() == AUTOAIM_ON_CONSOLE)
		{
			autoaim_params_t params;

			params.m_fScale = AUTOAIM_SCALE_DEFAULT * sv_vehicle_autoaim_scale.GetFloat();
			params.m_fMaxDist = autoaim_max_dist.GetFloat();
			m_hPlayer->GetAutoaimVector(params);

			// Use autoaim as the eye dir if there is an autoaim ent.
			vecEyeDir = params.m_vecAutoAimDir;
		}

		// Trace out from the player's eye point.
		Vector	vecEndPos = vecEyePos + (vecEyeDir * MAX_TRACE_LENGTH);
		trace_t	trace;
		UTIL_TraceLine(vecEyePos, vecEndPos, MASK_SHOT, this, COLLISION_GROUP_NONE, &trace);

		m_vecGunCrosshair = vecEndPos;

		// See if we hit something, if so, adjust end position to hit location.
		if (trace.fraction < 1.0)
		{
			vecEndPos = vecEyePos + (vecEyeDir * MAX_TRACE_LENGTH * trace.fraction);
		}

		AimPrimaryWeapon(vecEndPos);

		// Aim laser dot, attach to entity if it exists at the end of trace
		////
		if (m_hLaserDot && trace.fraction < 1.0)
		{
			Vector dotOrigin = vec3_origin;
			if (trace.endpos.IsValid()) dotOrigin = trace.endpos;
			m_hLaserDot->SetAbsOrigin(dotOrigin);
			EnableLaserDotAPC(m_hLaserDot, true);

			if (trace.m_pEnt && trace.m_pEnt->MyNPCPointer())
			{
				SetLaserDotAPCTarget(m_hLaserDot, trace.m_pEnt);

				m_flLaserTargetTime = CURTIME + 0.6f;

				if (trace.m_pEnt->IsAlive())
				{
			//		m_hTarget = tr.m_pEnt;

			//		m_flTargetSelectTime = CURTIME + 0.6f;
			//		m_iTargetType = HLSS_SelectTargetType(m_hTarget);
				}
			}
			else if (m_flLaserTargetTime < CURTIME)
			{
				SetLaserDotAPCTarget(m_hLaserDot, NULL);
			}
		}
		
		// Update the rocket target
		CreateAPCLaserDot();

	//	if (!DoesLaserDotHaveTarget(m_hLaserDot))
	//	{
	//		m_hLaserDot->SetAbsOrigin(vecEndPos);
	//	}

		if (m_hLaserDot)
		{
			EnableLaserDotAPC(m_hLaserDot, true);
			if( GetDriver() && GetDriver()->IsPlayer()) m_hLaserDot->SetOwnerEntity(GetDriver());
		}
		////
	}
#endif // DARKINTERVAL
	StudioFrameAdvance();
#ifdef DARKINTERVAL
	if (IsSequenceFinished() && (m_bExitAnimOn || m_bEnterAnimOn))
	{
		if (m_bEnterAnimOn)
		{
			m_VehiclePhysics.ReleaseHandbrake();
			StartEngine();

			// HACKHACK: This forces the jeep to play a sound when it gets entered underwater
			if (m_VehiclePhysics.IsEngineDisabled())
			{
				CBaseServerVehicle *pServerVehicle = dynamic_cast<CBaseServerVehicle *>(GetServerVehicle());
				if (pServerVehicle)
				{
					pServerVehicle->SoundStartDisabled();
				}
			}

			// The first few time we get into the jeep, print the jeep help
			if (m_iNumberOfEntries < hud_apchint_numentries.GetInt())
			{
				g_EventQueue.AddEvent(this, "ShowHudHint", 1.5f, this, this);
			}
		}

		if (hl2_episodic.GetBool())
		{
			// Set its running animation idle
			if (m_bEnterAnimOn)
			{
				// Idle running
				int nSequence = SelectWeightedSequence(ACT_IDLE_STIMULATED);
				if (nSequence > ACTIVITY_NOT_AVAILABLE)
				{
					SetCycle(0);
					m_flAnimTime = CURTIME;
					ResetSequence(nSequence);
					ResetClientsideFrame();
				}
			}
		}

		// Reset on exit anim
		GetServerVehicle()->HandleEntryExitFinish(m_bExitAnimOn, m_bExitAnimOn);
	}

#if 0
	// See if the driver door needs to close
	if ((m_flDoorCloseTime < CURTIME) && (GetSequence() == LookupSequence("door_open")))
	{
		m_flAnimTime = CURTIME;
		m_flPlaybackRate = 0.0;
		SetCycle(0);
		ResetSequence(LookupSequence("door_close"));
	}
	else if ((GetSequence() == LookupSequence("door_close")) && IsSequenceFinished())
	{
		m_flAnimTime = CURTIME;
		m_flPlaybackRate = 0.0;
		SetCycle(0);

		int nSequence = SelectWeightedSequence(ACT_IDLE);
		ResetSequence(nSequence);

		CPASAttenuationFilter sndFilter(this, "PropAPC.DoorClose");
		EmitSound(sndFilter, entindex(), "PropAPC.DoorClose");
	}
#endif 

	// DI NEW - door is now on a poseparameter
	float doorpos = GetPoseParameter(m_poseparam_door);
	if (GetDriver() && doorpos > 0 && (!GetDriver()->IsPlayer() || !m_bEnterAnimOn))
	{
		if (m_cabinStandbySprite) m_cabinStandbySprite->SetColor(0, 0, 0);
		SetPoseParameter(m_poseparam_door, doorpos - 0.1f);
	}
	if (!GetDriver() && doorpos < 1)
	{
		if (m_cabinStandbySprite) m_cabinStandbySprite->SetColor(25, 255, 75);
		SetPoseParameter(m_poseparam_door, doorpos + 0.1f);
	}	

	if (m_bEngineLocked) // gets locked during repair cycle
		SetPoseParameter(m_poseparam_handbrake, min(1, GetPoseParameter(m_poseparam_handbrake) + 0.05f)); // make it rotate instead of switching on/off
	else
		SetPoseParameter(m_poseparam_handbrake, max(0, GetPoseParameter(m_poseparam_handbrake) - 0.05f));
		
	// Display the thrusters panel if the vehicle has a driver
	SetBodygroup(BODYGROUP_HOLO_JUMP, int(GetDriver() != NULL));
	
	if (m_debugOverlays & OVERLAY_NPC_KILL_BIT)
	{
		CTakeDamageInfo info( this, this, m_iHealth, DMG_BLAST );
		info.SetDamagePosition( WorldSpaceCenter() );
		info.SetDamageForce( Vector( 0, 0, 1 ) );
		TakeDamage( info );
	}

	// See if the wheel dust should be on or off
	UpdateWheelFx();

	if (HasGun() && (m_nGunState == GUN_STATE_IDLE) && m_flRechargeTime < CURTIME )
	{
		RechargeAmmo();
	}

	if (!GetDriver()) m_nSkin = APC_SKIN_NOTCONTROLLED;
#else
	if ( IsSequenceFinished() )
	{
		int iSequence = SelectWeightedSequence(ACT_IDLE);
		if ( iSequence > ACTIVITY_NOT_AVAILABLE )
		{
			SetCycle(0);
			m_flAnimTime = CURTIME;
			ResetSequence(iSequence);
			ResetClientsideFrame();
		}
	}

	if ( m_debugOverlays & OVERLAY_NPC_KILL_BIT )
	{
		CTakeDamageInfo info(this, this, m_iHealth, DMG_BLAST);
		info.SetDamagePosition(WorldSpaceCenter());
		info.SetDamageForce(Vector(0, 0, 1));
		TakeDamage(info);
	}
#endif // DARKINTERVAL
}

//-----------------------------------------------------------------------------
// Purpose: This is used by NPC drivers.
//-----------------------------------------------------------------------------
#define SF_APCDRIVER_NO_ROCKET_ATTACK	1<<16
#define SF_APCDRIVER_NO_GUN_ATTACK		1<<17
void CPropAPC::AimSecondaryWeaponAt( CBaseEntity *pTarget )
{
#ifdef DARKINTERVAL
	if ((GetDriver()->IsPlayer()) == false )
	{
		return;
	}

	if ( !m_hRocketTarget ) // only replace the target if don't have one (from input)
#endif
	{
		m_hRocketTarget = pTarget;
	}

	// Update the rocket target
	CreateAPCLaserDot();

	if (m_hRocketTarget)
	{
		m_hLaserDot->SetAbsOrigin(m_hRocketTarget->BodyTarget(WorldSpaceCenter(), false));
	}
	SetLaserDotAPCTarget(m_hLaserDot, m_hRocketTarget);
	EnableLaserDotAPC(m_hLaserDot, m_hRocketTarget != NULL);
}
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
#define FIRING_DISCHARGE_RATE (1.0f / 1.0f)

void CPropAPC::UpdateGunState(CUserCmd *ucmd)
{
//	bool bStopRumble = false;

	m_accuracy_penalty_float -= gpGlobals->frametime;
	m_accuracy_penalty_float = clamp(m_accuracy_penalty_float, 0.0f, 5.0f);

//	char text[32];
//	V_snprintf(text, sizeof(text), "Accuracy Penalty is %.2f", m_accuracy_penalty_float);
//	NDebugOverlay::Text(m_vecBarrelPos, text, false, 0.1f);

	if (ucmd->buttons & IN_ATTACK)
	{
		if (m_nGunState == GUN_STATE_IDLE)
		{
			m_nGunState = GUN_STATE_FIRING;
		}

		if ( m_flMachineGunTime < CURTIME)
		{
			if (m_nAmmoCount > 0)
			{
				RemoveAmmo(FIRING_DISCHARGE_RATE);
				FireMachineGun();
			}
			else
			{
				if (m_NextDenyTurretSoundTime.Expired())
				{
					EmitSound("PropAPC.DenyTurret");
					m_NextDenyTurretSoundTime.Reset();
				}
			//	bStopRumble = true;
			}
		}
	}
	else
	{
		if (m_nGunState != GUN_STATE_IDLE)
		{
			if (m_nAmmoCount != 0)
			{
				EmitSound("PropAPC.StopFiringTurret");
			//	bStopRumble = true;
			}
			m_nGunState = GUN_STATE_IDLE;
		}
	}

	if (m_nRocketState == ROCKET_STATE_RELOAD && m_flRocketReloadTime < CURTIME)
	{
		m_nRocketState = ROCKET_STATE_READY;
	}
		
	if (m_hPlayer->m_nButtons & IN_ATTACK2)
	{
		if (m_nRocketState == ROCKET_STATE_RELOAD)
		{
			if (m_NextDenyRocketSoundTime.Expired())
			{
				if (m_nAmmo2Count == 0)
					EmitSound("PropAPC.DryFireRocket");
				else
					EmitSound("PropAPC.DenyRocket");
				m_NextDenyRocketSoundTime.Reset();
			}
		}
		else
		{
			Vector crossPos = vec3_origin;
			GetRocketShootPosition(&crossPos);
			if (m_nRocketState == ROCKET_STATE_READY)
			{
				m_flRocketTime = CURTIME;
				FirePlayerRocket();
			}

			m_flRocketAimingTime = 0; // reset timers
			m_flRocketStartAimTime = FLT_MAX;
		}
	}

	if (m_hPlayer->m_nButtons & IN_DUCK)
	{
		if (CURTIME >= m_flThrustersTime)
		{
			DoThrusters();
		}
	}
/*
	if (bStopRumble)
	{
		CBaseEntity *pDriver = GetDriver();
		CBasePlayer *pPlayerDriver;
		if (pDriver && pDriver->IsPlayer())
		{
			pPlayerDriver = dynamic_cast<CBasePlayer*>(pDriver);
			if (pPlayerDriver)
			{
				pPlayerDriver->RumbleEffect(RUMBLE_AIRBOAT_GUN, 0, RUMBLE_FLAG_STOP);
			}
		}
	}
*/
}

// Mass Effect Mako style thruster effect
void CPropAPC::DoThrusters(void)
{
	if (GetDriver())
	{
		m_flThrustersTime = CURTIME + 3.0f;
		EmitSound("PropAPC.Thrusters");
		if (!VPhysicsGetObject())
		{
			Assert(0);
			return;
		}
		Vector vThrusterUp, vThrusterFwd, vThrusterRt;
		GetVectors(&vThrusterFwd, &vThrusterRt, &vThrusterUp);

		if (GetDriver()->IsPlayer())
			ApplyAbsVelocityImpulse(vThrusterUp * 800 + vThrusterFwd * 100);
		else // NPC controlled APCs get a little helping spin and less amount of vertical boost
		{
			ApplyAbsVelocityImpulse(vThrusterUp * 400 + vThrusterFwd * 100 + vThrusterRt * 100);
			ApplyLocalAngularVelocityImpulse(AngularImpulse(RandomFloat(1000, 2000), 0, 0 ));
		}
	}
}

//-----------------------------------------------------------------------------
// Removes the ammo...
//-----------------------------------------------------------------------------
void CPropAPC::RemoveAmmo(float flAmmoAmount)
{
	m_flDrainRemainder += flAmmoAmount;
	int nAmmoToRemove = (int)m_flDrainRemainder;
	m_flDrainRemainder -= nAmmoToRemove;
	m_nAmmoCount -= nAmmoToRemove;
	if (m_nAmmoCount < 0)
	{
		m_nAmmoCount = 0;
		m_flDrainRemainder = 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Recharges the ammo...
//-----------------------------------------------------------------------------
void CPropAPC::RechargeAmmo(void)
{
	if (!m_bHasGun)
	{
		m_nAmmoCount = -1;
		return;
	}

	int nMaxAmmo = 100;
	if (m_nAmmoCount == nMaxAmmo)
		return;

	float flRechargeRate = 45;
	float flChargeAmount = flRechargeRate * gpGlobals->frametime;
	if (m_flDrainRemainder != 0.0f)
	{
		if (m_flDrainRemainder >= flChargeAmount)
		{
			m_flDrainRemainder -= flChargeAmount;
			return;
		}
		else
		{
			flChargeAmount -= m_flDrainRemainder;
			m_flDrainRemainder = 0.0f;
		}
	}

	m_flChargeRemainder += flChargeAmount;
	int nAmmoToAdd = (int)m_flChargeRemainder;
	m_flChargeRemainder -= nAmmoToAdd;
	m_nAmmoCount += nAmmoToAdd;
	if (m_nAmmoCount > nMaxAmmo)
	{
		m_nAmmoCount = nMaxAmmo;
		m_flChargeRemainder = 0.0f;
	}
}

void CPropAPC::SetupAPCInterface(void)
{
	// Most of these are going to be prototype for a while.

	// 1. Combine Mines highlight
	//	While driving, scan the area around the APC and draw markers on mine that are close enough
	/*
	CBaseEntity *pMine = NULL; 
	
	while ((pMine = gEntList.FindEntityByClassnameWithin(pMine, "combine_mine", GetAbsOrigin(), 2000)) != NULL)
	{
		if (pMine && pMine->MyNPCPointer())
		{
			pMine->MyNPCPointer()->StartPingEffect();
		}
	}
	*/
	// 2. Hull integrity warning, advice on repair stations and self-repair
	//int brightness = random->RandomInt(100, 255);
	//if (GetHealth() <= GetMaxHealth() * 0.66) // 500 with current values
	//{
	//	if (GetHealth() <= GetMaxHealth() * 0.33)
	//	{
	//		NDebugOverlay::ScreenText(0.4, 0.15, "...HULL INTEGRITY CRITICAL!!!...", 255, brightness, brightness, 255, 1.0);
	//		NDebugOverlay::ScreenText(0.4, 0.20, "AUTO-REPAIR PROTOCOL ON STANDBY", 255, brightness, brightness, 255, 1.0);
	//	}
	//	else
	//	{
	//		NDebugOverlay::ScreenText(0.4, 0.10, "...HULL INTEGRITY COMPROMISED...", 255, brightness, brightness, 200, 1.0);
	//		NDebugOverlay::ScreenText(0.4, 0.15, "...SEEK NEARBY REPAIR STATION...", 255, brightness, brightness, 200, 1.0);
	//		NDebugOverlay::ScreenText(0.4, 0.20, ".....OR REPLACEMENT VEHICLE.....", 255, brightness, brightness, 200, 1.0);
	//	}
	//}

	// 3. Reflect the turret ammo state on the side of the turret
	if (m_turretAmmoSprite && m_bHasGun)
	{
		if (m_nAmmoCount >= 0 && m_nAmmoCount <= 15)
			m_turretAmmoSprite->SetColor(255, 25, 25);
		else if (m_nAmmoCount > 15 && m_nAmmoCount < 50)
			m_turretAmmoSprite->SetColor(255, 150, 25);
		else
			m_turretAmmoSprite->SetColor(0, 0, 0);		
	}

	// 4. Set up poseparameters 
	// Headlight tumbler, thruster
	SetPoseParameter(m_poseparam_headlight, m_bHeadlightIsOn);

	if (m_flThrustersTime - 2.8f > CURTIME) // magic number - thrusters take 3 seconds to charge up, take away 0.2 to let the poseparamter drop down
		SetPoseParameter(m_poseparam_thruster, min(1, GetPoseParameter(m_poseparam_thruster) + 0.1f));
	else
		SetPoseParameter(m_poseparam_thruster, max(0, GetPoseParameter(m_poseparam_thruster) - 0.005f)); // magic number - need this for poseparameter to climb back to full in the 2.8 seconds left of thruster charge-up
	
	// 5. Blink with a warning panel if the armour is compromised
	// todo: blink it
	if (GetHealth() <= GetMaxHealth() * 0.66) // display hull warning if 500 hull or less
	{
		if (m_hullBlink_timer.Expired())
		{
			SetBodygroup(BODYGROUP_HOLO_HULL, !GetBodygroup(BODYGROUP_HOLO_HULL));
			m_hullBlink_timer.Set(0.5f);
		}
	}
	else
		SetBodygroup(BODYGROUP_HOLO_HULL, 0); // display hull warning if 500 hull or less

	// 6. Display rocket reload warning if it's not ready to fire yet
	SetBodygroup(BODYGROUP_HOLO_ROCKETS, int(m_nRocketState == ROCKET_STATE_RELOAD 
		&& CURTIME + 0.1f < m_flRocketReloadTime));

	// 4. Outlines for hostile APCs, vacant APCs
	// todo: make a not crappy implementation
	/*
	CBaseEntity *pEnt = NULL;
	while ((pEnt = gEntList.FindEntityInSphere(pEnt, GetLocalOrigin(), 4096.0f)) != NULL)
	{
		if (pEnt->m_takedamage != DAMAGE_NO)
		{
			CBaseCombatCharacter *pBCCp = ToBaseCombatCharacter(pEnt);

			if (pBCCp && pBCCp->IsAlive())
			{
				if (pEnt->m_iClassname == s_iszAPCDriver_Classname)
				{
					pBCCp->AddGlowEffect(0.875, 0.55, 0.05);
				}

				else if (pEnt->m_iClassname == s_iszHopper_Classname)
					pBCCp->AddGlowEffect(0.875, 0.55, 0.05);
			}
		}
	}
	*/
}

int CPropAPC::DrawDebugTextOverlays(int nOffset)
{
	char tempstr[512];
	Q_snprintf(tempstr, sizeof(tempstr), "Driver: %d", (GetDriver() ? GetDriver()->GetDebugName() : NULL)); // already done in base class (fourwheelvehiclephysics.cpp)
	EntityText(nOffset, tempstr, 0);
	nOffset++;

	return nOffset;
}
#endif // DARKINTERVAL
void CPropAPC::DriveVehicle( float flFrameTime, CUserCmd *ucmd, int iButtonsDown, int iButtonsReleased )
{
	switch( m_lifeState )
	{
	case LIFE_ALIVE:
#ifdef DARKINTERVAL // reworked and expanded controls
		{
			if (ucmd->impulse == 100)
			{
				if( m_bHeadlightIsOn )
					EmitSound("PropAPC.Headlight_off");
				else
					EmitSound("PropAPC.Headlight_on");
				m_bHeadlightIsOn ^= true;
			}

			if (GetDriver() && GetDriver()->IsPlayer())
			{
				SetupAPCInterface();
			}
			
			if (HasGun())
			{
				if (GetDriver() && GetDriver()->IsPlayer())
				{
					UpdateGunState(ucmd);
				}
				else
				{
					int iButtons = ucmd->buttons;

					if (m_flMachineGunTime < CURTIME)
					{
						if (iButtons & IN_ATTACK)
						{
							FireMachineGun();
						}
					}
					else if (iButtons & IN_ATTACK2)
					{
						FireNPCRocket();
					}
				}
			}
		}
#else // old controls
		{
			int iButtons = ucmd->buttons;
			if ( iButtons & IN_ATTACK )
			{
				FireMachineGun();
			} else if ( iButtons & IN_ATTACK2 )
			{
				FireRocket();
			}
		}
#endif // DARKINTERVAL
		break;

	case LIFE_DYING:
	{
#ifdef DARKINTERVAL // eject the driver, else a crash can happen
		if (GetDriver() && GetDriver()->IsPlayer())
		{
			inputdata_t input;
			input.pActivator = this; input.pCaller = this;
			ExitVehicle(VEHICLE_ROLE_DRIVER);
			InputLock(input);
		}
#endif
		FireDying();
	}
	break;

	case LIFE_DEAD:
		return;
	}

	BaseClass::DriveVehicle( flFrameTime, ucmd, iButtonsDown, iButtonsReleased );
}

void CPropAPC::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
#ifdef DARKINTERVAL
	CBasePlayer *pPlayer = ToBasePlayer(pActivator);

	if (pPlayer == NULL)
		return;

	// Find out if the player's looking at our ammocrate hitbox 
	Vector vecForward;
	pPlayer->EyeVectors(&vecForward, NULL, NULL);
#if 0
	trace_t tr;
	Vector vecStart = pPlayer->EyePosition();
	UTIL_TraceLine(vecStart, vecStart + vecForward * 1024, MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_HITBOX, pPlayer, COLLISION_GROUP_NONE, &tr);

//	if (tr.m_pEnt == this && tr.hitgroup == 1)
	{
		// Player's using the door. Play appropriate animation
		if ((GetSequence() != LookupSequence("door_open")))
		{
			// Open the crate
			m_flAnimTime = CURTIME;
			m_flPlaybackRate = 0.0;
			SetCycle(0);
			ResetSequence(LookupSequence("door_open"));

			CPASAttenuationFilter sndFilter(this, "PropAPC.DoorOpen");
			EmitSound(sndFilter, entindex(), "PropAPC.DoorOpen");
		}

		m_flDoorCloseTime = CURTIME + 1.0f;
		return;
	}
#endif
#endif // DARKINTERVAL
	// Fall back and get in the vehicle instead
	BaseClass::Use(pActivator, pCaller, useType, value);
#ifndef DARKINTERVAL // DI todo - we may want it for locked APCs in maps like CH04 WAREHOUSES?
	if ( pActivator->IsPlayer() )
	{
		EmitSound ("combine.door_lock");
	}
#endif
}
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: initialise the stuff for the cabin displays and repair stations
//-----------------------------------------------------------------------------
void CPropAPC::EnterVehicle(CBaseCombatCharacter *pPassenger)
{
	CBasePlayer *pPlayer = ToBasePlayer(pPassenger);
	if (!pPlayer)
		return;

	// pretty hacky, but it works - 
	// repair stations on the maps CH06 APCRIDE and CH06 OUTERWALL 
	// are keyed to a specific entity name, "apc_player". They could
	// be reworked to allow arbitrary names, but to save time, this solution
	// was used - ensuring the driven APC is "apc_player" and all others are not.
	// This fix exists here in the code and not just on the maps, because it
	// should work for APCs from previous levels as well as player-spawned ones
	// (through the console).
	CBaseEntity *pOtherAPCs = NULL;

	while ((pOtherAPCs = gEntList.FindEntityByClassname(pOtherAPCs, "prop_vehicle_apc")) != NULL)
	{
		if (pOtherAPCs && pOtherAPCs->NameMatches("apc_player"))
		{
			pOtherAPCs->SetName(NULL_STRING); // de-name any other APCs we may have been driving
		}
	}

	SetName(MAKE_STRING("apc_player"));

	// Play the engine start sound.
//	float flDuration;
//	EmitSound("PropAPC.StartEngine", 0.0, &flDuration);
	m_VehiclePhysics.TurnOn();

	// Start playing the engine's idle sound as the startup sound finishes.
//	m_flEngineIdleTime = CURTIME + flDuration - 0.1;

	// this is somewhat in conflict with OnTakeDamage deciding to delay self repair when taking damage,
	// but, if the player decides to hop out of the vehicle in the middle of combat then re-enter it,
	// yes, they will start the repair sooner, but it's almost certain that they'll get hit
	// in the vehicle again immediately, and then delay the self repair again. So it's okay.
	SetContextThink(&CPropAPC::AutoRepairThink, CURTIME + 1.0f, "AutoRepairContext"); 

	// Create and setup the sprites
	int iGunAttachment = LookupAttachment("gun_indicator");
	if (!m_turretAmmoSprite && iGunAttachment > 0) m_turretAmmoSprite = CSprite::SpriteCreate(APC_TURRET_SPRITE, GetAbsOrigin(), false);
	if (m_turretAmmoSprite != NULL) {
		m_turretAmmoSprite->FollowEntity(this);
		m_turretAmmoSprite->SetAttachment(this, iGunAttachment);
		m_turretAmmoSprite->SetTransparency(kRenderWorldGlow, 0, 0, 0, 255, kRenderFxNone);
		m_turretAmmoSprite->SetScale(0.025f);
	}

	int iPanelAttachment = LookupAttachment("cabin_sprite_right");
	if (!m_repairStateSprite && iPanelAttachment > 0) m_repairStateSprite = CSprite::SpriteCreate(APC_AUTOREPAIR_SPRITE, GetAbsOrigin(), false);
	if (m_repairStateSprite != NULL) {
		m_repairStateSprite->FollowEntity(this);
		m_repairStateSprite->SetAttachment(this, iPanelAttachment);
		m_repairStateSprite->SetTransparency(kRenderWorldGlow, 0, 0, 0, 255, kRenderFxStrobeSlow);
		m_repairStateSprite->SetScale(0.05f);
	}

	BaseClass::EnterVehicle(pPassenger);
}

//-----------------------------------------------------------------------------
// Purpose: Called when exiting, just before playing the exit animation.
//-----------------------------------------------------------------------------
void CPropAPC::PreExitVehicle(CBaseCombatCharacter *pPlayer, int nRole)
{
	// Stop shooting.
	m_nGunState = GUN_STATE_IDLE;

//	CBaseEntity *pDriver = GetDriver();
//	CBasePlayer *pPlayerDriver;
//	if (pDriver && pDriver->IsPlayer())
//	{
//		pPlayerDriver = dynamic_cast<CBasePlayer*>(pDriver);
	//	if (pPlayerDriver)
	//	{
	//		CBaseCombatWeapon *pWeapon = pPlayerDriver->GetActiveWeapon();
	//		if (pWeapon)
	//		{
	//			pWeapon->SetWeaponVisible(true);
	//		}
	//	}
	//	{
	//		pPlayerDriver->RumbleEffect(RUMBLE_AIRBOAT_GUN, 0, RUMBLE_FLAG_STOP);
	//	}
//	}

	if (m_hLaserDot)
	{
		UTIL_Remove(m_hLaserDot);
		m_hLaserDot = NULL;
	}
#if 0
	if ((GetSequence() != LookupSequence("door_open_close")))
	{
		// Open the crate
		m_flAnimTime = CURTIME;
		m_flPlaybackRate = 0.0;
		SetCycle(0);
		ResetSequence(LookupSequence("door_open_close"));
	}
#endif
	BaseClass::PreExitVehicle(pPlayer, nRole);
}

void CPropAPC::ExitVehicle(int nRole)
{
	if (m_bHeadlightIsOn)
	{
		EmitSound("PropAPC.Headlight_off"); 
		m_bHeadlightIsOn = false;
	}
	
	BaseClass::ExitVehicle(nRole);
}
#endif // DARKINTERVAL
void CPropAPC::AimPrimaryWeapon( const Vector &vecWorldTarget ) 
{
	EntityMatrix parentMatrix;
	parentMatrix.InitFromEntity( this, m_nMachineGunBaseAttachment );
	Vector target = parentMatrix.WorldToLocal( vecWorldTarget ); 

	float quadTarget = target.LengthSqr();
	float quadTargetXY = target.x*target.x + target.y*target.y;

	// Target is too close!  Can't aim at it
	if ( quadTarget > m_vecBarrelPos.LengthSqr() )
	{
		// We're trying to aim the offset barrel at an arbitrary point.
		// To calculate this, I think of the target as being on a sphere with 
		// it's center at the origin of the gun.
		// The rotation we need is the opposite of the rotation that moves the target 
		// along the surface of that sphere to intersect with the gun's shooting direction
		// To calculate that rotation, we simply calculate the intersection of the ray 
		// coming out of the barrel with the target sphere (that's the new target position)
		// and use atan2() to get angles

		// angles from target pos to center
		float targetToCenterYaw = atan2( target.y, target.x );
		float centerToGunYaw = atan2( m_vecBarrelPos.y, sqrt( quadTarget - (m_vecBarrelPos.y*m_vecBarrelPos.y) ) );

		float targetToCenterPitch = atan2( target.z, sqrt( quadTargetXY ) );
		float centerToGunPitch = atan2( -m_vecBarrelPos.z, sqrt( quadTarget - (m_vecBarrelPos.z*m_vecBarrelPos.z) ) );

		QAngle angles;
		angles.Init( -RAD2DEG(targetToCenterPitch+centerToGunPitch), RAD2DEG( targetToCenterYaw + centerToGunYaw ), 0 );

		SetPoseParameter(m_poseparam_gun_pitch, angles.x );
		SetPoseParameter(m_poseparam_gun_yaw, angles.y);
		StudioFrameAdvance();

		float curPitch = GetPoseParameter( m_poseparam_gun_pitch );
		float curYaw = GetPoseParameter( m_poseparam_gun_yaw );
		m_bInFiringCone = (fabs(curPitch - angles.x) < 1e-3) && (fabs(curYaw - angles.y) < 1e-3);
	}
	else
	{
		m_bInFiringCone = false;
	}
}
#ifndef DARKINTERVAL // replaced with a particle system, see ::FireMachineGun
const char *CPropAPC::GetTracerType(void)
{
	return "HelicopterTracer";
}
#endif
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: Show people how to drive!
//-----------------------------------------------------------------------------
void CPropAPC::InputShowHudHint(inputdata_t &inputdata)
{
	CBaseServerVehicle *pServerVehicle = dynamic_cast<CBaseServerVehicle *>(GetServerVehicle());
	if (pServerVehicle)
	{
		if (pServerVehicle->GetPassenger(VEHICLE_ROLE_DRIVER))
		{
			UTIL_HudHintText(m_hPlayer, "#hudhint_apc");
			m_iNumberOfEntries++;
		}
	}
}

void CPropAPC::MakeTracer(const Vector &vecTracerSrc, const trace_t &tr, int iTracerType)
{
	switch (iTracerType)
	{
	case TRACER_LINE:
	{
		float flTracerDist;
		Vector vecDir;
		Vector vecEndPos;

		vecDir = tr.endpos - vecTracerSrc;

		flTracerDist = VectorNormalize(vecDir);

		UTIL_Tracer(vecTracerSrc, tr.endpos, 0, TRACER_DONT_USE_ATTACHMENT, 15000, true, "TurretTracer");
	}
	break;

	default:
		BaseClass::MakeTracer(vecTracerSrc, tr, iTracerType);
		break;
	}
}
#endif // DARKINTERVAL
//-----------------------------------------------------------------------------
// Allows the shooter to change the impact effect of his bullets
//-----------------------------------------------------------------------------
void CPropAPC::DoImpactEffect( trace_t &tr, int nDamageType )
{
#ifdef DARKINTERVAL
	Vector vUp = tr.plane.normal;
	VectorRotate(vUp, QAngle(90, 0, 0), vUp);
	QAngle qUp;
	VectorAngles(vUp, qUp);
//	DispatchParticleEffect("impact_concrete", tr.endpos, qUp, this);
	UTIL_ImpactTrace( &tr, nDamageType, "HelicopterImpact" );
	BaseClass::DoImpactEffect(tr, DMG_BULLET);
#else
	UTIL_ImpactTrace(&tr, nDamageType, "HelicopterImpact");
#endif
} 


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAPC::DoMuzzleFlash( void )
{
	CEffectData data;
	data.m_nEntIndex = entindex();
	data.m_nAttachmentIndex = m_nMachineGunMuzzleAttachment;
	data.m_flScale = 1.0f;
	data.m_fFlags |= TRACER_LINE_AND_WHIZ;
#ifdef DARKINTERVAL
	DispatchEffect( "AirboatMuzzleFlash", data );
//	BaseClass::DoMuzzleFlash();
#else
	DispatchEffect("ChopperMuzzleFlash", data);
	BaseClass::DoMuzzleFlash();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAPC::FireMachineGun( void )
{
	if ( m_flMachineGunTime > CURTIME )
		return;
#ifdef DARKINTERVAL
	if (!GetDriver())
		return;

	// Add an accuracy penalty, but cap it
	m_accuracy_penalty_float += 0.3f;
	if (m_accuracy_penalty_float > 5) m_accuracy_penalty_float = 5.0f;
#endif
#ifdef DARKINTERVAL
	// If we're still firing the salvo, fire quickly
	if ((GetDriver()->IsPlayer()) == false)
#endif
	{
		if (m_flMachineGunTime > CURTIME) return; 

		m_iMachineGunBurstLeft--;
		if (m_iMachineGunBurstLeft > 0)
		{
			m_flMachineGunTime = CURTIME + MACHINE_GUN_BURST_TIME;
		}
		else
		{
			// Reload the salvo
#ifdef DARKINTERVAL
			m_iMachineGunBurstLeft = MACHINE_GUN_BURST_SIZE_NPC + RandomInt(-2, 2);
			m_flMachineGunTime = CURTIME + MACHINE_GUN_BURST_PAUSE_TIME_NPC;
#else
			m_iMachineGunBurstLeft = MACHINE_GUN_BURST_SIZE;
			m_flMachineGunTime = CURTIME + MACHINE_GUN_BURST_PAUSE_TIME;
#endif
		}
	}

	Vector vecMachineGunShootPos;
	Vector vecMachineGunDir;
	GetAttachment( m_nMachineGunMuzzleAttachment, vecMachineGunShootPos, &vecMachineGunDir );
#ifdef DARKINTERVAL
	// Fire the round
	int	bulletType = GetAmmoDef()->Index("APCTurret");
	if ( GetDriver()->IsPlayer())
	{
		EmitSound("PropAPC.FireTurret");
	}
	else
	{
		EmitSound("PropAPC.FireTurretNPC");
	}

	FireBulletsInfo_t fireInfo;
	fireInfo.m_iShots = 1;
	fireInfo.m_vecSrc = vecMachineGunShootPos;
	fireInfo.m_vecDirShooting = vecMachineGunDir;
	fireInfo.m_vecSpread = GetDriver()->IsPlayer() ? VECTOR_CONE_1DEGREES * (1 + m_accuracy_penalty_float) : VECTOR_CONE_4DEGREES;
	fireInfo.m_flDistance = MAX_TRACE_LENGTH;
	fireInfo.m_iAmmoType = bulletType;
	fireInfo.m_iTracerFreq = 2;
//	fireInfo.m_flDamage =  GetDriver()->IsPlayer() ? GetAmmoDef()->PlrDamage(bulletType) : GetAmmoDef()->NPCDamage(bulletType);
	fireInfo.m_pAttacker = GetDriver(); // necessary for plr/npc ammo damage values to work

	FireBullets(fireInfo);
//	FireBullets( 1, vecMachineGunShootPos, vecMachineGunDir, VECTOR_CONE_2DEGREES, MAX_TRACE_LENGTH, bulletType, 2 );

	QAngle qFwd;
	VectorAngles(vecMachineGunDir, qFwd);
	DispatchParticleEffect("apc_muzzleflash", vecMachineGunShootPos, qFwd, this);
//	DoMuzzleFlash();

	m_flMachineGunTime = CURTIME + MACHINE_GUN_BURST_TIME;

	m_flRechargeTime = (m_nAmmoCount > 0) ? CURTIME + MACHINE_GUN_BURST_PAUSE_TIME : CURTIME + MACHINE_GUN_BURST_PAUSE_TIME * 2.0;
#else
	// Fire the round
	int	bulletType = GetAmmoDef()->Index("AR2");
	FireBullets(1, vecMachineGunShootPos, vecMachineGunDir, VECTOR_CONE_8DEGREES, MAX_TRACE_LENGTH, bulletType, 1);
	DoMuzzleFlash();

	EmitSound("Weapon_AR2.Single");
#endif // DARKINTERVAL
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAPC::GetRocketShootPosition( Vector *pPosition )
{
	GetAttachment( m_nRocketAttachment, *pPosition );
}

//-----------------------------------------------------------------------------
// Create a corpse 
//-----------------------------------------------------------------------------
void CPropAPC::CreateCorpse( )
{
	m_lifeState = LIFE_DEAD;

	for ( int i = 0; i < APC_MAX_GIBS; ++i )
	{
		CPhysicsProp *pGib = assert_cast<CPhysicsProp*>(CreateEntityByName( "prop_physics" ));
		pGib->SetAbsOrigin( GetAbsOrigin() );
		pGib->SetAbsAngles( GetAbsAngles() );
		pGib->SetAbsVelocity( GetAbsVelocity() );
		pGib->SetModel( s_pGibModelName[i] );
		pGib->Spawn();
		pGib->SetMoveType( MOVETYPE_VPHYSICS );

		float flMass = pGib->GetMass();
		if ( flMass < 200 )
		{
			Vector vecVelocity;
			pGib->GetMassCenter( &vecVelocity );
			vecVelocity -= WorldSpaceCenter();
			vecVelocity.z = fabs(vecVelocity.z);
			VectorNormalize( vecVelocity );

			// Apply a force that would make a 100kg mass travel 150 - 300 m/s
			float flRandomVel = random->RandomFloat( 150, 300 );
			vecVelocity *= (100 * flRandomVel) / flMass;
			vecVelocity.z += 100.0f;
			AngularImpulse angImpulse = RandomAngularImpulse( -500, 500 );
			
			IPhysicsObject *pObj = pGib->VPhysicsGetObject();
			if ( pObj != NULL )
			{
				pObj->AddVelocity( &vecVelocity, &angImpulse );
			}
			pGib->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		}	
		if( hl2_episodic.GetBool() )
		{
			// EP1 perf hit
			pGib->Ignite( 6, false );
		}
		else
		{
			pGib->Ignite( 60, false );
		}
#ifdef DARKINTERVAL
		// start smoke coming out of the wreckage
		if (flMass > 500)
		{
			CParticleSystem *pParticle = (CParticleSystem *)CreateEntityByName("info_particle_system");
			if (pParticle != NULL)
			{
				// Setup our basic parameters
				pParticle->KeyValue("start_active", "1");
				pParticle->KeyValue("effect_name", "burning_engine_01");
				pParticle->SetParent(pGib);
				//	pParticle->SetParentAttachment("SetParentAttachment", "smoke_origin", true);
				pParticle->SetLocalOrigin(Vector(0, 0, 64));
				DispatchSpawn(pParticle);
				if (CURTIME > 0.5f)
					pParticle->Activate();

				//	pParticle->SetThink(&CBaseEntity::SUB_Remove);
				//	pParticle->SetNextThink(CURTIME + random->RandomFloat(30.0f, 50.0f));
			}
		}
#endif // DARKINTERVAL
	}

	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW );
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Death volley 
//-----------------------------------------------------------------------------
void CPropAPC::FireDying( )
{
#ifndef DARKINTERVAL // this is completely disabled in DI, not that it did much anyway
	if ( m_flRocketTime > CURTIME )
		return;

	Vector vecRocketOrigin;
	GetRocketShootPosition(	&vecRocketOrigin );

	Vector vecDir;
	vecDir.Random( -1.0f, 1.0f );
	if ( vecDir.z < 0.0f )
	{
		vecDir.z *= -1.0f;
	}

	VectorNormalize( vecDir );

	Vector vecVelocity;
	VectorMultiply( vecDir, ROCKET_SPEED * random->RandomFloat( 0.75f, 1.25f ), vecVelocity );

	QAngle angles;
	VectorAngles( vecDir, angles );

	CAPCMissile *pRocket = (CAPCMissile *) CAPCMissile::Create( vecRocketOrigin, angles, vecVelocity, this );
	float flDeathTime = random->RandomFloat( 0.3f, 0.5f );
	if ( random->RandomFloat( 0.0f, 1.0f ) < 0.3f )
	{
		pRocket->ExplodeDelay( flDeathTime );
	}
	else
	{
		pRocket->AugerDelay( flDeathTime );
	}

	// Make erratic firing
	m_flRocketTime = CURTIME + random->RandomFloat( DEATH_VOLLEY_MIN_FIRE_TIME, DEATH_VOLLEY_MAX_FIRE_TIME );
	if ( --m_iRocketSalvoLeft <= 0 )
	{
		CreateCorpse();
	}
#else
	// one important thing that it did was set the life state to dead so the APC driver gets removed	
	m_lifeState = LIFE_DEAD;
#endif // DARKINTERVAL
}
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: player version of the function. Shoot rocket from the front after
// aiming it (in ItemPostFrame).
//-----------------------------------------------------------------------------
void CPropAPC::FirePlayerRocket(void)
{
	if (!GetDriver() || (GetDriver()->IsPlayer()) == false) return;

	if (m_flRocketTime > CURTIME)
		return;

	m_flRocketTime = CURTIME + 1.0f;

	m_flRocketReloadTime = CURTIME + 2.0f;
	m_nRocketState = ROCKET_STATE_RELOAD;
	
	Vector vecRocketOrigin;
	GetRocketShootPosition(&vecRocketOrigin);
	
	Vector vecFwd, vecUp;
	GetVectors(&vecFwd, NULL, &vecUp);

	Vector vecDir = vecFwd * 2 + vecUp * 1;
	
	VectorNormalize(vecDir);

	Vector vecVelocity;
	VectorMultiply(vecDir, ROCKET_SPEED, vecVelocity);

	QAngle angles;
	VectorAngles(vecDir, angles);

	CBaseEntity *pOwner = this;
	if (GetDriver())
	{
		pOwner = GetDriver();
	}
	CAPCMissile *pRocket = (CAPCMissile *)CAPCMissile::Create(vecRocketOrigin, angles, vecVelocity, pOwner);
	pRocket->SetOwnerEntity(pOwner);
	pRocket->IgniteDelay();
	pRocket->EnableGuiding();
	if (m_hSpecificRocketTarget)
	{
		pRocket->AimAtSpecificTarget(m_hSpecificRocketTarget);
		m_hSpecificRocketTarget = NULL;
	}
	else if (m_strMissileHint != NULL_STRING)
	{
		pRocket->SetGuidanceHint(STRING(m_strMissileHint));
	}

	CPASAttenuationFilter filter(pRocket, 0.6f);
	EmitSound_t ep;
	ep.m_nChannel = CHAN_AUTO;
	ep.m_pSoundName = "PropAPC.FireRocket";
	ep.m_flVolume = 1.0f;
	ep.m_SoundLevel = SNDLVL_GUNFIRE;
	EmitSound(filter, GetDriver()->entindex(), ep);

	m_NextDenyRocketSoundTime.Set(1.0, false);
	m_OnFiredMissile.FireOutput(this, this);
}
#endif // DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: separate version to be used by NPC drivers.
// So far DI doesn't feature hostile APCs that shoot rockets. This is more
// for backwards compatibility with Half-Life 2 maps.
//-----------------------------------------------------------------------------
#ifdef DARKINTERVAL
void CPropAPC::FireNPCRocket( void )
#else
void CPropAPC::FireRocket(void)
#endif
{
//	Msg("Firing NPC rocket...\n");
	if (m_flRocketTime > CURTIME)
	{
	//	Msg("failed! m_flRocketTime > CURTIME!\n");
		return;
	}
#ifdef DARKINTERVAL
	CBaseEntity *pOwner = this;
	if (GetDriver())
	{
		pOwner = GetDriver();
	}

	if (!pOwner)
	{
	//	Msg("failed! no owner!\n");
		return;
	}

	if (pOwner->GetEnemy() == NULL && !m_hRocketTarget)
	{
	//	Msg("failed! no enemy nor rocket target!\n");
		return;
	}
#endif
	// If we're still firing the salvo, fire quickly
	m_iRocketSalvoLeft--;
	if ( m_iRocketSalvoLeft > 0 )
	{
		m_flRocketTime = CURTIME + ROCKET_DELAY_TIME;
	}
	else
	{
		// Reload the salvo
		m_iRocketSalvoLeft = ROCKET_SALVO_SIZE;
		m_flRocketTime = CURTIME + random->RandomFloat( ROCKET_MIN_BURST_PAUSE_TIME, ROCKET_MAX_BURST_PAUSE_TIME );
	}

	Vector vecRocketOrigin;
	GetRocketShootPosition(	&vecRocketOrigin );

	static float s_pSide[] = { 0.966, 0.866, 0.5, -0.5, -0.866, -0.966 };

	Vector forward;
	GetVectors( &forward, NULL, NULL );

	Vector vecDir;
	CrossProduct( Vector( 0, 0, 1 ), forward, vecDir );
	vecDir.z = 1.0f;
	vecDir.x *= s_pSide[m_nRocketSide];
	vecDir.y *= s_pSide[m_nRocketSide];
	if ( ++m_nRocketSide >= 6 )
	{
		m_nRocketSide = 0;
	}

	VectorNormalize( vecDir );

	Vector vecVelocity;
	VectorMultiply( vecDir, ROCKET_SPEED, vecVelocity );

	QAngle angles;
	VectorAngles( vecDir, angles );
	
#ifndef DARKINTERVAL
	CAPCMissileNPC *pRocket = (CAPCMissileNPC *)CAPCMissileNPC::Create( vecRocketOrigin, angles, vecVelocity, pOwner);
	pRocket->IgniteDelay();

	if ( m_hSpecificRocketTarget )
	{
		pRocket->AimAtSpecificTarget( m_hSpecificRocketTarget );
		m_hSpecificRocketTarget = NULL;
	}
	else if ( m_strMissileHint != NULL_STRING )
	{
		pRocket->SetGuidanceHint( STRING( m_strMissileHint ) );
	}
#else
	CGrenadeHomer *pGrenade = CGrenadeHomer::CreateGrenadeHomer(MAKE_STRING("models/weapons/w_missile_launch.mdl"), NULL_STRING, vecRocketOrigin, vec3_angle, edict());
	pGrenade->Spawn();
	//	pGrenade->SetSpin(npc_crabsynth_salvo_spin.GetFloat(), 
	//		npc_crabsynth_salvo_spin.GetFloat());
	pGrenade->SetHoming((0.01 * 40),
		0.1f, // Delay
		0.25f, // Ramp up
		1.0f, // Duration
		1.0f);// Ramp down
	pGrenade->SetDamage(50);
	pGrenade->SetDamageRadius(128);
	pGrenade->Launch(this,
		(m_hRocketTarget != NULL ? (CBaseEntity*)m_hRocketTarget : pOwner->GetEnemy()),
		vecVelocity,
		800,
		2.4,
		HOMER_SMOKE_TRAIL_ON); // owner is "this" and not pOwner because we don't want to collide with the VEHICLE

#endif // DARKINTERVAL

//	Msg("success!\n");
	EmitSound( "PropAPC.FireRocket" );
	m_OnFiredMissile.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CPropAPC::MaxAttackRange() const
{
	return ROCKET_ATTACK_RANGE_MAX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropAPC::OnRestore( void )
{
#ifdef DARKINTERVAL
	BaseClass::OnRestore();
#endif
	IServerVehicle *pServerVehicle = GetServerVehicle();
	if ( pServerVehicle != NULL )
	{
		// Restore the passenger information we're holding on to
		pServerVehicle->RestorePassengerInfo();
	}
#ifdef DARKINTERVAL
	s_iszAPC_Classname = AllocPooledString("prop_vehicle_apc");
	s_iszHopper_Classname = AllocPooledString("combine_mine");

	PopulatePoseParameters();
#endif
}

//========================================================================================================================================
// APC FOUR WHEEL PHYSICS VEHICLE SERVER VEHICLE
//========================================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAPCFourWheelServerVehicle::NPC_AimPrimaryWeapon( Vector vecTarget )
{
	CPropAPC *pAPC = ((CPropAPC*)m_pVehicle);
	pAPC->AimPrimaryWeapon( vecTarget );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAPCFourWheelServerVehicle::NPC_AimSecondaryWeapon( Vector vecTarget )
{
	// Add some random noise
//	Vector vecOffset = vecTarget + RandomVector( -128, 128 );
//	((CPropAPC*)m_pVehicle)->AimSecondaryWeaponAt( vecOffset );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAPCFourWheelServerVehicle::Weapon_PrimaryRanges( float *flMinRange, float *flMaxRange )
{
	*flMinRange = MACHINE_GUN_ATTACK_RANGE_MIN;
	*flMaxRange = MACHINE_GUN_ATTACK_RANGE_MAX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAPCFourWheelServerVehicle::Weapon_SecondaryRanges( float *flMinRange, float *flMaxRange )
{
	*flMinRange = ROCKET_ATTACK_RANGE_MIN;
	*flMaxRange = ROCKET_ATTACK_RANGE_MAX;
}

//-----------------------------------------------------------------------------
// Purpose: Return the time at which this vehicle's primary weapon can fire again
//-----------------------------------------------------------------------------
float CAPCFourWheelServerVehicle::Weapon_PrimaryCanFireAt( void )
{
	return ((CPropAPC*)m_pVehicle)->PrimaryWeaponFireTime();
}

//-----------------------------------------------------------------------------
// Purpose: Return the time at which this vehicle's secondary weapon can fire again
//-----------------------------------------------------------------------------
float CAPCFourWheelServerVehicle::Weapon_SecondaryCanFireAt( void )
{
	return ((CPropAPC*)m_pVehicle)->SecondaryWeaponFireTime();
}
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: When outside of combat, the vehicle can self-repair. It will 
// repair X amount of hull points to reach the next round amount, i. e.
// if it has 90 points, it'll repair up to 100, if it has 147, it'll 
// repair up to 200, etc.
// It won't initiate repair above N amount specified in a convar,
// this way we can make sure the player can self-repair at low HP values,
// but will also take advantage of repair stations.
// If it takes damage, it can't repair for the next
// X seconds, so the self-repair will only work outside of combat.
// (see also OnTakeDamage which bumps up the delay counter)
//-----------------------------------------------------------------------------
void CPropAPC::AutoRepairThink()
{
	bool shouldRepair = true;
	if (GetDriver() && GetDriver()->IsNPC() && sk_apc_auto_repair_enable_on_npcs.GetBool() == false)
		shouldRepair = false;
	if (GetHealth() >= sk_apc_auto_repair_max_amount.GetInt())
		shouldRepair = false;
	if (GetHealth() >= m_auto_repair_goal_hull_amount_int)
		shouldRepair = false;

	if( shouldRepair)
	{
		m_iHealth += sk_apc_auto_repair_increment.GetInt();
		SetNextThink(CURTIME + sk_apc_auto_repair_think_delay.GetFloat(), "AutoRepairContext");

		if (m_repairStateSprite ) m_repairStateSprite->SetColor(225, 750, 25);
	}
	else
	{
		if (m_repairStateSprite) m_repairStateSprite->SetColor(0, 0, 0);
		SetContextThink(NULL, CURTIME, "AutoRepairContext");
	}
}
#endif // DARKINTERVAL