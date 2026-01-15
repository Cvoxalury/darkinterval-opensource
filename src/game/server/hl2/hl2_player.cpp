//========================================================================//
//
// Purpose:		Player for HL2.
// DI changes that are not ifdef'd: 
// - removed all CS/TF/MP stuff
// DI todo - implement breather areas without water (vacuum, airless atmosphere)
//=============================================================================//

#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_criteria.h"
#include "ai_hull.h"
#include "ai_interactions.h"
#include "ai_squad.h"
#include "basehlcombatweapon_shared.h"
#include "collisionutils.h"
#include "coordsize.h"
#include "datacache/imdlcache.h"
#include "effect_color_tables.h"
#include "effect_dispatch_data.h"
#include "engine/ienginesound.h"
#include "entitylist.h"
#include "env_zoom.h"
#include "eventqueue.h"
#include "filters.h"
#include "game.h"
#include "gamerules.h"
#include "gamestats.h"
#include "globals.h"
#include "globalstate.h"
#include "hl2_gamerules.h"
#include "hl2_player.h"
#include "hl2_shareddefs.h"
#include "igamemovement.h"
#include "in_buttons.h"
#include "info_camera_link.h"
#include "iservervehicle.h"
#include "ivehicle.h"
#include "ndebugoverlay.h"
#ifdef HL2_EPISODIC
#include "npc_alyx_episodic.h"
#endif
#ifndef DARKINTERVAL
#include "physics_prop_ragdoll.h"
#endif
#include "player_pickup.h"
#include "point_camera.h"
#include "prop_combine_ball.h"
#include "script_intro.h"
#include "te_effect_dispatch.h" 
#include "tier0/icommandline.h"
#include "trains.h"
#include "vcollide_parse.h"
#include "vphysics/player_controller.h"
#include "weapon_physcannon.h"
#ifndef DARKINTERVAL
#include "world.h"
#endif
#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
#include "keyvalues.h"
#include "physicsshadowclone.h"
#include "predicted_viewmodel.h"
#include "prop_portal_shared.h"
#include "weapon_portalbase.h"
#include "Weapon_portalgun.h"
#endif
#else
#ifdef PORTAL_COMPILE
#include "portal_player.h"
#endif
#endif // DARKINTERVAL
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// extern ConVar weapon_showproficiency;
extern ConVar autoaim_max_dist;

// Do not touch with without seeing me, please! (sjb)
// For consistency's sake, enemy gunfire is traced against a scaled down
// version of the player's hull, not the hitboxes for the player's model
// because the player isn't aware of his model, and can't do anything about
// preventing headshots and other such things. Also, game difficulty will
// not change if the model changes. This is the value by which to scale
// the X/Y of the player's hull to get the volume to trace bullets against.
#define PLAYER_HULL_REDUCTION	0.70

// This switches between the single primary weapon, and multiple weapons with buckets approach (jdw)
#define	HL2_SINGLE_PRIMARY_WEAPON_MODE	0

#define TIME_IGNORE_FALL_DAMAGE 10.0

extern int gEvilImpulse101;

ConVar sv_autojump("sv_autojump", "0");
#ifdef DARKINTERVAL
ConVar hl2_slowspeed("hl2_walkspeed", "80");
ConVar hl2_walkspeed("hl2_walkspeed", "160");
ConVar hl2_normspeed("hl2_normspeed", "200");
ConVar hl2_sprintspeed("hl2_sprintspeed", "320");
ConVar always_run("always_run", "0");

#define	HL2_SLOW_SPEED hl2_slowspeed.GetFloat()
#define	HL2_WALK_SPEED hl2_walkspeed.GetFloat()
#define	HL2_NORM_SPEED hl2_normspeed.GetFloat()
#define	HL2_SPRINT_SPEED hl2_sprintspeed.GetFloat()
#define	HL2_SPRINT_SPEED_SUIT hl2_sprintspeed.GetFloat() + 40.0f
#else
ConVar hl2_walkspeed("hl2_walkspeed", "150");
ConVar hl2_normspeed("hl2_normspeed", "190");
ConVar hl2_sprintspeed("hl2_sprintspeed", "320");

#define	HL2_WALK_SPEED hl2_walkspeed.GetFloat()
#define	HL2_NORM_SPEED hl2_normspeed.GetFloat()
#define	HL2_SPRINT_SPEED hl2_sprintspeed.GetFloat()
#endif
#ifndef DARKINTERVAL // reducing amount of convars
ConVar hl2_darkness_flashlight_factor ("hl2_darkness_flashlight_factor", "1");
#endif

ConVar player_showpredictedposition("player_showpredictedposition", "0");
ConVar player_showpredictedposition_timestep("player_showpredictedposition_timestep", "1.0");

ConVar player_squad_transient_commands("player_squad_transient_commands", "1", FCVAR_REPLICATED);
ConVar player_squad_double_tap_time("player_squad_double_tap_time", "0.25");

ConVar sv_infinite_aux_power("sv_infinite_aux_power", "0", FCVAR_NONE);

ConVar autoaim_unlock_target("autoaim_unlock_target", "0.8666");

ConVar sv_stickysprint("sv_stickysprint", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX);
#ifdef DARKINTERVAL
ConVar snd_disable_tinnitus("snd_disable_tinnitus", "0", FCVAR_ARCHIVE, "If true, replace tinnitus DSP from explosions and sonic attacks with a muffled DSP");
#endif

#ifdef DARKINTERVAL
#define	FLASH_DRAIN_TIME	 2.2222	// 100 units / 45 secs
#define	FLASH_CHARGE_TIME	 50.0f	// 100 units / 2 secs
#else
#define	FLASH_DRAIN_TIME	 1.1111	// 100 units / 90 secs
#define	FLASH_CHARGE_TIME	 50.0f	// 100 units / 2 secs
#endif

#ifdef DARKINTERVAL // DI todo - this whole messy system needs to be replaced with calling the entities in viewcone and asking them to use glow, not applying them through the zoom!
static string_t s_iszOICW_Zoom_Citizen_Classname;
static string_t s_iszOICW_Zoom_Metrocop_Classname;
static string_t s_iszOICW_Zoom_Soldier_Classname;
static string_t s_iszOICW_Zoom_Houndeye_Classname;
static string_t s_iszOICW_Zoom_Squid_Classname;
static string_t s_iszOICW_Zoom_Zombie_Classname;
static string_t s_iszOICW_Zoom_FastZombie_Classname;
static string_t s_iszOICW_Zoom_PoisonZombie_Classname;
static string_t s_iszOICW_Zoom_ZombieWorker_Classname;
static string_t s_iszOICW_Zoom_Zombine_Classname;
static string_t s_iszOICW_Zoom_Headcrab_Classname;
static string_t s_iszOICW_Zoom_HeadcrabF_Classname;
static string_t s_iszOICW_Zoom_HeadcrabP_Classname;
static string_t s_iszOICW_Zoom_HeadcrabP2_Classname;
static string_t s_iszOICW_Zoom_AntlionSoldier_Classname;
static string_t s_iszOICW_Zoom_AntlionWorker_Classname;
static string_t s_iszOICW_Zoom_AntlionGuard_Classname;
static string_t s_iszOICW_Zoom_Barnacle_Classname;
static string_t s_iszOICW_Zoom_Vortigaunt_Classname;
static string_t s_iszOICW_Zoom_Tree_Classname;
static string_t s_iszOICW_Zoom_SynthScanner_Classname;
static string_t s_iszOICW_Zoom_ClawScanner_Classname;
static string_t s_iszOICW_Zoom_CityScanner_Classname;
static string_t s_iszOICW_Zoom_Cremator_Classname;

static const char *PLAYER_CONTEXT_BATTERY_PICKUP = "BatteryPickupContext";
#endif
//==============================================================================================
// CAPPED PLAYER PHYSICS DAMAGE TABLE
//==============================================================================================
static impactentry_t cappedPlayerLinearTable[] =
{
	{ 150 * 150, 5 },
	{ 250 * 250, 10 },
	{ 450 * 450, 20 },
	{ 550 * 550, 30 },
	//{ 700*700, 100 },
	//{ 1000*1000, 500 },
};

static impactentry_t cappedPlayerAngularTable[] =
{
	{ 100 * 100, 10 },
	{ 150 * 150, 20 },
	{ 200 * 200, 30 },
	//{ 300*300, 500 },
};

static impactdamagetable_t gCappedPlayerImpactDamageTable =
{
	cappedPlayerLinearTable,
	cappedPlayerAngularTable,

	ARRAYSIZE(cappedPlayerLinearTable),
	ARRAYSIZE(cappedPlayerAngularTable),

	24 * 24.0f,	// minimum linear speed
	360 * 360.0f,	// minimum angular speed
	2.0f,		// can't take damage from anything under 2kg

	5.0f,		// anything less than 5kg is "small"
	5.0f,		// never take more than 5 pts of damage from anything under 5kg
	36 * 36.0f,	// <5kg objects must go faster than 36 in/s to do damage

	0.0f,		// large mass in kg (no large mass effects)
	1.0f,		// large mass scale
	2.0f,		// large mass falling scale
	320.0f,		// min velocity for player speed to cause damage

};

// Flashlight utility
bool g_bCacheLegacyFlashlightStatus = true;
bool g_bUseLegacyFlashlight;
bool Flashlight_UseLegacyVersion(void)
{
	// If this is the first run through, cache off what the answer should be (cannot change during a session)
	if (g_bCacheLegacyFlashlightStatus)
	{
		char modDir[MAX_PATH];
		if (UTIL_GetModDir(modDir, sizeof(modDir)) == false)
			return false;
#ifdef DARKINTERVAL
		/*
		g_bUseLegacyFlashlight = (  !Q_strcmp( modDir, "hl2" ) ||
		!Q_strcmp( modDir, "episodic" ) ||
		!Q_strcmp( modDir, "lostcoast") ||
		!Q_strcmp(modDir, "darkinterval-beta" ) ||
		!Q_strcmp(modDir, "darkinterval-release") ||
		!Q_strcmp(modDir, "darkinterval-test") ||
		!Q_strcmp(modDir, "darkinterval-test2") ||
		!Q_strcmp(modDir, "darkinterval-wasteland-test") ||
		!Q_strcmp( modDir, "darkinterval" )); // DI change
		*/
		g_bUseLegacyFlashlight = true; //false;
#else
		if ( UTIL_GetModDir(modDir, sizeof(modDir)) == false )
			return false;

		g_bUseLegacyFlashlight = ( !Q_strcmp(modDir, "hl2") ||
			!Q_strcmp(modDir, "episodic") ||
			!Q_strcmp(modDir, "lostcoast") || !Q_strcmp(modDir, "hl1") );
#endif
		g_bCacheLegacyFlashlightStatus = false;
	}

	// Return the results
	return g_bUseLegacyFlashlight;
}

//-----------------------------------------------------------------------------
// Purpose: Used to relay outputs/inputs from the player to the world and viceversa
//-----------------------------------------------------------------------------
class CLogicPlayerProxy : public CLogicalEntity
{
	DECLARE_CLASS(CLogicPlayerProxy, CLogicalEntity);

private:

	DECLARE_DATADESC();

public:

	COutputEvent m_OnFlashlightOn;
	COutputEvent m_OnFlashlightOff;
	COutputEvent m_PlayerHasAmmo;
	COutputEvent m_PlayerHasNoAmmo;
	COutputEvent m_PlayerDied;
#ifdef DARKINTERVAL
	COutputEvent m_PlayerDamaged; // DI NEW - lost health to something. Output the attacker, if there is one
#endif
	COutputEvent m_PlayerMissedAR2AltFire; // Player fired a combine ball which did not dissolve any enemies. 

	COutputInt m_RequestedPlayerHealth;
#ifdef DARKINTERVAL
	COutputInt m_RequestedPlayerArmor;
#endif
	void InputRequestPlayerHealth(inputdata_t &inputdata);
	void InputSetFlashlightSlowDrain(inputdata_t &inputdata);
	void InputSetFlashlightNormalDrain(inputdata_t &inputdata);
	void InputSetPlayerHealth(inputdata_t &inputdata);
#ifdef DARKINTERVAL
	void InputSetPlayerArmor(inputdata_t &inputdata);
#endif
	void InputRequestAmmoState(inputdata_t &inputdata);
	void InputLowerWeapon(inputdata_t &inputdata);
	void InputEnableCappedPhysicsDamage(inputdata_t &inputdata);
	void InputDisableCappedPhysicsDamage(inputdata_t &inputdata);
	void InputSetLocatorTargetEntity(inputdata_t &inputdata);
#ifdef DARKINTERVAL
	void InputDisableFlashlight(inputdata_t &inputdata);
	void InputEnableFlashlight(inputdata_t &inputdata);
#endif
#ifdef DARKINTERVAL
	// DI NEW
	void InputSetFlashlightFastDrain(inputdata_t &inputdata);
	void InputSetFlashlightSuperFastDrain(inputdata_t &inputdata);
	void InputDisableFlashlightFlicker(inputdata_t &inputdata); // the inputs exist on the player, duplicated on proxy for convenience
	void InputEnableFlashlightFlicker(inputdata_t &inputdata);
	void InputDisableLocator(inputdata_t &inputdata);
	void InputEnableLocator(inputdata_t &inputdata);
	void InputEnableTinnitus(inputdata_t &inputdata);
	void InputDisableTinnitus(inputdata_t &inputdata);
	void InputRequestPlayerArmor(inputdata_t &inputdata);
	void InputEnableSuitComments(inputdata_t &inputdata);
	void InputDisableSuitComments(inputdata_t &inputdata);
	void InputDisableWeapons(inputdata_t &inputdata);
	void InputEnableWeapons(inputdata_t &inputdata);
	void InputDisableSuitlessHUD(inputdata_t &inputdata);
	void InputEnableSuitlessHUD(inputdata_t &inputdata);
	void InputAddBreather(inputdata_t &inputdata); // for non-underwater places without oxygen
	void InputRemoveBreather(inputdata_t &inputdata); // for non-underwater places without oxygen
	void InputBreakCurrentWeapon(inputdata_t &inputdata);
#endif
#ifdef PORTAL_COMPILE
	void InputSuppressCrosshair(inputdata_t &inputdata);
#endif // PORTAL2
	
	void Activate(void);

	bool PassesDamageFilter(const CTakeDamageInfo &info);

	EHANDLE m_hPlayer;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CC_ToggleZoom(void)
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();

	if (pPlayer)
	{
		CHL2_Player *pHL2Player = dynamic_cast<CHL2_Player*>(pPlayer);

		if (pHL2Player && pHL2Player->IsSuitEquipped())
		{
			pHL2Player->ToggleZoom();
		}
	}
}

static ConCommand toggle_zoom("toggle_zoom", CC_ToggleZoom, "Toggles zoom display");

// ConVar cl_forwardspeed( "cl_forwardspeed", "400", FCVAR_NONE ); // Links us to the client's version
ConVar xc_crouch_range("xc_crouch_range", "0.85", FCVAR_ARCHIVE, "Percentarge [1..0] of joystick range to allow ducking within");	// Only 1/2 of the range is used
ConVar xc_use_crouch_limiter("xc_use_crouch_limiter", "0", FCVAR_ARCHIVE, "Use the crouch limiting logic on the controller");

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CC_ToggleDuck(void)
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if (pPlayer == NULL)
		return;

	// Cannot be frozen
	if (pPlayer->GetFlags() & FL_FROZEN)
		return;

	static bool		bChecked = false;
	static ConVar *pCVcl_forwardspeed = NULL;
	if (!bChecked)
	{
		bChecked = true;
		pCVcl_forwardspeed = (ConVar *)cvar->FindVar("cl_forwardspeed");
	}


	// If we're not ducked, do extra checking
	if (xc_use_crouch_limiter.GetBool())
	{
		if (pPlayer->GetToggledDuckState() == false)
		{
			float flForwardSpeed = 400.0f;
			if (pCVcl_forwardspeed)
			{
				flForwardSpeed = pCVcl_forwardspeed->GetFloat();
			}

			flForwardSpeed = MAX(1.0f, flForwardSpeed);

			// Make sure we're not in the blindspot on the crouch detection
			float flStickDistPerc = (pPlayer->GetStickDist() / flForwardSpeed); // Speed is the magnitude
			if (flStickDistPerc > xc_crouch_range.GetFloat())
				return;
		}
	}

	// Toggle the duck
	pPlayer->ToggleDuck();
}

static ConCommand toggle_duck("toggle_duck", CC_ToggleDuck, "Toggles duck");

LINK_ENTITY_TO_CLASS(player, CHL2_Player);

PRECACHE_REGISTER(player);

CBaseEntity *FindEntityForward(CBasePlayer *pMe, bool fHull);

BEGIN_SIMPLE_DATADESC(LadderMove_t)
	DEFINE_FIELD(m_bForceLadderMove, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bForceMount, FIELD_BOOLEAN),
	DEFINE_FIELD(m_flStartTime, FIELD_TIME),
	DEFINE_FIELD(m_flArrivalTime, FIELD_TIME),
	DEFINE_FIELD(m_vecGoalPosition, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(m_vecStartPosition, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(m_hForceLadder, FIELD_EHANDLE),
	DEFINE_FIELD(m_hReservedSpot, FIELD_EHANDLE),
END_DATADESC()

// Global Savedata for HL2 player
BEGIN_DATADESC(CHL2_Player)

	DEFINE_FIELD(m_nControlClass, FIELD_INTEGER),
	DEFINE_EMBEDDED(m_HL2Local),

	DEFINE_FIELD(m_bSprintEnabled, FIELD_BOOLEAN),
	DEFINE_FIELD(m_flTimeAllSuitDevicesOff, FIELD_TIME),
	DEFINE_FIELD(m_fIsSprinting, FIELD_BOOLEAN),
	DEFINE_FIELD(m_fIsWalking, FIELD_BOOLEAN),

	/*
	// These are initialized every time the player calls Activate()
	DEFINE_FIELD( m_bIsAutoSprinting, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fAutoSprintMinTime, FIELD_TIME ),
	*/

	// 	Field is used within a single tick, no need to save restore
	// DEFINE_FIELD( m_bPlayUseDenySound, FIELD_BOOLEAN ),  
	//							m_pPlayerAISquad reacquired on load

	DEFINE_AUTO_ARRAY(m_vecMissPositions, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(m_nNumMissPositions, FIELD_INTEGER),

	//					m_pPlayerAISquad
	DEFINE_EMBEDDED(m_CommanderUpdateTimer),
	//					m_RealTimeLastSquadCommand
	DEFINE_FIELD(m_QueuedCommand, FIELD_INTEGER),

	DEFINE_FIELD(m_flTimeIgnoreFallDamage, FIELD_TIME),
	DEFINE_FIELD(m_bIgnoreFallDamageResetAfterImpact, FIELD_BOOLEAN),

	// Suit power fields
	DEFINE_FIELD(m_flSuitPowerLoad, FIELD_FLOAT),

	DEFINE_FIELD(m_flIdleTime, FIELD_TIME),
	DEFINE_FIELD(m_flMoveTime, FIELD_TIME),
	DEFINE_FIELD(m_flLastDamageTime, FIELD_TIME),
	DEFINE_FIELD(m_flTargetFindTime, FIELD_TIME),

	DEFINE_FIELD(m_flAdmireGlovesAnimTime, FIELD_TIME),
	DEFINE_FIELD(m_flNextFlashlightCheckTime, FIELD_TIME),
	DEFINE_FIELD(m_flFlashlightPowerDrainScale, FIELD_FLOAT),
	DEFINE_FIELD(m_flSprintPowerDrainScale, FIELD_FLOAT),
#ifdef DARKINTERVAL
	DEFINE_FIELD(m_bFlashlightDisabled, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bSuitCommentsDisabled, FIELD_BOOLEAN),
	DEFINE_FIELD(m_flLastBatteryTime, FIELD_FLOAT),

	DEFINE_THINKFUNC(ContextThink_BatteryPickup),
#endif
	DEFINE_FIELD(m_bUseCappedPhysicsDamageTable, FIELD_BOOLEAN),

	DEFINE_FIELD(m_hLockedAutoAimEntity, FIELD_EHANDLE),

	DEFINE_EMBEDDED(m_LowerWeaponTimer),
	DEFINE_EMBEDDED(m_AutoaimTimer),

	DEFINE_INPUTFUNC(FIELD_FLOAT, "IgnoreFallDamage", InputIgnoreFallDamage),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "IgnoreFallDamageWithoutReset", InputIgnoreFallDamageWithoutReset),
	DEFINE_INPUTFUNC(FIELD_VOID, "OnSquadMemberKilled", OnSquadMemberKilled),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableFlashlight", InputDisableFlashlight),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableFlashlight", InputEnableFlashlight),
#ifdef DARKINTERVAL
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableFlashlightFlicker", InputEnableFlashlightFlicker),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableFlashlightFlicker", InputDisableFlashlightFlicker),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableLocator", InputDisableLocator),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableLocator", InputEnableLocator),
#endif
	DEFINE_INPUTFUNC(FIELD_VOID, "ForceDropPhysObjects", InputForceDropPhysObjects),

	DEFINE_SOUNDPATCH(m_sndLeeches),
	DEFINE_SOUNDPATCH(m_sndWaterSplashes),
	DEFINE_FIELD(m_flArmorReductionTime, FIELD_TIME),
	DEFINE_FIELD(m_iArmorReductionFrom, FIELD_INTEGER),
	DEFINE_FIELD(m_flTimeUseSuspended, FIELD_TIME),
	DEFINE_FIELD(m_hLocatorTargetEntity, FIELD_EHANDLE),
	DEFINE_FIELD(m_flTimeNextLadderHint, FIELD_TIME),
#ifdef DARKINTERVAL
	DEFINE_FIELD(m_flNextLocatorUpdateTime, FIELD_TIME),
#endif
	//DEFINE_FIELD( m_hPlayerProxy, FIELD_EHANDLE ), //Shut up class check!
#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
	DEFINE_FIELD(m_bHeldObjectOnOppositeSideOfPortal, FIELD_BOOLEAN),
	DEFINE_FIELD(m_pHeldObjectPortal, FIELD_EHANDLE),
	DEFINE_FIELD(m_bIntersectingPortalPlane, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bStuckOnPortalCollisionObject, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bPitchReorientation, FIELD_BOOLEAN),
	DEFINE_FIELD(m_hPortalEnvironment, FIELD_EHANDLE),
	DEFINE_FIELD(m_qPrePortalledViewAngles, FIELD_VECTOR),
	DEFINE_FIELD(m_angEyeAngles, FIELD_VECTOR),
	DEFINE_FIELD(m_bFixEyeAnglesFromPortalling, FIELD_BOOLEAN),
	DEFINE_FIELD(m_matLastPortalled, FIELD_VMATRIX_WORLDSPACE),
	DEFINE_FIELD(m_vWorldSpaceCenterHolder, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(m_hSurroundingLiquidPortal, FIELD_EHANDLE),
#endif
#endif // DARKINTERVAL
END_DATADESC()
#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
//----------------------------------------------------
// Player Physics Shadow
//----------------------------------------------------
#define VPHYS_MAX_DISTANCE		2.0
#define VPHYS_MAX_VEL			10
#define VPHYS_MAX_DISTSQR		(VPHYS_MAX_DISTANCE*VPHYS_MAX_DISTANCE)
#define VPHYS_MAX_VELSQR		(VPHYS_MAX_VEL*VPHYS_MAX_VEL)
extern float IntervalDistance(float x, float x0, float x1);
#endif
#endif // DARKINTERVAL
CHL2_Player::CHL2_Player()
{
	m_nNumMissPositions = 0;
	m_pPlayerAISquad = 0;
	m_bSprintEnabled = true;

	m_flArmorReductionTime = 0.0f;
	m_iArmorReductionFrom = 0;
#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
	m_angEyeAngles.Init();
	m_iLastWeaponFireUsercmd = 0;
	m_iSpawnInterpCounter = 0;
	m_bHeldObjectOnOppositeSideOfPortal = false;
	m_pHeldObjectPortal = 0;
	m_bIntersectingPortalPlane = false;
	m_bPitchReorientation = false;
#endif
#endif // DARKINTERVAL
}

//
// SUIT POWER DEVICES
//
#define SUITPOWER_CHARGE_RATE	12.5												// 100 units in 8 seconds

#ifdef DARKINTERVAL
#if 0
ConVar sk_aux_sprint_reserve("sk_aux_sprint_reserve", "8.0");
ConVar sk_aux_flashlight_reserve("sk_aux_flashlight_reserve", "45.0");
ConVar sk_aux_oxygen_reserve("sk_aux_oxygen_reserve", "25.0");

CSuitPowerDevice SuitDeviceSprint(bits_SUIT_DEVICE_SPRINT, 100 / sk_aux_sprint_reserve.GetFloat());				// 100 units in 8 seconds

CSuitPowerDevice SuitDeviceFlashlight(bits_SUIT_DEVICE_FLASHLIGHT, 100 / sk_aux_flashlight_reserve.GetFloat());

CSuitPowerDevice SuitDeviceBreather(bits_SUIT_DEVICE_BREATHER, 100 / sk_aux_oxygen_reserve.GetFloat());				// 100 units in 20 seconds
#else
CSuitPowerDevice SuitDeviceSprint(bits_SUIT_DEVICE_SPRINT, 12.5f);				// 100 units in 8 seconds

CSuitPowerDevice SuitDeviceFlashlight(bits_SUIT_DEVICE_FLASHLIGHT, 2.222f);

CSuitPowerDevice SuitDeviceBreather(bits_SUIT_DEVICE_BREATHER, 4.0f);				// 100 units in 25 seconds
#endif
#else // DARKINTERVAL
#define SUITPOWER_CHARGE_RATE	12.5											// 100 units in 8 seconds
CSuitPowerDevice SuitDeviceSprint(bits_SUIT_DEVICE_SPRINT, 12.5f);				// 100 units in 8 seconds
#ifdef HL2_EPISODIC
CSuitPowerDevice SuitDeviceFlashlight(bits_SUIT_DEVICE_FLASHLIGHT, 1.111);	// 100 units in 90 second
#else
CSuitPowerDevice SuitDeviceFlashlight(bits_SUIT_DEVICE_FLASHLIGHT, 2.222);	// 100 units in 45 second
#endif
CSuitPowerDevice SuitDeviceBreather(bits_SUIT_DEVICE_BREATHER, 6.7f);		// 100 units in 15 seconds (plus three padded seconds)
#endif // DARKINTERVAL

IMPLEMENT_SERVERCLASS_ST(CHL2_Player, DT_HL2_Player)
SendPropDataTable(SENDINFO_DT(m_HL2Local), &REFERENCE_SEND_TABLE(DT_HL2Local), SendProxy_SendLocalDataTable),
SendPropBool(SENDINFO(m_fIsSprinting)),

#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
SendPropAngle(SENDINFO_VECTORELEM(m_angEyeAngles, 0), 11, SPROP_CHANGES_OFTEN),
SendPropAngle(SENDINFO_VECTORELEM(m_angEyeAngles, 1), 11, SPROP_CHANGES_OFTEN),
SendPropBool(SENDINFO(m_bHeldObjectOnOppositeSideOfPortal)),
SendPropEHandle(SENDINFO(m_pHeldObjectPortal)),
SendPropBool(SENDINFO(m_bPitchReorientation)),
SendPropEHandle(SENDINFO(m_hPortalEnvironment)),
SendPropEHandle(SENDINFO(m_hSurroundingLiquidPortal)),
#endif
#endif // DARKINTERVAL
END_SEND_TABLE()

#ifdef PORTAL_COMPILE
void CHL2_Player::NotifySystemEvent(CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params)
{
	// On teleport, we send event for tracking fling achievements
	if (eventType == NOTIFY_EVENT_TELEPORT)
	{
		CProp_Portal *pEnteredPortal = dynamic_cast<CProp_Portal*>(pNotify);
		IGameEvent *event = gameeventmanager->CreateEvent("portal_player_portaled");
		if (event)
		{
			event->SetInt("userid", GetUserID());
			event->SetBool("portal2", pEnteredPortal->m_bIsPortal2);
			gameeventmanager->FireEvent(event);
		}
	}

	BaseClass::NotifySystemEvent(pNotify, eventType, params);
}
#endif
void CHL2_Player::Precache(void)
{
	BaseClass::Precache();

	PrecacheScriptSound("HL2Player.SprintNoPower");
	PrecacheScriptSound("HL2Player.SprintStart");
	PrecacheScriptSound("HL2Player.UseDeny");
	PrecacheScriptSound("HL2Player.FlashLightOn");
	PrecacheScriptSound("HL2Player.FlashLightOff");
	PrecacheScriptSound("HL2Player.PickupWeapon");
	PrecacheScriptSound("HL2Player.TrainUse");
	PrecacheScriptSound("HL2Player.Use");
	PrecacheScriptSound("HL2Player.BurnPain");
#ifdef DARKINTERVAL
	PrecacheScriptSound("Locator.NewContactBeep");
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_Player::CheckSuitZoom(void)
{
#ifdef DARKINTERVAL
	if (true /*IsSuitEquipped() && ( Q_strncmp( gpGlobals->mapname.ToCStr(), "ch02", 4) != 0)*/)
#else
	if ( IsSuitEquipped() )
#endif
	{
		if (m_afButtonReleased & IN_ZOOM)
		{
			StopZooming();
		}
		else if (m_afButtonPressed & IN_ZOOM)
		{
			StartZooming(GetZoomTypeFromWeapon());
		}
	}
}

void CHL2_Player::EquipSuit(bool bPlayEffects)
{
	MDLCACHE_CRITICAL_SECTION();
	BaseClass::EquipSuit();

	m_HL2Local.m_bDisplayReticle = true;

	if (bPlayEffects == true)
	{
		StartAdmireGlovesAnimation();
	}
#ifdef DARKINTERVAL
	SetFlashlightPowerDrainScale(1.0f);
//	SuitPower_RemoveDevice(SuitDeviceFlashlight); // Remove so it doesn't show up on HUD,
	//SetSprintPowerDrainScale(10000.0f);
	SuitPower_RemoveDevice(SuitDeviceSprint);	  // because these are infinite with the HEV
#endif
}

void CHL2_Player::RemoveSuit(void)
{
	BaseClass::RemoveSuit();

	m_HL2Local.m_bDisplayReticle = false;
}

void CHL2_Player::HandleSpeedChanges(void)
{
	int buttonsChanged = m_afButtonPressed | m_afButtonReleased;

	bool bCanSprint = CanSprint();
	bool bIsSprinting = IsSprinting();
#ifdef DARKINTERVAL
	bool bWantSprint = (bCanSprint && (m_nButtons & IN_SPEED)); // DI change - sprint now allowed for all
#else
	bool bWantSprint = ( bCanSprint && IsSuitEquipped() && ( m_nButtons & IN_SPEED ) );
#endif
#ifdef DARKINTERVAL
	if (ToggleSprintIsOn())
	{
		if (bCanSprint)
		{
			if ((m_nButtons & IN_SPEED) == false)
			{
				if( !bIsSprinting) StartSprinting();
			}
			else
			{
				if( bIsSprinting )StopSprinting();
			}
		}
	}
	else
#endif
	{
#ifdef DARKINTERVAL
		if ( IsSprinting() && (m_nButtons & IN_SPEED) == false)
		{
			StopSprinting();
		}
#endif
		if (bIsSprinting != bWantSprint && (buttonsChanged & IN_SPEED))
		{
			// If someone wants to sprint, make sure they've pressed the button to do so. We want to prevent the
			// case where a player can hold down the sprint key and burn tiny bursts of sprint as the suit recharges
			// We want a full debounce of the key to resume sprinting after the suit is completely drained
			if (bWantSprint)
			{
				if (sv_stickysprint.GetBool())
				{
					StartAutoSprint();
				}
				else
				{
					StartSprinting();
				}
			}
			else
			{
				if (!sv_stickysprint.GetBool())
				{
					StopSprinting();
				}
				// Reset key, so it will be activated post whatever is suppressing it.
				m_nButtons &= ~IN_SPEED;
			}
		}
	}

	bool bIsWalking = IsWalking();
	// have suit, pressing button, not sprinting or ducking
	bool bWantWalking;
#ifdef DARKINTERVAL
	bWantWalking = (m_nButtons & IN_WALK) && !IsSprinting() && !(m_nButtons & IN_DUCK);
#else
	if ( IsSuitEquipped() )
	{
		bWantWalking = ( m_nButtons & IN_WALK ) && !IsSprinting() && !( m_nButtons & IN_DUCK );
	} else
	{
		bWantWalking = true;
	}
#endif
	if (bIsWalking != bWantWalking)
	{
		if (bWantWalking)
		{
			StartWalking();
		}
		else
		{
			StopWalking();
		}
	}
#ifdef DARKINTERVAL
	if (m_Local.m_bParalyzed)
	{
		SetMaxSpeed(HL2_NORM_SPEED * 0.5);
	}
#endif
}

//-----------------------------------------------------------------------------
// This happens when we powerdown from the mega physcannon to the regular one
//-----------------------------------------------------------------------------
void CHL2_Player::HandleArmorReduction(void)
{
	if (m_flArmorReductionTime < CURTIME)
		return;

	if (ArmorValue() <= 0)
		return;

	float flPercent = 1.0f - ((m_flArmorReductionTime - CURTIME) / ARMOR_DECAY_TIME);

	int iArmor = Lerp(flPercent, m_iArmorReductionFrom, 0);

	SetArmorValue(iArmor);
}

//-----------------------------------------------------------------------------
// Purpose: Allow pre-frame adjustments on the player
//-----------------------------------------------------------------------------
void CHL2_Player::PreThink(void)
{
#ifdef DARKINTERVAL
	if (GlobalEntity_GetState("playing_arcade") == GLOBAL_ON)
		m_Local.m_bPlayingArcade = true;
	else if (GlobalEntity_GetState("playing_arcade") == GLOBAL_OFF)
		m_Local.m_bPlayingArcade = false;

	if (m_nControlClass == CLASS_MANHACK)
	{
		m_Local.m_bPlayingArcadeAsManhack = true;
	}
	else
		m_Local.m_bPlayingArcadeAsManhack = false;
#endif
	if (player_showpredictedposition.GetBool())
	{
		Vector	predPos;

		UTIL_PredictedPosition(this, player_showpredictedposition_timestep.GetFloat(), &predPos);

		NDebugOverlay::Box(predPos, NAI_Hull::Mins(GetHullType()), NAI_Hull::Maxs(GetHullType()), 0, 255, 0, 0, 0.01f);
		NDebugOverlay::Line(GetAbsOrigin(), predPos, 0, 255, 0, 0, 0.01f);
	}

#ifdef HL2_EPISODIC
#ifdef DARKINTERVAL
	UpdateLocator();
#endif
	if (m_hLocatorTargetEntity != NULL)
	{
		// Keep track of the entity here, the client will pick up the rest of the work
		m_HL2Local.m_vecLocatorOrigin = m_hLocatorTargetEntity->WorldSpaceCenter();
	}
	else
	{
		m_HL2Local.m_vecLocatorOrigin = vec3_invalid; // This tells the client we have no locator target.
	}
#endif//HL2_EPISODIC

	// Riding a vehicle?
	if (IsInAVehicle())
	{
		VPROF("CHL2_Player::PreThink-Vehicle");
		// make sure we update the client, check for timed damage and update suit even if we are in a vehicle
		UpdateClientData();
		CheckTimeBasedDamage();

		// Allow the suit to recharge when in the vehicle.
		SuitPower_Update();
		CheckSuitUpdate();
		CheckSuitZoom();

		WaterMove();
		return;
	}

	// This is an experiment of mine- autojumping! 
	// only affects you if sv_autojump is nonzero.
	if ((GetFlags() & FL_ONGROUND) && sv_autojump.GetFloat() != 0)
	{
		VPROF("CHL2_Player::PreThink-Autojump");
		// check autojump
		Vector vecCheckDir;

		vecCheckDir = GetAbsVelocity();

		float flVelocity = VectorNormalize(vecCheckDir);

		if (flVelocity > 200)
		{
			// Going fast enough to autojump
			vecCheckDir = WorldSpaceCenter() + vecCheckDir * 34 - Vector(0, 0, 16);

			trace_t tr;

			UTIL_TraceHull(WorldSpaceCenter() - Vector(0, 0, 16), vecCheckDir, NAI_Hull::Mins(HULL_TINY_CENTERED), NAI_Hull::Maxs(HULL_TINY_CENTERED), MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER, &tr);

			//NDebugOverlay::Line( tr.startpos, tr.endpos, 0,255,0, true, 10 );

			if (tr.fraction == 1.0 && !tr.startsolid)
			{
				// Now trace down!
				UTIL_TraceLine(vecCheckDir, vecCheckDir - Vector(0, 0, 64), MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &tr);

				//NDebugOverlay::Line( tr.startpos, tr.endpos, 0,255,0, true, 10 );

				if (tr.fraction == 1.0 && !tr.startsolid)
				{
					// !!!HACKHACK
					// I KNOW, I KNOW, this is definitely not the right way to do this,
					// but I'm prototyping! (sjb)
					Vector vecNewVelocity = GetAbsVelocity();
					vecNewVelocity.z += 250;
					SetAbsVelocity(vecNewVelocity);
				}
			}
		}
	}

	VPROF_SCOPE_BEGIN("CHL2_Player::PreThink-Speed");
	HandleSpeedChanges();
#ifdef HL2_EPISODIC
	HandleArmorReduction();
#endif

	if (sv_stickysprint.GetBool() && m_bIsAutoSprinting)
	{
		// If we're ducked and not in the air
		if (IsDucked() && GetGroundEntity() != NULL)
		{
			StopSprinting();
		}
		// Stop sprinting if the player lets off the stick for a moment.
		else if (GetStickDist() == 0.0f)
		{
			if (CURTIME > m_fAutoSprintMinTime)
			{
				StopSprinting();
			}
		}
		else
		{
			// Stop sprinting one half second after the player stops inputting with the move stick.
			m_fAutoSprintMinTime = CURTIME + 0.5f;
		}
	}
	else if (IsSprinting())
	{
		// Disable sprint while ducked unless we're in the air (jumping)
		if (IsDucked() && (GetGroundEntity() != NULL))
		{
			StopSprinting();
		}
	}

	VPROF_SCOPE_END();

	if (g_fGameOver || IsPlayerLockedInPlace())
		return;         // finale

	VPROF_SCOPE_BEGIN("CHL2_Player::PreThink-ItemPreFrame");
	ItemPreFrame();
	VPROF_SCOPE_END();

	VPROF_SCOPE_BEGIN("CHL2_Player::PreThink-WaterMove");
	WaterMove();
	VPROF_SCOPE_END();

	if (g_pGameRules && g_pGameRules->FAllowFlashlight())
		m_Local.m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	else
		m_Local.m_iHideHUD |= HIDEHUD_FLASHLIGHT;


	VPROF_SCOPE_BEGIN("CHL2_Player::PreThink-CommanderUpdate");
	CommanderUpdate();
	VPROF_SCOPE_END();

	// Operate suit accessories and manage power consumption/charge
	VPROF_SCOPE_BEGIN("CHL2_Player::PreThink-SuitPower_Update");
	SuitPower_Update();
	VPROF_SCOPE_END();

	// checks if new client data (for HUD and view control) needs to be sent to the client
	VPROF_SCOPE_BEGIN("CHL2_Player::PreThink-UpdateClientData");
	UpdateClientData();
	VPROF_SCOPE_END();

	VPROF_SCOPE_BEGIN("CHL2_Player::PreThink-CheckTimeBasedDamage");
	CheckTimeBasedDamage();
	VPROF_SCOPE_END();

	VPROF_SCOPE_BEGIN("CHL2_Player::PreThink-CheckSuitUpdate");
	CheckSuitUpdate();
	VPROF_SCOPE_END();

	VPROF_SCOPE_BEGIN("CHL2_Player::PreThink-CheckSuitZoom");
	CheckSuitZoom();
	VPROF_SCOPE_END();

	if (m_lifeState >= LIFE_DYING)
	{
		PlayerDeathThink();
		return;
	}
#ifndef DARKINTERVAL
#ifdef HL2_EPISODIC
	CheckFlashlight();
#endif	// HL2_EPISODIC
#endif
	// So the correct flags get sent to client asap.
	//
	if (m_afPhysicsFlags & PFLAG_DIROVERRIDE)
		AddFlag(FL_ONTRAIN);
	else
		RemoveFlag(FL_ONTRAIN);

	// Train speed control
	if (m_afPhysicsFlags & PFLAG_DIROVERRIDE)
	{
		CBaseEntity *pTrain = GetGroundEntity();
		float vel;

		if (pTrain)
		{
			if (!(pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE))
				pTrain = NULL;
		}

		if (!pTrain)
		{
			if (GetActiveWeapon() && (GetActiveWeapon()->ObjectCaps() & FCAP_DIRECTIONAL_USE))
			{
				m_iTrain = TRAIN_ACTIVE | TRAIN_NEW;

				if (m_nButtons & IN_FORWARD)
				{
					m_iTrain |= TRAIN_FAST;
				}
				else if (m_nButtons & IN_BACK)
				{
					m_iTrain |= TRAIN_BACK;
				}
				else
				{
					m_iTrain |= TRAIN_NEUTRAL;
				}
				return;
			}
			else
			{
				trace_t trainTrace;
				// Maybe this is on the other side of a level transition
				UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, -38),
					MASK_PLAYERSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trainTrace);

				if (trainTrace.fraction != 1.0 && trainTrace.m_pEnt)
					pTrain = trainTrace.m_pEnt;


				if (!pTrain || !(pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) || !pTrain->OnControls(this))
				{
					//					Warning( "In train mode with no train!\n" );
					m_afPhysicsFlags &= ~PFLAG_DIROVERRIDE;
					m_iTrain = TRAIN_NEW | TRAIN_OFF;
					return;
				}
			}
		}
		else if (!(GetFlags() & FL_ONGROUND) || pTrain->HasSpawnFlags(SF_TRACKTRAIN_NOCONTROL) || (m_nButtons & (IN_MOVELEFT | IN_MOVERIGHT)))
		{
			// Turn off the train if you jump, strafe, or the train controls go dead
			m_afPhysicsFlags &= ~PFLAG_DIROVERRIDE;
			m_iTrain = TRAIN_NEW | TRAIN_OFF;
			return;
		}

		SetAbsVelocity(vec3_origin);
		vel = 0;
		if (m_afButtonPressed & IN_FORWARD)
		{
			vel = 1;
			pTrain->Use(this, this, USE_SET, (float)vel);
		}
		else if (m_afButtonPressed & IN_BACK)
		{
			vel = -1;
			pTrain->Use(this, this, USE_SET, (float)vel);
		}

		if (vel)
		{
			m_iTrain = TrainSpeed(pTrain->m_flSpeed, ((CFuncTrackTrain*)pTrain)->GetMaxSpeed());
			m_iTrain |= TRAIN_ACTIVE | TRAIN_NEW;
		}
	}
	else if (m_iTrain & TRAIN_ACTIVE)
	{
		m_iTrain = TRAIN_NEW; // turn off train
	}

	//
	// If we're not on the ground, we're falling. Update our falling velocity.
	//
	if (!(GetFlags() & FL_ONGROUND))
	{
		m_Local.m_flFallVelocity = -GetAbsVelocity().z;
	}

	if (m_afPhysicsFlags & PFLAG_ONBARNACLE)
	{
		bool bOnBarnacle = false;
		CAI_BaseNPC *pBarnacle = NULL;
		do
		{
			// FIXME: Not a good or fast solution, but maybe it will catch the bug!
			pBarnacle = (CAI_BaseNPC*)gEntList.FindEntityByClassname(pBarnacle, "npc_barnacle");
			if (pBarnacle)
			{
				if (pBarnacle->GetEnemy() == this)
				{
					bOnBarnacle = true;
				}
			}
		} while (pBarnacle);

		if (!bOnBarnacle)
		{
		//	Assert(0);
			m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
		}
		else
		{
			SetAbsVelocity(vec3_origin);
		}
	}
#ifdef DARKINTERVAL
	StudioFrameAdvance();//!!!HACKHACK!!! Can't be hit by traceline when not animating?
#endif
						 // Update weapon's ready status
	UpdateWeaponPosture();
	// Disallow shooting while zooming // DI CHANGE: don't
#ifndef DARKINTERVAL
	if (m_nButtons & IN_ZOOM)
	{
	//FIXME: Held weapons like the grenade get sad when this happens
	#ifdef HL2_EPISODIC
	// Episodic allows players to zoom while using a func_tank
	CBaseCombatWeapon* pWep = GetActiveWeapon();
	if (!m_hUseEntity || (pWep && pWep->IsWeaponVisible()))
	#endif
	m_nButtons &= ~(IN_ATTACK | IN_ATTACK2);
	}
#endif

	// DI NEW: contour enemies thru OICW scope. 
	// Run it in think so if we scope and then find an enemy thru it, 
	// he will get contoured.

	// DI todo - this whole messy system needs to be replaced with calling the entities in viewcone 
	// and asking them to use glow, not applying them through the zoom!
#ifdef DARKINTERVAL
	if (IsZooming() && GetActiveWeapon() != NULL)
	{
		if (GetZoomTypeFromWeapon() == ZOOM_OICW) // GetActiveWeapon()->ClassMatches("weapon_oicw"))
		{
			engine->ClientCommand(edict(), "mat_nightvision_enabled 1");
#if 1
			Vector forward;
			Vector origin;
			this->EyeVectors(&forward);
			origin = this->GetLocalOrigin();
			
			CBaseEntity *pEnt = NULL;
			while ((pEnt = gEntList.FindEntityInSphere(pEnt, GetLocalOrigin(), 4096.0f)) != NULL)
			{
				if (pEnt->IsNPC() && IsPointInCone(pEnt->WorldSpaceCenter(), origin, forward, CONE_10_DEGREES, 4096.0f))
				{
					CBaseCombatCharacter *pBCCp = ToBaseCombatCharacter(pEnt);

					if (pBCCp && pBCCp->IsAlive())
					{
						if (pEnt->m_iClassname == s_iszOICW_Zoom_AntlionGuard_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.55, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_AntlionSoldier_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.55, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_AntlionWorker_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.55, 0.05);
						}

						if (pEnt->m_iClassname == s_iszOICW_Zoom_Barnacle_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.55, 0.05);
						}

						if (pEnt->m_iClassname == s_iszOICW_Zoom_Tree_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.55, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_Citizen_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.15, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_Vortigaunt_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.15, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_Metrocop_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.15, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_Soldier_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.15, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_Headcrab_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.285, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_HeadcrabF_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.285, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_HeadcrabP_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.285, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_HeadcrabP2_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.285, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_Zombie_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.285, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_FastZombie_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.285, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_PoisonZombie_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.285, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_ZombieWorker_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.285, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_Zombine_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.285, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_Squid_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.55, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_Houndeye_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.55, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_SynthScanner_Classname)
						{
							pBCCp->AddGlowEffect(0.875, 0.675, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_ClawScanner_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.675, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_CityScanner_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.675, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_CityScanner_Classname)
						{
							pBCCp->AddGlowEffect(0.575, 0.675, 0.05);
						}

						else if (pEnt->m_iClassname == s_iszOICW_Zoom_Cremator_Classname)
						{
							pBCCp->AddGlowEffect(0.375, 0.475, 0.05);
						}
					}

					/*
					CBaseCombatCharacter *pBCCp = ToBaseCombatCharacter(pEnt);
					switch (pBCCp->GetHullType())
					{
					case HULL_HUMAN:
					{
					pBCCp->AddGlowEffect(0.875, 0.15, 0.05);
					} break;
					case HULL_HOUNDEYE:
					{
					pBCCp->AddGlowEffect(0.5, 0.5, 0.05);
					} break;
					case HULL_TINY:
					{
					pBCCp->AddGlowEffect(0.4, 0.05, 0.045);
					} break;
					case HULL_SMALL_CENTERED:
					{
					pBCCp->AddGlowEffect(0.1, 0.5, 0.05);
					} break;
					case HULL_WIDE_SHORT:
					{
					pBCCp->AddGlowEffect(0.5, 0.35, 0.05);
					} break;
					default:break;
					}
					*/
				}
			}
#endif
		}
	}
#endif // DARKINTERVAL
}

void CHL2_Player::PostThink(void)
{
	BaseClass::PostThink();

	if (!g_fGameOver && !IsPlayerLockedInPlace() && IsAlive())
	{
		HandleAdmireGlovesAnimation();
	}
#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
	// Try to fix the player if they're stuck
	if ( m_bStuckOnPortalCollisionObject )
	{
		Vector vForward = ( ( CProp_Portal* )m_hPortalEnvironment.Get() )->m_vPrevForward;
		Vector vNewPos = GetAbsOrigin() + vForward * gpGlobals->frametime * -1000.0f;
		Teleport(&vNewPos, NULL, &vForward);
		m_bStuckOnPortalCollisionObject = false;
	}
#endif
#endif // DARKINTERVAL
}

void CHL2_Player::StartAdmireGlovesAnimation(void)
{
	MDLCACHE_CRITICAL_SECTION();
	CBaseViewModel *vm = GetViewModel(0);

	if (vm && !GetActiveWeapon())
	{
		vm->SetWeaponModel("models/weapons/v_hands.mdl", NULL);
		ShowViewModel(true);

		int	idealSequence = vm->SelectWeightedSequence(ACT_VM_IDLE);

		if (idealSequence >= 0)
		{
			vm->SendViewModelMatchingSequence(idealSequence);
			m_flAdmireGlovesAnimTime = CURTIME + vm->SequenceDuration(idealSequence);
		}
	}
}

void CHL2_Player::HandleAdmireGlovesAnimation(void)
{
	CBaseViewModel *pVM = GetViewModel();

	if (pVM && pVM->GetOwningWeapon() == NULL)
	{
		if (m_flAdmireGlovesAnimTime != 0.0)
		{
			if (m_flAdmireGlovesAnimTime > CURTIME)
			{
				pVM->m_flPlaybackRate = 1.0f;
				pVM->StudioFrameAdvance();
			}
			else if (m_flAdmireGlovesAnimTime < CURTIME)
			{
				m_flAdmireGlovesAnimTime = 0.0f;
				pVM->SetWeaponModel(NULL, NULL);
			}
		}
	}
	else
		m_flAdmireGlovesAnimTime = 0.0f;
}

#define HL2PLAYER_RELOADGAME_ATTACK_DELAY 0.1f

void CHL2_Player::Activate(void)
{
	BaseClass::Activate();
	InitSprinting();

#ifdef HL2_EPISODIC

	// Delay attacks by 1 second after loading a game.
	if (GetActiveWeapon())
	{
		float flRemaining = GetActiveWeapon()->m_flNextPrimaryAttack - CURTIME;

		if (flRemaining < HL2PLAYER_RELOADGAME_ATTACK_DELAY)
		{
			GetActiveWeapon()->m_flNextPrimaryAttack = CURTIME + HL2PLAYER_RELOADGAME_ATTACK_DELAY;
		}

		flRemaining = GetActiveWeapon()->m_flNextSecondaryAttack - CURTIME;

		if (flRemaining < HL2PLAYER_RELOADGAME_ATTACK_DELAY)
		{
			GetActiveWeapon()->m_flNextSecondaryAttack = CURTIME + HL2PLAYER_RELOADGAME_ATTACK_DELAY;
		}
	}

#endif

	GetPlayerProxy();
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
Class_T  CHL2_Player::Classify(void)
{
	// If player controlling another entity?  If so, return this class
	if (m_nControlClass != CLASS_NONE)
	{
		return m_nControlClass;
	}
	else
	{
		if (IsInAVehicle())
		{
			IServerVehicle *pVehicle = GetVehicle();
			return pVehicle->ClassifyPassenger(this, CLASS_PLAYER);
		}
		else
		{
			return CLASS_PLAYER;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CHL2_Player::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	if (interactionType == g_interactionBarnacleVictimDangle)
		return false;

	if (interactionType == g_interactionBarnacleVictimReleased)
	{
		m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
		SetMoveType(MOVETYPE_WALK);
		return true;
	}
	else if (interactionType == g_interactionBarnacleVictimGrab)
	{
#ifdef HL2_EPISODIC
		CNPC_Alyx *pAlyx = CNPC_Alyx::GetAlyx();
		if ( pAlyx )
		{
			// Make Alyx totally hate this barnacle so that she saves the player.
			int priority;

			priority = pAlyx->IRelationPriority(sourceEnt);
			pAlyx->AddEntityRelationship(sourceEnt, D_HT, priority + 5);
		}
#endif//HL2_EPISODIC
		m_afPhysicsFlags |= PFLAG_ONBARNACLE;
		ClearUseEntity();
		return true;
	}
	return false;
}


void CHL2_Player::PlayerRunCommand(CUserCmd *ucmd, IMoveHelper *moveHelper)
{
	// Handle FL_FROZEN.
	if (m_afPhysicsFlags & PFLAG_ONBARNACLE)
	{
		ucmd->forwardmove = 0;
		ucmd->sidemove = 0;
		ucmd->upmove = 0;
		ucmd->buttons &= ~IN_USE;
	}

	// Can't use stuff while dead
	if (IsDead())
	{
		ucmd->buttons &= ~IN_USE;
	}

	//Update our movement information
	if ((ucmd->forwardmove != 0) || (ucmd->sidemove != 0) || (ucmd->upmove != 0))
	{
		m_flIdleTime -= TICK_INTERVAL * 2.0f;

		if (m_flIdleTime < 0.0f)
		{
			m_flIdleTime = 0.0f;
		}

		m_flMoveTime += TICK_INTERVAL;

		if (m_flMoveTime > 4.0f)
		{
			m_flMoveTime = 4.0f;
		}
	}
	else
	{
		m_flIdleTime += TICK_INTERVAL;

		if (m_flIdleTime > 4.0f)
		{
			m_flIdleTime = 4.0f;
		}

		m_flMoveTime -= TICK_INTERVAL * 2.0f;

		if (m_flMoveTime < 0.0f)
		{
			m_flMoveTime = 0.0f;
		}
	}

	//Msg("Player time: [ACTIVE: %f]\t[IDLE: %f]\n", m_flMoveTime, m_flIdleTime );
#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
	if (m_bFixEyeAnglesFromPortalling)
	{
		//the idea here is to handle the notion that the player has portalled, but they sent us an angle update before receiving that message.
		//If we don't handle this here, we end up sending back their old angles which makes them hiccup their angles for a frame
		float fOldAngleDiff = fabs(AngleDistance(ucmd->viewangles.x, m_qPrePortalledViewAngles.x));
		fOldAngleDiff += fabs(AngleDistance(ucmd->viewangles.y, m_qPrePortalledViewAngles.y));
		fOldAngleDiff += fabs(AngleDistance(ucmd->viewangles.z, m_qPrePortalledViewAngles.z));

		float fCurrentAngleDiff = fabs(AngleDistance(ucmd->viewangles.x, pl.v_angle.x));
		fCurrentAngleDiff += fabs(AngleDistance(ucmd->viewangles.y, pl.v_angle.y));
		fCurrentAngleDiff += fabs(AngleDistance(ucmd->viewangles.z, pl.v_angle.z));

		if (fCurrentAngleDiff > fOldAngleDiff)
			ucmd->viewangles = TransformAnglesToWorldSpace(ucmd->viewangles, m_matLastPortalled.As3x4());
		m_bFixEyeAnglesFromPortalling = false;
	}
#endif
#endif // DARKINTERVAL
	BaseClass::PlayerRunCommand(ucmd, moveHelper);
}

//-----------------------------------------------------------------------------
// Purpose: Sets HL2 specific defaults.
//-----------------------------------------------------------------------------
void CHL2_Player::Spawn(void)
{
	SetModel("models/player.mdl");

	BaseClass::Spawn();

	//
	// Our player movement speed is set once here. This will override the cl_xxxx
	// cvars unless they are set to be lower than this.
	//
	//m_flMaxspeed = 320;

	if (!IsSuitEquipped())
		StartWalking();

	SuitPower_SetCharge(100);

	m_Local.m_iHideHUD |= HIDEHUD_CHAT;

	m_pPlayerAISquad = g_AI_SquadManager.FindCreateSquad(AllocPooledString(PLAYER_SQUADNAME));

	InitSprinting();

	// Setup our flashlight values
	m_HL2Local.m_flFlashBattery = 100.0f;

	GetPlayerProxy();

	SetFlashlightPowerDrainScale(1.0f);
#ifdef DARKINTERVAL
	SetSprintPowerDrainScale(1.0f);
#endif
#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
	pl.deadflag = false;
	RemoveSolidFlags(FSOLID_NOT_SOLID);

	StopObserverMode();

	m_nRenderFX = kRenderNormal;

	AddFlag(FL_ONGROUND); // set the player on the ground at the start of the round.

	RemoveFlag(FL_FROZEN);

	m_iSpawnInterpCounter = (m_iSpawnInterpCounter + 1) % 8;

	m_Local.m_bDucked = false;

	SetPlayerUnderwater(false);
#endif
#endif // DARKINTERVAL

#ifdef DARKINTERVAL
	// OICW Zoom class-check functionality

	// DI todo - this whole messy system needs to be replaced with calling the entities in viewcone 
	// and asking them to use glow, not applying them through the zoom!

	s_iszOICW_Zoom_AntlionGuard_Classname = AllocPooledString("npc_antlionguard");
	s_iszOICW_Zoom_AntlionSoldier_Classname = AllocPooledString("npc_antlion");
	s_iszOICW_Zoom_AntlionWorker_Classname = AllocPooledString("npc_antlionguard");
	s_iszOICW_Zoom_Barnacle_Classname = AllocPooledString("npc_barnacle");
	s_iszOICW_Zoom_Citizen_Classname = AllocPooledString("npc_citizen");
	s_iszOICW_Zoom_Vortigaunt_Classname = AllocPooledString("npc_vortigaunt");
	s_iszOICW_Zoom_Metrocop_Classname = AllocPooledString("npc_metropolice");
	s_iszOICW_Zoom_Soldier_Classname = AllocPooledString("npc_combine_s");
	s_iszOICW_Zoom_Squid_Classname = AllocPooledString("npc_squid");
	s_iszOICW_Zoom_Houndeye_Classname = AllocPooledString("npc_houndeye");
	s_iszOICW_Zoom_Headcrab_Classname = AllocPooledString("npc_headcrab");
	s_iszOICW_Zoom_HeadcrabF_Classname = AllocPooledString("npc_headcrab_fast");
	s_iszOICW_Zoom_HeadcrabP_Classname = AllocPooledString("npc_headcrab_black");
	s_iszOICW_Zoom_HeadcrabP2_Classname = AllocPooledString("npc_headcrab_poison");
	s_iszOICW_Zoom_Zombie_Classname = AllocPooledString("npc_zombie");
	s_iszOICW_Zoom_FastZombie_Classname = AllocPooledString("npc_fastzombie");
	s_iszOICW_Zoom_PoisonZombie_Classname = AllocPooledString("npc_poisonzombie");
	s_iszOICW_Zoom_ZombieWorker_Classname = AllocPooledString("npc_zombie_worker");
	s_iszOICW_Zoom_Zombine_Classname = AllocPooledString("npc_zombine");
	s_iszOICW_Zoom_SynthScanner_Classname = AllocPooledString("npc_synth_scanner");
	s_iszOICW_Zoom_ClawScanner_Classname = AllocPooledString("npc_clawscanner");
	s_iszOICW_Zoom_CityScanner_Classname = AllocPooledString("npc_cscanner");
	s_iszOICW_Zoom_Cremator_Classname = AllocPooledString("npc_cremator");
	s_iszOICW_Zoom_Tree_Classname = AllocPooledString("npc_tree");

	m_flLastBatteryTime = -1.0f;

	m_Local.m_bSuitlessHUDVisible = true;  // DI NEW - restore after stripping worker suit on maps with credits
#endif // DARKINTERVAL
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::UpdateLocatorPosition(const Vector &vecPosition)
{
#ifdef HL2_EPISODIC
	m_HL2Local.m_vecLocatorOrigin = vecPosition;
#endif//HL2_EPISODIC 
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::InitSprinting(void)
{
	StopSprinting();
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we are allowed to sprint now.
//-----------------------------------------------------------------------------
bool CHL2_Player::CanSprint()
{
	return (m_bSprintEnabled &&										// Only if sprint is enabled 
		!IsWalking() &&												// Not if we're walking
		!(m_Local.m_bDucked && !m_Local.m_bDucking) &&				// Nor if we're ducking
		(GetWaterLevel() != 3) &&									// Certainly not underwater
		(GlobalEntity_GetState("suit_no_sprint") != GLOBAL_ON) &&	// Out of the question without the sprint module
		!(m_Local.m_bParalyzed));// not if we're paralyzed by a creature
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::StartAutoSprint()
{
	if (IsSprinting())
	{
		StopSprinting();
	}
	else
	{
		StartSprinting();
		m_bIsAutoSprinting = true;
		m_fAutoSprintMinTime = CURTIME + 1.5f;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::StartSprinting(void)
{
#ifdef DARKINTERVAL			// DI change - limit to suit power (stamina, essentially)
	if (!IsSuitEquipped())  // only for when no suit (first chapters). Otherwise stamina's infinite.
#endif
	{					  
		if (m_HL2Local.m_flSuitPower < 10)
		{
			// Don't sprint unless there's a reasonable
			// amount of suit power.

			// debounce the button for sound playing
			if (m_afButtonPressed & IN_SPEED)
			{
				CPASAttenuationFilter filter(this);
				filter.UsePredictionRules();
				EmitSound(filter, entindex(), "HL2Player.SprintNoPower");
			}
			return;
		}

		if (!SuitPower_AddDevice(SuitDeviceSprint))
			return;
	}
#ifdef DARKINTERVAL	
	if (m_Local.m_bParalyzed)
	{
		SetMaxSpeed((HL2_NORM_SPEED * 0.5));
	}
	else
	{
		SetMaxSpeed(IsSuitEquipped() ? HL2_SPRINT_SPEED_SUIT : HL2_SPRINT_SPEED);
	}
#else
	CPASAttenuationFilter filter(this);
	filter.UsePredictionRules();
	EmitSound(filter, entindex(), "HL2Player.SprintStart");

	SetMaxSpeed(HL2_SPRINT_SPEED);
#endif
	m_fIsSprinting = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::StopSprinting(void)
{
#ifdef DARKINTERVAL	
	if (!IsSuitEquipped()) // Moved it: if no suit, follow usual logic. If the suit is there, player's stamina is 
	{						// infinite, and we don't add spring device so why even check for it here. 
		if (m_HL2Local.m_bitsActiveDevices & SuitDeviceSprint.GetDeviceID())
		{
			SuitPower_RemoveDevice(SuitDeviceSprint);
		}
		SetMaxSpeed(m_Local.m_bParalyzed ? (HL2_WALK_SPEED * 0.5) : HL2_WALK_SPEED); // NORM
	}
	else
	{
		SetMaxSpeed(m_Local.m_bParalyzed ? (HL2_NORM_SPEED * 0.5) : HL2_NORM_SPEED); // WALK
	}
#else
	if ( m_HL2Local.m_bitsActiveDevices & SuitDeviceSprint.GetDeviceID() )
	{
		SuitPower_RemoveDevice(SuitDeviceSprint);
	}

	if ( IsSuitEquipped() )
	{
		SetMaxSpeed(HL2_NORM_SPEED);
	} else
	{
		SetMaxSpeed(HL2_WALK_SPEED);
	}
#endif
	m_fIsSprinting = false;

	if (sv_stickysprint.GetBool())
	{
		m_bIsAutoSprinting = false;
		m_fAutoSprintMinTime = 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called to disable and enable sprint due to temporary circumstances:
//			- Carrying a heavy object with the physcannon
//-----------------------------------------------------------------------------
void CHL2_Player::EnableSprint(bool bEnable)
{
	if (!bEnable && IsSprinting())
	{
		StopSprinting();
	}

	m_bSprintEnabled = bEnable;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::StartWalking(void)
{
#ifdef DARKINTERVAL	
	float walkSpeed = HL2_WALK_SPEED;

	if (m_nButtons & IN_WALK && !m_Local.m_bDucked && !m_Local.m_bDucking) walkSpeed = HL2_SLOW_SPEED;

	if (m_Local.m_bParalyzed) walkSpeed /= 2;

	SetMaxSpeed(walkSpeed);
#else
	SetMaxSpeed(HL2_WALK_SPEED);
#endif
	m_fIsWalking = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::StopWalking(void)
{
#ifdef DARKINTERVAL	
	SetMaxSpeed(m_Local.m_bParalyzed ? (HL2_NORM_SPEED * 0.5) : HL2_NORM_SPEED);
#else
	SetMaxSpeed(HL2_NORM_SPEED);
#endif
	m_fIsWalking = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHL2_Player::CanZoom(CBaseEntity *pRequester)
{
	if (IsZooming())
		return false;

	//Check our weapon

	return true;
}
#ifdef DARKINTERVAL	
int CHL2_Player::GetZoomTypeFromWeapon(void)
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	// hidehud weaponselection implies having a func tank or other thing suppressing the weapon
	if (pWeapon == NULL || pWeapon->IsHolstered() || (m_Local.m_iHideHUD & HIDEHUD_WEAPONSELECTION) == true ) 
		return IsSuitEquipped() ? ZOOM_SUIT : ZOOM_NAKEDEYE;

	int t = pWeapon->GetZoomType();

	return t;
}
#endif
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::ToggleZoom(void)
{
	if (IsZooming())
	{
		StopZooming();
	}
	else
	{
#ifdef DARKINTERVAL	
		StartZooming(GetZoomTypeFromWeapon());
#else
		StartZooming();
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: +zoom suit zoom
//-----------------------------------------------------------------------------
#ifdef DARKINTERVAL	
void CHL2_Player::StartZooming(int zoomtype)
{
	int iFOV = 75;
	float fTime = 0.0f;

	switch (zoomtype)
	{
	case ZOOM_NAKEDEYE:
	{
		iFOV = 60;
		fTime = 0.2f;
	}
	break;
	case ZOOM_SUIT:
	{
		iFOV = 35;
		fTime = 0.4f;
	}
	break;
	case ZOOM_OICW:
	{
		iFOV = 30.0;
		fTime = 0.33f;
	}
	case ZOOM_CROSSBOW:
	case ZOOM_SNIPER:
	{
		iFOV = 18.5;
		fTime = 0.25f;
	}
	break;
	default:
		break;
	}
	
	if (SetFOV(this, iFOV, fTime))
	{
		m_HL2Local.m_bZooming = true;
		m_HL2Local.m_nZoomType = zoomtype;
	}
}
#else
void CHL2_Player::StartZooming(void)
{
	int iFOV = 25;
	if ( SetFOV(this, iFOV, 0.4f) )
	{
		m_HL2Local.m_bZooming = true;
	}
}
#endif // DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_Player::StopZooming(void)
{
	int iFOV = GetZoomOwnerDesiredFOV(m_hZoomOwner);

	if (SetFOV(this, iFOV, 0.2f))
	{
		m_HL2Local.m_bZooming = false;
	}
#ifdef DARKINTERVAL	
	engine->ClientCommand(edict(), "mat_nightvision_enabled 0");

	CBaseEntity *pEntity = NULL;
	while ((pEntity = gEntList.FindEntityInSphere(pEntity, GetAbsOrigin(), 32768.0f)) != NULL)
	{
		CBaseCombatCharacter *pEntityBCC = pEntity->MyCombatCharacterPointer();
		if (pEntityBCC) pEntityBCC->RemoveGlowEffect();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHL2_Player::IsZooming(void)
{
	if (m_hZoomOwner != NULL)
		return true;

	return false;
}

class CPhysicsPlayerCallback : public IPhysicsPlayerControllerEvent
{
public:
	int ShouldMoveTo(IPhysicsObject *pObject, const Vector &position)
	{
		CHL2_Player *pPlayer = (CHL2_Player *)pObject->GetGameData();
		if (pPlayer)
		{
			if (pPlayer->TouchedPhysics())
			{
				return 0;
			}
		}
		return 1;
	}
};

static CPhysicsPlayerCallback playerCallback;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_Player::InitVCollision(const Vector &vecAbsOrigin, const Vector &vecAbsVelocity)
{
	BaseClass::InitVCollision(vecAbsOrigin, vecAbsVelocity);

	// Setup the HL2 specific callback.
	IPhysicsPlayerController *pPlayerController = GetPhysicsController();
	if (pPlayerController)
	{
		pPlayerController->SetEventHandler(&playerCallback);
	}
}

CHL2_Player::~CHL2_Player(void)
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool CHL2_Player::CommanderFindGoal(commandgoal_t *pGoal)
{
	CAI_BaseNPC *pAllyNpc;
	trace_t	tr;
	Vector	vecTarget;
	Vector	forward;

	EyeVectors(&forward);

	//---------------------------------
	// MASK_SHOT on purpose! So that you don't hit the invisible hulls of the NPCs.
	CTraceFilterSkipTwoEntities filter(this, PhysCannonGetHeldEntity(GetActiveWeapon()), COLLISION_GROUP_INTERACTIVE_DEBRIS);

	UTIL_TraceLine(EyePosition(), EyePosition() + forward * MAX_COORD_RANGE, MASK_SHOT, &filter, &tr);

	if (!tr.DidHitWorld())
	{
		CUtlVector<CAI_BaseNPC *> Allies;
		AISquadIter_t iter;
		for (pAllyNpc = m_pPlayerAISquad->GetFirstMember(&iter); pAllyNpc; pAllyNpc = m_pPlayerAISquad->GetNextMember(&iter))
		{
			if (pAllyNpc->IsCommandable())
				Allies.AddToTail(pAllyNpc);
		}

		for (int i = 0; i < Allies.Count(); i++)
		{
			if (Allies[i]->IsValidCommandTarget(tr.m_pEnt))
			{
				pGoal->m_pGoalEntity = tr.m_pEnt;
				return true;
			}
		}
	}

	if (tr.fraction == 1.0 || (tr.surface.flags & SURF_SKY))
	{
		// Move commands invalid against skybox.
		pGoal->m_vecGoalLocation = tr.endpos;
		return false;
	}

	if (tr.m_pEnt->IsNPC() && ((CAI_BaseNPC *)(tr.m_pEnt))->IsCommandable())
	{
		pGoal->m_vecGoalLocation = tr.m_pEnt->GetAbsOrigin();
	}
	else
	{
		vecTarget = tr.endpos;

		Vector mins(-16, -16, 0);
		Vector maxs(16, 16, 0);

		// Back up from whatever we hit so that there's enough space at the 
		// target location for a bounding box.
		// Now trace down. 
		//UTIL_TraceLine( vecTarget, vecTarget - Vector( 0, 0, 8192 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
		UTIL_TraceHull(vecTarget + tr.plane.normal * 24,
			vecTarget - Vector(0, 0, 8192),
			mins,
			maxs,
			MASK_SOLID_BRUSHONLY,
			this,
			COLLISION_GROUP_NONE,
			&tr);


		if (!tr.startsolid)
			pGoal->m_vecGoalLocation = tr.endpos;
		else
			pGoal->m_vecGoalLocation = vecTarget;
	}

	pAllyNpc = GetSquadCommandRepresentative();
	if (!pAllyNpc)
		return false;

	vecTarget = pGoal->m_vecGoalLocation;
	if (!pAllyNpc->FindNearestValidGoalPos(vecTarget, &pGoal->m_vecGoalLocation))
		return false;

	return ((vecTarget - pGoal->m_vecGoalLocation).LengthSqr() < Square(15 * 12));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CAI_BaseNPC *CHL2_Player::GetSquadCommandRepresentative()
{
	if (m_pPlayerAISquad != NULL)
	{
		CAI_BaseNPC *pAllyNpc = m_pPlayerAISquad->GetFirstMember();

		if (pAllyNpc)
		{
			return pAllyNpc->GetSquadCommandRepresentative();
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CHL2_Player::GetNumSquadCommandables()
{
	AISquadIter_t iter;
	int c = 0;
	for (CAI_BaseNPC *pAllyNpc = m_pPlayerAISquad->GetFirstMember(&iter); pAllyNpc; pAllyNpc = m_pPlayerAISquad->GetNextMember(&iter))
	{
		if (pAllyNpc->IsCommandable())
			c++;
	}
	return c;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CHL2_Player::GetNumSquadCommandableMedics()
{
	AISquadIter_t iter;
	int c = 0;
	for (CAI_BaseNPC *pAllyNpc = m_pPlayerAISquad->GetFirstMember(&iter); pAllyNpc; pAllyNpc = m_pPlayerAISquad->GetNextMember(&iter))
	{
		if (pAllyNpc->IsCommandable() && pAllyNpc->IsMedic())
			c++;
	}
	return c;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::CommanderUpdate()
{
	CAI_BaseNPC *pCommandRepresentative = GetSquadCommandRepresentative();
	bool bFollowMode = false;
	if (pCommandRepresentative)
	{
		bFollowMode = (pCommandRepresentative->GetCommandGoal() == vec3_invalid);

		// set the variables for network transmission (to show on the hud)
		m_HL2Local.m_iSquadMemberCount = GetNumSquadCommandables();
		m_HL2Local.m_iSquadMedicCount = GetNumSquadCommandableMedics();
		m_HL2Local.m_fSquadInFollowMode = bFollowMode;

		// debugging code for displaying extra squad indicators
		/*
		char *pszMoving = "";
		AISquadIter_t iter;
		for ( CAI_BaseNPC *pAllyNpc = m_pPlayerAISquad->GetFirstMember(&iter); pAllyNpc; pAllyNpc = m_pPlayerAISquad->GetNextMember(&iter) )
		{
		if ( pAllyNpc->IsCommandMoving() )
		{
		pszMoving = "<-";
		break;
		}
		}

		NDebugOverlay::ScreenText(
		0.932, 0.919,
		CFmtStr( "%d|%c%s", GetNumSquadCommandables(), ( bFollowMode ) ? 'F' : 'S', pszMoving ),
		255, 128, 0, 128,
		0 );
		*/

	}
	else
	{
		m_HL2Local.m_iSquadMemberCount = 0;
		m_HL2Local.m_iSquadMedicCount = 0;
		m_HL2Local.m_fSquadInFollowMode = true;
	}

	if (m_QueuedCommand != CC_NONE && (m_QueuedCommand == CC_FOLLOW || gpGlobals->realtime - m_RealTimeLastSquadCommand >= player_squad_double_tap_time.GetFloat()))
	{
		CommanderExecute(m_QueuedCommand);
		m_QueuedCommand = CC_NONE;
	}
	else if (!bFollowMode && pCommandRepresentative && m_CommanderUpdateTimer.Expired() && player_squad_transient_commands.GetBool())
	{
		m_CommanderUpdateTimer.Set(2.5);

		if (pCommandRepresentative->ShouldAutoSummon())
			CommanderExecute(CC_FOLLOW);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//
// bHandled - indicates whether to continue delivering this order to
// all allies. Allows us to stop delivering certain types of orders once we find
// a suitable candidate. (like picking up a single weapon. We don't wish for
// all allies to respond and try to pick up one weapon).
//----------------------------------------------------------------------------- 
bool CHL2_Player::CommanderExecuteOne(CAI_BaseNPC *pNpc, const commandgoal_t &goal, CAI_BaseNPC **Allies, int numAllies)
{
	if (goal.m_pGoalEntity)
	{
		return pNpc->TargetOrder(goal.m_pGoalEntity, Allies, numAllies);
	}
	else if (pNpc->IsInPlayerSquad())
	{
		pNpc->MoveOrder(goal.m_vecGoalLocation, Allies, numAllies);
	}

	return true;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CHL2_Player::CommanderExecute(CommanderCommand_t command)
{
	CAI_BaseNPC *pPlayerSquadLeader = GetSquadCommandRepresentative();

	if (!pPlayerSquadLeader)
	{
		EmitSound("HL2Player.UseDeny");
		return;
	}

	int i;
	CUtlVector<CAI_BaseNPC *> Allies;
	commandgoal_t goal;

	if (command == CC_TOGGLE)
	{
		if (pPlayerSquadLeader->GetCommandGoal() != vec3_invalid)
			command = CC_FOLLOW;
		else
			command = CC_SEND;
	}
	else
	{
		if (command == CC_FOLLOW && pPlayerSquadLeader->GetCommandGoal() == vec3_invalid)
			return;
	}

	if (command == CC_FOLLOW)
	{
		goal.m_pGoalEntity = this;
		goal.m_vecGoalLocation = vec3_invalid;
	}
	else
	{
		goal.m_pGoalEntity = NULL;
		goal.m_vecGoalLocation = vec3_invalid;

		// Find a goal for ourselves.
		if (!CommanderFindGoal(&goal))
		{
			EmitSound("HL2Player.UseDeny");
			return; // just keep following
		}
	}

#ifdef _DEBUG
	if (goal.m_pGoalEntity == NULL && goal.m_vecGoalLocation == vec3_invalid)
	{
		DevMsg(1, "**ERROR: Someone sent an invalid goal to CommanderExecute!\n");
	}
#endif // _DEBUG

	AISquadIter_t iter;
	for (CAI_BaseNPC *pAllyNpc = m_pPlayerAISquad->GetFirstMember(&iter); pAllyNpc; pAllyNpc = m_pPlayerAISquad->GetNextMember(&iter))
	{
		if (pAllyNpc->IsCommandable())
			Allies.AddToTail(pAllyNpc);
	}

	//---------------------------------
	// If the trace hits an NPC, send all ally NPCs a "target" order. Always
	// goes to targeted one first
#ifdef DBGFLAG_ASSERT
	int nAIs = g_AI_Manager.NumAIs();
#endif
	CAI_BaseNPC * pTargetNpc = (goal.m_pGoalEntity) ? goal.m_pGoalEntity->MyNPCPointer() : NULL;

	bool bHandled = false;
	if (pTargetNpc)
	{
		bHandled = !CommanderExecuteOne(pTargetNpc, goal, Allies.Base(), Allies.Count());
	}

	for (i = 0; !bHandled && i < Allies.Count(); i++)
	{
		if (Allies[i] != pTargetNpc && Allies[i]->IsPlayerAlly())
		{
			bHandled = !CommanderExecuteOne(Allies[i], goal, Allies.Base(), Allies.Count());
		}
		Assert(nAIs == g_AI_Manager.NumAIs()); // not coded to support mutating set of NPCs
	}
}

//-----------------------------------------------------------------------------
// Enter/exit commander mode, manage ally selection.
//-----------------------------------------------------------------------------
void CHL2_Player::CommanderMode()
{
	float commandInterval = gpGlobals->realtime - m_RealTimeLastSquadCommand;
	m_RealTimeLastSquadCommand = gpGlobals->realtime;
	if (commandInterval < player_squad_double_tap_time.GetFloat())
	{
		m_QueuedCommand = CC_FOLLOW;
	}
	else
	{
		m_QueuedCommand = (player_squad_transient_commands.GetBool()) ? CC_SEND : CC_TOGGLE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iImpulse - 
//-----------------------------------------------------------------------------
void CHL2_Player::CheatImpulseCommands(int iImpulse)
{
	switch (iImpulse)
	{
	case 50:
	{
		CommanderMode();
		break;
	}

	case 51:
	{
		// Cheat to create a dynamic resupply item
		Vector vecForward;
		AngleVectors(EyeAngles(), &vecForward);
		CBaseEntity *pItem = (CBaseEntity *)CreateEntityByName("item_dynamic_resupply");
		if (pItem)
		{
			Vector vecOrigin = GetAbsOrigin() + vecForward * 256 + Vector(0, 0, 64);
			QAngle vecAngles(0, GetAbsAngles().y - 90, 0);
			pItem->SetAbsOrigin(vecOrigin);
			pItem->SetAbsAngles(vecAngles);
			pItem->KeyValue("targetname", "resupply");
			pItem->Spawn();
			pItem->Activate();
		}
		break;
	}

	case 52:
	{
		// Rangefinder
		trace_t tr;
		UTIL_TraceLine(EyePosition(), EyePosition() + EyeDirection3D() * MAX_COORD_RANGE, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		if (tr.fraction != 1.0)
		{
			float flDist = (tr.startpos - tr.endpos).Length();
			float flDist2D = (tr.startpos - tr.endpos).Length2D();
			Msg("\nStartPos: %.4f %.4f %.4f --- EndPos: %.4f %.4f %.4f\n", tr.startpos.x, tr.startpos.y, tr.startpos.z, tr.endpos.x, tr.endpos.y, tr.endpos.z);
			Msg("3D Distance: %.4f units  (%.2f feet) --- 2D Distance: %.4f units  (%.2f feet)\n", flDist, flDist / 12.0, flDist2D, flDist2D / 12.0);
		}

		break;
	}

	default:
		BaseClass::CheatImpulseCommands(iImpulse);
	}
}
#ifdef DARKINTERVAL	
#ifdef PORTAL_COMPILE
//-----------------------------------------------------------------------------
// Purpose: Update the area bits variable which is networked down to the client to determine
//			which area portals should be closed based on visibility.
// Input  : *pvs - pvs to be used to determine visibility of the portals
//-----------------------------------------------------------------------------
void CHL2_Player::UpdatePortalViewAreaBits(unsigned char *pvs, int pvssize)
{
	Assert(pvs);

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if (iPortalCount == 0)
		return;

	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
	int *portalArea = (int *)stackalloc(sizeof(int) * iPortalCount);
	bool *bUsePortalForVis = (bool *)stackalloc(sizeof(bool) * iPortalCount);

	unsigned char *portalTempBits = (unsigned char *)stackalloc(sizeof(unsigned char) * 32 * iPortalCount);
	COMPILE_TIME_ASSERT((sizeof(unsigned char) * 32) >= sizeof(((CPlayerLocalData*)0)->m_chAreaBits));

	// setup area bits for these portals
	for (int i = 0; i < iPortalCount; ++i)
	{
		CProp_Portal* pLocalPortal = pPortals[i];
		// Make sure this portal is active before adding it's location to the pvs
		if (pLocalPortal && pLocalPortal->m_bActivated)
		{
			CProp_Portal* pRemotePortal = pLocalPortal->m_hLinkedPortal.Get();

			// Make sure this portal's linked portal is in the PVS before we add what it can see
			if (pRemotePortal && pRemotePortal->m_bActivated && pRemotePortal->NetworkProp() &&
				pRemotePortal->NetworkProp()->IsInPVS(edict(), pvs, pvssize))
			{
				portalArea[i] = engine->GetArea(pPortals[i]->GetAbsOrigin());

				if (portalArea[i] >= 0)
				{
					bUsePortalForVis[i] = true;
				}

				engine->GetAreaBits(portalArea[i], &portalTempBits[i * 32], sizeof(unsigned char) * 32);
			}
		}
	}

	// Use the union of player-view area bits and the portal-view area bits of each portal
	for (int i = 0; i < m_Local.m_chAreaBits.Count(); i++)
	{
		for (int j = 0; j < iPortalCount; ++j)
		{
			// If this portal is active, in PVS and it's location is valid
			if (bUsePortalForVis[j])
			{
				m_Local.m_chAreaBits.Set(i, m_Local.m_chAreaBits[i] | portalTempBits[(j * 32) + i]);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// AddPortalCornersToEnginePVS
// Subroutine to wrap the adding of portal corners to the PVS which is called once for the setup of each portal.
// input - pPortal: the portal we are viewing 'out of' which needs it's corners added to the PVS
//////////////////////////////////////////////////////////////////////////
void AddPortalCornersToEnginePVS(CProp_Portal* pPortal)
{
	Assert(pPortal);

	if (!pPortal)
		return;

	Vector vForward, vRight, vUp;
	pPortal->GetVectors(&vForward, &vRight, &vUp);

	// Center of the remote portal
	Vector ptOrigin = pPortal->GetAbsOrigin();

	// Distance offsets to the different edges of the portal... Used in the placement checks
	Vector vToTopEdge = vUp * (PORTAL_HALF_HEIGHT - PORTAL_BUMP_FORGIVENESS);
	Vector vToBottomEdge = -vToTopEdge;
	Vector vToRightEdge = vRight * (PORTAL_HALF_WIDTH - PORTAL_BUMP_FORGIVENESS);
	Vector vToLeftEdge = -vToRightEdge;

	// Distance to place PVS points away from portal, to avoid being in solid
	Vector vForwardBump = vForward * 1.0f;

	// Add center and edges to the engine PVS
	engine->AddOriginToPVS(ptOrigin + vForwardBump);
	engine->AddOriginToPVS(ptOrigin + vToTopEdge + vToLeftEdge + vForwardBump);
	engine->AddOriginToPVS(ptOrigin + vToTopEdge + vToRightEdge + vForwardBump);
	engine->AddOriginToPVS(ptOrigin + vToBottomEdge + vToLeftEdge + vForwardBump);
	engine->AddOriginToPVS(ptOrigin + vToBottomEdge + vToRightEdge + vForwardBump);
}

void PortalSetupVisibility(CBaseEntity *pPlayer, int area, unsigned char *pvs, int pvssize)
{
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if (iPortalCount == 0)
		return;

	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
	for (int i = 0; i != iPortalCount; ++i)
	{
		CProp_Portal *pPortal = pPortals[i];

		if (pPortal && pPortal->m_bActivated)
		{
			if (pPortal->NetworkProp()->IsInPVS(pPlayer->edict(), pvs, pvssize))
			{
				if (engine->CheckAreasConnected(area, pPortal->NetworkProp()->AreaNum()))
				{
					CProp_Portal *pLinkedPortal = static_cast<CProp_Portal*>(pPortal->m_hLinkedPortal.Get());
					if (pLinkedPortal)
					{
						AddPortalCornersToEnginePVS(pLinkedPortal);
					}
				}
			}
		}
	}
}
#endif
#endif // DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_Player::SetupVisibility(CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize)
{
#ifdef DARKINTERVAL	
#ifdef PORTAL_COMPILE
	BaseClass::SetupVisibility(pViewEntity, pvs, pvssize);

	int area = pViewEntity ? pViewEntity->NetworkProp()->AreaNum() : NetworkProp()->AreaNum();
	PointCameraSetupVisibility(this, area, pvs, pvssize);
	// At this point the EyePosition has been added as a view origin, but if we are currently stuck
	// in a portal, our EyePosition may return a point in solid. Find the reflected eye position
	// and use that as a vis origin instead.
	if (m_hPortalEnvironment)
	{
		CProp_Portal *pPortal = NULL, *pRemotePortal = NULL;
		pPortal = m_hPortalEnvironment;
		pRemotePortal = pPortal->m_hLinkedPortal;

		if (pPortal && pRemotePortal && pPortal->m_bActivated && pRemotePortal->m_bActivated)
		{
			Vector ptPortalCenter = pPortal->GetAbsOrigin();
			Vector vPortalForward;
			pPortal->GetVectors(&vPortalForward, NULL, NULL);

			Vector eyeOrigin = EyePosition();
			Vector vEyeToPortalCenter = ptPortalCenter - eyeOrigin;

			float fPortalDist = vPortalForward.Dot(vEyeToPortalCenter);
			if (fPortalDist > 0.0f) //eye point is behind portal
			{
				// Move eye origin to it's transformed position on the other side of the portal
				UTIL_Portal_PointTransform(pPortal->MatrixThisToLinked(), eyeOrigin, eyeOrigin);

				// Use this as our view origin (as this is where the client will be displaying from)
				engine->AddOriginToPVS(eyeOrigin);
				if (!pViewEntity || pViewEntity->IsPlayer())
				{
					area = engine->GetArea(eyeOrigin);
				}
			}
		}
	}
	PortalSetupVisibility(this, area, pvs, pvssize);
#else
	BaseClass::SetupVisibility(pViewEntity, pvs, pvssize);

	int area = pViewEntity ? pViewEntity->NetworkProp()->AreaNum() : NetworkProp()->AreaNum();
	PointCameraSetupVisibility(this, area, pvs, pvssize);

	// If the intro script is playing, we want to get it's visibility points
	if (g_hIntroScript)
	{
		Vector vecOrigin;
		CBaseEntity *pCamera;
		if (g_hIntroScript->GetIncludedPVSOrigin(&vecOrigin, &pCamera))
		{
			// If it's a point camera, turn it on
			CPointCamera *pPointCamera = dynamic_cast< CPointCamera* >(pCamera);
			if (pPointCamera)
			{
				pPointCamera->SetActive(true);
			}
			engine->AddOriginToPVS(vecOrigin);
		}
	}
#endif
#endif // DARKINTERVAL
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::SuitPower_Update(void)
{
	if (SuitPower_ShouldRecharge())
	{
		SuitPower_Charge(SUITPOWER_CHARGE_RATE * gpGlobals->frametime);
	}
	else if (m_HL2Local.m_bitsActiveDevices)
	{
		float flPowerLoad = m_flSuitPowerLoad;
		
		//Since stickysprint quickly shuts off sprint if it isn't being used, this isn't an issue.
		if (!sv_stickysprint.GetBool())
		{
			if (SuitPower_IsDeviceActive(SuitDeviceSprint))
			{
				if (!fabs(GetAbsVelocity().x) && !fabs(GetAbsVelocity().y))
				{
					// If player's not moving, don't drain sprint juice.
					flPowerLoad -= SuitDeviceSprint.GetDeviceDrainRate();
				}
#ifdef DARKINTERVAL
				else
				{
					float factor;
					factor = 1.0f / m_flSprintPowerDrainScale;
					flPowerLoad -= (SuitDeviceSprint.GetDeviceDrainRate() * (1.0f - factor));
				}
#endif
			}
		}

		if (IsSuitEquipped() && SuitPower_IsDeviceActive(SuitDeviceFlashlight))
		{
			float factor;
			factor = 1.0f / m_flFlashlightPowerDrainScale;
			flPowerLoad -= (SuitDeviceFlashlight.GetDeviceDrainRate() * (1.0f - factor));
		}
#ifdef DARKINTERVAL	
		if (IsSuitEquipped() && SuitPower_IsDeviceActive(SuitDeviceFlashlight) && SuitPower_IsDeviceActive(SuitDeviceBreather))
		{
			flPowerLoad -= SuitDeviceFlashlight.GetDeviceDrainRate() * 1.0f; // if underwater with flashlight on, make it not cost power
		}
#endif
		if ( !SuitPower_Drain(flPowerLoad * gpGlobals->frametime))
		{
			// TURN OFF ALL DEVICES!!
			if (IsSprinting() && !IsSuitEquipped())
			{
				StopSprinting();
			}

			if (Flashlight_UseLegacyVersion())
			{
				if (FlashlightIsOn())
					FlashlightTurnOff();
			}
		}

		if (Flashlight_UseLegacyVersion())
		{
			// turn off flashlight a little bit after it hits below one aux power notch (5%)
			if (m_HL2Local.m_flSuitPower < 4.8f && FlashlightIsOn())
				FlashlightTurnOff();
		}
	}
}

//-----------------------------------------------------------------------------
// Charge battery fully, turn off all devices.
//-----------------------------------------------------------------------------
void CHL2_Player::SuitPower_Initialize(void)
{
	m_HL2Local.m_bitsActiveDevices = 0x00000000;
	m_HL2Local.m_flSuitPower = 100.0;
	m_flSuitPowerLoad = 0.0;
}

//-----------------------------------------------------------------------------
// Purpose: Interface to drain power from the suit's power supply.
// Input:	Amount of charge to remove (expressed as percentage of full charge)
// Output:	Returns TRUE if successful, FALSE if not enough power available.
//-----------------------------------------------------------------------------
bool CHL2_Player::SuitPower_Drain(float flPower)
{
	// Suitpower cheat on?
	if (sv_infinite_aux_power.GetBool())
		return true;

	m_HL2Local.m_flSuitPower -= flPower;

	if (m_HL2Local.m_flSuitPower < 0.0)
	{
		// Power is depleted!
		// Clamp and fail
		m_HL2Local.m_flSuitPower = 0.0;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Interface to add power to the suit's power supply
// Input:	Amount of charge to add
//-----------------------------------------------------------------------------
void CHL2_Player::SuitPower_Charge(float flPower)
{
	m_HL2Local.m_flSuitPower += flPower;

	if (m_HL2Local.m_flSuitPower > 100.0)
	{
		// Full charge, clamp.
		m_HL2Local.m_flSuitPower = 100.0;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CHL2_Player::SuitPower_IsDeviceActive(const CSuitPowerDevice &device)
{
	return (m_HL2Local.m_bitsActiveDevices & device.GetDeviceID()) != 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CHL2_Player::SuitPower_AddDevice(const CSuitPowerDevice &device)
{
	// Make sure this device is NOT active!!
	if (m_HL2Local.m_bitsActiveDevices & device.GetDeviceID())
		return false;
#ifdef DARKINTERVAL	// because the player has flashlight and sprint before getting the HEV
	if( !IsSuitEquipped() && !m_Local.m_bSuitlessHUDVisible )
	return false;
#endif
	m_HL2Local.m_bitsActiveDevices |= device.GetDeviceID();
	m_flSuitPowerLoad += device.GetDeviceDrainRate();
	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CHL2_Player::SuitPower_RemoveDevice(const CSuitPowerDevice &device)
{
	// Make sure this device is active!!
	if (!(m_HL2Local.m_bitsActiveDevices & device.GetDeviceID()))
		return false;
#ifdef DARKINTERVAL	// because the player has flashlight and sprint before getting the HEV
	if( !IsSuitEquipped() && !m_Local.m_bSuitlessHUDVisible )
	return false;
#endif
	// Take a little bit of suit power when you disable a device. If the device is shutting off
	// because the battery is drained, no harm done, the battery charge cannot go below 0. 
	// This code in combination with the delay before the suit can start recharging are a defense
	// against exploits where the player could rapidly tap sprint and never run out of power.
	SuitPower_Drain(device.GetDeviceDrainRate() * 0.1f);

	m_HL2Local.m_bitsActiveDevices &= ~device.GetDeviceID();
	m_flSuitPowerLoad -= device.GetDeviceDrainRate();
	if (m_flSuitPowerLoad < 0) m_flSuitPowerLoad = 0;

	if (m_HL2Local.m_bitsActiveDevices == 0x00000000)
	{
		// With this device turned off, we can set this timer which tells us when the
		// suit power system entered a no-load state.
		m_flTimeAllSuitDevicesOff = CURTIME;
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define SUITPOWER_BEGIN_RECHARGE_DELAY	0.5f
bool CHL2_Player::SuitPower_ShouldRecharge(void)
{
	// Make sure all devices are off.
	if (m_HL2Local.m_bitsActiveDevices != 0x00000000)
		return false;

	// Is the system fully charged?
	if (m_HL2Local.m_flSuitPower >= 100.0f)
		return false;

	// Has the system been in a no-load state for long enough
	// to begin recharging?
	if (CURTIME < m_flTimeAllSuitDevicesOff + SUITPOWER_BEGIN_RECHARGE_DELAY)
		return false;

	return true;
}
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// This monitors battery level and lets the suit comment the precise level
//-----------------------------------------------------------------------------
void CHL2_Player::ContextThink_BatteryPickup()
{
	if( m_flLastBatteryTime < 0  )		
		SetContextThink(NULL, 0, PLAYER_CONTEXT_BATTERY_PICKUP);

	if (CURTIME >= m_flLastBatteryTime + 0.5f && m_flLastBatteryTime > 0)
	{
		const float MAX_NORMAL_BATTERY = 100;
		int pct;
		char szcharge[64];

		// Suit reports new power level, without rounding the value
		int currentSuitLevel = min(200,ArmorValue()); // 200 is possible w/ Citadel chargers 
		pct = (int)((float)(ArmorValue() * 100.0) * (1.0 / MAX_NORMAL_BATTERY) + 1.0);
		if (pct < 9)
			pct = 0;
		if (pct > 0)
			pct--;

		if (currentSuitLevel <= 100)
		{
			Q_snprintf(szcharge, sizeof(szcharge), "!HEV_%1dP", pct);

			SetSuitUpdate(szcharge, FALSE, 10);
		}
		else
		{
			Q_snprintf(szcharge, sizeof(szcharge), "!HEV_OVER100", pct);
			SetSuitUpdate(szcharge, FALSE, 10);
		}

		m_flLastBatteryTime = -1.0f;
	}

	SetContextThink(&CHL2_Player::ContextThink_BatteryPickup, CURTIME + 0.1f, PLAYER_CONTEXT_BATTERY_PICKUP);
}
#endif // DARKINTERVAL
ConVar	sk_battery("sk_battery", "0");
#ifdef DARKINTERVAL
bool CHL2_Player::ApplyBattery(float powerMultiplier, bool bSilent, bool bFromBattery)
#else
bool CHL2_Player::ApplyBattery(float powerMultiplier)
#endif
{
	const float MAX_NORMAL_BATTERY = 100;
#ifdef DARKINTERVAL
	if ( IsSuitEquipped())
#endif
	{
#ifdef DARKINTERVAL
		if (ArmorValue() < MAX_NORMAL_BATTERY)
#else
		if ( ( ArmorValue() < MAX_NORMAL_BATTERY ) && IsSuitEquipped() )
#endif
		{
#ifdef DARKINTERVAL
			IncrementArmorValue(sk_battery.GetFloat() * powerMultiplier, MAX_NORMAL_BATTERY * 2);
#else
			int pct;
			char szcharge[ 64 ];
			IncrementArmorValue(sk_battery.GetFloat() * powerMultiplier, MAX_NORMAL_BATTERY);
#endif
#ifdef DARKINTERVAL
			if (!bSilent)
#endif
			{
				CPASAttenuationFilter filter(this, "ItemBattery.Touch");
				EmitSound(filter, entindex(), "ItemBattery.Touch");

				CSingleUserRecipientFilter user(this);
				user.MakeReliable();

				UserMessageBegin(user, "ItemPickup");
				WRITE_STRING("item_battery");
				MessageEnd();
			}
#ifdef DARKINTERVAL
			/* // moved in ContextThink_BatteryPickup above
			if (!m_bSuitCommentsDisabled)
			{
				// Suit reports new power level, without rounding the value
				pct = (int)((float)(ArmorValue() * 100.0) * (1.0 / MAX_NORMAL_BATTERY) + 1.0);
				if (pct < 9)
					pct = 0;
				if (pct > 0)
					pct--;

				Q_snprintf(szcharge, sizeof(szcharge), "!HEV_%1dP", pct);

				SetSuitUpdate(szcharge, FALSE, 10);
			}
			*/	

			if ( !m_bSuitCommentsDisabled )
			{
				m_flLastBatteryTime = CURTIME;
				SetContextThink(&CHL2_Player::ContextThink_BatteryPickup, CURTIME + 0.1f, PLAYER_CONTEXT_BATTERY_PICKUP);
			}

			return true; // tells the battery to remove itself
#else
			// Suit reports new power level
			// For some reason this wasn't working in release build -- round it.
			pct = ( int )( ( float )( ArmorValue() * 100.0 ) * ( 1.0 / MAX_NORMAL_BATTERY ) + 0.5 );
			pct = ( pct / 5 );
			if ( pct > 0 )
				pct--;

			Q_snprintf(szcharge, sizeof(szcharge), "!HEV_%1dP", pct);

			//UTIL_EmitSoundSuit(edict(), szcharge);
			//SetSuitUpdate(szcharge, FALSE, SUIT_NEXT_IN_30SEC);
			return true;
#endif // DARKINTERVAL
		}
#ifdef DARKINTERVAL
		else
		{
			// Charge is at 100, but we still want to process the event to speak the line.
			// Unless we're trying to touch a battery. Don't make the suit speak in this case.

			if (!m_bSuitCommentsDisabled && !bFromBattery)
			{
				m_flLastBatteryTime = CURTIME;
				SetContextThink(&CHL2_Player::ContextThink_BatteryPickup, CURTIME + 0.1f, PLAYER_CONTEXT_BATTERY_PICKUP);

			}
			/* // moved in ContextThink_BatteryPickup above
			if (!m_bSuitCommentsDisabled && !bFromBattery)
			{
				// Suit reports new power level, without rounding the value
				pct = (int)((float)(ArmorValue() * 100.0) * (1.0 / MAX_NORMAL_BATTERY) + 1.0);
				if (pct < 9)
					pct = 0;
				if (pct > 0)
					pct--;

				Q_snprintf(szcharge, sizeof(szcharge), "!HEV_%1dP", pct);

				SetSuitUpdate(szcharge, FALSE, 10);
			}
			*/
			return false; // don't remove the battery.
		}	
#endif // DARKINTERVAL
	}
	return false; // don't remove the battery.
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CHL2_Player::FlashlightIsOn(void)
{
	return IsEffectActive(EF_DIMLIGHT);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::FlashlightTurnOn(void)
{
	if (m_bFlashlightDisabled)
		return;

	if (Flashlight_UseLegacyVersion())
	{
		if (!SuitPower_AddDevice(SuitDeviceFlashlight))
			return;
	}
#ifndef DARKINTERVAL
#ifdef HL2_DLL
		if( !IsSuitEquipped() )
			return;
#endif
#endif // DARKINTERVAL
	AddEffects(EF_DIMLIGHT);

	EmitSound("HL2Player.FlashLightOn");

	variant_t flashlighton;
	flashlighton.SetFloat(m_HL2Local.m_flSuitPower / 100.0f);
	FirePlayerProxyOutput("OnFlashlightOn", flashlighton, this, this);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::FlashlightTurnOff(void)
{
	if (Flashlight_UseLegacyVersion())
	{
		if (!SuitPower_RemoveDevice(SuitDeviceFlashlight))
			return;
	}

	RemoveEffects(EF_DIMLIGHT);

	EmitSound("HL2Player.FlashLightOff");

	variant_t flashlightoff;
	flashlightoff.SetFloat(m_HL2Local.m_flSuitPower / 100.0f);
	FirePlayerProxyOutput("OnFlashlightOff", flashlightoff, this, this);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define FLASHLIGHT_RANGE	Square(600)
bool CHL2_Player::IsIlluminatedByFlashlight(CBaseEntity *pEntity, float *flReturnDot)
{
	if (!FlashlightIsOn())
		return false;

	if (pEntity->Classify() == CLASS_BARNACLE && pEntity->GetEnemy() == this)
	{
		// As long as my flashlight is on, the barnacle that's pulling me in is considered illuminated.
		// This is because players often shine their flashlights at Alyx when they are in a barnacle's 
		// grasp, and wonder why Alyx isn't helping. Alyx isn't helping because the light isn't pointed
		// at the barnacle. This will allow Alyx to see the barnacle no matter which way the light is pointed.
		return true;
	}

	// Within 50 feet?
	float flDistSqr = GetAbsOrigin().DistToSqr(pEntity->GetAbsOrigin());
	if (flDistSqr > FLASHLIGHT_RANGE)
		return false;

	// Within 45 degrees?
	Vector vecSpot = pEntity->WorldSpaceCenter();
	Vector los;

	// If the eyeposition is too close, move it back. Solves problems
	// caused by the player being too close the target.
	if (flDistSqr < (128 * 128))
	{
		Vector vecForward;
		EyeVectors(&vecForward);
		Vector vecMovedEyePos = EyePosition() - (vecForward * 128);
		los = (vecSpot - vecMovedEyePos);
	}
	else
	{
		los = (vecSpot - EyePosition());
	}

	VectorNormalize(los);
	Vector facingDir = EyeDirection3D();
	float flDot = DotProduct(los, facingDir);

	if (flReturnDot)
	{
		*flReturnDot = flDot;
	}

	if (flDot < 0.92387f)
		return false;

	if (!FVisible(pEntity))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Let NPCs know when the flashlight is trained on them
//-----------------------------------------------------------------------------
void CHL2_Player::CheckFlashlight(void)
{
	if (!FlashlightIsOn())
		return;

#ifndef DARKINTERVAL // todo - no cases in DI of relying on this, may come back later
	if (m_flNextFlashlightCheckTime > CURTIME)
		return;
	m_flNextFlashlightCheckTime = CURTIME + FLASHLIGHT_NPC_CHECK_INTERVAL;

	// Loop through NPCs looking for illuminated ones
	for (int i = 0; i < g_AI_Manager.NumAIs(); i++)
	{
		CAI_BaseNPC *pNPC = g_AI_Manager.AccessAIs()[i];

		float flDot;

		if (IsIlluminatedByFlashlight(pNPC, &flDot))
		{
			pNPC->PlayerHasIlluminatedNPC(this, flDot);
		}
	}
#endif
}
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CHL2_Player::ToggleSprintIsOn(void)
{
	return always_run.GetBool();
}

void CHL2_Player::ToggleSprintTurnOn(void)
{	
	ConVarRef alwaysRun("always_run");
	if (alwaysRun.IsValid())
	{
		alwaysRun.SetValue(true);
	}
}

void CHL2_Player::ToggleSprintTurnOff(void)
{
	ConVarRef alwaysRun("always_run");
	if (alwaysRun.IsValid())
	{
		alwaysRun.SetValue(false);
	}
}
#endif // DARKINTERVAL
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::SetPlayerUnderwater(bool state)
{
	if (state)
	{
		SuitPower_AddDevice(SuitDeviceBreather);
	}
	else
	{
		SuitPower_RemoveDevice(SuitDeviceBreather);
	}

	BaseClass::SetPlayerUnderwater(state);
}

//-----------------------------------------------------------------------------
bool CHL2_Player::PassesDamageFilter(const CTakeDamageInfo &info)
{
	CBaseEntity *pAttacker = info.GetAttacker();
	if (pAttacker && pAttacker->MyNPCPointer() && pAttacker->MyNPCPointer()->IsPlayerAlly())
	{
		return false;
	}

	if (m_hPlayerProxy && !m_hPlayerProxy->PassesDamageFilter(info))
	{
		return false;
	}

	return BaseClass::PassesDamageFilter(info);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_Player::SetFlashlightEnabled(bool bState)
{
	m_bFlashlightDisabled = !bState;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::InputDisableFlashlight(inputdata_t &inputdata)
{
	if (FlashlightIsOn())
		FlashlightTurnOff();

	SetFlashlightEnabled(false);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::InputEnableFlashlight(inputdata_t &inputdata)
{
	SetFlashlightEnabled(true);
}
#ifdef DARKINTERVAL
void CHL2_Player::InputEnableFlashlightFlicker(inputdata_t &inputdata)
{
	m_HL2Local.m_flashlight_force_flicker_bool = true;
}

void CHL2_Player::InputDisableFlashlightFlicker(inputdata_t &inputdata)
{
	m_HL2Local.m_flashlight_force_flicker_bool = false;
}

void CHL2_Player::SetForceFlashlightFlicker(bool force)
{
	m_HL2Local.m_flashlight_force_flicker_bool = force;
}

void CHL2_Player::InputDisableLocator(inputdata_t &inputdata)
{
	m_Local.m_bLocatorVisible = false;
}

void CHL2_Player::InputEnableLocator(inputdata_t &inputdata)
{
	m_Local.m_bLocatorVisible = true;
}

void CHL2_Player::SetSuitCommentsEnabled(bool bState)
{
	m_bSuitCommentsDisabled = !bState;
}
#endif // DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: Prevent the player from taking fall damage for [n] seconds, but
// reset back to taking fall damage after the first impact (so players will be
// hurt if they bounce off what they hit). This is the original behavior.
//-----------------------------------------------------------------------------
void CHL2_Player::InputIgnoreFallDamage(inputdata_t &inputdata)
{
	float timeToIgnore = inputdata.value.Float();

	if (timeToIgnore <= 0.0)
		timeToIgnore = TIME_IGNORE_FALL_DAMAGE;

	m_flTimeIgnoreFallDamage = CURTIME + timeToIgnore;
	m_bIgnoreFallDamageResetAfterImpact = true;
}


//-----------------------------------------------------------------------------
// Purpose: Absolutely prevent the player from taking fall damage for [n] seconds. 
//-----------------------------------------------------------------------------
void CHL2_Player::InputIgnoreFallDamageWithoutReset(inputdata_t &inputdata)
{
	float timeToIgnore = inputdata.value.Float();

	if (timeToIgnore <= 0.0)
		timeToIgnore = TIME_IGNORE_FALL_DAMAGE;

	m_flTimeIgnoreFallDamage = CURTIME + timeToIgnore;
	m_bIgnoreFallDamageResetAfterImpact = false;
}

//-----------------------------------------------------------------------------
// Purpose: Notification of a player's npc ally in the players squad being killed
//-----------------------------------------------------------------------------
void CHL2_Player::OnSquadMemberKilled(inputdata_t &data)
{
	// send a message to the client, to notify the hud of the loss
	CSingleUserRecipientFilter user(this);
	user.MakeReliable();
	UserMessageBegin(user, "SquadMemberDied");
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_Player::NotifyFriendsOfDamage(CBaseEntity *pAttackerEntity)
{
	CAI_BaseNPC *pAttacker = pAttackerEntity->MyNPCPointer();
	if (pAttacker)
	{
		const Vector &origin = GetAbsOrigin();
		for (int i = 0; i < g_AI_Manager.NumAIs(); i++)
		{
			const float NEAR_Z = 12 * 12;
			const float NEAR_XY_SQ = Square(50 * 12);
			CAI_BaseNPC *pNpc = g_AI_Manager.AccessAIs()[i];
			if (pNpc->IsPlayerAlly())
			{
				const Vector &originNpc = pNpc->GetAbsOrigin();
				if (fabsf(originNpc.z - origin.z) < NEAR_Z)
				{
					if ((originNpc.AsVector2D() - origin.AsVector2D()).LengthSqr() < NEAR_XY_SQ)
					{
						pNpc->OnFriendDamaged(this, pAttacker);
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ConVar test_massive_dmg("test_massive_dmg", "30");
ConVar test_massive_dmg_clip("test_massive_dmg_clip", "0.5");
int	CHL2_Player::OnTakeDamage(const CTakeDamageInfo &info)
{
	if (GlobalEntity_GetState("gordon_invulnerable") == GLOBAL_ON)
		return 0;

	// ignore fall damage if instructed to do so by input
	if ((info.GetDamageType() & DMG_FALL) && m_flTimeIgnoreFallDamage > CURTIME)
	{
		// usually, we will reset the input flag after the first impact. However there is another input that
		// prevents this behavior.
		if (m_bIgnoreFallDamageResetAfterImpact)
		{
			m_flTimeIgnoreFallDamage = 0;
		}
		return 0;
	}
#ifdef DARKINTERVAL
	if (info.GetDamageType() & DMG_SONIC)
	{
		if (info.GetDamage() >= 5)
		{
			int dsp = snd_disable_tinnitus.GetBool() ? RandomInt(32,34) : RandomInt(35, 37);
			CSingleUserRecipientFilter user(this);
			enginesound->SetPlayerDSP(user, dsp, false);
		}
	//	if( info.GetDamage() < 10) // let us have damage-less sonic attacks that only inflict hearing loss.
	//		return 0;
	}
#endif 
	if (info.GetDamageType() & DMG_BLAST_SURFACE)
	{
		if (GetWaterLevel() > 2)
		{
			// Don't take blast damage from anything above the surface.
			if (info.GetInflictor()->GetWaterLevel() == 0)
			{
				return 0;
			}
		}
	}

	if (info.GetDamage() > 0.0f)
	{
		m_flLastDamageTime = CURTIME;

		if (info.GetAttacker())
			NotifyFriendsOfDamage(info.GetAttacker());
	}

	// Modify the amount of damage the player takes, based on skill.
	CTakeDamageInfo playerDamage = info;

	// Should we run this damage through the skill level adjustment?
	bool bAdjustForSkillLevel = true;

	if (info.GetDamageType() == DMG_GENERIC && info.GetAttacker() == this && info.GetInflictor() == this)
	{
		// Only do a skill level adjustment if the player isn't his own attacker AND inflictor.
		// This prevents damage from SetHealth() inputs from being adjusted for skill level.
		bAdjustForSkillLevel = false;
	}

	if (GetVehicleEntity() != NULL && GlobalEntity_GetState("gordon_protect_driver") == GLOBAL_ON)
	{
		if (playerDamage.GetDamage() > test_massive_dmg.GetFloat() && playerDamage.GetInflictor() == GetVehicleEntity() && (playerDamage.GetDamageType() & DMG_CRUSH))
		{
			playerDamage.ScaleDamage(test_massive_dmg_clip.GetFloat() / playerDamage.GetDamage());
		}
	}

	if (bAdjustForSkillLevel)
	{
		playerDamage.AdjustPlayerDamageTakenForSkillLevel();
	}

	gamestats->Event_PlayerDamage(this, info);

	return BaseClass::OnTakeDamage(playerDamage);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
int CHL2_Player::OnTakeDamage_Alive(const CTakeDamageInfo &info)
{
	// Drown
	if (info.GetDamageType() & DMG_DROWN)
	{
		if (m_idrowndmg == m_idrownrestored)
		{
			EmitSound("Player.DrownStart");
		}
		else
		{
			EmitSound("Player.DrownContinue");
		}
	}

	// Burnt
	if (info.GetDamageType() & DMG_BURN)
	{
		EmitSound("HL2Player.BurnPain");
	}


	if ((info.GetDamageType() & DMG_SLASH))
	{
		if (m_afPhysicsFlags & PFLAG_USING)
		{
			// Stop the player using a rotating button for a short time if hit by a creature's melee attack.
			// This is for the antlion burrow-corking training in EP1 (sjb).
			SuspendUse(0.5f);
		}
	}
#ifdef DARKINTERVAL
	// DI NEW - lost health to something. Output the attacker, if there is one
	FirePlayerProxyOutput("PlayerDamaged", variant_t(), info.GetAttacker(), this);
#endif
	// Call the base class implementation
	return BaseClass::OnTakeDamage_Alive(info);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::OnDamagedByExplosion(const CTakeDamageInfo &info)
{
	if (info.GetInflictor() && info.GetInflictor()->ClassMatches("mortarshell"))
	{
		// No ear ringing for mortar
		UTIL_ScreenShake(info.GetInflictor()->GetAbsOrigin(), 4.0, 1.0, 0.5, 1000, SHAKE_START, false);
		return;
	}
	BaseClass::OnDamagedByExplosion(info);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CHL2_Player::ShouldShootMissTarget(CBaseCombatCharacter *pAttacker)
{
	if (CURTIME > m_flTargetFindTime)
	{
		// Put this off into the future again.
		m_flTargetFindTime = CURTIME + random->RandomFloat(3, 5);
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Notifies Alyx that player has put a combine ball into a socket so she can comment on it.
// Input  : pCombineBall - ball the was socketed
//-----------------------------------------------------------------------------
void CHL2_Player::CombineBallSocketed(CPropCombineBall *pCombineBall)
{
#ifdef HL2_EPISODIC
	CNPC_Alyx *pAlyx = CNPC_Alyx::GetAlyx();
	if ( pAlyx )
	{
		pAlyx->CombineBallSocketed(pCombineBall->NumBounces());
	}
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::Event_KilledOther(CBaseEntity *pVictim, const CTakeDamageInfo &info)
{
	BaseClass::Event_KilledOther(pVictim, info);

#ifdef HL2_EPISODIC

	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();

	for (int i = 0; i < g_AI_Manager.NumAIs(); i++)
	{
		if (ppAIs[i] && ppAIs[i]->IRelationType(this) == D_LI)
		{
			ppAIs[i]->OnPlayerKilledOther(pVictim, info);
		}
	}

#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::Event_Killed(const CTakeDamageInfo &info)
{
	BaseClass::Event_Killed(info);

	FirePlayerProxyOutput("PlayerDied", variant_t(), this, this);
	NotifyScriptsOfDeath();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::NotifyScriptsOfDeath(void)
{
	CBaseEntity *pEnt = gEntList.FindEntityByClassname(NULL, "scripted_sequence");

	while (pEnt)
	{
		variant_t emptyVariant;
		pEnt->AcceptInput("ScriptPlayerDeath", NULL, NULL, emptyVariant, 0);

		pEnt = gEntList.FindEntityByClassname(pEnt, "scripted_sequence");
	}

	pEnt = gEntList.FindEntityByClassname(NULL, "logic_choreographed_scene");

	while (pEnt)
	{
		variant_t emptyVariant;
		pEnt->AcceptInput("ScriptPlayerDeath", NULL, NULL, emptyVariant, 0);

		pEnt = gEntList.FindEntityByClassname(pEnt, "logic_choreographed_scene");
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_Player::GetAutoaimVector(autoaim_params_t &params)
{
	BaseClass::GetAutoaimVector(params);
#ifdef DARKINTERVAL
	if (true) // DI todo - investigate
	{
		if (IsInAVehicle())
		{
			if (m_hLockedAutoAimEntity && m_hLockedAutoAimEntity->IsAlive() && ShouldKeepLockedAutoaimTarget(m_hLockedAutoAimEntity))
			{
				if (params.m_hAutoAimEntity && params.m_hAutoAimEntity != m_hLockedAutoAimEntity)
				{
					// Autoaim has picked a new target. Switch.
					m_hLockedAutoAimEntity = params.m_hAutoAimEntity;
				}

				// Ignore autoaim and just keep aiming at this target.
				params.m_hAutoAimEntity = m_hLockedAutoAimEntity;
				Vector vecTarget = m_hLockedAutoAimEntity->BodyTarget(EyePosition(), false);
				Vector vecDir = vecTarget - EyePosition();
				VectorNormalize(vecDir);

				params.m_vecAutoAimDir = vecDir;
				params.m_vecAutoAimPoint = vecTarget;
				return;
			}
			else
			{
				m_hLockedAutoAimEntity = NULL;
			}
		}

		// If the player manually gets his crosshair onto a target, make that target sticky
		if (params.m_fScale != AUTOAIM_SCALE_DIRECT_ONLY)
		{
			// Only affect this for 'real' queries
			//if( params.m_hAutoAimEntity && params.m_bOnTargetNatural )
			if (params.m_hAutoAimEntity)
			{
				// Turn on sticky.
				m_HL2Local.m_bStickyAutoAim = true;

				if (IsInAVehicle())
				{
					m_hLockedAutoAimEntity = params.m_hAutoAimEntity;
				}
			}
			else if (!params.m_hAutoAimEntity)
			{
				// Turn off sticky only if there's no target at all.
				m_HL2Local.m_bStickyAutoAim = false;

				m_hLockedAutoAimEntity = NULL;
			}
		}
	}
#endif // DARKINTERVAL
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CHL2_Player::ShouldKeepLockedAutoaimTarget(EHANDLE hLockedTarget)
{
	Vector vecLooking;
	Vector vecToTarget;

	vecToTarget = hLockedTarget->WorldSpaceCenter() - EyePosition();
	float flDist = vecToTarget.Length2D();
	VectorNormalize(vecToTarget);

	if (flDist > autoaim_max_dist.GetFloat())
		return false;

	float flDot;

	vecLooking = EyeDirection3D();
	flDot = DotProduct(vecLooking, vecToTarget);

	if (flDot < autoaim_unlock_target.GetFloat())
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iCount - 
//			iAmmoIndex - 
//			bSuppressSound - 
// Output : int
//-----------------------------------------------------------------------------
int CHL2_Player::GiveAmmo(int nCount, int nAmmoIndex, bool bSuppressSound)
{
	// Don't try to give the player invalid ammo indices.
	if (nAmmoIndex < 0)
		return 0;

	bool bCheckAutoSwitch = false;

#ifndef DARKINTERVAL	// DI REMOVE - Remove the super annoying weapon auto-switch on pickup.
	if (!HasAnyAmmoOfType(nAmmoIndex))
	{
	bCheckAutoSwitch = true;
	}
#endif
	int nAdd = BaseClass::GiveAmmo(nCount, nAmmoIndex, bSuppressSound);

	if (nCount > 0 && nAdd == 0)
	{
		// we've been denied the pickup, display a hud icon to show that
		CSingleUserRecipientFilter user(this);
		user.MakeReliable();
		UserMessageBegin(user, "AmmoDenied");
		WRITE_SHORT(nAmmoIndex);
		MessageEnd();
	}

	if (bCheckAutoSwitch)
	{
		CBaseCombatWeapon *pWeapon = g_pGameRules->GetNextBestWeapon(this, GetActiveWeapon());

		if (pWeapon && pWeapon->GetPrimaryAmmoType() == nAmmoIndex)
		{
			SwitchToNextBestWeapon(GetActiveWeapon());
		}
	}

	return nAdd;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CHL2_Player::Weapon_CanUse(CBaseCombatWeapon *pWeapon)
{
	if (pWeapon->ClassMatches("weapon_stunstick"))
	{
#ifdef DARKINTERVAL // balance
		if (ApplyBattery(0.3))
#else
		if ( ApplyBattery(0.5) )
#endif
			UTIL_Remove(pWeapon);
		return false;
	}
	return BaseClass::Weapon_CanUse(pWeapon);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pWeapon - 
//-----------------------------------------------------------------------------
void CHL2_Player::Weapon_Equip(CBaseCombatWeapon *pWeapon)
{
#if	HL2_SINGLE_PRIMARY_WEAPON_MODE

	if (pWeapon->GetSlot() == WEAPON_PRIMARY_SLOT)
	{
		Weapon_DropSlot(WEAPON_PRIMARY_SLOT);
	}

#endif

	if (GetActiveWeapon() == NULL)
	{
		m_HL2Local.m_bWeaponLowered = false;
	}

	BaseClass::Weapon_Equip(pWeapon);
}

//-----------------------------------------------------------------------------
// Purpose: Player reacts to bumping a weapon. 
// Input  : pWeapon - the weapon that the player bumped into.
// Output : Returns true if player picked up the weapon
//-----------------------------------------------------------------------------
bool CHL2_Player::BumpWeapon(CBaseCombatWeapon *pWeapon)
{
#if	HL2_SINGLE_PRIMARY_WEAPON_MODE

	CBaseCombatCharacter *pOwner = pWeapon->GetOwner();

	// Can I have this weapon type?
	if (pOwner || !Weapon_CanUse(pWeapon) || !g_pGameRules->CanHavePlayerItem(this, pWeapon))
	{
		if (gEvilImpulse101)
		{
			UTIL_Remove(pWeapon);
		}
		return false;
	}

	// ----------------------------------------
	// If I already have it just take the ammo
	// ----------------------------------------
	if (Weapon_OwnsThisType(pWeapon->GetClassname(), pWeapon->GetSubType()))
	{
		//Only remove the weapon if we attained ammo from it
		if (Weapon_EquipAmmoOnly(pWeapon) == false)
			return false;

		// Only remove me if I have no ammo left
		// Can't just check HasAnyAmmo because if I don't use clips, I want to be removed, 
		if (pWeapon->UsesClipsForAmmo1() && pWeapon->HasPrimaryAmmo())
			return false;

		UTIL_Remove(pWeapon);
		return false;
	}
	// -------------------------
	// Otherwise take the weapon
	// -------------------------
	else
	{
		//Make sure we're not trying to take a new weapon type we already have
		if (Weapon_SlotOccupied(pWeapon))
		{
			CBaseCombatWeapon *pActiveWeapon = Weapon_GetSlot(WEAPON_PRIMARY_SLOT);

			if (pActiveWeapon != NULL && pActiveWeapon->HasAnyAmmo() == false && Weapon_CanSwitchTo(pWeapon))
			{
				Weapon_Equip(pWeapon);
				return true;
			}

			//Attempt to take ammo if this is the gun we're holding already
			if (Weapon_OwnsThisType(pWeapon->GetClassname(), pWeapon->GetSubType()))
			{
				Weapon_EquipAmmoOnly(pWeapon);
			}

			return false;
		}

		pWeapon->CheckRespawn();

		pWeapon->AddSolidFlags(FSOLID_NOT_SOLID);
		pWeapon->AddEffects(EF_NODRAW);

		Weapon_Equip(pWeapon);

		EmitSound("HL2Player.PickupWeapon");

		return true;
	}
#else

	return BaseClass::BumpWeapon(pWeapon);

#endif

}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cmd - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHL2_Player::ClientCommand(const CCommand &args)
{
#if	HL2_SINGLE_PRIMARY_WEAPON_MODE

	//Drop primary weapon
	if (!Q_stricmp(args[0], "DropPrimary"))
	{
		Weapon_DropSlot(WEAPON_PRIMARY_SLOT);
		return true;
	}

#endif

	if (!Q_stricmp(args[0], "emit"))
	{
		CSingleUserRecipientFilter filter(this);
		if (args.ArgC() > 1)
		{
			EmitSound(filter, entindex(), args[1]);
		}
		else
		{
			EmitSound(filter, entindex(), "Test.Sound");
		}
		return true;
	}

	return BaseClass::ClientCommand(args);
}
#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
void CHL2_Player::ShutdownUseEntity(void)
{
	ShutdownPickupController(m_hUseEntity);
}

const Vector& CHL2_Player::WorldSpaceCenter() const
{
	m_vWorldSpaceCenterHolder = GetAbsOrigin();
	m_vWorldSpaceCenterHolder.z += ((IsDucked()) ? (VEC_DUCK_HULL_MAX.z) : (VEC_HULL_MAX.z)) * 0.5f;
	return m_vWorldSpaceCenterHolder;
}

void CHL2_Player::Teleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity)
{
	Vector oldOrigin = GetLocalOrigin();
	QAngle oldAngles = GetLocalAngles();
	BaseClass::Teleport(newPosition, newAngles, newVelocity);
	m_angEyeAngles = pl.v_angle;
}

void CHL2_Player::VPhysicsShadowUpdate(IPhysicsObject *pPhysics)
{
	if (m_hPortalEnvironment.Get() == NULL)
		return BaseClass::VPhysicsShadowUpdate(pPhysics);


	//below is mostly a cut/paste of existing CBasePlayer::VPhysicsShadowUpdate code with some minor tweaks to avoid getting stuck in stuff when in a portal environment
	Vector newPosition;

	bool physicsUpdated = m_pPhysicsController->GetShadowPosition(&newPosition, NULL) > 0 ? true : false;

	// UNDONE: If the player is penetrating, but the player's game collisions are not stuck, teleport the physics shadow to the game position
	if (pPhysics->GetGameFlags() & FVPHYSICS_PENETRATING)
	{
		CUtlVector<CBaseEntity *> list;
		PhysGetListOfPenetratingEntities(this, list);
		for (int i = list.Count() - 1; i >= 0; --i)
		{
			// filter out anything that isn't simulated by vphysics
			// UNDONE: Filter out motion disabled objects?
			if (list[i]->GetMoveType() == MOVETYPE_VPHYSICS)
			{
				// I'm currently stuck inside a moving object, so allow vphysics to 
				// apply velocity to the player in order to separate these objects
				m_touchedPhysObject = true;
			}
		}
	}

	if (m_pPhysicsController->IsInContact() || (m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER))
	{
		m_touchedPhysObject = true;
	}

	if (IsFollowingPhysics())
	{
		m_touchedPhysObject = true;
	}

	if (GetMoveType() == MOVETYPE_NOCLIP)
	{
		m_oldOrigin = GetAbsOrigin();
		return;
	}

	if (phys_timescale.GetFloat() == 0.0f)
	{
		physicsUpdated = false;
	}

	if (!physicsUpdated)
		return;

	IPhysicsObject *pPhysGround = GetGroundVPhysics();

	Vector newVelocity;
	pPhysics->GetPosition(&newPosition, 0);
	m_pPhysicsController->GetShadowVelocity(&newVelocity);



	Vector tmp = GetAbsOrigin() - newPosition;
	if (!m_touchedPhysObject && !(GetFlags() & FL_ONGROUND))
	{
		tmp.z *= 0.5f;	// don't care about z delta as much
	}

	float dist = tmp.LengthSqr();
	float deltaV = (newVelocity - GetAbsVelocity()).LengthSqr();

	float maxDistErrorSqr = VPHYS_MAX_DISTSQR;
	float maxVelErrorSqr = VPHYS_MAX_VELSQR;
	if (IsRideablePhysics(pPhysGround))
	{
		maxDistErrorSqr *= 0.25;
		maxVelErrorSqr *= 0.25;
	}

	if (dist >= maxDistErrorSqr || deltaV >= maxVelErrorSqr || (pPhysGround && !m_touchedPhysObject))
	{
		if (m_touchedPhysObject || pPhysGround)
		{
			// BUGBUG: Rewrite this code using fixed timestep
			if (deltaV >= maxVelErrorSqr)
			{
				Vector dir = GetAbsVelocity();
				float len = VectorNormalize(dir);
				float dot = DotProduct(newVelocity, dir);
				if (dot > len)
				{
					dot = len;
				}
				else if (dot < -len)
				{
					dot = -len;
				}

				VectorMA(newVelocity, -dot, dir, newVelocity);

				if (m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER)
				{
					float val = Lerp(0.1f, len, dot);
					VectorMA(newVelocity, val - len, dir, newVelocity);
				}

				if (!IsRideablePhysics(pPhysGround))
				{
					if (!(m_afPhysicsFlags & PFLAG_VPHYSICS_MOTIONCONTROLLER) && IsSimulatingOnAlternateTicks())
					{
						newVelocity *= 0.5f;
					}
					ApplyAbsVelocityImpulse(newVelocity);
				}
			}

			trace_t trace;
			UTIL_TraceEntity(this, newPosition, newPosition, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace);
			if (!trace.allsolid && !trace.startsolid)
			{
				SetAbsOrigin(newPosition);
			}
		}
		else
		{
			trace_t trace;

			Ray_t ray;
			ray.Init(GetAbsOrigin(), GetAbsOrigin(), WorldAlignMins(), WorldAlignMaxs());

			CTraceFilterSimple OriginalTraceFilter(this, COLLISION_GROUP_PLAYER_MOVEMENT);
			CTraceFilterTranslateClones traceFilter(&OriginalTraceFilter);
			UTIL_Portal_TraceRay_With(m_hPortalEnvironment, ray, MASK_PLAYERSOLID, &traceFilter, &trace);

			// current position is not ok, fixup
			if (trace.allsolid || trace.startsolid)
			{
				//try again with new position
				ray.Init(newPosition, newPosition, WorldAlignMins(), WorldAlignMaxs());
				UTIL_Portal_TraceRay_With(m_hPortalEnvironment, ray, MASK_PLAYERSOLID, &traceFilter, &trace);

				if (trace.startsolid == false)
				{
					SetAbsOrigin(newPosition);
				}
				else
				{
					if (!FindClosestPassableSpace(this, newPosition - GetAbsOrigin(), MASK_PLAYERSOLID))
					{
						// Try moving the player closer to the center of the portal
						CProp_Portal *pPortal = m_hPortalEnvironment.Get();
						newPosition += (pPortal->GetAbsOrigin() - WorldSpaceCenter()) * 0.1f;
						SetAbsOrigin(newPosition);

						DevMsg("Hurting the player for FindClosestPassableSpaceFailure!");

						// Deal 1 damage per frame... this will kill a player very fast, but allow for the above correction to fix some cases
						CTakeDamageInfo info(this, this, vec3_origin, vec3_origin, 1, DMG_CRUSH);
						OnTakeDamage(info);
					}
				}
			}
		}
	}
	else
	{
		if (m_touchedPhysObject)
		{
			// check my position (physics object could have simulated into my position
			// physics is not very far away, check my position
			trace_t trace;
			UTIL_TraceEntity(this, GetAbsOrigin(), GetAbsOrigin(),
				MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace);

			// is current position ok?
			if (trace.allsolid || trace.startsolid)
			{
				// stuck????!?!?
				//Msg("Stuck on %s\n", trace.m_pEnt->GetClassname());
				SetAbsOrigin(newPosition);
				UTIL_TraceEntity(this, GetAbsOrigin(), GetAbsOrigin(),
					MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace);
				if (trace.allsolid || trace.startsolid)
				{
					//Msg("Double Stuck\n");
					SetAbsOrigin(m_oldOrigin);
				}
			}
		}
	}
	m_oldOrigin = GetAbsOrigin();
}

bool CHL2_Player::UseFoundEntity(CBaseEntity *pUseEntity)
{
	bool usedSomething = false;

	//!!!UNDONE: traceline here to prevent +USEing buttons through walls			
	int caps = pUseEntity->ObjectCaps();
	variant_t emptyVariant;

	if (m_afButtonPressed & IN_USE)
	{
		// Robin: Don't play sounds for NPCs, because NPCs will allow respond with speech.
		if (!pUseEntity->MyNPCPointer())
		{
			EmitSound("HL2Player.Use");
		}
	}

	if (((m_nButtons & IN_USE) && (caps & FCAP_CONTINUOUS_USE)) ||
		((m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE | FCAP_ONOFF_USE))))
	{
		if (caps & FCAP_CONTINUOUS_USE)
			m_afPhysicsFlags |= PFLAG_USING;

		pUseEntity->AcceptInput("Use", this, this, emptyVariant, USE_TOGGLE);

		usedSomething = true;
	}
	// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
	else if ((m_afButtonReleased & IN_USE) && (pUseEntity->ObjectCaps() & FCAP_ONOFF_USE))	// BUGBUG This is an "off" use
	{
		pUseEntity->AcceptInput("Use", this, this, emptyVariant, USE_TOGGLE);

		usedSomething = true;
	}

#if	HL2_SINGLE_PRIMARY_WEAPON_MODE

	//Check for weapon pick-up
	if (m_afButtonPressed & IN_USE)
	{
		CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>(pUseEntity);

		if ((pWeapon != NULL) && (Weapon_CanSwitchTo(pWeapon)))
		{
			//Try to take ammo or swap the weapon
			if (Weapon_OwnsThisType(pWeapon->GetClassname(), pWeapon->GetSubType()))
			{
				Weapon_EquipAmmoOnly(pWeapon);
			}
			else
			{
				Weapon_DropSlot(pWeapon->GetSlot());
				Weapon_Equip(pWeapon);
			}

			usedSomething = true;
		}
	}
#endif

	return usedSomething;
}
#endif // PORTAL_COMPILE
#endif // DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: 
// Output : void CBasePlayer::PlayerUse
//-----------------------------------------------------------------------------
void CHL2_Player::PlayerUse(void)
{
	// Was use pressed or released?
	if (!((m_nButtons | m_afButtonPressed | m_afButtonReleased) & IN_USE))
		return;

	if (m_afButtonPressed & IN_USE)
	{
		// Currently using a latched entity?
		if (ClearUseEntity())
		{
			return;
		}
		else
		{
			if (m_afPhysicsFlags & PFLAG_DIROVERRIDE)
			{
				m_afPhysicsFlags &= ~PFLAG_DIROVERRIDE;
				m_iTrain = TRAIN_NEW | TRAIN_OFF;
				return;
			}
			else
			{	// Start controlling the train!
				CBaseEntity *pTrain = GetGroundEntity();
				if (pTrain && !(m_nButtons & IN_JUMP) && (GetFlags() & FL_ONGROUND) && (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) && pTrain->OnControls(this))
				{
					m_afPhysicsFlags |= PFLAG_DIROVERRIDE;
					m_iTrain = TrainSpeed(pTrain->m_flSpeed, ((CFuncTrackTrain*)pTrain)->GetMaxSpeed());
					m_iTrain |= TRAIN_NEW;
					EmitSound("HL2Player.TrainUse");
					return;
				}
			}
		}

		// Tracker 3926:  We can't +USE something if we're climbing a ladder
		if (GetMoveType() == MOVETYPE_LADDER)
		{
			return;
		}
	}

	if (m_flTimeUseSuspended > CURTIME)
	{
		// Something has temporarily stopped us being able to USE things.
		// Obviously, this should be used very carefully.(sjb)
		return;
	}
#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
	CBaseEntity *pUseEntity = FindUseEntity();

	bool usedSomething = false;

	// Found an object
	if (pUseEntity)
	{
		SetHeldObjectOnOppositeSideOfPortal(false);

		// TODO: Removed because we no longer have ghost animatings. May need to rework this code.
		//// If we found a ghost animating then it needs to be held across a portal
		//CGhostAnimating *pGhostAnimating = dynamic_cast<CGhostAnimating*>( pUseEntity );
		//if ( pGhostAnimating )
		//{
		//	CProp_Portal *pPortal = NULL;

		//	CPortalSimulator *pPortalSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pGhostAnimating->GetSourceEntity() );

		//	//HACKHACK: This assumes all portal simulators are a member of a prop_portal
		//	pPortal = (CProp_Portal *)(((char *)pPortalSimulator) - ((int)&(((CProp_Portal *)0)->m_PortalSimulator)));
		//	Assert( (&(pPortal->m_PortalSimulator)) == pPortalSimulator ); //doublechecking the hack

		//	if ( pPortal )
		//	{
		//		SetHeldObjectPortal( pPortal->m_hLinkedPortal );
		//		SetHeldObjectOnOppositeSideOfPortal( true );
		//	}
		//}
		usedSomething = UseFoundEntity(pUseEntity);
	}
	else
	{
		Vector forward;
		EyeVectors(&forward, NULL, NULL);
		Vector start = EyePosition();

		Ray_t rayPortalTest;
		rayPortalTest.Init(start, start + forward * PLAYER_USE_RADIUS);

		float fMustBeCloserThan = 2.0f;

		CProp_Portal *pPortal = UTIL_Portal_FirstAlongRay(rayPortalTest, fMustBeCloserThan);

		if (pPortal)
		{
			SetHeldObjectPortal(pPortal);
			pUseEntity = FindUseEntityThroughPortal();
		}

		if (pUseEntity)
		{
			SetHeldObjectOnOppositeSideOfPortal(true);
			usedSomething = UseFoundEntity(pUseEntity);
		}
		else if (m_afButtonPressed & IN_USE)
		{
			// Signal that we want to play the deny sound, unless the user is +USEing on a ladder!
			// The sound is emitted in ItemPostFrame, since that occurs after GameMovement::ProcessMove which
			// lets the ladder code unset this flag.
			m_bPlayUseDenySound = true;
		}
	}

	// Debounce the use key
	if (usedSomething && pUseEntity)
	{
		m_Local.m_nOldButtons |= IN_USE;
		m_afButtonPressed &= ~IN_USE;
	}
#else
	CBaseEntity *pUseEntity = FindUseEntity();

	bool usedSomething = false;

	// Found an object
	if (pUseEntity)
	{
		//!!!UNDONE: traceline here to prevent +USEing buttons through walls			
		int caps = pUseEntity->ObjectCaps();
		variant_t emptyVariant;

		if (m_afButtonPressed & IN_USE)
		{
			// Robin: Don't play sounds for NPCs, because NPCs will allow respond with speech.
			if (!pUseEntity->MyNPCPointer())
			{
				EmitSound("HL2Player.Use");
			}
		}

		if (((m_nButtons & IN_USE) && (caps & FCAP_CONTINUOUS_USE)) ||
			((m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE | FCAP_ONOFF_USE))))
		{
			if (caps & FCAP_CONTINUOUS_USE)
				m_afPhysicsFlags |= PFLAG_USING;

			pUseEntity->AcceptInput("Use", this, this, emptyVariant, USE_TOGGLE);

			usedSomething = true;
		}
		// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
		else if ((m_afButtonReleased & IN_USE) && (pUseEntity->ObjectCaps() & FCAP_ONOFF_USE))	// BUGBUG This is an "off" use
		{
			pUseEntity->AcceptInput("Use", this, this, emptyVariant, USE_TOGGLE);

			usedSomething = true;
		}
	}
	else if (m_afButtonPressed & IN_USE)
	{
		// Signal that we want to play the deny sound, unless the user is +USEing on a ladder!
		// The sound is emitted in ItemPostFrame, since that occurs after GameMovement::ProcessMove which
		// lets the ladder code unset this flag.
		m_bPlayUseDenySound = true;
	}

	// Debounce the use key
	if (usedSomething && pUseEntity)
	{
		m_Local.m_nOldButtons |= IN_USE;
		m_afButtonPressed &= ~IN_USE;
	}
#endif // PORTAL_COMPILE
#else // DARKINTERVAL
	CBaseEntity *pUseEntity = FindUseEntity();

	bool usedSomething = false;

	// Found an object
	if ( pUseEntity )
	{
		//!!!UNDONE: traceline here to prevent +USEing buttons through walls			
		int caps = pUseEntity->ObjectCaps();
		variant_t emptyVariant;

		if ( m_afButtonPressed & IN_USE )
		{
			// Robin: Don't play sounds for NPCs, because NPCs will allow respond with speech.
			if ( !pUseEntity->MyNPCPointer() )
			{
				EmitSound("HL2Player.Use");
			}
		}

		if ( ( ( m_nButtons & IN_USE ) && ( caps & FCAP_CONTINUOUS_USE ) ) ||
			( ( m_afButtonPressed & IN_USE ) && ( caps & ( FCAP_IMPULSE_USE | FCAP_ONOFF_USE ) ) ) )
		{
			if ( caps & FCAP_CONTINUOUS_USE )
				m_afPhysicsFlags |= PFLAG_USING;

			pUseEntity->AcceptInput("Use", this, this, emptyVariant, USE_TOGGLE);

			usedSomething = true;
		}
		// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
		else if ( ( m_afButtonReleased & IN_USE ) && ( pUseEntity->ObjectCaps() & FCAP_ONOFF_USE ) )	// BUGBUG This is an "off" use
		{
			pUseEntity->AcceptInput("Use", this, this, emptyVariant, USE_TOGGLE);

			usedSomething = true;
		}

	#if	HL2_SINGLE_PRIMARY_WEAPON_MODE

		//Check for weapon pick-up
		if ( m_afButtonPressed & IN_USE )
		{
			CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>( pUseEntity );

			if ( ( pWeapon != NULL ) && ( Weapon_CanSwitchTo(pWeapon) ) )
			{
				//Try to take ammo or swap the weapon
				if ( Weapon_OwnsThisType(pWeapon->GetClassname(), pWeapon->GetSubType()) )
				{
					Weapon_EquipAmmoOnly(pWeapon);
				} else
				{
					Weapon_DropSlot(pWeapon->GetSlot());
					Weapon_Equip(pWeapon);
				}

				usedSomething = true;
			}
		}
	#endif
	} else if ( m_afButtonPressed & IN_USE )
	{
		// Signal that we want to play the deny sound, unless the user is +USEing on a ladder!
		// The sound is emitted in ItemPostFrame, since that occurs after GameMovement::ProcessMove which
		// lets the ladder code unset this flag.
		m_bPlayUseDenySound = true;
	}

	// Debounce the use key
	if ( usedSomething && pUseEntity )
	{
		m_Local.m_nOldButtons |= IN_USE;
		m_afButtonPressed &= ~IN_USE;
	}
#endif // DARKINTERVAL
}
#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
CWeaponPortalBase* CHL2_Player::GetActivePortalWeapon() const
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if (pWeapon)
	{
		return dynamic_cast< CWeaponPortalBase* >(pWeapon);
	}
	else
	{
		return NULL;
	}
}

CBaseEntity *CHL2_Player::FindUseEntity()
{
	Vector forward, up;
	EyeVectors(&forward, NULL, &up);

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition();

	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	UTIL_TraceLine(searchCenter, searchCenter + forward * 1024, useableContents, this, COLLISION_GROUP_NONE, &tr);
	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *pNearest = NULL;
	CBaseEntity *pObject = tr.m_pEnt;

	// TODO: Removed because we no longer have ghost animatings. We may need similar code that clips rays against transformed objects.
	//#ifndef CLIENT_DLL
	//	// Check for ghost animatings (these aren't hit in the normal trace because they aren't solid)
	//	if ( !IsUseableEntity(pObject, 0) )
	//	{
	//		Ray_t rayGhostAnimating;
	//		rayGhostAnimating.Init( searchCenter, searchCenter + forward * 1024 );
	//
	//		CBaseEntity *list[1024];
	//		int nCount = UTIL_EntitiesAlongRay( list, 1024, rayGhostAnimating, 0 );
	//
	//		// Loop through all entities along the pick up ray
	//		for ( int i = 0; i < nCount; i++ )
	//		{
	//			CGhostAnimating *pGhostAnimating = dynamic_cast<CGhostAnimating*>( list[i] );
	//
	//			// If the entity is a ghost animating
	//			if( pGhostAnimating )
	//			{
	//				trace_t trGhostAnimating;
	//				enginetrace->ClipRayToEntity( rayGhostAnimating, MASK_ALL, pGhostAnimating, &trGhostAnimating );
	//
	//				if ( trGhostAnimating.fraction < tr.fraction )
	//				{
	//					// If we're not grabbing the clipped ghost
	//					VPlane plane = pGhostAnimating->GetLocalClipPlane();
	//					UTIL_Portal_PlaneTransform( pGhostAnimating->GetCloneTransform(), plane, plane );
	//					if ( plane.GetPointSide( trGhostAnimating.endpos ) != SIDE_FRONT )
	//					{
	//						tr = trGhostAnimating;
	//						pObject = tr.m_pEnt;
	//					}
	//				}
	//			}
	//		}
	//	}
	//#endif

	int count = 0;
	// UNDONE: Might be faster to just fold this range into the sphere query
	const int NUM_TANGENTS = 7;
	while (!IsUseableEntity(pObject, 0) && count < NUM_TANGENTS)
	{
		// trace a box at successive angles down
		//							45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
		const float tangents[NUM_TANGENTS] = { 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f };
		Vector down = forward - tangents[count] * up;
		VectorNormalize(down);
		UTIL_TraceHull(searchCenter, searchCenter + down * 72, -Vector(16, 16, 16), Vector(16, 16, 16), useableContents, this, COLLISION_GROUP_NONE, &tr);
		pObject = tr.m_pEnt;
		count++;
	}
	float nearestDot = CONE_90_DEGREES;
	if (IsUseableEntity(pObject, 0))
	{
		Vector delta = tr.endpos - tr.startpos;
		float centerZ = CollisionProp()->WorldSpaceCenter().z;
		delta.z = IntervalDistance(tr.endpos.z, centerZ + CollisionProp()->OBBMins().z, centerZ + CollisionProp()->OBBMaxs().z);
		float dist = delta.Length();
		if (dist < PLAYER_USE_RADIUS)
		{
#ifndef CLIENT_DLL
			/*
			if (sv_debug_player_use.GetBool())
			{
			NDebugOverlay::Line(searchCenter, tr.endpos, 0, 255, 0, true, 30);
			NDebugOverlay::Cross3D(tr.endpos, 16, 0, 255, 0, true, 30);
			}
			*/
			if (pObject->MyNPCPointer() && pObject->MyNPCPointer()->IsPlayerAlly(this))
			{
				// If about to select an NPC, do a more thorough check to ensure
				// that we're selecting the right one from a group.
				pObject = DoubleCheckUseNPC(pObject, searchCenter, forward);
			}

			//			g_PortalGameStats.Event_PlayerUsed( searchCenter, forward, pObject );
#endif

			return pObject;
		}
	}

#ifndef CLIENT_DLL
	/*
	CBaseEntity *pFoundByTrace = pObject;
	*/
#endif

	// check ground entity first
	// if you've got a useable ground entity, then shrink the cone of this search to 45 degrees
	// otherwise, search out in a 90 degree cone (hemisphere)
	if (GetGroundEntity() && IsUseableEntity(GetGroundEntity(), FCAP_USE_ONGROUND))
	{
		pNearest = GetGroundEntity();
		nearestDot = CONE_45_DEGREES;
	}

	for (CEntitySphereQuery sphere(searchCenter, PLAYER_USE_RADIUS); (pObject = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity())
	{
		if (!pObject)
			continue;

		if (!IsUseableEntity(pObject, FCAP_USE_IN_RADIUS))
			continue;

		// see if it's more roughly in front of the player than previous guess
		Vector point;
		pObject->CollisionProp()->CalcNearestPoint(searchCenter, &point);

		Vector dir = point - searchCenter;
		VectorNormalize(dir);
		float dot = DotProduct(dir, forward);

		// Need to be looking at the object more or less
		if (dot < 0.8)
			continue;

		if (dot > nearestDot)
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			trace_t trCheckOccluded;
			UTIL_TraceLine(searchCenter, point, useableContents, this, COLLISION_GROUP_NONE, &trCheckOccluded);

			if (trCheckOccluded.fraction == 1.0 || trCheckOccluded.m_pEnt == pObject)
			{
				pNearest = pObject;
				nearestDot = dot;
			}
		}
	}

#ifndef CLIENT_DLL
	if (!pNearest)
	{
		// Haven't found anything near the player to use, nor any NPC's at distance.
		// Check to see if the player is trying to select an NPC through a rail, fence, or other 'see-though' volume.
		trace_t trAllies;
		UTIL_TraceLine(searchCenter, searchCenter + forward * PLAYER_USE_RADIUS, MASK_OPAQUE_AND_NPCS, this, COLLISION_GROUP_NONE, &trAllies);

		if (trAllies.m_pEnt && IsUseableEntity(trAllies.m_pEnt, 0) && trAllies.m_pEnt->MyNPCPointer() && trAllies.m_pEnt->MyNPCPointer()->IsPlayerAlly(this))
		{
			// This is an NPC, take it!
			pNearest = trAllies.m_pEnt;
		}
	}

	if (pNearest && pNearest->MyNPCPointer() && pNearest->MyNPCPointer()->IsPlayerAlly(this))
	{
		pNearest = DoubleCheckUseNPC(pNearest, searchCenter, forward);
	}
	/*
	if (sv_debug_player_use.GetBool())
	{
	if (!pNearest)
	{
	NDebugOverlay::Line(searchCenter, tr.endpos, 255, 0, 0, true, 30);
	NDebugOverlay::Cross3D(tr.endpos, 16, 255, 0, 0, true, 30);
	}
	else if (pNearest == pFoundByTrace)
	{
	NDebugOverlay::Line(searchCenter, tr.endpos, 0, 255, 0, true, 30);
	NDebugOverlay::Cross3D(tr.endpos, 16, 0, 255, 0, true, 30);
	}
	else
	{
	NDebugOverlay::Box(pNearest->WorldSpaceCenter(), Vector(-8, -8, -8), Vector(8, 8, 8), 0, 255, 0, true, 30);
	}
	}
	*/
	//	g_PortalGameStats.Event_PlayerUsed( searchCenter, forward, pNearest );
#endif

	return pNearest;
}

CBaseEntity* CHL2_Player::FindUseEntityThroughPortal(void)
{
	Vector forward, up;
	EyeVectors(&forward, NULL, &up);

	CProp_Portal *pPortal = GetHeldObjectPortal();

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition();

	Vector vTransformedForward, vTransformedUp, vTransformedSearchCenter;

	VMatrix matThisToLinked = pPortal->MatrixThisToLinked();
	UTIL_Portal_PointTransform(matThisToLinked, searchCenter, vTransformedSearchCenter);
	UTIL_Portal_VectorTransform(matThisToLinked, forward, vTransformedForward);
	UTIL_Portal_VectorTransform(matThisToLinked, up, vTransformedUp);


	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	//UTIL_TraceLine( vTransformedSearchCenter, vTransformedSearchCenter + vTransformedForward * 1024, useableContents, this, COLLISION_GROUP_NONE, &tr );
	Ray_t rayLinked;
	rayLinked.Init(searchCenter, searchCenter + forward * 1024);
	UTIL_PortalLinked_TraceRay(pPortal, rayLinked, useableContents, this, COLLISION_GROUP_NONE, &tr);

	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *pNearest = NULL;
	CBaseEntity *pObject = tr.m_pEnt;
	int count = 0;
	// UNDONE: Might be faster to just fold this range into the sphere query
	const int NUM_TANGENTS = 7;
	while (!IsUseableEntity(pObject, 0) && count < NUM_TANGENTS)
	{
		// trace a box at successive angles down
		//							45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
		const float tangents[NUM_TANGENTS] = { 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f };
		Vector down = vTransformedForward - tangents[count] * vTransformedUp;
		VectorNormalize(down);
		UTIL_TraceHull(vTransformedSearchCenter, vTransformedSearchCenter + down * 72, -Vector(16, 16, 16), Vector(16, 16, 16), useableContents, this, COLLISION_GROUP_NONE, &tr);
		pObject = tr.m_pEnt;
		count++;
	}
	float nearestDot = CONE_90_DEGREES;
	if (IsUseableEntity(pObject, 0))
	{
		Vector delta = tr.endpos - tr.startpos;
		float centerZ = CollisionProp()->WorldSpaceCenter().z;
		delta.z = IntervalDistance(tr.endpos.z, centerZ + CollisionProp()->OBBMins().z, centerZ + CollisionProp()->OBBMaxs().z);
		float dist = delta.Length();
		if (dist < PLAYER_USE_RADIUS)
		{
#ifndef CLIENT_DLL

			if (pObject->MyNPCPointer() && pObject->MyNPCPointer()->IsPlayerAlly(this))
			{
				// If about to select an NPC, do a more thorough check to ensure
				// that we're selecting the right one from a group.
				pObject = DoubleCheckUseNPC(pObject, vTransformedSearchCenter, vTransformedForward);
			}
#endif

			return pObject;
		}
	}

	// check ground entity first
	// if you've got a useable ground entity, then shrink the cone of this search to 45 degrees
	// otherwise, search out in a 90 degree cone (hemisphere)
	if (GetGroundEntity() && IsUseableEntity(GetGroundEntity(), FCAP_USE_ONGROUND))
	{
		pNearest = GetGroundEntity();
		nearestDot = CONE_45_DEGREES;
	}

	for (CEntitySphereQuery sphere(vTransformedSearchCenter, PLAYER_USE_RADIUS); (pObject = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity())
	{
		if (!pObject)
			continue;

		if (!IsUseableEntity(pObject, FCAP_USE_IN_RADIUS))
			continue;

		// see if it's more roughly in front of the player than previous guess
		Vector point;
		pObject->CollisionProp()->CalcNearestPoint(vTransformedSearchCenter, &point);

		Vector dir = point - vTransformedSearchCenter;
		VectorNormalize(dir);
		float dot = DotProduct(dir, vTransformedForward);

		// Need to be looking at the object more or less
		if (dot < 0.8)
			continue;

		if (dot > nearestDot)
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			trace_t trCheckOccluded;
			UTIL_TraceLine(vTransformedSearchCenter, point, useableContents, this, COLLISION_GROUP_NONE, &trCheckOccluded);

			if (trCheckOccluded.fraction == 1.0 || trCheckOccluded.m_pEnt == pObject)
			{
				pNearest = pObject;
				nearestDot = dot;
			}
		}
	}

#ifndef CLIENT_DLL
	if (!pNearest)
	{
		// Haven't found anything near the player to use, nor any NPC's at distance.
		// Check to see if the player is trying to select an NPC through a rail, fence, or other 'see-though' volume.
		trace_t trAllies;
		UTIL_TraceLine(vTransformedSearchCenter, vTransformedSearchCenter + vTransformedForward * PLAYER_USE_RADIUS, MASK_OPAQUE_AND_NPCS, this, COLLISION_GROUP_NONE, &trAllies);

		if (trAllies.m_pEnt && IsUseableEntity(trAllies.m_pEnt, 0) && trAllies.m_pEnt->MyNPCPointer() && trAllies.m_pEnt->MyNPCPointer()->IsPlayerAlly(this))
		{
			// This is an NPC, take it!
			pNearest = trAllies.m_pEnt;
		}
	}

	if (pNearest && pNearest->MyNPCPointer() && pNearest->MyNPCPointer()->IsPlayerAlly(this))
	{
		pNearest = DoubleCheckUseNPC(pNearest, vTransformedSearchCenter, vTransformedForward);
	}

#endif

	return pNearest;
}
#endif // PORTAL_COMPILE
#endif // DARKINTERVAL

ConVar	sv_show_crosshair_target("sv_show_crosshair_target", "0");
//-----------------------------------------------------------------------------
// Purpose: Updates the posture of the weapon from lowered to ready
//-----------------------------------------------------------------------------
void CHL2_Player::UpdateWeaponPosture(void)
{
	CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>(GetActiveWeapon());

	if (pWeapon && m_LowerWeaponTimer.Expired() && pWeapon->CanLower())
	{
		m_LowerWeaponTimer.Set(.3);
		VPROF("CHL2_Player::UpdateWeaponPosture-CheckLower");
		Vector vecAim = BaseClass::GetAutoaimVector(AUTOAIM_SCALE_DIRECT_ONLY);

		const float CHECK_FRIENDLY_RANGE = 50 * 12;
		trace_t	tr;
		UTIL_TraceLine(EyePosition(), EyePosition() + vecAim * CHECK_FRIENDLY_RANGE, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		CBaseEntity *aimTarget = tr.m_pEnt;

		//If we're over something
		if (aimTarget && !tr.DidHitWorld())
		{
			if (!aimTarget->IsNPC() || aimTarget->MyNPCPointer()->GetState() != NPC_STATE_COMBAT)
			{
				Disposition_t dis = IRelationType(aimTarget);

				//Debug info for seeing what an object "cons" as
				if (sv_show_crosshair_target.GetBool())
				{
					int text_offset = BaseClass::DrawDebugTextOverlays();

					char tempstr[255];

					switch (dis)
					{
					case D_LI:
						Q_snprintf(tempstr, sizeof(tempstr), "Disposition: Like");
						break;

					case D_HT:
						Q_snprintf(tempstr, sizeof(tempstr), "Disposition: Hate");
						break;

					case D_FR:
						Q_snprintf(tempstr, sizeof(tempstr), "Disposition: Fear");
						break;

					case D_NU:
						Q_snprintf(tempstr, sizeof(tempstr), "Disposition: Neutral");
						break;

					default:
					case D_ER:
						Q_snprintf(tempstr, sizeof(tempstr), "Disposition: !!!ERROR!!!");
						break;
					}

					//Draw the text
					NDebugOverlay::EntityText(aimTarget->entindex(), text_offset, tempstr, 0);
				}

				//See if we hates it
				if (dis == D_LI)
				{
					//We're over a friendly, drop our weapon
					if (Weapon_Lower() == false)
					{
						//FIXME: We couldn't lower our weapon!
					}

					return;
				}
			}
		}

		if (Weapon_Ready() == false)
		{
			//FIXME: We couldn't raise our weapon!
		}
	}

	if (g_pGameRules->GetAutoAimMode() != AUTOAIM_NONE)
	{
		if (!pWeapon)
		{
			// This tells the client to draw no crosshair
			m_HL2Local.m_bWeaponLowered = true;
			return;
		}
		else
		{
			if (!pWeapon->CanLower() && m_HL2Local.m_bWeaponLowered)
				m_HL2Local.m_bWeaponLowered = false;
		}

		if (!m_AutoaimTimer.Expired())
			return;

		m_AutoaimTimer.Set(.1);

		VPROF("hl2_auto_aiming");

		// Call the autoaim code to update the local player data, which allows the client to update.
		autoaim_params_t params;
		params.m_vecAutoAimPoint.Init();
		params.m_vecAutoAimDir.Init();
		params.m_fScale = AUTOAIM_SCALE_DEFAULT;
		params.m_fMaxDist = autoaim_max_dist.GetFloat();
		GetAutoaimVector(params);
		m_HL2Local.m_hAutoAimTarget.Set(params.m_hAutoAimEntity);
		m_HL2Local.m_vecAutoAimPoint.Set(params.m_vecAutoAimPoint);
		m_HL2Local.m_bAutoAimTarget = (params.m_bAutoAimAssisting || params.m_bOnTargetNatural);
		return;
	}
	else
	{
		// Make sure there's no residual autoaim target if the user changes the xbox_aiming convar on the fly.
		m_HL2Local.m_hAutoAimTarget.Set(NULL);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Lowers the weapon posture (for hovering over friendlies)
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHL2_Player::Weapon_Lower(void)
{
	VPROF("CHL2_Player::Weapon_Lower");
	// Already lowered?
	if (m_HL2Local.m_bWeaponLowered)
		return true;

	m_HL2Local.m_bWeaponLowered = true;

	CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>(GetActiveWeapon());

	if (pWeapon == NULL)
		return false;

	return pWeapon->Lower();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the weapon posture to normal
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHL2_Player::Weapon_Ready(void)
{
	VPROF("CHL2_Player::Weapon_Ready");

	// Already ready?
	if (m_HL2Local.m_bWeaponLowered == false)
		return true;

	m_HL2Local.m_bWeaponLowered = false;

	CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>(GetActiveWeapon());

	if (pWeapon == NULL)
		return false;

	return pWeapon->Ready();
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we can switch to the given weapon.
// Input  : pWeapon - 
//-----------------------------------------------------------------------------
bool CHL2_Player::Weapon_CanSwitchTo(CBaseCombatWeapon *pWeapon)
{
	CBasePlayer *pPlayer = (CBasePlayer *)this;
#if !defined( CLIENT_DLL )
	IServerVehicle *pVehicle = pPlayer->GetVehicle();
#else
	IClientVehicle *pVehicle = pPlayer->GetVehicle();
#endif
	if (pVehicle && !pPlayer->UsingStandardWeaponsInVehicle())
		return false;

	if (!pWeapon->HasAnyAmmo() && !GetAmmoCount(pWeapon->m_iPrimaryAmmoType))
		return false;

	if (!pWeapon->CanDeploy())
		return false;

	if (GetActiveWeapon())
	{
		if (PhysCannonGetHeldEntity(GetActiveWeapon()) == pWeapon &&
			Weapon_OwnsThisType(pWeapon->GetClassname(), pWeapon->GetSubType()))
		{
			return true;
		}

		if (!GetActiveWeapon()->CanHolster())
			return false;
	}

	return true;
}
#ifdef DARKINTERVAL
#ifdef PORTAL_COMPILE
void CHL2_Player::ForceDuckThisFrame(void)
{
	if (m_Local.m_bDucked != true)
	{
		//m_Local.m_bDucking = false;
		m_Local.m_bDucked = true;
		ForceButtons(IN_DUCK);
		AddFlag(FL_DUCKING);
		SetVCollisionState(GetAbsOrigin(), GetAbsVelocity(), VPHYS_CROUCH);
	}
}

void CHL2_Player::UnDuck(void)
{
	if (m_Local.m_bDucked != false)
	{
		m_Local.m_bDucked = false;
		UnforceButtons(IN_DUCK);
		RemoveFlag(FL_DUCKING);
		SetVCollisionState(GetAbsOrigin(), GetAbsVelocity(), VPHYS_WALK);
	}
}
#endif
#endif // DARKINTERVAL
void CHL2_Player::PickupObject(CBaseEntity *pObject, bool bLimitMassAndSize)
{
	// can't pick up what you're standing on
	if (GetGroundEntity() == pObject)
		return;

	if (bLimitMassAndSize == true)
	{
		if (CBasePlayer::CanPickupObject(pObject, 35, 128) == false)
		{
			return;
		}
	}

	// Can't be picked up if NPCs are on me
	if (pObject->HasNPCsOnIt())
		return;

	PlayerPickupObject(this, pObject);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseEntity
//-----------------------------------------------------------------------------
bool CHL2_Player::IsHoldingEntity(CBaseEntity *pEnt)
{
	return PlayerPickupControllerIsHoldingEntity(m_hUseEntity, pEnt);
}

float CHL2_Player::GetHeldObjectMass(IPhysicsObject *pHeldObject)
{
	float mass = PlayerPickupGetHeldObjectMass(m_hUseEntity, pHeldObject);
	if (mass == 0.0f)
	{
		mass = PhysCannonGetHeldObjectMass(GetActiveWeapon(), pHeldObject);
	}
	return mass;
}

//-----------------------------------------------------------------------------
// Purpose: Force the player to drop any physics objects he's carrying
//-----------------------------------------------------------------------------
void CHL2_Player::ForceDropOfCarriedPhysObjects(CBaseEntity *pOnlyIfHoldingThis)
{
	if (PhysIsInCallback())
	{
		variant_t value;
		g_EventQueue.AddEvent(this, "ForceDropPhysObjects", value, 0.01f, pOnlyIfHoldingThis, this);
		return;
	}

#ifdef HL2_EPISODIC
	if (hl2_episodic.GetBool())
	{
		CBaseEntity *pHeldEntity = PhysCannonGetHeldEntity(GetActiveWeapon());
		if (pHeldEntity && pHeldEntity->ClassMatches("grenade_helicopter"))
		{
			return;
		}
	}
#endif

	// Drop any objects being handheld.
	ClearUseEntity();

	// Then force the physcannon to drop anything it's holding, if it's our active weapon
	PhysCannonForceDrop(GetActiveWeapon(), NULL);
}

void CHL2_Player::InputForceDropPhysObjects(inputdata_t &data)
{
	ForceDropOfCarriedPhysObjects(data.pActivator);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_Player::UpdateClientData(void)
{
	if (m_DmgTake || m_DmgSave || m_bitsHUDDamage != m_bitsDamageType)
	{
		// Comes from inside me if not set
		Vector damageOrigin = GetLocalOrigin();
		// send "damage" message
		// causes screen to flash, and pain compass to show direction of damage
		damageOrigin = m_DmgOrigin;

		// only send down damage type that have hud art
		int iShowHudDamage = g_pGameRules->Damage_GetShowOnHud();
		int visibleDamageBits = m_bitsDamageType & iShowHudDamage;

		m_DmgTake = clamp(m_DmgTake, 0, 255);
		m_DmgSave = clamp(m_DmgSave, 0, 255);

		// If we're poisoned, but it wasn't this frame, don't send the indicator
		// Without this check, any damage that occured to the player while they were
		// recovering from a poison bite would register as poisonous as well and flash
		// the whole screen! -- jdw
		if (visibleDamageBits & DMG_POISON)
		{
			float flLastPoisonedDelta = CURTIME - m_tbdPrev;
			if (flLastPoisonedDelta > 0.1f)
			{
				visibleDamageBits &= ~DMG_POISON;
			}
		}
#ifdef DARKINTERVAL
/*
		if (visibleDamageBits & DMG_ACID)
		{
			float flLastAcidDelta = CURTIME - m_tbdPrev;
			if (flLastAcidDelta > 0.1f)
			{
				visibleDamageBits &= ~DMG_ACID;
			}
		}
*/

		if (visibleDamageBits & DMG_RADIATION)
		{
			float flLastRadiationDelta = CURTIME - m_tbdPrev;
			if (flLastRadiationDelta > 0.1f)
			{
				visibleDamageBits &= ~DMG_RADIATION;
			}
		}
#endif
		CSingleUserRecipientFilter user(this);
		user.MakeReliable();
		UserMessageBegin(user, "Damage");
		WRITE_BYTE(m_DmgSave);
		WRITE_BYTE(m_DmgTake);
		WRITE_LONG(visibleDamageBits);
		WRITE_FLOAT(damageOrigin.x);	//BUG: Should be fixed point (to hud) not floats
		WRITE_FLOAT(damageOrigin.y);	//BUG: However, the HUD does _not_ implement bitfield messages (yet)
		WRITE_FLOAT(damageOrigin.z);	//BUG: We use WRITE_VEC3COORD for everything else
		MessageEnd();

		m_DmgTake = 0;
		m_DmgSave = 0;
		m_bitsHUDDamage = m_bitsDamageType;

		// Clear off non-time-based damage indicators
		int iTimeBasedDamage = g_pGameRules->Damage_GetTimeBased();
		m_bitsDamageType &= iTimeBasedDamage;
	}

	// Update Flashlight
#ifdef HL2_EPISODIC
	if (Flashlight_UseLegacyVersion() == false)
	{
		if (FlashlightIsOn() && sv_infinite_aux_power.GetBool() == false /*&& IsSuitEquipped() == false*/)
		{
			m_HL2Local.m_flFlashBattery -= FLASH_DRAIN_TIME * gpGlobals->frametime;
			if (m_HL2Local.m_flFlashBattery < 0.0f)
			{
				FlashlightTurnOff();
				m_HL2Local.m_flFlashBattery = 0.0f;
			}
		}
		else
		{
			m_HL2Local.m_flFlashBattery += FLASH_CHARGE_TIME * gpGlobals->frametime;
			if (m_HL2Local.m_flFlashBattery > 100.0f)
			{
				m_HL2Local.m_flFlashBattery = 100.0f;
			}
		}
	}
	else
	{
		m_HL2Local.m_flFlashBattery = -1.0f;
	}
#endif // HL2_EPISODIC
	
	BaseClass::UpdateClientData();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CHL2_Player::OnRestore()
{
	BaseClass::OnRestore();
	m_pPlayerAISquad = g_AI_SquadManager.FindCreateSquad(AllocPooledString(PLAYER_SQUADNAME));
#ifdef DARKINTERVAL
	// OICW Zoom class-check functionality

	// DI todo - this whole messy system needs to be replaced with calling the entities in viewcone 
	// and asking them to use glow, not applying them through the zoom!
	s_iszOICW_Zoom_AntlionGuard_Classname = AllocPooledString("npc_antlionguard");
	s_iszOICW_Zoom_AntlionSoldier_Classname = AllocPooledString("npc_antlion");
	s_iszOICW_Zoom_AntlionWorker_Classname = AllocPooledString("npc_antlionguard");
	s_iszOICW_Zoom_Barnacle_Classname = AllocPooledString("npc_barnacle");
	s_iszOICW_Zoom_Citizen_Classname = AllocPooledString("npc_citizen");
	s_iszOICW_Zoom_Vortigaunt_Classname = AllocPooledString("npc_vortigaunt");
	s_iszOICW_Zoom_Metrocop_Classname = AllocPooledString("npc_metropolice");
	s_iszOICW_Zoom_Soldier_Classname = AllocPooledString("npc_combine_s");
	s_iszOICW_Zoom_Squid_Classname = AllocPooledString("npc_squid");
	s_iszOICW_Zoom_Houndeye_Classname = AllocPooledString("npc_houndeye");
	s_iszOICW_Zoom_Headcrab_Classname = AllocPooledString("npc_headcrab");
	s_iszOICW_Zoom_HeadcrabF_Classname = AllocPooledString("npc_headcrab_fast");
	s_iszOICW_Zoom_HeadcrabP_Classname = AllocPooledString("npc_headcrab_black");
	s_iszOICW_Zoom_HeadcrabP2_Classname = AllocPooledString("npc_headcrab_poison");
	s_iszOICW_Zoom_Zombie_Classname = AllocPooledString("npc_zombie");
	s_iszOICW_Zoom_FastZombie_Classname = AllocPooledString("npc_fastzombie");
	s_iszOICW_Zoom_PoisonZombie_Classname = AllocPooledString("npc_poisonzombie");
	s_iszOICW_Zoom_ZombieWorker_Classname = AllocPooledString("npc_zombie_worker");
	s_iszOICW_Zoom_Zombine_Classname = AllocPooledString("npc_zombine");
	s_iszOICW_Zoom_SynthScanner_Classname = AllocPooledString("npc_synth_scanner");
	s_iszOICW_Zoom_ClawScanner_Classname = AllocPooledString("npc_clawscanner");
	s_iszOICW_Zoom_CityScanner_Classname = AllocPooledString("npc_cscanner");
	s_iszOICW_Zoom_Cremator_Classname = AllocPooledString("npc_cremator");
	s_iszOICW_Zoom_Tree_Classname = AllocPooledString("npc_tree");
#endif // DARKINTERVAL
}

//---------------------------------------------------------
//---------------------------------------------------------
Vector CHL2_Player::EyeDirection2D(void)
{
	Vector vecReturn = EyeDirection3D();
	vecReturn.z = 0;
	vecReturn.AsVector2D().NormalizeInPlace();

	return vecReturn;
}

//---------------------------------------------------------
//---------------------------------------------------------
Vector CHL2_Player::EyeDirection3D(void)
{
	Vector vecForward;

	// Return the vehicle angles if we request them
	if (GetVehicle() != NULL)
	{
		CacheVehicleView();
		EyeVectors(&vecForward);
		return vecForward;
	}

	AngleVectors(EyeAngles(), &vecForward);
	return vecForward;
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CHL2_Player::Weapon_Switch(CBaseCombatWeapon *pWeapon, int viewmodelindex)
{
	MDLCACHE_CRITICAL_SECTION();

	// Recalculate proficiency!
	SetCurrentWeaponProficiency(CalcWeaponProficiency(pWeapon));

	// Come out of suit zoom mode
	if (IsZooming())
	{
		StopZooming();
	}

	return BaseClass::Weapon_Switch(pWeapon, viewmodelindex);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WeaponProficiency_t CHL2_Player::CalcWeaponProficiency(CBaseCombatWeapon *pWeapon)
{
	WeaponProficiency_t proficiency;
#ifdef DARKINTERVAL
	proficiency = WEAPON_PROFICIENCY_GOOD; // Used to be PERFECT, but now GOOD does what PERFECT used to do (1.00 mult on bullet spread for weapons).
#else
	proficiency = WEAPON_PROFICIENCY_PERFECT;

	if( weapon_showproficiency.GetBool() != 0 )
	{
		Msg("Player switched to %s, proficiency is %s\n", pWeapon->GetClassname(), GetWeaponProficiencyName( proficiency ) );
	}
#endif
	return proficiency;
}

//-----------------------------------------------------------------------------
// Purpose: override how single player rays hit the player
//-----------------------------------------------------------------------------
bool LineCircleIntersection(
	const Vector2D &center,
	const float radius,
	const Vector2D &vLinePt,
	const Vector2D &vLineDir,
	float *fIntersection1,
	float *fIntersection2)
{
	// Line = P + Vt
	// Sphere = r (assume we've translated to origin)
	// (P + Vt)^2 = r^2
	// VVt^2 + 2PVt + (PP - r^2)
	// Solve as quadratic:  (-b  +/-  sqrt(b^2 - 4ac)) / 2a
	// If (b^2 - 4ac) is < 0 there is no solution.
	// If (b^2 - 4ac) is = 0 there is one solution (a case this function doesn't support).
	// If (b^2 - 4ac) is > 0 there are two solutions.
	Vector2D P;
	float a, b, c, sqr, insideSqr;


	// Translate circle to origin.
	P[0] = vLinePt[0] - center[0];
	P[1] = vLinePt[1] - center[1];

	a = vLineDir.Dot(vLineDir);
	b = 2.0f * P.Dot(vLineDir);
	c = P.Dot(P) - (radius * radius);

	insideSqr = b*b - 4 * a*c;
	if (insideSqr <= 0.000001f)
		return false;

	// Ok, two solutions.
	sqr = (float)FastSqrt(insideSqr);

	float denom = 1.0 / (2.0f * a);

	*fIntersection1 = (-b - sqr) * denom;
	*fIntersection2 = (-b + sqr) * denom;

	return true;
}

static void Collision_ClearTrace(const Vector &vecRayStart, const Vector &vecRayDelta, CBaseTrace *pTrace)
{
	pTrace->startpos = vecRayStart;
	pTrace->endpos = vecRayStart;
	pTrace->endpos += vecRayDelta;
	pTrace->startsolid = false;
	pTrace->allsolid = false;
	pTrace->fraction = 1.0f;
	pTrace->contents = 0;
}


bool IntersectRayWithAACylinder(const Ray_t &ray,
	const Vector &center, float radius, float height, CBaseTrace *pTrace)
{
	Assert(ray.m_IsRay);
	Collision_ClearTrace(ray.m_Start, ray.m_Delta, pTrace);

	// First intersect the ray with the top + bottom planes
	float halfHeight = height * 0.5;

	// Handle parallel case
	Vector vStart = ray.m_Start - center;
	Vector vEnd = vStart + ray.m_Delta;

	float flEnterFrac, flLeaveFrac;
	if (FloatMakePositive(ray.m_Delta.z) < 1e-8)
	{
		if ((vStart.z < -halfHeight) || (vStart.z > halfHeight))
		{
			return false; // no hit
		}
		flEnterFrac = 0.0f; flLeaveFrac = 1.0f;
	}
	else
	{
		// Clip the ray to the top and bottom of box
		flEnterFrac = IntersectRayWithAAPlane(vStart, vEnd, 2, 1, halfHeight);
		flLeaveFrac = IntersectRayWithAAPlane(vStart, vEnd, 2, 1, -halfHeight);

		if (flLeaveFrac < flEnterFrac)
		{
			float temp = flLeaveFrac;
			flLeaveFrac = flEnterFrac;
			flEnterFrac = temp;
		}

		if (flLeaveFrac < 0 || flEnterFrac > 1)
		{
			return false;
		}
	}

	// Intersect with circle
	float flCircleEnterFrac, flCircleLeaveFrac;
	if (!LineCircleIntersection(vec3_origin.AsVector2D(), radius,
		vStart.AsVector2D(), ray.m_Delta.AsVector2D(), &flCircleEnterFrac, &flCircleLeaveFrac))
	{
		return false; // no hit
	}

	Assert(flCircleEnterFrac <= flCircleLeaveFrac);
	if (flCircleLeaveFrac < 0 || flCircleEnterFrac > 1)
	{
		return false;
	}

	if (flEnterFrac < flCircleEnterFrac)
		flEnterFrac = flCircleEnterFrac;
	if (flLeaveFrac > flCircleLeaveFrac)
		flLeaveFrac = flCircleLeaveFrac;

	if (flLeaveFrac < flEnterFrac)
		return false;

	VectorMA(ray.m_Start, flEnterFrac, ray.m_Delta, pTrace->endpos);
	pTrace->fraction = flEnterFrac;
	pTrace->contents = CONTENTS_SOLID;

	// Calculate the point on our center line where we're nearest the intersection point
	Vector collisionCenter;
	CalcClosestPointOnLineSegment(pTrace->endpos, center + Vector(0, 0, halfHeight), center - Vector(0, 0, halfHeight), collisionCenter);

	// Our normal is the direction from that center point to the intersection point
	pTrace->plane.normal = pTrace->endpos - collisionCenter;
	VectorNormalize(pTrace->plane.normal);

	return true;
}


bool CHL2_Player::TestHitboxes(const Ray_t &ray, unsigned int fContentsMask, trace_t& tr)
{
	if (g_pGameRules->IsMultiplayer())
	{
		return BaseClass::TestHitboxes(ray, fContentsMask, tr);
	}
	else
	{
		Assert(ray.m_IsRay);

		Vector mins, maxs;

		mins = WorldAlignMins();
		maxs = WorldAlignMaxs();

		if (IntersectRayWithAACylinder(ray, WorldSpaceCenter(), maxs.x * PLAYER_HULL_REDUCTION, maxs.z - mins.z, &tr))
		{
			tr.hitbox = 0;
			CStudioHdr *pStudioHdr = GetModelPtr();
			if (!pStudioHdr)
				return false;

			mstudiohitboxset_t *set = pStudioHdr->pHitboxSet(m_nHitboxSet);
			if (!set || !set->numhitboxes)
				return false;

			mstudiobbox_t *pbox = set->pHitbox(tr.hitbox);
			mstudiobone_t *pBone = pStudioHdr->pBone(pbox->bone);
			tr.surface.name = "**studio**";
			tr.surface.flags = SURF_HITBOX;
			tr.surface.surfaceProps = physprops->GetSurfaceIndex(pBone->pszSurfaceProp());
		}

		return true;
	}
}

//---------------------------------------------------------
// Show the player's scaled down bbox that we use for
// bullet impacts.
//---------------------------------------------------------
void CHL2_Player::DrawDebugGeometryOverlays(void)
{
	BaseClass::DrawDebugGeometryOverlays();

	if (m_debugOverlays & OVERLAY_BBOX_BIT)
	{
		Vector mins, maxs;

		mins = WorldAlignMins();
		maxs = WorldAlignMaxs();

		mins.x *= PLAYER_HULL_REDUCTION;
		mins.y *= PLAYER_HULL_REDUCTION;

		maxs.x *= PLAYER_HULL_REDUCTION;
		maxs.y *= PLAYER_HULL_REDUCTION;

		NDebugOverlay::Box(GetAbsOrigin(), mins, maxs, 255, 0, 0, 100, 0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Helper to remove from ladder
//-----------------------------------------------------------------------------
void CHL2_Player::ExitLadder()
{
	if (MOVETYPE_LADDER != GetMoveType())
		return;

	SetMoveType(MOVETYPE_WALK);
	SetMoveCollide(MOVECOLLIDE_DEFAULT);
	// Remove from ladder
	m_HL2Local.m_hLadder.Set(NULL);
}

surfacedata_t *CHL2_Player::GetLadderSurface(const Vector &origin)
{
	extern const char *FuncLadder_GetSurfaceprops(CBaseEntity *pLadderEntity);

	CBaseEntity *pLadder = m_HL2Local.m_hLadder.Get();
	if (pLadder) // THIS is used with entity ladders
	{
		const char *pSurfaceprops = FuncLadder_GetSurfaceprops(pLadder);
		// get ladder material from func_ladder
		return physprops->GetSurfaceData(physprops->GetSurfaceIndex(pSurfaceprops));

	}
#ifdef DARKINTERVAL
	else // THIS will happen with brush/displacement ladders
	{
		return physprops->GetSurfaceData(physprops->GetSurfaceIndex("web")); // TODO: somehow figure out how to support multiple materials. 
																			 // Right now this is only needed for climbable webs in Dark Interval. 
	}
#endif
	return BaseClass::GetLadderSurface(origin);
}

//-----------------------------------------------------------------------------
// Purpose: Queues up a use deny sound, played in ItemPostFrame.
//-----------------------------------------------------------------------------
void CHL2_Player::PlayUseDenySound()
{
	m_bPlayUseDenySound = true;
}

void CHL2_Player::ItemPostFrame()
{
	BaseClass::ItemPostFrame();

	if (m_bPlayUseDenySound)
	{
		m_bPlayUseDenySound = false;
		EmitSound("HL2Player.UseDeny");
	}
}

void CHL2_Player::StartWaterDeathSounds(void)
{
	CPASAttenuationFilter filter(this);

	if (m_sndLeeches == NULL)
	{
		m_sndLeeches = (CSoundEnvelopeController::GetController()).SoundCreate(filter, entindex(), CHAN_STATIC, "coast.leech_bites_loop", ATTN_NORM);
	}

	if (m_sndLeeches)
	{
		(CSoundEnvelopeController::GetController()).Play(m_sndLeeches, 1.0f, 100);
	}

	if (m_sndWaterSplashes == NULL)
	{
		m_sndWaterSplashes = (CSoundEnvelopeController::GetController()).SoundCreate(filter, entindex(), CHAN_STATIC, "coast.leech_water_churn_loop", ATTN_NORM);
	}

	if (m_sndWaterSplashes)
	{
		(CSoundEnvelopeController::GetController()).Play(m_sndWaterSplashes, 1.0f, 100);
	}
}

void CHL2_Player::StopWaterDeathSounds(void)
{
	if (m_sndLeeches)
	{
		(CSoundEnvelopeController::GetController()).SoundFadeOut(m_sndLeeches, 0.5f, true);
		m_sndLeeches = NULL;
	}

	if (m_sndWaterSplashes)
	{
		(CSoundEnvelopeController::GetController()).SoundFadeOut(m_sndWaterSplashes, 0.5f, true);
		m_sndWaterSplashes = NULL;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CHL2_Player::MissedAR2AltFire()
{
	if (GetPlayerProxy() != NULL)
	{
		GetPlayerProxy()->m_PlayerMissedAR2AltFire.FireOutput(this, this);
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CHL2_Player::DisplayLadderHudHint()
{
#if !defined( CLIENT_DLL )
	if (CURTIME > m_flTimeNextLadderHint)
	{
		m_flTimeNextLadderHint = CURTIME + 60.0f;

		CFmtStr hint;
		hint.sprintf("#Valve_Hint_Ladder");
		UTIL_HudHintText(this, hint.Access());
	}
#endif//CLIENT_DLL
}

//-----------------------------------------------------------------------------
// Shuts down sounds
//-----------------------------------------------------------------------------
void CHL2_Player::StopLoopingSounds(void)
{
	if (m_sndLeeches != NULL)
	{
		(CSoundEnvelopeController::GetController()).SoundDestroy(m_sndLeeches);
		m_sndLeeches = NULL;
	}

	if (m_sndWaterSplashes != NULL)
	{
		(CSoundEnvelopeController::GetController()).SoundDestroy(m_sndWaterSplashes);
		m_sndWaterSplashes = NULL;
	}

	BaseClass::StopLoopingSounds();
}

//-----------------------------------------------------------------------------
void CHL2_Player::ModifyOrAppendPlayerCriteria(AI_CriteriaSet& set)
{
	BaseClass::ModifyOrAppendPlayerCriteria(set);

	if (GlobalEntity_GetIndex("gordon_precriminal") == -1)
	{
		set.AppendCriteria("gordon_precriminal", "0");
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const impactdamagetable_t &CHL2_Player::GetPhysicsImpactDamageTable()
{
	if (m_bUseCappedPhysicsDamageTable)
		return gCappedPlayerImpactDamageTable;

	return BaseClass::GetPhysicsImpactDamageTable();
}

//-----------------------------------------------------------------------------
// Purpose: Makes a splash when the player transitions between water states
//-----------------------------------------------------------------------------
void CHL2_Player::Splash(void)
{
	CEffectData data;
	data.m_fFlags = 0;
	data.m_vOrigin = GetAbsOrigin();
	data.m_vNormal = Vector(0, 0, 1);
	data.m_vAngles = QAngle(0, 0, 0);

	if (GetWaterType() & CONTENTS_SLIME)
	{
		data.m_fFlags |= FX_WATER_IN_SLIME;
	}

	float flSpeed = GetAbsVelocity().Length();
	if (flSpeed < 300)
	{
		data.m_flScale = random->RandomFloat(10, 12);
		DispatchEffect("waterripple", data);
	}
	else
	{
		data.m_flScale = random->RandomFloat(6, 8);
		DispatchEffect("watersplash", data);
	}
}

CLogicPlayerProxy *CHL2_Player::GetPlayerProxy(void)
{
	CLogicPlayerProxy *pProxy = dynamic_cast< CLogicPlayerProxy* > (m_hPlayerProxy.Get());

	if (pProxy == NULL)
	{
		pProxy = (CLogicPlayerProxy*)gEntList.FindEntityByClassname(NULL, "logic_playerproxy");

		if (pProxy == NULL)
			return NULL;

		pProxy->m_hPlayer = this;
		m_hPlayerProxy = pProxy;
	}

	return pProxy;
}

void CHL2_Player::FirePlayerProxyOutput(const char *pszOutputName, variant_t variant, CBaseEntity *pActivator, CBaseEntity *pCaller)
{
	if (GetPlayerProxy() == NULL)
		return;

	GetPlayerProxy()->FireNamedOutput(pszOutputName, variant, pActivator, pCaller);
}
#ifdef DARKINTERVAL
void CHL2_Player::BreakCurrentWeapon(CBaseCombatWeapon *pWeapon)
{
	if (!pWeapon) return;
	if (pWeapon == NULL) return;

	pWeapon->BreakWeapon();
}

void CHL2_Player::UpdateLocator(bool forceUpdate)
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	if (!pPlayer || !pPlayer->IsSuitEquipped())
		return;

	if (!forceUpdate && CURTIME < m_flNextLocatorUpdateTime)
		return;

	// Count the targets on radar. If any more targets come on the radar, we beep.
	int m_iNumOldRadarContacts = m_HL2Local.m_iNumLocatorContacts;

	m_flNextLocatorUpdateTime = CURTIME + LOCATOR_UPDATE_FREQUENCY;
	m_HL2Local.m_iNumLocatorContacts = 0;

	CBaseEntity *pEnt = gEntList.FirstEnt();

	string_t iszPointOfInterest = FindPooledString("info_locator_interest");
	string_t iszOutpost = FindPooledString("info_locator_combine");
	string_t iszMainObjective = FindPooledString("info_locator_objective");

	while (pEnt != NULL)
	{
		int type = LOCATOR_CONTACT_NONE;

		//	DevMsg( "className: %s\n", pEnt->m_iClassname );

		if (pEnt->ClassMatches(iszPointOfInterest))
		{
			type = LOCATOR_CONTACT_POINT_OF_INTEREST;
			//	ConColorMsg(Color(100,200,255,255),"Found point of interest at %.02f %.02f %.02f, player at %.02f %.02f %.02f\n", 
			//		pEnt->WorldSpaceCenter().x, pEnt->WorldSpaceCenter().y, pEnt->WorldSpaceCenter().z,
			//		WorldSpaceCenter().x, WorldSpaceCenter().y, WorldSpaceCenter().z);
		}
		else if (pEnt->m_iClassname == iszOutpost)
		{
			type = LOCATOR_CONTACT_OUTPOST;
		}
		else if (pEnt->m_iClassname == iszMainObjective)
		{
			type = LOCATOR_CONTACT_MAIN_OBJECTIVE;
		}

		if (type != RADAR_CONTACT_NONE)
		{
			Vector vecPos = pEnt->GetAbsOrigin();
			float x_diff = vecPos.x - pPlayer->GetAbsOrigin().x;
			float y_diff = vecPos.y - pPlayer->GetAbsOrigin().y;
			float flDist = sqrt(pow(x_diff, 2) + pow(y_diff, 2));

			if (flDist <= 60000 || type == LOCATOR_CONTACT_MAIN_OBJECTIVE)
			{
				// DevMsg( "Adding %s : %s to locator list.\n", pEnt->GetClassname(), pEnt->GetEntityName() );

				m_HL2Local.m_locatorEnt.Set(m_HL2Local.m_iNumLocatorContacts, pEnt);
				m_HL2Local.m_iLocatorContactType.Set(m_HL2Local.m_iNumLocatorContacts, type);
				m_HL2Local.m_iNumLocatorContacts++;

				if (m_HL2Local.m_iNumLocatorContacts == LOCATOR_MAX_CONTACTS)
					break;
			}
		}

		pEnt = gEntList.NextEnt(pEnt);
	}

	if( m_HL2Local.m_iNumLocatorContacts > m_iNumOldRadarContacts )
	{
		EmitSound( "Locator.NewContactBeep" );
	}

	CSingleUserRecipientFilter filter(pPlayer);
	UserMessageBegin(filter, "UpdatePlayerLocator");
	WRITE_BYTE(0); // end marker
	MessageEnd();	// send message
}
#endif // DARKINTERVAL

LINK_ENTITY_TO_CLASS(logic_playerproxy, CLogicPlayerProxy);

BEGIN_DATADESC(CLogicPlayerProxy)
	DEFINE_OUTPUT(m_OnFlashlightOn, "OnFlashlightOn"),
	DEFINE_OUTPUT(m_OnFlashlightOff, "OnFlashlightOff"),
	DEFINE_OUTPUT(m_RequestedPlayerHealth, "PlayerHealth"),
#ifdef DARKINTERVAL
	DEFINE_OUTPUT(m_RequestedPlayerArmor, "PlayerArmor"),
#endif
	DEFINE_OUTPUT(m_PlayerHasAmmo, "PlayerHasAmmo"),
	DEFINE_OUTPUT(m_PlayerHasNoAmmo, "PlayerHasNoAmmo"),
	DEFINE_OUTPUT(m_PlayerDied, "PlayerDied"),
#ifdef DARKINTERVAL
	DEFINE_OUTPUT(m_PlayerDamaged, "PlayerDamaged"),
#endif
	DEFINE_OUTPUT(m_PlayerMissedAR2AltFire, "PlayerMissedAR2AltFire"),
	DEFINE_INPUTFUNC(FIELD_VOID, "RequestPlayerHealth", InputRequestPlayerHealth),
	DEFINE_INPUTFUNC(FIELD_VOID, "SetFlashlightSlowDrain", InputSetFlashlightSlowDrain),
	DEFINE_INPUTFUNC(FIELD_VOID, "SetFlashlightNormalDrain", InputSetFlashlightNormalDrain),
	DEFINE_INPUTFUNC(FIELD_INTEGER, "SetPlayerHealth", InputSetPlayerHealth),
#ifdef DARKINTERVAL
	DEFINE_INPUTFUNC(FIELD_INTEGER, "SetPlayerArmor", InputSetPlayerArmor),
#endif
	DEFINE_INPUTFUNC(FIELD_VOID, "RequestAmmoState", InputRequestAmmoState),
	DEFINE_INPUTFUNC(FIELD_VOID, "LowerWeapon", InputLowerWeapon),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableCappedPhysicsDamage", InputEnableCappedPhysicsDamage),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableCappedPhysicsDamage", InputDisableCappedPhysicsDamage),
	DEFINE_INPUTFUNC(FIELD_STRING, "SetLocatorTargetEntity", InputSetLocatorTargetEntity),
#ifndef DARKINTERVAL // yes, disabled in DI compile, but present in HL2
#ifdef PORTAL_COMPILE
	DEFINE_INPUTFUNC(FIELD_VOID, "SuppressCrosshair", InputSuppressCrosshair),
#endif
#endif
#ifdef DARKINTERVAL
	DEFINE_INPUTFUNC(FIELD_VOID, "RequestPlayerArmor", InputRequestPlayerArmor),
	DEFINE_INPUTFUNC(FIELD_VOID, "SetFlashlightFastDrain", InputSetFlashlightFastDrain),
	DEFINE_INPUTFUNC(FIELD_VOID, "SetFlashlightSuperFastDrain", InputSetFlashlightSuperFastDrain),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableFlashlightFlicker", InputEnableFlashlightFlicker),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableFlashlightFlicker", InputDisableFlashlightFlicker),
	DEFINE_INPUTFUNC(FIELD_VOID, "BreakCurrentWeapon", InputBreakCurrentWeapon),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableFlashlight", InputDisableFlashlight),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableFlashlight", InputEnableFlashlight),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableSuitComments", InputDisableSuitComments),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableSuitComments", InputEnableSuitComments),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableTinnitus", InputDisableTinnitus),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableTinnitus", InputEnableTinnitus),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableWeapons", InputDisableWeapons),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableWeapons", InputEnableWeapons),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableSuitlessHUD", InputDisableSuitlessHUD),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableSuitlessHUD", InputEnableSuitlessHUD),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableLocator", InputDisableLocator),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableLocator", InputEnableLocator),
	DEFINE_INPUTFUNC(FIELD_VOID, "AddBreather", InputAddBreather),
	DEFINE_INPUTFUNC(FIELD_VOID, "RemoveBreather", InputRemoveBreather),
#endif // DARKINTERVAL
/*
#ifdef PORTAL_COMPILE
	
#endif // PORTAL
*/
	DEFINE_FIELD(m_hPlayer, FIELD_EHANDLE),
END_DATADESC()

void CLogicPlayerProxy::Activate(void)
{
	BaseClass::Activate();

	if (m_hPlayer == NULL)
	{
		m_hPlayer = AI_GetSinglePlayer();
	}
}

bool CLogicPlayerProxy::PassesDamageFilter(const CTakeDamageInfo &info)
{
	if (m_hDamageFilter)
	{
		CBaseFilter *pFilter = (CBaseFilter *)(m_hDamageFilter.Get());
		return pFilter->PassesDamageFilter(info);
	}

	return true;
}

void CLogicPlayerProxy::InputSetPlayerHealth(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	m_hPlayer->SetHealth(inputdata.value.Int());

}
#ifdef DARKINTERVAL
void CLogicPlayerProxy::InputSetPlayerArmor(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	if (pPlayer && pPlayer->IsSuitEquipped())
	{
		pPlayer->SetArmorValue(inputdata.value.Int());
	}

}
#endif
void CLogicPlayerProxy::InputRequestPlayerHealth(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	m_RequestedPlayerHealth.Set(m_hPlayer->GetHealth(), inputdata.pActivator, inputdata.pCaller);
}

void CLogicPlayerProxy::InputSetFlashlightSlowDrain(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer)
		pPlayer->SetFlashlightPowerDrainScale(2.1f);
}

void CLogicPlayerProxy::InputSetFlashlightNormalDrain(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer)
		pPlayer->SetFlashlightPowerDrainScale(1.0f);
}
#ifdef DARKINTERVAL
void CLogicPlayerProxy::InputDisableFlashlight(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer)
	{
		if (pPlayer->FlashlightIsOn())
			pPlayer->FlashlightTurnOff();
		pPlayer->SetFlashlightEnabled(false);
	}
}

void CLogicPlayerProxy::InputEnableFlashlight(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer)
		pPlayer->SetFlashlightEnabled(true);
}
#endif // DARKINTERVAL
void CLogicPlayerProxy::InputRequestAmmoState(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	for (int i = 0; i < pPlayer->WeaponCount(); ++i)
	{
		CBaseCombatWeapon* pCheck = pPlayer->GetWeapon(i);

		if (pCheck)
		{
			if (pCheck->HasAnyAmmo() && (pCheck->UsesPrimaryAmmo() || pCheck->UsesSecondaryAmmo()))
			{
				m_PlayerHasAmmo.FireOutput(this, this, 0);
				return;
			}
		}
	}

	m_PlayerHasNoAmmo.FireOutput(this, this, 0);
}

void CLogicPlayerProxy::InputLowerWeapon(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	pPlayer->Weapon_Lower();
}

void CLogicPlayerProxy::InputEnableCappedPhysicsDamage(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	pPlayer->EnableCappedPhysicsDamage();
}

void CLogicPlayerProxy::InputDisableCappedPhysicsDamage(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	pPlayer->DisableCappedPhysicsDamage();
}

void CLogicPlayerProxy::InputSetLocatorTargetEntity(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CBaseEntity *pTarget = NULL; // assume no target
	string_t iszTarget = MAKE_STRING(inputdata.value.String());

	if (iszTarget != NULL_STRING)
	{
		pTarget = gEntList.FindEntityByName(NULL, iszTarget);
	}

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	pPlayer->SetLocatorTargetEntity(pTarget);
}
/*
#ifdef PORTAL_COMPILE
void CLogicPlayerProxy::InputSuppressCrosshair( inputdata_t &inputdata )
{
if( m_hPlayer == NULL )
return;

CHL2_Player *pPlayer = ToHL2Player(m_hPlayer.Get());
pPlayer->SuppressCrosshair( true );
}
#endif // PORTAL
*/
#ifdef DARKINTERVAL
void CLogicPlayerProxy::InputRequestPlayerArmor(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	if (pPlayer && pPlayer->IsSuitEquipped())
	{
		m_RequestedPlayerArmor.Set(pPlayer->ArmorValue(), inputdata.pActivator, inputdata.pCaller);
	}
}

void CLogicPlayerProxy::InputDisableLocator(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer)
		pPlayer->m_Local.m_bLocatorVisible = false;
}

void CLogicPlayerProxy::InputEnableLocator(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer)
		pPlayer->m_Local.m_bLocatorVisible = true;
}

void CLogicPlayerProxy::InputBreakCurrentWeapon(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();

	if (pPlayer && pWeapon)
		pPlayer->BreakCurrentWeapon(pWeapon);
}

void CLogicPlayerProxy::InputEnableTinnitus(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer)
	{
		CSingleUserRecipientFilter user(pPlayer);
		enginesound->SetPlayerDSP(user, snd_disable_tinnitus.GetBool() ? 34 : 37, false); // 34 is deafen without ringing
	}
}

void CLogicPlayerProxy::InputDisableTinnitus(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer)
	{
		CSingleUserRecipientFilter user(pPlayer);
		enginesound->SetPlayerDSP(user, 0, false);
	}
}

void CLogicPlayerProxy::InputEnableSuitComments(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer && pPlayer->IsSuitEquipped())
	{
		pPlayer->SetSuitCommentsEnabled(true);
	}
}

void CLogicPlayerProxy::InputDisableSuitComments(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer && pPlayer->IsSuitEquipped())
	{
		pPlayer->SetSuitCommentsEnabled(false);
	}
}

void CLogicPlayerProxy::InputEnableWeapons(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer && pPlayer->IsAlive() )
	{
		pPlayer->SetActiveWeapon(pPlayer->Weapon_GetLast());
		if (pPlayer->GetActiveWeapon() && pPlayer->GetActiveWeapon()->IsWeaponVisible() == false)
		{
			pPlayer->GetActiveWeapon()->Deploy();
			pPlayer->GetActiveWeapon()->SetWeaponVisible(true);
		}
		else
		{
			pPlayer->SwitchToNextBestWeapon(NULL);
		}

		pPlayer->m_Local.m_suppressing_weapons_bool = false;
		pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_WEAPONSELECTION;
	}
}

void CLogicPlayerProxy::InputDisableWeapons(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer && pPlayer->IsAlive())
	{
		CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
		if (pWeapon != NULL)
		{
			pWeapon->Holster(NULL);
			pWeapon->SetWeaponVisible(false);
			pPlayer->Weapon_SetLast(pWeapon);
			pPlayer->ClearActiveWeapon();
		}

		pPlayer->m_Local.m_suppressing_weapons_bool = true;
		pPlayer->m_Local.m_iHideHUD |= HIDEHUD_WEAPONSELECTION;
	}
}

void CLogicPlayerProxy::InputEnableSuitlessHUD(inputdata_t &inputdata)
{
	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer && pPlayer->IsAlive())
	{
		pPlayer->m_Local.m_bSuitlessHUDVisible = true;
	}
}

void CLogicPlayerProxy::InputDisableSuitlessHUD(inputdata_t &inputdata)
{
	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer && pPlayer->IsAlive())
	{
		pPlayer->m_Local.m_bSuitlessHUDVisible = false;
	}
}

void CLogicPlayerProxy::InputEnableFlashlightFlicker(inputdata_t &inputdata)
{
	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	if (pPlayer) pPlayer->SetForceFlashlightFlicker(true);
}

void CLogicPlayerProxy::InputDisableFlashlightFlicker(inputdata_t &inputdata)
{
	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());
	if (pPlayer) pPlayer->SetForceFlashlightFlicker(false);
}

void CLogicPlayerProxy::InputSetFlashlightFastDrain(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer)
		pPlayer->SetFlashlightPowerDrainScale(0.5f);
}

void CLogicPlayerProxy::InputSetFlashlightSuperFastDrain(inputdata_t &inputdata)
{
	if (m_hPlayer == NULL)
		return;

	CHL2_Player *pPlayer = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (pPlayer)
		pPlayer->SetFlashlightPowerDrainScale(0.2f);
}

void CLogicPlayerProxy::InputAddBreather(inputdata_t &inputdata)
{
	if (!m_hPlayer) return;

	CHL2_Player *player = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (player)
	{
		player->SuitPower_AddDevice(SuitDeviceBreather);
	}
}

void CLogicPlayerProxy::InputRemoveBreather(inputdata_t &inputdata)
{
	if (!m_hPlayer) return;

	CHL2_Player *player = dynamic_cast<CHL2_Player*>(m_hPlayer.Get());

	if (player)
	{
		player->SuitPower_RemoveDevice(SuitDeviceBreather);
	}
}
#endif // DARKINTERVAL
////////////////////////////////////////////////////////////////////////
#ifdef DARKINTERVAL
class CLocatorMarkerBase : public CBaseCombatCharacter
{
	DECLARE_CLASS(CLocatorMarkerBase, CBaseCombatCharacter);
public:
	DECLARE_DATADESC();

	CLocatorMarkerBase() { };
	~CLocatorMarkerBase() {};
	Class_T Classify(void)
	{
		return CLASS_NONE;
	}
	virtual void	Precache(void) { PrecacheModel("models/player.mdl"); }
	virtual void	Spawn(void)
	{
		Precache();
		SetModel("models/player.mdl");
		SetRenderMode(kRenderNone);
		AddEffects(EF_NOSHADOW | EF_NORECEIVESHADOW);
		SetTransmitState(FL_EDICT_ALWAYS);
		UTIL_SetSize(this, Vector(-2, -2, -2), Vector(2, 2, 2));
		SetSolid(SOLID_BBOX);
		AddSolidFlags(FSOLID_NOT_SOLID);
		SetHealth(1000);
		SetMaxHealth(1000);
		m_takedamage = DAMAGE_NO;
		SetBloodColor(DONT_BLEED);
	}
	virtual void	Activate(void) { BaseClass::Activate(); }
	//	virtual void	InputEnable(inputdata_t &inputdata)
	//	{
	//	}
	//	virtual void	InputDisable(inputdata_t &inputdata)
	//	{
	//	}
private:
	int				m_memorytemp;
};

LINK_ENTITY_TO_CLASS(info_locator_basic, CLocatorMarkerBase);
BEGIN_DATADESC(CLocatorMarkerBase)
	DEFINE_FIELD(m_memorytemp, FIELD_INTEGER),
	//DEFINE_INPUTFUNC(FIELD_VOID, "Enable", InputEnable),
	//DEFINE_INPUTFUNC(FIELD_VOID, "Disable", InputDisable),
END_DATADESC()

class CLocatorMarkerCombine : public CLocatorMarkerBase
{
	DECLARE_CLASS(CLocatorMarkerCombine, CLocatorMarkerBase);
public:
	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(info_locator_combine, CLocatorMarkerCombine);
BEGIN_DATADESC(CLocatorMarkerCombine)
END_DATADESC()

class CLocatorMarkerInterest : public CLocatorMarkerBase
{
	DECLARE_CLASS(CLocatorMarkerInterest, CLocatorMarkerBase);
public:
	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(info_locator_interest, CLocatorMarkerInterest);
BEGIN_DATADESC(CLocatorMarkerInterest)
END_DATADESC()

class CLocatorMarkerObjective : public CLocatorMarkerBase
{
	DECLARE_CLASS(CLocatorMarkerObjective, CLocatorMarkerBase);
public:
	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(info_locator_objective, CLocatorMarkerObjective);
BEGIN_DATADESC(CLocatorMarkerObjective)
END_DATADESC()
#endif // DARKINTERVAL