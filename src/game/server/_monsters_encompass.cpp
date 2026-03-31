#include "cbase.h"
//#define RIDEABLE_NPCS // allows the player to get on top of crab synths, if they're not hostile
#include "_monsters_encompass.h"
#include "_sharedents_encompass.h"
#include "darkinterval\npc_parse.h"
// #include "ai_basenpc.h" /// already included through ai_baseactor.h
#include "ai_baseactor.h"
//#include "ai_basenpc_physicsflyer.h" /// already included through npc_basescanner.h - though may want to get rid of that
#include "ai_behavior_actbusy.h"
#include "ai_behavior_assault.h"
#include "ai_behavior_follow.h"
//#include "ai_blended_movement.h" /// already included through ai_baseactor.h
#include "ai_hint.h"
#include "ai_interactions.h"
#include "ai_moveprobe.h"
#include "ai_node.h"
#include "ai_senses.h"
#include "ai_sentence.h"
#include "ai_squad.h"
#include "ai_tacticalservices.h"
#include "ai_waypoint.h"
#include "ammodef.h"
#include "ar2_explosion.h" // for dusty explosions
#include "collisionutils.h"
#include "env_explosion.h"
#include "darkinterval\env_gravity_vortex.h" // for particle storm
#include "env_particlesmokegrenade.h"
#include "env_spritetrail.h"
#include "filesystem.h"
#include "filters.h"
#include "fire.h"
#include "gib.h"
#include "globalstate.h"
#include "grenade_homer.h"
#include "hl2_shareddefs.h"
#include "datacache/imdlcache.h"
#include "items.h"
#include "iservervehicle.h"
#include "func_brush.h"
#include "model_types.h"
#include "npcevent.h"
//#include "npc_BaseZombie.h" /// was only used in barnacle code to get bodygroup number (ZOMBIE_BODYGROUP_HEADCRAB).
//#include "npc_combine.h" /// not used in code below, currently
#include "npc_combine_mine.h"
#include "particle_system.h"
#include "physics_bone_follower.h"
#include "physics_prop_ragdoll.h"
#include "physics_saverestore.h"
#include "physobj.h"
#include "point_tesla.h" // for particle storm
//#include "player_pickup.h" /// already included through items.h
#include "props.h"
#include "prop_combine_ball.h" // needed for UTIL_IsCombineBall determination
#include "prop_scalable.h" // for scanner forcefield shields, and polyp smoke clouds
#include "sceneentity.h"
#include "shot_manipulator.h" // for applying angle spread in shooting (e. g. hunter fletchettes shoot call)
//#include "soundenvelope.h" /// already included through npc_bazezombie - though may want to get rid of that
#include "spotlightend.h"
#include "studio.h"
//#include "te_effect_dispatch.h"
#include "saverestore_utlvector.h"
#ifdef RIDEABLE_NPCS
#include "in_buttons.h"
#include "vehicle_base.h"
#endif
#include "world.h"

#include "npc_basescanner.h"
#include "darkinterval\grenade_toxiccloud.h"

#include "tier0\memdbgon.h"

// extern convars - may be shared between several npcs
extern ConVar sk_squid_spit_speed;
extern ConVar sk_immolator_dmg;
extern Vector PointOnLineNearestPoint(const Vector& vStartPos, const Vector& vEndPos, const Vector& vPoint);

float GetCurrentGravity(void);

// Shared animevents - TODO add more of these 
int AE_LAUNCH_PROJECTILE;
int AE_LAUNCH_STREAM;
int AE_CEASE_STREAM;
int AE_MELEE_CONTACT;

// Shared activities - TODO add more of these
Activity ACT_CHARGE_START;
Activity ACT_CHARGE_LOOP;
Activity ACT_CHARGE_END;
Activity ACT_CARRIED_BY_DROPSHIP;

// Shared effects and projectiles
extern short	g_sModelIndexFireball;

// Memo:
//=============================================================================//
//
// To give an NPC the ability to shoot while moving:
//
// - In the NPC's Spawn function, add:
//		CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
//
// - The NPC must either have a weapon (return non-NULL from GetActiveWeapon)
//	  or must have bits_CAP_INNATE_RANGE_ATTACK1 or bits_CAP_INNATE_RANGE_ATTACK2.
//
// - Support the activities ACT_WALK_AIM and/or ACT_RUN_AIM in the NPC.
//
// - Support the activity ACT_GESTURE_RANGE_ATTACK1 as a gesture that plays
//	 over ACT_WALK_AIM and ACT_RUN_AIM.
//
//=============================================================================

// Memo:
//=============================================================================//
//
// We can make blended-moving NPCs strafe while facing the enemy, by using 
// OverrideMoveFacing function. It works by itself (handled by base class) 
// and no extra initilisation is required. Examples are in CNPC_Antlion
// and CNPC_Squid.
// 
//
// It also works on NPCs that move with FL_FLY. Example is CNPC_GliderSynth.
// So it either requires blended movement enabled in the model,
// and the NPC being a CAI_BlendedNPC; or the NPC being moved by
// OverrideMove and other associated functions, and not depending on
// root bone movement.
// 
// 
//
//=============================================================================
#define ASSASSIN
#define CREMATOR
#define BULLSQUID
#define WATERSQUID
#define HOUNDEYE
#define POLYP
#define PROTOZOA
#define XENTREE
#define CRABSYNTH
#define HOPPER
#define MINIBARNACLE
#define GLIDERSYNTH
#define SYNTHSCANNER
#define STINGER
#define FLOATER
#define MOBILEMINE
#define BEETURRET
#define PSTORM

#if defined CRABSYNTH
//-----------------------------------------------------------------------------
// A simple trace filter class to skip small moveable physics objects
//-----------------------------------------------------------------------------
void ChargeDamage_Inline(CBaseEntity *pHunter, CBaseEntity *pTarget, float flDamage)
{
	Vector attackDir = (pTarget->WorldSpaceCenter() - pHunter->WorldSpaceCenter());
	VectorNormalize(attackDir);
	Vector offset = RandomVector(-32, 32) + pTarget->WorldSpaceCenter();

	// Generate enough force to make a 75kg guy move away at 700 in/sec
	Vector vecForce = attackDir * ImpulseScale(75, 700);

	// Deal the damage
	CTakeDamageInfo	info(pHunter, pHunter, vecForce, offset, flDamage, DMG_CLUB);
	pTarget->TakeDamage(info);
}

class CCrabSynthTraceFilterSkipPhysics : public CTraceFilter
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE(CCrabSynthTraceFilterSkipPhysics);

	CCrabSynthTraceFilterSkipPhysics(const IHandleEntity *passentity, int collisionGroup, float minMass)
		: m_pPassEnt(passentity), m_collisionGroup(collisionGroup), m_minMass(minMass)
	{
	}
	bool ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask)
	{
		if (!StandardFilterRules(pHandleEntity, contentsMask))
			return false;

		if (!PassServerEntityFilter(pHandleEntity, m_pPassEnt))
			return false;

		// Don't test if the game code tells us we should ignore this collision...
		CBaseEntity *pEntity = EntityFromEntityHandle(pHandleEntity);
		if (pEntity)
		{
			if (!pEntity->ShouldCollide(m_collisionGroup, contentsMask))
				return false;

			if (!g_pGameRules->ShouldCollide(m_collisionGroup, pEntity->GetCollisionGroup()))
				return false;

			// don't test small moveable physics objects (unless it's an NPC)
			if (!pEntity->IsNPC() && pEntity->GetMoveType() == MOVETYPE_VPHYSICS)
			{
				float entMass = PhysGetEntityMass(pEntity);
				if (entMass < m_minMass)
				{
					if (entMass < m_minMass * 0.666f || pEntity->CollisionProp()->BoundingRadius() < (assert_cast<const CAI_BaseNPC *>(EntityFromEntityHandle(m_pPassEnt)))->GetHullHeight())
					{
						return false;
					}
				}
			}

			// If we hit an antlion, don't stop, but kill it
			if (pEntity->Classify() == CLASS_ANTLION)
			{
				CBaseEntity *myentity = (CBaseEntity *)EntityFromEntityHandle(m_pPassEnt);
				ChargeDamage_Inline(myentity, pEntity, pEntity->GetHealth());
				return false;
			}
		}

		return true;
	}

private:
	const IHandleEntity *m_pPassEnt;
	int m_collisionGroup;
	float m_minMass;
};

inline void CrabSynthTraceHull_SkipPhysics(const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &hullMin,
	const Vector &hullMax, unsigned int mask, const CBaseEntity *ignore,
	int collisionGroup, trace_t *ptr, float minMass)
{
	Ray_t ray;
	ray.Init(vecAbsStart, vecAbsEnd, hullMin, hullMax);
	CCrabSynthTraceFilterSkipPhysics traceFilter(ignore, collisionGroup, minMass);
	enginetrace->TraceRay(ray, mask, &traceFilter, ptr);
}
#endif // CRABSYNTH

//====================================
// Alien Assassin (Dark Interval)
//====================================
#if defined (ASSASSIN)
class CNPC_AssassinRollingBomb : public CAI_BaseNPC
{
};

class CNPC_Assassin : public CAI_BaseNPC
{
	DECLARE_CLASS(CNPC_Assassin, CAI_BaseNPC);
	DECLARE_DATADESC();
public:
	CNPC_Assassin(void);
	~CNPC_Assassin(void);
	Class_T Classify(void) { return CLASS_NONE; }

	void Precache(void);
	void Spawn(void);

	void CreateEyeFX();

	void Event_Killed(const CTakeDamageInfo &info);

	//	void HandleAnimEvent(animevent_t *pEvent);
	//	Vector LeftFootHit(float eventtime);
	//	Vector RightFootHit(float eventtime);
	//	void FootstepEffect(const Vector &origin);

	bool IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const;

//	CSprite *glowSprite_lefteye;
//	CSprite *glowSprite_righteye;
	CSpriteTrail *glowTrail_lefteye;
	CSpriteTrail *glowTrail_righteye;
};

static ConVar sk_assassin_jump_rise("sk_assassin_jump_rise", "2000.0");
static ConVar sk_assassin_jump_dist("sk_assassin_jump_dist", "2000.0");
static ConVar sk_assassin_jump_drop("sk_assassin_jump_drop", "2000.0");
static ConVar sk_assassin_eyetrail_lifetime("sk_assassin_eyetrail_lifetime", "0.25");
static ConVar sk_assassin_eyetrail_width("sk_assassin_eyetrail_width", "3.5");

#define ASSASSIN_EYEGLOW "sprites/glow1.vmt"
#define ASSASSIN_EYETRAIL "sprites/bluelaser1.vmt"

#define ASSASSIN_AE_LEFTSTEP 17
#define ASSASSIN_AE_RIGHTSTEP 18

// Constructor
CNPC_Assassin::CNPC_Assassin(void) : 
	glowTrail_lefteye(NULL), 
	glowTrail_righteye(NULL) 
{
	m_hNPCFileInfo = LookupNPCInfoScript("npc_assassin");
}

CNPC_Assassin::~CNPC_Assassin(void) {
//	if (glowSprite_lefteye)	UTIL_Remove(glowSprite_lefteye);
//	if (glowSprite_righteye) UTIL_Remove(glowSprite_righteye);
	if (glowTrail_lefteye)	UTIL_Remove(glowTrail_lefteye);
	if (glowTrail_righteye)	UTIL_Remove(glowTrail_righteye);
}

// Generic spawn-related
void CNPC_Assassin::Precache(void) {
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
//	PrecacheModel(ASSASSIN_EYEGLOW);
	PrecacheModel(ASSASSIN_EYETRAIL);
	BaseClass::Precache();
}

void CNPC_Assassin::Spawn(void) 
{
	Precache();
	SetModel(STRING(GetModelName()));
	BaseClass::Spawn();
	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

	SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
	SetHullSizeNormal();
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MoveType_t(GetNPCScriptData().NPC_Movement_MoveType_int));

	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_Climb_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_CLIMB);
	if ((GetNPCScriptData().NPC_Capable_Doors_boolean) == true) CapabilitiesAdd(bits_CAP_DOORS_GROUP);

	//	m_bShouldPatrol = GetNPCScriptData().bShouldPatrolAfterSpawn;

	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));

	m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);

	NPCInit();
	CreateEyeFX();
}

// Damage handling
void CNPC_Assassin::Event_Killed(const CTakeDamageInfo &info)
{
	m_nSkin = 1; //dying eyes
	BaseClass::Event_Killed(info);
}

// Movement
bool CNPC_Assassin::IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const
{
	const float MAX_JUMP_RISE = sk_assassin_jump_rise.GetFloat();
	const float MAX_JUMP_DISTANCE = sk_assassin_jump_dist.GetFloat();
	const float MAX_JUMP_DROP = sk_assassin_jump_drop.GetFloat();

	return BaseClass::IsJumpLegal(startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE);
}

// Functions used anywhere above
void CNPC_Assassin::CreateEyeFX(void)
{
	int LE = LookupAttachment("LEye");
	int RE = LookupAttachment("REye");
	float lifetime = sk_assassin_eyetrail_lifetime.GetFloat();
	float width = sk_assassin_eyetrail_width.GetFloat();
	/*
	glowSprite_lefteye = CSprite::SpriteCreate( EYEGLOW, GetLocalOrigin(), false);

	if (glowSprite_lefteye != NULL) {
	glowSprite_lefteye->FollowEntity(this);
	glowSprite_lefteye->SetAttachment(this, LE);
	glowSprite_lefteye->SetTransparency(kRenderTransAdd, 25, 200, 195, 0, kRenderFxNone);
	glowSprite_lefteye->SetScale(0.25f);
	}
	*/
	glowTrail_lefteye = CSpriteTrail::SpriteTrailCreate(ASSASSIN_EYETRAIL, GetLocalOrigin(), false);

	if (glowTrail_lefteye != NULL) {
		glowTrail_lefteye->FollowEntity(this);
		glowTrail_lefteye->SetAttachment(this, LE);
		glowTrail_lefteye->SetTransparency(kRenderTransAdd, 25, 200, 195, 200, kRenderFxNone);
		glowTrail_lefteye->SetStartWidth(width);
		glowTrail_lefteye->SetLifeTime(lifetime);
	}
	/*
	glowSprite_righteye = CSprite::SpriteCreate( EYEGLOW, GetLocalOrigin(), false);

	if (glowSprite_righteye != NULL) {
	glowSprite_righteye->FollowEntity(this);
	glowSprite_righteye->SetAttachment(this, RE);
	glowSprite_righteye->SetTransparency(kRenderTransAdd, 25, 200, 195, 0, kRenderFxNone);
	glowSprite_righteye->SetScale(0.25f);
	}
	*/
	glowTrail_righteye = CSpriteTrail::SpriteTrailCreate(ASSASSIN_EYETRAIL, GetLocalOrigin(), false);

	if (glowTrail_righteye != NULL) {
		glowTrail_righteye->FollowEntity(this);
		glowTrail_righteye->SetAttachment(this, RE);
		glowTrail_righteye->SetTransparency(kRenderTransAdd, 25, 200, 195, 200, kRenderFxNone);
		glowTrail_righteye->SetStartWidth(width);
		glowTrail_righteye->SetLifeTime(lifetime);
	}
}

// Datadesc
BEGIN_DATADESC(CNPC_Assassin)
	//DEFINE_FIELD(glowSprite_lefteye, FIELD_CLASSPTR),
	//DEFINE_FIELD(glowSprite_righteye, FIELD_CLASSPTR),
	DEFINE_FIELD(glowTrail_lefteye, FIELD_CLASSPTR),
	DEFINE_FIELD(glowTrail_righteye, FIELD_CLASSPTR),
END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_assassin, CNPC_Assassin);
#endif // ASSASSIN

//====================================
// Cremator (Dark Interval)
//====================================
#if defined (CREMATOR)
class CNPC_Cremator : public CAI_BlendedNPC
{
	DECLARE_CLASS(CNPC_Cremator, CAI_BlendedNPC);
	DECLARE_DATADESC();
protected:
	float crematorAttackRange_float = 1000;
public:
	CNPC_Cremator();
	Class_T Classify(void) { return CLASS_COMBINE; }

	void Precache(void);
	void Spawn(void);
	void RunAI(void);

	void StopAttackFX(void);
	void SelfDestructThink(void);
	void ExplodeThink(void);

	bool damageIsHeadshot;
	bool damageIsBallshot;
	bool HeadDamageIsSlice(const CTakeDamageInfo &info);
	bool HeadDamageIsHeavy(const CTakeDamageInfo &info);
	void TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr);
	int OnTakeDamage_Alive(const CTakeDamageInfo &info);
	void Event_Killed(const CTakeDamageInfo &info);
	void HandleSpecialDeath(bool headsliced, bool shouldexplode, const CTakeDamageInfo &info);
	
	void BreatheSound(void);
	void IdleSound(void);
	void AlertSound(void);

	void HandleAnimEvent(animevent_t *pEvent);
	Activity NPC_TranslateActivity(Activity activity);
	
	bool IsValidEnemy(CBaseEntity *pEnemy);
	int RangeAttack1Conditions(float flDot, float flDist);
	int RangeAttack2Conditions(float flDot, float flDist);

	void BuildScheduleTestBits(void);
	void PrescheduleThink(void);
	int SelectSchedule(void);
	
	int GetSoundInterests (void) 
	{
		return	SOUND_WORLD 
			| SOUND_COMBAT 
			| SOUND_PLAYER 
			| SOUND_DANGER 
			| SOUND_PHYSICS_DANGER 
			| SOUND_BULLET_IMPACT 
			| SOUND_MOVE_AWAY;
	};

	bool CanBecomeRagdoll(void) { return false; }

	bool m_crematorTraceAttackCone_boolean;
	bool ShouldTraceAttackCone(void) { return m_crematorTraceAttackCone_boolean; }

	bool m_crematorIgnorePlayer_boolean;
	bool ShouldIgnorePlayer(void) { return m_crematorIgnorePlayer_boolean; }

	void InputIgnorePlayer(inputdata_t &inputdata);
	void InputSelfDestruct(inputdata_t &inputdata);

	bool CheckEdgeOverstep();
	bool OverrideMoveFacing(const AILocalMoveGoal_t &move, float flInterval);
		
	DEFINE_CUSTOM_AI;

	int m_consequtiveattacks_int;
protected:
	float m_crematorBreatheSound_time;
	float m_crematorIdleSound_time;

	float m_crematorSelfDestructStart_time;
	bool m_crematorOnSelfDestruct_boolean;	// Going to blow up

	CHandle<CParticleSystem> m_crematorOnFireEffect_handle;

private:
	float m_crematorNextEdgePush_time;
};

int AE_CREMATOR_FOOTSTEPLEFT;
int AE_CREMATOR_FOOTSTEPRIGHT;
int AE_CREMATOR_IMMO_PARTICLESTART;
int AE_CREMATOR_IMMO_PARTICLESTOP;
int AE_CREMATOR_IMMO_LAUNCHFIREBALL;
int AE_CREMATOR_ATTACK_START;
int AE_CREMATOR_ATTACK_END;
int AE_CREMATOR_IDLE_START;
int AE_CREMATOR_IDLE_STOP;
int AE_CREMATOR_MOVEMENT_START;
int AE_CREMATOR_WEAPON_JAMM;
int AE_CREMATOR_FLINCH;

Activity ACT_CREMATOR_SCRIPTED;
Activity ACT_CREMATOR_END_ATTACK;
Activity ACT_CREMATOR_FLINCH_HEAVYDAMAGE;

static const char *CREMATOR_SELFDESTRUCT_THINK = "SelfDestruct";

// Constructor
CNPC_Cremator::CNPC_Cremator()
{
	m_hNPCFileInfo = LookupNPCInfoScript("npc_cremator");
}

// Generic spawn-related
void CNPC_Cremator::Precache()
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	PrecacheModel("models/_Monsters/Combine/cremator_head.mdl");
	PrecacheModel("models/_Monsters/Combine/cremator_body.mdl");
	PropBreakablePrecacheAll(GetModelName());

	PrecacheParticleSystem("flamethrower_orange");

	PrecacheScriptSound("NPC_Cremator.NPCAlert");
	PrecacheScriptSound("NPC_Cremator.PlayerAlert");
	PrecacheScriptSound("NPC_Cremator.FootstepLeft");
	PrecacheScriptSound("NPC_Cremator.FootstepRight");
	PrecacheScriptSound("NPC_Cremator.BreathingNew");
	PrecacheScriptSound("NPC_Cremator.MadBreathe");
	PrecacheScriptSound("NPC_Cremator.Cloth");
	PrecacheScriptSound("NPC_Cremator.DeathSound");
	PrecacheScriptSound("NPC_Cremator.Explode");
	PrecacheScriptSound("Weapon_Immolator.Single");
	PrecacheScriptSound("Weapon_Immolator.Stop");
	PrecacheScriptSound("Weapon_Immolator.Clear");
	PrecacheScriptSound("E3_Phystown.Slicer");
	PrecacheScriptSound("NPC_Manhack.Bat");
	PrecacheScriptSound("DI_Immolator.LaunchFireball");

	// TEMP
	PrecacheParticleSystem("explosion_turret_break");

	BaseClass::Precache();
}

void CNPC_Cremator::Spawn(void)
{
	Precache();
	SetModel(STRING(GetModelName()));

	BaseClass::Spawn();

	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MoveType_t(GetNPCScriptData().NPC_Movement_MoveType_int));
	SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
	SetHullSizeNormal();
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;

	SetBodygroup(1, 0); // the gun
	SetBodygroup(2, 0); // the head
	
	m_bloodColor = BLOOD_COLOR_MECH; // TODO: basically turns blood into sparks. Need something more special.
	
	m_NPCState = NPC_STATE_NONE;
	
	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
	CapabilitiesAdd(bits_CAP_AIM_GUN);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_Climb_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_CLIMB); 
	if ((GetNPCScriptData().NPC_Capable_Doors_boolean) == true) CapabilitiesAdd(bits_CAP_DOORS_GROUP); // TODO: cremator thus can open doors but he is too tall to fit normal doors?

	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));

	m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);
	
	SetDistLook(2000);

	m_crematorBreatheSound_time = CURTIME + 2.0f;
	m_crematorIdleSound_time = CURTIME + 0.0f;

	m_crematorNextEdgePush_time = CURTIME;

	m_crematorIgnorePlayer_boolean = false; // true;
	m_crematorTraceAttackCone_boolean = false;

	m_consequtiveattacks_int = 0;

	NPCInit();
}

// Thinks
void CNPC_Cremator::RunAI(void)
{
	BaseClass::RunAI();
	// HACKHACK: sorta "fixes" cremators continuing to deal damage when they stark walking after firing
	if (GetActivity() != ACT_RANGE_ATTACK2) m_crematorTraceAttackCone_boolean = false;
	SetNextThink(CURTIME + 0.1f);
}

void CNPC_Cremator::SelfDestructThink()
{
	// If we're done, explode
	if ((CURTIME - m_crematorSelfDestructStart_time) >= 4.0f)
	{
		SetThink(&CNPC_Cremator::ExplodeThink);
		SetNextThink(CURTIME + 0.1f);
		UTIL_Remove(m_crematorOnFireEffect_handle);
		m_crematorOnFireEffect_handle = NULL;
		return;
	}

	Vector vUp, vF, vR;
	GetVectors(&vF, &vR, &vUp);

	UTIL_BloodSpray(WorldSpaceCenter(), (vUp * 8.0f + vF * RandomFloat(-4, 4) + vR * RandomFloat(-4, 4)), BLOOD_COLOR_BLACK, RandomInt(4, 8), FX_BLOODSPRAY_DROPS);

	SetNextThink(CURTIME + 0.1f, CREMATOR_SELFDESTRUCT_THINK);
}

void CNPC_Cremator::ExplodeThink(void)
{
	Vector vecUp;
	GetVectors(NULL, NULL, &vecUp);
	Vector vecOrigin = WorldSpaceCenter() + (vecUp * 12.0f);
	float radius = 75;

	// Our effect
	DispatchParticleEffect("explosion_turret_break", vecOrigin, GetAbsAngles());

	// K-boom
	RadiusDamage(CTakeDamageInfo(this, this, radius, DMG_BLAST | DMG_BURN), vecOrigin, 256.0f, CLASS_NONE, this);

	CFire *pFires[64];
	int fireCount = FireSystem_GetFiresInSphere(pFires, ARRAYSIZE(pFires), false, GetAbsOrigin(), radius);

	for (int i = 0; i < fireCount; i++)
	{
		pFires[i]->AddHeat(100);
	}

	EmitSound("NPC_Cremator.Explode");

	breakablepropparams_t params(GetAbsOrigin(), GetAbsAngles(), vec3_origin, RandomAngularImpulse(-800.0f, 800.0f));
	params.impactEnergyScale = 1.0f;
	params.defCollisionGroup = COLLISION_GROUP_INTERACTIVE;

	// no damage/damage force? set a burst of 100 for some movement
	params.defBurstScale = 100;
	PropBreakableCreateAll(GetModelIndex(), VPhysicsGetObject(), params, this, -1, true);

	// Create physical, interactive head
	Vector forceVector;
	forceVector = (GetAbsOrigin() - EyePosition());
	CPhysicsProp *head = assert_cast<CPhysicsProp*>(CreateEntityByName("prop_physics"));
	head->SetModel("models/_Monsters/Combine/cremator_head.mdl");
	head->SetAbsOrigin(EyePosition());
	head->SetAbsAngles(GetLocalAngles());
	head->SetMoveType(MOVETYPE_VPHYSICS);
	head->SetCollisionGroup(COLLISION_GROUP_INTERACTIVE);
	//	head->SetOwnerEntity(GetWorldEntity());
	head->Spawn();
	head->VelocityPunch(forceVector * (2500.0f + RandomFloat(-100.0f, 100.0f)));

	// Throw out some small chunks too obscure the explosion even more
	CPVSFilter filter(vecOrigin);
	for (int i = 0; i < 4; i++)
	{
		Vector gibVelocity = RandomVector(-100, 100);
		int iModelIndex = modelinfo->GetModelIndex(g_PropDataSystem.GetRandomChunkModel("MetalChunks"));
		te->BreakModel(filter, 0.0, vecOrigin, GetAbsAngles(), Vector(40, 40, 40), gibVelocity, iModelIndex, 150, 4, 2.5, BREAK_METAL);
	}

	// Scorch the ground beneath us
	trace_t tr;
	Vector	vecStart;
	Vector	vecTraceDir;

	GetVectors(NULL, NULL, &vecTraceDir);
	vecTraceDir.Negate();

	for (int i = 0; i < 6; i++)
	{
		vecStart.x = GetAbsOrigin().x + random->RandomFloat(-16.0f, 16.0f);
		vecStart.y = GetAbsOrigin().y + random->RandomFloat(-16.0f, 16.0f);
		vecStart.z = GetAbsOrigin().z + 4;

		UTIL_TraceLine(vecStart, vecStart + (vecTraceDir * (5 * 12)), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);

		if (tr.fraction != 1.0)
		{
			UTIL_DecalTrace(&tr, "Scorch");
		}
	}

	// We're done!
	UTIL_Remove(this);
}

// Damage handling
bool CNPC_Cremator::HeadDamageIsHeavy(const CTakeDamageInfo &info)
{
	if (info.GetDamageType() & DMG_CRUSH)
		return true;

//	if (info.GetDamageType() & DMG_BUCKSHOT)
//		return true; // Don't want this. Shotguns fire in pellets and *every* one that hits target, will count. Overkill.

	if (info.GetDamageType() & DMG_BULLET && info.GetDamage() > 20)
		return true; // not sure how, but ok

	if (info.GetAmmoType() == GetAmmoDef()->Index("357"))
		return true;

	if (info.GetAmmoType() == GetAmmoDef()->Index("SniperRound"))
		return true;

	if (info.GetAmmoType() == GetAmmoDef()->Index("XBowBolt"))
		return true;

	return false;
}

bool CNPC_Cremator::HeadDamageIsSlice(const CTakeDamageInfo &info)
{
	// If you take crush and slash damage, you're hit by a sharp physics item.
	if ((info.GetDamageType() & DMG_SLASH) && (info.GetDamageType() & DMG_CRUSH))
		return true;

	return false;
}

void CNPC_Cremator::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr)
{
	CTakeDamageInfo infoCopy = info;

	// Keep track of headshots so we can determine whether to pop off our headcrab.
	if (ptr->hitgroup == HITGROUP_HEAD )
	{
		g_pEffects->Ricochet(ptr->endpos, (vecDir*-1.0f));
		infoCopy.SetDamage(info.GetDamage() * 1.5f);

		if (HeadDamageIsHeavy(info))
		{
			EmitSound("NPC_Manhack.Bat");
			SetIdealActivity(ACT_BIG_FLINCH);
		}
	}

	else if (ptr->hitgroup == HITGROUP_LEFTARM || ptr->hitgroup == HITGROUP_RIGHTARM)
	{
		UTIL_BloodSpray(ptr->endpos + (vecDir*-4.0f), (vecDir*-1.0f), BLOOD_COLOR_RED, 2, FX_BLOODSPRAY_ALL);
		UTIL_BloodSpray(ptr->endpos + (vecDir*-4.0f), (vecDir*-1.0f), BLOOD_COLOR_RED, 3, FX_BLOODSPRAY_DROPS);
	//	infoCopy.SetDamage(info.GetDamage() * 0.5f);
	}

	else if (ptr->hitgroup == HITGROUP_STOMACH)
	{
		UTIL_BloodSpray(ptr->endpos + (vecDir*-4.0f), (vecDir*-1.0f), BLOOD_COLOR_GREEN, 4, FX_BLOODSPRAY_ALL);
		UTIL_BloodSpray(ptr->endpos + (vecDir*-4.0f), (vecDir*-1.0f), BLOOD_COLOR_GREEN, 6, FX_BLOODSPRAY_DROPS);
		infoCopy.ScaleDamage(1.2f);
		damageIsBallshot = true;
	}
	
	BaseClass::TraceAttack(infoCopy, vecDir, ptr);
}

int CNPC_Cremator::OnTakeDamage_Alive(const CTakeDamageInfo &inputInfo)
{
	CTakeDamageInfo info = inputInfo;

	if (info.GetInflictor() == this)
	{
		return 0; // take no damage from your own fireballs. 
	}

	if ((info.GetInflictor()->ClassMatches(GetClassname())))
	{
		return 0; // take no damage from other cremators' fireballs.
	}

	if (info.GetAttacker()->IsPlayer() == 0)
	{
		info.ScaleDamage(0.5f); // take half damage from other sources, like enemy NPCs, or friendly fire.
	}
	else
	{
		m_OnDamagedByPlayer.FireOutput(info.GetAttacker(), this);
	}

	m_OnDamaged.FireOutput(info.GetAttacker(), this);

	if (HeadDamageIsSlice(info))
	{
		EmitSound("E3_Phystown.Slicer");
		HandleSpecialDeath(true, false, info);
		m_OnDeath.FireOutput(this, this);
		return 0;
	}

	if(info.GetDamageType() & DMG_VEHICLE /*&& info.GetDamage() > (m_iHealth * 0.25f)*/) // allow vehicles to kill us instantly if they dealt at least 25% of our health damage
	{
		HandleSpecialDeath(true, false, info);
		m_OnDeath.FireOutput(this, this);
		return 0;
	}
		
	m_iHealth -= info.GetDamage();

	if (m_iHealth <= 1 && m_takedamage != DAMAGE_NO)
	{
		m_iHealth = 1;
		m_takedamage = DAMAGE_NO;

		inputdata_t inputdata;
		InputSelfDestruct(inputdata);
		m_OnDeath.FireOutput(this, this, 4.0f); // four seconds is how long it takes to burh and explode
		return 0;
	}
	
	damageIsBallshot = false;
	return 1; // BaseClass::OnTakeDamage_Alive(info);
}

void CNPC_Cremator::Event_Killed(const CTakeDamageInfo &info)
{
	StopAttackFX();
	BaseClass::Event_Killed(info);
}

void CNPC_Cremator::HandleSpecialDeath(bool headsliced, bool shouldexplode, const CTakeDamageInfo &info)
{
	if (IsEffectActive(EF_NODRAW)) return;
	Vector forceVector(vec3_origin);
	forceVector += CalcDamageForceVector(info);

	StopAttackFX();

	if (headsliced)
	{
		CPhysicsProp *head = assert_cast<CPhysicsProp*>(CreateEntityByName("prop_physics"));
		head->SetModel("models/_Monsters/Combine/cremator_head.mdl");
		head->SetAbsOrigin(EyePosition());
		head->SetAbsAngles(GetLocalAngles());
		head->SetMoveType(MOVETYPE_VPHYSICS);
		head->SetCollisionGroup(COLLISION_GROUP_INTERACTIVE);
		head->SetOwnerEntity(GetWorldEntity());
		head->Spawn();
		head->VelocityPunch(forceVector * (200 + RandomFloat(-10, 10)));

		float flFadeTime = 0.0;

		Vector vecLegsForce;
		vecLegsForce.x = random->RandomFloat(-400, 400);
		vecLegsForce.y = random->RandomFloat(-400, 600);
		vecLegsForce.z = random->RandomFloat(0, 250);

		CBaseEntity *headless = CreateRagGib("models/_Monsters/Combine/cremator_body.mdl", GetAbsOrigin() + Vector(0,0,2), GetAbsAngles(), vecLegsForce, flFadeTime, false);
		if (headless)
		{
			CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating*>(headless);
			if (pAnimating)
			{
				pAnimating->SetBodygroup(2, 1);
			}

			headless->SetCollisionGroup(COLLISION_GROUP_INTERACTIVE);
			headless->SetOwnerEntity(GetWorldEntity());

			forceVector *= random->RandomFloat(0.06, 0.08);
			forceVector.z = (100 * 12 * 5) * random->RandomFloat(0.8, 1.2);

			headless->VelocityPunch(forceVector);
		}
		
		SetSolid(SOLID_NONE);
		AddEffects(EF_NODRAW);
		SetThink(&CNPC_Cremator::SUB_Remove);
		SetNextThink(CURTIME + 0.1f);
	}

	if (shouldexplode)
	{
		Ignite(5.0f, false, 8.0f, false);
		SetThink(&CNPC_Cremator::SUB_Remove);
		SetNextThink(CURTIME + 5.0f);
	}
}

// NPC sounds
void CNPC_Cremator::BreatheSound(void)
{
	EmitSound("NPC_Cremator.BreathingNew");
	m_crematorBreatheSound_time = CURTIME + 15.0f;
}

void CNPC_Cremator::IdleSound(void)
{
	EmitSound("NPC_Cremator.Cloth");
	m_crematorIdleSound_time = CURTIME + RandomFloat(3.0f, 10.0f);
}

void CNPC_Cremator::AlertSound(void)
{
	if (GetEnemy() && GetEnemy()->IsPlayer())
		EmitSound("NPC_Cremator.PlayerALert");
	else
		EmitSound("NPC_Cremator.NPCALert");

}

// Animations
void CNPC_Cremator::HandleAnimEvent(animevent_t *pEvent)
{
	if (pEvent->event == AE_CREMATOR_FOOTSTEPLEFT)
	{
		EmitSound("NPC_Cremator.FootstepLeft");
		return;
	}

	if (pEvent->event == AE_CREMATOR_FOOTSTEPRIGHT)
	{
		EmitSound("NPC_Cremator.FootstepRight");
		return;
	}

	if (pEvent->event == AE_CREMATOR_IMMO_PARTICLESTART)
	{
		m_crematorTraceAttackCone_boolean = true;
		DispatchParticleEffect("flamethrower_orange", PATTACH_POINT_FOLLOW, this, "muzzle");
		EmitSound("Weapon_Immolator.Single");
		AddEffects(EF_CREMATORLIGHT);
		return;
	}

	if (pEvent->event == AE_CREMATOR_IMMO_PARTICLESTOP)
	{
		m_crematorTraceAttackCone_boolean = false;
		DispatchParticleEffect("null", PATTACH_ABSORIGIN, this, -1, true);
		StopSound("Weapon_Immolator.Single");
		EmitSound("Weapon_Immolator.Stop");
		RemoveEffects(EF_CREMATORLIGHT);
		return;
	}

	if (pEvent->event == AE_CREMATOR_IMMO_LAUNCHFIREBALL)
	{
		StopAttackFX();

		Vector vecAiming;
		GetVectors(&vecAiming, NULL, NULL);

		Vector vecSrc, vForward, vRight;
		GetAttachment("muzzle", vecSrc, &vForward, &vRight);

		DispatchParticleEffect("fireball_muzzle", PATTACH_POINT_FOLLOW,
			this, "muzzle", true);

		QAngle angAiming;
		VectorAngles(vecAiming, angAiming);

		vecAiming *= 1000;

		vecSrc.z += 8.0f; // raise grenade spawn origin, but only AFTER dispatching muzzle flash particle effect

		CSharedProjectile *pGrenade = (CSharedProjectile*)CreateEntityByName("shared_projectile");
		pGrenade->SetAbsOrigin(vecSrc);
		pGrenade->SetAbsAngles(vec3_angle);
		DispatchSpawn(pGrenade);
		pGrenade->SetThrower(this);
		pGrenade->SetOwnerEntity(this);

		float flGravity = 750;
		Vector vTarget;
		CBaseEntity* pBlocker;
		if (GetEnemy() != NULL)
		{
			vTarget = GetEnemy()->GetAbsOrigin();
			ThrowLimit(vecSrc, vTarget, flGravity, 3, Vector(0, 0, 32), Vector(0, 0, 0), GetEnemy(), &vecAiming, &pBlocker);

			if (flGravity > 750.0f) flGravity = 750.0f;
			if (flGravity < 100.0f) flGravity = 120.0f;
		}
		else flGravity = 120;

		pGrenade->SetProjectileStats(Projectile_SMALL, Projectile_INFERNO, Projectile_CONTACT, sk_immolator_dmg.GetFloat(), 160.0f, flGravity, 64.0f, 0.01f);
		pGrenade->SetModelScale(RandomFloat(0.95, 1.05)); // we use Projectile_SMALL to make its bbox small, but we want larger visual appearance
		pGrenade->SetCollisionBounds(Vector(-2, -2, -2), Vector(2, 2, 2));
		pGrenade->SetAbsVelocity(vecAiming);

		pGrenade->CreateProjectileEffects();

		// Tumble through the air
		pGrenade->SetLocalAngularVelocity(
			QAngle(random->RandomFloat(-250, -500),
				random->RandomFloat(-250, -500),
				random->RandomFloat(-250, -500)));

		EmitSound("DI_Immolator.LaunchFireball");

		m_consequtiveattacks_int++;
		return;
	}

	if (pEvent->event == AE_CREMATOR_ATTACK_START ||
		pEvent->event == AE_CREMATOR_ATTACK_END ||
		pEvent->event == AE_CREMATOR_MOVEMENT_START ||
		pEvent->event == AE_CREMATOR_FLINCH ||
		pEvent->event == AE_CREMATOR_WEAPON_JAMM ||
		pEvent->event == AE_CREMATOR_IDLE_START ||
		pEvent->event == AE_CREMATOR_IDLE_STOP)
	{
		StopAttackFX();
		return;
	}

	BaseClass::HandleAnimEvent(pEvent);
	return;
}

Activity CNPC_Cremator::NPC_TranslateActivity(Activity activity)
{
	if (m_crematorOnSelfDestruct_boolean)
	{
		if (activity == ACT_RUN_AIM || activity == ACT_RUN || activity == ACT_WALK || activity == ACT_WALK_AIM)
		{
			return ACT_RUN_ON_FIRE;
		}
	}
	return BaseClass::NPC_TranslateActivity(activity);
}

void CNPC_Cremator::StopAttackFX(void)
{
	DispatchParticleEffect("null", PATTACH_ABSORIGIN, this, -1, true);
	StopSound("Weapon_Immolator.Single");
	EmitSound("Weapon_Immolator.Clear");
	RemoveEffects(EF_CREMATORLIGHT);
}

// Conditions
bool CNPC_Cremator::IsValidEnemy(CBaseEntity *pEnemy)
{
	Class_T enemyClass = pEnemy->Classify();

	if (m_crematorOnSelfDestruct_boolean)
	{
		return false; // stop caring at all if we're going to blow up
	}
	
	if (enemyClass == CLASS_PLAYER || enemyClass == CLASS_PLAYER_ALLY || enemyClass == CLASS_PLAYER_ALLY_VITAL)
	{
		return (ShouldIgnorePlayer() == false);
	}
	if (!FVisible(pEnemy) && GetLastEnemyTime() > CURTIME + 5.0f)
	{
		// Don't take an enemy you can't see. Since stalkers move way too slowly to
		// establish line of fire, usually an enemy acquired by means other than
		// the Stalker's own eyesight will always get away while the stalker plods
		// slowly to their last known position. So don't take enemies you can't see.
		return false;
	}
	if (pEnemy->GetFlags() & FL_ONFIRE)
	{
		return false; // stop paying attention to enemies already set ablaze
	}
	return BaseClass::IsValidEnemy(pEnemy);
}


int CNPC_Cremator::RangeAttack1Conditions(float flDot, float flDist)
{
	//	if (m_consequtiveattacks_int >= 4)
	//	{
	//		return COND_NO_PRIMARY_AMMO;
	//	}

	if (flDot < 0.7)
	{
		return COND_NOT_FACING_ATTACK;
	}

	float dist = HasSpawnFlags(SF_NPC_LONG_RANGE) ? crematorAttackRange_float * 2 : crematorAttackRange_float;

	if (flDist > dist) // create a buffer between us and the target
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	if (flDist < crematorAttackRange_float * 0.3) // create a buffer between us and the target
	{
		return COND_NONE;
	}

	return COND_CAN_RANGE_ATTACK1;
}

int CNPC_Cremator::RangeAttack2Conditions(float flDot, float flDist)
{
	//	if (m_consequtiveattacks_int >= 4)
	//	{
	//		return COND_NO_PRIMARY_AMMO;
	//	}

	if (flDot < 0.7)
	{
		return COND_NOT_FACING_ATTACK;
	}

	if (flDist > crematorAttackRange_float * 0.4) // create a buffer between us and the target
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	return COND_CAN_RANGE_ATTACK2;
}

// Schedules, states, tasks
void CNPC_Cremator::BuildScheduleTestBits(void)
{
	// Ignore damage if we're already attacking.
	if (GetActivity() == ACT_RANGE_ATTACK1)
	{
		ClearCustomInterruptCondition(COND_LIGHT_DAMAGE);
		ClearCustomInterruptCondition(COND_NEW_ENEMY);
		ClearCustomInterruptCondition(COND_ENEMY_OCCLUDED);
	}

	if (GetActivity() == ACT_RANGE_ATTACK2)
	{
		ClearCustomInterruptCondition(COND_LIGHT_DAMAGE);
		ClearCustomInterruptCondition(COND_NEW_ENEMY);
	}

	if (m_NPCState == NPC_STATE_COMBAT)
	{
		SetCustomInterruptCondition(COND_TOO_FAR_TO_ATTACK);
	}

	BaseClass::BuildScheduleTestBits();
}

void CNPC_Cremator::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();

//	if (IsCurSchedule(SCHED_RUN_RANDOM))
//	{
//		m_consequtiveattacks_int = 0;
		ClearCondition(COND_NO_PRIMARY_AMMO);
//	}
	
	if (m_crematorIdleSound_time < CURTIME)
		IdleSound();
	if (m_crematorBreatheSound_time < CURTIME)
		BreatheSound();
	
//	if (ShouldTraceAttackCone())
		crematorAttackRange_float = 1000;
//	else
//		crematorAttackRange_float = 2000;

	// Attack tracing
	if (ShouldTraceAttackCone() && (m_NPCState == NPC_STATE_COMBAT) && !m_crematorOnSelfDestruct_boolean)
	{
		// Trace a cone (sort of) in front of the gun. Annihilate things within it.
		Vector vGun, vForward, vRight;
		GetAttachment("muzzle", vGun, &vForward, &vRight);
		Vector vAttackDir; //

		CBaseEntity *pEntity = NULL;
		float flAdjustedDamage, flDist;
		flAdjustedDamage = 1.0f;

		// iterate on all entities in the vicinity.
		while ((pEntity = gEntList.FindEntityInSphere(pEntity, vGun, crematorAttackRange_float)) != NULL)
		{
			vAttackDir = GetLocalOrigin() - (pEntity->WorldSpaceCenter());
			VectorNormalize(vAttackDir);
			flDist = (pEntity->WorldSpaceCenter() - vGun).Length2D();
			//	flDot = DotProduct(pEntity->WorldSpaceCenter(), vAttackDir);
			
			if ( flDist <= crematorAttackRange_float * 0.4f
				&& FInAimCone(pEntity->WorldSpaceCenter())
				&& pEntity != this)
			{
				// test if the entity is a fire and add heat to it
				CFire *pFire = dynamic_cast<CFire*>(pEntity);
				if (pFire)
				{
					pFire->AddHeat(0.5, false);
					continue;
				}

				if (pEntity->m_takedamage != DAMAGE_YES)
					continue;

				if (pEntity->IsPlayer())
				{
					// for the sake of player, check again to see if clear line
					// can be traced between gun barrel and the player.
					// without it they won't be able to shield themselves from fire
					// unless they move far enough to force the cremator to cease attack
					// and reposition. 
					if (!IsLineOfSightClear(vGun, IGNORE_ACTORS, this))
					{
					//	NDebugOverlay::Line(vGun, pEntity->WorldSpaceCenter(), 255, 50, 50, true, 0.1f);
						return;
					}
				}

				ClearMultiDamage();
				CTakeDamageInfo cInfo;
				cInfo.SetDamageType(DMG_BURN | DMG_DISSOLVE_BURN);
				cInfo.SetDamage(flAdjustedDamage);
				cInfo.ScaleDamageForce(0.0f);
				cInfo.SetAttacker(this);
				cInfo.SetInflictor(this);
				pEntity->TakeDamage(cInfo);

				if (pEntity->IsNPC())
				{
					if (pEntity->MyNPCPointer()->IsOnFire())
					{
						pEntity->MyNPCPointer()->AddEntityRelationship(this, D_FR, 99);
						pEntity->MyNPCPointer()->UpdateEnemyMemory(this, GetAbsOrigin());
					}
				}
			}
		}
	}

	if (GetEnemy() != NULL)
	{
		if (m_NPCState == NPC_STATE_COMBAT
			&& GetActivity() == ACT_RANGE_ATTACK2
			&& EnemyDistance(GetEnemy()) > crematorAttackRange_float * 0.4f)
		{
			SetIdealActivity(ACT_CREMATOR_END_ATTACK);
		}
	}

	// Experimental feature! Cremators jumping off ledges.
	if (!CheckEdgeOverstep())
	{
		if (CURTIME >= m_crematorNextEdgePush_time)
		{
			Vector vF;
			GetVectors(&vF, NULL, NULL);
			ApplyLocalVelocityImpulse(vF * 200.0f);
			m_crematorNextEdgePush_time = CURTIME + 2.0f;
		}
	}

}

int CNPC_Cremator::SelectSchedule(void)
{
	if (m_crematorOnSelfDestruct_boolean)
	{
		return SCHED_RUN_RANDOM;
	}

	if (m_NPCState == NPC_STATE_ALERT)
	{
		if (HasCondition(COND_HEAR_COMBAT))
		{
			CSound *pSound = GetBestSound();

			if (pSound && pSound->IsSoundType(SOUND_COMBAT))
			{
				return SCHED_INVESTIGATE_SOUND;
			}
		}
	}

	if (m_NPCState == NPC_STATE_COMBAT)
	{
		if (HasCondition(COND_NO_PRIMARY_AMMO))
		{
			return SCHED_RUN_RANDOM;
		}
		if (HasCondition(COND_TOO_FAR_TO_ATTACK) && !HasCondition(COND_CAN_RANGE_ATTACK1))
		{
			return SCHED_CHASE_ENEMY;
		}

		if (HasCondition(COND_ENEMY_OCCLUDED) || HasCondition(COND_WEAPON_SIGHT_OCCLUDED))
		{
			return SCHED_CHASE_ENEMY;
		}

		if (HasCondition(COND_CAN_RANGE_ATTACK2))
		{
			return SCHED_RANGE_ATTACK2;
		}

		if (HasCondition(COND_CAN_RANGE_ATTACK1))
		{
			return SCHED_RANGE_ATTACK1;
		}
	}

	return BaseClass::SelectSchedule();
}

// Inputs
void CNPC_Cremator::InputIgnorePlayer(inputdata_t &inputdata)
{
	m_crematorIgnorePlayer_boolean = inputdata.value.Bool();
}

void CNPC_Cremator::InputSelfDestruct(inputdata_t &inputdata)
{
	// Ka-boom!
	m_crematorSelfDestructStart_time = CURTIME;
	m_crematorOnSelfDestruct_boolean = true;

	m_NPCState = NPC_STATE_ALERT;

	StopAttackFX();

	SetContextThink(&CNPC_Cremator::SelfDestructThink, CURTIME + 0.1f, CREMATOR_SELFDESTRUCT_THINK);

	// Create the dust effect in place
	m_crematorOnFireEffect_handle = (CParticleSystem *)CreateEntityByName("info_particle_system");
	if (m_crematorOnFireEffect_handle != NULL)
	{
		Vector vecUp;
		GetVectors(NULL, NULL, &vecUp);

		// Setup our basic parameters
		m_crematorOnFireEffect_handle->KeyValue("start_active", "1");
		m_crematorOnFireEffect_handle->KeyValue("effect_name", "explosion_turret_fizzle");
		m_crematorOnFireEffect_handle->SetParent(this);
		m_crematorOnFireEffect_handle->SetAbsOrigin(WorldSpaceCenter() + (vecUp * 12.0f));
		DispatchSpawn(m_crematorOnFireEffect_handle);
		m_crematorOnFireEffect_handle->Activate();
	}
}

// Movement
bool CNPC_Cremator::OverrideMoveFacing(const AILocalMoveGoal_t &move, float flInterval)
{
	if (GetEnemy() && GetNavigator()->GetMovementActivity() == ACT_RUN && IsCurSchedule(SCHED_CHASE_ENEMY ))
	{
		// FIXME: this will break scripted sequences that walk when they have an enemy
		Vector vecEnemyLKP = GetEnemyLKP();
		if (UTIL_DistApprox(vecEnemyLKP, GetAbsOrigin()) < 1024)
		{
			// Only start facing when we're close enough
			AddFacingTarget(GetEnemy(), vecEnemyLKP, 1.0, 0.2);
		}
	}

	// If we're in our aiming gesture, then always face our target as we run
	Activity nActivity = NPC_TranslateActivity(ACT_RANGE_ATTACK1);
	if (IsPlayingGesture(nActivity) )
	{
		Vector vecEnemyLKP = GetEnemyLKP();
		AddFacingTarget(GetEnemy(), vecEnemyLKP, 1.0, 0.2);
	}

	return BaseClass::OverrideMoveFacing(move, flInterval);
}

// Experimental feature - if a cremator moved too close to the edge of solid ground,
// push him forward as if he's jumping down.
bool CNPC_Cremator::CheckEdgeOverstep(void)
{
	if (!(GetFlags()&FL_ONGROUND))
		return false;

	Vector vF;
	GetVectors(&vF, NULL, NULL);

	Vector front = GetAbsOrigin() + vF * 20.0f;
//	NDebugOverlay::Cross3D(front, 8, 255, 0, 0, true, 0.1f);

	trace_t tr;
	UTIL_TraceLine(front, front - Vector(0, 0, 16.0f), MASK_NPCSOLID, this, GetCollisionGroup(), &tr);
	return tr.fraction != 1.0;
}

// Datadesc
BEGIN_DATADESC(CNPC_Cremator)
	DEFINE_FIELD(m_crematorBreatheSound_time, FIELD_TIME),
	DEFINE_FIELD(m_crematorIdleSound_time, FIELD_TIME),
	DEFINE_FIELD(m_crematorIgnorePlayer_boolean, FIELD_BOOLEAN),
	DEFINE_FIELD(m_crematorTraceAttackCone_boolean, FIELD_BOOLEAN),
	DEFINE_FIELD(m_crematorOnSelfDestruct_boolean, FIELD_BOOLEAN),
	DEFINE_FIELD(m_crematorSelfDestructStart_time, FIELD_TIME),
	DEFINE_FIELD(m_crematorOnFireEffect_handle, FIELD_EHANDLE),
	DEFINE_FIELD(m_crematorNextEdgePush_time, FIELD_TIME),
	DEFINE_INPUTFUNC(FIELD_BOOLEAN, "SetIgnorePlayer", InputIgnorePlayer),
	DEFINE_INPUTFUNC(FIELD_VOID, "SelfDestruct", InputSelfDestruct),
	DEFINE_THINKFUNC(SelfDestructThink),
	DEFINE_THINKFUNC(ExplodeThink),
END_DATADESC();

LINK_ENTITY_TO_CLASS(npc_cremator, CNPC_Cremator);

AI_BEGIN_CUSTOM_NPC(npc_cremator, CNPC_Cremator)

DECLARE_USES_SCHEDULE_PROVIDER(CAI_BlendedNPC)
DECLARE_ACTIVITY(ACT_CREMATOR_SCRIPTED)
DECLARE_ACTIVITY(ACT_CREMATOR_END_ATTACK)
DECLARE_ACTIVITY(ACT_CREMATOR_FLINCH_HEAVYDAMAGE)
DECLARE_ANIMEVENT(AE_CREMATOR_FOOTSTEPLEFT)
DECLARE_ANIMEVENT(AE_CREMATOR_FOOTSTEPRIGHT)
DECLARE_ANIMEVENT(AE_CREMATOR_IMMO_PARTICLESTART)
DECLARE_ANIMEVENT(AE_CREMATOR_IMMO_PARTICLESTOP)
DECLARE_ANIMEVENT(AE_CREMATOR_IMMO_LAUNCHFIREBALL)
DECLARE_ANIMEVENT(AE_CREMATOR_ATTACK_START)
DECLARE_ANIMEVENT(AE_CREMATOR_ATTACK_END)
DECLARE_ANIMEVENT(AE_CREMATOR_IDLE_START)
DECLARE_ANIMEVENT(AE_CREMATOR_IDLE_STOP)
DECLARE_ANIMEVENT(AE_CREMATOR_MOVEMENT_START)
DECLARE_ANIMEVENT(AE_CREMATOR_WEAPON_JAMM)
DECLARE_ANIMEVENT(AE_CREMATOR_FLINCH)
AI_END_CUSTOM_NPC()
#endif // CREMATOR

//====================================
// Bullsquid (Dark Interval)
//====================================
#if defined (BULLSQUID)

#define SQUID_USES_BONE_FOLLOWERS 0
typedef CAI_BlendingHost< CAI_BehaviorHost<CAI_BlendedNPC> > CAI_BaseSquidBase;
class CNPC_Squid : public CAI_BaseSquidBase
{
	DECLARE_CLASS(CNPC_Squid, CAI_BaseSquidBase);
	DECLARE_DATADESC();
public:
	CNPC_Squid(void);

	Class_T Classify(void) { return CLASS_BULLSQUID; }
	void Precache(void);
	void Spawn(void);
	void OnRestore();

	void NPCThink(void);

	void TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr);
	void Event_Killed(const CTakeDamageInfo &info);
	
	void IdleSound(void) { SquidIdleSound(); }
	void SquidIdleSound(void);
	void SquidAttackSound(void);
	void PainSound(const CTakeDamageInfo &inf)
	{
		SquidPainSound();

		if (!GetEnemy())
		{
			SetSchedule(SCHED_ALERT_FACE);
			if (inf.GetAttacker() != NULL)
				SetEnemy(inf.GetAttacker()); // dubious... but squids are so dumb otherwise! // todo: move into gatherconditions or something?
		}
	}
	void SquidPainSound(void);
	void SquidDeathSound(void);

	
	void HandleAnimEvent(animevent_t *pEvent);

//private: // todo: why are these made private?
	bool IAmWithinEnemyFOV(CBaseEntity *pEnemy);
	bool SeenEnemyWithinTime(float flTime);
	bool IsValidEnemy(CBaseEntity *pEnemy);
	int MeleeAttack1Conditions(float flDot, float flDist);
	int RangeAttack1Conditions(float flDot, float flDist);
//public:
	void GatherConditions(void);
	void PrescheduleThink(void);
	int TranslateSchedule(int scheduleType);
	int SelectSchedule(void);
	void StartTask(const Task_t *pTask);
	void RunTask(const Task_t *pTask);

	void FootstepEffect(const Vector &origin, bool bHeavy);
	void Spit(CBaseEntity *pEnemy);
	void MeleeCheckContact(bool bWhip);

	float MaxYawSpeed(void) { return 90; }
	bool OverrideMoveFacing(const AILocalMoveGoal_t &move, float flInterval);

	int GetSoundInterests(void)
	{
		return	SOUND_WORLD
			| SOUND_COMBAT
			| SOUND_PLAYER
			| SOUND_DANGER
			| SOUND_PHYSICS_DANGER
			| SOUND_BULLET_IMPACT
			| SOUND_MOVE_AWAY;
	};
	
	// Physics Bone Shadows test
#if SQUID_USES_BONE_FOLLOWERS
	int UpdateTransmitState();
	bool CreateVPhysics(void);
	void InitBoneFollowers(void);
	CBoneFollowerManager m_BoneFollowerManager;
	CBoneFollower *GetBoneFollowerByIndex(int nIndex);
	int GetBoneFollowerIndex(CBoneFollower *pFollower);
	void UpdateOnRemove(void);
#endif
private:
	Vector m_vecSaveSpitVelocity;
	float m_flNextSquidIdleTime;
	float m_flNextSquidAttackTime;
	float m_flNextSquidPainTime;
	float m_flNextSquidSnortTime;
	float m_flLastSquidChaseTime;
	float m_flSpottedNewEnemyTime;
	bool m_squid_hopped_bool;
public:
	DEFINE_CUSTOM_AI;
	enum
	{
		SCHED_SQUID_IDLE_WANDER = LAST_SHARED_SCHEDULE,
		SCHED_SQUID_RANGE_ATTACK1_FAR,
		SCHED_SQUID_RANGE_ATTACK1_CLOSE,
		SCHED_SQUID_RANGE_ATTACK1_DEFAULT,
		SCHED_SQUID_CHARGE_ENEMY,
		SCHED_SQUID_RUN_RANDOM
	};

	enum
	{
		TASK_SQUID_ANNOUNCE_RANGE_ATTACK = LAST_SHARED_TASK,
		TASK_SQUID_REPOSITION_AFTER_RANGE_ATTACK,
		TASK_SQUID_CUT_DISTANCE_TO_ENEMY_FOR_MELEE
	};

	enum
	{
		COND_SQUID_TARGET_WITHIN_FAR_SPIT_RANGE = LAST_SHARED_CONDITION,
		COND_SQUID_TARGET_WITHIN_CLOSE_SPIT_RANGE,
		COND_SQUID_TARGET_WITHIN_MELEE_RANGE,
		COND_SQUID_TARGET_WITHIN_RANGE_BUT_UNREACHABLE,
		COND_SQUID_CHASE_ENEMY_TOO_LONG,
		COND_SQUID_LOW_CONFIDENCE
	};
};

int AE_SQUID_TAILWHIP;
int AE_SQUID_TAILHIT;
int AE_SQUID_SPIT;
int AE_SQUID_BITE;
int AE_SQUID_FOOTSTEP_L;
int AE_SQUID_FOOTSTEP_L_RUN;
int AE_SQUID_FOOTSTEP_R;
int AE_SQUID_FOOTSTEP_R_RUN;

ConVar npc_squid_debug("npc_squid_debug", "0");
ConVar npc_squid_debug_attack("npc_squid_debug_attack", "0");
ConVar sk_squid_spit_delay("sk_squid_spit_delay", "2");

#define SQUID_CLOSE_SPIT_RANGE	480
#define SQUID_FAR_SPIT_RANGE	1600
#define SQUID_MELEE_RANGE		72

// Constructor
CNPC_Squid::CNPC_Squid()
{
	m_hNPCFileInfo = LookupNPCInfoScript("npc_squid");
}

#if SQUID_USES_BONE_FOLLOWERS
static const char *pSquidBoneNames[] =
{
	"Bullsquid.Tail_Bone",
	"Bullsquid.Tail_Bone1",
	"Bullsquid.Tail_Bone2",
	"Bullsquid.Tail_Bone3",
	"Bullsquid.Tail_Bone4",
	"Bullsquid.Head_Bone",
	"Bullsquid.Head_Bone1",
	"Bullsquid.Head_Bone",
	"Bullsquid.Body_bone",
	"Bullsquid.Leg_L",
	"Bullsquid.Leg_L1",
	"Bullsquid.Leg_R",
	"Bullsquid.Leg_R1",
	"Bullsquid.Hand_BoneL",
	"Bullsquid.Hand_BoneR",
	"Bullsquid.Finger_BoneL",
	"Bullsquid.Finger_BoneR",
};
int CNPC_Squid::UpdateTransmitState()
{
	return SetTransmitState(FL_EDICT_ALWAYS);
}

bool CNPC_Squid::CreateVPhysics(void)
{
	// The strider has bone followers for every solid part of its body,
	// so there's no reason for the bounding box to be solid.
	//BaseClass::CreateVPhysics();
	InitBoneFollowers();
	return true;
}
void CNPC_Squid::InitBoneFollowers(void)
{
	// Don't do this if we're already loaded
	if (m_BoneFollowerManager.GetNumBoneFollowers() != 0)
		return;

	// Init our followers
	m_BoneFollowerManager.InitBoneFollowers(this, ARRAYSIZE(pSquidBoneNames), pSquidBoneNames);
}

void CNPC_Squid::UpdateOnRemove(void)
{
	//	m_BoneFollowerManager.DestroyBoneFollowers();
	BaseClass::UpdateOnRemove();
}
#endif

// Generic spawn-related
void CNPC_Squid::Precache(void)
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	PrecacheScriptSound("NPC_Squid.AttackBite");
	PrecacheScriptSound("NPC_Squid.AttackScream");
	PrecacheScriptSound("NPC_Squid.AttackTailHit");
	PrecacheScriptSound("NPC_Squid.AttackTailWhip");
	PrecacheScriptSound("NPC_Squid.Die");
	PrecacheScriptSound("NPC_Squid.Footstep");
	PrecacheScriptSound("NPC_Squid.Idle");
	PrecacheScriptSound("NPC_Squid.Pain");
	PrecacheScriptSound("NPC_Squid.Scrape");
	PrecacheScriptSound("NPC_Squid.Snort");
//	UTIL_PrecacheOther("squid_spit");
	BaseClass::Precache();
}

void CNPC_Squid::Spawn(void)
{
	Precache();

	SetModel(STRING(GetModelName()));

//	BaseClass::Spawn();

	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MOVETYPE_STEP);
	SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
	SetHullSizeNormal();
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;
	
//	m_bShouldPatrol = GetNPCScriptData().bShouldPatrolAfterSpawn;

	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));

	m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);

	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);

#if SQUID_USES_BONE_FOLLOWERS
	SetBoneCacheFlags(BCF_NO_ANIMATION_SKIP);
	AddSolidFlags(FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST);
#endif
	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
	CapabilitiesAdd(bits_CAP_LIVE_RAGDOLL);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);

	BaseClass::Spawn();

	NPCInit();

	m_flFieldOfView = VIEW_FIELD_WIDE;
	m_NPCState = NPC_STATE_NONE;

	m_flNextSquidAttackTime = CURTIME;
	m_flNextSquidIdleTime = CURTIME;
	m_flNextSquidPainTime = CURTIME;
	m_flNextSquidSnortTime = CURTIME + 5.0f;
	m_flSpottedNewEnemyTime = CURTIME;
	m_flLastSquidChaseTime = FLT_MAX;

	SetDistLook(SQUID_FAR_SPIT_RANGE * 1.2);

	m_flNextAttack = CURTIME + 1;
}

void CNPC_Squid::OnRestore()
{
	BaseClass::OnRestore();
	m_flNextSquidIdleTime = CURTIME + RandomFloat(0, 5);
#if (SQUID_USES_BONE_FOLLOWERS)
	CreateVPhysics();
#endif
}

// Thinks
void CNPC_Squid::NPCThink(void)
{
	BaseClass::NPCThink();
#if (SQUID_USES_BONE_FOLLOWERS)
	// update follower bones
	m_BoneFollowerManager.UpdateBoneFollowers(this);
#endif
	if (npc_squid_debug_attack.GetBool()) Msg("npc_squid's %s next attack is in %.1f\n", GetDebugName(), -CURTIME + m_flNextAttack);
}

// Damage handling
void CNPC_Squid::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr)
{
	CTakeDamageInfo infoCopy = info;

	// Make squids take a bit more damage from SMGs and Shotguns
	if (infoCopy.GetAmmoType() == GetAmmoDef()->Index("SMG1"))
	{
		infoCopy.SetDamage(info.GetDamage() * 1.3f);
	}
	
	if (infoCopy.GetAmmoType() == GetAmmoDef()->Index("357"))
	{
		infoCopy.SetDamage(info.GetDamage() * RandomFloat(1.0f, 1.3f));
	}
		
	BaseClass::TraceAttack(infoCopy, vecDir, ptr);
}

void CNPC_Squid::Event_Killed(const CTakeDamageInfo &info)
{
	SquidDeathSound();
	BaseClass::Event_Killed(info);
}

// NPC sounds
void CNPC_Squid::SquidIdleSound(void)
{
	EmitSound("NPC_Squid.Idle");
	m_flNextSquidIdleTime = CURTIME + RandomFloat(5.0f, 8.0f);

	if (m_flNextSquidSnortTime < CURTIME)
	{
		EmitSound("NPC_Squid.Snort");
		m_flNextSquidSnortTime = CURTIME + RandomFloat(4.5f, 15.0f);
		m_flNextSquidIdleTime = CURTIME + RandomFloat(4.5f, 8.0f);
	}
}

void CNPC_Squid::SquidAttackSound(void)
{
	EmitSound("NPC_Squid.AttackScream");
	m_flNextSquidAttackTime = CURTIME + RandomFloat(2.0f, 5.0f);
}

void CNPC_Squid::SquidPainSound(void)
{
	EmitSound("NPC_Squid.Pain");
	m_flNextSquidPainTime = CURTIME + RandomFloat(1.0f, 2.0f);
}

void CNPC_Squid::SquidDeathSound(void)
{
	EmitSound("NPC_Squid.Die");
}

// Animations
void CNPC_Squid::HandleAnimEvent(animevent_t *pEvent)
{
	if (pEvent->event == AE_NPC_SWISHSOUND)
	{
		return;
	}
	if (pEvent->event == AE_SQUID_TAILWHIP)
	{
		EmitSound("NPC_Squid.AttackTailWhip");
		return;
	}
	if (pEvent->event == AE_SQUID_TAILHIT)
	{
		EmitSound("NPC_Squid.AttackTailHit");
		MeleeCheckContact(true);
		return;
	}
	if (pEvent->event == AE_SQUID_BITE)
	{
		EmitSound("NPC_Squid.AttackBite");
		MeleeCheckContact(false);
		return;
	}
	if (pEvent->event == AE_SQUID_SPIT)
	{
		if (GetEnemy())
		{
			EmitSound("NPC_Squid.AttackSpit");
			Spit(GetEnemy());
			m_flNextAttack = CURTIME + RandomFloat(sk_squid_spit_delay.GetFloat() - 0.5f, sk_squid_spit_delay.GetFloat() + 0.5f);
			// this is set here initially, but then reset to CURTIME + 1.0f in RunTask, 
			// once the spit anim is over. Reasoning - if the anim gets somehow interrupted 
			// after this animevent, we'll still have a reasonable next attack timing.
		}
		return;
	}
	if (pEvent->event == AE_SQUID_FOOTSTEP_L)
	{
		Vector footPosition;
		GetAttachment("foot_left", footPosition);
		FootstepEffect(footPosition, false);
		return;
	}
	if (pEvent->event == AE_SQUID_FOOTSTEP_L_RUN)
	{
		Vector footPosition;
		GetAttachment("foot_left", footPosition);
		FootstepEffect(footPosition, true);
		return;
	}
	if (pEvent->event == AE_SQUID_FOOTSTEP_R)
	{
		Vector footPosition;
		GetAttachment("foot_right", footPosition);
		FootstepEffect(footPosition, false);
		return;
	}
	if (pEvent->event == AE_SQUID_FOOTSTEP_R_RUN)
	{
		Vector footPosition;
		GetAttachment("foot_right", footPosition);
		FootstepEffect(footPosition, true);
		return;
	}
	BaseClass::HandleAnimEvent(pEvent);
}

// Conditions
bool CNPC_Squid::IAmWithinEnemyFOV(CBaseEntity *pEnemy)
{
	return pEnemy->FVisible(this);
}

bool CNPC_Squid::SeenEnemyWithinTime(float flTime)
{
	float flLastSeenTime = GetEnemies()->LastTimeSeen(GetEnemy());
	return (flLastSeenTime != 0.0f && (CURTIME - flLastSeenTime) < flTime);
}

bool CNPC_Squid::IsValidEnemy(CBaseEntity *pEnemy)
{
	Class_T enemyClass = pEnemy->Classify();

	if (enemyClass == CLASS_BULLSEYE && pEnemy->GetParent())
	{
		// This bullseye is in heirarchy with something. If that
		// something is held by the physcannon, this bullseye is 
		// NOT a valid enemy.
		IPhysicsObject *pPhys = pEnemy->GetParent()->VPhysicsGetObject();
		if (pPhys && (pPhys->GetGameFlags() & FVPHYSICS_PLAYER_HELD))
		{
			return false;
		}
	}

	if (GetEnemy() && HasCondition(COND_SEE_ENEMY))
	{
		if (GetEnemy() != pEnemy)
		{
			return false; // TODO: improve this detection. Take into account... size, maybe? At least distance?..
		}
	}

	return BaseClass::IsValidEnemy(pEnemy);
}

int CNPC_Squid::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (flDist > SQUID_MELEE_RANGE)
		return COND_NONE;
	if (flDot < 0.5)
		return COND_NOT_FACING_ATTACK;

	return COND_CAN_MELEE_ATTACK1;
}

int CNPC_Squid::RangeAttack1Conditions(float flDot, float flDist)
{
	if (GetEnemy() == NULL)
		return COND_NONE;
	if (flDist > SQUID_CLOSE_SPIT_RANGE * 2)
		return COND_TOO_FAR_TO_ATTACK;
	if (flDot < 0.5)
		return COND_NOT_FACING_ATTACK;
	if (GetHealth() < m_iMaxHealth * 0.25)
		return COND_TOO_CLOSE_TO_ATTACK;
	if (HasCondition(COND_ENEMY_VULNERABLE))
		return COND_NONE;
	if (!IsLineOfSightClear(GetEnemy(), IGNORE_ACTORS))
		return COND_ENEMY_OCCLUDED;
	if (m_flNextAttack > CURTIME)
		return COND_TOO_CLOSE_TO_ATTACK;

	return COND_CAN_RANGE_ATTACK1;
}

// Schedules, states, tasks
void CNPC_Squid::GatherConditions(void)
{
	if (CombatConfidence() < 25)
	{
		SetCondition(COND_SQUID_LOW_CONFIDENCE);
	}
	else if (CombatConfidence() > 25 && (m_iHealth > m_iMaxHealth * 0.4))
	{
		ClearCondition(COND_SQUID_LOW_CONFIDENCE);
	}

	if (CURTIME - m_flLastSquidChaseTime > 2.0f)
	{
		if (HasCondition(COND_ENEMY_VULNERABLE))
			ClearCondition(COND_ENEMY_VULNERABLE);
		SetCondition(COND_SQUID_CHASE_ENEMY_TOO_LONG);
	}
	else
	{
		ClearCondition(COND_SQUID_CHASE_ENEMY_TOO_LONG);
	}

	if (CURTIME - m_flLastEnemyTime > 15.0f)
	{
		m_squid_hopped_bool = false;
	}

	BaseClass::GatherConditions();
}

void CNPC_Squid::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();
}

int CNPC_Squid::TranslateSchedule(int scheduleType)
{
	if ((scheduleType == SCHED_IDLE_STAND))
		return SCHED_SQUID_IDLE_WANDER;

	return BaseClass::TranslateSchedule(scheduleType);
}

int CNPC_Squid::SelectSchedule(void)
{
	if (m_NPCState == NPC_STATE_IDLE)
	{
		if (HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE) || HasCondition(COND_REPEATED_DAMAGE))
		{
			if (!GetEnemy())
			{
				GetEnemies()->SetFreeKnowledgeDuration(3.0f);
			}
		}
		if (HasCondition(COND_HEAR_COMBAT) || HasCondition(COND_HEAR_BULLET_IMPACT) || HasCondition(COND_HEAR_DANGER))
		{
			CSound *pSound = GetBestSound();

			if (pSound /*&& pSound->IsSoundType(SOUND_COMBAT)*/)
			{
				return SCHED_ALERT_FACE_BESTSOUND; //SCHED_INVESTIGATE_SOUND;
			}
		}

		return SCHED_SQUID_IDLE_WANDER;
	}
	else if (m_NPCState == NPC_STATE_ALERT)
	{
		if (HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE) || HasCondition(COND_REPEATED_DAMAGE))
		{
			if (!GetEnemy())
			{
				GetEnemies()->SetFreeKnowledgeDuration(3.0f);
			}
		}
		if (HasCondition(COND_HEAR_COMBAT))
		{
			CSound *pSound = GetBestSound();

			if (pSound && pSound->IsSoundType(SOUND_COMBAT))
			{
				return SCHED_INVESTIGATE_SOUND;
			}
		}
		return SCHED_SQUID_RUN_RANDOM;
	}
	else if (m_NPCState == NPC_STATE_COMBAT)
	{
		// dead enemy
		if (HasCondition(COND_ENEMY_DEAD))
		{
			// call base class, all code to handle dead enemies is centralized there.
			return BaseClass::SelectSchedule();
		}

		if (CombatConfidence() < 25)
		{
			return SCHED_RUN_FROM_ENEMY;
		}

		if (HasCondition(COND_CAN_MELEE_ATTACK1) && GetEnemy())
		{
			return SCHED_MELEE_ATTACK1;
		}

		if (HasCondition(COND_ENEMY_VULNERABLE)
			&& !IsCurSchedule(SCHED_SQUID_CHARGE_ENEMY))
		{
			m_flLastSquidChaseTime = CURTIME;
			return SCHED_SQUID_CHARGE_ENEMY;
		}

		if (CURTIME < m_flNextAttack)
		{
			return SCHED_SQUID_RUN_RANDOM;
		}

		if (HasCondition(COND_CAN_RANGE_ATTACK1)
			&& !HasCondition(COND_ENEMY_VULNERABLE))
		{
			return SCHED_SQUID_RANGE_ATTACK1_DEFAULT;
		}
	}
	return BaseClass::SelectSchedule();
}

void CNPC_Squid::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_SQUID_ANNOUNCE_RANGE_ATTACK:
	{
		if (m_flNextSquidAttackTime < CURTIME)
		{
			SquidAttackSound();
			if (!m_squid_hopped_bool)
			{
				// Trace upward, hop up only if there's enough room.
				trace_t tr;
				AI_TraceHull(WorldSpaceCenter(), WorldSpaceCenter() + Vector(0, 0, 72), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);
				if (tr.fraction == 1.0)
				{
					if (HasCondition(COND_NEW_ENEMY))
					{
						ResetIdealActivity(ACT_HOP); 
						m_squid_hopped_bool = true;
					}
					else
						TaskComplete();
				}
				else
				{
					TaskComplete();
				}
			}
			else
				TaskComplete();
		}
		else
			TaskComplete();
		break;
	}
	case TASK_RANGE_ATTACK1:
	{
		if (CURTIME < m_flNextAttack)
		{
			TaskComplete();
		}
		else
		{
			ResetIdealActivity(ACT_RANGE_ATTACK1);
		}
		break;
	}
	case TASK_SQUID_REPOSITION_AFTER_RANGE_ATTACK:
	{
		if (GetNavigator()->
			SetRadialGoal(GetNavigator()->GetNodePos(GetNavigator()->GetNearestNode()),
				GetEnemy()->WorldSpaceCenter(),
				WorldSpaceCenter().DistTo(GetEnemy()->WorldSpaceCenter()) + 100,
				5.0f, 16.0f, false))
		{
		//	m_flNextAttack = CURTIME + RandomFloat(1.2f, 1.7f);
			TaskComplete();
		}
		else if (GetNavigator()->
			SetRadialGoal(GetNavigator()->GetNodePos(GetNavigator()->GetNearestNode()),
				GetEnemy()->WorldSpaceCenter(),
				WorldSpaceCenter().DistTo(GetEnemy()->WorldSpaceCenter()) + 100,
				5.0f, 16.0f, true))
		{
		//	m_flNextAttack = CURTIME + RandomFloat(1.2f, 1.7f);
			TaskComplete();
		}
		else
		{
			TaskComplete();
		}
		break;
	}
	case TASK_SQUID_CUT_DISTANCE_TO_ENEMY_FOR_MELEE:
	{
		if (GetEnemy() != NULL)
		{
			GetNavigator()->SetGoalTolerance(64.0f);
			GetNavigator()->SetMaxRouteRebuildTime(5);
			if (GetNavigator()->SetGoal(GetEnemy()->WorldSpaceCenter()))
			{
			//	if (npc_squid_debug.GetBool())
			//		m_flNextAttack = CURTIME + pTask->flTaskData;
			}
			else
			{
			//	if (npc_squid_debug.GetBool())
			//		m_flNextAttack = CURTIME + 1.0f;
				TaskFail(FAIL_NO_ROUTE);
			}
		}
		break;
	}
	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

void CNPC_Squid::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_SQUID_ANNOUNCE_RANGE_ATTACK:
	{
		if (IsActivityFinished())
		{
			m_flNextSquidAttackTime = CURTIME + RandomFloat(5.0f, 10.0f);
			TaskComplete();
		}
		break;
	}
	case TASK_RANGE_ATTACK1:
	{
		if (IsActivityFinished()) // spit animation we called in StartTask
		{
			m_flNextAttack = CURTIME + RandomFloat(sk_squid_spit_delay.GetFloat() - 0.5f, sk_squid_spit_delay.GetFloat() + 0.5f);
			TaskComplete();
		}
		break;
	}
	case TASK_SQUID_CUT_DISTANCE_TO_ENEMY_FOR_MELEE:
	{
		if (GetLocalOrigin().DistTo(GetEnemy()->WorldSpaceCenter()) <= SQUID_MELEE_RANGE)
			TaskComplete();
		else if (GetLocalOrigin().DistTo(GetEnemy()->WorldSpaceCenter()) > SQUID_MELEE_RANGE * 4)
			TaskFail(FAIL_NO_ROUTE);
		else
			TaskFail(FAIL_NO_ROUTE);
		break;
	}
	default:
		BaseClass::RunTask(pTask);
		break;
	}
}

// Movement
bool CNPC_Squid::OverrideMoveFacing(const AILocalMoveGoal_t &move, float flInterval)
{
	if ( GetEnemy())
	{
			AddFacingTarget(GetEnemy(), GetEnemy()->WorldSpaceCenter(), 1.0f, 0.2f);
			return BaseClass::OverrideMoveFacing(move, flInterval);
	}

	if (GetEnemy() && GetNavigator()->GetMovementActivity() == ACT_RUN)
	{
		// FIXME: this will break scripted sequences that walk when they have an enemy
		Vector vecEnemyLKP = GetEnemyLKP();
		if (UTIL_DistApprox(vecEnemyLKP, GetAbsOrigin()) < 512)
		{
			// Only start facing when we're close enough
			AddFacingTarget(GetEnemy(), vecEnemyLKP, 1.0, 0.2);
		}
	}

	return BaseClass::OverrideMoveFacing(move, flInterval);
}

// Functions used anywhere above
void CNPC_Squid::Spit(CBaseEntity *pEnemy)
{
	Vector vSpitPos;
	GetAttachment("mouth", vSpitPos);
	Vector	vTarget = GetEnemy()->WorldSpaceCenter();
	if( !(g_pGameRules->IsSkillLevel(SKILL_EASY)))
		UTIL_PredictedPosition(GetEnemy(), 0.25f, &vTarget);
	Vector vecToss;
	CBaseEntity* pBlocker;
	float flGravity = 900;
	ThrowLimit(vSpitPos, vTarget, flGravity, 3, Vector(0, 0, 0), Vector(0, 0, 0), GetEnemy(), &vecToss, &pBlocker);

	for (int i = 0; i < 3; i++)
	{
		CSharedProjectile *pGrenade = (CSharedProjectile*)CreateEntityByName("shared_projectile");
		pGrenade->SetAbsOrigin(vSpitPos);
		pGrenade->SetAbsAngles(vec3_angle);
		DispatchSpawn(pGrenade);
		pGrenade->SetThrower(this);
		pGrenade->SetOwnerEntity(this);

		if (i == 0)
		{
			pGrenade->SetProjectileStats(Projectile_LARGE, Projectile_ACID, Projectile_CONTACT, 10.0f, 32.0f, flGravity);
			pGrenade->SetAbsVelocity(vecToss);
		}
		else
		{
			pGrenade->SetProjectileStats(Projectile_SMALL, Projectile_ACID, Projectile_CONTACT, 4.0f, 32.0f, flGravity);
			pGrenade->SetAbsVelocity((vecToss + RandomVector(-100, 100)));
		}

		pGrenade->CreateProjectileEffects();

		// Tumble through the air
		pGrenade->SetLocalAngularVelocity(
			QAngle(random->RandomFloat(-250, -500),
				random->RandomFloat(-250, -500),
				random->RandomFloat(-250, -500)));
	}

	m_flLastSquidChaseTime = FLT_MAX;
}

void CNPC_Squid::MeleeCheckContact(bool bWhip)
{
	Vector forward;
	GetVectors(&forward, NULL, NULL);

	Vector vecSrc;
	if (bWhip)
		GetAttachment("tail_contact", vecSrc);
	else
		GetAttachment("mouth_contact", vecSrc);

	float flAdjustedDamage;

	CBaseEntity *pEntity = NULL;

	while ((pEntity = gEntList.FindEntityInSphere(pEntity, vecSrc, 50.0f)) != NULL)
	{
		if (pEntity->m_takedamage != DAMAGE_NO && pEntity != this)
		{
			Vector attackDir = (pEntity->WorldSpaceCenter() - this->WorldSpaceCenter());
			VectorNormalize(attackDir);

			// Generate enough force to make a 75kg guy move away at 250 in/sec
			Vector vecForce = attackDir * ImpulseScale(75, 150);

			Vector offset = pEntity->WorldSpaceCenter();
			// If it's being held by the player, break that bond
			Pickup_ForcePlayerToDropThisObject(pEntity);

			if (pEntity->IsPlayer())
			{
				//	float yawKick = random->RandomFloat(-48, -24);
				//	pEntity->ViewPunch(QAngle(-16, yawKick, 2));

				CBasePlayer *player = ToBasePlayer(pEntity);
				if (player->IsInAVehicle() && player->GetVehicleEntity()->ClassMatches("prop_vehicle_apc")) // for part 2; DI REVISIT later
					flAdjustedDamage = 0;
				else
					flAdjustedDamage = 20;

				if (flAdjustedDamage > 0)
				{
					CTakeDamageInfo info(this, this, vecForce, offset, flAdjustedDamage, DMG_SLASH);
					pEntity->TakeDamage(info);
				}

				if (bWhip)
				{
					Vector	dir = pEntity->GetAbsOrigin() - GetAbsOrigin();
					VectorNormalize(dir);

					QAngle angles;
					VectorAngles(dir, angles);
					Vector forward, right;
					AngleVectors(angles, &forward, &right, NULL);

					//Push the target back
					pEntity->ApplyAbsVelocityImpulse(-right * 1.0f + forward * 500.0f);
				}
			}
			else
			{
				flAdjustedDamage = 20;

				if (flAdjustedDamage > 0)
				{
					CTakeDamageInfo info(this, this, vecForce, offset, flAdjustedDamage, DMG_SLASH);
					pEntity->TakeDamage(info);
				}
			}
		}
	}

	if (HasCondition(COND_ENEMY_VULNERABLE))
	{
		ClearCondition(COND_ENEMY_VULNERABLE);
	}
}

void CNPC_Squid::FootstepEffect(const Vector &origin, bool bHeavy)
{
	if (RandomFloat(0, 1) > 0.65f) return; // skip this thing 1/3 of the time

	bool InPVS = ((UTIL_FindClientInPVS(edict()) != NULL)
		|| (UTIL_ClientPVSIsExpanded()
			&& UTIL_FindClientInVisibilityPVS(edict())));

	CBasePlayer *pPlayerEnt = AI_GetSinglePlayer();
	float flDistToPlayerSqr = (GetAbsOrigin() - pPlayerEnt->GetAbsOrigin()).LengthSqr();

	if (flDistToPlayerSqr > 600 * 600 || !InPVS)
	{
		return;
	}

	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "NPC_Squid.Footstep", &origin);

	trace_t tr;
	AI_TraceLine(origin, origin - Vector(0, 0, 0), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);
	float yaw = RandomInt(0, 0);
	for (int i = 0; i < 2; i++)
	{
		if (UTIL_PointContents(tr.endpos + Vector(0, 0, 1)) & MASK_WATER)
		{
			float flWaterZ = UTIL_FindWaterSurface(tr.endpos, tr.endpos.z, tr.endpos.z + 100.0f);

			CEffectData	data;
			data.m_fFlags = 0;
			data.m_vOrigin = tr.endpos;
			data.m_vOrigin.z = flWaterZ;
			data.m_vNormal = Vector(0, 0, 1);
			data.m_flScale = bHeavy ? RandomFloat(10.0, 14.0) : RandomFloat(5, 8.0);

			//	if (bHeavy)
			DispatchEffect("watersplash", data);
			//	else
			//		data.m_flScale = RandomFloat(10.0, 14.0);
			//	DispatchEffect("waterripple", data);
		}
		else
		{
			if (bHeavy)
			{
				Vector dir = UTIL_YawToVector(yaw + i * 180) * 10;
				VectorNormalize(dir);
				dir.z = 0.25;
				VectorNormalize(dir);
				g_pEffects->Dust(tr.endpos, dir, 12, 50);
			}
		}
	}
}

// Datadesc
BEGIN_DATADESC(CNPC_Squid)
	DEFINE_FIELD(m_vecSaveSpitVelocity, FIELD_VECTOR),
//	DEFINE_FIELD(m_flNextSquidIdleTime, FIELD_TIME), // not much need to memorise these
//	DEFINE_FIELD(m_flNextSquidAttackTime, FIELD_TIME),
//	DEFINE_FIELD(m_flNextSquidPainTime, FIELD_TIME),
//	DEFINE_FIELD(m_flNextSquidSnortTime, FIELD_TIME),
	DEFINE_FIELD(m_flLastSquidChaseTime, FIELD_TIME),
	DEFINE_FIELD(m_squid_hopped_bool, FIELD_BOOLEAN),
	// Physics Bone Follower test
#if SQUID_USES_BONE_FOLLOWERS
	DEFINE_EMBEDDED(m_BoneFollowerManager),
#endif
END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_squid, CNPC_Squid);
LINK_ENTITY_TO_CLASS(npc_bullsquid, CNPC_Squid);
LINK_ENTITY_TO_CLASS(monster_bullchicken, CNPC_Squid);

AI_BEGIN_CUSTOM_NPC(npc_squid, CNPC_Squid)

//DECLARE_USES_SCHEDULE_PROVIDER(CAI_BlendedNPC)

DECLARE_TASK(TASK_SQUID_ANNOUNCE_RANGE_ATTACK)
DECLARE_TASK(TASK_SQUID_REPOSITION_AFTER_RANGE_ATTACK)
DECLARE_TASK(TASK_SQUID_CUT_DISTANCE_TO_ENEMY_FOR_MELEE)

DECLARE_CONDITION(COND_SQUID_TARGET_WITHIN_FAR_SPIT_RANGE)
DECLARE_CONDITION(COND_SQUID_TARGET_WITHIN_CLOSE_SPIT_RANGE)
DECLARE_CONDITION(COND_SQUID_TARGET_WITHIN_MELEE_RANGE)
DECLARE_CONDITION(COND_SQUID_TARGET_WITHIN_RANGE_BUT_UNREACHABLE)
DECLARE_CONDITION(COND_SQUID_LOW_CONFIDENCE)
DECLARE_CONDITION(COND_SQUID_CHASE_ENEMY_TOO_LONG)

//DECLARE_SQUADSLOT(SQUID_SQUADSLOT_1)

DECLARE_ANIMEVENT(AE_SQUID_FOOTSTEP_L)
DECLARE_ANIMEVENT(AE_SQUID_FOOTSTEP_L_RUN)
DECLARE_ANIMEVENT(AE_SQUID_FOOTSTEP_R)
DECLARE_ANIMEVENT(AE_SQUID_FOOTSTEP_R_RUN)
DECLARE_ANIMEVENT(AE_SQUID_BITE)
DECLARE_ANIMEVENT(AE_SQUID_SPIT)
DECLARE_ANIMEVENT(AE_SQUID_TAILWHIP)
DECLARE_ANIMEVENT(AE_SQUID_TAILHIT)

DEFINE_SCHEDULE
(
	SCHED_SQUID_IDLE_WANDER,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_WANDER						90384"		// 90 to 384 units
	"		TASK_WALK_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_STOP_MOVING				0"
	"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_IDLE"
	"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_IDLE"
	"		TASK_WAIT_RANDOM				3"
	"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_SQUID_IDLE_WANDER" // keep doing it
	""
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_COMBAT"
	"		COND_NEW_ENEMY"
)

DEFINE_SCHEDULE
(
	SCHED_SQUID_RANGE_ATTACK1_FAR,

	"	Tasks"
	"	TASK_SQUID_ANNOUNCE_RANGE_ATTACK	0"
	"	TASK_RANGE_ATTACK1					0"
	""
	"	Interrupts"
	"	COND_CAN_MELEE_ATTACK1"
	"	COND_ENEMY_DEAD"
	"	COND_LOST_ENEMY"
)

DEFINE_SCHEDULE
(
	SCHED_SQUID_RANGE_ATTACK1_CLOSE,

	"	Tasks"
	"	TASK_SQUID_ANNOUNCE_RANGE_ATTACK	0"
	"	TASK_RANGE_ATTACK1					0"
	"	TASK_SET_SCHEDULE					SCHEDULE:SCHED_SQUID_CHARGE_ENEMY"
	""
	"	Interrupts"
	"	COND_ENEMY_DEAD"
	"	COND_LOST_ENEMY"
	"	COND_CAN_MELEE_ATTACK1"
)

DEFINE_SCHEDULE
(
	SCHED_SQUID_RANGE_ATTACK1_DEFAULT,

	"	Tasks"
	"	TASK_SQUID_ANNOUNCE_RANGE_ATTACK	0"
	"	TASK_RANGE_ATTACK1					0"
	""
	"	Interrupts"
	"	COND_ENEMY_VULNERABLE"
	"   COND_SQUID_LOW_CONFIDENCE"
	"	COND_HEAVY_DAMAGE"
	"	COND_ENEMY_DEAD"
	"	COND_LOST_ENEMY"
)

DEFINE_SCHEDULE
(
	SCHED_SQUID_CHARGE_ENEMY,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_SQUID_RANGE_ATTACK1_DEFAULT"
	"		TASK_STOP_MOVING				0"
	"		TASK_GET_CHASE_PATH_TO_ENEMY	512"
	"		TASK_RUN_PATH_TIMED				2.0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_FACE_ENEMY					0"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
	"		COND_HEAVY_DAMAGE"
	"		COND_ENEMY_UNREACHABLE"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_SQUID_CHASE_ENEMY_TOO_LONG"
)

DEFINE_SCHEDULE
(
	SCHED_SQUID_RUN_RANDOM,
	"	Tasks"
	"		TASK_SET_ROUTE_SEARCH_TIME		1"	// Spend 1 seconds trying to build a path if stuck
	"		TASK_GET_PATH_TO_RANDOM_NODE	250"
	"		TASK_FACE_ENEMY					0"
	"		TASK_RUN_PATH_TIMED				1.0"
	"		TASK_FACE_ENEMY					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
	"		COND_HEAVY_DAMAGE"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_ENEMY_VULNERABLE"
)

AI_END_CUSTOM_NPC()
#endif // BULLSQUID

//====================================
// Water squid (Dark Interval)
//====================================
#if defined (WATERSQUID)
// constraint entity for the stabbed ragdolls
class CWaterSquidConstraint : public CBaseAnimating
{
	DECLARE_CLASS(CWaterSquidConstraint, CBaseAnimating);
public:
	DECLARE_DATADESC();

	void	Spawn(void);
	void	Precache(void);
	void	DragThink(void);

	IPhysicsConstraint *CreateConstraint(CBaseEntity *pOwner, IPhysicsObject *pTargetPhys, IPhysicsConstraintGroup *pGroup);
	static CWaterSquidConstraint *Create(CBaseEntity *pOwner, CBaseEntity *pObject2);

public:
	IPhysicsConstraint *m_pConstraint;
	CHandle<CBaseEntity> m_hOwner;
};

BEGIN_DATADESC(CWaterSquidConstraint)
	DEFINE_PHYSPTR(m_pConstraint),
	DEFINE_FIELD(m_hOwner, FIELD_EHANDLE),
	DEFINE_THINKFUNC(DragThink),
END_DATADESC()

LINK_ENTITY_TO_CLASS(watersquid_constraint, CWaterSquidConstraint);

//-----------------------------------------------------------------------------
// Purpose: To by usable by the constraint system, this needs to have a phys model.
//-----------------------------------------------------------------------------
void CWaterSquidConstraint::Spawn(void)
{
	Precache();
	SetModel("models/props_junk/cardboard_box001a.mdl");
	AddEffects(EF_NODRAW);

	// We don't want this to be solid, because we don't want it to collide with the hydra.
	SetSolid(SOLID_VPHYSICS);
	AddSolidFlags(FSOLID_NOT_SOLID);
	VPhysicsInitShadow(true, true);

	// Disable movement on this sucker, we're going to move him manually
	SetMoveType(MOVETYPE_FLY);

	BaseClass::Spawn();

	m_pConstraint = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaterSquidConstraint::Precache(void)
{
	PrecacheModel("models/props_junk/cardboard_box001a.mdl");
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Update the impale entity's position to the hydra's desired
//-----------------------------------------------------------------------------
void CWaterSquidConstraint::DragThink(void)
{	
	if (!m_hOwner)
	{
		// Remove ourselves.
		m_pConstraint->Deactivate();
		UTIL_Remove(this);
		return;
	}
	if (!(m_hOwner->MyNPCPointer()))
	{
		// Remove ourselves.
		m_pConstraint->Deactivate();
		UTIL_Remove(this);
		return;
	}

	if (m_pConstraint->GetAttachedObject() == NULL)
	{
		// Remove ourselves.
		m_pConstraint->Deactivate();
		UTIL_Remove(this);
		return;
	}

	// Ask the owner where it'd like the attached object, and move ourselves there
	Vector vecOrigin;
	QAngle vecAngles;
	m_hOwner->MyNPCPointer()->GetAttachment(m_hOwner->MyNPCPointer()->LookupAttachment("Grab"), vecOrigin, vecAngles);
	SetAbsOrigin(vecOrigin);
//	SetAbsAngles(vecAngles);

	NDebugOverlay::Cross3D(vecOrigin, 8, 125, 175, 255, true, 0.1f);

	SetNextThink(CURTIME);
}

//-----------------------------------------------------------------------------
// Purpose: Activate/create the constraint
//-----------------------------------------------------------------------------
IPhysicsConstraint *CWaterSquidConstraint::CreateConstraint(CBaseEntity *pOwner, IPhysicsObject *pTargetPhys, IPhysicsConstraintGroup *pGroup)
{
	m_hOwner = pOwner;

	IPhysicsObject *pGrabPhysObject = VPhysicsGetObject();
	Assert(pGrabPhysObject);
	
	constraint_fixedparams_t constraint;
	constraint.Defaults();
	constraint.InitWithCurrentObjectState(pGrabPhysObject, pTargetPhys);
	constraint.constraint.Defaults();
	m_pConstraint = physenv->CreateFixedConstraint(pGrabPhysObject, pTargetPhys, pGroup, constraint);
	
	if (m_pConstraint)
	{
		m_pConstraint->SetGameData((void *)this);
	}

	SetThink(&CWaterSquidConstraint::DragThink);
	SetNextThink(CURTIME);
	//	SetParent(m_hHydra->GetBaseEntity(), m_hHydra->LookupAttachment("needle"));
	return m_pConstraint;
}

//-----------------------------------------------------------------------------
// Purpose: Create a Hydra Impale between the hydra and the entity passed in
//-----------------------------------------------------------------------------
CWaterSquidConstraint *CWaterSquidConstraint::Create(CBaseEntity *pOwner, CBaseEntity *pTarget)
{
	if (!pOwner || !pTarget) return NULL;

	if (!(pOwner->MyNPCPointer())) return NULL;

	Vector vecOrigin;
	QAngle vecAngles;
	int attachment = pOwner->MyNPCPointer()->LookupAttachment("Grab");
	pOwner->MyNPCPointer()->GetAttachment(attachment, vecOrigin, vecAngles);
//	pTarget->Teleport(&vecOrigin, NULL, &vec3_origin);

	CWaterSquidConstraint *pConstraint = (CWaterSquidConstraint *)CBaseEntity::Create("watersquid_constraint", vecOrigin, vecAngles);
	if (!pConstraint)
		return NULL;

	IPhysicsObject *pTargetPhysObject = pTarget->VPhysicsGetObject();
	if (!pTargetPhysObject)
	{
		Warning(" Error: Tried to watersquid_constraint an entity without a physics model.\n");
		return NULL;
	}

	IPhysicsConstraintGroup *pGroup = NULL;

	if (!pConstraint->CreateConstraint(pOwner, pTargetPhysObject, pGroup))
		return NULL;

	return pConstraint;
}

class CNPC_WaterSquid : public CAI_BaseNPC
{
	DECLARE_CLASS(CNPC_WaterSquid, CAI_BaseNPC);
	DECLARE_DATADESC();
public:
	CNPC_WaterSquid();
	~CNPC_WaterSquid();
	Class_T Classify(void) { return CLASS_BULLSQUID; }
	void Precache(void);
	void Spawn(void);
	void RunAI(void);
private:
	void ThinkDragVictim(void);
public:
	int OnTakeDamage_Alive(const CTakeDamageInfo &info);
	void Event_Killed(const CTakeDamageInfo &info);

	void IdleSound(void);
	void AlertSound(void);
	void PainSound(const CTakeDamageInfo &info);
	void PreAttackSound(void);
	void AttackHitSound(void);

	void HandleAnimEvent(animevent_t *pEvent);
	Activity NPC_TranslateActivity(Activity activity);

	bool IsValidEnemy(CBaseEntity *pEnemy);
	int MeleeAttack1Conditions(float flDot, float flDist); // grab and drag down
	int MeleeAttack2Conditions(float flDot, float flDist); // bite if victim is in water and not above us
	int RangeAttack1Conditions(float flDot, float flDist); // spit through water
	bool IsUnreachable(CBaseEntity* pEntity);			// Is entity is unreachable?

	void BuildScheduleTestBits(void);
	int TranslateSchedule(int scheduleType);
	int SelectSchedule(void);

	void StartTask(const Task_t *pTask);
	void RunTask(const Task_t *pTask);

//	bool OverrideMove(float flInterval);
//	void DoMovement(float flInterval, const Vector &MoveTarget); 
//	void SteerArrive(Vector &Steer, const Vector &Target);
	float ChargeSteer(void);

	bool HandleChargeImpact(Vector vecImpact, CBaseEntity *pEntity);
	void Spit(CBaseEntity *pEnemy);
	void MeleeCheckContact(CBaseEntity *pEnemy);
	void Bite(CBaseEntity *pEnemy);

private:
	bool AttachVictim(CBaseEntity *pVictim, float fDist);
	void ReleaseVictim();
public:	
	float m_nextsquidwarnsound_time;
	Vector m_vecSaveSpitVelocity;	
private:
	float m_nextsquiddragattack_time;
	float m_nextsquidbiteattack_time;
	float m_nextsquidspitattack_time;

	int m_victimmovetype_int;
	//CBaseEntity *m_dragvictim_handle;
	CHandle<CBaseEntity> m_dragvictim_handle;
	bool m_bounds_shrink_boolean;
	// the constraint system is unused right now.
	CWaterSquidConstraint *m_dragconstraint_handle;
	// Cached attachment points
	int	m_mouth_attachment;
	int m_tentacles_attachment;

public:

	float MaxYawSpeed(void) { return 90; }

	static const Vector	m_vecAccelerationMax;
	static const Vector	m_vecAccelerationMin;
	
	Vector m_vecLastMoveTarget;
	bool m_bHasMoveTarget;
	bool m_bIgnoreSurface;
//	float m_lastleftbodyofwater_time;

	enum
	{
		COND_WATERSQUID_SPOTTED_BY_ENEMY = LAST_SHARED_CONDITION,
		COND_WATERSQUID_HIDE_MODE
	};

	enum
	{
		SCHED_WATERSQUID_IDLE_WANDER = LAST_SHARED_SCHEDULE,
		SCHED_WATERSQUID_ENTER_HIDE_MODE,
		SCHED_WATERSQUID_EXIT_HIDE_MODE,
		SCHED_WATERSQUID_CHASE_ENEMY,
		SCHED_WATERSQUID_BITE_ATTACK // charge consisting of three animations: open mouth, loop swim with open mouth, and close mouth
	};

	enum
	{
		TASK_WATERSQUID_GET_PATH_TO_RANDOM_NODE = LAST_SHARED_TASK,
		TASK_WATERSQUID_SUBMERGE,
		TASK_WATERSQUID_EMERGE,
		TASK_WATERSQUID_GET_PATH_TO_HIDE_NODE,
		TASK_WATERSQUID_BITE_ATTACK
	};

	DEFINE_CUSTOM_AI;
};

int AE_WATERSQUID_SPIT; // speaks for itself. Launch acid projectile.
int AE_WATERSQUID_GRAB_VICTIM; // check if target is within range. If yes, start context think to grab entity with us.
int AE_WATERSQUID_RELEASE_VICTIM; // end of melee anim, we went underwater with our target - release them, stop context think.
int AE_WATERSQUID_BIG_SPLASH; // suprise come out of water during melee attack - produce big water splash
int AE_WATERSQUID_BITE;
Activity ACT_WSQUID_IDLE;
Activity ACT_WSQUID_SWIM;
Activity ACT_WSQUID_SWIM_FAST;
Activity ACT_WSQUID_IDLE_SUBMERGED;
Activity ACT_WSQUID_SWIM_SUBMERGED;
Activity ACT_WSQUID_SWIM_FAST_SUBMERGED;
Activity ACT_WSQUID_EMERGE;
Activity ACT_WSQUID_SUBMERGE;
Activity ACT_WSQUID_BITE_START;
Activity ACT_WSQUID_BITE_LOOP;
Activity ACT_WSQUID_BITE_END;

ConVar sk_watersquid_bite_dmg("sk_watersquid_bite_dmg", "10");
ConVar npc_watersquid_use_new_method("npc_watersquid_use_new_method", "1", 0, "Old method means turning on Noclip for grabbed players, new method is using physical suction and no Noclip");

const Vector CNPC_WaterSquid::m_vecAccelerationMax = Vector(1024, 1024, 128);
const Vector CNPC_WaterSquid::m_vecAccelerationMin = Vector(-1024, -1024, -512);

inline void WSquid_TraceHull_SkipPhysics(const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &hullMin,
	const Vector &hullMax, unsigned int mask, const CBaseEntity *ignore,
	int collisionGroup, trace_t *ptr, float minMass);

// Constructor
CNPC_WaterSquid::CNPC_WaterSquid() 
{
	m_hNPCFileInfo = LookupNPCInfoScript("npc_watersquid");
	m_dragvictim_handle = NULL;
	m_dragconstraint_handle = NULL;
}
CNPC_WaterSquid::~CNPC_WaterSquid()
{
//	if (m_dragconstraint_handle) UTIL_Remove(m_dragconstraint_handle); m_dragconstraint_handle = NULL; // Causes crashes
}

// Generic spawn-related
void CNPC_WaterSquid::Precache(void)
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	PrecacheScriptSound("NPC_WaterSquid.PreAttack");
	PrecacheScriptSound("NPC_WaterSquid.AttackHit");
	PrecacheScriptSound("NPC_WaterSquid.Idle");
	PrecacheScriptSound("NPC_WaterSquid.Pain");
	UTIL_PrecacheOther("watersquid_constraint");
	BaseClass::Precache();
}

void CNPC_WaterSquid::Spawn(void)
{
	Precache();
	SetModel(STRING(GetModelName()));

	m_mouth_attachment = -1;
	m_tentacles_attachment = -1;

	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MoveType_t(GetNPCScriptData().NPC_Movement_MoveType_int));
	SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
	SetHullSizeNormal();
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;
	
//	m_bShouldPatrol = GetNPCScriptData().bShouldPatrolAfterSpawn;

	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));
	m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);
	
	NPCInit();

	CapabilitiesClear();
//	CapabilitiesAdd(bits_CAP_MOVE_SWIM);
	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);

//	AddFlag(FL_SWIM | FL_STEPMOVEMENT);
	SetBoneCacheFlags(BCF_NO_ANIMATION_SKIP);
	SetCollisionGroup(HL2COLLISION_GROUP_GUNSHIP);
	SetNavType(NAV_GROUND);
	SetGravity(UTIL_ScaleForGravity(100.0f));
	m_flGroundSpeed = 60.0f;
	m_NPCState = NPC_STATE_NONE;
	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);
	SetDistLook(1024);

	m_nextsquiddragattack_time = CURTIME;
	m_nextsquidbiteattack_time = CURTIME;
	m_nextsquidspitattack_time = CURTIME;

	m_nextsquidwarnsound_time = CURTIME;

	m_dragconstraint_handle = NULL;

	float wl = UTIL_FindWaterSurface(GetAbsOrigin(), -4096, 4096);
//	wl += 80.0f;
	Vector originWL = GetAbsOrigin();
	originWL.z = wl;
	Teleport(&originWL, &GetAbsAngles(), &GetAbsVelocity());

	m_bIgnoreSurface = false;

	m_mouth_attachment = LookupAttachment("Mouth");
	m_tentacles_attachment = LookupAttachment("Grab");

	RegisterThinkContext("DragVictimContext");
}

// Thinks
void CNPC_WaterSquid::RunAI(void)
{
	BaseClass::RunAI();	
	float nextthink = 0.3f;
	if (m_hCine || GetEnemy() && GetAbsOrigin().DistToSqr(GetEnemy()->GetAbsOrigin()) < 1024 * 1024) nextthink = 0.1f;

	SetNextThink(CURTIME + nextthink);
}

void CNPC_WaterSquid::ThinkDragVictim(void)
{
	// Todo: rework to not be player-specific
	CBasePlayer *player = UTIL_GetLocalPlayer();
	if (!player)
	{
		SetContextThink(NULL, 0, "DragVictimContext");
		return;
	}

	Vector grab;
	GetAttachment(m_tentacles_attachment, grab);

	Vector mouth;
	GetAttachment(m_mouth_attachment, mouth);
	
	if (npc_watersquid_use_new_method.GetBool())
	{
		if (player->WorldSpaceCenter().DistTo(mouth) > 96) // got caught on something, cannot pull with us
		{
			Msg("watersquid %s had to let go of victim - cannot pull with it!\n", GetDebugName());
			ReleaseVictim();
			SetContextThink(NULL, 0, "DragVictimContext");
			return;
		}
	//	if (m_dragconstraint_handle)
	//	{
	//		UTIL_Remove(m_dragconstraint_handle);
	//		m_dragconstraint_handle = CWaterSquidConstraint::Create(this, m_dragvictim_handle);
	//	}
		
		Vector	vecForce = mouth - player->WorldSpaceCenter();
		VectorNormalize(vecForce);
	//	float	dist = VectorNormalize(vecForce);

		player->SetBaseVelocity(vecForce * 360); // FIXME: 360 is magic number corresponding with the animation velocity
		player->AddFlag(FL_BASEVELOCITY);
	}
	else
	{
		grab.z -= 40.0f;
		player->Teleport(&grab, NULL, &GetAbsVelocity());
	}

	if (player->GetFlags() & FL_ONGROUND)
	{
		// Try to fight OnGround
		player->RemoveFlag(FL_ONGROUND);
	}

	if (player->GetWaterLevel() >= WL_Waist)
	{
		UTIL_Bubbles(mouth + Vector(-20, -20, -20), mouth + Vector(20, 20, 20), RandomInt(6, 18));
	}

	SetNextThink(CURTIME, "DragVictimContext");
}

// Damage handling
int	CNPC_WaterSquid::OnTakeDamage_Alive(const CTakeDamageInfo &info)
{
	if (info.GetAttacker()->ClassMatches(GetClassName()) || info.GetAttacker()->ClassMatches("npc_squid"))
		return 0;

	if (info.GetDamageType() & DMG_ACID || info.GetDamageType() & DMG_NERVEGAS)
		return 0;

	if (IsHeavyDamage(info))
	{
		ReleaseVictim();
	}

	return BaseClass::OnTakeDamage_Alive(info);
}

void CNPC_WaterSquid::Event_Killed(const CTakeDamageInfo &info)
{
//	if (GetEnemy())
	{
		ReleaseVictim();
	}
	BaseClass::Event_Killed(info);
}

// NPC sounds
void CNPC_WaterSquid::IdleSound(void)
{
	Vector mouth;
	GetAttachment("mouth", mouth);
	UTIL_Bubbles(mouth - Vector(2, 2, 2), mouth + Vector(2, 2, 2), 8);
	EmitSound("NPC_WaterSquid.Idle");
}

void CNPC_WaterSquid::AlertSound(void)
{
	Vector mouth;
	GetAttachment("mouth", mouth);
	UTIL_Bubbles(mouth - Vector(2, 2, 2), mouth + Vector(2, 2, 2), 8);
	EmitSound("NPC_WaterSquid.PreAttack");
}

void CNPC_WaterSquid::PreAttackSound(void)
{
	Vector mouth;
	GetAttachment("mouth", mouth);
	UTIL_Bubbles(mouth - Vector(2, 2, 2), mouth + Vector(2, 2, 2), 8);
	EmitSound("NPC_WaterSquid.PreAttack");
	m_nextsquidwarnsound_time = CURTIME + 10; 
}

void CNPC_WaterSquid::AttackHitSound(void)
{
	EmitSound("NPC_WaterSquid.AttackHit");
}

void CNPC_WaterSquid::PainSound(const CTakeDamageInfo &info)
{
	Vector mouth;
	GetAttachment("mouth", mouth);
	UTIL_Bubbles(mouth - Vector(2, 2, 2), mouth + Vector(2, 2, 2), 8);
	EmitSound("NPC_WaterSquid.Pain");
}

// Animations
void CNPC_WaterSquid::HandleAnimEvent(animevent_t *pEvent)
{
	if (pEvent->event == AE_WATERSQUID_SPIT)
	{
		if (GetEnemy())
		{
			Spit(GetEnemy());
		}
		return;
	}

	if (pEvent->event == AE_WATERSQUID_GRAB_VICTIM)
	{
		if (GetEnemy())
		{
			MeleeCheckContact(GetEnemy());
			m_nextsquiddragattack_time = CURTIME + 10.0f; // a big delay, normally we'll reset next attack time on RELEASE_VICTIM animevent. But just in case we don't get there, use this "insurance" delay.
		}
		return;
	}
	
	if (pEvent->event == AE_WATERSQUID_RELEASE_VICTIM)
	{
	//	if (GetEnemy())
		{
			ReleaseVictim();
			m_nextsquiddragattack_time = CURTIME + 3.0f; // a big delay, normally we'll reset next attack time on RELEASE_VICTIM animevent. But just in case we don't get there, use this "insurance" delay.
		}
		return;
	}
	
	if (pEvent->event == AE_WATERSQUID_BIG_SPLASH)
	{
		CEffectData	data;
		data.m_fFlags = 0;
		data.m_vOrigin = GetAbsOrigin();
		data.m_vNormal = Vector(0, 0, 1);
		data.m_flScale = RandomFloat(10.0, 14.0);
		DispatchEffect("watersplash", data);
		return;
	}
	
	if (pEvent->event == AE_WATERSQUID_BITE)
	{
		if (GetEnemy())
		{
			MeleeCheckContact(GetEnemy());
			m_nextsquidbiteattack_time = CURTIME + RandomFloat(2.5,4);
		}
		return;
	}

	BaseClass::HandleAnimEvent(pEvent);
}

Activity CNPC_WaterSquid::NPC_TranslateActivity(Activity activity)
{
	if (activity == ACT_WALK)
	{
		return ACT_FLY;
	}

	activity = BaseClass::NPC_TranslateActivity(activity);

	return activity;
}

// Conditions
bool CNPC_WaterSquid::IsValidEnemy(CBaseEntity *pEnemy)
{
	// If we're already dragging a victim, ignore all others
	if (m_dragvictim_handle && m_dragvictim_handle != GetEnemy())
	{
		return false;
	}

	return BaseClass::IsValidEnemy(pEnemy);
}

int CNPC_WaterSquid::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (CURTIME < m_nextsquidspitattack_time) return COND_NONE;
	if (GetEnemy() && GetEnemy()->GetGroundEntity() == this)
		return COND_CAN_MELEE_ATTACK1;
	if (GetEnemy() && (GetEnemy()->GetWaterLevel() == WL_Eyes)) return COND_NONE; // don't do this to players who are under water already
	if (flDist > 128.0f) return COND_TOO_FAR_TO_ATTACK;
//	if (GetEnemy() && fabs(GetAbsOrigin().z - GetEnemy()->GetAbsOrigin().z) > 64) return COND_TOO_FAR_TO_ATTACK;
	if (flDot < 0.5) return COND_NOT_FACING_ATTACK;
	return COND_CAN_MELEE_ATTACK1;
}

int CNPC_WaterSquid::MeleeAttack2Conditions(float flDot, float flDist)
{
	if (CURTIME < m_nextsquidbiteattack_time) return COND_NONE;
	if (GetEnemy() && GetEnemy()->GetWaterLevel() != WL_Eyes) return COND_NONE; // don't do this to players who are *not* under water to see it
//	if (GetEnemy() && fabs(GetAbsOrigin().z - GetEnemy()->GetAbsOrigin().z) > 64) return COND_NONE;
//	if (flDist < 256.0f) return COND_TOO_CLOSE_TO_ATTACK;
	if (flDist > 512.0f) return COND_TOO_FAR_TO_ATTACK;
	if (flDot < 0.5) return COND_NOT_FACING_ATTACK;
	return COND_CAN_MELEE_ATTACK2;
}

int CNPC_WaterSquid::RangeAttack1Conditions(float flDot, float flDist)
{
	if (CURTIME < m_nextsquidspitattack_time) return COND_NONE;
	if (GetEnemy() && GetEnemy()->GetWaterLevel() == WL_NotInWater) return COND_NONE;
//	if (GetEnemy() && GetEnemy()->EyePosition().z > GetAbsOrigin().z) return COND_NONE;
	if (flDist > 256.0f) return COND_TOO_FAR_TO_ATTACK;
	if (flDot < 0.5) return COND_NOT_FACING_ATTACK;
	return COND_CAN_RANGE_ATTACK1;
}

bool CNPC_WaterSquid::IsUnreachable(CBaseEntity *pEntity)
{
	float UNREACHABLE_DIST_TOLERANCE_SQ = (300 * 300);

	// Note that it's ok to remove elements while I'm iterating
	// as long as I iterate backwards and remove them using FastRemove
	for (int i = m_UnreachableEnts.Size() - 1; i >= 0; i--)
	{
		// Remove any dead elements
		if (m_UnreachableEnts[i].hUnreachableEnt == NULL)
		{
			m_UnreachableEnts.FastRemove(i);
		}
		else if (pEntity == m_UnreachableEnts[i].hUnreachableEnt)
		{
			// Test for reachability on any elements that have timed out
			if (CURTIME > m_UnreachableEnts[i].fExpireTime ||
				pEntity->GetAbsOrigin().DistToSqr(m_UnreachableEnts[i].vLocationWhenUnreachable) > UNREACHABLE_DIST_TOLERANCE_SQ)
			{
				m_UnreachableEnts.FastRemove(i);
				return false;
			}
			return true;
		}
	}
	return false;
}

// Schedules, states, tasks
void CNPC_WaterSquid::BuildScheduleTestBits(void)
{
	BaseClass::BuildScheduleTestBits();

	if (IsCurSchedule(SCHED_MELEE_ATTACK1))
	{
		// Ignore enemy lost, new enemy, damage etc
		ClearCustomInterruptConditions();

		if (GetEnemy()
			&& GetEnemy()->GetAbsOrigin().DistTo(GetAbsOrigin()) <= 400
			&& GetEnemy()->GetAbsOrigin().DistTo(GetAbsOrigin()) > 300
			&& m_nextsquidwarnsound_time <= CURTIME)
			PreAttackSound();
	}
}

int CNPC_WaterSquid::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_IDLE_WANDER: return SCHED_WATERSQUID_IDLE_WANDER; break;

	case SCHED_CHASE_ENEMY: return SCHED_WATERSQUID_CHASE_ENEMY; break;

	case SCHED_MELEE_ATTACK2: return SCHED_WATERSQUID_BITE_ATTACK; break;

	default: return BaseClass::TranslateSchedule(scheduleType); break;
	}
}

int CNPC_WaterSquid::SelectSchedule(void)
{
	if (m_NPCState == NPC_STATE_ALERT)
	{
		return SCHED_PATROL_WALK;
	}
	if (m_NPCState == NPC_STATE_COMBAT)
	{
		if (GetEnemy())
		{
			if (HasCondition(COND_CAN_RANGE_ATTACK1))
				return SCHED_RANGE_ATTACK1;

			if (HasCondition(COND_CAN_MELEE_ATTACK1))
				return SCHED_MELEE_ATTACK1;

			if (HasCondition(COND_CAN_MELEE_ATTACK2))
				return SCHED_MELEE_ATTACK2;

			if (m_nextsquidbiteattack_time < CURTIME /*&& HasCondition(COND_TOO_FAR_TO_ATTACK)*/ && GetEnemy() && GetEnemy()->GetWaterLevel() == WL_Eyes)
				return SCHED_CHASE_ENEMY;

			if (m_nextsquiddragattack_time < CURTIME && GetEnemy() && !IsUnreachable(GetEnemy()) && GetEnemy()->GetWaterLevel() != WL_Eyes)
				return SCHED_CHASE_ENEMY; // todo: handle needing to back up in order to be able to do melee attack 2
			
			if (HasCondition(COND_REPEATED_DAMAGE))
				return SCHED_RUN_FROM_ENEMY;
			
			return SCHED_RUN_RANDOM;
		}

		if (m_nextsquidspitattack_time > CURTIME && m_nextsquiddragattack_time > CURTIME)
			return SCHED_RUN_RANDOM;
	}

	return BaseClass::SelectSchedule();
}

void CNPC_WaterSquid::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{

	case TASK_WATERSQUID_BITE_ATTACK:
	{
		EmitSound("NPC_WaterSquid.PreAttack");
		GetMotor()->MoveStop();
		SetActivity(Activity(ACT_WSQUID_BITE_START));
	}
	break;

	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

void CNPC_WaterSquid::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{

	case TASK_WATERSQUID_BITE_ATTACK:
	{
		Activity eActivity = GetActivity();

		// See if we're trying to stop after hitting/missing our target
		if (eActivity == Activity(ACT_WSQUID_BITE_END))
		{
			if (IsActivityFinished())
			{
				TaskComplete();
				return;
			}

			// Still in the process of slowing down. Run movement until it's done.
			AutoMovement();
			return;
		}

		// Check for manual transition
		if ((eActivity == Activity(ACT_WSQUID_BITE_START)) && (IsActivityFinished()))
		{
			SetActivity(Activity(ACT_WSQUID_BITE_LOOP));
		}

		// See if we're still running
		if (eActivity == Activity(ACT_WSQUID_BITE_LOOP) || Activity(ACT_WSQUID_BITE_START))
		{
			if (RandomInt(0, 3) == 0)
			{
				UTIL_Bubbles(EyePosition() - Vector(2, 2, 2), EyePosition() + Vector(2, 2, 2), 8);
			}

			if (HasCondition(COND_LOST_ENEMY) || HasCondition(COND_ENEMY_DEAD))
			{
				SetActivity(Activity(ACT_WSQUID_BITE_END));
				return;
			}
			else
			{
				if (GetEnemy() != NULL)
				{
					Vector	goalDir = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
					VectorNormalize(goalDir);

					if (DotProduct(BodyDirection2D(), goalDir) < 0.25f)
					{
						SetActivity(Activity(ACT_WSQUID_BITE_END));
					}
				}
			}
		}

		// Steer towards our target
		float idealYaw;
		if (GetEnemy() == NULL)
		{
			idealYaw = GetMotor()->GetIdealYaw();
		}
		else
		{
			idealYaw = CalcIdealYaw(GetEnemy()->GetAbsOrigin());
		}

		// Add in our steering offset
		idealYaw += ChargeSteer();

		// Turn to face
		GetMotor()->SetIdealYawAndUpdate(idealYaw);

		// See if we're going to run into anything soon
		//	ChargeLookAhead();

		// Make sure we're not going to exit the water with our movement
		if ((UTIL_PointContents(GetAbsOrigin() + GetAbsVelocity() * 2) & MASK_WATER) == false)
		{
			NDebugOverlay::Cross3D(GetAbsOrigin() + GetAbsVelocity() * 2, 8, 255, 125, 50, true, 5);
			// Crash unless we're trying to stop already
			if (eActivity != Activity(ACT_WSQUID_BITE_END))
			{
				SetActivity(Activity(ACT_WSQUID_BITE_END));
			}
		}

		// Let our animations simply move us forward. Keep the result
		// of the movement so we know whether we've hit our target.
		AIMoveTrace_t moveTrace;
		if (AutoMovement(GetEnemy(), &moveTrace) == false)
		{
			// Only stop if we hit the world
			if (HandleChargeImpact(moveTrace.vEndPosition, moveTrace.pObstruction))
			{
				// If we're starting up, this is an error
				if (eActivity == Activity(ACT_WSQUID_BITE_START))
				{
					TaskFail("Unable to make initial movement of charge\n");
					return;
				}

				// Crash unless we're trying to stop already
				if (eActivity != Activity(ACT_WSQUID_BITE_END))
				{
					SetActivity(Activity(ACT_WSQUID_BITE_END));
				}
			}
		}
	}
	break;

	default:
		BaseClass::RunTask(pTask);
		break;
	}
}
/*
// Movement
bool CNPC_WaterSquid::OverrideMove(float flInterval)
{
	return false;
}

void CNPC_WaterSquid::DoMovement(float flInterval, const Vector &MoveTarget)
{
	// dvs: something is setting this bit, causing us to stop moving and get stuck that way
//	Forget(bits_MEMORY_TURNING);

	Vector Steer, SteerAvoid, SteerRel;
	Vector forward, right, up;
	//Get our orientation vectors.
	GetVectors(&forward, &right, &up);

	SteerArrive(Steer, MoveTarget);

	//See if we need to avoid any obstacles.
//	if (SteerAvoidObstacles(SteerAvoid, GetAbsVelocity(), forward, right, up))
//	{
//		//Take the avoidance vector
//		Steer = SteerAvoid;
//	}

	//Clamp our ideal steering vector to within our physical limitations.

	float fForwardSteer = DotProduct(Steer, forward);
	float fRightSteer = DotProduct(Steer, right);
	float fUpSteer = DotProduct(Steer, up);

	if (fForwardSteer > 0)
	{
		fForwardSteer = MIN(fForwardSteer, m_vecAccelerationMax.x);
	}
	else
	{
		fForwardSteer = MAX(fForwardSteer, m_vecAccelerationMin.x);
	}

	if (fRightSteer > 0)
	{
		fRightSteer = MIN(fRightSteer, m_vecAccelerationMax.y);
	}
	else
	{
		fRightSteer = MAX(fRightSteer, m_vecAccelerationMin.y);
	}

	if (fUpSteer > 0)
	{
		fUpSteer = MIN(fUpSteer, m_vecAccelerationMax.z);
	}
	else
	{
		fUpSteer = MAX(fUpSteer, m_vecAccelerationMin.z);
	}

	Steer = (fForwardSteer*forward) + (fRightSteer*right) + (fUpSteer*up);

	SteerRel.x = fForwardSteer;
	SteerRel.y = fRightSteer;
	SteerRel.z = fUpSteer;

	NDebugOverlay::Cross3D(GetAbsOrigin() + Steer * flInterval, 1, 0, 255, 255, true, 0.1f);
	if (UTIL_PointContents(Steer * flInterval) != CONTENTS_WATER && UTIL_PointContents(Steer * flInterval) != CONTENTS_SLIME)
	{
		return;
	}

	ApplyAbsVelocityImpulse(Steer * flInterval);

	Vector vecNewVelocity = GetAbsVelocity();
	float flLength = vecNewVelocity.Length();

	//Clamp our final speed
	if (flLength > m_flGroundSpeed)
	{
		vecNewVelocity *= (m_flGroundSpeed / flLength);
		flLength = m_flGroundSpeed;
	}

	Vector	workVelocity = vecNewVelocity;
	
	//Move along the current velocity vector
	if (WalkMove(workVelocity * flInterval, MASK_NPCSOLID) == false)
	{
		//Attempt a half-step
		if (WalkMove((workVelocity*0.5f) * flInterval, MASK_NPCSOLID) == false)
		{
			//Restart the velocity
			//VectorNormalize( m_vecVelocity );
			vecNewVelocity *= 0.5f;
		}
		else
		{
			//Cut our velocity in half
			vecNewVelocity *= 0.5f;
		}
	}

	SetAbsVelocity(vecNewVelocity);
}

void CNPC_WaterSquid::SteerArrive(Vector &Steer, const Vector &Target)
{
	Vector goalPos = Target;
	// Trace down and make sure we're not floating too low (64 = min ground dist).
	trace_t	tr;
	AI_TraceHull(goalPos, goalPos - Vector(0, 0, 64), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

	// Move up otherwise
	if (tr.fraction < 1.0f)
	{
		goalPos.z += (64 * (1.0f - tr.fraction));
	}

	// Do an additional trace for max flying height, too.
	AI_TraceHull(goalPos, goalPos - Vector(0, 0, 512), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

	// Move up otherwise
	if (tr.fraction == 1.0f)
	{
		goalPos.z -= 256;
	}

	Vector Offset = goalPos - GetAbsOrigin();
	float fTargetDistance = Offset.Length();

	float fIdealSpeed = m_flGroundSpeed * (fTargetDistance / 64.0f);
	float fClippedSpeed = MIN(fIdealSpeed, m_flGroundSpeed);

	Vector DesiredVelocity(0, 0, 0);

	if (fTargetDistance > 128.0f)
	{
		DesiredVelocity = (fClippedSpeed / fTargetDistance) * Offset;
	}

	Steer = DesiredVelocity - GetAbsVelocity();
}
*/

// Functions used anywhere above
class WSquidTraceFilterSkipPhysics : public CTraceFilter
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE(WSquidTraceFilterSkipPhysics);

	WSquidTraceFilterSkipPhysics(const IHandleEntity *passentity, int collisionGroup, float minMass)
		: m_pPassEnt(passentity), m_collisionGroup(collisionGroup), m_minMass(minMass)
	{
	}
	virtual bool ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask)
	{
		if (!StandardFilterRules(pHandleEntity, contentsMask))
			return false;

		if (!PassServerEntityFilter(pHandleEntity, m_pPassEnt))
			return false;

		// Don't test if the game code tells us we should ignore this collision...
		CBaseEntity *pEntity = EntityFromEntityHandle(pHandleEntity);
		if (pEntity)
		{
			if (!pEntity->ShouldCollide(m_collisionGroup, contentsMask))
				return false;

			if (!g_pGameRules->ShouldCollide(m_collisionGroup, pEntity->GetCollisionGroup()))
				return false;

			// don't test small moveable physics objects (unless it's an NPC)
			if (!pEntity->IsNPC() && pEntity->GetMoveType() == MOVETYPE_VPHYSICS)
			{
				IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
				Assert(pPhysics);
				if (pPhysics->IsMoveable() && pPhysics->GetMass() < m_minMass)
					return false;
			}
		}

		return true;
	}
private:
	const IHandleEntity *m_pPassEnt;
	int m_collisionGroup;
	float m_minMass;
};

inline void WSquid_TraceHull_SkipPhysics(const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &hullMin,
	const Vector &hullMax, unsigned int mask, const CBaseEntity *ignore,
	int collisionGroup, trace_t *ptr, float minMass)
{
	Ray_t ray;
	ray.Init(vecAbsStart, vecAbsEnd, hullMin, hullMax);
	WSquidTraceFilterSkipPhysics traceFilter(ignore, collisionGroup, minMass);
	enginetrace->TraceRay(ray, mask, &traceFilter, ptr);
}

float CNPC_WaterSquid::ChargeSteer(void)
{
	trace_t	tr;
	Vector	testPos, steer, forward, right;
	QAngle	angles;
	const float	testLength = m_flGroundSpeed * 1.0f;

	//Get our facing
	GetVectors(&forward, &right, NULL);

	steer = forward;

	const float faceYaw = UTIL_VecToYaw(forward);

	//Offset right
	VectorAngles(forward, angles);
	angles[YAW] += 45.0f;
	AngleVectors(angles, &forward);

	//Probe out
	testPos = GetAbsOrigin() + (forward * testLength);

	//Offset by step height
	Vector testHullMins = GetHullMins();
	testHullMins.z += (StepHeight() * 2);

	//Probe
	WSquid_TraceHull_SkipPhysics(GetAbsOrigin(), testPos, testHullMins, GetHullMaxs(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr, VPhysicsGetObject()->GetMass() * 0.5f);

	//Add in this component
	steer += (right * 0.5f) * (1.0f - tr.fraction);

	//Offset left
	angles[YAW] -= 90.0f;
	AngleVectors(angles, &forward);

	//Probe out
	testPos = GetAbsOrigin() + (forward * testLength);

	// Probe
	WSquid_TraceHull_SkipPhysics(GetAbsOrigin(), testPos, testHullMins, GetHullMaxs(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr, VPhysicsGetObject()->GetMass() * 0.5f);

	//Add in this component
	steer -= (right * 0.5f) * (1.0f - tr.fraction);
	return UTIL_AngleDiff(UTIL_VecToYaw(steer), faceYaw);
}

bool CNPC_WaterSquid::HandleChargeImpact(Vector vecImpact, CBaseEntity *pEntity)
{
	if (!pEntity || pEntity->IsWorld())
	{
		if (pEntity->IsWorld())
		{
			return true;
		}
	}

	// Hit anything we don't like
	if (IRelationType(pEntity) == D_HT && (GetNextAttack() < CURTIME))
	{
		EmitSound("NPC_WaterSquid.AttackHit");
		
		Bite(pEntity);

		pEntity->ApplyAbsVelocityImpulse((BodyDirection2D() * 200));

		if (!pEntity->IsAlive() && GetEnemy() == pEntity)
		{
			SetEnemy(NULL);
		}

		m_nextsquidbiteattack_time = CURTIME + RandomFloat(2.5, 4);
		SetActivity(ACT_WSQUID_BITE_END);

		// We've hit something, so clear our miss count
		return false;
	}

	// Hit something we don't hate. If it's not moveable, crash into it.
	if (pEntity->GetMoveType() == MOVETYPE_NONE || pEntity->GetMoveType() == MOVETYPE_PUSH)
		return true;

	// If it's a vphysics object that's too heavy, crash into it too.
	if (pEntity->GetMoveType() == MOVETYPE_VPHYSICS)
	{
		IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
		if (pPhysics)
		{
			// If the object is being held by the player, knock it out of his hands
			if (pPhysics->GetGameFlags() & FVPHYSICS_PLAYER_HELD)
			{
				Pickup_ForcePlayerToDropThisObject(pEntity);
				return false;
			}

			if ((!pPhysics->IsMoveable() || pPhysics->GetMass() > VPhysicsGetObject()->GetMass() * 0.5f))
				return true;
		}
	}

	return false;
}

void CNPC_WaterSquid::Spit(CBaseEntity *pEnemy)
{
	Assert(GetEnemy());
	Vector mouth;
	GetAttachment("mouth", mouth);
	Vector	vTarget = GetEnemy()->GetAbsOrigin();
	Vector vecToss;
	CBaseEntity* pBlocker;
	float flGravity = 620;
	ThrowLimit(mouth, vTarget, flGravity, 3, Vector(0, 0, 0), Vector(0, 0, 0), GetEnemy(), &vecToss, &pBlocker);

	for (int i = 0; i < 4; i++)
	{
		CSharedProjectile *pGrenade = (CSharedProjectile*)CreateEntityByName("shared_projectile");
		pGrenade->SetAbsOrigin(mouth);
		pGrenade->SetAbsAngles(vec3_angle);
		DispatchSpawn(pGrenade);
		pGrenade->SetThrower(this);
		pGrenade->SetOwnerEntity(this);

		if (i == 0)
		{
			pGrenade->SetProjectileStats(Projectile_LARGE, Projectile_ACID, Projectile_CONTACT, 5.0f, 32.0f, flGravity);
			pGrenade->SetAbsVelocity(vecToss);
		}
		else
		{
			pGrenade->SetProjectileStats(Projectile_SMALL, Projectile_ACID, Projectile_CONTACT, 2.0f, 16.0f, flGravity);
			pGrenade->SetAbsVelocity((vecToss + RandomVector(-100, 100)));
		}

		pGrenade->CreateProjectileEffects();

		// Tumble through the air
		pGrenade->SetLocalAngularVelocity(
			QAngle(random->RandomFloat(-250, -500),
				random->RandomFloat(-250, -500),
				random->RandomFloat(-250, -500)));
	}

	DispatchParticleEffect("blood_impact_yellow_01", mouth, RandomAngle(0, 360));

	m_nextsquidspitattack_time = CURTIME + RandomFloat(3, 5);
	//	m_nConsequitiveAttacks++;
}

void CNPC_WaterSquid::MeleeCheckContact(CBaseEntity *pEnemy)
{
	if (pEnemy == NULL)
	{
		Warning("Water squid %s tried grabbing enemy, but enemy is NULL!\n", GetDebugName());
		return;
	}

	float grabdist = 96.0f;
	if (pEnemy->IsPlayer())	grabdist = 96.0f; // for humanoid sized prey, check up to 96 units away from mouth
	else
	{
		if (FClassnameIs(pEnemy, "npc_houndeye")) grabdist = 160.0f; // they have wider hitbox - check from mouth to origin could fail, but visually squid should be able to grab them
		if (FClassnameIs(pEnemy, "npc_vortigaunt")) grabdist = 128.0f; // more or less same as humanoids

	}

	if (!(pEnemy->IsPlayer()) && pEnemy->GetMoveType() != MOVETYPE_STEP)
		return; // don't bother catching flying enemies and such

	Vector mouth;
	GetAttachment(m_mouth_attachment, mouth);

	Vector check = pEnemy->GetAbsOrigin(); // usually points us at the feet of the target... it's ok, squid is mostly likely to attack from below

	if ((mouth - check).Length() > grabdist)
	{
		//EmitSound( some 'miss melee' sound here);
		return;
	}
	else
	{
		float flAdjustedDamage;
		Vector meleeDir = mouth - check;

		trace_t tr;
		AI_TraceLine(mouth, mouth + meleeDir * grabdist, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		if (pEnemy->IsPlayer() && tr.fraction != 1.0f)
		{
			flAdjustedDamage = sk_watersquid_bite_dmg.GetFloat();

			ClearMultiDamage();
			CTakeDamageInfo info(this, this, flAdjustedDamage, DMG_SLASH);
			info.ScaleDamageForce(4.0f);
			CalculateMeleeDamageForce(&info, meleeDir, mouth);
			pEnemy->DispatchTraceAttack(info, meleeDir, &tr);
			ApplyMultiDamage();

			if (!AttachVictim(pEnemy, grabdist)) return;
		}		
		else
		{
			flAdjustedDamage = 1.0f; // we don't want to kill it, just make it emit pain sound
			ClearMultiDamage();
			CTakeDamageInfo info(this, this, flAdjustedDamage, DMG_SLASH | DMG_REMOVENORAGDOLL);
			CalculateMeleeDamageForce(&info, meleeDir, mouth);
			pEnemy->DispatchTraceAttack(info, meleeDir, &tr);
			ApplyMultiDamage();
		}		
	}
}

void CNPC_WaterSquid::Bite(CBaseEntity *pEnemy)
{
	if (pEnemy == NULL)
	{
		Warning("Water squid %s tried biting enemy, but enemy is NULL!\n", GetDebugName());
		return;
	}

	float grabdist = 96.0f;
	if (pEnemy->IsPlayer())	grabdist = 96.0f; // for humanoid sized prey, check up to 96 units away from mouth
	else
	{
		if (FClassnameIs(pEnemy, "npc_houndeye")) grabdist = 160.0f; // they have wider hitbox - check from mouth to origin could fail, but visually squid should be able to grab them
		if (FClassnameIs(pEnemy, "npc_vortigaunt")) grabdist = 128.0f; // more or less same as humanoids
	}

	Vector mouth;
	GetAttachment(m_mouth_attachment, mouth);

	Vector check = pEnemy->WorldSpaceCenter(); // usually points us at the feet of the target... it's ok, squid is mostly likely to attack from below

	if ((mouth - check).Length() > grabdist)
	{
		//EmitSound( some 'miss melee' sound here);
		return;
	}
	else
	{
		float flAdjustedDamage;
		Vector meleeDir = mouth - check;

		trace_t tr;
		AI_TraceLine(mouth, mouth + meleeDir * grabdist, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		if ( tr.fraction != 1.0f)
		{
			flAdjustedDamage = sk_watersquid_bite_dmg.GetFloat();

			ClearMultiDamage();
			CTakeDamageInfo info(this, this, flAdjustedDamage, DMG_SLASH);
			CalculateMeleeDamageForce(&info, meleeDir, mouth);
			info.ScaleDamageForce(0.5f);
			pEnemy->DispatchTraceAttack(info, meleeDir, &tr);
			ApplyMultiDamage();
		}
	}
}

bool CNPC_WaterSquid::AttachVictim(CBaseEntity *pVictim, float grabDist)
{
	if (!pVictim) return false;
	if (!(pVictim->IsAlive())) return false;
	if (!GetEnemy()) return false;
	if (GetEnemy() != pVictim) return false;
//	if (!GetEnemy()->IsPlayer()) return false;
	if (GetEnemy()->GetMoveType() == MOVETYPE_NOCLIP || GetEnemy()->GetMoveType() == MOVETYPE_FLY) return false;

	//CBasePlayer *player = UTIL_GetLocalPlayer();
	m_dragvictim_handle = pVictim;

	if (npc_watersquid_use_new_method.GetBool())
	{
		if (!VPhysicsGetObject() || !m_dragvictim_handle->VPhysicsGetObject())
		{
			Warning("Water squid %s cannot grab victim due to no vphysics object!\n", GetDebugName());
			return false;
		}
	}

	Vector grab;
	GetAttachment(m_tentacles_attachment, grab);
	
	if (/*player*/m_dragvictim_handle->WorldSpaceCenter().DistTo(grab) > grabDist
		&& m_dragvictim_handle->GetAbsOrigin().DistTo(grab) > grabDist
		&& m_dragvictim_handle->EyePosition().DistTo(grab) > grabDist) // check three grabbable points - top, mid and bottom
	{
		return false;
	}

	// Blow up the bounds to default size. It's shrunken down in ReleaseVictim().
	if (m_bounds_shrink_boolean)
	{
		SetCollisionBounds(Vector(-40, -16, -64), Vector(32, 16, 8));
	}
	if (npc_watersquid_use_new_method.GetBool())
	{
	//	m_dragconstraint_handle = CWaterSquidConstraint::Create(this, m_dragvictim_handle);

	//	if (m_dragconstraint_handle)
	//	{
	//		Msg("Water squid %s grabbed victim via constraint\n", GetDebugName());
	//	}
	//	else
	//	{
	//		Msg("Water squid %s failed to make a grab constraint!\n", GetDebugName());
	//		return false;
	//	}

		// if player is on the ladder, disengage him
		if (m_dragvictim_handle->IsPlayer())
		{
			CBasePlayer *pPlayer = ToBasePlayer(m_dragvictim_handle);
			if (pPlayer)
			{
				if (pPlayer->GetMoveType() == MOVETYPE_LADDER)
				{
					pPlayer->ExitLadder();
				}
			}
		}

		Vector	vecForce = grab - m_dragvictim_handle->WorldSpaceCenter();
		VectorNormalize(vecForce);
		//	float	dist = VectorNormalize(vecForce);

		m_dragvictim_handle->VelocityPunch(vecForce * 200);

		m_victimmovetype_int = m_dragvictim_handle->GetMoveType();
		m_dragvictim_handle->SetMoveType(MOVETYPE_FLY);

		m_dragvictim_handle->AddEFlags(EFL_IS_BEING_LIFTED_BY_BARNACLE);
	}
	else
	{
		/*player*/m_dragvictim_handle->SetMoveType(MOVETYPE_NOCLIP);
	}
	SetContextThink(&CNPC_WaterSquid::ThinkDragVictim, CURTIME, "DragVictimContext");
	return true;
}

void CNPC_WaterSquid::ReleaseVictim(void)
{
//	CBasePlayer *player = UTIL_GetLocalPlayer();
	if (!m_dragvictim_handle) return;

	if (GetEnemy() && /*!(GetEnemy()->IsPlayer())*/(GetEnemy() != m_dragvictim_handle)) return;

	Vector vFwd;
	GetVectors(&vFwd, NULL, NULL);
	if (npc_watersquid_use_new_method.GetBool())
	{
	}
	else
	{
		if (/*player*/m_dragvictim_handle->GetMoveType() != MOVETYPE_NOCLIP)
		{
			return;
		}

		trace_t touchtest;
		UTIL_TraceHull(/*player*/m_dragvictim_handle->GetAbsOrigin(),
			/*player*/m_dragvictim_handle->GetAbsOrigin(), /*player*/m_dragvictim_handle->WorldAlignMins(), /*player*/m_dragvictim_handle->WorldAlignMaxs(),
			CONTENTS_SOLID, this,
			COLLISION_GROUP_NONE, &touchtest);

		if (touchtest.fraction != 1.0)
		{
			/*player*/m_dragvictim_handle->Teleport(&(/*player*/m_dragvictim_handle->GetAbsOrigin() + Vector(0, 0, 16.0f)), NULL, NULL);
		}

		/*player*/m_dragvictim_handle->Teleport(
			&(/*player*/m_dragvictim_handle->GetAbsOrigin() + vFwd * 8),
			NULL,
			&(/*player*/m_dragvictim_handle->GetAbsVelocity() + vFwd * 400));
	}

	// Shrink the bound to avoid player getting stuck. These are restored on the next attack.
	SetCollisionBounds(Vector(-8, -8, -64), Vector(8, 8, 8));

	m_bounds_shrink_boolean = true;

	// more bubbles to cover up the hackery a bit. // removed, was prone to crashing for some reason?
	UTIL_Bubbles(
		/*player*/m_dragvictim_handle->GetAbsOrigin() + Vector(-20, -20, -20), 
		/*player*/m_dragvictim_handle->GetAbsOrigin() + Vector(20, 20, 64), RandomInt(24, 36));
	// push the squid backwards to clear room for the player.

	this->VelocityPunch(-vFwd * 400);
	// restore player to normal movement.
	if (npc_watersquid_use_new_method.GetBool())
	{
	//	if (m_dragconstraint_handle)
	//	{
	//		UTIL_Remove(m_dragconstraint_handle);
	//		m_dragconstraint_handle = NULL;
	//	}
		m_dragvictim_handle->SetMoveType((MoveType_t)m_victimmovetype_int);
		m_dragvictim_handle->RemoveEFlags(EFL_IS_BEING_LIFTED_BY_BARNACLE);
		m_dragvictim_handle->RemoveFlag(FL_BASEVELOCITY);
	}
	else
	{
		/*player*/m_dragvictim_handle->SetMoveType(MOVETYPE_WALK);
	}

	// release the handle
	if(m_dragvictim_handle)	m_dragvictim_handle = NULL;

	// a farewell kiss
	/*
	float flAdjustedDamage = sk_watersquid_bite_dmg.GetFloat();

	ClearMultiDamage();
	CTakeDamageInfo info(this, this, flAdjustedDamage, DMG_SLASH);
	player->TakeDamage(info);
	ApplyMultiDamage();
	*/
	Vector mouth;
	GetAttachment(m_mouth_attachment, mouth);
	// with bubbles
	UTIL_Bubbles(mouth + Vector(-20, -20, -20), mouth + Vector(20, 20, 20), RandomInt(24, 36));

//	Msg("watersquid %s released the victim\n", GetDebugName());

	SetContextThink(NULL, 0, "DragVictimContext");
}

// Datadesc
BEGIN_DATADESC(CNPC_WaterSquid)
	DEFINE_FIELD(m_bHasMoveTarget, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bIgnoreSurface, FIELD_BOOLEAN),
	DEFINE_FIELD(m_vecSaveSpitVelocity, FIELD_VECTOR),
//	DEFINE_FIELD(m_nextsquidwarnsound_time, FIELD_TIME),
	DEFINE_FIELD(m_nextsquiddragattack_time, FIELD_TIME),
	DEFINE_FIELD(m_nextsquidspitattack_time, FIELD_TIME),
	DEFINE_FIELD(m_nextsquidbiteattack_time, FIELD_TIME),
	DEFINE_FIELD(m_bounds_shrink_boolean, FIELD_BOOLEAN),
	DEFINE_FIELD(m_dragvictim_handle, FIELD_EHANDLE),
	DEFINE_FIELD(m_victimmovetype_int, FIELD_INTEGER),
	DEFINE_FIELD(m_dragconstraint_handle, FIELD_EHANDLE),
	DEFINE_FIELD(m_mouth_attachment, FIELD_INTEGER),
	DEFINE_FIELD(m_tentacles_attachment, FIELD_INTEGER),
//	DEFINE_PHYSPTR(m_dragconstraint_ent),
	DEFINE_THINKFUNC(ThinkDragVictim),
END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_watersquid, CNPC_WaterSquid);

AI_BEGIN_CUSTOM_NPC(npc_watersquid, CNPC_WaterSquid)
DECLARE_ANIMEVENT(AE_WATERSQUID_SPIT)
DECLARE_ANIMEVENT(AE_WATERSQUID_GRAB_VICTIM)
DECLARE_ANIMEVENT(AE_WATERSQUID_RELEASE_VICTIM)
DECLARE_ANIMEVENT(AE_WATERSQUID_BIG_SPLASH)
DECLARE_ANIMEVENT(AE_WATERSQUID_BITE)
DECLARE_TASK(TASK_WATERSQUID_GET_PATH_TO_RANDOM_NODE)
DECLARE_TASK(TASK_WATERSQUID_EMERGE)
DECLARE_TASK(TASK_WATERSQUID_SUBMERGE)
DECLARE_TASK(TASK_WATERSQUID_GET_PATH_TO_HIDE_NODE)
DECLARE_TASK(TASK_WATERSQUID_BITE_ATTACK)
DECLARE_CONDITION(COND_WATERSQUID_HIDE_MODE)
DECLARE_CONDITION(COND_WATERSQUID_SPOTTED_BY_ENEMY)
DECLARE_ACTIVITY(ACT_WSQUID_EMERGE)
DECLARE_ACTIVITY(ACT_WSQUID_SUBMERGE)
DECLARE_ACTIVITY(ACT_WSQUID_IDLE)
DECLARE_ACTIVITY(ACT_WSQUID_IDLE_SUBMERGED)
DECLARE_ACTIVITY(ACT_WSQUID_SWIM)
DECLARE_ACTIVITY(ACT_WSQUID_SWIM_FAST)
DECLARE_ACTIVITY(ACT_WSQUID_SWIM_SUBMERGED)
DECLARE_ACTIVITY(ACT_WSQUID_SWIM_FAST_SUBMERGED)
DECLARE_ACTIVITY(ACT_WSQUID_BITE_START)
DECLARE_ACTIVITY(ACT_WSQUID_BITE_LOOP)
DECLARE_ACTIVITY(ACT_WSQUID_BITE_END)

DEFINE_SCHEDULE
(
	SCHED_WATERSQUID_IDLE_WANDER,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_IDLE_WANDER"
	"		TASK_SET_TOLERANCE_DISTANCE			64"
	"		TASK_SET_ROUTE_SEARCH_TIME			3"
	"		TASK_WATERSQUID_GET_PATH_TO_RANDOM_NODE	200"
	"		TASK_FACE_PATH						0"
	"		TASK_RUN_PATH						0"
	"		TASK_WAIT_FOR_MOVEMENT				0"
	"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_IDLE"
	"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_WATERSQUID_IDLE_WANDER" // keep doing it
	""
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_HEAR_DANGER"
	"		COND_NEW_ENEMY"
)

DEFINE_SCHEDULE
(
	SCHED_WATERSQUID_ENTER_HIDE_MODE,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_IDLE_WANDER"
	"		TASK_WATERSQUID_SUBMERGE			0"
	""
	"	Interrupts"
)

DEFINE_SCHEDULE
(
	SCHED_WATERSQUID_EXIT_HIDE_MODE,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_IDLE_WANDER"
	"		TASK_WATERSQUID_EMERGE				0"
	""
	"	Interrupts"
)

DEFINE_SCHEDULE
(
	SCHED_WATERSQUID_CHASE_ENEMY,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_BACK_AWAY_FROM_ENEMY"
	"		TASK_GET_CHASE_PATH_TO_ENEMY	300"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_STOP_MOVING				0"
	"		TASK_FACE_ENEMY					0"
	"	"
	"	Interrupts"
	"		COND_ENEMY_UNREACHABLE"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_TOO_CLOSE_TO_ATTACK"
	"		COND_LOST_ENEMY"
	"		COND_ENEMY_DEAD"
//	"		COND_HEAR_DANGER"
)

DEFINE_SCHEDULE
(
	SCHED_WATERSQUID_BITE_ATTACK,

	"	Tasks"
	"		TASK_STOP_MOVING					0"
	"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
	"		TASK_FACE_ENEMY						0"
	"		TASK_WATERSQUID_BITE_ATTACK			0"
	"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_BACK_AWAY_FROM_ENEMY"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_HEAVY_DAMAGE"

	// These are deliberately left out so they can be detected during the 
	// charge Task and correctly play the charge stop animation.
	//"		COND_NEW_ENEMY"
	//"		COND_ENEMY_DEAD"
	//"		COND_LOST_ENEMY"
)
AI_END_CUSTOM_NPC()
#endif // WATERSQUID

//====================================
// Houndeye (Dark Interval)
//====================================
#if defined (HOUNDEYE)
#define	HOUNDEYE_FOLLOW_DISTANCE	350
#define	HOUNDEYE_FOLLOW_DISTANCE_SQR	(HOUNDEYE_FOLLOW_DISTANCE*HOUNDEYE_FOLLOW_DISTANCE)
#define	HOUNDEYE_OBEY_FOLLOW_TIME	FLT_MAX//5.0f

// Houndeye follow behavior
class CAI_HoundeyeFollowBehavior : public CAI_FollowBehavior
{
	typedef CAI_FollowBehavior BaseClass;

public:

	CAI_HoundeyeFollowBehavior()
		: BaseClass(AIF_ANTLION) // for now, reuse the antlion template
	{
	}

	bool FarFromFollowTarget(void)
	{
		return (GetFollowTarget() && (GetAbsOrigin() - GetFollowTarget()->GetAbsOrigin()).LengthSqr() > HOUNDEYE_FOLLOW_DISTANCE_SQR);
	}

	bool ShouldFollow(void)
	{
		if (GetFollowTarget() == NULL)
			return false;

		if (GetEnemy() != NULL)
			return false;

		return true;
	}
};

enum HoundeyeMoveState_e
{
	HOUNDEYE_MOVE_FREE,
	HOUNDEYE_MOVE_FOLLOW,
//	HOUNDEYE_MOVE_FIGHT_TO_GOAL,
};

class CNPC_Houndeye : public CAI_BaseActorNoBlend
{
	DECLARE_CLASS(CNPC_Houndeye, CAI_BaseActorNoBlend);
	DECLARE_DATADESC();
public:
	CNPC_Houndeye();
	~CNPC_Houndeye();
	Class_T	Classify(void); 
	int ObjectCaps(void)
	{
		return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE;
	}

	void Spawn(void);
	void Precache(void);

	void NPCThink();
	void ChargeUpThink(void);
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	
	float GetHitgroupDamageMultiplier(int iHitGroup, const CTakeDamageInfo &info);
	void TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );

	void IdleSound(void);
	void WarnSound(void);
	void HuntSound(void);
	void WarmUpSound(void);
	void AlertSound(void);
	void PainSound(const CTakeDamageInfo &info);
	void DeathSound(const CTakeDamageInfo &info);
	void FootstepSound(void);
		
	void HandleAnimEvent(animevent_t *pEvent);
	Activity NPC_TranslateActivity(Activity activity);
	void OnChangeActivity(Activity eNewActivity);
	//	bool HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt);
	//	bool CanBeRescuedFromBarnacle(void) { return true; }

	bool IsValidEnemy(CBaseEntity *pEnemy);
	float InnateRange1MinRange(void) { return 48; }
	float InnateRange1MaxRange(void) { return 96; }
	float InnateRange2MinRange(void) { return 48; }
	float InnateRange2MaxRange(void) { return 96; }
	int RangeAttack2Conditions(float flDot, float flDist);
	void GatherConditions(void);
	void ClearAttackConditions(void);

	bool CreateBehaviors(void);
	bool ShouldBehaviorSelectSchedule(CAI_BehaviorBase *pBehavior);
	void PrescheduleThink(void);
	int TranslateSchedule(int scheduleType);
	int SelectSchedule(void);
	void StartTask(const Task_t *pTask);
	void RunTask(const Task_t *pTask);

	float MaxYawSpeed(void);
	int ChooseMoveSchedule(void);
	void SetMoveState(HoundeyeMoveState_e state);
	
	void SonicAttack(void); // charged up strong sonic attack
	void SonicBark(void); // a weaker bark that deals minimum damage, but requires no cooldown
	void SetFollowTarget(CBaseEntity *pTarget);
	bool ShouldResumeFollow(void);
	bool ShouldAbandonFollow(void);

	void InputStartFollowingTarget(inputdata_t &inputdata);
	void InputStopFollowing(inputdata_t &inputdata);
	void InputEnablePatrol(inputdata_t &inputdata);
	void InputDisablePatrol(inputdata_t &inputdata);
	void InputSetAlertRadius(inputdata_t &inputdata);
	void InputSetThreatRadius(inputdata_t &inputdata);

	int	GetSoundInterests(void) {
		return (BaseClass::GetSoundInterests() | SOUND_DANGER | SOUND_BULLET_IMPACT | SOUND_PHYSICS_DANGER | SOUND_PLAYER);
	};
	bool AllowedToIgnite(void) { return true; }
	bool AllowedToIgniteByFlare(void) { return true; }
	bool ShouldPatrol(void) { return m_shouldpatrol_boolean;	}
	
	bool FValidateHintType(CAI_Hint *pHint)
	{
		switch (pHint->HintType())
		{
		case HINT_TACTICAL_ENEMY_DISADVANTAGED:
		{
			return true;
			break;
		}

		default:
			return false;
			break;
		}

		return FALSE;
	}

	DEFINE_CUSTOM_AI;

	enum
	{
		TASK_HOUND_THREAT_DISPLAY = LAST_SHARED_TASK,
		TASK_HOUND_GET_PATH_TO_VANTAGE_POINT,
		TASK_HOUND_FIND_COVER_FROM_SAVEPOSITION,
	};

	enum
	{
		COND_HOUND_TOO_SOON_TO_ATTACK = LAST_SHARED_CONDITION,
		COND_HOUND_CAN_INITIATE_ATTACK,
		COND_HOUND_PRE_INITIATE_ATTACK,
		COND_HOUND_TOO_FAR_FOR_ATTACK,
	};

	enum
	{
		SCHED_HOUND_AGITATED = LAST_SHARED_SCHEDULE,
		SCHED_HOUND_RANGEATTACK,
		SCHED_HOUND_PRANCE,
		SCHED_HOUND_RUN_RANDOM,
		SCHED_HOUND_RUN_DANGER,
		SCHED_HOUND_TAKE_COVER_FROM_SAVEPOSITION,
		SCHED_HOUND_FLANK_ENEMY, // non-leader squadmates do this to distrack the target, leader does it while waiting for cooldown
		SCHED_HOUND_GO_FOR_ATTACK, // leader does this when ready to attack
		SCHED_HOUND_CHASE_ENEMY, // custom version of the schedule to fix running bug...
	};
	
private:
	int m_iShockWaveTexture;
	CAI_HoundeyeFollowBehavior	m_FollowBehavior;
	CAI_AssaultBehavior m_AssaultBehavior;
	HoundeyeMoveState_e	m_MoveState;
	float m_flSuppressFollowTime;		// Amount of time to suppress our follow time
	float m_flObeyFollowTime;			// A range of time the houndeye must be obedient
	EHANDLE m_hFollowTarget;
	CSoundPatch *m_sndChargeSound;
	float m_flDangerSoundTime;
	float m_nextidlesound_time;
//	float m_flUnragdollTime;
protected:
	float m_alertradius_float;
	float m_threatdisplayradius_float;
	COutputEvent m_OnPlayerUse;
	COutputEvent m_OnPlayerInAlertRadius;	//Player has entered within m_flAlertRadius sphere
//	CSimpleSimTimer m_EyeSwitchTimer; // Controls how often we switch eye expressions
public:
	int m_iChargeValue; // public so other NPCs (friend classes) can potentially extract and detect this value
	bool m_shouldpatrol_boolean;
};

// houndeye does 20 points of damage spread over a sphere 384 units in diameter, and each additional 
// squad member increases the BASE damage by 110%, per the spec.
#define HOUNDEYE_MAX_CHARGE				100.0f
#define HOUNDEYE_TOP_MASS				300.0f

#define SF_HOUNDEYE_BARKS_DURING_RUN  1<<18
#define SF_HOUNDEYE_SLOW_RUNNING 1<<19

ConVar sk_houndeye_dmg_blast("sk_houndeye_dmg_blast", "40");
ConVar sk_houndeye_max_blastwave_radius("sk_houndeye_max_blastwave_radius", "400");
ConVar sk_houndeye_min_charge_amount("sk_houndeye_min_charge_amount", "40", 0, "When a houndeye begins charging his sonic attack,\nthey start with this amount pre-charged,\nand when they reach 100 they're ready to boom.");
ConVar npc_houndeye_max_chargesnd_pitch("npc_houndeye_max_chargesnd_pitch", "250");
ConVar npc_houndeye_debug("npc_houndeye_debug", "0");
//ConVar npc_houndeye_fadedist("npc_houndeye_fadedist", "2500");

int AE_HOUNDEYE_BARK;
int AE_HOUNDEYE_FOOTSTEP;
int AE_HOUNDEYE_CHARGE_START;
int AE_HOUNDEYE_CHARGE_RELEASE;
int AE_HOUNDEYE_RUN_BARK;
int AE_HOUNDEYE_RUN_DUST;

// Constructor
CNPC_Houndeye::CNPC_Houndeye()
{
	m_hNPCFileInfo = LookupNPCInfoScript("npc_houndeye");
	// houndeye follow
	m_flObeyFollowTime = 0.0f;
	m_hFollowTarget = NULL;
	m_nextidlesound_time = CURTIME;
	m_alertradius_float = 0;
	m_threatdisplayradius_float = 0;
	//
}

CNPC_Houndeye::~CNPC_Houndeye()
{
	if (m_sndChargeSound != NULL)
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundDestroy(m_sndChargeSound);
		m_sndChargeSound = NULL;
	}
}

Class_T	CNPC_Houndeye::Classify(void)
{
	return CLASS_HOUNDEYE;
}

// Generic spawn-related
void CNPC_Houndeye::Precache()
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	m_iShockWaveTexture = PrecacheModel("custom_uni/shockwave_refract.vmt");
	PrecacheScriptSound("NPC_Houndeye.Alert");
	PrecacheScriptSound("NPC_Houndeye.Anger1");
	PrecacheScriptSound("NPC_Houndeye.Anger2");
	PrecacheScriptSound("NPC_Houndeye.Fear");
	PrecacheScriptSound("NPC_Houndeye.Die");
	PrecacheScriptSound("NPC_Houndeye.Idle");
	PrecacheScriptSound("NPC_Houndeye.Footstep");
	PrecacheScriptSound("NPC_Houndeye.Hunt");
	PrecacheScriptSound("NPC_Houndeye.Pain");
	PrecacheScriptSound("NPC_Houndeye.Retreat");
	PrecacheScriptSound("NPC_Houndeye.Sonic");
	PrecacheScriptSound("NPC_Houndeye.WarmUp");
	PrecacheScriptSound("NPC_Houndeye.Warn");
//	PrecacheInstancedScene("scenes/di_npc/houndeye_eyemovements.vcd");
	PrecacheScriptSound("NPC_Houndeye.Charge");
	BaseClass::Precache();
}

void CNPC_Houndeye::Spawn()
{
	Precache();
	SetModel(STRING(GetModelName()));
	BaseClass::Spawn();
	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

	SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
	SetHullSizeNormal();
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MoveType_t(GetNPCScriptData().NPC_Movement_MoveType_int));

	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_Climb_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_CLIMB);
	if ((GetNPCScriptData().NPC_Capable_Doors_boolean) == true) CapabilitiesAdd(bits_CAP_DOORS_GROUP);

	CapabilitiesAdd(bits_CAP_LIVE_RAGDOLL);

	//	m_bShouldPatrol = GetNPCScriptData().bShouldPatrolAfterSpawn;

	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));
			
	ClearEffects();
	m_iChargeValue = 0;
	
	m_NPCState = NPC_STATE_NONE;
	m_flNextAttack = CURTIME;
	m_flDangerSoundTime = CURTIME;
//	m_flUnragdollTime = 0;

	RegisterThinkContext("HoundeyeSonicChargeContext");

	NPCInit();

	SetUse(&CNPC_Houndeye::Use);
	
//	m_iszIdleExpression = MAKE_STRING("scenes/di_npc/houndeye_eyemovements.vcd");
//	m_iszAlertExpression = MAKE_STRING("scenes/di_npc/houndeye_eyemovements.vcd");
//	m_iszCombatExpression = MAKE_STRING("scenes/di_npc/houndeye_eyemovements.vcd");
	
	if (m_nSkin == -1)
	{
		m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);
	}
}

// Thinks
void CNPC_Houndeye::NPCThink()
{
	BaseClass::NPCThink();
//	if (CURTIME > m_flUnragdollTime + 0.1f)
//	{
//		if (GetRenderMode() == kRenderNone)
//		{
//			SetRenderMode(kRenderNormal);
//			RemoveEffects(EF_NOSHADOW);
//		}
//	}

	// update eye expression
	// If the eyes are controlled by a script, do nothing.
	if (GetState() == NPC_STATE_SCRIPT)
		return;

//	if (m_EyeSwitchTimer.Expired())
//	{
		//	RemoveActorFromScriptedScenes(this, false);

		//	if( m_NPCState == NPC_STATE_IDLE || m_NPCState == NPC_STATE_ALERT )
		//	{
		//	}

		//	m_EyeSwitchTimer.Set(random->RandomFloat(1.0f, 3.0f));
//	}

	if (IsCurSchedule(SCHED_HOUND_FLANK_ENEMY)) NDebugOverlay::Sphere(EyePosition(), 20, 200, 200, 100, true, 0.1f);

	if (m_alertradius_float > 0)
	{
		CBaseEntity *pEntity = NULL;
		if (UTIL_FindClientInPVS(edict()))
		{
			// iterate on all entities in the vicinity.
			while ((pEntity = gEntList.FindEntityInSphere(pEntity, GetAbsOrigin(), m_alertradius_float)) != NULL)
			{
				if (pEntity->IsPlayer() && (pEntity->GetFlags() & FL_NOTARGET) == false)
				{
					m_OnPlayerInAlertRadius.FireOutput(this, this);
				}
			}
		}

		if (npc_houndeye_debug.GetBool())
		{
			NDebugOverlay::Sphere(WorldSpaceCenter(), m_alertradius_float, 255, 175, 25, true, 0.1f);
			NDebugOverlay::Sphere(WorldSpaceCenter(), m_threatdisplayradius_float, 255, 25, 175, true, 0.1f);
		}
	}
}

void CNPC_Houndeye::ChargeUpThink()
{
	m_iChargeValue += 2;
	if (m_iChargeValue > 100) m_iChargeValue = 100;

	//	if (m_iChargeValue >= 100)
	//	{
	SetPlaybackRate(1.37f); // get us to end of animation quickly
							//	}
							//	else
							//	{
							//		SetPlaybackRate(1.2f); // generally speed up this attack...
							//	}
							// Don't get interrupted by danger sounds
	m_flDangerSoundTime = CURTIME + 1.0f;

	SetNextThink(CURTIME + 0.15f, "HoundeyeSonicChargeContext");
}

void CNPC_Houndeye::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	m_OnPlayerUse.FireOutput(pActivator, pCaller);
}

// Damage handling
float CNPC_Houndeye::GetHitgroupDamageMultiplier(int iHitGroup, const CTakeDamageInfo &info)
{
	switch (iHitGroup)
	{
	case HITGROUP_HEAD:
	{
		if ((info.GetDamageType() == DMG_CLUB)
			|| (info.GetDamageType() == DMG_SLASH)
			|| (info.GetDamageType() == DMG_CRUSH))
			return 2.0f;
		else
			return 1.25f;
	}
	default:
		return 1.0f;
	}

	return BaseClass::GetHitgroupDamageMultiplier(iHitGroup, info);
}

void CNPC_Houndeye::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr)
{
	CTakeDamageInfo	newInfo = info;

	BaseClass::TraceAttack(newInfo, vecDir, ptr);
}

// NPC sounds
void CNPC_Houndeye::IdleSound(void)
{
	if (CURTIME < m_nextidlesound_time) return;
	if (CombatConfidence() < 25 && (m_NPCState == NPC_STATE_ALERT || m_NPCState == NPC_STATE_COMBAT))
	{
		EmitSound("NPC_Houndeye.Fear");
	}
	else
	{
		EmitSound("NPC_HoundEye.Idle");
	}
	m_nextidlesound_time = CURTIME + RandomFloat(4, 10);
}
void CNPC_Houndeye::AlertSound(void)
{
	//	if (m_pSquad && !m_pSquad->IsLeader(this))
	//		return; // only leader makes ALERT sound.
	EmitSound("NPC_HoundEye.Alert");
}
void CNPC_Houndeye::WarmUpSound(void) { EmitSound("NPC_HoundEye.WarmUp"); }
void CNPC_Houndeye::WarnSound(void) { EmitSound("NPC_HoundEye.Warn"); }
void CNPC_Houndeye::HuntSound(void) { EmitSound("NPC_HoundEye.Hunt"); }
void CNPC_Houndeye::PainSound(const CTakeDamageInfo &info) { EmitSound("NPC_HoundEye.Pain"); }
void CNPC_Houndeye::DeathSound(const CTakeDamageInfo &info) { EmitSound("NPC_HoundEye.Die"); }
void CNPC_Houndeye::FootstepSound(void) { EmitSound("NPC_HoundEye.FootStep"); }

// Animation
void CNPC_Houndeye::HandleAnimEvent(animevent_t *pEvent)
{
	if (pEvent->event == AE_HOUNDEYE_BARK)
	{
		SonicBark(); // a weaker bark that deals minimum damage, but requires no cooldown
		HuntSound();
		return;
	}

	if (pEvent->event == AE_HOUNDEYE_RUN_BARK)
	{
		if (HasSpawnFlags(SF_HOUNDEYE_BARKS_DURING_RUN))
		{
			if (RandomInt(0, 1) > 0)
				HuntSound();
		}
		return;
	}

	if (pEvent->event == AE_HOUNDEYE_RUN_DUST)
	{
		bool InPVS = ((UTIL_FindClientInPVS(edict()) != NULL)
			|| (UTIL_ClientPVSIsExpanded()
				&& UTIL_FindClientInVisibilityPVS(edict())));

		CBasePlayer *pPlayerEnt = AI_GetSinglePlayer();
		float flDistToPlayerSqr = (GetAbsOrigin() - pPlayerEnt->GetAbsOrigin()).LengthSqr();

		if (flDistToPlayerSqr > 600 * 600 || !InPVS)
		{
			return;
		}
		else
		{
			trace_t tr;
			AI_TraceLine(GetLocalOrigin(), GetLocalOrigin() - Vector(0, 0, 24), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
			float yaw = RandomInt(0, 0);

			Vector dir = UTIL_YawToVector(yaw + 1 * 180) * 10;
			VectorNormalize(dir);
			dir.z = 1.0;
			VectorNormalize(dir);
			g_pEffects->Dust(tr.endpos, dir, 24, 50);
			g_pEffects->Dust(tr.endpos, dir, 16, 50);
		}
		return;
	}

	if (pEvent->event == AE_HOUNDEYE_FOOTSTEP)
	{
		FootstepSound();
		return;
	}

	if (pEvent->event == AE_HOUNDEYE_CHARGE_START)
	{
		//	SetPlaybackRate(1.0f); // reset anim speed
		m_iChargeValue = sk_houndeye_min_charge_amount.GetInt(); // start at minimum of 40% charge power

																 //Start charging sound
		CPASAttenuationFilter filter(this);
		m_sndChargeSound = (CSoundEnvelopeController::GetController()).SoundCreate(filter, entindex(),
			CHAN_STATIC, "NPC_Houndeye.Charge", ATTN_NORM);
		assert(m_sndChargeSound != NULL);
		if (m_sndChargeSound != NULL)
		{
			CSoundEnvelopeController::GetController().Play(m_sndChargeSound, 1.0f, 50);
			CSoundEnvelopeController::GetController().SoundChangePitch(m_sndChargeSound,
				npc_houndeye_max_chargesnd_pitch.GetFloat(), 1.5f);
		}

		// tell nearby creatures to get out of the blast zone. Primarily meant for our houndeye buddies
		CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin(), sk_houndeye_max_blastwave_radius.GetFloat() * 2, 0.2, this);

		SetContextThink(&CNPC_Houndeye::ChargeUpThink, CURTIME + 0.1f, "HoundeyeSonicChargeContext");
		return;
	}

	if (pEvent->event == AE_HOUNDEYE_CHARGE_RELEASE)
	{
		SetContextThink(NULL, 0, "HoundeyeSonicChargeContext");
		m_iChargeValue = 100;
		SonicAttack();
		SetPlaybackRate(1.0f); // reset anim speed
		return;
	}

	BaseClass::HandleAnimEvent(pEvent);
}

/*
extern ConVar di_barnacle_rescue_live_ragdoll;
bool CNPC_Houndeye::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
// prototyping barnacle rescue
if (interactionType == g_interactionBarnacleVictimGrab)
{
ClearSchedule("grabbed from barnacle");
return true;
}

// prototyping barnacle rescue
else if (interactionType == g_interactionBarnacleVictimReleased)
{
RemoveEFlags(EFL_IS_BEING_LIFTED_BY_BARNACLE); // these are largely failsafes, because the barnacle is supposed to do it too.
SetGravity(GetGravity());
//	AddFlag(FL_ONGROUND);
SetMoveType(MOVETYPE_STEP);

if (GetHealth() <= 0) // happens if we got shot on the way up, but not killed by the barnacle
{
ClearSchedule("released from barnacle, but dead");
SetSchedule(SCHED_DIE_RAGDOLL);
return true;
}

if (di_barnacle_rescue_live_ragdoll.GetBool())
{
if (!IsCurSchedule(SCHED_DI_FALL_IN_RAGDOLL_PUSH))
{
ClearSchedule("released from barnacle");
m_flExtraRagdollTime = 5;
SetSchedule(SCHED_DI_FALL_IN_RAGDOLL_PUSH);
}
}
else
{
ClearSchedule("released from barnacle");
SetSchedule(SCHED_HOUND_RUN_RANDOM);
}
return true;
}

return BaseClass::HandleInteraction(interactionType, data, sourceEnt);
}
*/

Activity CNPC_Houndeye::NPC_TranslateActivity(Activity activity)
{
	if (HasSpawnFlags(SF_HOUNDEYE_SLOW_RUNNING))
	{
		if (activity == ACT_RUN)
		{
			return ACT_RUN_HURT;
		}
	}

	activity = BaseClass::NPC_TranslateActivity(activity);

	return activity;
}

void CNPC_Houndeye::OnChangeActivity(Activity eNewActivity)
{
//	m_EyeSwitchTimer.Force();

	BaseClass::OnChangeActivity(eNewActivity);
}

// Conditions
bool CNPC_Houndeye::IsValidEnemy(CBaseEntity *pEnemy)
{
	//See if houndeyes are friendly to the player in this map
	//	if (IsAllied() && pEnemy->IsPlayer())
	//		return false;

	if (pEnemy->IsWorld()) // wut?
		return false;

	// If we're following an entity we limit our attack distances
	if (m_FollowBehavior.GetFollowTarget() != NULL)
	{
		float enemyDist = (pEnemy->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();

		if (m_flObeyFollowTime > CURTIME)
		{
			// Unless we're right next to the enemy, follow our target
			if (enemyDist > (128 * 128))
				return false;
		}
		else
		{
			// Otherwise don't follow if the target is far 
			if (enemyDist > (2000 * 2000))
				return false;
		}
	}

	return BaseClass::IsValidEnemy(pEnemy);
}

void CNPC_Houndeye::GatherConditions(void)
{
	BaseClass::GatherConditions();

	ClearCondition(COND_PROVOKED);
	ClearCustomInterruptCondition(COND_PROVOKED);

	if (m_threatdisplayradius_float > 0 && UTIL_FindClientInPVS(edict()))
	{
		CBasePlayer *player = AI_GetSinglePlayer();		
		if (player && GetAbsOrigin().DistToSqr(player->GetAbsOrigin()) <= m_threatdisplayradius_float * m_threatdisplayradius_float)
		{
			SetCustomInterruptCondition(COND_PROVOKED);
			SetCondition(COND_PROVOKED);
		}
	}

	if (m_flNextAttack < CURTIME)
	{
		SetCondition(COND_HOUND_CAN_INITIATE_ATTACK);
	}
	else
	{
		ClearCondition(COND_HOUND_CAN_INITIATE_ATTACK);
	}

	if (HasCondition(COND_HEAR_DANGER) && m_flDangerSoundTime < CURTIME + 1)
	{
		//	ClearCondition(COND_HEAR_DANGER);
	}

	if (HasCondition(COND_HEAR_COMBAT) && m_flDangerSoundTime < CURTIME + 1)
	{
		//	ClearCondition(COND_HEAR_COMBAT);
	}
}

void CNPC_Houndeye::ClearAttackConditions()
{
	bool fCanRangeAttack = HasCondition(COND_CAN_RANGE_ATTACK2);
	bool fCanRangeAttack2 = HasCondition(COND_CAN_RANGE_ATTACK2);

	// Call the base class.
	BaseClass::ClearAttackConditions();

	if (fCanRangeAttack)
	{
		// We don't allow the base class to clear this condition because we
		// don't sense for it every frame.
		SetCondition(COND_CAN_RANGE_ATTACK1);
	}

	if (fCanRangeAttack2)
	{
		// We don't allow the base class to clear this condition because we
		// don't sense for it every frame.
		SetCondition(COND_CAN_RANGE_ATTACK2);
	}
}

int CNPC_Houndeye::RangeAttack2Conditions(float flDot, float flDist)
{
	// If I'm really close to my enemy allow me to attack if 
	// I'm facing regardless of next attack time
	//	if (flDist < 100 && flDot >= 0.3)
	//	{
	//		return COND_CAN_RANGE_ATTACK1;
	//	}
	if (CURTIME < m_flNextAttack)
	{
		return(COND_HOUND_TOO_SOON_TO_ATTACK);
	}
	if (flDist >(sk_houndeye_max_blastwave_radius.GetFloat() * 0.5))
	{
		return COND_HOUND_TOO_FAR_FOR_ATTACK;
	}
	if (flDot < 0.3)
	{
		return COND_NOT_FACING_ATTACK;
	}
	return COND_CAN_RANGE_ATTACK1;
}

// Schedules, states, tasks
bool CNPC_Houndeye::CreateBehaviors(void)
{
	AddBehavior(&m_FollowBehavior);
	AddBehavior(&m_AssaultBehavior);
	return BaseClass::CreateBehaviors();
	//	return true;
}

bool CNPC_Houndeye::ShouldBehaviorSelectSchedule(CAI_BehaviorBase *pBehavior)
{
	return BaseClass::ShouldBehaviorSelectSchedule(pBehavior);
}

void CNPC_Houndeye::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();

	if (m_iChargeValue > 100) m_iChargeValue = 100;

	// if the hound is mad and is running, make hunt noises.
	if (m_NPCState == NPC_STATE_COMBAT && GetActivity() == ACT_RUN && random->RandomFloat(0, 1) < 0.2)
	{
		WarnSound();
	}

	if (CombatConfidence() < 25) IdleSound(); // fear sounds
}

int CNPC_Houndeye::ChooseMoveSchedule(void)
{
	// See if we need to invalidate our fight goal
	if (ShouldResumeFollow())
	{
		// Set us back to following
		SetMoveState(HOUNDEYE_MOVE_FOLLOW);
	}

	// Figure out our move state
	switch (m_MoveState)
	{
	case HOUNDEYE_MOVE_FREE:
		return SCHED_NONE;	// Let the base class handle us
		break;

		// Following a goal
	case HOUNDEYE_MOVE_FOLLOW:
	{
		if (m_FollowBehavior.CanSelectSchedule())
		{
			// See if we should burrow away if our target it too far off
			if (ShouldAbandonFollow())
				return SCHED_RUN_RANDOM; // todo: see if there isn't something better

			DeferSchedulingToBehavior(&m_FollowBehavior);
			return BaseClass::SelectSchedule();
		}
	}
	break;
	}

	return SCHED_NONE;
}

int CNPC_Houndeye::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_IDLE_STAND: return ShouldPatrol() ? SCHED_IDLE_WANDER : SCHED_IDLE_STAND; break;

	case SCHED_RANGE_ATTACK1: return SCHED_HOUND_RANGEATTACK; break;

	case SCHED_FLEE_FROM_BEST_SOUND: return SCHED_HOUND_RUN_DANGER; break;

	case SCHED_CHASE_ENEMY: return SCHED_HOUND_CHASE_ENEMY; break;

	default: return BaseClass::TranslateSchedule(scheduleType); break;
	}
}

int CNPC_Houndeye::SelectSchedule(void)
{
	// Make houndeyes flee from grenades and other danger sounds
	if (m_flDangerSoundTime <= CURTIME)
	{
		if (HasCondition(COND_HEAR_DANGER) || HasCondition(COND_HEAR_COMBAT || HasCondition(COND_HEAR_BULLET_IMPACT)))
		{
			m_flDangerSoundTime = CURTIME + 5.0f;
			return SCHED_HOUND_RUN_DANGER;
		}
	}

	if (m_flDangerSoundTime <= CURTIME)
	{
		if ( HasCondition(COND_HEAR_BULLET_IMPACT))
		{
			m_flDangerSoundTime = CURTIME + 2.0f; // make reacting to bullets happen more often
			return SCHED_HOUND_RUN_DANGER;
		}
	}

	if (m_AssaultBehavior.CanSelectSchedule())
	{
		DeferSchedulingToBehavior(&m_AssaultBehavior);
		return BaseClass::SelectSchedule();
	}

	if (m_FollowBehavior.CanSelectSchedule())
	{
		DeferSchedulingToBehavior(&m_FollowBehavior);
		return BaseClass::SelectSchedule();
	}

	switch (m_NPCState)
	{
	case NPC_STATE_IDLE:
	case NPC_STATE_ALERT:
		{
			if (HasCondition(COND_PROVOKED))
			{
				AlertSound();
				return SCHED_HOUND_AGITATED;
			}

			return BaseClass::SelectSchedule();
		}
	break;
	case NPC_STATE_COMBAT:
	{
		// too wounded to fight
		if (GetEnemy() && CombatConfidence() < 25 && (!GetSquad() || GetSquad()->NumMembers() <= 2))
		{
			return SCHED_TAKE_COVER_FROM_ENEMY;
		}

		// dead enemy
		if (HasCondition(COND_ENEMY_DEAD))
		{
			// call base class, all code to handle dead enemies is centralized there.
			return BaseClass::SelectSchedule();
		}

		if (HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE))
		{
			//	SonicAttack();
			return SCHED_TAKE_COVER_FROM_ENEMY; // todo: replace with custom hop animation, not running as far as this one
		}

		if (HasCondition(COND_HOUND_TOO_SOON_TO_ATTACK))
		{
			if (GetSquad() && (/*GetSquad()->GetLeader() == this*/ OccupyStrategySlotRange(SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2) || GetSquad()->NumMembers() <= 1))
				return SCHED_HOUND_RUN_RANDOM;
			else
				return SCHED_HOUND_PRANCE;
		}

		if (HasCondition(COND_HOUND_TOO_FAR_FOR_ATTACK))
		{
			//	if (GetSquad() && GetSquad()->IsLeader(this))//OccupyStrategySlot(SQUAD_SLOT_EXCLUSIVE_HANDSIGN))
			//	{
			//		return SCHED_HOUND_FLANK_ENEMY; // todo: write a good schedule for leader flanking.
			//	}
			//	else
			{
				return SCHED_CHASE_ENEMY;
			}
		}

		if (HasCondition(COND_CAN_RANGE_ATTACK1))
		{
			if (GetSquad())
			{
				if ( /*GetSquad()->GetLeader() == this*/ OccupyStrategySlotRange(SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2) || GetSquad()->NumMembers() <= 1)
					return SCHED_HOUND_RANGEATTACK;
				else
				{
					if (RandomFloat(0, 1) > 0.5)
						return SCHED_HOUND_PRANCE;
					else
						return SCHED_RANGE_ATTACK2;
				}
			}
			else
			{
				if (RandomFloat(0, 1) > 0.5)
					return SCHED_HOUND_RANGEATTACK;
				else
					return SCHED_RANGE_ATTACK2;
			}
		}

		return BaseClass::SelectSchedule();
	}
	break;

	default:
	{
		//	int	moveSched = ChooseMoveSchedule();

		//	if (moveSched != SCHED_NONE)
		//		return moveSched;

		if (GetEnemy() == NULL)
		{
			if (HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE))
			{
				Vector vecEnemyLKP;

				// Retrieve a memory for the damage taken
				// Fill in where we're trying to look
				if (GetEnemies()->Find(AI_UNKNOWN_ENEMY))
				{
					vecEnemyLKP = GetEnemies()->LastKnownPosition(AI_UNKNOWN_ENEMY);
				}
				else
				{
					// Don't have an enemy, so face the direction the last attack came from (don't face north)
					vecEnemyLKP = WorldSpaceCenter() + (g_vecAttackDir * 128);
				}

				// If we're already facing the attack direction, then take cover from it
				if (FInViewCone(vecEnemyLKP))
				{
					// Save this position for our cover search
					m_vSavePosition = vecEnemyLKP;
					return SCHED_HOUND_TAKE_COVER_FROM_SAVEPOSITION; // todo: consider run_danger one?
				}

				// By default, we'll turn to face the attack
			}
			else // no damage, nothing going on
			{
				return SCHED_IDLE_STAND;
			}
		}
	}
	break;
	}

	return BaseClass::SelectSchedule();
}

void CNPC_Houndeye::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_HOUND_THREAT_DISPLAY:
	{
		SetIdealActivity(ACT_IDLE_AGITATED);
	}
	break;
	case TASK_HOUND_GET_PATH_TO_VANTAGE_POINT:
	{
		assert(GetEnemy() != NULL);
		if (GetEnemy() == NULL)
			break;

		Vector	goalPos;

		CHintCriteria	hintCriteria;
		// Find a disadvantage node near the player, but away from ourselves
		hintCriteria.SetHintType(HINT_TACTICAL_ENEMY_DISADVANTAGED);
		hintCriteria.AddExcludePosition(GetAbsOrigin(), pTask->flTaskData);
		hintCriteria.AddExcludePosition(GetEnemy()->GetAbsOrigin(), pTask->flTaskData);
		hintCriteria.SetFlag(bits_HINT_NODE_NOT_VISIBLE_TO_PLAYER | bits_HINT_NODE_NEAREST);

		CAI_Hint *pHint = CAI_HintManager::FindHint(GetEnemy()->GetAbsOrigin(), hintCriteria);

		if (pHint == NULL)
		{
			//	TaskFail("Unable to find vantage point!\n");
			//	break;
			// fallback case - use this origin instead
			goalPos = GetEnemy()->GetAbsOrigin();
		}
		else
		{
			pHint->GetPosition(this, &goalPos);
		}

		AI_NavGoal_t goal(goalPos);

		//Try to run directly there
		if (GetNavigator()->SetGoal(goal) == false)
		{
			TaskFail("Unable to find path to vantage point!\n");
			break;
		}

		TaskComplete();
		break;
	}
	case TASK_HOUND_FIND_COVER_FROM_SAVEPOSITION:
	{
		Vector coverPos;

		if (GetTacticalServices()->FindCoverPos(m_vSavePosition, EyePosition(), 0, CoverRadius(), &coverPos))
		{
			AI_NavGoal_t goal(coverPos, ACT_RUN, AIN_HULL_TOLERANCE);
			GetNavigator()->SetGoal(goal);

			m_flMoveWaitFinished = CURTIME + pTask->flTaskData;
		}
		else
		{
			// no coverwhatsoever.
			TaskFail(FAIL_NO_COVER);
		}
		break;
	}
	/*
	case TASK_WAIT_FOR_MOVEMENT:
	{
	if (GetNavigator()->GetGoalType() == GOALTYPE_NONE)
	{
	TaskComplete();
	GetNavigator()->ClearGoal();		// Clear residual state
	}
	else if (!GetNavigator()->IsGoalActive())
	{
	SetIdealActivity(GetStoppedActivity());
	}
	else
	{
	// Check validity of goal type
	//	ValidateNavGoal();
	Msg("goaltype is %i\n", (int)GetNavigator()->GetGoalType());
	TaskComplete();
	GetNavigator()->StopMoving();		// Stop moving
	}
	break;
	}
	*/
	default:
	{
		BaseClass::StartTask(pTask);
		break;
	}
	}
}

void CNPC_Houndeye::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_HOUND_THREAT_DISPLAY:
	{
		if (GetEnemy())
		{
			float idealYaw;
			idealYaw = UTIL_VecToYaw(GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
			GetMotor()->SetIdealYawAndUpdate(idealYaw);
		}

		if (IsSequenceFinished())
		{
			TaskComplete();
		}

		break;
	}
	case TASK_SPECIAL_ATTACK1:
	{
		float idealYaw;
		idealYaw = UTIL_VecToYaw(GetNavigator()->GetGoalPos());
		GetMotor()->SetIdealYawAndUpdate(idealYaw);
		/*
		float life;
		life = ((255 - GetCycle()) / (m_flPlaybackRate * m_flPlaybackRate));
		if (life < 0.1)
		{
		life = 0.1;
		}
		*/
		if (IsSequenceFinished())
		{
			SonicAttack();
			ClearCondition(COND_CAN_RANGE_ATTACK1);
			TaskComplete();
		}

		break;
	}
	/*
	case TASK_WAIT_FOR_MOVEMENT:
	{
	bool fTimeExpired = (pTask->flTaskData != 0 && pTask->flTaskData < CURTIME - GetTimeTaskStarted());

	if (fTimeExpired || GetNavigator()->GetGoalType() == GOALTYPE_NONE)
	{
	TaskComplete();
	GetNavigator()->StopMoving();		// Stop moving
	}
	else if (!GetNavigator()->IsGoalActive())
	{
	SetIdealActivity(GetStoppedActivity());
	}
	else
	{
	// Check validity of goal type
	//	ValidateNavGoal();
	Msg("goaltype is %i\n", (int)GetNavigator()->GetGoalType());
	TaskComplete();
	GetNavigator()->StopMoving();		// Stop moving
	}
	break;
	}
	*/
	default:
	{
		BaseClass::RunTask(pTask);
		break;
	}
	}
}

// Inputs
void CNPC_Houndeye::InputStartFollowingTarget(inputdata_t &inputdata)
{
	CBaseEntity *forcedFollow = gEntList.FindEntityByName(NULL,
		inputdata.value.String(),
		this,
		inputdata.pActivator,
		inputdata.pCaller);

	SetFollowTarget(forcedFollow);
}

void CNPC_Houndeye::InputStopFollowing(inputdata_t &inputdata)
{
	SetFollowTarget(NULL);
}

void CNPC_Houndeye::InputEnablePatrol(inputdata_t &inputdata)
{
	m_shouldpatrol_boolean = true;
}

void CNPC_Houndeye::InputDisablePatrol(inputdata_t &inputdata)
{
	m_shouldpatrol_boolean = false;
}

void CNPC_Houndeye::InputSetAlertRadius(inputdata_t &inputdata)
{
	m_alertradius_float = inputdata.value.Float();
}

void CNPC_Houndeye::InputSetThreatRadius(inputdata_t &inputdata)
{
	m_threatdisplayradius_float = inputdata.value.Float();
}

// Functions used anywhere above
void CNPC_Houndeye::SetFollowTarget(CBaseEntity *pTarget)
{
	m_FollowBehavior.SetFollowTarget(pTarget);
	m_hFollowTarget = pTarget;
	m_flObeyFollowTime = CURTIME + HOUNDEYE_OBEY_FOLLOW_TIME;
	m_FollowBehavior.SetParameters(AIF_MEDIUM);

	//	SetCondition(COND_ANTLION_RECEIVED_ORDERS);

	// Play an acknowledgement noise
	//	if (m_flNextAcknowledgeTime < CURTIME)
	//	{
	//		EmitSound("NPC_Antlion.Distracted");
	//		m_flNextAcknowledgeTime = CURTIME + 1.0f;
	//	}
}

bool CNPC_Houndeye::ShouldResumeFollow(void)
{
	//	if (IsAllied() == false)
	//		return false;

	if (m_MoveState == HOUNDEYE_MOVE_FOLLOW || m_hFollowTarget == NULL)
		return false;

	if (m_flSuppressFollowTime > CURTIME)
		return false;

	if (GetEnemy() != NULL)
	{
		m_flSuppressFollowTime = CURTIME + random->RandomInt(5, 10);
		return false;
	}

	//TODO: See if the follow target has wandered off too far from where we last followed them to

	return true;
}

bool CNPC_Houndeye::ShouldAbandonFollow(void)
{
	// Never give up if we can see the goal
	if (m_FollowBehavior.FollowTargetVisible())
		return false;

	// Never give up if we're too close
	float flDistance = UTIL_DistApprox2D(m_FollowBehavior.GetFollowTarget()->WorldSpaceCenter(), WorldSpaceCenter());

	if (flDistance < 1500)
		return false;

	if (flDistance > 1500 * 2.0f)
		return true;

	// If we've failed too many times, give up
	if (m_FollowBehavior.GetNumFailedFollowAttempts())
		return true;

	// If the target simply isn't reachable to us, give up
	if (m_FollowBehavior.TargetIsUnreachable())
		return true;

	return false;
}

// charged up strong sonic attack
void CNPC_Houndeye::SonicAttack(void)
{
	if (m_iChargeValue == 0) return;

	float		flAdjustedDamage;
	float		flDist;

	if (m_sndChargeSound != NULL)
		(CSoundEnvelopeController::GetController()).SoundFadeOut(m_sndChargeSound, 0.1f);

	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "NPC_HoundEye.Sonic");

	CBroadcastRecipientFilter filter2;
	te->BeamRingPoint(filter2, 0.0,
		GetAbsOrigin(),							//origin
		16,										//start radius
		sk_houndeye_max_blastwave_radius.GetFloat(),//end radius
		m_iShockWaveTexture,						//texture
		0,										//halo index
		0,										//start frame
		0,										//framerate
		0.2,									//life
		8,										//width
		16,										//spread
		0,										//amplitude
		255,									//r
		255,									//g
		255,									//b
		192,									//a
		0										//speed
	);

	CBroadcastRecipientFilter filter3;
	te->BeamRingPoint(filter3, 0.0,
		GetAbsOrigin(),									//origin
		16,												//start radius
		sk_houndeye_max_blastwave_radius.GetFloat() / 2,											//end radius
		m_iShockWaveTexture,								//texture
		0,												//halo index
		0,												//start frame
		0,												//framerate
		0.2,											//life
		8,												//width
		4,												//spread
		0,												//amplitude
		255,											//r
		255,											//g
		255,											//b
		192,											//a
		0												//speed
	);

	CEffectData	data;

	data.m_nEntIndex = entindex();
	data.m_vOrigin = GetLocalOrigin();
	data.m_flScale = 128.0f * m_flPlaybackRate;
	DispatchEffect("ThumperDust", data);
	UTIL_ScreenShake(GetLocalOrigin(), 10.0 * m_flPlaybackRate, m_flPlaybackRate, m_flPlaybackRate / 2, 512.0f * m_flPlaybackRate, SHAKE_START, false);

	CBaseEntity *pEntity = NULL;
	// iterate on all entities in the vicinity.
	while ((pEntity = gEntList.FindEntityInSphere(pEntity, GetAbsOrigin(), sk_houndeye_max_blastwave_radius.GetFloat())) != NULL)
	{
		if (pEntity->m_takedamage == DAMAGE_NO) continue;
		if (FClassnameIs(pEntity, GetClassname())) continue;

		/*CBaseAnimating*/ CBaseCombatCharacter *entityPointer = ToBaseCombatCharacter(pEntity);//dynamic_cast <CBaseAnimating*>(pEntity);

		if (entityPointer) entityPointer->Extinguish(); // blast off the fire from nearby things

														// houndeyes do FULL damage if the ent in question is visible. Half damage otherwise.
														// This means that you must get out of the houndeye's attack range entirely to avoid damage.
														// Calculate full damage first

		flAdjustedDamage = sk_houndeye_dmg_blast.GetFloat() * (m_iChargeValue / 100);

		if (!GetSquad() || (GetSquad() && GetSquad()->GetLeader() != this) || (GetSquad() && GetSquad()->NumMembers() <= 1))
			flAdjustedDamage *= 0.5; // solo houndeyes or non-leaders deal 50% damage

		flDist = (pEntity->WorldSpaceCenter() - GetAbsOrigin()).Length();

		flAdjustedDamage -= (flDist / sk_houndeye_max_blastwave_radius.GetFloat()) * flAdjustedDamage;

		// Handle entities that are within damage radius, but are blocked
		// by walls and such.
		if (!FVisible(pEntity))
		{
			// Players AND breakable objects take reduced damage.
			if (pEntity->IsPlayer()
				|| FClassnameIs(pEntity, "func_breakable")
				|| FClassnameIs(pEntity, "func_breakable_surf"))
			{
				flAdjustedDamage *= 0.5;
			}

			// Other entities take NO damage. This avoids 
			// agitating monsters and characters somewhere else
			// in the world. 
			else
			{
				flAdjustedDamage = 0;
			}
		}

		// Squids receive less damage in general. They're tough.
		if (FClassnameIs(pEntity, "npc_squid"))
		{
			flAdjustedDamage *= 0.5;
		}

		if (flAdjustedDamage <= 0) continue;

		CTakeDamageInfo info(this, this, flAdjustedDamage, DMG_SONIC | DMG_ALWAYSGIB);
		CalculateExplosiveDamageForce(&info, (pEntity->GetAbsOrigin() - GetAbsOrigin()), pEntity->GetAbsOrigin());

		pEntity->TakeDamage(info);

		if (pEntity->GetMoveType() == MOVETYPE_VPHYSICS || (pEntity->VPhysicsGetObject() && !pEntity->IsPlayer()))
		{
			IPhysicsObject *pPhysObject = pEntity->VPhysicsGetObject();

			if (!pPhysObject) continue;

			float flMass = pPhysObject->GetMass();

			// Increase the vertical lift of the force if
			// physics object is heavy
			Vector vecForce = info.GetDamageForce();
			vecForce.z *= (flMass <= HOUNDEYE_TOP_MASS) ? 2.0f : 1.0f;
			info.SetDamageForce(vecForce);

			pEntity->VPhysicsTakeDamage(info);
		}
	}

	// Ignore danger sounds for a couple of seconds after blasting
	m_flDangerSoundTime = CURTIME + 2.0f;

	SetContextThink(NULL, 0, "HoundeyeSonicChargeContext"); // cancel thinking in case we are still doing that.
	m_iChargeValue = 0;
	m_flNextAttack = CURTIME + 5.0f;
}

// a weaker bark that deals minimum damage, but requires no cooldown
void CNPC_Houndeye::SonicBark(void)
{
	if (!GetEnemy()) return;

	Vector vecStart, vecEnd, vecDir;
	vecStart = WorldSpaceCenter();
	vecEnd = GetEnemy()->WorldSpaceCenter();
	vecDir = vecEnd - vecStart;
	VectorNormalize(vecDir);

	CBroadcastRecipientFilter filter;
	//	te->EnergySplash(filter, 0.0f, &(vecStart + vecDir * 32), &vecDir, false);

	te->BeamRingPoint(filter, 0.0,
		vecStart + vecDir * 32,							//origin - in front of our center
		8,												//start radius
		sk_houndeye_max_blastwave_radius.GetFloat() / 2, //end radius
		m_iShockWaveTexture,								//texture
		0,												//halo index
		0,												//start frame
		0,												//framerate
		0.2,											//life
		8,												//width
		2,												//spread
		4,												//amplitude
		125,											//r
		25,												//g
		255,											//b
		192,											//a
		0												//speed
	);

	if (vecEnd.DistTo(vecStart) <= 200)
	{
		VectorNormalize(vecDir);
		//	if (PointWithinViewAngle(vecStart, vecEnd, vecDir, cos(37.5)))
		{
			CTakeDamageInfo kickInfo;
			kickInfo.SetDamage(1); // <5 for sonic means no tinnitus
			kickInfo.SetDamageType(DMG_SONIC);
			kickInfo.ScaleDamageForce(8.0f);
			kickInfo.SetAttacker(this);
			kickInfo.SetInflictor(this);
			CalculateMeleeDamageForce(&kickInfo, vecDir, WorldSpaceCenter(), 4.0f);

			if (GetEnemy()->IsPlayer())
			{
				CBasePlayer *pPlayer = ToBasePlayer(GetEnemy());
				if (pPlayer != NULL)
				{
					//Kick the player angles
					int punch = 16;
					if (RandomFloat(0, 1) > 0.5) punch ^= punch;

					pPlayer->ViewPunch(QAngle(2, punch, 0));

					VectorNormalize(vecDir);

					QAngle angles;
					VectorAngles(vecDir, angles);
					Vector forward, right;
					AngleVectors(angles, &forward, &right, NULL);

					//If not on ground, then don't make them fly!
					if (!(pPlayer->GetFlags() & FL_ONGROUND))
						forward.z = 0.0f;

					//Push the target back
					pPlayer->ApplyAbsVelocityImpulse(forward * 50.0f);

					// Force the player to drop anyting they were holding
					//pPlayer->ForceDropOfCarriedPhysObjects(); // this is a bit too bs

				}
			}
			else
			{
				kickInfo.SetDamage(5);
			}

			GetEnemy()->TakeDamage(kickInfo);
		}
	}
}

// Movement
float CNPC_Houndeye::MaxYawSpeed(void)
{
	int flYS;

	flYS = 45;

	switch (GetActivity())
	{
	case ACT_CROUCHIDLE://sleeping!
		flYS = 0;
	case ACT_IDLE:
		flYS = 45;
	case ACT_WALK:
		flYS = 30;
	case ACT_RUN:
		flYS = 15;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		flYS = 45;

	default:
		flYS = 60;
	}

	return flYS;
}

void CNPC_Houndeye::SetMoveState(HoundeyeMoveState_e state)
{
	m_MoveState = state;

	switch (m_MoveState)
	{
	case HOUNDEYE_MOVE_FOLLOW:
	{
		if (m_hFollowTarget)
		{
			m_FollowBehavior.SetFollowTarget(m_hFollowTarget);

			// Clear any previous state
			m_flSuppressFollowTime = 0;
		}
		break;
	}

	default:
		break;
	}
}

// Datadesc
BEGIN_DATADESC(CNPC_Houndeye)
DEFINE_FIELD(m_iShockWaveTexture, FIELD_INTEGER),
DEFINE_FIELD(m_iChargeValue, FIELD_INTEGER),
DEFINE_FIELD(m_flDangerSoundTime, FIELD_TIME),
//DEFINE_FIELD(m_nextidlesound_time, FIELD_FLOAT),
DEFINE_THINKFUNC(ChargeUpThink),
// houndeye follow
DEFINE_FIELD(m_hFollowTarget, FIELD_EHANDLE),
DEFINE_FIELD(m_flSuppressFollowTime, FIELD_FLOAT),
DEFINE_FIELD(m_MoveState, FIELD_INTEGER),
DEFINE_FIELD(m_flObeyFollowTime, FIELD_TIME),
DEFINE_KEYFIELD(m_shouldpatrol_boolean, FIELD_BOOLEAN, "ShouldPatrol"),
DEFINE_KEYFIELD(m_alertradius_float, FIELD_FLOAT, "radius"),
DEFINE_KEYFIELD(m_threatdisplayradius_float, FIELD_FLOAT, "threatradius"),
DEFINE_INPUTFUNC(FIELD_STRING, "StartFollowingTarget", InputStartFollowingTarget),
DEFINE_INPUTFUNC(FIELD_VOID, "StopFollowingAll", InputStopFollowing),
DEFINE_INPUTFUNC(FIELD_VOID, "EnablePatrol", InputEnablePatrol),
DEFINE_INPUTFUNC(FIELD_VOID, "DisablePatrol", InputDisablePatrol),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetAlertRadius", InputSetAlertRadius),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetThreatRadius", InputSetThreatRadius),
DEFINE_OUTPUT(m_OnPlayerUse, "OnPlayerUse"),
DEFINE_OUTPUT(m_OnPlayerInAlertRadius, "OnPlayerInAlertRadius"),
DEFINE_USEFUNC(Use),
//
//DEFINE_EMBEDDED(m_EyeSwitchTimer),
END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_houndeye, CNPC_Houndeye);
LINK_ENTITY_TO_CLASS(monster_houndeye, CNPC_Houndeye);

AI_BEGIN_CUSTOM_NPC(npc_houndeye, CNPC_Houndeye)

DECLARE_CONDITION(COND_HOUND_CAN_INITIATE_ATTACK)
DECLARE_CONDITION(COND_HOUND_PRE_INITIATE_ATTACK)
DECLARE_CONDITION(COND_HOUND_TOO_SOON_TO_ATTACK)
DECLARE_CONDITION(COND_HOUND_TOO_FAR_FOR_ATTACK)
DECLARE_TASK(TASK_HOUND_THREAT_DISPLAY)
DECLARE_TASK(TASK_HOUND_GET_PATH_TO_VANTAGE_POINT)
DECLARE_TASK(TASK_HOUND_FIND_COVER_FROM_SAVEPOSITION)
DECLARE_ANIMEVENT(AE_HOUNDEYE_BARK)
DECLARE_ANIMEVENT(AE_HOUNDEYE_CHARGE_START)
DECLARE_ANIMEVENT(AE_HOUNDEYE_CHARGE_RELEASE)
DECLARE_ANIMEVENT(AE_HOUNDEYE_FOOTSTEP)
DECLARE_ANIMEVENT(AE_HOUNDEYE_RUN_BARK)
DECLARE_ANIMEVENT(AE_HOUNDEYE_RUN_DUST)

DEFINE_SCHEDULE
(
	SCHED_HOUND_AGITATED,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_FACE_PLAYER			0"
	"		TASK_HOUND_THREAT_DISPLAY	0"
	"		TASK_PLAY_ACT_IDLE			0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
)

DEFINE_SCHEDULE
(
	SCHED_HOUND_RANGEATTACK,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_FACE_IDEAL				0"
	"		TASK_RANGE_ATTACK1			0"
	""
	"	Interrupts"
)

DEFINE_SCHEDULE
(
	SCHED_HOUND_RUN_RANDOM,
	"	Tasks"
	"		TASK_SET_ROUTE_SEARCH_TIME		1"	// Spend 1 seconds trying to build a path if stuck
	"		TASK_GET_PATH_TO_RANDOM_NODE	256"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"		COND_HOUND_CAN_INITIATE_ATTACK"
	"		COND_HOUND_PRE_INITIATE_ATTACK"
)

DEFINE_SCHEDULE
(
	// This is the modified version of ai_basenpc's "flee_from_best_sound",
	// omitting the "face_saveposition" task, because it caused the houndeye
	// to get stuck in turning animation once it's done running away.
	// Also, with "task_stop_moving", they wouldn't return to idle
	// wandering behaviour, essentially locking into their flee position
	// and playing idle anim there. (until bothered again by either enemy 
	// or new danger sound)
	SCHED_HOUND_RUN_DANGER,
	"	Tasks"
	"		 TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_HOUND_RUN_RANDOM"
	"		 TASK_STORE_BESTSOUND_REACTORIGIN_IN_SAVEPOSITION	0"
	"		 TASK_GET_PATH_AWAY_FROM_BEST_SOUND	200"
	"		 TASK_RUN_PATH_FLEE					100"
	"		 TASK_STOP_MOVING					0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
)

DEFINE_SCHEDULE
(
	SCHED_HOUND_PRANCE,

	"	Tasks"
	"		TASK_SET_TOLERANCE_DISTANCE 64"
	"		TASK_SET_ROUTE_SEARCH_TIME	2"
	"		TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS		64"
	"		TASK_RUN_PATH				0"
	"		TASK_STOP_MOVING			0"
	"		TASK_FACE_IDEAL				0"
	""
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_ENEMY_DEAD"
)

DEFINE_SCHEDULE
(
	SCHED_HOUND_FLANK_ENEMY,

	"	Tasks"
	"		TASK_SET_TOLERANCE_DISTANCE 64"
	"		TASK_SET_ROUTE_SEARCH_TIME	2"
//	"		TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS 64"
	"		TASK_HOUND_GET_PATH_TO_VANTAGE_POINT 128" // run somewhere behind the enemy.
	"		TASK_RUN_PATH				0"
	"		TASK_FACE_IDEAL				0"
	""
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_ENEMY_DEAD"
	"		COND_HOUND_CAN_INITIATE_ATTACK"
)

DEFINE_SCHEDULE
(
	SCHED_HOUND_GO_FOR_ATTACK,

	"	Tasks"
	"		TASK_SET_TOLERANCE_DISTANCE 64"
	"		TASK_SET_ROUTE_SEARCH_TIME	2"
	"		TASK_GET_FLANK_RADIUS_PATH_TO_ENEMY_LOS		128"
	"		TASK_RUN_PATH				0"
	"		TASK_WAIT_FOR_MOVEMENT		0"
	"		TASK_FACE_IDEAL				0"
	""
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_ENEMY_DEAD"
	"		COND_HOUND_CAN_INITIATE_ATTACK"
)

DEFINE_SCHEDULE
(
	SCHED_HOUND_CHASE_ENEMY,

	"	Tasks"
	"		TASK_GET_CHASE_PATH_TO_ENEMY	300"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_STOP_MOVING				0"
	"		TASK_FACE_ENEMY					0"
	"	"
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_ENEMY_UNREACHABLE"
	"		COND_HOUND_CAN_INITIATE_ATTACK"
	"		COND_HOUND_PRE_INITIATE_ATTACK"
	"		COND_TOO_CLOSE_TO_ATTACK"
	"		COND_TASK_FAILED"
	"		COND_LOST_ENEMY"
	"		COND_HEAR_DANGER"
)

DEFINE_SCHEDULE
(
	SCHED_HOUND_TAKE_COVER_FROM_SAVEPOSITION,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE						SCHEDULE:SCHED_FAIL_TAKE_COVER"
	"		TASK_HOUND_FIND_COVER_FROM_SAVEPOSITION		0"
	"		TASK_RUN_PATH								0"
	"		TASK_WAIT_FOR_MOVEMENT						0"
	"		TASK_STOP_MOVING							0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_NEW_ENEMY"
)
AI_END_CUSTOM_NPC()
#endif // HOUNDEYE

//====================================
// Alien Polyp (Dark Interval)
//====================================
#if defined (POLYP)
enum PolypState_t
{
	POLYP_WHOLE,
	POLYP_BROKEN,
};

class PolypNugget : public CItem
{
	DECLARE_CLASS(PolypNugget, CItem);
public:
	virtual void Spawn(void);
	virtual void Precache(void);
	virtual bool VPhysicsIsFlesh(void);
	bool MyTouch(CBasePlayer *pPlayer);
};

void PolypNugget::Precache(void)
{
	PrecacheModel("models/grub_nugget_medium.mdl");
	PrecacheScriptSound("GrubNugget.Touch");
	PrecacheScriptSound("NPC_Antlion_Grub.Explode");
}

void PolypNugget::Spawn(void)
{
	Precache();
	SetModel("models/grub_nugget_medium.mdl");
	// We're self-illuminating, so we don't take or give shadows
	AddEffects(EF_NOSHADOW | EF_NORECEIVESHADOW);
	BaseClass::Spawn();
	m_takedamage = DAMAGE_NO;
}

bool PolypNugget::VPhysicsIsFlesh(void)
{
	return false;
}

bool PolypNugget::MyTouch(CBasePlayer *pPlayer)
{
	// Attempt to give the player health
	if (pPlayer->TakeHealth(5, DMG_GENERIC) == 0)
		return false;
	CSingleUserRecipientFilter user(pPlayer);
	user.MakeReliable();
	UserMessageBegin(user, "ItemPickup");
	WRITE_STRING(GetClassname());
	MessageEnd();
	CPASAttenuationFilter filter(pPlayer, "GrubNugget.Touch");
	EmitSound(filter, pPlayer->entindex(), "GrubNugget.Touch");
	UTIL_Remove(this);
	return true;
}

LINK_ENTITY_TO_CLASS(item_polypnugget, PolypNugget);

abstract_class Polyp : public CBaseAnimating
{
	DECLARE_CLASS(Polyp, CBaseAnimating);
public:
	virtual void Precache(void);
	virtual void Spawn(void);
	virtual void Activate(void);

	virtual void TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr);
	virtual void Event_Killed(const CTakeDamageInfo &info);

	virtual void InputPop(inputdata_t &data);

	DECLARE_DATADESC();

	COutputEvent		m_OnPopped;

protected:
	//	virtual void	PopSound(void);
	virtual void MakeSpillDecals(const Vector &vecOrigin, int bloodColour);
	virtual void Pop(CBaseEntity *pOther, bool bSpawnNugget, bool bSpawnBlood) {};
	PolypState_t m_State;
};

void Polyp::Precache(void)
{
	BaseClass::Precache();
}

void Polyp::Spawn(void)
{
	Precache();
	BaseClass::Spawn();

	AddEffects(EF_NOSHADOW /*| EF_NORECEIVESHADOW*/);
	SetSolid(SOLID_BBOX);
	SetSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MOVETYPE_NONE);

	m_takedamage = DAMAGE_YES;

	// Start our idle activity
	SetSequence(SelectWeightedSequence(ACT_IDLE));
	SetCycle(random->RandomFloat(0.0f, 1.0f));
	ResetSequenceInfo();
	m_State = POLYP_WHOLE;
}

void Polyp::Activate(void)
{
	BaseClass::Activate();
}

void Polyp::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr)
{
	CTakeDamageInfo ainfo = info;

	if (ptr->hitgroup != HITGROUP_HEAD)
	{
		ainfo.ScaleDamage(0.1);
		ainfo.ScaleDamageForce(0.1);
	}
	if (m_State == POLYP_BROKEN)
	{
		ainfo.SetDamage(0);
	}
	if (ainfo.GetDamageType() == DMG_CLUB)
	{
		ainfo.ScaleDamage(100);
		ainfo.ScaleDamageForce(100);
	}

	BaseClass::TraceAttack(ainfo, vecDir, ptr );
}

void Polyp::Event_Killed(const CTakeDamageInfo &info)
{
	//
}

void Polyp::InputPop(inputdata_t &data)
{
	Pop(data.pActivator, true, true);
}

void Polyp::MakeSpillDecals(const Vector &vecOrigin, int bloodColour)
{
	trace_t tr;
	Vector	vecStart;
	Vector	vecTraceDir;

	GetVectors(NULL, NULL, &vecTraceDir);
	vecTraceDir.Negate();

	for (int i = 0; i < 8; i++)
	{
		vecStart.x = vecOrigin.x + random->RandomFloat(-16.0f, 16.0f);
		vecStart.y = vecOrigin.y + random->RandomFloat(-16.0f, 16.0f);
		vecStart.z = vecOrigin.z + 4;

		UTIL_TraceLine(vecStart, vecStart + (vecTraceDir * (5 * 12)), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

		if (tr.fraction != 1.0)
		{
			UTIL_BloodDecalTrace(&tr, bloodColour);
		}
	}
}

BEGIN_DATADESC(Polyp)
DEFINE_FIELD(m_State, FIELD_INTEGER),
DEFINE_INPUTFUNC(FIELD_VOID, "PopPolyp", InputPop),
DEFINE_OUTPUT(m_OnPopped, "OnPopped" ),
END_DATADESC()

class PolypYng : public Polyp
{
	DECLARE_CLASS(PolypYng, Polyp);
	DECLARE_DATADESC()

public:
	void Precache(void);
	void Spawn(void);
	void IdleThink(void);

	int  OnTakeDamage(const CTakeDamageInfo &info);
	void Event_Killed(const CTakeDamageInfo &info);

	void MakeIdleSounds(void);

	void Pop(CBaseEntity *pOther, bool bSpawnNugget, bool bSpawnBlood);
	void SpawnNugget(void);

	float m_nextidlesound_time;
};

// Generic spawn-related
void PolypYng::Precache(void)
{
	BaseClass::Precache();
	UTIL_PrecacheOther("item_polypnugget");
	PrecacheModel("models/_Monsters/Xen/xen_polyp.mdl"); // young, whole
	PrecacheModel("models/_Monsters/Xen/xen_polyp_b.mdl"); // young, broken
	PrecacheModel("models/_Monsters/Xen/xen_polyp_gib1.mdl"); // young, gibs
	PrecacheModel("models/_Monsters/Xen/xen_polyp_gib2.mdl"); // young, gibs
	PrecacheModel("models/_Monsters/Xen/xen_polyp_gib3.mdl"); // young, gibs
	PrecacheParticleSystem("blood_impact_antlion_01");
	PrecacheParticleSystem("antlion_spit");
	PrecacheScriptSound("XenPolyp.Idle");
	PrecacheScriptSound("XenPolyp.Explode");
}

void PolypYng::Spawn(void)
{
	Precache();
	SetModel("models/_Monsters/Xen/xen_polyp.mdl");
	BaseClass::Spawn();

	Vector vecMins, vecMaxs;
	RotateAABB(EntityToWorldTransform(), CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs(), vecMins, vecMaxs);

	UTIL_SetSize(this, vecMins, vecMaxs);

	SetHealth(10);
	m_nextidlesound_time = CURTIME + random->RandomFloat(4.0f, 18.0f);
	SetThink(&PolypYng::IdleThink);
	SetNextThink(CURTIME + 0.1f);
}

// Thinks
void PolypYng::IdleThink(void)
{
	if (m_State == POLYP_BROKEN)
	{
		SetThink(NULL);
		SetNextThink(TICK_NEVER_THINK);
	}

	if (m_nextidlesound_time < CURTIME)
	{
		MakeIdleSounds();
	}
	SetNextThink(CURTIME + 0.2f);
}

// Damage handling
int PolypYng::OnTakeDamage(const CTakeDamageInfo &info)
{
	CTakeDamageInfo copyInfo = info;
	if (copyInfo.GetDamageType() & DMG_PHYSGUN) copyInfo.SetDamage(GetHealth() + 1);
	if (copyInfo.GetAttacker()->ClassMatches("npc_pstorm")) copyInfo.SetDamage(GetHealth() + 1);
	return BaseClass::OnTakeDamage(copyInfo);
}

void PolypYng::Event_Killed(const CTakeDamageInfo &info)
{
	if (m_State != POLYP_BROKEN)
	{
		Pop(info.GetAttacker(), true, true);
	}
}

// NPC sounds
void PolypYng::MakeIdleSounds(void)
{
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if (pPlayer == NULL)
	{
		return;
	}

	float flDistToPlayerSqr = (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr();
	if (flDistToPlayerSqr < (512 * 512))
	{
		EmitSound("XenPolyp.Idle");
	}

	m_nextidlesound_time = CURTIME + random->RandomFloat(4.0f, 18.0f);
}

// Functions used anywhere above
void PolypYng::Pop(CBaseEntity *pOther, bool bSpawnNugget, bool bSpawnBlood)
{
	SetModel("models/_Monsters/Xen/xen_polyp_b.mdl");
	EmitSound("XenPolyp.Explode");
	CGib::SpawnSpecificGibs(this, 1, 350, 150, "models/_Monsters/Xen/xen_polyp_gib1.mdl", 20.0f, m_nSkin);
	CGib::SpawnSpecificGibs(this, 1, 350, 150, "models/_Monsters/Xen/xen_polyp_gib2.mdl", 20.0f, m_nSkin);
	CGib::SpawnSpecificGibs(this, 1, 350, 150, "models/_Monsters/Xen/xen_polyp_gib3.mdl", 20.0f, m_nSkin);
	if (bSpawnNugget)
		SpawnNugget();
	if (bSpawnBlood)
	{
		Vector vecOrigin;
		Vector vecForward;
		GetAttachment(LookupAttachment("nugget"), vecOrigin, &vecForward);
		DispatchParticleEffect("blood_impact_antlion_01", vecOrigin, QAngle(0, 0, 0));
		DispatchParticleEffect("antlion_spit", vecOrigin, QAngle(0, 0, 0));
		BaseClass::MakeSpillDecals(GetAbsOrigin(), BLOOD_COLOR_GREEN);
	}
	SetCollisionGroup(COLLISION_GROUP_DEBRIS);
	SetThink(NULL);
	m_State = POLYP_BROKEN;
	m_OnPopped.FireOutput(pOther, this);
}

void PolypYng::SpawnNugget(void)
{
	PolypNugget *pNugget = (PolypNugget *)CreateEntityByName("item_polypnugget");
	if (pNugget == NULL)
		return;

	Vector vecOrigin;
	Vector vecForward;
	GetAttachment(LookupAttachment("nugget"), vecOrigin, &vecForward);

	pNugget->SetAbsOrigin(vecOrigin);
	pNugget->SetAbsAngles(RandomAngle(0, 360));
	DispatchSpawn(pNugget);

	IPhysicsObject *pPhys = pNugget->VPhysicsGetObject();
	if (pPhys)
	{
		Vector vecForward;
		GetVectors(&vecForward, NULL, NULL);

		Vector vecVelocity = RandomVector(-15.0f, 15.0f) + (vecForward * -RandomFloat(10.0f, 15.0f));
		AngularImpulse vecAngImpulse = RandomAngularImpulse(-30.0f, 30.0f);

		pPhys->AddVelocity(&vecVelocity, &vecAngImpulse);
	}
}

// Datadesc
BEGIN_DATADESC(PolypYng)
DEFINE_FIELD(m_nextidlesound_time, FIELD_TIME),
DEFINE_THINKFUNC(IdleThink),
END_DATADESC()
LINK_ENTITY_TO_CLASS(npc_polyp, PolypYng)

class PolypOld : public Polyp
{
	DECLARE_CLASS(PolypOld, Polyp);
	DECLARE_DATADESC();

//	CHandle<CToxicCloud> m_hGasCloud;

public:
	PolypOld(void);
	void Precache(void);
	void Spawn(void);
	void IdleThink(void);
	void PoppedThink(void);

	int OnTakeDamage(const CTakeDamageInfo &info)
	{
		CTakeDamageInfo copyInfo = info;
		if (copyInfo.GetDamageType() & DMG_PHYSGUN) copyInfo.SetDamage(GetHealth() + 1);
		return BaseClass::OnTakeDamage(copyInfo);
	}
	void Event_Killed(const CTakeDamageInfo &info)
	{
		Pop(info.GetAttacker(), true);
	}

	void MakeIdleSounds(void);

	float m_nextidlesound_time;
	float m_ivebeenpopped_float; // not time, because we only need to register it once, not keep track of.
protected:
	void Pop(CBaseEntity *pOther, bool bSpawnBlood); // always creates cloud
//	void SpawnToxicCloud(void);
	void SpawnLOSBlocker(void);
};

//ConVar npc_polyp_old_smoke_expansion("npc_polyp_old_smoke_expansion", "1.5");
//ConVar npc_polyp_old_smoke_dimensions("npc_polyp_old_smoke_dimensions", "60 60 60");
ConVar sk_polyp_old_cloud_duration("sk_polyp_old_cloud_duration", "13");
ConVar sk_polyp_old_cloud_dmg("sk_polyp_old_cloud_dmg", "3");

// Constructor
PolypOld::PolypOld(void)/* : m_hGasCloud(NULL)*/
{
}

// Generic spawn-related
void PolypOld::Precache(void)
{
	BaseClass::Precache();
	PrecacheModel("models/_Monsters/Xen/xen_polyp_old.mdl"); // old, whole
	PrecacheModel("models/_Monsters/Xen/xen_polyp_old_b.mdl");
	PrecacheModel("models/_Monsters/Xen/smoke_cloud_placeholder.mdl");
	PrecacheScriptSound("XenPolypOld.Idle");
	PrecacheScriptSound("XenPolypOld.Explode");
}

void PolypOld::Spawn(void)
{
	Precache();
	SetModel("models/_Monsters/Xen/xen_polyp_old.mdl");
	BaseClass::Spawn();

	Vector vecMins, vecMaxs;
	RotateAABB(EntityToWorldTransform(), CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs(), vecMins, vecMaxs);

	UTIL_SetSize(this, vecMins, vecMaxs);

	SetHealth(8);
	m_nextidlesound_time = CURTIME + random->RandomFloat(2.0f, 7.0f);
	SetThink(&PolypOld::IdleThink);
	SetNextThink(CURTIME + 0.1f);
}

// Thinks
void PolypOld::IdleThink(void)
{
	StudioFrameAdvance();

	if (m_State == POLYP_BROKEN)
	{
		SetThink(&PolypOld::PoppedThink);
		SetNextThink(CURTIME + 0.5f);
	}
	if (m_nextidlesound_time < CURTIME)
	{
		MakeIdleSounds();
	}

	SetNextThink(CURTIME + 0.2f);
}

// This startes after we've been popped. 
// On each tick it'll deal toxic damage to things
// around, until it's been long enough since
// m_ivebeenpopped_float mark that the gas have dissipated.
void PolypOld::PoppedThink(void)
{
	if (m_State != POLYP_BROKEN) Assert(0); // how did we come to this?

	StudioFrameAdvance();
	
	if (CURTIME > m_ivebeenpopped_float + sk_polyp_old_cloud_duration.GetFloat())
	{
		SetThink(NULL);
		SetNextThink(TICK_NEVER_THINK);
	}
	
	CBaseEntity *hurt = NULL;
	while ((hurt = gEntList.FindEntityInSphere(hurt, WorldSpaceCenter(), 100)) != NULL)
	{
		if ((hurt->IsNPC() || hurt->IsPlayer()) && hurt->m_takedamage == DAMAGE_YES)
		{
			hurt->TakeDamage(CTakeDamageInfo(this, this, sk_polyp_old_cloud_dmg.GetInt(), DMG_NERVEGAS));
		}
	}
//	RadiusDamage(CTakeDamageInfo(this, this, sk_polyp_old_cloud_dmg.GetInt(), DMG_NERVEGAS), WorldSpaceCenter(), 100, CLASS_NONE, NULL);

	SetNextThink(CURTIME + 0.5f);
}

// NPC sounds
void PolypOld::MakeIdleSounds(void)
{
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if (pPlayer == NULL)
	{
		return;
	}

	float flDistToPlayerSqr = (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr();
	if (flDistToPlayerSqr < (512 * 512))
	{
		EmitSound("XenPolypOld.Idle");
	}
	m_nextidlesound_time = CURTIME + random->RandomFloat(4.0f, 18.0f);
}

// Functions used anywhere above
void PolypOld::Pop(CBaseEntity *pOther, bool bSpawnBlood)
{
	if (m_State != POLYP_BROKEN)
	{
		Vector vSrc = GetAbsOrigin();
		GetAttachment(LookupAttachment("glans"), vSrc);
		vSrc.z -= 8;
		EmitSound("XenPolypOld.Explode");
		DispatchParticleEffect("polyp_old_burst_system", vSrc, GetAbsAngles());
	//	SpawnToxicCloud();
	//	SpawnLOSBlocker();
		if (bSpawnBlood)
			BaseClass::MakeSpillDecals(GetAbsOrigin(), BLOOD_COLOR_RED);
		SetModel("models/_Monsters/Xen/xen_polyp_old_b.mdl");
		m_ivebeenpopped_float = CURTIME;
		SetThink(&PolypOld::PoppedThink);
		SetNextThink(CURTIME + 0.5f);
		SetCollisionGroup(COLLISION_GROUP_INTERACTIVE_DEBRIS);
		m_State = POLYP_BROKEN;
		m_OnPopped.FireOutput(pOther, this);
	}
}

//void PolypOld::SpawnToxicCloud(void)
//{
//	Vector vSrc = WorldSpaceCenter();
//	GetAttachment(LookupAttachment("toxiccloud"), vSrc);
//	Vector mins, maxs;
//	mins = Vector(-75, -75, -64);
//	maxs = Vector(75, 75, 64);
//	m_hGasCloud = CToxicCloud::CreateGasCloud(vSrc, this, "null", CURTIME + 25.0f, TOXICCLOUD_VENOM, 105);
//};

void PolypOld::SpawnLOSBlocker(void)
{
	Vector vSrc = WorldSpaceCenter();
	GetAttachment(LookupAttachment("toxiccloud"), vSrc);

	CDynamicProp *losBlocker = dynamic_cast< CDynamicProp * >(CreateEntityByName("prop_dynamic"));
	if (losBlocker)
	{
		char buf[512];
		// Pass in standard key values
		Q_snprintf(buf, sizeof(buf), "%.10f %.10f %.10f", vSrc.x, vSrc.y, vSrc.z);
		losBlocker->KeyValue("origin", buf);
		losBlocker->KeyValue("angles", "0 0 0");
		losBlocker->KeyValue("model", "models/_Monsters/Xen/smoke_cloud_placeholder.mdl");
		losBlocker->KeyValue("solid", "6" );
		losBlocker->KeyValue("fademindist", "-1");
		losBlocker->KeyValue("fademaxdist", "0");
		losBlocker->KeyValue("fadescale", "1");
		losBlocker->KeyValue("MinAnimTime", "5");
		losBlocker->KeyValue("MaxAnimTime", "10");
		losBlocker->Precache();
		DispatchSpawn(losBlocker);
		losBlocker->Activate();
		losBlocker->SetBlocksLOS(true);
		losBlocker->SetAIWalkable(false);
		if( UTIL_GetLocalPlayer())
			PhysDisableEntityCollisions(losBlocker, UTIL_GetLocalPlayer());
		losBlocker->SetRenderMode(kRenderTransAlpha);
		losBlocker->SetRenderColor(100, 100, 100, 50);
		losBlocker->ThinkSet(&CBaseEntity::SUB_Remove);
		losBlocker->SetNextThink(CURTIME + 15);
	}
};

// Datadesc
BEGIN_DATADESC(PolypOld)
DEFINE_FIELD(m_nextidlesound_time, FIELD_TIME),
DEFINE_FIELD(m_ivebeenpopped_float, FIELD_FLOAT),
DEFINE_THINKFUNC(IdleThink),
DEFINE_THINKFUNC(PoppedThink),
//DEFINE_FIELD(m_hGasCloud, FIELD_EHANDLE),
END_DATADESC()
LINK_ENTITY_TO_CLASS(npc_polyp_old, PolypOld)
#endif // POLYP

//====================================
// Protozoa (Dark Interval)
//====================================
#if defined (PROTOZOA)
class CNPC_Protozoa : public CNPC_BaseScanner
{
	DECLARE_CLASS(CNPC_Protozoa, CNPC_BaseScanner);
	DECLARE_DATADESC();
public:
	CNPC_Protozoa()
	{
		m_hNPCFileInfo = LookupNPCInfoScript("npc_protozoa");
	}
	void Precache(void)
	{
		const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
		SetModelName(MAKE_STRING(pModelName));
		PrecacheModel(STRING(GetModelName()));
		if (GetNPCScriptData().NPC_AltModel_Chance_float > 0 && MAKE_STRING(GetNPCScriptData().NPC_AltModel_Path_char) != NULL_STRING)
		{
			pModelName = GetNPCScriptData().NPC_AltModel_Path_char;
			PrecacheModel(pModelName);
		}
		PrecacheScriptSound("NPC_Protozoa.FlyLoop");
		PrecacheScriptSound("Flesh_Bloody.ImpactHard");
		BaseClass::Precache();
	};

	void Spawn(void)
	{
		Precache();
		BaseClass::Spawn();

		bool altModel = false;

		if (RandomFloat(0, 1) > GetNPCScriptData().NPC_AltModel_Chance_float && MAKE_STRING(GetNPCScriptData().NPC_AltModel_Path_char) != NULL_STRING)
		{
			const char *pAltModelName = GetNPCScriptData().NPC_AltModel_Path_char;
			SetModelName(MAKE_STRING(pAltModelName));
			altModel = true;
		}

		SetModel(STRING(GetModelName()));

		SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
		SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

		SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
		m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;

		CapabilitiesClear();
		
		if (altModel)
		{
			SetModelScale(RandomFloat(GetNPCScriptData().NPC_AltModel_ScaleMin_float, GetNPCScriptData().NPC_AltModel_ScaleMax_float));
			m_nSkin = RandomInt(GetNPCScriptData().NPC_AltModel_SkinMin_int, GetNPCScriptData().NPC_AltModel_SkinMax_int);
		}
		else
		{
			SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));
			m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);
		}

		UTIL_ScaleForGravity(1.0);

		m_nFlyMode = SCANNER_FLY_PATROL;
		m_vCurrentBanking = vec3_origin;
		m_NPCState = NPC_STATE_IDLE;
		StopLoopingSounds();

		// Efficiencty option - don't let protozoa sense anything
		GetSenses()->AddSensingFlags(SENSING_FLAGS_DONT_LISTEN|SENSING_FLAGS_DONT_LOOK);

		NPCInit();
	};

	void Activate()
	{
		BaseClass::Activate();
	};

	void NPCThink(void)
	{
		BaseClass::NPCThink();
	};

	void PrescheduleThink(void)
	{
		BaseClass::PrescheduleThink();
	};

	int OnTakeDamage_Alive(const CTakeDamageInfo &info)
	{
		return (BaseClass::OnTakeDamage_Alive(info));
	};
	
private:
	bool OverrideMove(float flInterval)
	{
		return BaseClass::OverrideMove(flInterval);
	};

	void MoveExecute_Alive(float flInterval)
	{
		if (m_lifeState == LIFE_DYING)
		{
			SetCurrentVelocity(vec3_origin);
			StopLoopingSounds();
		}

		else
		{
			// Amount of noise to add to flying
			float noiseScale = 3.0f;

			SetCurrentVelocity(GetCurrentVelocity() + VelocityToAvoidObstacles(flInterval));

			IPhysicsObject *pPhysics = VPhysicsGetObject();

			if (pPhysics && pPhysics->IsAsleep())
			{
				pPhysics->Wake();
			}

			// Add time-coherent noise to the current velocity so that it never looks bolted in place.
			AddNoiseToVelocity(noiseScale);

			AdjustScannerVelocity();

			float maxSpeed = GetEnemy() ? (GetMaxSpeed() * 2.0f) : GetMaxSpeed();

			// Limit fall speed
			LimitSpeed(maxSpeed);

			// Blend out desired velocity when launched by the physcannon
			BlendPhyscannonLaunchSpeed();

			//	PlayFlySound();
		}
	};

	void ClampMotorForces(Vector &linear, AngularImpulse &angular)
	{
		float zmodule = 200;
		linear.x = clamp(linear.x, -500, 500);
		linear.y = clamp(linear.y, -500, 500);
		linear.z = clamp(linear.z, -zmodule, zmodule);

		linear.z += (GetFloorDistance(WorldSpaceCenter()) > 400.0f)
			? 0 : 500;
	};

	void StopLoopingSounds(void)
	{
		BaseClass::StopLoopingSounds();
	};
public:
	Class_T Classify() { return CLASS_NONE; }

	float GetMaxSpeed(void) { return 300.0f; }
	int GetSoundInterests(void) { return (SOUND_NONE); }
	char *GetEngineSound(void) { return "NPC_Protozoa.FlyLoop"; }
	float MinGroundDist(void) { return 128.0f; }
	float GetGoalDistance(void) { return 64.0f; }

	void Event_Killed(const CTakeDamageInfo &info)
	{
	
		DispatchParticleEffect("blood_impact_protozoa", GetAbsOrigin(), GetAbsAngles());

		EmitSound("Flesh_Bloody.ImpactHard");

		// Replace baseclass's Event_Killed (baseclass is scanner, 
		// and we don't want its explosive Event_Killed), with manual
		// ragdoll creation.

		CleanupOnDeath(info.GetAttacker());
						
		// Go away
		RemoveDeferred();

		// no longer standing on a nav area
		ClearLastKnownArea();
	};
private:
	int m_memorytemp;
};

BEGIN_DATADESC(CNPC_Protozoa)
DEFINE_FIELD(m_memorytemp, FIELD_INTEGER),
END_DATADESC()
LINK_ENTITY_TO_CLASS(npc_protozoa, CNPC_Protozoa);
#endif // PROTOZOA

//====================================
// Xen Tree (HL1, updated)
//====================================
#if defined (XENTREE)
class XenTreeTrigger : public CBaseEntity
{
	DECLARE_CLASS(XenTreeTrigger, CBaseEntity);
public:
	void Touch(CBaseEntity *pOther);
	static XenTreeTrigger *TriggerCreate(CBaseEntity *pOwner, const Vector &position);
};
LINK_ENTITY_TO_CLASS(npc_tree_trigger, XenTreeTrigger);

XenTreeTrigger *XenTreeTrigger::TriggerCreate(CBaseEntity *pOwner, const Vector &position)
{
	XenTreeTrigger *pTrigger = CREATE_ENTITY(XenTreeTrigger, "npc_tree_trigger");
	pTrigger->SetAbsOrigin(position);

	pTrigger->SetSolid(SOLID_BBOX);
	pTrigger->AddSolidFlags(FSOLID_TRIGGER | FSOLID_NOT_SOLID);
	pTrigger->SetMoveType(MOVETYPE_NONE);
	pTrigger->SetOwnerEntity(pOwner);

	return pTrigger;
}

void XenTreeTrigger::Touch(CBaseEntity *pOther)
{
	if (pOther->ClassMatches("npc_tree")) return;
	if (pOther->ClassMatches("npc_tree_hair")) return;
	if (GetOwnerEntity())
	{
		GetOwnerEntity()->Touch(pOther);
	}
}

#define SF_XENHAIR_STANDALONE 1<<17

class XenHair : public CBaseAnimating
{
	DECLARE_CLASS(XenHair, CBaseAnimating);
public:
	void Precache(void);
	void Spawn(void);
	void Think(void);
	bool AllowedToIgnite(void) { return false; }
};
LINK_ENTITY_TO_CLASS(npc_tree_hair, XenHair);

void XenHair::Precache(void)
{
	BaseClass::Precache();
	PrecacheModel("models/_Monsters/Xen/Xen_hair.mdl");
}

void XenHair::Spawn(void)
{
	Precache();
	BaseClass::Spawn();
	SetModel("models/_Monsters/Xen/Xen_hair.mdl");
	UTIL_SetSize(this, Vector(-4, -4, 0), Vector(4, 4, 32));
	if (HasSpawnFlags(SF_XENHAIR_STANDALONE))
	{
		SetModelScale(RandomFloat(0.47, 0.6));
	}
	SetSequence(SelectWeightedSequence(ACT_IDLE));
	SetCycle(RandomFloat(0.0f, 1.0f));
	SetPlaybackRate(RandomFloat(0.7f, 1.4f));
	AddEffects(EF_NOSHADOW);
	ResetSequenceInfo();
	SetNextThink(CURTIME + random->RandomFloat(0.1, 0.4));	// Load balance these a bit
}

void XenHair::Think(void)
{
	StudioFrameAdvance();
	if (GetModelScale() < 0.05f)
	{
		UTIL_Remove(this);
		SetThink(NULL);
		SetNextThink(CURTIME);
	}

	if (GetOwnerEntity() != NULL && GetOwnerEntity()->IsAlive() || HasSpawnFlags(SF_XENHAIR_STANDALONE))
	{
		SetNextThink(CURTIME + 0.1f);
	}
	else
	{
		SetModelScale(0.01f, 4.0f);
		SetNextThink(CURTIME + 0.1f);
	}

	// See how close the player is
	CBasePlayer *pPlayerEnt = AI_GetSinglePlayer();
	float flDistToPlayerSqr = (GetAbsOrigin() - pPlayerEnt->GetAbsOrigin()).LengthSqr();

	float f = Square(500);
	float g = Square(750);
	float h = Square(1000);
	if (flDistToPlayerSqr > h)	SetNextThink(CURTIME + 0.75f);
	else if (flDistToPlayerSqr > g)	SetNextThink(CURTIME + 0.5f);
	else if (flDistToPlayerSqr > f)	SetNextThink(CURTIME + 0.25f);
	else SetNextThink(CURTIME + 0.1f);
}

class XenTree : public CAI_BaseNPC
{
	DECLARE_CLASS(XenTree, CAI_BaseNPC);
public:
	XenTree()
	{
		m_hNPCFileInfo = LookupNPCInfoScript("npc_tree");
	};
	void Precache(void);
	void Spawn(void);
	void XenTreeThink(void);
	void Touch(CBaseEntity *pOther);
	void SpawnSlicedGibs(int slicesection, bool serversidegibs);
	virtual void TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr);
	virtual int OnTakeDamage_Alive(const CTakeDamageInfo &info);
	virtual void Event_Killed(const CTakeDamageInfo &info);
	void HandleAnimEvent(animevent_t *pEvent);
	void InputAttack(inputdata_t &data);
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;

	// We don't actually use relations and combat AI. The attacks are
	// triggered by the tree's own code, the touch function.
	Class_T Classify(void) { return CLASS_NONE; }

	bool AllowedToIgnite(void) { return true; }

	void NPCThink(void);
	int UpdateTransmitState();
	bool CreateVPhysics(void);
	void InitBoneFollowers(void);
	CBoneFollowerManager m_BoneFollowerManager;
	CBoneFollower *GetBoneFollowerByIndex(int nIndex);
	int GetBoneFollowerIndex(CBoneFollower *pFollower);
	void UpdateOnRemove(void);
	void OnRestore();
	void Attack(void);
	void SpawnHairs(void);

	// The tree shouldn't take in sounds and try to face the enemy.
	int GetSoundInterests(void) {return	NULL;}
	float MaxYawSpeed(void) { return 0.0f; }

private:
	bool m_xentree_spawn_hairs_bool;
	XenTreeTrigger *m_pTrigger;
};

int AE_TREE_ATTACK_BEGIN;
int AE_TREE_ATTACK_HIT;
int AE_TREE_ATTACK_END;

// custom hitgroups
#define HITGROUP_ROOT	100
#define HITGROUP_BODY	101
#define HITGROUP_ELBOW	102
#define HITGROUP_NECK	103
#define HITGROUP_ARM	104
#define HITGROUP_STING	105

// The tree has physical followers for every bone. 
static const char *pTreeFollowerBoneNames[] =
{
	"Bone01",
	"Bone03",
	"Bone04",
	"Bone05",
	"Bone06",
	"Bone07",
//	"Bone08",
};

void XenTree::Precache(void)
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	PrecacheModel("models/_Monsters/Xen/Xen_tree_stump_low.mdl");
	PrecacheModel("models/_Monsters/Xen/Xen_tree_gib_low.mdl");
	PrecacheModel("models/_Monsters/Xen/Xen_tree_stump_high.mdl");
	PrecacheModel("models/_Monsters/Xen/Xen_tree_gib_high.mdl");

	// TODO: don't have these models yet
//	PrecacheModel("models/_Monsters/Xen/Xen_tree_stump_mid.mdl");
//	PrecacheModel("models/_Monsters/Xen/Xen_tree_gib_mid.mdl");

	PrecacheScriptSound("XenTree.AttackBegin");
	PrecacheScriptSound("XenTree.AttackMiss");
	PrecacheScriptSound("XenTree.AttackHit");
	PrecacheScriptSound("E3_Phystown.Slicer"); // TODO: make real sound
	UTIL_PrecacheOther("npc_tree_hair");
	BaseClass::Precache();
}

void XenTree::Spawn(void)
{
	Precache();
	SetModel(STRING(GetModelName()));

	SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
	SetHullSizeNormal();
	SetSolid(SOLID_BBOX);
	SetSolidFlags(FSOLID_NOT_STANDABLE);
	SetCollisionGroup(COLLISION_GROUP_NPC);
	SetMoveType(MOVETYPE_NONE);
	AddSolidFlags(FSOLID_TRIGGER); // this NPC serves as a trigger - makes reactions possible
	SetBoneCacheFlags(BCF_NO_ANIMATION_SKIP);
	AddSolidFlags(FSOLID_CUSTOMRAYTEST | FSOLID_CUSTOMBOXTEST);

	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;
	m_iHealth = GetNPCScriptData().NPC_Stats_Health_int;
	m_NPCState = NPC_STATE_NONE;
	m_takedamage = DAMAGE_YES;
	
	// Start our idle activity
	SetActivity(ACT_IDLE);
	SetPlaybackRate(random->RandomFloat(0.8f, 1.2f));

	SetThink(&XenTree::XenTreeThink);
	SetNextThink(CURTIME + 0.1f);

// TODO: apparently there's some problem with the trigger
// and hairs. Disabling trigger for now. The tree still works
// because it has its own touch function anyway... with the trigger,
// the touch zone was larger and you didn't have to touch the tree
// in direct manner for it to retaliate. But oh well. 
#if 1
	Vector triggerPosition, vForward;
	AngleVectors(GetAbsAngles(), &vForward);
	triggerPosition = GetAbsOrigin() + (vForward * 64);
	// Create the trigger
	m_pTrigger = XenTreeTrigger::TriggerCreate(this, triggerPosition);
	UTIL_SetSize(m_pTrigger, Vector(-64, -64, 0), Vector(64, 64, 64));
#endif
	
	// Efficiency option - trees don't need to see or listen, 
	// they have the touch trigger entity.
	GetSenses()->AddSensingFlags(SENSING_FLAGS_DONT_LISTEN | SENSING_FLAGS_DONT_LOOK);

	NPCInit();

	if( m_xentree_spawn_hairs_bool) SpawnHairs();
}

// Spawn a bunch of hair entities and disperce them around our bottom.
// Currently doesn't take into account trees that are placed 
// horizontally or upside down... uncertain if we'll end up using
// those anyway. TODO: don't let the random position vector
// end up inside the trunk - do some sort of 'exclusion zone'.
void XenTree::SpawnHairs()
{
	trace_t tr;
	Vector	traceStart;
	Vector	traceDir;

	GetVectors(NULL, NULL, &traceDir);
	traceDir.Negate(); // take the 'up' vector and reverse it

	int maxHairs = RandomInt(3, 9);

	for (int i = 0; i < maxHairs; i++)
	{
		traceStart.x = GetAbsOrigin().x + random->RandomFloat(-40.0f, 40.0f);
		traceStart.y = GetAbsOrigin().y + random->RandomFloat(-40.0f, 40.0f);
		traceStart.z = GetAbsOrigin().z + 40;

		XenHair *pHair = CREATE_ENTITY(XenHair, "npc_tree_hair");
		pHair->Spawn();
		pHair->SetAbsOrigin(GetAbsOrigin());
		// Randomise yaw. TODO: do smarter yaw so most of them
		// face outward from the trunk.
		pHair->SetAbsAngles(QAngle(0, RandomInt(0, 180), 0));
		pHair->SetSolid(SOLID_NONE);
		pHair->AddSolidFlags(FSOLID_NOT_SOLID);
		pHair->SetMoveType(MOVETYPE_NONE);
		pHair->SetOwnerEntity(this);
		pHair->SetModelScale(RandomFloat(0.5,0.8));
		trace_t tr;
		UTIL_TraceLine(traceStart, traceStart - Vector(0, 0, 2048), 
			MASK_SHOT, this, 
			COLLISION_GROUP_NONE, &tr);
		if (tr.fraction != 0)
		{
			pHair->Teleport(&tr.endpos, &pHair->GetAbsAngles(), &vec3_origin);
		}
	}
}

#define XEN_TREE_MAX_SIDE_FACING 40
void XenTree::NPCThink(void)
{
	BaseClass::NPCThink();
	// update follower bones
	m_BoneFollowerManager.UpdateBoneFollowers(this);
	
	if (m_lifeState == LIFE_ALIVE)
	{
		// See how close the player is
		CBasePlayer *pPlayerEnt = AI_GetSinglePlayer();
		float flDistToPlayerSqr = (GetAbsOrigin() - pPlayerEnt->GetAbsOrigin()).LengthSqr();
		float flDistThreshold = 200;
		float flDistCheck = MIN(flDistToPlayerSqr, flDistThreshold * flDistThreshold);
		Vector vecDistToPose(0, 0, 0);
		vecDistToPose.x = flDistCheck;
		VectorNormalize(vecDistToPose);

		// set hit distance according to distance to player
		SetPoseParameter("attack_dist", sqrt(flDistCheck));

		// check the angle, which way is our enemy
		Vector vecEnemyDir = GetAbsOrigin() - pPlayerEnt->GetAbsOrigin();
		Vector vecEnemyFacingDir;
		VectorIRotate(vecEnemyDir, EntityToWorldTransform(), vecEnemyFacingDir);

		QAngle angles;
		VectorAngles(vecEnemyFacingDir, angles);
		angles.y = AngleNormalize(angles.y);

		QAngle angFacing;

		if (angles.y > XEN_TREE_MAX_SIDE_FACING)
		{
			angFacing.y = MIN(angles.y, XEN_TREE_MAX_SIDE_FACING);
		}
		if (angles.y < -XEN_TREE_MAX_SIDE_FACING)
		{
			angFacing.y = MAX(angles.y, -XEN_TREE_MAX_SIDE_FACING);
		}

		SetPoseParameter("attack_facing", angFacing.y);
	}
	else
	{
		SetPoseParameter("attack_dist", 64);
		SetPoseParameter("attack_facing", 0);
	}
	SetNextThink(CURTIME + 0.1f);

//	There's no need to move trigger every think...
//	Because the NPC is stationary. HOWEVER, if we ever end up
//	having it parented to floating islands or some such thing,
//	this will become relevant again. 

//	Vector triggerPosition, vForward;
//	AngleVectors(GetAbsAngles(), &vForward);
//	triggerPosition = GetAbsOrigin() + (vForward * 64);
}

int XenTree::UpdateTransmitState()
{
	return SetTransmitState(FL_EDICT_ALWAYS);
}

void XenTree::OnRestore()
{
	BaseClass::OnRestore();
	CreateVPhysics();
}

bool XenTree::CreateVPhysics(void)
{
	InitBoneFollowers();
	return true;
}
void XenTree::InitBoneFollowers(void)
{
	// Don't do this if we're already loaded
	if (m_BoneFollowerManager.GetNumBoneFollowers() != 0)
		return;

	// Init our followers
	m_BoneFollowerManager.InitBoneFollowers(this, ARRAYSIZE(pTreeFollowerBoneNames), pTreeFollowerBoneNames);
}

void XenTree::UpdateOnRemove(void)
{
	// Need to destroy followers to avoid assert in physics_bone_follower.cpp
	m_BoneFollowerManager.DestroyBoneFollowers();
	BaseClass::UpdateOnRemove();
}

void XenTree::Touch(CBaseEntity *pOther)
{
	if( GetHealth() > 0) // somehow this check is needed?
		Attack();
}

// slicesection - defines which models to use for stump and tentacle. 
// serversidegibs - tells if the two parts need to have collision (always on for now).
void XenTree::SpawnSlicedGibs(int slicesection, bool serversidegibs)
{
	// Make preparations...
	const char* tentacleModel;
	const char* stumpModel;
	Vector slicepoint;
	QAngle sliceangles;

	// Decide which models to use.
	// "low" point means the stump is about 2.5 meters tall,
	// and the cut off tentacle is about 2.5 meters long.
	// "mid" is 3/2 meters ratio, give or take.
	// And "high" means most of the body is present
	// in stump, and we only cut off the pointed, hard
	// bit at the end (about 1.2-1.5m). It still kills the tree,
	// and we can pick up that bit and throw it
	// with gravity gun. (similar to harpoon prop)
	// Luckily, all these models are made from the same
	// whole body model, so we don't have to position-adjust
	// any of them manually.
	switch (slicesection)
	{
	case 0:
	{
		tentacleModel = "models/_Monsters/Xen/Xen_tree_gib_low.mdl";
		stumpModel = "models/_Monsters/Xen/Xen_tree_stump_low.mdl";
	}
	break;
//	case 1: // TODO: this model isn't made yet.
//	{
//		tentacleModel = "models/_Monsters/Xen/Xen_tree_gib_mid.mdl";
//		stumpModel = "models/_Monsters/Xen/Xen_tree_stump_mid.mdl";
//	}
//	break;
	case 2:
	{
		tentacleModel = "models/_Monsters/Xen/Xen_tree_gib_high.mdl";
		stumpModel = "models/_Monsters/Xen/Xen_tree_stump_high.mdl";
	}
	break;
	default:
	{
		tentacleModel = "models/_Monsters/Xen/Xen_tree_gib_low.mdl";
		stumpModel = "models/_Monsters/Xen/Xen_tree_stump_low.mdl";
	}
	break;
	}

	CBaseAnimating *pGibAnimating = dynamic_cast<CBaseAnimating*>(this);
	Vector vecGibForce;

	// Spawn remaining stump part, give it momentum.
	vecGibForce.x = random->RandomFloat(-400, 400);
	vecGibForce.y = random->RandomFloat(-400, 400);
	vecGibForce.z = random->RandomFloat(0, 300);

	CRagdollProp *pGibStump = (CRagdollProp *)CBaseEntity::CreateNoSpawn("prop_ragdoll", GetAbsOrigin(), GetAbsAngles(), NULL);
	pGibStump->CopyAnimationDataFrom(pGibAnimating);
	pGibStump->SetModelName(MAKE_STRING(stumpModel));
	pGibStump->SetOwnerEntity(NULL); // we want to collide with the tentacle!
	pGibStump->RemoveEffects(EF_NODRAW); // this is set in OnTakeDamage_Alive and
										 // would be carried onto gib entity.
	pGibStump->Spawn();
		
	if (pGibStump)
	{
		// Extract the attachment vector from the newly
		// created stump gib. This is needed because
		// different stumps end on different point,
		// and we can't use the original tree model
		// because it's gone (EF_NODRAW'd) at this point.
		int iBone = LookupBone("Bone07");
		if (iBone >= 0)
		pGibStump->GetBonePosition(iBone, slicepoint, sliceangles);
	}
	else
	{
		return;
	}

	// Spawn sliced tentacle part
	vecGibForce.x = random->RandomFloat(1000, 3000);
	vecGibForce.y = random->RandomFloat(-500, -500);
	vecGibForce.z = random->RandomFloat(1000, 3000);

	if (slicesection < 2)
	{
		CRagdollProp *pGibTentacle = (CRagdollProp *)CBaseEntity::CreateNoSpawn("prop_ragdoll", GetAbsOrigin(), GetAbsAngles(), NULL);
		pGibTentacle->CopyAnimationDataFrom(pGibAnimating);
		pGibTentacle->SetModelName(MAKE_STRING(tentacleModel));
		pGibTentacle->SetOwnerEntity(NULL); // we want to collide with the stump!
		pGibTentacle->RemoveEffects(EF_NODRAW); // need to clear it after Event_Killed()
		pGibTentacle->Spawn();

		// Send the tentacle into spasmatic agony
		inputdata_t input;
		input.pActivator = this;
		input.pCaller = this;
		input.value.SetFloat(7.0f); // N seconds of spasms
		pGibTentacle->InputStartRadgollBoogie(input);
	}
	else
	{
		vecGibForce.x = random->RandomFloat(50, 150);
		vecGibForce.y = random->RandomFloat(-30, 30);
		vecGibForce.z = random->RandomFloat(50, 150);

		CPhysicsProp *pGibSting = dynamic_cast< CPhysicsProp * >(CreateEntityByName("prop_physics"));
		if (pGibSting)
		{
			char buf[512];
			// Pass in standard key values
			Q_snprintf(buf, sizeof(buf), "%.10f %.10f %.10f", slicepoint.x, slicepoint.y, slicepoint.z);
			pGibSting->KeyValue("origin", buf);
			Q_snprintf(buf, sizeof(buf), "%.10f %.10f %.10f", GetAbsAngles().x, GetAbsAngles().y, GetAbsAngles().z);
			pGibSting->KeyValue("angles", buf);
			pGibSting->KeyValue("model", tentacleModel);
			pGibSting->KeyValue("fademindist", "-1");
			pGibSting->KeyValue("fademaxdist", "0");
			pGibSting->KeyValue("fadescale", "1");
			pGibSting->KeyValue("inertiaScale", "1.0");
			pGibSting->KeyValue("physdamagescale", "0.1");
			pGibSting->Precache();
			DispatchSpawn(pGibSting);
			pGibSting->Teleport(&slicepoint, &GetAbsAngles(), &vecGibForce);
			pGibSting->Activate();
		}
	}
	// Add a cloud of blood 
	Vector vecBloodDelta = slicepoint - pGibStump->GetAbsOrigin();
	vecBloodDelta.z = 0; // effect looks better this way
	VectorNormalize(vecBloodDelta);
	UTIL_BloodSpray(slicepoint + vecBloodDelta * 4, vecBloodDelta, BloodColor(), 8, FX_BLOODSPRAY_ALL);
	UTIL_BloodSpray(slicepoint + vecBloodDelta * 4, vecBloodDelta, BloodColor(), 11, FX_BLOODSPRAY_DROPS);

	// Add a particle blood spray. It's precached globally.
	DispatchParticleEffect("blood_advisor_puncture_withdraw", PATTACH_POINT_FOLLOW, pGibStump, "particle_point");
	DispatchParticleEffect("blood_advisor_pierce_spray", PATTACH_POINT_FOLLOW, pGibStump, "particle_point");
}

void XenTree::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr)
{
	CTakeDamageInfo infoCopy = info;

	// Adjust damage income for different parts of the tree.
	// Trunk takes little damage, root takes no damage,
	// middle part has standard durability,
	// and the sting is all solid -
	// takes no damage. 

	// NOTE that this is not at all taken into account
	// when dealing physics damage (like throwing sawblades at it).
	// TraceAttack(...) simply is not called for them.

	// If it were possible we could've had physics attacks
	// doing different things based on bodypart it hits.

	if (ptr->hitgroup == 3) // root
	{
		infoCopy.SetDamage(0.1);
	}

	if (ptr->hitgroup == 4) // trunk // FIXME: actually never seem to hit this one? why? hitbox is correctly defined!
	{
		infoCopy.SetDamage(0.1);
	}
	
	if (ptr->hitgroup == 10) // sting
	{
		// Yes, technically it's still taking damage -
		// we want this miniscule damage to register
		// so it can be taken into account, for whatever purpose.
		infoCopy.SetDamage(0.01);
	}

	if (infoCopy.GetDamageType() & DMG_BLAST)
	{
	//	Msg("adding removenoragdoll\n");
		infoCopy.AddDamageType(DMG_REMOVENORAGDOLL);
	}

	BaseClass::TraceAttack(infoCopy, vecDir, ptr);
}

int XenTree::OnTakeDamage_Alive(const CTakeDamageInfo &info)
{
	CTakeDamageInfo infoCopy = info;
	
	// Take more damage from sharp physics,
	// but reduce force so the slice gib
	// don't fly off ridiculously. 
	if (infoCopy.GetDamageType() & (DMG_SLASH))
	{
		infoCopy.ScaleDamage(2.0);
		infoCopy.ScaleDamageForce(0.1);
	}

	// Xen trees are immune to fire. Why? It's just so.
	if (infoCopy.GetDamageType() & DMG_BURN)
	{
		infoCopy.ScaleDamage(0.0);
	}

	// Explosions are guaranteed to kill the tree
	if (infoCopy.GetDamageType() & DMG_BLAST)
	{
		infoCopy.SetDamage(GetHealth() + 10);
		infoCopy.ScaleDamageForce(0.5);
	}

	// Make it harder to kill with the crowbar.
	if (infoCopy.GetDamageType() & DMG_CLUB)
	{
		infoCopy.ScaleDamage(0.5);
	}
	
	// Hitting the tree provokes it.
	if (infoCopy.GetDamageType() & DMG_BULLET ||
		infoCopy.GetDamageType() & DMG_CLUB ||
		infoCopy.GetDamageType() & DMG_PHYSGUN ||
		infoCopy.GetDamageType() & DMG_SHOCK ||
		infoCopy.GetDamageType() & DMG_SONIC)
	{
		Attack();
	}

	// Fixate our tweaked damage at this point. Below we either 
	// detect slash/blast cut and kill the tree in a special way,
	// or if not just pass this value onto base function.
	int tookDamage = BaseClass::OnTakeDamage_Alive(infoCopy);

	// Detect the impact point. This is done here because
	// TraceAttack() is NOT called when it's physics damage.
	Vector dPos = infoCopy.GetDamagePosition();
	Vector b1Pos, b2Pos, b3Pos;	QAngle bAng;
	// Look up bone positions for parts we choose as cut points.
	// These are "neck" below joint, part after joint, 
	// and the base of the sting.
	GetBonePosition(LookupBone("Bone02"), b1Pos, bAng);
	GetBonePosition(LookupBone("Bone06"), b2Pos, bAng);
	GetBonePosition(LookupBone("Bone08"), b3Pos, bAng);
	// Calculate distance between impact point and the cut points.
	float dLng1 = (dPos - b1Pos).Length();
	float dLng2 = (dPos - b2Pos).Length();
	float dLng3 = (dPos - b3Pos).Length();

	// Override default event_killed, spawn two parted ragdoll.

	// This value lets us shrug off minor slash hits, and only 
	// cut the tree if slash damage is sufficient.
	float hT = MIN(1, infoCopy.GetDamage() / GetHealth());
	if( infoCopy.GetDamageType() & DMG_SLASH && hT >= 0.5 )
	{
	//	NDebugOverlay::Cross3D(dPos, 8, 255, 0, 0, true, 0.5f);
	//	NDebugOverlay::Cross3D(b1Pos, 8, 255, 255, 0, true, 0.5f);
	//	NDebugOverlay::Cross3D(b2Pos, 8, 0, 255, 255, true, 0.5f);
	//	NDebugOverlay::Cross3D(b3Pos, 8, 255, 0, 255, true, 0.5f);
		int i = -1; // This is our selector value.
		if (dLng1 < 16.0f && dLng2 > 16.0f && dLng3 > 16.0f)
			i = 0; // impactis within 16 units of "neck".
		else if (dLng2 < 16.0f && dLng1 > 16.0f && dLng3 > 16.0f)
			i = 0; // impact within 16 units of "arm".
		else if (dLng3 < 16.0f && dLng1 > 16.0f && dLng2 > 16.0f)
			i = 2; // impact within 16 units of base of sting.
				   // 16 and not 8 because sting is long, we want
				   // to make it easier to cut off.
		
		if (i >= 0) // if impact was close to any of the points...
		{
			// Slice it apart.
			EmitSound("E3_Phystown.Slicer");
			AddEffects(EF_NODRAW);
			AddSolidFlags(FSOLID_NOT_SOLID);
			infoCopy.SetDamageType(DMG_REMOVENORAGDOLL);
			Event_Killed(infoCopy);
			SpawnSlicedGibs(i, true);
		}
		// Clear the selector value
		i = -1;
		return 0; // Do not pass it further. We already killed
				  // the tree and spawned the gibs.
	}

	// but if damage is DMG_BLAST and happens close enough, destroy the tree immediately. 
	// Special case! spawn multiple chunks (stump, "shoulder" gib, and the sting).
	if (infoCopy.GetDamageType() & DMG_BLAST && infoCopy.GetDamagePosition().DistToSqr(GetAbsOrigin()) <= 256 * 256)
	{
		// Slice it apart.
		EmitSound("E3_Phystown.Slicer");
		AddEffects(EF_NODRAW);
		AddSolidFlags(FSOLID_NOT_SOLID);
		infoCopy.SetDamageType(DMG_REMOVENORAGDOLL);
		Event_Killed(infoCopy);
		SpawnSlicedGibs(2, true);
		return 0; // Do not pass it further. We already killed
				  // the tree and spawned the gibs.
	}
		
	return tookDamage;
}

void XenTree::Event_Killed(const CTakeDamageInfo &info)
{
	// Deal with our produced entities. 
	// Xen hairs look into their owner entity on their own,
	// no need to call and tell them.
	m_BoneFollowerManager.DestroyBoneFollowers();

	if (m_pTrigger)
	{
		m_pTrigger->SetOwnerEntity(NULL); // disable trigger outputs
		UTIL_Remove(m_pTrigger);
	}

	if (info.GetDamageType() & DMG_REMOVENORAGDOLL)
	{
	//	Msg("is removenoragdoll\n");
		UTIL_Remove(this);
		return;
	}

	BaseClass::Event_Killed(info);
}

void XenTree::InputAttack(inputdata_t &data)
{
	Attack();
}

void XenTree::Attack(void)
{
	if (m_flNextAttack > CURTIME) return;

	if (GetActivity() == ACT_IDLE)
	{
		SetIdealActivity(ACT_MELEE_ATTACK1);
		m_flPlaybackRate = random->RandomFloat(1.0, 1.4);
	}

	// disable immediate attacks in a row
	m_flNextAttack = CURTIME + 0.5f;
}

void XenTree::HandleAnimEvent(animevent_t *pEvent)
{
	if (pEvent->event == AE_TREE_ATTACK_BEGIN)
	{
		CPASAttenuationFilter filter(this);
		EmitSound(filter, entindex(), "XenTree.AttackBegin");
		return;
	}
	
	if (pEvent->event == AE_TREE_ATTACK_HIT)
	{
		CPASAttenuationFilter filter(this);
		EmitSound(filter, entindex(), "XenTree.AttackHit");

		CBaseEntity *pList[8];
		BOOL sound = false;
		int count = UTIL_EntitiesInBox(pList, 8, m_pTrigger->GetAbsOrigin() + m_pTrigger->WorldAlignMins(), m_pTrigger->GetAbsOrigin() + m_pTrigger->WorldAlignMaxs(), FL_NPC | FL_CLIENT);

		Vector forward;
		AngleVectors(GetAbsAngles(), &forward);

		for (int i = 0; i < count; i++)
		{
			if (pList[i] != this && (pList[i]->ClassMatches("npc_tree_hair")) == 0)
			{
				if (pList[i]->GetOwnerEntity() != this)
				{
					sound = true;
					pList[i]->TakeDamage(CTakeDamageInfo(this, this, 25, DMG_SLASH));
					pList[i]->ViewPunch(QAngle(15, 0, 18));

					pList[i]->SetAbsVelocity(pList[i]->GetAbsVelocity() + forward * 100);
				}
			}
		}
		if (sound)
		{
			CPASAttenuationFilter filter(this);
			EmitSound(filter, entindex(), "XenTree.AttackHit");
		}

		return;
	}
	
	if (pEvent->event == AE_TREE_ATTACK_END)
	{
		SetIdealActivity(ACT_IDLE);
		m_flPlaybackRate = random->RandomFloat(0.8f, 1.4f);
		m_flNextAttack = CURTIME + 0.5f;
		return;
	}

	BaseClass::HandleAnimEvent(pEvent);
}

void XenTree::XenTreeThink(void)
{
	DispatchAnimEvents(this);
	m_BoneFollowerManager.UpdateBoneFollowers(this);	
	StudioFrameAdvance();
	SetNextThink(CURTIME + 0.1f);
}

BEGIN_DATADESC(XenTree)
	DEFINE_KEYFIELD(m_xentree_spawn_hairs_bool, FIELD_BOOLEAN, "ShouldSpawnXenHairs"),
	DEFINE_INPUTFUNC(FIELD_VOID, "Attack", InputAttack),
	DEFINE_FIELD(m_pTrigger, FIELD_CLASSPTR),
	DEFINE_THINKFUNC(XenTreeThink),
	DEFINE_EMBEDDED(m_BoneFollowerManager),
END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_tree, XenTree);

AI_BEGIN_CUSTOM_NPC(npc_tree, XenTree)
	DECLARE_ANIMEVENT(AE_TREE_ATTACK_BEGIN)
	DECLARE_ANIMEVENT(AE_TREE_ATTACK_HIT)
	DECLARE_ANIMEVENT(AE_TREE_ATTACK_END)
AI_END_CUSTOM_NPC()
#endif // XENTREE

//====================================
// Crab Synth (Dark Interval)
//====================================
#if defined (CRABSYNTH)
#ifdef RIDEABLE_NPCS
//-----------------------------------------------------------------------------
// Purpose: Strider vehicle server
//-----------------------------------------------------------------------------
class CNPC_CrabSynth;
class CCrabSynthServerVehicle : public CBaseServerVehicle
{
	typedef CBaseServerVehicle BaseClass;
	// IServerVehicle
public:
	void	GetVehicleViewPosition(int nRole, Vector *pAbsOrigin, QAngle *pAbsAngles, float *pFOV = NULL);
	bool	GetPassengerExitPoint(int nRole, Vector *pPoint, QAngle *pAngles);
	bool	IsPassengerUsingStandardWeapons(int nRole = VEHICLE_ROLE_DRIVER) { return true; }

protected:
	CNPC_CrabSynth *GetCrabSynth(void);
};
#endif


class CNPC_CrabSynth : public CAI_BlendedNPC
#ifdef RIDEABLE_NPCS
	, public IDrivableVehicle
#endif
{
	DECLARE_CLASS(CNPC_CrabSynth, CAI_BlendedNPC);
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
public:
	CNPC_CrabSynth();
	~CNPC_CrabSynth();
	void Spawn(void);
	void Precache(void);
	Class_T Classify(void) { return CLASS_COMBINE; }
	void PostNPCInit();
	void NPCThink(void);
	void OnRestore();

#ifdef RIDEABLE_NPCS
	void			PlayerTouch(CBaseEntity *pOther);
#endif

	virtual bool	IsHeavyDamage(const CTakeDamageInfo &info);
	void TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	int OnTakeDamage_Alive(const CTakeDamageInfo &info);
	void Event_Killed(const CTakeDamageInfo &info);
	bool BecomeRagdoll(const CTakeDamageInfo &info, const Vector &forceVector);

	void IdleSound(void);
	void AlertSound(void);
//	void PainSound(const CTakeDamageInfo &info);
	void DeathSound(const CTakeDamageInfo &info);
	void StopLoopingSounds(void);
	CSoundPatch	*m_pEngineSound;
	CSoundPatch	*m_pEngineSound_Hurt;

	bool CarriedByDropship() 
	{
		if (GetOwnerEntity() && FClassnameIs(GetOwnerEntity(), "npc_combinedropship"))
			return true;

		return false;
	}

	void CarriedThink() 
	{
		SetNextThink(CURTIME + 0.05);
		StudioFrameAdvance();

		Vector vecGround = GetAbsOrigin();
		TranslateNavGoal(NULL, vecGround);

		if (!CarriedByDropship())
		{
			SetSolid(SOLID_BBOX);
			SetThink(&CAI_BaseNPC::CallNPCThink);
		}
	}

	Activity NPC_TranslateActivity(Activity activity);
	void PrescheduleThink(void);
	int SelectSchedule(void);
	int TranslateSchedule(int scheduleType);
	void StartTask(const Task_t *pTask);
	void RunTask(const Task_t *pTask);

	float MaxYawSpeed(void);
	bool OverrideMoveFacing(const AILocalMoveGoal_t &move, float flInterval);

	float StepHeight() const { return 64.0f; }

	void HandleAnimEvent(animevent_t *pEvent);

	void RegenThink(void);
	void FireRocketsThink(void);
	void FireMinigunThink(void);
	float ChargeSteer();
	void ChargeDamage(CBaseEntity *pTarget);
	void CheckMeleeAttack(float distance, QAngle& viewPunch, Vector& shove, bool bIsSwipe);
	bool HandleChargeImpact(Vector vecImpact, CBaseEntity *pEntity);
	void FireMinigun(void);
	void FirePulsegun(void);
	void DoMuzzleFlash(void);
	void DoImpactEffect(trace_t &tr, int nDamageType);

	bool IsValidEnemy(CBaseEntity *pEnemy)
	{
		if (m_crabsynth_shield_enabled) // don't care about outside world while regenerating
		{
			return false;
		}
		
		return BaseClass::IsValidEnemy(pEnemy);
	}
	
	int MeleeAttack1Conditions(float flDot, float flDist); // claw attack
	int MeleeAttack2Conditions(float flDot, float flDist); // charge

	void CollapseRegenShield(void);
	bool CreateRegenShield(float flDur);
	void InputEnableShield(inputdata_t &data) 
	{ 
		float dur = 10;
		dur = data.value.Float();

		if (m_crabsynth_shield_enabled && m_hShieldEntity)
		{
			// already did that - in this case just set the timer to input value
			m_regen_duration_time = CURTIME + dur;
			return;
		}

		m_auto_regen_duration_float = dur;
		ClearSchedule("forced input");
	//	SetCondition(COND_CRABSYNTH_ENTER_REGEN);
		SetSchedule(SCHED_CRABSYNTH_ENTER_REGEN);

	//	if (!CreateRegenShield( dur ))
	//	{
	//		Msg("Crab Synth failed to create its regen field!\n");
	//		Assert(0);
	//		if (m_hShieldEntity)
	//			CollapseRegenShield();
	//	}
	}
	void InputDisableShield(inputdata_t &data) 
	{ 
		CollapseRegenShield();
	}
	void InputEnableAutoRegen(inputdata_t &data)
	{
		m_allowed_to_auto_regen_bool = true;
	}
	void InputDisableAutoRegen(inputdata_t &data)
	{
		m_allowed_to_auto_regen_bool = false;
	}

	void InputSetHullSizeSmall(inputdata_t &data);
	void InputSetHullSizeNormal(inputdata_t &data);

	void InputSetSalvoType_HomingVolley(inputdata_t &data);
	void InputSetSalvoType_LinedVolley(inputdata_t &data);
	void InputSetSalvoType_UmbrellaVolley(inputdata_t &data);
	void InputSetSalvoType_TwinMissilesDirect(inputdata_t &data);
	void InputSetSalvoType_TwinMissilesArched(inputdata_t &data);

	void InputDispatchPStormKidnapFx(inputdata_t &inputdata)
	{
		CEffectData	data;
		data.m_nEntIndex = entindex();
		data.m_flMagnitude = 16;
		data.m_flScale = random->RandomFloat(0.25f, 1.0f);
		data.m_bCustomColors = true;

		DispatchEffect("TeslaHitboxes", data);
	}

	bool m_enablechargeattack_bool;
	bool m_enableminigunattack_bool;
	bool m_enablesalvoattack_bool;
	void InputEnableSalvoAttack(inputdata_t &data);
	void InputEnableChargeAttack(inputdata_t &data);
	void InputEnableMinigunAttack(inputdata_t &data);
	void InputDisableSalvoAttack(inputdata_t &data);
	void InputDisableChargeAttack(inputdata_t &data);
	void InputDisableMinigunAttack(inputdata_t &data);
	void InputDoBodyThrowTowardTarget(inputdata_t &data);

	void InputSetMinigunFiringDuration(inputdata_t &data);
	void InputSetMinigunFiringPause(inputdata_t &data);

	void InputSetSalvoSize(inputdata_t &data);
	void InputSetSalvoNextFireDelay(inputdata_t &data);

	void InputForceStartMinigunAttack(inputdata_t &data);
	void InputForceStartPulsegunAttack(inputdata_t &data);

	void InputUnfoldGuns(inputdata_t &data);
	void InputFoldGuns(inputdata_t &data);

	COutputEvent m_OnFiredRockets;
	COutputEvent m_OnStartedSalvo;
	COutputEvent m_OnFinishedSalvo;
	COutputEvent m_OnStartedMinigun;
	COutputEvent m_OnFinishedMinigun;
	COutputEvent m_OnStartedCharge;
	COutputEvent m_OnStartedRegen;
	COutputEvent m_OnFinishedRegen;
	COutputEvent m_OnReachedLowHealthThreshold;

private:
	float m_nextcharge_time;
	int m_iSalvoAttacksCounter;
	int m_nRocketSide;
	EHANDLE m_hSpecificRocketTarget;

	int m_salvotype_int;
	int m_salvosize_int;
	float m_salvonextfiredelay_float;

	float m_chargeminigundelay_time;

	float m_fireminigunperiod_time;
	bool  m_minigunreadytofire_bool;

	float m_minigunduration_kv;
	float m_minigunpause_kv; 

	bool m_gun_platform_lowered_bool;	// this is switched on via animevent. 
										// When on, TranslateActivity will replace ACT_WALK with ACT_WALK_AIM,
										// ACT_RUN with ACT_RUN_AIM and so on, where _AIM activities use the anims
										// with the gun platform hanging out.
										// Switched off via another animevent when the platform's put back into the body.

	float m_regen_duration_time;
	CHandle<CPropScalable> m_hShieldEntity;

	float m_crabsynthIdleSound_time;
	float m_crabsynthLaughSound_time;
	float m_crabsynthLaughSpecialSound_time;
	int m_iShockWaveTexture;

	int m_max_auto_regen_times_int;
	float m_auto_regen_health_threshold_float;
	float m_auto_regen_duration_float;
	bool m_allowed_to_auto_regen_bool;

	bool m_played_shield_warning_bool;
	bool m_played_demented_fluffing_hack_bool;

#ifdef RIDEABLE_NPCS
	float	m_nextTouchTime;
	Vector	m_savedViewOffset;

	// IDrivableVehicle
public:
	virtual CBaseEntity *GetDriver(void) { return m_hPlayer; }
	virtual void		ItemPostFrame(CBasePlayer *pPlayer)
	{
	/*	if (!m_isHatchOpen && (pPlayer->m_afButtonPressed & IN_USE))
		{
			OpenHatch();
		}
		else*/ if (pPlayer->m_afButtonPressed & IN_JUMP)
		{
			pPlayer->LeaveVehicle();
			m_nextTouchTime = CURTIME + 2; // debounce
		}

		// Prevent the base class using the use key
		pPlayer->m_afButtonPressed &= ~IN_USE;
	}
	virtual void		ProcessMovement(CBasePlayer *pPlayer, CMoveData *pMoveData) { return; }
	virtual void		SetupMove(CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move);
	virtual void		FinishMove(CBasePlayer *player, CUserCmd *ucmd, CMoveData *move) { return; }
	virtual bool		CanEnterVehicle(CBaseEntity *pEntity) { return true; }
	virtual bool		CanExitVehicle(CBaseEntity *pEntity) { return true; }
	virtual void		EnterVehicle(CBaseCombatCharacter *pPlayer);
	virtual void		ExitVehicle(int iRole);
	virtual bool		PlayExitAnimation(void) { return false; }

	//IDrivableVehicle's Pure Virtuals
	virtual void		SetVehicleEntryAnim(bool bOn) { }
	virtual void		SetVehicleExitAnim(bool bOn, Vector vecEyeExitEndpoint) { }

	virtual bool		AllowBlockedExit(CBaseCombatCharacter *pPassenger, int nRole) { return true; }
	virtual bool		AllowMidairExit(CBaseCombatCharacter *pPassenger, int nRole) { return false; }
	virtual void		PreExitVehicle(CBaseCombatCharacter *pPassenger, int nRole) {}

	virtual string_t GetVehicleScriptName() { return NULL_STRING; /*m_vehicleScript;*/ }
	virtual bool		PassengerShouldReceiveDamage(CTakeDamageInfo &info) { return false; }

	// If this is a vehicle, returns the vehicle interface
	virtual IServerVehicle	*GetServerVehicle() { return &m_ServerVehicle; }

	CNetworkHandle(CBasePlayer, m_hPlayer);
//	bool	m_isHatchOpen;
//	void	OpenHatch();
//	void	InputOpenHatch(inputdata_t &input);
	void	InputEnterCrabSynthVehicle(inputdata_t &input);

protected:
	// Contained IServerVehicle
	CCrabSynthServerVehicle	m_ServerVehicle;
#endif

public:	
	CNetworkVar(bool, m_crabsynth_shield_enabled); // turn on, turn off

	enum
	{
		COND_CRABSYNTH_ENTER_REGEN = LAST_SHARED_SCHEDULE,
	};

	enum
	{
		SCHED_CRABSYNTH_ATTACK_ROCKETS = LAST_SHARED_SCHEDULE,
		SCHED_CRABSYNTH_ATTACK_SALVO,
		SCHED_CRABSYNTH_ATTACK_CHARGE,
		SCHED_CRABSYNTH_ATTACK_MINIGUN,
		SCHED_CRABSYNTH_STRAFE_DURING_PAUSE,
		SCHED_CRABSYNTH_RUN_RANDOM_256,
		SCHED_CRABSYNTH_ENTER_REGEN,
		SCHED_CRABSYNTH_BODYTHROW,
		SCHED_CRABSYNTH_UNFOLD_WEAPONS, // told to play the gun-unfolding anim via input
		SCHED_CRABSYNTH_FOLD_WEAPONS, // told to play the gun-folding anim via input
		SCHED_CRABSYNTH_ATTACK_PULSEGUN, // told to play the pulsegun anim via input
	//	NEXT_SCHEDULE,
	};

	enum
	{
		TASK_CRABSYNTH_SHOOT_ROCKETS = LAST_SHARED_TASK,
		TASK_CRABSYNTH_START_SALVO,
		TASK_CRABSYNTH_STOP_SALVO,
		TASK_CRABSYNTH_CHARGE,
		TASK_CRABSYNTH_CHARGE_MINIGUN,
		TASK_CRABSYNTH_LOOP_MINIGUN,
		TASK_CRABSYNTH_STOP_MINIGUN,
		TASK_CRABSYNTH_ANNOUNCE_REGEN,
		TASK_CRABSYNTH_ENTER_REGEN,
		TASK_CRABSYNTH_REGEN_WAIT,
		TASK_CRABSYNTH_UNFOLD_WEAPONS,
		TASK_CRABSYNTH_FOLD_WEAPONS,
	//	TASK_CRABSYNTH_START_BEAM,
	//	TASK_CRABSYNTH_STOP_BEAM,
	};

	enum
	{
		CRABSYNTH_SALVO_TYPE_HOMINGVOLLEY = 0,
		CRABSYNTH_SALVO_TYPE_LINEDVOLLEY,
		CRABSYNTH_SALVO_TYPE_UMBRELLAVOLLEY,
		CRABSYNTH_SALVO_TYPE_TWINMISSILESDIRECT,
		CRABSYNTH_SALVO_TYPE_TWINMISSILESARCHED,
	};
};

Activity ACT_CRABSYNTH_ROCKETS_START;
Activity ACT_CRABSYNTH_ROCKETS_END;
Activity ACT_CRABSYNTH_CUE;
Activity ACT_CRABSYNTH_MINIGUN_START;
Activity ACT_CRABSYNTH_MINIGUN_LOOP;
Activity ACT_CRABSYNTH_MINIGUN_GESTURE;
Activity ACT_CRABSYNTH_MINIGUN_END;
Activity ACT_CRABSYNTH_BODY_THROW;
Activity ACT_CRABSYNTH_UNFOLD_WEAPONS;
Activity ACT_CRABSYNTH_FOLD_WEAPONS;
Activity ACT_CRABSYNTH_PULSE_FIRE;

int AE_CRABSYNTH_CHARGE_START;
int AE_CRABSYNTH_CHARGE_END;
int AE_CRABSYNTH_MINIGUN_FIRE;
int AE_CRABSYNTH_MELEE_PIN;
int AE_CRABSYNTH_MELEE_SWIPE;
int AE_CRABSYNTH_PULSEGUN_FIRE;
int AE_CRABSYNTH_UNFOLDED_GUNS;
int AE_CRABSYNTH_FOLDED_GUNS;

#define CRABSYNTH_NEW_MINIGUN 1
#define SF_CRABSYNTH_NO_DEATH_SMOKE 1<<16

#define MYMODEL "models/_Monsters/Combine/crabsynth.mdl"

// these values are more or less for range of 1000-1500 units between crab and player.
//ConVar npc_crabsynth_salvo_size("npc_crabsynth_salvo_size", "6");
ConVar npc_crabsynth_salvo_delay("npc_crabsynth_salvo_delay", "0.12");
ConVar npc_crabsynth_salvo_homing_amount("npc_crabsynth_salvo_homing_amount", "40"); //60
ConVar npc_crabsynth_salvo_homing_rate("npc_crabsynth_salvo_homing_rate", "800");
ConVar npc_crabsynth_salvo_speed("npc_crabsynth_salvo_speed", "500");
ConVar npc_crabsynth_salvo_gravity("npc_crabsynth_salvo_gravity", "2.4"); // 1
ConVar npc_crabsynth_salvo_spin("npc_crabsynth_salvo_spin", "1.0");

ConVar npc_crabsynth_minigun_windup("npc_crabsynth_minigun_windup", "1.7");	

//ConVar npc_crabsynth_salvo_mode("npc_crabsynth_salvo_mode", "0", 0, "0 = homing salvos (fly at player position, low gravity); 1 = frontal bombardment (fall and explode in a line between the crab and the player, low homing, high gravity); 2 = umbrella cover bombardment (fly out simultaneously, fall down, explode in a circle around the crab); 3 = twin rockets fired horizontally; 4 = twin rockets fired vertically");

//ConVar npc_crabsynth_salvo_x("npc_crabsynth_salvo_x", "-0.3f", FCVAR_ARCHIVE);
//ConVar npc_crabsynth_salvo_y("npc_crabsynth_salvo_y", "0.3f", FCVAR_ARCHIVE);
//ConVar npc_crabsynth_salvo_z("npc_crabsynth_salvo_z", "1.0f", FCVAR_ARCHIVE);

CNPC_CrabSynth::CNPC_CrabSynth() : m_hShieldEntity(NULL)
{
	m_pEngineSound = NULL;
	m_pEngineSound_Hurt = NULL;
	m_crabsynth_shield_enabled = false;
	m_max_auto_regen_times_int = 1;
	m_allowed_to_auto_regen_bool = 1;
	m_auto_regen_duration_float = 15;
	m_auto_regen_health_threshold_float = 0.5;
	m_minigunreadytofire_bool = false;
	m_gun_platform_lowered_bool = false;
	m_minigunduration_kv = 5;
	m_minigunpause_kv = 5;
#ifdef RIDEABLE_NPCS
	m_ServerVehicle.SetVehicle(this);
#endif
}

CNPC_CrabSynth::~CNPC_CrabSynth()
{
	if (m_hShieldEntity) UTIL_Remove(m_hShieldEntity);
}

void CNPC_CrabSynth::Precache(void)
{
	PrecacheModel(MYMODEL);
	PrecacheModel("models/_Monsters/Combine/crabsynth_shell.mdl");
	PrecacheScriptSound("NPC_CrabSynth.SalvoSingleFire");

	m_iShockWaveTexture = PrecacheModel("custom_uni/shockwave_beam.vmt");

	PrecacheScriptSound("NPC_CrabSynth.Idle");
	PrecacheScriptSound("NPC_CrabSynth.Alert");
	PrecacheScriptSound("NPC_CrabSynth.Pain");
	PrecacheScriptSound("NPC_CrabSynth.Death");
	PrecacheScriptSound("NPC_CrabSynth.EngineLoop");
	PrecacheScriptSound("NPC_CrabSynth.EngineLoopHurt");
	PrecacheScriptSound("NPC_CrabSynth.AttackCue");
	PrecacheScriptSound("NPC_CrabSynth.Charge");
	PrecacheScriptSound("NPC_CrabSynth.UnfoldWeapons");
	PrecacheScriptSound("NPC_CrabSynth.FoldWeapons");
	PrecacheScriptSound("NPC_CrabSynth.MinigunCharge");
	PrecacheScriptSound("NPC_CrabSynth.MinigunWindup");
	PrecacheScriptSound("NPC_CrabSynth.MinigunFire");
	PrecacheScriptSound("NPC_CrabSynth.MinigunStop");
	PrecacheScriptSound("NPC_CrabSynth.PulsegunFire");
	PrecacheScriptSound("NPC_CrabSynth.Laugh");
	PrecacheScriptSound("NPC_CrabSynth.LaughSpecial");
	PrecacheScriptSound("NPC_CrabSynth.ArmorPenetration"); // loud crack/pop of armour being penetrated by 357, OICW...
	PrecacheScriptSound("NPC_CrabSynth.AnnounceRegen"); // some kind of alarm right before entering regen state
	PrecacheScriptSound("NPC_CrabSynth.CreateRegenShield"); // energy sound of creating the forcefield
	PrecacheScriptSound("NPC_CrabSynth.RegenShieldLoop"); // some kind of energy hum if we want it
	PrecacheScriptSound("NPC_CrabSynth.CollapseRegenShield"); // the sound of shield collapsing and emitting an energy wave
	PrecacheScriptSound("NPC_CrabSynth.PreCollapseRegenShield"); // if we want some announcements that the shield collapse is imminent (3 secs before disabling it)
	PrecacheScriptSound("NPC_CrabSynth.BodyThrow");
	PrecacheScriptSound("NPC_CrabSynth.BodyThrowLand");
	// TEMP
	PrecacheScriptSound("NPC_AntlionGuard.ShellCrack");
	PrecacheScriptSound("NPC_AntlionGuard.Pain_Roar");
	PrecacheScriptSound("NPC_AntlionGuard.Shove");
	PrecacheScriptSound("NPC_AntlionGuard.HitHard");
	PrecacheScriptSound("LoudSpark");
	PrecacheParticleSystem("wastelandscanner_shield_impact");
	PrecacheParticleSystem("apc_muzzleflash");
	PrecacheParticleSystem("Weapon_Combine_Ion_Cannon"); // for pulse gun
	PrecacheParticleSystem("impact_concrete"); // for minigun impacts
	PrecacheParticleSystem("crabsynth_armor_penetration"); // for indicating armour is being penetrated; also has blood
	PrecacheScriptSound("NPC_CombineGunship.Explode");
	//

	PrecacheModel("models/_Monsters/Combine/gunship_projectile.mdl");
	UTIL_PrecacheOther("grenade_homer");
//	UTIL_PrecacheOther("hunter_flechette");
	BaseClass::Precache();
}

void CNPC_CrabSynth::Spawn(void)
{
	Precache();
	SetModel(MYMODEL);
	SetHullType(HULL_CRABSYNTH);
	SetHullSizeNormal();
	SetDefaultEyeOffset();

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MOVETYPE_STEP);
	SetBloodColor(DONT_BLEED); // MICROTEST ARENA CHANGE - by default no blood, emit it manually in TraceAttack
	m_iHealth = m_iMaxHealth = 1000;
	m_flFieldOfView = 0.5f;
	SetDistLook(2500.0f);
	m_NPCState = NPC_STATE_NONE;

	m_crabsynthIdleSound_time = CURTIME;
	m_crabsynthLaughSound_time = CURTIME;
	m_crabsynthLaughSpecialSound_time = CURTIME;

	m_played_shield_warning_bool = 0;
	m_played_demented_fluffing_hack_bool = 0;

	m_enablechargeattack_bool = false;
	m_enableminigunattack_bool = true;
	m_enablesalvoattack_bool = true;

	m_regen_duration_time = CURTIME;
	m_nextcharge_time = CURTIME;
	m_iSalvoAttacksCounter = 0;
	m_nRocketSide = 0;
	m_salvosize_int = 5; // default
	m_salvotype_int = 0; // default, Homing Volley
	m_salvonextfiredelay_float = 1.5f; // will define next attack time for salvo launches
	m_chargeminigundelay_time = CURTIME + 0.5f;
	m_fireminigunperiod_time = CURTIME + 0.5f;

	RegisterThinkContext("SalvoAttackContext");
	RegisterThinkContext("MinigunAttackContext");
	RegisterThinkContext("RegenContext");

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
//	CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
//	CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);
	CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1); // claws
	CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2); // charge

#ifdef RIDEABLE_NPCS
	SetTouch(&CNPC_CrabSynth::PlayerTouch);
//	SetBodygroup(1, 0); // group 1 = intact vent cover, value 0 = cover exists
#endif
	
	NPCInit();
}

void CNPC_CrabSynth::PostNPCInit()
{
	BaseClass::PostNPCInit();

	if (CarriedByDropship())
	{
		SetMoveType(MOVETYPE_NONE);
		SetActivity((Activity)ACT_CARRIED_BY_DROPSHIP);
		SetThink(&CNPC_CrabSynth::CarriedThink);
		RemoveFlag(FL_FLY);
	}
}

void CNPC_CrabSynth::NPCThink(void)
{
	BaseClass::NPCThink();

	if (m_allowed_to_auto_regen_bool && m_max_auto_regen_times_int > 0 && !HasCondition(COND_CRABSYNTH_ENTER_REGEN) && !m_crabsynth_shield_enabled)
	{
		if (m_iHealth > 0 && m_iMaxHealth > 0)
		{
			float hpRatio = (float)m_iHealth / (float)m_iMaxHealth;
			if (hpRatio <= m_auto_regen_health_threshold_float)
			{
				m_OnReachedLowHealthThreshold.FireOutput(this, this);
				ClearSchedule("forced input");
				SetCondition(COND_CRABSYNTH_ENTER_REGEN);
			}
		}
	}
}

void CNPC_CrabSynth::OnRestore()
{
	BaseClass::OnRestore();

	if (CURTIME < m_regen_duration_time)
	{
		SetContextThink(&CNPC_CrabSynth::RegenThink, CURTIME + 0.1f, "RegenContext");
	}
}

bool CNPC_CrabSynth::CreateRegenShield( float flDur )
{
//	if (m_hShieldEntity)
//	{
//		DevMsg("Already have a shield entity, not creating another!\n");
//		return true;
//	}
	m_hShieldEntity = (CPropScalable*)CreateEntityByName("prop_scalable");
	if (m_hShieldEntity)
	{
		m_hShieldEntity->SetModel("models/_Monsters/Combine/crabsynth_shell.mdl");
		m_hShieldEntity->SetAbsOrigin(GetAbsOrigin());
		m_hShieldEntity->SetAbsVelocity(GetAbsVelocity());
		m_hShieldEntity->SetLocalAngularVelocity(GetLocalAngularVelocity());
		m_hShieldEntity->SetParent(this);
		//	shield->SetParentAttachment("SetParentAttachment", "origin", false);
		m_hShieldEntity->SetOwnerEntity(this);
		m_hShieldEntity->Spawn();

		variant_t varX, varY, varZ;
		varX.SetVector3D(Vector(0.1, 0.0, 0));
		varY.SetVector3D(Vector(0.1, 0.0, 0));
		varZ.SetVector3D(Vector(0.1, 0.0, 0));
		m_hShieldEntity->AcceptInput("SetScaleX", this, this, varX, 0);
		m_hShieldEntity->AcceptInput("SetScaleY", this, this, varY, 0);
		m_hShieldEntity->AcceptInput("SetScaleZ", this, this, varZ, 0);

	//	if (!enable_shield_model.GetBool()) m_hShieldEntity->SetRenderMode(kRenderNone);
		m_hShieldEntity->SetSequence(SelectWeightedSequence(ACT_IDLE_STEALTH));

		varX.SetVector3D(Vector(1, 0.4, 0));
		varY.SetVector3D(Vector(1, 0.4, 0));
		varZ.SetVector3D(Vector(1, 0.75, 0));
		m_hShieldEntity->AcceptInput("SetScaleX", this, this, varX, 0);
		m_hShieldEntity->AcceptInput("SetScaleY", this, this, varY, 0);
		m_hShieldEntity->AcceptInput("SetScaleZ", this, this, varZ, 0);
	}

	if (m_hShieldEntity)
	{
		EmitSound("NPC_CrabSynth.CreateRegenShield");
		m_crabsynth_shield_enabled = true;
		m_regen_duration_time = CURTIME + flDur; 
		SetBloodColor(DONT_BLEED); // temporarily prevent blood impacts
		RemoveAllDecals();
		SetContextThink(&CNPC_CrabSynth::RegenThink, CURTIME + 0.1f, "RegenContext");
		if( m_allowed_to_auto_regen_bool )
			--m_max_auto_regen_times_int;
		m_OnStartedRegen.FireOutput(this, this);
		return true;
	}

	return false;
}

void CNPC_CrabSynth::CollapseRegenShield(void)
{
	if (m_hShieldEntity)
	{
		variant_t var;
		var.SetVector3D(Vector(0.1, 0.5, 0));
		m_hShieldEntity->AcceptInput("SetScaleX", this, this, var, 0);
		m_hShieldEntity->AcceptInput("SetScaleY", this, this, var, 0);
		m_hShieldEntity->AcceptInput("SetScaleZ", this, this, var, 0);
		m_hShieldEntity->SetThink(&CBaseEntity::SUB_Remove);
		m_hShieldEntity->SetNextThink(CURTIME + 0.5f);
	}
	else
	{
		Msg("No shield entity was found\n");
	}

	// DI HACK: find ALL existing prop scalable crab shields and delete them.
	// this will prevent the bug we have right now (Jan 2024) with shield twinning.
	// this will do for now because we only have one active crab at a time.
	// in the future it needs something better if we ever to have arenas with
	// more than one crab, so they don't interefere with one another.
	CBaseEntity *redundantShields = NULL;
	while ((redundantShields = gEntList.FindEntityByModel(redundantShields, "models/_Monsters/Combine/crabsynth_shell.mdl")) != NULL)
	{
		redundantShields->SetThink(&CBaseEntity::SUB_Remove);
		redundantShields->SetNextThink(CURTIME + 0.5f);
	}
	//

	// emit energy wave // TEMP
	EmitSound("NPC_CrabSynth.CollapseRegenShield");

	CBroadcastRecipientFilter filter;
	te->BeamRingPoint(filter, 0.0,
		GetAbsOrigin() + Vector(0,0,4),							//origin
		16,										//start radius
		400,//end radius
		m_iShockWaveTexture,						//texture
		0,										//halo index
		0,										//start frame
		0,										//framerate
		0.2,									//life
		8,										//width
		16,										//spread
		0,										//amplitude
		255,									//r
		255,									//g
		255,									//b
		192,									//a
		0										//speed
	); 
	
	CBroadcastRecipientFilter filter2;
	te->BeamRingPoint(filter2, 0.0,
		GetAbsOrigin() + Vector(0,0,32),							//origin
		16,										//start radius
		200,//end radius
		m_iShockWaveTexture,						//texture
		0,										//halo index
		0,										//start frame
		0,										//framerate
		0.2,									//life
		8,										//width
		16,										//spread
		0,										//amplitude
		255,									//r
		255,									//g
		255,									//b
		192,									//a
		0										//speed
	);

	CBroadcastRecipientFilter filter3;
	te->BeamRingPoint(filter3, 0.0,
		GetAbsOrigin() + Vector(0, 0, 64),							//origin
		16,										//start radius
		100,//end radius
		m_iShockWaveTexture,						//texture
		0,										//halo index
		0,										//start frame
		0,										//framerate
		0.2,									//life
		8,										//width
		16,										//spread
		0,										//amplitude
		255,									//r
		255,									//g
		255,									//b
		192,									//a
		0										//speed
	);

	CBroadcastRecipientFilter filter6;
	te->BeamRingPoint(filter6, 0.0,
		GetAbsOrigin(),									//origin
		16,												//start radius
		400 / 2,											//end radius
		m_iShockWaveTexture,								//texture
		0,												//halo index
		0,												//start frame
		0,												//framerate
		0.2,											//life
		8,												//width
		4,												//spread
		0,												//amplitude
		255,											//r
		255,											//g
		255,											//b
		192,											//a
		0												//speed
	);
	
	UTIL_ScreenShake(GetAbsOrigin(), 32.0f, 8.0f, 0.5f, 512, SHAKE_START);

	SetBloodColor(BLOOD_COLOR_GREEN); // restore our blood color for impact fx
	RemoveAllDecals();

	m_crabsynth_shield_enabled = false;
	m_played_demented_fluffing_hack_bool = false;
	m_played_shield_warning_bool = false;
	m_regen_duration_time = CURTIME + 0.5f;
	m_flNextAttack = CURTIME + 2;
	ClearCondition(COND_CRABSYNTH_ENTER_REGEN);
	ClearSchedule("forced input");
	m_OnFinishedRegen.FireOutput(this, this);
}

bool CNPC_CrabSynth::IsHeavyDamage(const CTakeDamageInfo &info)
{
	if (m_crabsynth_shield_enabled) return false;

	// Struck by blast
	if (info.GetDamageType() & DMG_BULLET)
	{
		if (GetAmmoDef()->ArmorMitigation(info.GetAmmoType()) >= 1.0f)
			return true;
		else
			return false;
	}
	
	return BaseClass::IsHeavyDamage(info);
}

void CNPC_CrabSynth::TraceAttack(const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr )
{
	CTakeDamageInfo info = inputInfo;

	if (info.GetDamageType() & (DMG_SLASH | DMG_CLUB))
	{
		if (m_crabsynth_shield_enabled)
		{
			EmitSound("NPC_SynthScanner.ShieldImpact");
		//	QAngle vecAngles;
		//	VectorAngles(-vecDir, vecAngles);
		//	DispatchParticleEffect("warp_shield_impact", ptr->endpos, vecAngles);

			// MICROTEST ARENA CHANGE - show shield fx like on the scanners
			QAngle vecAngles;
			Vector traceDir = ptr->endpos - ptr->startpos;
			VectorNormalize(traceDir);
			VectorAngles(-vecDir, vecAngles);
			vecAngles[1] += 90;
			DispatchParticleEffect("wastelandscanner_shield_impact", ptr->endpos - traceDir * 16, vecAngles);
		}
		else
		{
			g_pEffects->Ricochet(ptr->endpos, (vecDir*-1.0f));
			if (m_crabsynthLaughSound_time <= CURTIME)
			{
				EmitSound("NPC_Crabsynth.Laugh");
				m_crabsynthLaughSound_time = CURTIME + RandomFloat(2.0f, 6.0f);
			}
		}
		info.SetDamage(0.01);
	}

	if (info.GetDamageType() & (DMG_BULLET))
	{
		if (m_crabsynth_shield_enabled)
		{
			EmitSound("NPC_SynthScanner.ShieldImpact");
		//	QAngle vecAngles;
		//	VectorAngles(-vecDir, vecAngles);
		//	DispatchParticleEffect("warp_shield_impact", ptr->endpos, vecAngles);

			// MICROTEST ARENA CHANGE - show shield fx like on the scanners
			QAngle vecAngles;
			Vector traceDir = ptr->endpos - ptr->startpos;
			VectorNormalize(traceDir);
			VectorAngles(-vecDir, vecAngles);
			vecAngles[1] += 90;
			DispatchParticleEffect("wastelandscanner_shield_impact", ptr->endpos - traceDir * 16, vecAngles);

			info.SetDamage(0.01);
		}
		else
		{
			//if (GetAmmoDef()->ArmorMitigation(info.GetAmmoType()) >= 1.0f)
			if( IsHeavyDamage(info))
			{
				info.ScaleDamage(0.4 * GetAmmoDef()->ArmorMitigation(info.GetAmmoType()));

				// MICROTEST ARENA CHANGE - emit blood manually if broke through armour, npc has DONT_BLEED for other cases
				QAngle vecAngles;
				Vector traceDir = ptr->endpos - ptr->startpos;
				VectorNormalize(traceDir);
				VectorAngles(-vecDir, vecAngles);
				vecAngles[0] += 90;
				DispatchParticleEffect("crabsynth_armor_penetration", ptr->endpos, vecAngles);
			}
			else
			{
				g_pEffects->Ricochet(ptr->endpos, (vecDir*-1.0f));
				info.SetDamage(0.01);
				if (m_crabsynthLaughSound_time <= CURTIME)
				{
					EmitSound("NPC_Crabsynth.Laugh");
					m_crabsynthLaughSound_time = CURTIME + RandomFloat(2.0f, 6.0f);
				}
			}			
		}
	}

	BaseClass::TraceAttack(info, vecDir, ptr );
}

int CNPC_CrabSynth::OnTakeDamage_Alive(const CTakeDamageInfo &inputInfo)
{
	CTakeDamageInfo info = inputInfo;

	if (m_crabsynth_shield_enabled)
	{
		info.SetDamage(0.01);
	}

	else
	{
		if (IsHeavyDamage(info))
		{
			// Always take a set amount of damage from a combine ball
			if (info.GetInflictor() && UTIL_IsCombineBall(info.GetInflictor()))
			{
				info.SetDamage(50);
			}

			// Set our response animation
		//	SetHeavyDamageAnim(info.GetDamagePosition());

			// Explosive barrels don't carry through their attacker, so this 
			// condition isn't set, and we still want to flinch. So we set it.
			SetCondition(COND_HEAVY_DAMAGE);

		//	CEffectData data;
		//	data.m_vOrigin = info.GetDamagePosition();
		//	data.m_vNormal = -info.GetDamageForce();
		//	VectorNormalize(data.m_vNormal);
		//	DispatchEffect("CrabsynthAPDamage", data); // decided to go with a particle system in TraceAttack; this fx is too much

			// Play a sound for a physics impact
			if (info.GetDamageType() & DMG_CRUSH)
			{
				EmitSound("NPC_AntlionGuard.ShellCrack");
			}

			// Play a sound feedback of our armour being penetrated
			if (info.GetDamageType() & DMG_BULLET)
			{
				EmitSound("NPC_Crabsynth.ArmorPenetration");
			}

			// Roar in pain
			if (m_flNextPainSoundTime <= CURTIME)
			{
				UTIL_ScreenShake(GetAbsOrigin(), 32.0f, 8.0f, 0.5f, 512, SHAKE_START);
				EmitSound("NPC_Crabsynth.Pain");
				m_flNextPainSoundTime = CURTIME + RandomFloat(1.3, 2.0);
			}

			if (info.GetDamageType() & DMG_BLAST)
			{
				// Create the dust effect in place
				CParticleSystem *pParticle = (CParticleSystem *)CreateEntityByName("info_particle_system");
				if (pParticle != NULL)
				{
					// Setup our basic parameters
					pParticle->KeyValue("start_active", "1");
					pParticle->KeyValue("effect_name", "burning_engine_01");
					pParticle->SetParent(this);
					pParticle->SetParentAttachment("SetParentAttachment", "smoke_origin", true);
					pParticle->SetAbsOrigin(GetAbsOrigin());
					pParticle->SetLocalOrigin(Vector(-16, 24, 0));
					DispatchSpawn(pParticle);
					if (CURTIME > 0.5f)
						pParticle->Activate();

					pParticle->SetThink(&CBaseEntity::SUB_Remove);
					pParticle->SetNextThink(CURTIME + random->RandomFloat(2.0f, 3.0f));
				}
			}
		}
	}

	return BaseClass::OnTakeDamage_Alive(info);
}

void CNPC_CrabSynth::Event_Killed(const CTakeDamageInfo &info)
{
#ifdef RIDEABLE_NPCS
	if (m_hPlayer.Get())
	{
		m_hPlayer->LeaveVehicle();
	}
#endif
	StopLoopingSounds();
	BaseClass::Event_Killed(info);
}

bool CNPC_CrabSynth::BecomeRagdoll(const CTakeDamageInfo &info, const Vector &forceVector)
{
	StopLoopingSounds();
	
	CRagdollProp *pRagdoll = NULL;
	pRagdoll = assert_cast<CRagdollProp *>(CreateServerRagdoll(this, m_nForceBone, info, HL2COLLISION_GROUP_STRIDER));
	pRagdoll->DisableAutoFade();

	if (pRagdoll && !HasSpawnFlags(SF_CRABSYNTH_NO_DEATH_SMOKE))
	{
		// start smoke coming out of ragdoll
		// Create the dust effect in place
		CParticleSystem *pParticle = (CParticleSystem *)CreateEntityByName("info_particle_system");
		if (pParticle != NULL)
		{
			// Setup our basic parameters
			pParticle->KeyValue("start_active", "1");
			pParticle->KeyValue("effect_name", "burning_engine_01");
			pParticle->SetParent(pRagdoll);
			pParticle->SetParentAttachment("SetParentAttachment", "smoke_origin", true);
			pParticle->SetLocalOrigin(Vector(0, 0, 24));
			DispatchSpawn(pParticle);
			if (CURTIME > 0.5f)
				pParticle->Activate();

			pParticle->SetThink(&CBaseEntity::SUB_Remove);
			pParticle->SetNextThink(CURTIME + random->RandomFloat(30.0f, 50.0f));
		}

		CEffectData	data;
		data.m_nEntIndex = pRagdoll->entindex();
		data.m_flMagnitude = 16;
		data.m_flScale = random->RandomFloat(0.25f, 1.0f);

		DispatchEffect("TeslaHitboxes", data);
		EmitSound("LoudSpark");

		DispatchParticleEffect("Advisor_Pod_Explosion_Debris", pRagdoll->GetAbsOrigin(), QAngle(-90, 0, 0), pRagdoll);
		DispatchParticleEffect("electrical_arc_01_system", pRagdoll->GetAbsOrigin() + RandomVector(-32,32), QAngle(-90, 0, 0), pRagdoll);
		DispatchParticleEffect("electrical_arc_01_system", pRagdoll->GetAbsOrigin() + RandomVector(-32, 32), QAngle(-90, 0, 0), pRagdoll);
		DispatchParticleEffect("electrical_arc_01_system", pRagdoll->GetAbsOrigin() + RandomVector(-32, 32), QAngle(-90, 0, 0), pRagdoll);
	}
	
	// Create some explosions on the gunship body
	Vector vecDelta;
	for (int i = 0; i < 3; i++)
	{
		vecDelta = RandomVector(-32, 32);
		ExplosionCreate(pRagdoll->GetAbsOrigin() + vecDelta, QAngle(-90, 0, 0), this, 10, 10, false);
	}

	AR2Explosion *pExplosion = AR2Explosion::CreateAR2Explosion(GetAbsOrigin());
	if (pExplosion)
	{
		pExplosion->SetLifetime(10);
	}

	// Get rid of our old body
	UTIL_Remove(this);

	return true;
}

#ifdef RIDEABLE_NPCS
void CNPC_CrabSynth::PlayerTouch(CBaseEntity *pOther)
{
	// debounce players who just jumped off
	if (CURTIME < m_nextTouchTime)
		return;

	CBasePlayer *pPlayer = ToBasePlayer(pOther);
	if (!pPlayer)
		return;

	if ( IRelationType(pOther) == D_HT ) // ignore players that we're hostile toward
		return;

	if (m_hPlayer.Get() != NULL && m_hPlayer.Get() == pPlayer)
		return; // ignore players who are already riding us

	if (pPlayer->IsInAVehicle())
		return; // ignore players who bumped into us in a vehicle

	Vector mins, maxs;
	mins = WorldAlignMins() + GetAbsOrigin();
	maxs = WorldAlignMaxs() + GetAbsOrigin();
	mins += Vector(8, 8, 8);
	maxs -= Vector(8, 8, 8);
	float height = maxs.z - mins.z;
	mins.z += height;
	maxs.z += height;

	// did player land on top?
	//	if (!IsPointInBox(pPlayer->GetAbsOrigin(), mins, maxs))
	if (pPlayer->EyePosition().z < WorldAlignMaxs().z)
		return; // no.

	pPlayer->GetInVehicle(GetServerVehicle(), VEHICLE_ROLE_DRIVER);
#if 0
	static bool g_Draw = true;
	if (g_Draw)
	{
		Vector center = 0.5 * mins + 0.5 * maxs;
		NDebugOverlay::Box(center, mins - center, maxs - center, 255, 0, 0, 32, 10);
		g_Draw = false;
	}
	Msg("STRIDER KILL!\n");
#endif
}
#endif

void CNPC_CrabSynth::IdleSound(void)
{
	if( !m_crabsynth_shield_enabled && !HasSpawnFlags(SF_NPC_GAG))
	{
		if (m_NPCState == NPC_STATE_IDLE)
		{
			EmitSound("NPC_CrabSynth.Idle");
			m_crabsynthIdleSound_time = CURTIME + RandomFloat(3.0f, 10.0f);
		}
	}
}

void CNPC_CrabSynth::AlertSound(void)
{
	if (!m_crabsynth_shield_enabled)
	{
		EmitSound("NPC_CrabSynth.Alert");
	}
}

void CNPC_CrabSynth::DeathSound(const CTakeDamageInfo &info)
{
	EmitSound("NPC_CombineGunship.Explode");
//	EmitSound("NPC_CrabSynth.Death");
	StopLoopingSounds();
}

void CNPC_CrabSynth::StopLoopingSounds(void)
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if (m_pEngineSound != NULL)
	{
		controller.SoundDestroy(m_pEngineSound);
		m_pEngineSound = NULL;
	}

	if (m_pEngineSound_Hurt != NULL)
	{
		controller.SoundDestroy(m_pEngineSound_Hurt);
		m_pEngineSound_Hurt = NULL;
	}

	BaseClass::StopLoopingSounds();
}

void CNPC_CrabSynth::HandleAnimEvent(animevent_t *pEvent)
{
	if ((pEvent->event == AE_CRABSYNTH_CHARGE_START) || (pEvent->event == AE_CRABSYNTH_CHARGE_END))
	{
		CEffectData	data;
		data.m_nEntIndex = entindex();
		data.m_vOrigin = GetAbsOrigin();
		data.m_flScale = 64.0f;
		DispatchEffect("ThumperDust", data);
		UTIL_ScreenShake(GetAbsOrigin(), 5.0 * m_flPlaybackRate, m_flPlaybackRate, m_flPlaybackRate / 2, 256, SHAKE_START, false);
		return;
	}

	if ((pEvent->event == AE_CRABSYNTH_MINIGUN_FIRE))
	{
		FireMinigun();
		return;
	}

	if ((pEvent->event == AE_CRABSYNTH_PULSEGUN_FIRE))
	{
		FirePulsegun();
		return;
	}

	if ((pEvent->event == AE_CRABSYNTH_MELEE_SWIPE) || (pEvent->event == AE_CRABSYNTH_MELEE_PIN))
	{
		bool bIsSwipe = (pEvent->event == AE_CRABSYNTH_MELEE_SWIPE);
		CheckMeleeAttack(256, bIsSwipe ? QAngle(12.0f, 0.0f, 0.0f) : QAngle(4.0f, 0.0f, 0.0f), bIsSwipe ? Vector(-500.0f, 1.0f, 1.0f) : Vector(-250.0f, 1.0f, 1.0f), bIsSwipe);
	//	if (!bIsSwipe) // feels good on both attacks, actually
		{
			UTIL_ScreenShake(GetAbsOrigin(), 32.0f, 8.0f, 0.5f, 512, SHAKE_START, true);
		}
		return;
	}

	if (pEvent->event == AE_CRABSYNTH_FOLDED_GUNS)
	{
		m_gun_platform_lowered_bool = false;
		return;
	}

	if (pEvent->event == AE_CRABSYNTH_UNFOLDED_GUNS)
	{
		m_gun_platform_lowered_bool = true;
		return;
	}

	BaseClass::HandleAnimEvent(pEvent);
}

void CNPC_CrabSynth::CheckMeleeAttack(float distance, QAngle &viewPunch, Vector &shove, bool bIsSwipe)
{
	Vector vecForceDir;
 
	float damage = bIsSwipe ? 50 : 25;
	
	// Always hurt bullseyes for now
	if ((GetEnemy() != NULL) && GetEnemy()->IsNPC())
	{
		if (GetEnemy()->Classify() == CLASS_BULLSEYE)
		{
			vecForceDir = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
			CTakeDamageInfo info(this, this, damage, DMG_SLASH);
			CalculateMeleeDamageForce(&info, vecForceDir, GetEnemy()->GetAbsOrigin());
			GetEnemy()->TakeDamage(info);
			return;
		}
		else
		{
			damage *= 10; // NPCs take ten times more damage from melee - it looks really painful!
		}
	}

	CBaseEntity *pHurt = CheckTraceHullAttack(distance, -Vector(32, 32, 64), Vector(32, 32, 64), damage, DMG_SLASH, bIsSwipe ? 10 : 5.0f);

	if (pHurt)
	{
		vecForceDir = (pHurt->WorldSpaceCenter() - WorldSpaceCenter());
		
		CBasePlayer *pPlayer = ToBasePlayer(pHurt);

		if (pPlayer != NULL)
		{
			//Kick the player angles
			if (!(pPlayer->GetFlags() & FL_GODMODE) && pPlayer->GetMoveType() != MOVETYPE_NOCLIP)
			{
				pPlayer->ViewPunch(viewPunch);

				Vector	dir = pHurt->GetAbsOrigin() - GetAbsOrigin();
				VectorNormalize(dir);

				QAngle angles;
				VectorAngles(dir, angles);
				Vector forward, right;
				AngleVectors(angles, &forward, &right, NULL);

				//Push the target back
				pHurt->ApplyAbsVelocityImpulse(-right * shove[1] - forward * shove[0]);
			}
		}

		// Play a random attack hit sound
		if( bIsSwipe) EmitSound("NPC_AntlionGuard.Shove");
	}
}

float CNPC_CrabSynth::ChargeSteer()
{
	trace_t	tr;
	Vector	testPos, steer, forward, right;
	QAngle	angles;
	const float	testLength = m_flGroundSpeed * 0.15f;

	//Get our facing
	GetVectors(&forward, &right, NULL);

	steer = forward;

	const float faceYaw = UTIL_VecToYaw(forward);

	//Offset right
	VectorAngles(forward, angles);
	angles[YAW] += 45.0f;
	AngleVectors(angles, &forward);

	// Probe out
	testPos = GetAbsOrigin() + (forward * testLength);

	// Offset by step height
	Vector testHullMins = GetHullMins();
	testHullMins.z += (StepHeight() * 2);

	// Probe
	CrabSynthTraceHull_SkipPhysics(GetAbsOrigin(), testPos, testHullMins, 
		GetHullMaxs(), MASK_NPCSOLID, this, 
		COLLISION_GROUP_NONE, &tr, /*VPhysicsGetObject()->GetMass() * 0.5f*/ 1000.0f); // don't use our vphysics getobject. It may not exist if we're killed during charge.
	
	// Add in this component
	steer += (right * 0.5f) * (1.0f - tr.fraction);

	// Offset left
	angles[YAW] -= 90.0f;
	AngleVectors(angles, &forward);

	// Probe out
	testPos = GetAbsOrigin() + (forward * testLength);
	CrabSynthTraceHull_SkipPhysics(GetAbsOrigin(), testPos, testHullMins, 
		GetHullMaxs(), MASK_NPCSOLID, this, 
		COLLISION_GROUP_NONE, &tr, /*VPhysicsGetObject()->GetMass() * 0.5f*/ 1000.0f);

	// Add in this component
	steer -= (right * 0.5f) * (1.0f - tr.fraction);
	
	return UTIL_AngleDiff(UTIL_VecToYaw(steer), faceYaw);
}

void CNPC_CrabSynth::ChargeDamage(CBaseEntity *pTarget)
{
	if (pTarget == NULL)
		return;

	CBasePlayer *pPlayer = ToBasePlayer(pTarget);

	if (pPlayer != NULL)
	{
		//Kick the player angles
		pPlayer->ViewPunch(QAngle(20, 20, -30));

		Vector	dir = pPlayer->WorldSpaceCenter() - WorldSpaceCenter();
		VectorNormalize(dir);
		dir.z = 0.0f;

		Vector vecNewVelocity = dir * 250.0f;
		vecNewVelocity[2] += 128.0f;
		pPlayer->SetAbsVelocity(vecNewVelocity);

		color32 red = { 128,0,0,128 };
		UTIL_ScreenFade(pPlayer, red, 1.0f, 0.1f, FFADE_IN);

		EmitSound("NPC_AntlionGuard.Shove");

		if (m_crabsynthLaughSound_time <= CURTIME)
		{
			// Hit the player, play the taunt sound
			EmitSound("NPC_Crabsynth.Laugh");
			m_crabsynthLaughSpecialSound_time = CURTIME + RandomFloat(2.0f, 4.0f);
		}
	}

	// Player takes less damage
	float flDamage = (pPlayer == NULL) ? 250 : 25;

	// If it's being held by the player, break that bond
	Pickup_ForcePlayerToDropThisObject(pTarget);

	// Calculate the physics force
	ChargeDamage_Inline(this, pTarget, flDamage);
}

bool CNPC_CrabSynth::HandleChargeImpact(Vector vecImpact, CBaseEntity *pEntity)
{	
	// Hit anything we don't like
	if (GetNextAttack() < CURTIME)
	{
		EmitSound("NPC_Hunter.ChargeHitEnemy");
		
		ChargeDamage(pEntity);

	//	if (!pEntity->IsNPC())
	//	{
	//		pEntity->ApplyAbsVelocityImpulse((BodyDirection2D() * 400) + Vector(0, 0, 200));
	//	}

		if (!pEntity->IsAlive() && GetEnemy() == pEntity)
		{
			SetEnemy(NULL);
		}

		SetNextAttack(CURTIME + 2.0f);

		if (!pEntity->IsAlive() || !pEntity->IsNPC())
		{
			SetIdealActivity(ACT_CHARGE_END);
			return false;
		}
		else
			return true;

	}
	/*
	// Hit something we don't hate. If it's not moveable, crash into it.
	if (pEntity->GetMoveType() == MOVETYPE_NONE || pEntity->GetMoveType() == MOVETYPE_PUSH)
	{
		CBreakable *pBreakable = dynamic_cast<CBreakable *>(pEntity);
		if (pBreakable  && pBreakable->IsBreakable() && pBreakable->m_takedamage == DAMAGE_YES && pBreakable->GetHealth() > 0)
		{
			ChargeDamage(pEntity);
		}
		return true;
	}
	*/

	// If it's a vphysics object that's too heavy, crash into it too.
	if (pEntity->GetMoveType() == MOVETYPE_VPHYSICS)
	{
		IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
		if (pPhysics)
		{
			// If the object is being held by the player, knock it out of his hands
			if (pPhysics->GetGameFlags() & FVPHYSICS_PLAYER_HELD)
			{
				Pickup_ForcePlayerToDropThisObject(pEntity);
				return false;
			}

			if (!pPhysics->IsMoveable())
				return true;

			float entMass = PhysGetEntityMass(pEntity);
			float minMass = VPhysicsGetObject()->GetMass() * 0.5f;
			if (entMass < minMass)
			{
				if (entMass < minMass * 0.666f || pEntity->CollisionProp()->BoundingRadius() < GetHullHeight())
				{
					if (pEntity->GetHealth() > 0)
					{
						CBreakableProp *pBreakable = dynamic_cast<CBreakableProp *>(pEntity);
						if (pBreakable && pBreakable->m_takedamage == DAMAGE_YES && pBreakable->GetHealth() > 0 && pBreakable->GetHealth() <= 50)
						{
							ChargeDamage(pEntity);
						}
					}
					pEntity->SetNavIgnore(2.0);
					return false;
				}
			}
			return true;

		}
	}

	return false;
}

Activity CNPC_CrabSynth::NPC_TranslateActivity(Activity activity)
{
	if (m_crabsynth_shield_enabled)
	{
	//	if (activity == ACT_IDLE || activity == ACT_IDLE_AGITATED || activity == ACT_IDLE_ANGRY)
	//	{
			return ACT_IDLE_STEALTH; // do we really want ALL anims to be replaced by this one while in shield? I guess right now we do.
	//	}
	}

	if (m_gun_platform_lowered_bool)
	{
		if (activity == ACT_WALK) return ACT_WALK_AIM;
		if (activity == ACT_RUN) return ACT_RUN_AIM;
		if (activity == ACT_IDLE) return ACT_IDLE_AIM_AGITATED;
		if (activity == ACT_MELEE_ATTACK1) return ACT_MELEE_ATTACK2;
	}

	if (m_NPCState == NPC_STATE_ALERT || m_NPCState == NPC_STATE_COMBAT)
	{
		if (activity == ACT_WALK )
		{
			return m_gun_platform_lowered_bool ? ACT_RUN_AIM : ACT_RUN;
		}
		if (activity == ACT_WALK_AIM)
		{
			return ACT_RUN_AIM;
		}
	}
	return BaseClass::NPC_TranslateActivity(activity);
}

void CNPC_CrabSynth::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();
	
	if (m_crabsynthIdleSound_time < CURTIME)
		IdleSound();

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	if (m_pEngineSound == NULL && m_lifeState != LIFE_DYING || m_lifeState != LIFE_DEAD)
	{
		// Create the sound
		CPASAttenuationFilter filter(this);

		m_pEngineSound = controller.SoundCreate(filter, entindex(), CHAN_STATIC, "NPC_CrabSynth.EngineLoop", ATTN_NORM);

		Assert(m_pEngineSound);

		// Start the engine sound
		controller.Play(m_pEngineSound, 0.0f, 100.0f);
		controller.SoundChangeVolume(m_pEngineSound, 1.0f, 2.0f);
	}
	else
	{
		if (GetHealth() <= GetMaxHealth() * 0.5f && GetHealth() > 0)
		{
			controller.SoundChangeVolume(m_pEngineSound, 0.0f, 0.25f);
		}
		if (m_lifeState == LIFE_DYING || m_lifeState == LIFE_DEAD)
		{
			controller.Shutdown(m_pEngineSound);
			controller.SoundDestroy(m_pEngineSound);
		}
	}

	if (m_pEngineSound_Hurt == NULL && m_lifeState != LIFE_DYING || m_lifeState != LIFE_DEAD)
	{
		// Create the sound
		CPASAttenuationFilter filter(this);

		m_pEngineSound_Hurt = controller.SoundCreate(filter, entindex(), CHAN_STATIC, "NPC_CrabSynth.EngineLoopHurt", ATTN_NORM);

		Assert(m_pEngineSound_Hurt);

		// Start the engine sound
		controller.Play(m_pEngineSound_Hurt, 0.0f, 100.0f);
		controller.SoundChangeVolume(m_pEngineSound_Hurt, 0.01f, 0.1f);
	}
	else
	{
		if (GetHealth() <= GetMaxHealth() * 0.5f && GetHealth() > 0)
		{
			controller.SoundChangeVolume(m_pEngineSound_Hurt, 1.0f, 0.25f);
		}
		if (m_lifeState == LIFE_DYING || m_lifeState == LIFE_DEAD)
		{
			controller.Shutdown(m_pEngineSound_Hurt);
			controller.SoundDestroy(m_pEngineSound_Hurt);
		}
	}

	if (m_flNextAttack - 1.6f < CURTIME && m_flNextAttack - 1.4f > CURTIME)
	{
		EmitSound("NPC_Crabsynth.AttackCue");
	}

	if (IsCurSchedule(SCHED_CRABSYNTH_STRAFE_DURING_PAUSE) || IsCurSchedule(SCHED_CRABSYNTH_RUN_RANDOM_256) || IsCurSchedule(SCHED_CRABSYNTH_ENTER_REGEN))
	{
		if (m_flNextAttack <= CURTIME && (m_enableminigunattack_bool || m_enablesalvoattack_bool))
		{
			ClearSchedule("can attack again, manually stopping chase enemy");
		}
	}

	if (IsCurSchedule(SCHED_ESTABLISH_LINE_OF_FIRE_FALLBACK))
	{
		if (m_flNextAttack <= CURTIME && ((m_enablesalvoattack_bool) || (m_enableminigunattack_bool &&
			IsLineOfSightClear(GetEnemy(), IGNORE_NOTHING) && (float)abs(GetEnemy()->GetAbsOrigin().z - GetAbsOrigin().z) <= 128.0f)))
		{
			ClearSchedule("can attack again, manually stopping chase enemy");
		}
	}
}

int CNPC_CrabSynth::SelectSchedule(void)
{
	if (HasCondition(COND_CRABSYNTH_ENTER_REGEN))
		return SCHED_CRABSYNTH_ENTER_REGEN;
	else
	{
		if (m_NPCState == NPC_STATE_IDLE)
		{
			if (!IsInAScript())
			{
				if (!GetEnemy() && m_gun_platform_lowered_bool)
				{
					return SCHED_CRABSYNTH_FOLD_WEAPONS; // pack up our guns
				}
			}

			//	if (m_bShouldPatrol)
			//	{
			//		return SCHED_PATROL_WALK;
			//	}
			//	else
			return SCHED_IDLE_STAND;
		}

		if (m_NPCState == NPC_STATE_COMBAT)
		{
			if (!IsInAScript())
			{
				if (!GetEnemy() && m_gun_platform_lowered_bool)
				{
					return SCHED_CRABSYNTH_FOLD_WEAPONS; // pack up our guns
				}

				if (GetEnemy() && !m_gun_platform_lowered_bool)
				{
					return SCHED_CRABSYNTH_UNFOLD_WEAPONS; // unpack guns when first facing an enemy
				}
			}
			if (HasCondition(COND_ENEMY_DEAD))
			{
				return BaseClass::SelectSchedule();
			}

			if (HasCondition(COND_CAN_MELEE_ATTACK1))
			{
				// If we can charge, then charge.
				return SCHED_MELEE_ATTACK1;
			}

			if (HasCondition(COND_CAN_MELEE_ATTACK2))
			{
				// If we can charge, then charge.
				return SCHED_MELEE_ATTACK2;
			}

			else//if (HasCondition(COND_CAN_RANGE_ATTACK1))
			{
				if (m_flNextAttack < CURTIME)
				{
					// If only minigun is available, use it in any case. This is more so for scripted crabs...
					if (!m_enablechargeattack_bool
						&& !m_enablesalvoattack_bool
						&& m_enableminigunattack_bool) return SCHED_RANGE_ATTACK2;
					
					// Check if the enemy is far above or far below or obscured from direct firing.
					if (!IsLineOfSightClear(GetEnemy(), IGNORE_NOTHING)
						|| (float)abs(GetEnemy()->GetAbsOrigin().z - GetAbsOrigin().z) > 128.0f)
					{
						// if yes, use rockets, if we can...
						if (m_enablesalvoattack_bool)
							return SCHED_RANGE_ATTACK1;
						// otherwise there's not much choice. Just wait for them to come out.
						else
							return SCHED_ESTABLISH_LINE_OF_FIRE; //SCHED_CHASE_ENEMY;
					}
					else if( IsLineOfSightClear(GetEnemy(), IGNORE_NOTHING) && (float)abs(GetEnemy()->GetAbsOrigin().z - GetAbsOrigin().z) <= 128.0f )
					{
						// If enemy is within acceptable height limit, and not obscured, fire with minigun.
						if (m_enableminigunattack_bool)
							return SCHED_RANGE_ATTACK2;
						else
						{
							// Unless it's disabled. Then use rockets.
							if (m_enablesalvoattack_bool)
								return SCHED_RANGE_ATTACK1;
							else
								// Unless we can't! Then just track them.
								return SCHED_CHASE_ENEMY;
						}
					}
				}
				else
				{
					return SCHED_CHASE_ENEMY;
				}
			}
		}
	return BaseClass::SelectSchedule();
	}
}

int CNPC_CrabSynth::TranslateSchedule(int scheduleType)
{
	switch (scheduleType) 
	{
	case SCHED_RANGE_ATTACK1: return (m_salvotype_int > 2) ? SCHED_CRABSYNTH_ATTACK_ROCKETS : SCHED_CRABSYNTH_ATTACK_SALVO;
	case SCHED_RANGE_ATTACK2: return SCHED_CRABSYNTH_ATTACK_MINIGUN;
	case SCHED_MELEE_ATTACK2: return SCHED_CRABSYNTH_ATTACK_CHARGE;
	case SCHED_DIE:
	{
		StopLoopingSounds();
		return SCHED_DIE;
	}
	case SCHED_CHASE_ENEMY:
	{
		return SCHED_CRABSYNTH_STRAFE_DURING_PAUSE;
	}
	case SCHED_ALERT_STAND:
	{
		return SCHED_PATROL_RUN;
	}

	default: return BaseClass::TranslateSchedule(scheduleType);
	}
}

float CNPC_CrabSynth::MaxYawSpeed(void)
{
	if (m_crabsynth_shield_enabled)
		return 0.0f; // don't turn around during regen

	Activity eActivity = GetActivity();

	CBaseEntity *pEnemy = GetEnemy();

	if (pEnemy != NULL && pEnemy->IsPlayer() == false)
		return 20.0f;

	// Turn slowly when you're charging
	if (eActivity == ACT_CHARGE_START)
		return 8.0f;
	
	// Turn more slowly as we close in on our target
	if (eActivity == ACT_CHARGE_LOOP)
	{
		if (pEnemy == NULL)
			return 4.0f;

		float dist = UTIL_DistApprox2D(GetEnemy()->WorldSpaceCenter(), WorldSpaceCenter());

		if (dist > 512)
			return 16.0f;

		//FIXME: Alter by skill level
		float yawSpeed = RemapVal(dist, 0, 512, 1.0f, 2.0f);
		yawSpeed = clamp(yawSpeed, 1.0f, 2.0f);

		return yawSpeed;
	}

	if (eActivity == ACT_CHARGE_END)
		return 10.0f;

	switch (eActivity)
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		return 40.0f;
		break;

	case ACT_RUN:
	default:
		return 24.0f;
		break;
	}
	
	if (IsCurSchedule(SCHED_CRABSYNTH_ATTACK_ROCKETS))
	{
		return 0.1f;
	}
}

bool CNPC_CrabSynth::OverrideMoveFacing(const AILocalMoveGoal_t &move, float flInterval)
{
	if (IsCurSchedule(SCHED_CRABSYNTH_ATTACK_MINIGUN))
	{
		if( GetEnemy())	AddFacingTarget(GetEnemy(), GetEnemy()->WorldSpaceCenter(), 1.0f, 0.2f);
		return BaseClass::OverrideMoveFacing(move, flInterval);
	}

	if (GetEnemy() && (GetNavigator()->GetMovementActivity() == ACT_RUN || GetNavigator()->GetMovementActivity() == ACT_RUN_AIM))
	{
		// FIXME: this will break scripted sequences that walk when they have an enemy
		Vector vecEnemyLKP = GetEnemyLKP();
		if (UTIL_DistApprox(vecEnemyLKP, GetAbsOrigin()) < 2048)
		{
			// Only start facing when we're close enough
			AddFacingTarget(GetEnemy(), vecEnemyLKP, 1.0, 0.2);
		}
	}

	return BaseClass::OverrideMoveFacing(move, flInterval);
}

void CNPC_CrabSynth::RegenThink()
{
	CEffectData	data;
	data.m_nEntIndex = entindex();
	data.m_flMagnitude = 16;
	data.m_flScale = random->RandomFloat(0.25f, 1.0f);

	if ( CURTIME < m_regen_duration_time )
	{
		if (m_hShieldEntity)
		{
			// keep the shield shell anchored to the body and updated
			m_hShieldEntity->SetAbsOrigin(GetAbsOrigin());
			m_hShieldEntity->SetAbsAngles(GetAbsAngles());
		}

		if ((m_regen_duration_time - CURTIME) <= 3.0f && !m_played_shield_warning_bool )
		{
			EmitSound("NPC_CrabSynth.PreCollapseRegenShield");
			m_played_shield_warning_bool = true;
		}

		if ((float)m_iHealth < (float)(m_iMaxHealth * 0.7))
		{
			m_iHealth += 2;
		}

		DispatchEffect("TeslaHitboxes", data);
		if (RandomFloat(0, 1) > 0.5) EmitSound("LoudSpark");
		SetNextThink(CURTIME + RandomFloat(0.05f, 0.15f), "RegenContext");
	}
	else
	{
		CollapseRegenShield();
		SetContextThink(NULL, CURTIME, "RegenContext");
	}
}

void CNPC_CrabSynth::FireRocketsThink()
{
	StudioFrameAdvance();

	int burstAmount = m_salvosize_int;
	float homingAmount = npc_crabsynth_salvo_homing_amount.GetFloat();
	float gravityAmount = npc_crabsynth_salvo_gravity.GetFloat();
	float speedAmount = npc_crabsynth_salvo_speed.GetFloat();
	float delayAmount = npc_crabsynth_salvo_delay.GetFloat();

	if (m_salvotype_int == 1)
	{
	//	burstAmount = 6;
		homingAmount = 15;
		gravityAmount = 6.0;
		speedAmount = 900;
		delayAmount = 0.0;
	}
	else if (m_salvotype_int == 2)
	{
	//	burstAmount = 16;
		homingAmount = 5;
		gravityAmount = 12;
		speedAmount = 750;
		delayAmount = 0;
	}

	if (m_iSalvoAttacksCounter > (burstAmount - 1))
		return;

	if (GetEnemy() == NULL)
		return;
	
	Vector vecRocketOrigin;
	GetAttachment(LookupAttachment("salvo_origin"), vecRocketOrigin);
		
	static float s_pSide[] = { 0.25, 0.125, 0.0675, -0.0675, -0.125, -0.25 };

	static float x_umbrella[] = {0, 0.4, 0.7, 0.93, 1, 0.92, 0.71, 0.42, 0, -0.41, -0.7, -0.92, -1, -0.93, -0.7, -0.41 };
	static float y_umbrella[] = {1, 0.92, 0.7, 0.37, 0, -0.39, -0.7, -0.91, -1, -0.92, -0.7, -0.4, 0, 0.4, 0.7, 0.91 };

	Vector forward;
	GetVectors(&forward, NULL, NULL);

	Vector vecDir;
	CrossProduct(Vector(0, 0, 1), forward, vecDir);
	vecDir.z = 1.0f;
	if (m_salvotype_int < 2)
	{
		vecDir.x *= s_pSide[m_nRocketSide];
		vecDir.y *= s_pSide[m_nRocketSide];
	}
	else
	{
		vecDir.x *= x_umbrella[m_nRocketSide];
		vecDir.y *= y_umbrella[m_nRocketSide];
	}

	if (++m_nRocketSide >= burstAmount)
	{
		m_nRocketSide = 0;
	}

	VectorNormalize(vecDir);

	Vector vecVelocity;
	VectorMultiply(vecDir, speedAmount, vecVelocity);

	QAngle angles;
	VectorAngles(vecDir, angles);
	
	//
	Vector vUp;
	AngleVectors(GetAbsAngles(), NULL, NULL, &vUp);
//	Vector vLaunchVelocity = (vUp * 750);

	CGrenadeHomer *pGrenade = CGrenadeHomer::CreateGrenadeHomer(MAKE_STRING("models/_Monsters/Combine/gunship_projectile.mdl"), NULL_STRING, vecRocketOrigin, vec3_angle, edict());
	pGrenade->Spawn();
//	pGrenade->SetSpin(npc_crabsynth_salvo_spin.GetFloat(), 
//		npc_crabsynth_salvo_spin.GetFloat());
	pGrenade->SetHoming((0.01 * homingAmount),
		0.1f, // Delay
		0.25f, // Ramp up
		1.0f, // Duration
		1.0f);// Ramp down
	pGrenade->SetDamage(40); //10
	pGrenade->SetDamageRadius(40); //6
	pGrenade->Launch(this, 
		GetEnemy(),
		vecVelocity,
		npc_crabsynth_salvo_homing_rate.GetFloat(),
		gravityAmount, 
		HOMER_SMOKE_TRAIL_ON);
	
	EmitSound("NPC_CrabSynth.SalvoSingleFire");

	m_iSalvoAttacksCounter++;

	SetNextThink(CURTIME + delayAmount, "SalvoAttackContext");
}

void CNPC_CrabSynth::FireMinigunThink()
{
	StudioFrameAdvance();
	
	if (m_fireminigunperiod_time < CURTIME) return;

	FireMinigun();
	
	SetNextThink(CURTIME + 0.1f, "MinigunAttackContext");
}

void CNPC_CrabSynth::FireMinigun(void)
{
	if (!m_minigunreadytofire_bool && !m_hCine) return; // if we're performing FireMinigun though an animevent, 
																 // but we haven't finished winding up the minigun, return.
																 // Unless we're doing it as part of a scripted scene.

	// Firing operation
	Vector minigunBase;
	GetAttachment(LookupAttachment("minigun_tip"), minigunBase);

	Vector minigunTip, minigunForward;
	GetVectors(&minigunForward, NULL, NULL);
	minigunTip = minigunBase + minigunForward * 8;

	if (GetEnemy() != NULL)
	{
		float cathetus_a = (minigunTip - minigunBase).Length();
		float cathetus_b;

		float height_diff = ((GetEnemy()->IsPlayer()) ? GetEnemy()->EyePosition().z : GetEnemy()->WorldSpaceCenter().z) - minigunTip.z;
		float length_diff = (GetEnemy()->WorldSpaceCenter() - minigunBase).Length();

		float angle_a = (float)atan(height_diff / length_diff);

		cathetus_b = cathetus_a * tan(angle_a);

		minigunTip.z += cathetus_b;
	}

	//	GetAttachment(LookupAttachment("minigun_tip"), minigunBase);

	Vector minigunDir = minigunTip - minigunBase;
	VectorNormalize(minigunDir);

	// If target is npc_bullseye, fire with better precision.
	Vector vecSpread = VECTOR_CONE_3DEGREES;

	if (GetEnemy() != NULL)
		if (FClassnameIs(GetEnemy(), "npc_bullseye")) vecSpread = VECTOR_CONE_1DEGREES;

	FireBulletsInfo_t info;
	info.m_vecSrc = minigunBase;
	info.m_vecSpread = vecSpread;
	info.m_flDistance = MAX_COORD_RANGE;
	info.m_iAmmoType = GetAmmoDef()->Index("APCTurret"); // using this ammo to allow us to damage vehicles
	info.m_flDamage = 10;
	info.m_iPlayerDamage = 3;
	info.m_iTracerFreq = 1;
	info.m_vecDirShooting = minigunDir;
	info.m_nFlags = FIRE_BULLETS_TEMPORARY_DANGER_SOUND;
	info.m_pAdditionalIgnoreEnt = this;

	if (GetEnemy())
	{
		CBasePlayer *pPlayer = assert_cast<CBasePlayer*>(GetEnemy());

		//	int oldEnemyHealth = pPlayer->GetHealth();
		//	int oldEnemyArmor = pPlayer->ArmorValue();

		FireBullets(info);

		//	CShotManipulator manipulator(minigunDir);
		//	Vector vecShoot;
		//	vecShoot = manipulator.ApplySpread(VECTOR_CONE_4DEGREES, 1.0f);
		//	QAngle angShoot;
		//	VectorAngles(vecShoot, angShoot);
		//	vecShoot *= FLECHETTE_AIR_VELOCITY;

		//	CHunterFlechette *pFlechette = CHunterFlechette::FlechetteCreate(minigunBase, angShoot, this);

		//	pFlechette->AddEffects(EF_NOSHADOW);
		//	pFlechette->Shoot(vecShoot, true);

		if (pPlayer != NULL)
		{
			if (pPlayer->GetHealth() < 1)
			{
				if (m_crabsynthLaughSpecialSound_time <= CURTIME)
				{
					// Hit the player, play the taunt sound
					EmitSound("NPC_Crabsynth.LaughSpecial");
					m_crabsynthLaughSpecialSound_time = CURTIME + RandomFloat(3.0f, 8.0f);
				}
			}
		}
	}

	DoMuzzleFlash();

	EmitSound("NPC_CrabSynth.MinigunFire");
}

void CNPC_CrabSynth::FirePulsegun(void)
{	
	// Firing operation
	Vector pulsegunBase;
	GetAttachment(LookupAttachment("pulsegun_tip"), pulsegunBase);

	Vector pulsegunTip, pulsegunForward;
	GetVectors(&pulsegunForward, NULL, NULL);
	pulsegunTip = pulsegunBase + pulsegunForward * 16;

	if (GetEnemy() != NULL)
	{
		float cathetus_a = (pulsegunTip - pulsegunBase).Length();
		float cathetus_b;

		float height_diff = GetEnemy()->WorldSpaceCenter().z - pulsegunTip.z;
		float length_diff = (GetEnemy()->WorldSpaceCenter() - pulsegunBase).Length();

		float angle_a = (float)atan(height_diff / length_diff);

		cathetus_b = cathetus_a * tan(angle_a);

		pulsegunTip.z += cathetus_b;
	}

	Vector pulsegunDir = pulsegunTip - pulsegunBase;
	VectorNormalize(pulsegunDir);

	// If target is npc_bullseye, fire with better precision.
	Vector vecSpread = VECTOR_CONE_3DEGREES;

	if (GetEnemy() != NULL)
		if (FClassnameIs(GetEnemy(), "npc_bullseye")) vecSpread = VECTOR_CONE_1DEGREES;

	FireBulletsInfo_t info;
	info.m_vecSrc = pulsegunBase;
	info.m_vecSpread = vecSpread;
	info.m_flDistance = MAX_COORD_RANGE;
	info.m_iAmmoType = GetAmmoDef()->Index("CombineHeavyCannon"); //Index("HelicopterGun");
	info.m_iTracerFreq = 1;
	info.m_vecDirShooting = pulsegunDir;
	info.m_nFlags = FIRE_BULLETS_TEMPORARY_DANGER_SOUND;
	info.m_pAdditionalIgnoreEnt = this;
	
	FireBullets(info);

	// Send the railgun effect
	trace_t tr;
	UTIL_TraceLine(pulsegunBase, pulsegunBase + pulsegunDir * 4096, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
	DispatchParticleEffect("Weapon_Combine_Ion_Cannon", pulsegunBase, tr.endpos, vec3_angle, NULL);
	Vector vecPlayer = AI_GetSinglePlayer()->EyePosition();

	Vector vecNearestPoint = PointOnLineNearestPoint(pulsegunBase, tr.endpos, vecPlayer);

	float flDist = vecPlayer.DistTo(vecNearestPoint);

	if (flDist >= 10.0f && flDist <= 120.0f)
	{
		// Don't shake the screen if we're hit (within 10 inches), but do shake if a shot otherwise comes within 10 feet.
		UTIL_ScreenShake(vecNearestPoint, 2, 60, 0.3, 120.0f, SHAKE_START, false);
	}
	
	EmitSound("NPC_CrabSynth.PulsegunFire");
}

void CNPC_CrabSynth::DoMuzzleFlash(void)
{
	BaseClass::DoMuzzleFlash();

//	CEffectData data;
//	data.m_nAttachmentIndex = LookupAttachment("muzzle");
//	data.m_nAttachmentIndex = LookupAttachment("minigun_tip");
//	data.m_nEntIndex = entindex();
//	DispatchEffect("StriderMuzzleFlash", data);


	DispatchParticleEffect("apc_muzzleflash", PATTACH_POINT_FOLLOW, this, LookupAttachment("muzzle"));
}

// Custom impact effect for bullets. More smoke, flecks, 
// to give impression of a deadly high caliber minigun. (it actually fires Pistol ammo)
void CNPC_CrabSynth::DoImpactEffect(trace_t &tr, int nDamageType)
{
	Vector vUp = tr.plane.normal;
	VectorRotate(vUp, QAngle(90, 0, 0), vUp);
	QAngle qUp;
	VectorAngles(vUp, qUp);
	DispatchParticleEffect("impact_concrete", tr.endpos, qUp, this);
	UTIL_ImpactTrace(&tr, nDamageType, "ImpactGauss");
}

void CNPC_CrabSynth::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_CRABSYNTH_SHOOT_ROCKETS:
	{
		Vector vecRocketOrigin;
		GetAttachment(LookupAttachment("rocket_l"), vecRocketOrigin);

		Vector vecDir;
		if (m_salvotype_int == 3) GetVectors(&vecDir, NULL, NULL);
		else if (m_salvotype_int == 4)
		{
			GetVectors(NULL, NULL, &vecDir);
			GetAttachment(LookupAttachment("salvo_origin"), vecRocketOrigin);
		}

		VectorNormalize(vecDir);
		vecDir.x += RandomFloat(-0.1, 0.1);
		vecDir.y += RandomFloat(-0.1, 0.1);
		
		Vector vecVelocity;
		float randomVel = 0;
		if (m_salvotype_int == 4) randomVel = RandomFloat(-25, 25); // if firing twin arched missiles, give it a bit of randomess so the two missiles don't appear as one
		VectorMultiply(vecDir, 1500 + randomVel, vecVelocity);

		QAngle angles;
		VectorAngles(vecDir, angles);

		Vector vUp;
		AngleVectors(GetAbsAngles(), NULL, NULL, &vUp);

		CGrenadeHomer *pGrenade = CGrenadeHomer::CreateGrenadeHomer(MAKE_STRING("models/_Monsters/Combine/gunship_projectile.mdl"), NULL_STRING, vecRocketOrigin, GetAbsAngles(), edict());
		pGrenade->Spawn();

		pGrenade->SetHoming(0.01 * npc_crabsynth_salvo_homing_amount.GetFloat(), 0.1, 0.1, 1.0, 1.5);
		pGrenade->SetDamage((GetEnemy()->IsPlayer() ? 15 : 100));
		pGrenade->SetDamageRadius(6);
		pGrenade->Launch(this,
			GetEnemy(),
			vecVelocity,
			1000,
			0.5f,
			HOMER_SMOKE_TRAIL_ON);

		EmitSound("NPC_CrabSynth.SalvoSingleFire");

		GetAttachment(LookupAttachment("rocket_r"), vecRocketOrigin);
		if (m_salvotype_int == 4)
		{
			GetAttachment(LookupAttachment("salvo_origin"), vecRocketOrigin);
		}

		CGrenadeHomer *pGrenade2 = CGrenadeHomer::CreateGrenadeHomer(MAKE_STRING("models/_Monsters/Combine/gunship_projectile.mdl"), NULL_STRING, vecRocketOrigin, GetAbsAngles(), edict());
		pGrenade2->Spawn();

		pGrenade2->SetHoming(0.01 * npc_crabsynth_salvo_homing_amount.GetFloat(), 0.1, 0.1, 1.0, 1.5);
		pGrenade2->SetDamage(10);
		pGrenade2->SetDamageRadius(6);
		pGrenade2->Launch(this,
			GetEnemy(),
			vecVelocity,
			1000,
			0.5f,
			HOMER_SMOKE_TRAIL_ON);

		EmitSound("NPC_CrabSynth.SalvoSingleFire");

		SetNextAttack(CURTIME + m_salvonextfiredelay_float);

		m_OnFiredRockets.FireOutput(this, this);

		TaskComplete();
	}
	break;
	case TASK_CRABSYNTH_START_SALVO:
	{
		SetContextThink(&CNPC_CrabSynth::FireRocketsThink, CURTIME + 0.1f, "SalvoAttackContext");
		m_iSalvoAttacksCounter = 0;
		m_OnStartedSalvo.FireOutput(this, this);
		TaskComplete();
	}
	break;
	case TASK_CRABSYNTH_STOP_SALVO:
	{
		if (m_iSalvoAttacksCounter > (m_salvosize_int - 1))
		{
			SetContextThink(NULL, 0, "SalvoAttackContext");
		}
	}
	break;
	case TASK_CRABSYNTH_CHARGE:
	{
		SetIdealActivity((Activity)ACT_CHARGE_START);
		m_OnStartedCharge.FireOutput(this, this);
	}
	break;
	case TASK_CRABSYNTH_CHARGE_MINIGUN: // 'charging' minigun means starting to play minigun loop anim, with a wind up delay 
	{									// when the minigun is spinning, but isn't producing bullets yet.
#ifndef CRABSYNTH_NEW_MINIGUN
		EmitSound("NPC_CrabSynth.Charge");
#else
		SetIdealActivity(ACT_CRABSYNTH_MINIGUN_LOOP); // here the looping animation starts.
#endif
		EmitSound("NPC_CrabSynth.MinigunWindup");
		m_chargeminigundelay_time = CURTIME + npc_crabsynth_minigun_windup.GetFloat(); // after this delay, this task will finish,
	}																				   // and loop task will start, and turn on bullets (see HandleAnimEvent).
	break;
	case TASK_CRABSYNTH_LOOP_MINIGUN:
	{
		m_minigunreadytofire_bool = true;
		m_fireminigunperiod_time = CURTIME + m_minigunduration_kv; // after this long, this task will finish,
#ifndef CRABSYNTH_NEW_MINIGUN															// and the minigun will wind down.
		SetContextThink(&CNPC_CrabSynth::FireMinigunThink, CURTIME + 0.1f, "MinigunAttackContext");
#else
//		SetIdealActivity(ACT_CRABSYNTH_MINIGUN_LOOP);
#endif
		m_OnStartedMinigun.FireOutput(this, this);
//		TaskComplete();
	}
	break;
	case TASK_CRABSYNTH_STOP_MINIGUN:
	{
	//	if (m_fireminigunperiod_time < CURTIME)
	//	{
#ifndef CRABSYNTH_NEW_MINIGUN
			SetContextThink(NULL, 0, "MinigunAttackContext");
#endif
			EmitSound("NPC_CrabSynth.MinigunStop");
			m_minigunreadytofire_bool = false;
			m_OnFinishedMinigun.FireOutput(this, this);
			m_flNextAttack = CURTIME + m_minigunpause_kv;
			TaskComplete();
	//	}
	}
	break;
	case TASK_CRABSYNTH_ANNOUNCE_REGEN:
	{
		// stop ongoing attacks (charge will get cleared when clearing schedule)
		SetContextThink(NULL, 0, "MinigunAttackContext");
		SetContextThink(NULL, 0, "SalvottackContext");
		TaskComplete();
	}
	break;
	case TASK_CRABSYNTH_ENTER_REGEN:
	{
		if (CreateRegenShield(m_auto_regen_duration_float))
		{
			TaskComplete();
		}
		else
		{
			TaskFail("failed to create regen shield!");
		}
	}
	break;
	case TASK_CRABSYNTH_REGEN_WAIT:
	{
	//	Msg("Regen wait\n"); 
	//	SetSequence(SelectWeightedSequence(ACT_IDLE_STEALTH));
	}
	break;
	case TASK_CRABSYNTH_UNFOLD_WEAPONS:
	{ 
		if (m_gun_platform_lowered_bool) TaskComplete(); // don't play the animation if called while having guns unpacked for whatever reason.
		SetActivity(ACT_CRABSYNTH_UNFOLD_WEAPONS);
	}
	break;
	case TASK_CRABSYNTH_FOLD_WEAPONS:
	{
		if (!m_gun_platform_lowered_bool) TaskComplete(); // don't play the animation if called while having guns packed for whatever reason.
		SetActivity(ACT_CRABSYNTH_FOLD_WEAPONS);
	}
	break;
	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

void CNPC_CrabSynth::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_CRABSYNTH_STOP_SALVO:
	{
		if (m_iSalvoAttacksCounter > (m_salvosize_int - 1))
		{
			SetContextThink(NULL, 0, "SalvoAttackContext");
			SetNextAttack(CURTIME + m_salvonextfiredelay_float);
			m_OnFinishedSalvo.FireOutput(this, this);
			TaskComplete();
		}
	}
	break;
	case TASK_CRABSYNTH_CHARGE:
	{
		Activity eActivity = GetActivity();

		// See if we're trying to stop after hitting/missing our target
		if (eActivity == ACT_CHARGE_END  )
		{
			if (IsActivityFinished())
			{
				m_nextcharge_time = CURTIME + 3.0f + random->RandomFloat(0, 2.5) + random->RandomFloat(0, 2.5);
				m_flNextAttack = CURTIME + 1.0f;
				TaskComplete();
				return;
			}

			// Still in the process of slowing down. Run movement until it's done.
			AutoMovement();
			return;
		}

		// Check for manual transition
		if ((eActivity == ACT_CHARGE_START) && (IsActivityFinished()))
		{
			SetIdealActivity(ACT_CHARGE_LOOP);
		}

		// See if we're still running
		if (eActivity == ACT_CHARGE_LOOP || eActivity == ACT_CHARGE_START)
		{
			if (HasCondition(COND_NEW_ENEMY) || HasCondition(COND_LOST_ENEMY) || HasCondition(COND_ENEMY_DEAD))
			{
				SetIdealActivity(ACT_CHARGE_END);
				return;
			}
			else
			{
				if (GetEnemy() != NULL)
				{
					Vector	goalDir = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
					VectorNormalize(goalDir);

					if (DotProduct(BodyDirection2D(), goalDir) < 0.25f)
					{
						SetIdealActivity(ACT_CHARGE_END);
					}
				}
			}
		}

		// Steer towards our target
		float idealYaw;
		if (GetEnemy() == NULL)
		{
			idealYaw = GetMotor()->GetIdealYaw();
		}
		else
		{
			idealYaw = CalcIdealYaw(GetEnemy()->GetAbsOrigin());
		}

		// Add in our steering offset
		idealYaw += ChargeSteer();

		// Turn to face
		GetMotor()->SetIdealYawAndUpdate(idealYaw);
		
		// Let our animations simply move us forward. Keep the result
		// of the movement so we know whether we've hit our target.
		AIMoveTrace_t moveTrace;
		if (AutoMovement(GetEnemy(), &moveTrace) == false)
		{
			// Only stop if we hit the world
			if (HandleChargeImpact(moveTrace.vEndPosition, moveTrace.pObstruction))
			{
				// If we're starting up, this is an error
				if (eActivity == ACT_CHARGE_START)
				{
					TaskFail("Unable to make initial movement of charge\n");
					return;
				}

				// Crash unless we're trying to stop already
				if (eActivity != ACT_CHARGE_END)
				{
					if (moveTrace.fStatus == AIMR_BLOCKED_WORLD && moveTrace.vHitNormal == vec3_origin)
					{
						SetIdealActivity(ACT_CHARGE_END);
					}
					else
					{
						// Shake the screen
						if (moveTrace.fStatus != AIMR_BLOCKED_NPC)
						{
							EmitSound("NPC_Hunter.ChargeHitWorld");
							UTIL_ScreenShake(GetAbsOrigin(), 16.0f, 4.0f, 1.0f, 400.0f, SHAKE_START);
						}
						SetIdealActivity(ACT_CHARGE_END);
					}
				}
			}
			else if (moveTrace.pObstruction)
			{
				// If we hit another hunter, stop
				if (moveTrace.pObstruction->Classify() == CLASS_COMBINE_HUNTER)
				{
					// Crash unless we're trying to stop already
					if (eActivity != ACT_CHARGE_END)
					{
						SetIdealActivity(ACT_CHARGE_END);
					}
				}
				// If we hit an antlion, don't stop, but kill it
				// We never have hunters and antlions together, but you never know.
				else if (moveTrace.pObstruction->Classify() == CLASS_ANTLION)
				{
					if (FClassnameIs(moveTrace.pObstruction, "npc_antlionguard"))
					{
						// Crash unless we're trying to stop already
						if (eActivity != ACT_CHARGE_END)
						{
							SetIdealActivity(ACT_CHARGE_END);
						}
					}
					else
					{
						ChargeDamage_Inline(this, moveTrace.pObstruction, moveTrace.pObstruction->GetHealth());
					}
				}
			}
		}
	}
	break;
	case TASK_CRABSYNTH_CHARGE_MINIGUN:
	{ 
		// Steer towards our target
		float idealYaw;
		if (GetEnemy() == NULL)
		{
			idealYaw = GetMotor()->GetIdealYaw();
		}
		else
		{
			idealYaw = CalcIdealYaw(GetEnemy()->GetAbsOrigin());
		}

		// Add in our steering offset
		idealYaw += ChargeSteer();

		// Turn to face
		GetMotor()->SetIdealYawAndUpdate(idealYaw);

		if (m_chargeminigundelay_time < CURTIME)
		{
			TaskComplete();
		}
	}
	break;
	case TASK_CRABSYNTH_LOOP_MINIGUN:
	{
		// Steer towards our target
		float idealYaw;
		if (GetEnemy() == NULL)
		{
			idealYaw = GetMotor()->GetIdealYaw();
		}
		else
		{
			idealYaw = CalcIdealYaw(GetEnemy()->GetAbsOrigin());
		}

		// Add in our steering offset
		idealYaw += ChargeSteer();

		// Turn to face
		GetMotor()->SetIdealYawAndUpdate(idealYaw);

		if (m_fireminigunperiod_time < CURTIME)
		{
			TaskComplete();
		}
	}
	break;
	/*
	case TASK_CRABSYNTH_STOP_MINIGUN:
	{
		if (m_fireminigunperiod_time > CURTIME)
		{
			// turn our body to follow the target
			// Steer towards our target
			float idealYaw;
			if (GetEnemy() == NULL)
			{
				idealYaw = GetMotor()->GetIdealYaw();
			}
			else
			{
				idealYaw = CalcIdealYaw(GetEnemy()->GetAbsOrigin());
			}

			// Add in our steering offset
			idealYaw += ChargeSteer();

			// Turn to face
			GetMotor()->SetIdealYawAndUpdate(idealYaw);

			return;
		}

#ifndef CRABSYNTH_NEW_MINIGUN
		SetContextThink(NULL, 0, "MinigunAttackContext");
#endif
		m_flNextAttack = CURTIME + 2.0f;
		m_OnFinishedMinigun.FireOutput(this, this);
		TaskComplete();
	}
	break;
	*/
	case TASK_CRABSYNTH_REGEN_WAIT:
	{
		if( CURTIME < m_regen_duration_time + 0.1f)
		{
			m_flNextAttack = m_regen_duration_time + 2.0f;
			SetIdealActivity(Activity(ACT_ARM));
			return;
		}
		m_flNextAttack = CURTIME + m_minigunpause_kv;
		TaskComplete();
	}
	break;
	case TASK_CRABSYNTH_UNFOLD_WEAPONS:
	{
		if (IsActivityFinished())
		{
			m_gun_platform_lowered_bool = true; // also duplicated in HandleAnimEvent.
			TaskComplete();
		}
	}
	break;
	case TASK_CRABSYNTH_FOLD_WEAPONS:
	{
		if (IsActivityFinished())
		{
			m_gun_platform_lowered_bool = false; // also duplicated in HandleAnimEvent.
			TaskComplete();
		}
	}
	break;

	default:
		BaseClass::RunTask(pTask);
		break;
	}
}

int CNPC_CrabSynth::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (GetEnemy())
	{
		if (FClassnameIs(GetEnemy(), "npc_bullseye")) return COND_NONE; // only use rockets and salvo against bullseyes
	}

//	if (!m_enablechargeattack_bool) return COND_NONE;

	if (flDist > 256.0f)
		return COND_TOO_FAR_TO_ATTACK;
	if (flDot < 0.5f)
		return COND_NOT_FACING_ATTACK;

	return COND_CAN_MELEE_ATTACK1;
}

int CNPC_CrabSynth::MeleeAttack2Conditions(float flDot, float flDist)
{
	if (GetEnemy())
	{
		if (FClassnameIs(GetEnemy(), "npc_bullseye")) return COND_NONE; // only use rockets and salvo against bullseyes
	}

	if (!m_enablechargeattack_bool) return COND_NONE;

	if (flDist < 300.0f)
		return COND_TOO_FAR_TO_ATTACK;
	if (flDist > 1000.0f)
		return COND_TOO_FAR_TO_ATTACK;
	if (flDot < 0.5f)
		return COND_NOT_FACING_ATTACK;
	if (m_nextcharge_time > CURTIME)
		return COND_NONE;

	return COND_CAN_MELEE_ATTACK2;
}

void CNPC_CrabSynth::InputSetHullSizeSmall(inputdata_t &data)
{
	SetHullType(HULL_HUMAN);
	SetHullSizeSmall( true ); // this will turn the crab's HULL_CRABSYNTH into small maxS/minS which in turn are equal to that of HULL_LARGE.
}

void CNPC_CrabSynth::InputSetHullSizeNormal(inputdata_t &data)
{
	SetHullType(HULL_CRABSYNTH);
	SetHullSizeNormal( true ); // back to normal maxS/minS values. 
}

void CNPC_CrabSynth::InputSetMinigunFiringDuration(inputdata_t &data)
{
	m_minigunduration_kv = data.value.Float();
}

void CNPC_CrabSynth::InputSetMinigunFiringPause(inputdata_t &data)
{
	m_minigunpause_kv = data.value.Float();
}

void CNPC_CrabSynth::InputSetSalvoType_HomingVolley(inputdata_t &data)
{
	m_salvotype_int = CRABSYNTH_SALVO_TYPE_HOMINGVOLLEY;
//	printf("Crab synth switches salvo mode to Homing Volley\n");
}

void CNPC_CrabSynth::InputSetSalvoType_LinedVolley(inputdata_t &data)
{
	m_salvotype_int = CRABSYNTH_SALVO_TYPE_LINEDVOLLEY;
//	printf("Crab synth switches salvo mode to Lined Volley\n");
}

void CNPC_CrabSynth::InputSetSalvoType_UmbrellaVolley(inputdata_t &data)
{
	m_salvotype_int = CRABSYNTH_SALVO_TYPE_UMBRELLAVOLLEY;
//	printf("Crab synth switches salvo mode to Umbrella Volley\n");
}

void CNPC_CrabSynth::InputSetSalvoType_TwinMissilesDirect(inputdata_t &data)
{
	m_salvotype_int = CRABSYNTH_SALVO_TYPE_TWINMISSILESDIRECT;
//	printf("Crab synth switches salvo mode to Twin Missiles, Direct\n");
}

void CNPC_CrabSynth::InputSetSalvoType_TwinMissilesArched(inputdata_t &data)
{
	m_salvotype_int = CRABSYNTH_SALVO_TYPE_TWINMISSILESARCHED;
//	printf("Crab synth switches salvo mode to Twin Missiles, Arched\n");
}

void CNPC_CrabSynth::InputSetSalvoSize(inputdata_t &data)
{
	m_salvosize_int = data.value.Int();
}

void CNPC_CrabSynth::InputSetSalvoNextFireDelay(inputdata_t &data)
{
	m_salvonextfiredelay_float = data.value.Float();
}

void CNPC_CrabSynth::InputEnableChargeAttack(inputdata_t &data){
	m_enablechargeattack_bool = true; }

void CNPC_CrabSynth::InputDisableChargeAttack(inputdata_t &data) {
	m_enablechargeattack_bool = false;
}

void CNPC_CrabSynth::InputEnableMinigunAttack(inputdata_t &data) {
	m_enableminigunattack_bool = true;
}

void CNPC_CrabSynth::InputDisableMinigunAttack(inputdata_t &data) {
	m_enableminigunattack_bool = false;
}

void CNPC_CrabSynth::InputEnableSalvoAttack(inputdata_t &data) {
	m_enablesalvoattack_bool = true;
}

void CNPC_CrabSynth::InputDisableSalvoAttack(inputdata_t &data) {
	m_enablesalvoattack_bool = false;
}

void CNPC_CrabSynth::InputForceStartMinigunAttack(inputdata_t &data)
{
	/*
	// clear existing minigun cycle if active
	SetContextThink(NULL, 0, "MinigunAttackContext");

	m_fireminigunperiod_time = CURTIME + data.value.Float();
	SetContextThink(&CNPC_CrabSynth::FireMinigunThink, CURTIME + 0.1f, "MinigunAttackContext");
	m_OnStartedMinigun.FireOutput(this, this);

	m_flNextAttack = m_fireminigunperiod_time + 0.5f;
	*/

	ClearSchedule("forced to clear schedule via input");
	SetSchedule(SCHED_CRABSYNTH_ATTACK_MINIGUN);
}

void CNPC_CrabSynth::InputForceStartPulsegunAttack(inputdata_t &data)
{
	ClearSchedule("forced to clear schedule via input");
	SetSchedule(SCHED_CRABSYNTH_ATTACK_PULSEGUN);
}

#ifdef RIDEABLE_NPCS
void CNPC_CrabSynth::InputEnterCrabSynthVehicle(inputdata_t &input)
{
//	if (m_behavior != BEHAVIOR_2003) return;

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if (pPlayer)
	{
		Vector mins, maxs;
		mins = WorldAlignMins() + GetAbsOrigin();
		maxs = WorldAlignMaxs() + GetAbsOrigin();
		mins += Vector(8, 8, 8);
		maxs -= Vector(8, 8, 8);
		float height = maxs.z - mins.z;
		mins.z += height;
		maxs.z += height;

		// did player land on top?
		if (pPlayer->EyePosition().z > WorldAlignMaxs().z)
		{
			pPlayer->GetInVehicle(GetServerVehicle(), VEHICLE_ROLE_DRIVER);
		}
	}
}
#endif

void CNPC_CrabSynth::InputDoBodyThrowTowardTarget(inputdata_t &inputdata)
{
	// Ignore if we're inside a scripted sequence
	if (m_NPCState == NPC_STATE_SCRIPT && m_hCine)
		return;

	CBaseEntity *pEntity = gEntList.FindEntityByName(NULL, inputdata.value.String(), NULL, inputdata.pActivator, inputdata.pCaller);
	if (!pEntity)
	{
		DevMsg("%s (%s) received ThrowGrenadeAtTarget input, but couldn't find target entity '%s'\n", GetClassname(), GetDebugName(), inputdata.value.String());
		return;
	}

	m_vSavePosition = pEntity->GetAbsOrigin();
	ClearSchedule("forced to clear schedule via input");
	SetSchedule(SCHED_CRABSYNTH_BODYTHROW);
}

void CNPC_CrabSynth::InputUnfoldGuns(inputdata_t &inputdata)
{
	m_gun_platform_lowered_bool = true;
	ClearSchedule("forced to clear schedule via input");
	SetSchedule(SCHED_CRABSYNTH_UNFOLD_WEAPONS);
}

void CNPC_CrabSynth::InputFoldGuns(inputdata_t &inputdata)
{
	m_gun_platform_lowered_bool = false;
	ClearSchedule("forced to clear schedule via input");
	SetSchedule(SCHED_CRABSYNTH_FOLD_WEAPONS);
}

BEGIN_DATADESC(CNPC_CrabSynth)
#ifdef RIDEABLE_NPCS
DEFINE_EMBEDDED(m_ServerVehicle),
DEFINE_FIELD(m_nextTouchTime, FIELD_TIME),
DEFINE_FIELD(m_savedViewOffset, FIELD_VECTOR),
DEFINE_FIELD(m_hPlayer, FIELD_EHANDLE),
//DEFINE_FIELD(m_isHatchOpen, FIELD_BOOLEAN),
//DEFINE_INPUTFUNC(FIELD_VOID, "OpenHatch", InputOpenHatch),
DEFINE_INPUTFUNC(FIELD_VOID, "EnterVehicle", InputEnterCrabSynthVehicle),
DEFINE_ENTITYFUNC(PlayerTouch),
#endif
DEFINE_SOUNDPATCH(m_pEngineSound),
DEFINE_SOUNDPATCH(m_pEngineSound_Hurt),
DEFINE_FIELD(m_iShockWaveTexture, FIELD_INTEGER),
DEFINE_FIELD(m_regen_duration_time, FIELD_TIME),
DEFINE_FIELD(m_hShieldEntity, FIELD_EHANDLE),
DEFINE_FIELD(m_nextcharge_time, FIELD_TIME),
DEFINE_FIELD(m_crabsynthIdleSound_time, FIELD_TIME),
DEFINE_FIELD(m_iSalvoAttacksCounter, FIELD_INTEGER),
DEFINE_FIELD(m_nRocketSide, FIELD_INTEGER),
DEFINE_FIELD(m_hSpecificRocketTarget, FIELD_EHANDLE),
DEFINE_FIELD(m_salvotype_int, FIELD_INTEGER),
DEFINE_FIELD(m_salvosize_int, FIELD_INTEGER),
DEFINE_FIELD(m_salvonextfiredelay_float, FIELD_FLOAT),
DEFINE_FIELD(m_enablechargeattack_bool, FIELD_BOOLEAN),
DEFINE_FIELD(m_enableminigunattack_bool, FIELD_BOOLEAN),
DEFINE_FIELD(m_enablesalvoattack_bool, FIELD_BOOLEAN),
DEFINE_FIELD(m_played_demented_fluffing_hack_bool, FIELD_BOOLEAN),
DEFINE_FIELD(m_minigunreadytofire_bool, FIELD_BOOLEAN),
DEFINE_FIELD(m_gun_platform_lowered_bool, FIELD_BOOLEAN),

DEFINE_KEYFIELD(m_max_auto_regen_times_int, FIELD_INTEGER, "MaxAutoRegenTimes"),
DEFINE_KEYFIELD(m_allowed_to_auto_regen_bool, FIELD_BOOLEAN, "CanAutoRegen"),
DEFINE_KEYFIELD(m_auto_regen_health_threshold_float, FIELD_FLOAT, "AutoRegenAtHealthRatio"),
DEFINE_KEYFIELD(m_auto_regen_duration_float, FIELD_FLOAT, "AutoRegenDuration"),

DEFINE_KEYFIELD(m_minigunduration_kv, FIELD_FLOAT, "MinigunFiringDuration"),
DEFINE_KEYFIELD(m_minigunpause_kv, FIELD_FLOAT, "MinigunFiringPause"),

DEFINE_INPUTFUNC( FIELD_VOID, "SetHullSizeSmall", InputSetHullSizeSmall ),
DEFINE_INPUTFUNC(FIELD_VOID, "SetHullSizeNormal", InputSetHullSizeNormal ),

DEFINE_INPUTFUNC(FIELD_VOID, "SetSalvoType_HomingVolley", InputSetSalvoType_HomingVolley),
DEFINE_INPUTFUNC(FIELD_VOID, "SetSalvoType_LinedVolley", InputSetSalvoType_LinedVolley),
DEFINE_INPUTFUNC(FIELD_VOID, "SetSalvoType_UmbrellaVolley", InputSetSalvoType_UmbrellaVolley),
DEFINE_INPUTFUNC(FIELD_VOID, "SetSalvoType_TwinMissilesDirect", InputSetSalvoType_TwinMissilesDirect),
DEFINE_INPUTFUNC(FIELD_VOID, "SetSalvoType_TwinMissilesArched", InputSetSalvoType_TwinMissilesArched),
DEFINE_INPUTFUNC(FIELD_VOID, "DispatchPStormKidnapFx", InputDispatchPStormKidnapFx),

DEFINE_INPUTFUNC(FIELD_INTEGER, "SetSalvoSize", InputSetSalvoSize),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetSalvoNextFireDelay", InputSetSalvoNextFireDelay),

DEFINE_INPUTFUNC(FIELD_VOID, "EnableChargeAttack", InputEnableChargeAttack),
DEFINE_INPUTFUNC(FIELD_VOID, "DisableChargeAttack", InputDisableChargeAttack),

DEFINE_INPUTFUNC(FIELD_VOID, "EnableMinigunAttack", InputEnableMinigunAttack),
DEFINE_INPUTFUNC(FIELD_VOID, "DisableMinigunAttack", InputDisableMinigunAttack),

DEFINE_INPUTFUNC(FIELD_FLOAT, "SetMinigunFiringDuration", InputSetMinigunFiringDuration),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetMinigunFiringPause", InputSetMinigunFiringPause),

DEFINE_INPUTFUNC(FIELD_VOID, "UnfoldWeapons", InputUnfoldGuns),
DEFINE_INPUTFUNC(FIELD_VOID, "FoldWeapons", InputFoldGuns),

DEFINE_INPUTFUNC(FIELD_VOID, "EnableSalvoAttack", InputEnableSalvoAttack),
DEFINE_INPUTFUNC(FIELD_VOID, "DisableSalvoAttack", InputDisableSalvoAttack),

DEFINE_INPUTFUNC(FIELD_VOID, "ForceStartMinigunAttack", InputForceStartMinigunAttack),
DEFINE_INPUTFUNC(FIELD_VOID, "ForceStartPulsegunAttack", InputForceStartPulsegunAttack),

DEFINE_INPUTFUNC(FIELD_FLOAT, "EnableShield", InputEnableShield), // input with amount of seconds the shield will be active
DEFINE_INPUTFUNC(FIELD_VOID, "DisableShield", InputDisableShield),
DEFINE_INPUTFUNC(FIELD_VOID, "EnableAutoRegen", InputEnableAutoRegen),
DEFINE_INPUTFUNC(FIELD_VOID, "DisableAutoRegen", InputDisableAutoRegen),

DEFINE_INPUTFUNC(FIELD_STRING, "DoBodyThrowTowardTarget", InputDoBodyThrowTowardTarget),

DEFINE_OUTPUT(m_OnFiredRockets, "OnFiredRockets" ),
DEFINE_OUTPUT(m_OnStartedSalvo, "OnStartedSalvo"),
DEFINE_OUTPUT(m_OnFinishedSalvo, "OnFinishedSalvo"),
DEFINE_OUTPUT(m_OnStartedMinigun, "OnStartedMinigun"),
DEFINE_OUTPUT(m_OnFinishedMinigun, "OnFinishedMinigun"),
DEFINE_OUTPUT(m_OnStartedCharge, "OnStartedCharge"),
DEFINE_OUTPUT(m_OnStartedRegen, "OnStartedRegen"),
DEFINE_OUTPUT(m_OnFinishedRegen, "OnFinishedRegen"),
DEFINE_OUTPUT(m_OnReachedLowHealthThreshold, "OnLowHealthThreshold"),

DEFINE_THINKFUNC(FireRocketsThink),
DEFINE_THINKFUNC(FireMinigunThink),
DEFINE_THINKFUNC(CarriedThink),
DEFINE_THINKFUNC(RegenThink),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CNPC_CrabSynth, DT_NPC_CrabSynth)
SendPropBool(SENDINFO(m_crabsynth_shield_enabled)),
#ifdef RIDEABLE_NPCS
SendPropEHandle(SENDINFO(m_hPlayer)),
#endif
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(npc_crabsynth, CNPC_CrabSynth);
LINK_ENTITY_TO_CLASS(npc_raider, CNPC_CrabSynth);
LINK_ENTITY_TO_CLASS(npc_stomper, CNPC_CrabSynth);

AI_BEGIN_CUSTOM_NPC(npc_crabsynth, CNPC_CrabSynth)

DECLARE_ANIMEVENT(AE_CRABSYNTH_CHARGE_START)
DECLARE_ANIMEVENT(AE_CRABSYNTH_CHARGE_END)
DECLARE_ANIMEVENT(AE_CRABSYNTH_MINIGUN_FIRE)
DECLARE_ANIMEVENT(AE_CRABSYNTH_PULSEGUN_FIRE)
DECLARE_ANIMEVENT(AE_CRABSYNTH_MELEE_PIN)
DECLARE_ANIMEVENT(AE_CRABSYNTH_MELEE_SWIPE)
DECLARE_ANIMEVENT(AE_CRABSYNTH_UNFOLDED_GUNS)
DECLARE_ANIMEVENT(AE_CRABSYNTH_FOLDED_GUNS)
DECLARE_CONDITION(COND_CRABSYNTH_ENTER_REGEN)
DECLARE_TASK(TASK_CRABSYNTH_SHOOT_ROCKETS)
DECLARE_TASK(TASK_CRABSYNTH_START_SALVO)
DECLARE_TASK(TASK_CRABSYNTH_STOP_SALVO)
DECLARE_TASK(TASK_CRABSYNTH_CHARGE)
DECLARE_TASK(TASK_CRABSYNTH_CHARGE_MINIGUN)
DECLARE_TASK(TASK_CRABSYNTH_LOOP_MINIGUN)
DECLARE_TASK(TASK_CRABSYNTH_STOP_MINIGUN)
DECLARE_TASK(TASK_CRABSYNTH_ANNOUNCE_REGEN)
DECLARE_TASK(TASK_CRABSYNTH_ENTER_REGEN)
DECLARE_TASK(TASK_CRABSYNTH_REGEN_WAIT)
DECLARE_TASK(TASK_CRABSYNTH_UNFOLD_WEAPONS)
DECLARE_TASK(TASK_CRABSYNTH_FOLD_WEAPONS)
//DECLARE_TASK(TASK_CRABSYNTH_START_BEAM)
//DECLARE_TASK(TASK_CRABSYNTH_STOP_BEAM)
DECLARE_ACTIVITY(ACT_CRABSYNTH_ROCKETS_START)
DECLARE_ACTIVITY(ACT_CRABSYNTH_ROCKETS_END)
DECLARE_ACTIVITY(ACT_CHARGE_START)
DECLARE_ACTIVITY(ACT_CHARGE_LOOP)
DECLARE_ACTIVITY(ACT_CHARGE_END)
DECLARE_ACTIVITY(ACT_CRABSYNTH_CUE)
DECLARE_ACTIVITY(ACT_CARRIED_BY_DROPSHIP)
DECLARE_ACTIVITY(ACT_CRABSYNTH_MINIGUN_START) 
DECLARE_ACTIVITY(ACT_CRABSYNTH_MINIGUN_LOOP)
DECLARE_ACTIVITY(ACT_CRABSYNTH_MINIGUN_GESTURE)
DECLARE_ACTIVITY(ACT_CRABSYNTH_MINIGUN_END)
DECLARE_ACTIVITY(ACT_CRABSYNTH_BODY_THROW)
DECLARE_ACTIVITY(ACT_CRABSYNTH_UNFOLD_WEAPONS)
DECLARE_ACTIVITY(ACT_CRABSYNTH_FOLD_WEAPONS)
DECLARE_ACTIVITY(ACT_CRABSYNTH_PULSE_FIRE)

DEFINE_SCHEDULE
(
	SCHED_CRABSYNTH_ATTACK_ROCKETS,

	"	Tasks"
	"		TASK_STOP_MOVING 0"
	"		TASK_FACE_ENEMY 0"
	"		TASK_PLAY_SEQUENCE ACTIVITY:ACT_CRABSYNTH_ROCKETS_START"
	"		TASK_CRABSYNTH_SHOOT_ROCKETS 0"
	"		TASK_PLAY_SEQUENCE ACTIVITY:ACT_CRABSYNTH_ROCKETS_END"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
)

DEFINE_SCHEDULE
(
	SCHED_CRABSYNTH_ATTACK_SALVO,

	"	Tasks"
	"		TASK_STOP_MOVING 0"
	"		TASK_FACE_ENEMY 0"
	"		TASK_PLAY_SEQUENCE ACTIVITY:ACT_CHARGE_START"
	"		TASK_CRABSYNTH_START_SALVO 0"
	"		TASK_CRABSYNTH_STOP_SALVO 0"
	"		TASK_PLAY_SEQUENCE ACTIVITY:ACT_CHARGE_END"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
)

DEFINE_SCHEDULE
(
	SCHED_CRABSYNTH_ATTACK_CHARGE,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_IDLE_STAND"
	"		TASK_STOP_MOVING				0"
	"		TASK_CRABSYNTH_FOLD_WEAPONS		0"
	"		TASK_FACE_ENEMY					0"
	"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_CRABSYNTH_CUE"
	"		TASK_CRABSYNTH_CHARGE			0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_ENEMY_DEAD"
)

DEFINE_SCHEDULE
(
	SCHED_CRABSYNTH_ATTACK_MINIGUN,

	"	Tasks"
	"		TASK_STOP_MOVING 0"
	"		TASK_FACE_ENEMY 0"
//	"		TASK_PLAY_SEQUENCE ACTIVITY:ACT_CRABSYNTH_MINIGUN_START"
//	"		TASK_SET_ACTIVITY ACTIVITY:ACT_CRABSYNTH_MINIGUN_LOOP"
	"		TASK_CRABSYNTH_CHARGE_MINIGUN 0"
	"		TASK_CRABSYNTH_LOOP_MINIGUN 0"
//	"		TASK_PLAY_SEQUENCE ACTIVITY:ACT_CRABSYNTH_MINIGUN_END"
	"		TASK_CRABSYNTH_STOP_MINIGUN 0"
	""
	"	Interrupts"
	"	"
)

DEFINE_SCHEDULE
(
	SCHED_CRABSYNTH_STRAFE_DURING_PAUSE,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE					SCHEDULE:SCHED_ESTABLISH_LINE_OF_FIRE" //SCHEDULE:SCHED_CRABSYNTH_RUN_RANDOM_256"
	"		TASK_SET_TOLERANCE_DISTANCE				48"
	"		TASK_SET_ROUTE_SEARCH_TIME				1"	// Spend 1 second trying to build a path if stuck
	"		TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS	25"
	"		TASK_RUN_PATH							0"
	"		TASK_WAIT_FOR_MOVEMENT					0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_HEAVY_DAMAGE"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK1"
)

DEFINE_SCHEDULE
(
	SCHED_CRABSYNTH_RUN_RANDOM_256,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_COMBAT_FACE"
	"		TASK_SET_TOLERANCE_DISTANCE		48"
	"		TASK_SET_ROUTE_SEARCH_TIME		1"	// Spend 1 second trying to build a path if stuck
	"		TASK_GET_PATH_TO_RANDOM_NODE	512"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_CAN_RANGE_ATTACK1"
)

DEFINE_SCHEDULE
(
	SCHED_CRABSYNTH_ENTER_REGEN,

	"	Tasks"
//	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_DIE"
//	"		TASK_STOP_MOVING 0"
//	"		TASK_FACE_ENEMY 0"
	"		TASK_CRABSYNTH_ANNOUNCE_REGEN	0"
	"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_ARM"
	"		TASK_CRABSYNTH_ENTER_REGEN		0"
	"		TASK_CRABSYNTH_REGEN_WAIT		0"
	""
	"	Interrupts"
	"	"
)

DEFINE_SCHEDULE
(
	SCHED_CRABSYNTH_BODYTHROW,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_IDLE_STAND"
	"		TASK_STOP_MOVING				0"
	"		TASK_FACE_SAVEPOSITION			0"
	"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_CRABSYNTH_BODY_THROW"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"		COND_TASK_FAILED"
)

DEFINE_SCHEDULE
(
	SCHED_CRABSYNTH_UNFOLD_WEAPONS,

	"	Tasks"
	"		TASK_STOP_MOVING 0"
	"		TASK_FACE_ENEMY 0"
	"		TASK_CRABSYNTH_UNFOLD_WEAPONS 0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	" "
)

DEFINE_SCHEDULE
(
	SCHED_CRABSYNTH_FOLD_WEAPONS,

	"	Tasks"
	"		TASK_STOP_MOVING 0"
//	"		TASK_FACE_ENEMY 0"
	"		TASK_CRABSYNTH_FOLD_WEAPONS 0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	" "
)

DEFINE_SCHEDULE
(
	SCHED_CRABSYNTH_ATTACK_PULSEGUN,

	"	Tasks"
	"		TASK_STOP_MOVING 0"
	"		TASK_FACE_ENEMY 0"
	"		TASK_PLAY_SEQUENCE ACTIVITY:ACT_CRABSYNTH_PULSE_FIRE"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"	"
)

AI_END_CUSTOM_NPC()

//=============================================================================
#ifdef RIDEABLE_NPCS
// Crab synth vehicle stuff:
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CrabSynth::EnterVehicle(CBaseCombatCharacter *pPlayer)
	{
		if (!pPlayer)
			return;

		// Remove any player who may be in the vehicle at the moment
		ExitVehicle(VEHICLE_ROLE_DRIVER);

		m_hPlayer = ToBasePlayer(pPlayer);

		m_savedViewOffset = pPlayer->GetViewOffset();
		pPlayer->SetViewOffset(vec3_origin);
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CNPC_CrabSynth::ExitVehicle(int iRole)
	{
		CBasePlayer *pPlayer = m_hPlayer;
		if (!pPlayer)
			return;

		m_hPlayer = NULL;
		pPlayer->SetViewOffset(m_savedViewOffset);
	}

	//-----------------------------------------------------------------------------
	// Purpose: Crane rotates around with +left and +right, and extends/retracts 
	//			the cable with +forward and +back.
	//-----------------------------------------------------------------------------
	void CNPC_CrabSynth::SetupMove(CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move)
	{
		// process input
#if 0
		if (ucmd->buttons & IN_MOVELEFT)
		{
		}
		else if (ucmd->buttons & IN_MOVERIGHT)
		{
		}
#endif
	}

	CNPC_CrabSynth *CCrabSynthServerVehicle::GetCrabSynth(void)
	{
		return (CNPC_CrabSynth*)GetDrivableVehicle();
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CCrabSynthServerVehicle::GetVehicleViewPosition(int nRole, Vector *pAbsOrigin, QAngle *pAbsAngles, float *pFOV)
	{
		Assert(nRole == VEHICLE_ROLE_DRIVER);
		CBasePlayer *pPlayer = ToBasePlayer(GetPassenger(nRole));
		Assert(pPlayer);
		*pAbsAngles = pPlayer->EyeAngles();
		int eyeAttachmentIndex = GetCrabSynth()->LookupAttachment("vehicle_driver_eyes");
		QAngle tmp;
		GetCrabSynth()->GetAttachment(eyeAttachmentIndex, *pAbsOrigin, tmp);
	}

	//-----------------------------------------------------------------------------
	// Purpose: Where does this passenger exit the vehicle?
	//-----------------------------------------------------------------------------
	bool CCrabSynthServerVehicle::GetPassengerExitPoint(int nRole, Vector *pExitPoint, QAngle *pAngles)
	{
		Assert(nRole == VEHICLE_ROLE_DRIVER);

		// Get our attachment point
		Vector vehicleExitOrigin;
		QAngle vehicleExitAngles;
		GetCrabSynth()->GetAttachment("vehicle_driver_exit", vehicleExitOrigin, vehicleExitAngles);
		*pAngles = vehicleExitAngles;

		// Make sure it's clear
		trace_t tr;
		AI_TraceHull(vehicleExitOrigin + Vector(0, 0, 64), 
			vehicleExitOrigin, VEC_HULL_MIN, VEC_HULL_MAX, MASK_SOLID, 
			GetCrabSynth(), COLLISION_GROUP_NONE, &tr);
		if (!tr.startsolid)
		{
			*pExitPoint = tr.endpos;
			return true;
		}

		// Worst case, jump out on top
		*pExitPoint = GetCrabSynth()->GetAbsOrigin();
		pExitPoint->z += GetCrabSynth()->WorldAlignMaxs()[2] + 50.0f;

		// pop out behind this guy a bit
		Vector forward;
		GetCrabSynth()->GetVectors(&forward, NULL, NULL);
		*pExitPoint -= forward * 64;

		return true;
	}
#endif // crab vehicle
#endif // CRABSYNTH

//====================================
// Tripod Hopper (Dark Interval)
//====================================
#if defined (HOPPER)
#define SF_HOPPER_ALWAYSHOSTILE 1 << 17
ConVar sk_tripodhopper_mingroupsize("sk_tripodhopper_mingroupsize", "3");
class CNPC_Hopper : public CAI_BaseActorNoBlend
{
	DECLARE_CLASS(CNPC_Hopper, CAI_BaseActorNoBlend);
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;

public:
	CNPC_Hopper();

	Class_T Classify(void);
	void Precache(void);
	void Spawn(void);
	void Activate();
	void NPCThink();
	void ThrowThink(void);

	void Event_Killed(const CTakeDamageInfo &info);

	void IdleSound(void);
	void AlertSound(void);
	void PainSound(const CTakeDamageInfo &info);
	void DeathSound(const CTakeDamageInfo &info);
	void BiteSound(void);
	void StabSound(void);
	void MissSound(void);
	void ImpactSound(void);
	void AnnounceAttackSound(void);

	void HandleAnimEvent(animevent_t *pEvent); 	
	bool HandleInteraction(int interactionType, void *data, CBaseCombatCharacter *sender = NULL);
	Activity NPC_TranslateActivity(Activity activity);
	float InnateRange1MinRange(void);
	float InnateRange1MaxRange(void);
	int MeleeAttack1Conditions(float flDot, float flDist); // running up to the prey and stabbing it
	int RangeAttack1Conditions(float flDot, float flDist); // headcrab-like jump
	void GatherConditions(void);

	bool IsValidEnemy(CBaseEntity *pEnemy)
	{
		// Hoppers given this flag by the mapper
		// can be hostile even by themselves
		if (HasSpawnFlags(SF_HOPPER_ALWAYSHOSTILE))
			return BaseClass::IsValidEnemy(pEnemy);

		// Hoppers are normally hostile to this tiny enemies
		if( pEnemy->ClassMatches("npc_bee"))
			return BaseClass::IsValidEnemy(pEnemy);

		// In other cases, they are only hostile if there're
		// enough of them in a group
	//	if (!GetSquad() || (GetSquad() && GetSquad()->NumMembers() <= sk_tripodhopper_mingroupsize.GetInt())) // if there's not enough of us, be cowardly
	//	{
	//			return false;
	//	} // decided to change this to have always neutral-by-defaul squads, that become hostile if attacked. See Event_Killed for more detail

		return BaseClass::IsValidEnemy(pEnemy);
	}

	int TranslateSchedule(int scheduleType);
	int SelectSchedule(void);
	void StartTask(const Task_t *pTask);
	void RunTask(const Task_t *pTask);

	void InputEnterSleep(inputdata_t &data);
	void InputExitSleep(inputdata_t &data);

	int m_dummy_int; 

	float GetMaxJumpSpeed() const { return 400.0f; }
	float GetJumpGravity() const { return 8.0f; }
	int GetSoundInterests(void) { return SOUND_DANGER | SOUND_COMBAT | SOUND_PHYSICS_DANGER | SOUND_BULLET_IMPACT; };
	bool AllowedToIgnite(void) { return true; }
	bool AllowedToIgniteByFlare(void) { return true; }
	bool ShouldGib(const CTakeDamageInfo &info) { return false; };

	bool IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const
	{

		const float JUMP_RISE = 200.0f;
		const float JUMP_DISTANCE = 300.0f;
		const float JUMP_DROP = 200.0f;

		return BaseClass::IsJumpLegal(startPos, apex, endPos, JUMP_RISE, JUMP_DROP, JUMP_DISTANCE);
	};

	bool MovementCost(int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost)
	{
		float delta = vecEnd.z - vecStart.z;

		float multiplier = 1;
		if (moveType == bits_CAP_MOVE_JUMP)
		{
			multiplier = (delta < 0) ? 0.5 : 0.5;
		}
		else if (moveType == bits_CAP_MOVE_GROUND)
		{
			multiplier = entindex() % 3 ? 0.2 : 2.0;
		}

		*pCost *= multiplier;

		return (multiplier != 1);
	}

	bool HasHeadroom();
	void JumpAttack(bool bRandomJump, const Vector &vecPos = vec3_origin, bool bThrown = false);
	void Leap(const Vector &vecVel);
protected:
	void LeapTouch(CBaseEntity *pOther);
	void MoveOrigin(const Vector &vecDelta);
private:
	float	m_ignoreworldcollision_float;
	bool	m_midjump_boolean;
	bool	m_attackfailed_boolean;
	float	m_nextthrowthink_float;
	Vector	m_jumpattack_vector;	// The position of our enemy when we locked in our jump attack.
	bool	m_committed_to_jump_boolean;		// Whether we have 'locked in' to jump at our enemy.
public:
	enum
	{
		SCHED_HOPPER_SLEEP = LAST_SHARED_SCHEDULE,
	};

	enum
	{
		TASK_HOPPER_ENTER_SLEEP = LAST_SHARED_TASK,
		TASK_HOPPER_SLEEP_WAIT,
		TASK_HOPPER_EXIT_SLEEP,
	};

	enum
	{
		COND_HOPPER_DISTURB_SLEEP = LAST_SHARED_CONDITION,
		COND_HOPPER_ENTER_SLEEP,
	};
};

int AE_HOPPER_JUMPATTACK;
int AE_HOPPER_JUMP_TELEGRAPH;
int AE_HOPPER_STAB;

// todo: we can do it without these. Have a vcd that controls the sack state
// and key it in during sleep tasks.
int AE_HOPPER_TAUT_SACK; // pull up the sack via flexes for sleeping, so it doesn't clip through the ground.
int AE_HOPPER_LOOSE_SACK; // unravel the sack while standing up.

ConVar sk_tripodhopper_stab_damage("sk_tripodhopper_stab_damage", "5");
ConVar sk_tripodhopper_leap_damage("sk_tripodhopper_leap_damage", "10");
ConVar sk_tripodhopper_min_attackrange("sk_tripodhopper_min_attackrange", "48");
ConVar sk_tripodhopper_max_attackrange("sk_tripodhopper_max_attackrange", "256");

// Constructor
CNPC_Hopper::CNPC_Hopper(void)
{
	m_hNPCFileInfo = LookupNPCInfoScript("npc_hopper");
	m_dummy_int = 69;
	m_ignoreworldcollision_float = 0;
}

Class_T	CNPC_Hopper::Classify(void)
{
	return CLASS_BULLSEYE; // currently, don't react to anything. 
}

// Generic spawn-related
void CNPC_Hopper::Precache()
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	PrecacheScriptSound("NPC_Hopper.Idle");
	PrecacheScriptSound("NPC_Hopper.Squeak");
	PrecacheScriptSound("NPC_Hopper.Die_Pop");
	PrecacheScriptSound("NPC_Hopper.Bite");
	PrecacheScriptSound("NPC_Hopper.Leap");
	PrecacheScriptSound("NPC_Hopper.Impact");
	PrecacheScriptSound("NPC_Hopper.Stab");
	PrecacheScriptSound("NPC_Hopper.StabHit");
	PrecacheScriptSound("NPC_Hopper.StabMiss");
	PrecacheScriptSound("NPC_Hopper.AnnounceAttack");
	PrecacheScriptSound("NPC_Hopper.Alert");
	PrecacheScriptSound("NPC_Hopper.Footstep_Walk");
	PrecacheScriptSound("NPC_Hopper.Footstep_Jump");
	PrecacheParticleSystem("antlion_gib_01");
	PrecacheInstancedScene("scenes/di_npc/hopper_pulsating.vcd");
	BaseClass::Precache();
}

void CNPC_Hopper::Spawn()
{
	Precache();
	SetModel(STRING(GetModelName()));

//	m_iszIdleExpression = MAKE_STRING("scenes/di_npc/hopper_pulsating.vcd");
//	m_iszAlertExpression = MAKE_STRING("scenes/di_npc/hopper_pulsating.vcd");
//	m_iszCombatExpression = MAKE_STRING("scenes/di_npc/hopper_pulsating.vcd");

	BaseClass::Spawn();
	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);
	
	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MoveType_t(GetNPCScriptData().NPC_Movement_MoveType_int));

	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_Climb_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_CLIMB);
	if ((GetNPCScriptData().NPC_Capable_Doors_boolean) == true) CapabilitiesAdd(bits_CAP_DOORS_GROUP);
	CapabilitiesAdd(bits_CAP_LIVE_RAGDOLL);
	
	//	m_bShouldPatrol = GetNPCScriptData().bShouldPatrolAfterSpawn;

	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));

	m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);
	
	SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;
	SetDistLook(512);
	m_NPCState = NPC_STATE_NONE;
	m_flNextAttack = CURTIME;

//	SetCollisionGroup(HL2COLLISION_GROUP_HEADCRAB);
	
	NPCInit();
}

void CNPC_Hopper::Activate()
{
	BaseClass::Activate();
}

void CNPC_Hopper::NPCThink()
{
	BaseClass::NPCThink();
}

void CNPC_Hopper::ThrowThink(void)
{
	if (CURTIME > m_nextthrowthink_float)
	{
		NPCThink();
		m_nextthrowthink_float = CURTIME + 0.1;
	}

	if (GetFlags() & FL_ONGROUND)
	{
		SetThink(&CNPC_Hopper::CallNPCThink);
		SetNextThink(CURTIME + 0.1);
		return;
	}

	SetNextThink(CURTIME);
}

// Damage handling
void CNPC_Hopper::Event_Killed(const CTakeDamageInfo &info)
{
	CTakeDamageInfo infoNew = info;
	AddEffects(EF_NODRAW);
	AddSolidFlags(FSOLID_NOT_SOLID);
	infoNew.SetDamageType(DMG_REMOVENORAGDOLL);

	// if we have a squad, tell everyone on our murderer
	if (infoNew.GetAttacker() && GetSquad() && GetSquad()->NumMembers() > 0)
	{
		AISquadIter_t iter;
		for (CAI_BaseNPC *pSquadMember = GetSquad()->GetFirstMember(&iter); pSquadMember; pSquadMember = GetSquad()->GetNextMember(&iter))
		{
			pSquadMember->AddEntityRelationship(infoNew.GetAttacker(), D_HT, 100);
			pSquadMember->m_flNextAlertSoundTime = CURTIME + RandomFloat(0, 1);
			if (pSquadMember->ClassMatches(GetClassName()))
			{
				variant_t var;
				var.SetInt(0);
				pSquadMember->AcceptInput("ExitSleep", this, this, var, 0);
			}
		}
		GetSquad()->UpdateEnemyMemory(this, infoNew.GetAttacker(), info.GetDamagePosition());
	}

	//	CPASAttenuationFilter filter(this);
	//	EmitSound(filter, entindex(), "NPC_Antlion.RunOverByVehicle");

	DispatchParticleEffect("antlion_gib_01", WorldSpaceCenter(), QAngle(0, 0, 0));

	// Spawn ragdoll gib (manually) and boogie it...
	if ((info.GetDamageType() & DMG_REMOVENORAGDOLL) == 0)
	{
		CBaseAnimating *pGibAnimating = dynamic_cast<CBaseAnimating*>(this);
		Vector vecGibForce;
		vecGibForce.x = random->RandomFloat(-400, 400);
		vecGibForce.y = random->RandomFloat(-400, 400);
		vecGibForce.z = random->RandomFloat(0, 300);

		CRagdollProp *pRagdollGib = (CRagdollProp *)CBaseEntity::CreateNoSpawn("prop_ragdoll", GetAbsOrigin(), GetAbsAngles(), NULL);
		pRagdollGib->CopyAnimationDataFrom(pGibAnimating);
		pRagdollGib->SetModelName(GetModelName());
		pRagdollGib->SetOwnerEntity(NULL);
		pRagdollGib->RemoveEffects(EF_NODRAW); // need to clear it, because original entity has it
		pRagdollGib->SetCollisionGroup(COLLISION_GROUP_INTERACTIVE_DEBRIS); // very annoying without it!
	//	pRagdollGib->AddSpawnFlags(131072); // 'allow pickup' flag
		pRagdollGib->Spawn();

		// Send the ragdoll into spasmatic agony
		inputdata_t input;
		input.pActivator = this;
		input.pCaller = this;
		input.value.SetFloat(4.0f); // N seconds of spasms
		pRagdollGib->InputStartRadgollBoogie(input);
	}
	BaseClass::Event_Killed(infoNew);
}

// NPC sounds
void CNPC_Hopper::IdleSound(void) 
{ 
	EmitSound("NPC_Hopper.Idle");
}

void CNPC_Hopper::AlertSound(void)
{
	if (CURTIME <= m_flNextAlertSoundTime)
	{//	EmitSound("NPC_Hopper.Squeak");
		EmitSound("NPC_Hopper.Alert");
		m_flNextAlertSoundTime = CURTIME + RandomFloat(5, 10);
	}
}

void CNPC_Hopper::PainSound(const CTakeDamageInfo &info)
{
	EmitSound("NPC_Hopper.Squeak");
}

void CNPC_Hopper::DeathSound(const CTakeDamageInfo &info)
{
	EmitSound("NPC_Hopper.Die_Pop");
}

void CNPC_Hopper::BiteSound(void)
{
	EmitSound("NPC_Hopper.Bite");
}

void CNPC_Hopper::StabSound(void)
{
	EmitSound("NPC_Hopper.StabHit");
}

void CNPC_Hopper::MissSound(void)
{
	EmitSound("NPC_Hopper.StabMiss");
}

void CNPC_Hopper::ImpactSound(void)
{
	EmitSound("NPC_Hopper.Impact");
}

void CNPC_Hopper::AnnounceAttackSound(void)
{
	EmitSound("NPC_Hopper.AnnounceAttack");
}

// Animations
void CNPC_Hopper::HandleAnimEvent(animevent_t *pEvent)
{	
	if (pEvent->event == AE_HOPPER_STAB)
	{
		float damage = sk_tripodhopper_stab_damage.GetFloat();
		float distance = 64;

		Vector vecForceDir;

		// Always hurt bullseyes for now
		if ((GetEnemy() != NULL) && (GetEnemy()->Classify() == CLASS_BULLSEYE))
		{
			vecForceDir = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
			CTakeDamageInfo info(this, this, damage, DMG_SLASH);
			CalculateMeleeDamageForce(&info, vecForceDir, GetEnemy()->GetAbsOrigin());
			GetEnemy()->TakeDamage(info);
			return;
		}

		CBaseEntity *pHurt = CheckTraceHullAttack(distance, -Vector(8, 8, 16), Vector(8, 8, 16), damage, DMG_SLASH, 5.0f);

		if (pHurt)
		{
			vecForceDir = (pHurt->WorldSpaceCenter() - WorldSpaceCenter());

			if ((abs)(pHurt->WorldAlignMaxs().z - pHurt->WorldAlignMins().z) >= 64) // can't hurt big things
			{
				return;
			}

			CBasePlayer *pPlayer = ToBasePlayer(pHurt);

			if (pPlayer != NULL)
			{
				//Kick the player angles
				if (!(pPlayer->GetFlags() & FL_GODMODE) && pPlayer->GetMoveType() != MOVETYPE_NOCLIP)
				{
					pPlayer->ViewPunch(QAngle(1, 0, 0));

					Vector	dir = pHurt->GetAbsOrigin() - GetAbsOrigin();
					VectorNormalize(dir);

					QAngle angles;
					VectorAngles(dir, angles);
					Vector forward, right;
					AngleVectors(angles, &forward, &right, NULL);
				}
			}

			// Play an attack hit sound
			StabSound();

			m_flNextAttack = CURTIME + RandomFloat(0.5f, 1.5f);
		}
		else
		{
			// Play an attack miss sound
			MissSound();
		}

		return;
	}
	
	if (pEvent->event == AE_HOPPER_JUMPATTACK)
	{
		// Ignore if we're in mid air
		if (m_midjump_boolean)
			return;

		CBaseEntity *pEnemy = GetEnemy();

		if (pEnemy)
		{
			if (m_committed_to_jump_boolean)
			{
				JumpAttack(false, m_jumpattack_vector);
			}
			else
			{
				// Jump at my enemy's eyes.
				JumpAttack(false, pEnemy->WorldSpaceCenter());
			}

			m_committed_to_jump_boolean = false;

		}
		else
		{
			// Jump hop, don't care where.
			JumpAttack(true);
		}

		return;
	}

	if (pEvent->event == AE_HOPPER_JUMP_TELEGRAPH)
	{
		AnnounceAttackSound();

		CBaseEntity *pEnemy = GetEnemy();

		if (pEnemy)
		{
			// Once we telegraph, we MUST jump. This is also when commit to what point
			// we jump at. Jump at our enemy's eyes.
			m_jumpattack_vector = pEnemy->EyePosition();
			m_committed_to_jump_boolean = true;
		}

		return;
	}

	if (pEvent->event == AE_HOPPER_TAUT_SACK)
	{
	//	SetFlexWeight("sack_taut", 0.2f);
	//	SetFlexWeight("sack_taut2", 1.0f);
		return;
	}

	if (pEvent->event == AE_HOPPER_LOOSE_SACK)
	{
	//	SetFlexWeight("sack_taut", 0.0f);
	//	SetFlexWeight("sack_taut2", 0.0f);
		return;
	}

	BaseClass::HandleAnimEvent(pEvent);
}

bool CNPC_Hopper::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	if (interactionType == g_interactionVortigauntStomp)
	{
		ClearSchedule("Being stomped by vortigaunt");
		GetMotor()->SetIdealYawToTargetAndUpdate(sourceEnt->GetAbsOrigin());
		SetTurnActivity();
		SetIdealState(NPC_STATE_PRONE);
		return true;
	}

	else if (interactionType == g_interactionVortigauntStompFail)
	{
		SetIdealState(NPC_STATE_COMBAT);
		return true;
	}

	else if (interactionType == g_interactionVortigauntStompHit)
	{
		ClearSchedule("Stomped by vortigaunt");
		OnTakeDamage(CTakeDamageInfo(sourceEnt, sourceEnt, m_iHealth, DMG_CRUSH ));
		return true;
	}

	else if (interactionType == g_interactionVortigauntKick
		/* || (interactionType ==	g_interactionBullsquidThrow) */
		)
	{
		ClearSchedule("Kicked by vortigaunt");
		SetIdealState(NPC_STATE_PRONE);
		
		Vector vHitDir = GetLocalOrigin() - sourceEnt->GetLocalOrigin();
		VectorNormalize(vHitDir);

		CTakeDamageInfo info(sourceEnt, sourceEnt, m_iHealth + 1, DMG_CLUB);
		CalculateMeleeDamageForce(&info, vHitDir, GetAbsOrigin());
		info.ScaleDamageForce(5.0f);

		TakeDamage(info);

		return true;
	}

	else if (interactionType == g_interactionSmallHydraStab)
	{
		ClearSchedule("Being stabbed by hydra");
		GetMotor()->SetIdealYawToTargetAndUpdate(sourceEnt->GetAbsOrigin());
		SetTurnActivity();
		SetIdealState(NPC_STATE_PRONE);
		return true;
	}

	else if (interactionType == g_interactionSmallHydraStabHit)
	{
		ClearSchedule("Stabbed by hydra");
		
		OnTakeDamage(CTakeDamageInfo(sourceEnt, sourceEnt, m_iHealth, DMG_CRUSH |/*DMG_ALWAYSGIB*/DMG_REMOVENORAGDOLL));
		// the hydra will create a fake ragdoll in our place and carry it

		return true;
	}

	else if (interactionType == g_interactionSmallHydraStabFail)
	{
		SetIdealState(NPC_STATE_COMBAT);
		return true;
	}

	return BaseClass::HandleInteraction(interactionType, data, sourceEnt);
}

Activity CNPC_Hopper::NPC_TranslateActivity(Activity activity)
{	
	if (m_NPCState == NPC_STATE_ALERT || m_NPCState == NPC_STATE_COMBAT)
	{
		if (activity == ACT_WALK)
		{
			return ACT_RUN;
		}

		if (activity == ACT_IDLE)
		{
			return ACT_IDLE_AGITATED;
		}
	}
	return BaseClass::NPC_TranslateActivity(activity);
}

// Conditions
#define HOPPER_MELEE_RANGE 64
int CNPC_Hopper::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (CURTIME < m_flNextAttack)
		return 0;

	if (flDot < 0.5f)
		return COND_NOT_FACING_ATTACK;

	float flAdjustedDist = HOPPER_MELEE_RANGE;
	
	if (flDist > flAdjustedDist)
		return COND_TOO_FAR_TO_ATTACK;

	trace_t	tr;
	AI_TraceHull(WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), -Vector(4, 4, 8), Vector(4, 4, 8), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction < 1.0f)
		return 0;
	
	return COND_CAN_MELEE_ATTACK1;
}

float CNPC_Hopper::InnateRange1MinRange(void)
{
	return sk_tripodhopper_min_attackrange.GetFloat();
}

float CNPC_Hopper::InnateRange1MaxRange(void)
{
	return sk_tripodhopper_max_attackrange.GetFloat();
}

int CNPC_Hopper::RangeAttack1Conditions(float flDot, float flDist)
{
	if (CURTIME < m_flNextAttack)
		return 0;

	if ((GetFlags() & FL_ONGROUND) == false)
		return 0;
	
	// This code stops lots of headcrabs swarming you and blocking you
	// whilst jumping up and down in your face over and over. It forces
	// them to back up a bit. If this causes problems, consider using it
	// for the fast headcrabs only, rather than just removing it.(sjb)
	if (flDist < sk_tripodhopper_min_attackrange.GetFloat())
		return COND_TOO_CLOSE_TO_ATTACK;

	if (flDist > sk_tripodhopper_max_attackrange.GetFloat())
		return COND_TOO_FAR_TO_ATTACK;

	// Make sure the way is clear!
	CBaseEntity *pEnemy = GetEnemy();
	if (pEnemy)
	{
		bool bEnemyIsBullseye = (FClassnameIs(pEnemy, "npc_bullseye"));

		trace_t tr;
		AI_TraceLine(EyePosition(), pEnemy->EyePosition(), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);

		if (tr.m_pEnt != GetEnemy())
		{
			if (!bEnemyIsBullseye || tr.m_pEnt != NULL)
				return COND_NONE;
		}

		if (GetEnemy()->EyePosition().z - 36.0f > GetAbsOrigin().z)
		{
			// Only run this test if trying to jump at a player who is higher up than me, else this 
			// code will always prevent a headcrab from jumping down at an enemy, and sometimes prevent it
			// jumping just slightly up at an enemy.
			Vector vStartHullTrace = GetAbsOrigin();
			vStartHullTrace.z += 1.0;

			Vector vEndHullTrace = GetEnemy()->EyePosition() - GetAbsOrigin();
			vEndHullTrace.NormalizeInPlace();
			vEndHullTrace *= 8.0;
			vEndHullTrace += GetAbsOrigin();

			AI_TraceHull(vStartHullTrace, vEndHullTrace, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, GetCollisionGroup(), &tr);

			if (tr.m_pEnt != NULL && tr.m_pEnt != GetEnemy())
			{
				return COND_TOO_CLOSE_TO_ATTACK;
			}
		}
	}

	return COND_CAN_RANGE_ATTACK1;
}

void CNPC_Hopper::GatherConditions(void)
{
	BaseClass::GatherConditions();

	if (m_NPCState == NPC_STATE_IDLE)
	{
		if (!HasCondition(COND_HOPPER_DISTURB_SLEEP))
		{
			if (HasCondition(COND_HEAR_DANGER) || HasCondition(COND_HEAR_BULLET_IMPACT) || HasCondition(COND_HEAR_COMBAT) ||
				HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE) || HasCondition(COND_REPEATED_DAMAGE))

				SetCondition(COND_HOPPER_DISTURB_SLEEP);
		}
	}
}

// Schedules, states, tasks
int CNPC_Hopper::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_ALERT_STAND: return SCHED_RUN_RANDOM; break;
	case SCHED_CHASE_ENEMY: return SCHED_MOVE_TO_WEAPON_RANGE; break;
	default: return BaseClass::TranslateSchedule(scheduleType); break;
	}
}

int CNPC_Hopper::SelectSchedule(void)
{
	if (HasCondition(COND_NEW_ENEMY))
	{
		return SCHED_WAKE_ANGRY;
	}

	if (m_NPCState == NPC_STATE_IDLE)
	{
		if (HasCondition(COND_HOPPER_ENTER_SLEEP))
			return SCHED_HOPPER_SLEEP;

		return SCHED_PATROL_WALK;
	}
	else if (m_NPCState == NPC_STATE_COMBAT)
	{
		if (HasCondition(COND_CAN_RANGE_ATTACK1))
		{
			return SCHED_RANGE_ATTACK1;
		}
		else if (HasCondition(COND_CAN_MELEE_ATTACK1))
		{
			return SCHED_MELEE_ATTACK1;
		}
		else
		{
			AlertSound();
			return SCHED_CHASE_ENEMY;
		}
	}
	
	if (HasCondition(COND_FLOATING_OFF_GROUND))
	{
		SetGravity(1.0);
		SetGroundEntity(NULL);
		return SCHED_FALL_TO_GROUND;
	}

	return BaseClass::SelectSchedule();
}

void CNPC_Hopper::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_HOPPER_ENTER_SLEEP:
	{
		ResetIdealActivity(ACT_CROUCH);
	}
	break;

	case TASK_HOPPER_SLEEP_WAIT:
	{
		ResetIdealActivity(ACT_CROUCHIDLE);
	}
	break;

	case TASK_HOPPER_EXIT_SLEEP:
	{
		ClearCondition(COND_HOPPER_DISTURB_SLEEP);
		ResetIdealActivity(ACT_STAND);
	}
	break; 
	
	case TASK_RANGE_ATTACK1:
	{
		SetIdealActivity(ACT_RANGE_ATTACK1);
	}
	break;
	
	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

void CNPC_Hopper::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_HOPPER_ENTER_SLEEP:
	{
		if(IsActivityFinished()) TaskComplete();
	}
	break;

	case TASK_HOPPER_SLEEP_WAIT:
	{
		if (HasCondition(COND_HOPPER_DISTURB_SLEEP)) TaskComplete();
	}
	break;

	case TASK_HOPPER_EXIT_SLEEP:
	{
		if (IsActivityFinished()) TaskComplete();
	}
	break;

	case TASK_RANGE_ATTACK1:
	case TASK_RANGE_ATTACK2:
	{
		if (IsActivityFinished())
		{
			TaskComplete();
			m_midjump_boolean = false;
			SetTouch(NULL);
			SetThink(&CNPC_Hopper::CallNPCThink);
			SetIdealActivity(ACT_IDLE);

			if (m_attackfailed_boolean)
			{
				// our attack failed because we just ran into something solid.
				// delay attacking for a while so we don't just repeatedly leap
				// at the enemy from a bad location.
				m_attackfailed_boolean = false;
				m_flNextAttack = CURTIME + 1.2f;
			}
		}
		break;
	}

	default:
		BaseClass::RunTask(pTask);
		break;
	}
}

// Inputs
void CNPC_Hopper::InputEnterSleep(inputdata_t &data)
{
	ClearSchedule("Forced to clear schedule via input");
	SetCondition(COND_HOPPER_ENTER_SLEEP);
}

void CNPC_Hopper::InputExitSleep(inputdata_t &data)
{
	SetCondition(COND_HOPPER_DISTURB_SLEEP);
}

// Functions used anywhere above
void CNPC_Hopper::JumpAttack(bool bRandomJump, const Vector &vecPos, bool bThrown)
{
	Vector vecJumpVel;
	if (!bRandomJump)
	{
		float gravity = GetCurrentGravity();
		if (gravity <= 1)
		{
			gravity = 1;
		}

		// How fast does the headcrab need to travel to reach the position given gravity?
		float flActualHeight = vecPos.z - GetAbsOrigin().z;
		float height = flActualHeight;
		if (height < 16)
		{
			height = 16;
		}
		else
		{
			float flMaxHeight = bThrown ? 400 : 64;
			if (height > flMaxHeight)
			{
				height = flMaxHeight;
			}
		}

		// overshoot the jump by an additional 8 inches
		// NOTE: This calculation jumps at a position INSIDE the box of the enemy (player)
		// so if you make the additional height too high, the crab can land on top of the
		// enemy's head.  If we want to jump high, we'll need to move vecPos to the surface/outside
		// of the enemy's box.

		float additionalHeight = 0;
		if (height < 32)
		{
			additionalHeight = 8;
		}

		height += additionalHeight;

		// NOTE: This equation here is from vf^2 = vi^2 + 2*a*d
		float speed = sqrt(2 * gravity * height);
		float time = speed / gravity;

		// add in the time it takes to fall the additional height
		// So the impact takes place on the downward slope at the original height
		time += sqrt((2 * additionalHeight) / gravity);

		// Scale the sideways velocity to get there at the right time
		VectorSubtract(vecPos, GetAbsOrigin(), vecJumpVel);
		vecJumpVel /= time;

		// Speed to offset gravity at the desired height.
		vecJumpVel.z = speed;

		// Don't jump too far/fast.
		float flJumpSpeed = vecJumpVel.Length();
		float flMaxSpeed = bThrown ? 1000.0f : 650.0f;
		if (flJumpSpeed > flMaxSpeed)
		{
			vecJumpVel *= flMaxSpeed / flJumpSpeed;
		}
	}
	else
	{
		//
		// Jump hop, don't care where.
		//
		Vector forward, up;
		AngleVectors(GetLocalAngles(), &forward, NULL, &up);
		vecJumpVel = Vector(forward.x, forward.y, up.z) * 350;
	}

//	AttackSound();
	Leap(vecJumpVel);
}

void CNPC_Hopper::Leap(const Vector &vecVel)
{
	SetTouch(&CNPC_Hopper::LeapTouch);

	SetCondition(COND_FLOATING_OFF_GROUND);
	SetGroundEntity(NULL);

	m_ignoreworldcollision_float = CURTIME + 0.5f;

	if (HasHeadroom())
	{
		// Take him off ground so engine doesn't instantly reset FL_ONGROUND.
		MoveOrigin(Vector(0, 0, 1));
	}

	SetAbsVelocity(vecVel);

	// Think every frame so the player sees the headcrab where he actually is...
	m_midjump_boolean = true;
	SetThink(&CNPC_Hopper::ThrowThink);
	SetNextThink(CURTIME);
}

bool CNPC_Hopper::HasHeadroom()
{
	trace_t tr;
	UTIL_TraceEntity(this, GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, 1), MASK_NPCSOLID, this, GetCollisionGroup(), &tr);	
	return (tr.fraction == 1.0);
}

// Purpose: LeapTouch - this is the headcrab's touch function when it is in the air.
void CNPC_Hopper::LeapTouch(CBaseEntity *pOther)
{
	m_midjump_boolean = false;

	if (IRelationType(pOther) == D_HT)
	{
		// Don't hit if back on ground
		if (!(GetFlags() & FL_ONGROUND))
		{
			if (pOther->m_takedamage != DAMAGE_NO)
			{
				BiteSound();

				CTakeDamageInfo info;
				info.Set(this, this, sk_tripodhopper_leap_damage.GetFloat(), DMG_SLASH);
				CalculateMeleeDamageForce(&info, GetAbsVelocity(), GetAbsOrigin());
				pOther->TakeDamage(info);

				// attack succeeded, so don't delay our next attack if we previously thought we failed
				m_attackfailed_boolean = false;
			}
			else
			{
				ImpactSound();
			}
		}
		else
		{
			ImpactSound();
		}
	}
	else if (!(GetFlags() & FL_ONGROUND))
	{
		// Still in the air...
		if (!pOther->IsSolid())
		{
			// Touching a trigger or something.
			return;
		}

		// just ran into something solid, so the attack probably failed.  make a note of it
		// so that when the attack is done, we'll delay attacking for a while so we don't
		// just repeatedly leap at the enemy from a bad location.
		m_attackfailed_boolean = true;

		if (CURTIME < m_ignoreworldcollision_float)
		{
			// Headcrabs try to ignore the world, static props, and friends for a 
			// fraction of a second after they jump. This is because they often brush
			// doorframes or props as they leap, and touching those objects turns off
			// this touch function, which can cause them to hit the player and not bite.
			// A timer probably isn't the best way to fix this, but it's one of our 
			// safer options at this point (sjb).
			return;
		}

		ImpactSound();
	}

	// Shut off the touch function.
	SetTouch(NULL);
	SetThink(&CNPC_Hopper::CallNPCThink);
}

void CNPC_Hopper::MoveOrigin(const Vector &vecDelta)
{
	UTIL_SetOrigin(this, GetLocalOrigin() + vecDelta);
}

// Datadesc
BEGIN_DATADESC(CNPC_Hopper)
	DEFINE_FIELD(m_dummy_int, FIELD_INTEGER ),
	DEFINE_FIELD(m_midjump_boolean, FIELD_BOOLEAN),
	DEFINE_FIELD(m_attackfailed_boolean, FIELD_BOOLEAN),
	DEFINE_FIELD(m_committed_to_jump_boolean, FIELD_BOOLEAN),
	DEFINE_FIELD(m_jumpattack_vector, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(m_ignoreworldcollision_float, FIELD_FLOAT),
	DEFINE_FIELD(m_nextthrowthink_float, FIELD_FLOAT),
	DEFINE_THINKFUNC(ThrowThink),
	DEFINE_ENTITYFUNC(LeapTouch),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnterSleep", InputEnterSleep),
	DEFINE_INPUTFUNC(FIELD_VOID, "ExitSleep", InputExitSleep),
END_DATADESC()
LINK_ENTITY_TO_CLASS(npc_hopper, CNPC_Hopper);

AI_BEGIN_CUSTOM_NPC(npc_hopper, CNPC_Hopper)
DECLARE_USES_SCHEDULE_PROVIDER(CAI_BaseNPC)
DECLARE_TASK(TASK_HOPPER_ENTER_SLEEP)
DECLARE_TASK(TASK_HOPPER_SLEEP_WAIT)
DECLARE_TASK(TASK_HOPPER_EXIT_SLEEP)
DECLARE_ANIMEVENT(AE_HOPPER_STAB)
DECLARE_ANIMEVENT(AE_HOPPER_JUMPATTACK)
DECLARE_ANIMEVENT(AE_HOPPER_JUMP_TELEGRAPH)
DECLARE_ANIMEVENT(AE_HOPPER_TAUT_SACK)
DECLARE_ANIMEVENT(AE_HOPPER_LOOSE_SACK)
DECLARE_CONDITION(COND_HOPPER_DISTURB_SLEEP)
DECLARE_CONDITION(COND_HOPPER_ENTER_SLEEP)
DEFINE_SCHEDULE
(
	SCHED_HOPPER_SLEEP,

	" Tasks"
	" TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_IDLE_STAND"
	" TASK_STOP_MOVING					0"
	" TASK_HOPPER_ENTER_SLEEP			0"
	" TASK_HOPPER_SLEEP_WAIT			0"
	" TASK_HOPPER_EXIT_SLEEP			0"
	" TASK_WAIT_FOR_MOVEMENT			0"
	""
	" Interrupts"
	" COND_TASK_FAILED"
)

AI_END_CUSTOM_NPC()
#endif // HOPPER

//====================================
// Mini-barnacle
//====================================
#if defined (MINIBARNACLE)
class MiniBarnacle : public CBaseAnimating
{
	DECLARE_CLASS(MiniBarnacle, CBaseAnimating);
public:
	int ObjectCaps(void)
	{
		return BaseClass::ObjectCaps() | FCAP_DONT_TRANSITION_EVER;
	}
	void Activate(void);
	void Spawn(void);
	void Precache(void);
	void Event_Killed(const CTakeDamageInfo &info);
	DECLARE_DATADESC();
};

void MiniBarnacle::Precache(void)
{
	BaseClass::Precache();
	PrecacheModel("models/_Monsters/Xen/minibarnacle.mdl");
	PrecacheParticleSystem("blood_impact_zombie_01");
	PrecacheParticleSystem("blood_impact_red_01_mist");
	PrecacheParticleSystem("blood_impact_red_01_smalldroplets");
	PrecacheScriptSound("NPC_Barnacle.MiniDie");
}

void MiniBarnacle::Spawn(void)
{
	Precache();
	SetModel("models/_Monsters/Xen/minibarnacle.mdl");
	BaseClass::Spawn();

	AddEffects(EF_NOSHADOW);
	SetSolid(SOLID_VPHYSICS);
	SetSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MOVETYPE_NONE);


	m_takedamage = DAMAGE_YES;
	SetHealth(10);

	// Start our idle activity // currently don't have any animations! consider VTA?
	SetSequence(SelectWeightedSequence(ACT_IDLE));
	SetCycle(random->RandomFloat(0.0f, 1.0f));
	ResetSequenceInfo();

//	SetFadeDistance(900, 1000);
}

void MiniBarnacle::Activate(void)
{
	BaseClass::Activate();
}

void MiniBarnacle::Event_Killed(const CTakeDamageInfo &info)
{
	EmitSound("NPC_Barnacle.MiniDie");

	Vector vecOrigin;
	vecOrigin = WorldSpaceCenter();
	DispatchParticleEffect("blood_impact_zombie_01", vecOrigin, QAngle(0, 0, 0));
	DispatchParticleEffect("blood_impact_red_01_mist", vecOrigin, QAngle(0, 0, 0));
	DispatchParticleEffect("blood_impact_red_01_smalldroplets", vecOrigin, QAngle(0, 0, 0));
	
	trace_t tr;
	Vector	vecStart;
	Vector	vecTraceDir;

	GetVectors(NULL, NULL, &vecTraceDir);
	vecTraceDir.Negate();

	// One separate trace to always place blood where our roots used to be
	vecStart.x = vecOrigin.x;
	vecStart.y = vecOrigin.y;
	vecStart.z = vecOrigin.z - 8;

	UTIL_TraceLine(vecStart, vecStart + Vector(0,0,32), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction != 1.0)
	{
		UTIL_BloodDecalTrace(&tr, BLOOD_COLOR_GREEN);
	}

	for (int i = 0; i < 8; i++)
	{
		vecStart.x = vecOrigin.x + random->RandomFloat(-16.0f, 16.0f);
		vecStart.y = vecOrigin.y + random->RandomFloat(-16.0f, 16.0f);
		vecStart.z = vecOrigin.z - 8;

		UTIL_TraceLine(vecStart, vecStart + (vecTraceDir * (5 * 12)), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

		if (tr.fraction != 1.0)
		{
			UTIL_BloodDecalTrace(&tr, BLOOD_COLOR_GREEN);
		}
	}

	UTIL_Remove(this);
}

BEGIN_DATADESC(MiniBarnacle)
END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_barnacle_mini, MiniBarnacle);
LINK_ENTITY_TO_CLASS(npc_minibarnacle, MiniBarnacle);
LINK_ENTITY_TO_CLASS(minibarnacle, MiniBarnacle);
#endif // MINIBARNACLE

//====================================
// Mortar Synth (Dark Interval)
//====================================
#if defined (GLIDERSYNTH)
//#define GLIDER_BASED_ON_SCANNER
//#define GLIDER_MAESTRAS_MSYNTH_CODE
#ifdef GLIDER_BASED_ON_SCANNER
class CNPC_Glider : public CNPC_BaseScanner
{
	DECLARE_CLASS(CNPC_Glider, CNPC_BaseScanner);
#else
class CNPC_Glider : public CAI_BaseNPC
{
	DECLARE_CLASS(CNPC_Glider, CAI_BaseNPC);
#endif
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
public:
	CNPC_Glider();
	Class_T Classify(void) { return CLASS_COMBINE; }
	Disposition_t	IRelationType(CBaseEntity *pTarget);
	void	Precache(void);
	void	Spawn(void);
	void	RunAI(void);

	void    TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr);
	int		OnTakeDamage_Alive(const CTakeDamageInfo &info);
	void	Event_Killed(const CTakeDamageInfo &info);
	void	LaunchProjectile(CBaseEntity *pEnemy);
	void	InputLaunchProjectileAtTarget(inputdata_t &inputdata);
	void	InputForceDodge(inputdata_t &inputdata);

	void	AlertSound(void);
	
	void	HandleAnimEvent(animevent_t *pEvent);

	void	StartTask(const Task_t *pTask);
	void	RunTask(const Task_t *pTask);
	int		SelectSchedule(void); 
	int		SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode);
	int		TranslateSchedule(int scheduleType);
	//	void	GatherConditions(void);
	void	ClearAttackConditions(void);
	void	PrescheduleThink(void);
	Activity NPC_TranslateActivity(Activity eNewActivity);

	Vector	m_vecSaveProjectileVelocity;

	float	InnateRange1MinRange(void) { return 350.0f; }
	float	InnateRange1MaxRange(void) { return 800.0f; }
	int		RangeAttack1Conditions(float flDot, float flDist);

	float	MaxYawSpeed(void) { return 25; }
#ifdef GLIDER_MAESTRAS_MSYNTH_CODE
protected :
	virtual float	MinGroundDist(void);
	virtual void	MoveExecute_Alive(float flInterval);
	virtual bool	OverridePathMove(CBaseEntity *pMoveTarget, float flInterval);
	bool			OverrideMove(float flInterval);
	void			MoveToTarget(float flInterval, const Vector &MoveTarget);
	virtual void	MoveToAttack(float flInterval);
	virtual void	TurnHeadToTarget(float flInterval, const Vector &moveTarget);
	void			PlayFlySound(void);
private:
	Vector m_vTargetBanking;
public:
#else
	bool	OverrideMove(float flInterval);
	float	GetDefaultNavGoalTolerance();
	void	MoveFlyExecute(CBaseEntity *pTargetEnt, const Vector & vecDir, float flDistance, float flInterval);
	void	DoMovement(float flInterval, const Vector &MoveTarget);
	void	SteerArrive(Vector &Steer, const Vector &Target);
	void	SteerSeek(Vector &Steer, const Vector &Target);
	bool	SteerAvoidObstacles(Vector &Steer, const Vector &Velocity, const Vector &Forward, const Vector &Right, const Vector &Up);
	void	ClampSteer(Vector &SteerAbs, Vector &SteerRel, Vector &forward, Vector &right, Vector &up);
	void	AddHoverNoise(Vector *velocity);
	bool	OverrideMoveFacing(const AILocalMoveGoal_t &move, float flInterval);
//	void	TurnHeadToTarget(float flInterval, const Vector &moveTarget);
#endif
	float	GetGroundSpeed(void)
	{
		return m_flGroundSpeed;
	}

	static const Vector	m_vecAccelerationMax;
	static const Vector	m_vecAccelerationMin;
	Vector	m_vecLastMoveTarget;
	bool	m_bHasMoveTarget;
	bool	m_bIgnoreSurface;

private:
	Vector		m_vCurrentBanking;
	CSoundPatch	*m_pEngineSound;
	CSprite		*m_pEyeGlow;
	Activity	m_dodge_activity;

	// Custom schedules
	enum
	{
		SCHED_GLIDER_RANGE_ATTACK1 = BaseClass::NEXT_SCHEDULE,
		SCHED_GLIDER_DODGE,
		SCHED_GLIDER_FAIL_DODGE,

		TASK_GLIDER_FIND_DODGE_POSITION = BaseClass::NEXT_TASK,
		TASK_GLIDER_DODGE,
	};
};

const Vector CNPC_Glider::m_vecAccelerationMax = Vector(1024, 1024, 128);
const Vector CNPC_Glider::m_vecAccelerationMin = Vector(-1024, -1024, -512);

ConVar npc_glider_projectile_radius("npc_glider_projectile_radius", "250.0");
ConVar npc_glider_clusterbomb_z("npc_glider_clusterbomb_z", "250");
ConVar npc_glider_clusterbomb_g("npc_glider_clusterbomb_g", "800");

CNPC_Glider::CNPC_Glider()
{
	m_hNPCFileInfo = LookupNPCInfoScript("npc_glider");
	
	m_pEngineSound = NULL;
}

Disposition_t CNPC_Glider::IRelationType(CBaseEntity *pTarget)
{
	Disposition_t disp = BaseClass::IRelationType(pTarget);

	if (pTarget == NULL)
		return disp;
	
	// If the player's not a criminal, then we don't necessary hate him
	if (pTarget->Classify() == CLASS_PLAYER)
	{
		return GlobalEntity_GetState("gordon_precriminal") == 0 ? D_HT : D_NU;
	}

	if (GetEnemy() && pTarget != GetEnemy())
		return D_NU; // ignore this new target if already have a target

	if (pTarget->ClassMatches("npc_headcrab"))
		return D_NU; // too small fish 

	return disp;
}

void CNPC_Glider::Precache(void)
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	PropBreakablePrecacheAll(MAKE_STRING(pModelName));
	PrecacheScriptSound("NPC_Glider.Alert");
	PrecacheScriptSound("NPC_Glider.Hover");
	PrecacheScriptSound("NPC_Glider.AttackCue");
	PrecacheScriptSound("NPC_Glider.Pain");
	PrecacheScriptSound("NPC_Glider.Explode");
	PrecacheModel("sprites/glow1.vmt");
	PrecacheParticleSystem("blood_impact_synth_01");
	PrecacheParticleSystem("blood_impact_synth_01_arc_parent");
	PrecacheParticleSystem("blood_spurt_synth_01");
	PrecacheParticleSystem("blood_drip_synth_01");
	PrecacheParticleSystem("hunter_projectile_explosion_1");
	PrecacheModel("models/_Monsters/Xen/squid_spitball_large.mdl");
	UTIL_PrecacheOther("shared_projectile");
	BaseClass::Precache();
}

void CNPC_Glider::Spawn(void)
{
	Precache();
//	BaseClass::Spawn();

	SetModel(STRING(GetModelName()));

	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetCollisionGroup(HL2COLLISION_GROUP_GUNSHIP); // FIXME:: make separate group?
	SetMoveType(MoveType_t(GetNPCScriptData().NPC_Movement_MoveType_int));
	SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
	SetHullSizeNormal();
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;
	
	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));
	m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);
	
	NPCInit();

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_FLY);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);

	AddFlag(FL_FLY | FL_STEPMOVEMENT);
	SetBoneCacheFlags(BCF_NO_ANIMATION_SKIP);
	SetNavType(NAV_FLY);
	m_flGroundSpeed = 900;
	m_bIgnoreSurface = false;
	m_NPCState = NPC_STATE_NONE;
	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);
	SetDistLook(2048);

	m_flNextAttack = CURTIME + 0.5f;
}

void CNPC_Glider::RunAI(void)
{
	BaseClass::RunAI();
	SetNextThink(CURTIME + 0.01f);
}

void CNPC_Glider::TraceAttack(const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr)
{
	CTakeDamageInfo info = inputInfo;

	// Even though the damage might not hurt us, we want to react to it
	// if it's from the player.
	if (info.GetAttacker()->IsPlayer())
	{
		if (!HasMemory(bits_MEMORY_PROVOKED))
		{
			GetEnemies()->ClearMemory(info.GetAttacker());
			Remember(bits_MEMORY_PROVOKED);
			SetCondition(COND_LIGHT_DAMAGE);
		}
	}

	// Gliders have special resisitance to some types of damage.
	if ((info.GetDamageType() & DMG_BULLET) ||
		(info.GetDamageType() & DMG_BUCKSHOT) ||
		(info.GetDamageType() & DMG_CLUB) ||
		(info.GetDamageType() & DMG_NEVERGIB))
	{
		float flScale = 0.75;

		if (info.GetDamageType() & DMG_BUCKSHOT)
		{
			flScale = 0.5f;
			if(RandomInt(0,2) == 0)
				g_pEffects->Ricochet(ptr->endpos, ptr->plane.normal);
		}
		else if ((info.GetDamageType() & DMG_BULLET) || (info.GetDamageType() & DMG_NEVERGIB))
		{
			// Gliders resist most bullet damage, but they are vulnerable to armour-piercing rounds, 
			// since players regard that weapon as one of the game's truly powerful weapons.
			if (info.GetAmmoType() == GetAmmoDef()->Index("357") || info.GetAmmoType() == GetAmmoDef()->Index("OICW"))
			{
				flScale = 1.2f;
				UTIL_BloodImpact(ptr->endpos, ptr->plane.normal, BloodColor(), RandomInt(3, 6));
			}
			else
			{
				flScale = 0.6f;
			}
		}

		if (flScale != 0)
		{
			info.ScaleDamage(flScale);
		}

		QAngle vecAngles;
		VectorAngles(ptr->plane.normal, vecAngles);
		DispatchParticleEffect("blood_impact_synth_01", ptr->endpos, vecAngles);
		DispatchParticleEffect("blood_impact_synth_01_arc_parent", PATTACH_POINT_FOLLOW, this, 4);
	}

	BaseClass::TraceAttack(info, vecDir, ptr);
}

int	CNPC_Glider::OnTakeDamage_Alive(const CTakeDamageInfo &info)
{
//	if (!(info.GetAttacker()->IsPlayer()))
//		return 0;
/*
	// Make sure they ignore a bunch of conditions
	static int g_Conditions[] =
	{
		COND_LIGHT_DAMAGE,
		COND_HEAVY_DAMAGE,
		COND_PHYSICS_DAMAGE,
		COND_REPEATED_DAMAGE,
	};

	if (IsCurSchedule(SCHED_RANGE_ATTACK1))
	{
		// Don't let these conditions interrupt attach.
		SetIgnoreConditions(g_Conditions, ARRAYSIZE(g_Conditions));
		GetEnemies()->SetFreeKnowledgeDuration(3.0f);
	}
*/
	if (m_flNextDodgeTime < CURTIME)
	{
		EmitSound("NPC_Glider.Pain");
		m_flNextDodgeTime = CURTIME + RandomFloat(1.5, 3.0f);
	}

	return BaseClass::OnTakeDamage_Alive(info);
}

void CNPC_Glider::Event_Killed(const CTakeDamageInfo &info)
{
	if (IsMarkedForDeletion())
		return;

	CBaseEntity *pBreakEnt = this;
	Vector angVelocity;
	QAngleToAngularImpulse(pBreakEnt->GetLocalAngularVelocity(), angVelocity);
	PropBreakableCreateAll(pBreakEnt->GetModelIndex(), pBreakEnt->VPhysicsGetObject(), pBreakEnt->GetAbsOrigin(), RandomAngle(0,180), /*pBreakEnt->GetAbsVelocity()*/ Vector(0,0,100), angVelocity, 1.0, 200, COLLISION_GROUP_INTERACTIVE_DEBRIS, pBreakEnt, false);
	
	// Random chance to spawn a battery
	if (!HasSpawnFlags(SF_NPC_NO_WEAPON_DROP) && random->RandomFloat(0.0f, 1.0f) < 0.7f)
	{
		int random_i = RandomInt(1, 2);
		for (int i = 0; i < random_i; i++)
		{
			CItem *pBattery = (CItem*)CreateEntityByName("item_battery");
			if (pBattery)
			{
				pBattery->SetAbsOrigin(GetAbsOrigin());
				pBattery->SetAbsVelocity(GetAbsVelocity());
				pBattery->SetLocalAngularVelocity(GetLocalAngularVelocity());
				pBattery->ActivateWhenAtRest();
				pBattery->Spawn();
			}
		}
	}

	DispatchParticleEffect("antlion_spit", EyePosition(), QAngle(0, 0, 0));
	DispatchParticleEffect("hunter_projectile_explosion_1", EyePosition(), QAngle(0, 0, 0));
	DispatchParticleEffect("blood_impact_synth_01", EyePosition(), QAngle(0, 0, 0));
		
	// Sparks
	for (int i = 0; i < 4; i++)
	{
		Vector sparkPos = GetAbsOrigin();
		sparkPos.x += random->RandomFloat(-12, 12);
		sparkPos.y += random->RandomFloat(-12, 12);
		sparkPos.z += random->RandomFloat(-12, 12);
		g_pEffects->Sparks(sparkPos);
	}

	// Cover the gib spawn
	ExplosionCreate(WorldSpaceCenter(), GetAbsAngles(), this, 64, 64, false);

	// Throw out an extra attack unless we just did
//	if (CURTIME > m_last_attack_time + 1)
//	{
//		LaunchProjectile(GetEnemy());
//	}

	EmitSound("NPC_Glider.Explode");

	BaseClass::StopLoopingSounds();
	
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if (m_pEngineSound != NULL)
	{
		controller.SoundDestroy(m_pEngineSound);
		m_pEngineSound = NULL;
	}

	if (m_pEyeGlow)
	{
		UTIL_Remove(m_pEyeGlow);
	}

	m_OnDeath.FireOutput(this, this);

	UTIL_Remove(this);
}

void CNPC_Glider::AlertSound(void)
{
	if (m_flNextAlertSoundTime < CURTIME)
	{
		EmitSound("NPC_Glider.Alert");
		m_flNextAlertSoundTime = CURTIME + RandomFloat(2, 5);
	}
}

void CNPC_Glider::HandleAnimEvent(animevent_t *pEvent)
{
	if (pEvent->event == AE_LAUNCH_PROJECTILE)
	{
		if (GetEnemy() != NULL)
		{
			LaunchProjectile(GetEnemy());
			return;
		}
		else
			return;
	}
	BaseClass::HandleAnimEvent(pEvent);
}

void CNPC_Glider::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_GLIDER_FIND_DODGE_POSITION:
	{
		if (GetEnemy() == NULL)
		{
			TaskFail("No enemy to dodge");
			return;
		}
		
		// Check left or right randomly first.
		bool bDodgeLeft = false;
		if (random->RandomInt(0, 1) == 0)
		{
			bDodgeLeft = true;
		}

		bool bFoundDir = false;
		int nTries = 0;

		while (!bFoundDir && (nTries < 2))
		{
			// Pick a dodge activity to try.
			if (bDodgeLeft)
			{
				m_dodge_activity = ACT_ROLL_LEFT;
			}
			else
			{
				m_dodge_activity = ACT_ROLL_RIGHT;
			}

			// See where the dodge will put us.
			Vector vecLocalDelta;
			int nSeq = SelectWeightedSequence(m_dodge_activity);
			GetSequenceLinearMotion(nSeq, &vecLocalDelta);

			// Transform the sequence delta into local space.
			matrix3x4_t fRotateMatrix;
			AngleMatrix(GetLocalAngles(), fRotateMatrix);
			Vector vecDelta;
			VectorRotate(vecLocalDelta, fRotateMatrix, vecDelta);

			// Trace a bit high so this works better on uneven terrain.
			Vector testHullMins = GetHullMins();
			testHullMins.z += (StepHeight() * 2);

			// See if all is clear in that direction.
			trace_t tr;

			Ray_t ray;
			ray.Init(GetAbsOrigin(), GetAbsOrigin() + vecDelta, testHullMins, GetHullMaxs());
			CCrabSynthTraceFilterSkipPhysics traceFilter(this, GetCollisionGroup(), 100);
			enginetrace->TraceRay(ray, MASK_NPCSOLID, &traceFilter, &tr);

			// TODO: dodge anyway if we'll make it a certain percentage of the way through the dodge?
			if (tr.fraction == 1.0f)
			{
			//	NDebugOverlay::SweptBox( GetAbsOrigin(), GetAbsOrigin() + vecDelta, testHullMins, GetHullMaxs(), QAngle( 0, 0, 0 ), 0, 255, 0, 128, 5 );
				bFoundDir = true;
				TaskComplete();
			}
			else
			{
			//	NDebugOverlay::SweptBox( GetAbsOrigin(), GetAbsOrigin() + vecDelta, testHullMins, GetHullMaxs(), QAngle( 0, 0, 0 ), 255, 0, 0, 128, 5 );
				nTries++;
				bDodgeLeft = !bDodgeLeft;
			}
		}

		if (nTries < 2)
		{
			TaskComplete();
		}
		else
		{
			TaskFail("Couldn't find dodge position\n");
		}
		break;
	}
	case TASK_GLIDER_DODGE:
	{
		SetIdealActivity(m_dodge_activity);
		break;
	}
	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

void CNPC_Glider::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_GLIDER_DODGE:
	{
		AutoMovement();

		if (IsActivityFinished())
		{
			m_flNextDodgeTime = CURTIME + 2;
		//	m_flNextAttack = CURTIME; // too OP
			TaskComplete();
		}
		break;
	}
	default:
		BaseClass::RunTask(pTask);
		break;
	}
}

ConVar npc_glider_groundspeed("npc_glider_groundspeed", "-1");
void CNPC_Glider::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();

	if (IsCurSchedule(SCHED_IDLE_STAND) || IsCurSchedule(SCHED_IDLE_WANDER) || IsCurSchedule(SCHED_MOVE_TO_WEAPON_RANGE))
		SetCustomInterruptCondition(COND_CAN_RANGE_ATTACK1);
	else
		ClearCustomInterruptCondition(COND_CAN_RANGE_ATTACK1);

	// Make sure they ignore a bunch of conditions
	static int g_IgnoreConditions[] =
	{
		COND_WEAPON_BLOCKED_BY_FRIEND,
		COND_LOST_ENEMY,
		COND_LOST_PLAYER,
		COND_WEAPON_SIGHT_OCCLUDED,
	};

	if (IsCurSchedule(SCHED_RANGE_ATTACK1))
		SetIgnoreConditions(g_IgnoreConditions, ARRAYSIZE(g_IgnoreConditions));
	else
		ClearIgnoreConditions(g_IgnoreConditions, ARRAYSIZE(g_IgnoreConditions));

	if (m_NPCState == NPC_STATE_IDLE)
		m_flGroundSpeed = 450.0f;
	else if (m_NPCState == NPC_STATE_SCRIPT )
		m_flGroundSpeed = 150.0f;
	else if (m_NPCState == NPC_STATE_ALERT)
		m_flGroundSpeed = 500.0f;
	else if (m_NPCState == NPC_STATE_COMBAT)
		m_flGroundSpeed = 500.0f; // not a typo

	if (npc_glider_groundspeed.GetFloat() >= 0) m_flGroundSpeed = npc_glider_groundspeed.GetFloat();
/*
	if (m_flLastDamageTime + 0.1f < CURTIME)
	{
		ClearCondition(COND_LIGHT_DAMAGE); // don't keep it around, it's used to trigger flying away in SelectSchedule()
	}

	static int g_Conditions[] =
	{
		COND_LIGHT_DAMAGE,
		COND_HEAVY_DAMAGE,
		COND_PHYSICS_DAMAGE,
		COND_REPEATED_DAMAGE,
	};
	
	// Clear after setting these in OnTakeDamageAlive
	ClearIgnoreConditions(g_Conditions, ARRAYSIZE(g_Conditions));
*/
	if (!IsMarkedForDeletion())
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		if (m_pEngineSound == NULL)
		{
			// Create the sound
			CPASAttenuationFilter filter(this);

			m_pEngineSound = controller.SoundCreate(filter, entindex(), CHAN_STATIC, "NPC_Glider.Hover", ATTN_NORM);

			Assert(m_pEngineSound);

			// Start the engine sound
			controller.Play(m_pEngineSound, 0.0f, 100.0f);
			controller.SoundChangeVolume(m_pEngineSound, 1.0f, 0.25f);
		}

		float	speed = GetAbsVelocity().Length();
		float	flVolume = 0.25f + (0.75f*(speed / m_flGroundSpeed));
		int		iPitch = MIN(255, 80 + (20 * (speed / m_flGroundSpeed)));

		//Update our pitch and volume based on our speed
		controller.SoundChangePitch(m_pEngineSound, iPitch, 0.1f);
		controller.SoundChangeVolume(m_pEngineSound, flVolume, 0.1f);
	}
	// Create our Eye sprite
	if (m_pEyeGlow == NULL)
	{
		m_pEyeGlow = CSprite::SpriteCreate("sprites/glow1.vmt", GetAbsOrigin(), false);
		m_pEyeGlow->SetAttachment(this, LookupAttachment("sprite"));

		m_pEyeGlow->SetTransparency(kRenderTransAdd, 5, 50, 255, 128, kRenderFxNoDissipation);
		m_pEyeGlow->SetColor(5, 50, 255);

		m_pEyeGlow->SetBrightness(164, 0.1f);
		m_pEyeGlow->SetScale(0.75f, 0.1f);
		m_pEyeGlow->SetAsTemporary();
	}
}

void CNPC_Glider::ClearAttackConditions()
{
#if 1
	// Range attack 1 is the projectile attack.
	bool fCanRangeAttack1 = HasCondition(COND_CAN_RANGE_ATTACK1);

	// Call the base class.
	BaseClass::ClearAttackConditions();

	if (fCanRangeAttack1)
	{
		// We don't allow the base class to clear this condition because we
		// don't sense for it every frame.
		SetCondition(COND_CAN_RANGE_ATTACK1);
	}
#else
	BaseClass::ClearAttackConditions();
#endif
}

void CNPC_Glider::LaunchProjectile(CBaseEntity *pEnemy)
{
	Assert(pEnemy != NULL);
	Vector vSpitPos, vTarget;
	GetAttachment("launch_point", vSpitPos);
	vTarget = pEnemy->GetAbsOrigin();	

#if 1
	Vector vecToss;
	CBaseEntity* pBlocker;
	float flGravity = 1050;
	ThrowLimit(vSpitPos, vTarget, flGravity, 1.2, Vector(0, 0, 0), Vector(0, 0, 0), pEnemy, &vecToss, &pBlocker);

	CSharedProjectile *pGrenade = (CSharedProjectile*)CreateEntityByName("shared_projectile");
	pGrenade->SetAbsOrigin(vSpitPos);
	pGrenade->SetAbsAngles(vec3_angle);
	DispatchSpawn(pGrenade);
	pGrenade->SetThrower(this);
	pGrenade->SetOwnerEntity(this);

	int prtype = Projectile_LIGHTNING;
	int prbeh = Projectile_CLUSTER;
	float prrad = npc_glider_projectile_radius.GetFloat();

	pGrenade->SetProjectileStats(Projectile_LARGE, prtype, prbeh, 
		10.0f, prrad, flGravity);
	pGrenade->SetAbsVelocity(vecToss);
	
	pGrenade->CreateProjectileEffects();
	pGrenade->SetModelScale(1.0);

	pGrenade->SetCollisionGroup(HL2COLLISION_GROUP_GUNSHIP);

	// Tumble through the air
	pGrenade->SetLocalAngularVelocity(
		QAngle(random->RandomFloat(-250, -500),
			random->RandomFloat(-250, -500),
			random->RandomFloat(-250, -500)));
#else
	Vector vecDir;
	GetVectors(&vecDir, NULL, NULL);

	VectorNormalize(vecDir);
	vecDir.x += RandomFloat(-0.1, 0.1);
	vecDir.y += RandomFloat(-0.1, 0.1);

	Vector vecVelocity;
	float randomVel = RandomFloat(-25, 25);
	VectorMultiply(vecDir, 450 + randomVel, vecVelocity);

	CGrenadeHomer *pGrenade = CGrenadeHomer::CreateGrenadeHomer(MAKE_STRING("models/_Monsters/Xen/squid_spitball_large.mdl"), NULL_STRING, vSpitPos, vec3_angle, edict());
	pGrenade->Spawn();
	//	pGrenade->SetSpin(npc_crabsynth_salvo_spin.GetFloat(), 
	//		npc_crabsynth_salvo_spin.GetFloat());
	pGrenade->SetHoming((0.01 * 50),
		0.1f, // Delay
		0.25f, // Ramp up
		60.0f, // Duration
		1.0f);// Ramp down
	pGrenade->SetDamage(10);
	pGrenade->SetDamageRadius(50);
	pGrenade->Launch(this,
		GetEnemy(),
		vecVelocity,
		450,
		2,
		HOMER_SMOKE_TRAIL_ALIEN);
#endif
	m_last_attack_time = CURTIME;
	m_flNextAttack = CURTIME + RandomFloat(3.0f, 4.0f);
}


void CNPC_Glider::InputLaunchProjectileAtTarget(inputdata_t &inputdata)
{
	CBaseEntity *pEntity = gEntList.FindEntityByName(NULL, inputdata.value.String(), NULL, inputdata.pActivator, inputdata.pCaller);
	if (!pEntity)
	{
		DevMsg("%s (%s) received LaunchProjectileAtTarget input, but couldn't find target entity '%s'\n", GetClassname(), GetDebugName(), inputdata.value.String());
		return;
	}

	LaunchProjectile(pEntity);

	ClearSchedule("Told to throw grenade via input");
}

void CNPC_Glider::InputForceDodge(inputdata_t &inputdata)
{
	ClearSchedule("forced by input");
	SetSchedule(SCHED_GLIDER_DODGE);
}

int CNPC_Glider::RangeAttack1Conditions(float flDot, float flDist)
{
	if (flDist < InnateRange1MinRange()) return COND_TOO_CLOSE_TO_ATTACK;
	if (flDist > InnateRange1MaxRange()) return COND_TOO_FAR_TO_ATTACK;
	if (flDot < 0.5) return COND_NOT_FACING_ATTACK;
	if (m_flNextAttack > CURTIME) return COND_NONE;
	return COND_CAN_RANGE_ATTACK1;
}

int CNPC_Glider::SelectSchedule(void)
{
//	if ((HasCondition(COND_REPEATED_DAMAGE)
//		|| HasCondition(COND_HEAVY_DAMAGE))
//		&& CURTIME > m_flNextDodgeTime)
//		return SCHED_GLIDER_DODGE;
	if( m_flNextDodgeTime < CURTIME)
		return SCHED_GLIDER_DODGE;
	
	if (m_NPCState == NPC_STATE_IDLE || m_NPCState == NPC_STATE_ALERT)
	{
		return SCHED_IDLE_STAND;
	}

	if (m_NPCState == NPC_STATE_COMBAT)
	{
		if (m_flNextAttack > CURTIME)
		{
			if (HasCondition(COND_TOO_FAR_TO_ATTACK))
				return SCHED_CHASE_ENEMY;

			else if (HasCondition(COND_TOO_CLOSE_TO_ATTACK))
				return SCHED_COMBAT_FACE;
		}

		if (HasCondition(COND_CAN_RANGE_ATTACK1))
			return	SCHED_RANGE_ATTACK1;
		else
		{
			if( HasCondition(COND_TOO_FAR_TO_ATTACK))
				return SCHED_CHASE_ENEMY;

			else if (HasCondition(COND_TOO_CLOSE_TO_ATTACK))
				return SCHED_COMBAT_FACE;
		}
	}

	return BaseClass::SelectSchedule();
}

int CNPC_Glider::SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode)
{	
	if (failedSchedule == SCHED_BACK_AWAY_FROM_ENEMY)
		return SCHED_RUN_RANDOM;

	if (failedSchedule == SCHED_CHASE_ENEMY)
		return SCHED_COMBAT_FACE;

	return BaseClass::SelectFailSchedule(failedSchedule, failedTask, taskFailCode);
}

int CNPC_Glider::TranslateSchedule(int scheduleType)
{
	if (scheduleType == SCHED_RANGE_ATTACK1) return SCHED_GLIDER_RANGE_ATTACK1;

	return BaseClass::TranslateSchedule(scheduleType);
}

Activity CNPC_Glider::NPC_TranslateActivity(Activity eNewActivity)
{
	if(eNewActivity == ACT_FLY) return ACT_RUN;

	return BaseClass::NPC_TranslateActivity(eNewActivity);
}

#ifdef GLIDER_MAESTRAS_MSYNTH_CODE
ConVar npc_glider_mingrounddist("npc_glider_mingrounddist", "96");
float CNPC_Glider::MinGroundDist(void)
{
	return npc_glider_mingrounddist.GetFloat();
}

void CNPC_Glider::MoveExecute_Alive(float flInterval)
{
	// Amount of noise to add to flying
	float noiseScale = 3.0f;

	SetCurrentVelocity(GetCurrentVelocity() + VelocityToAvoidObstacles(flInterval));

	// Add time-coherent noise to the current velocity so that it never looks bolted in place.
	AddNoiseToVelocity(noiseScale);

	float maxSpeed = GetEnemy() ? (GetMaxSpeed() * 2.0f) : GetMaxSpeed();

	// Limit fall speed
	LimitSpeed(maxSpeed);

	QAngle angles = GetLocalAngles();

	// Make frame rate independent
	float	iRate = 0.5;
	float timeToUse = flInterval;

	while (timeToUse > 0)
	{
		// Get the relationship between my current velocity and the way I want to be going.
		Vector vecCurrentDir = GetCurrentVelocity();
		VectorNormalize(vecCurrentDir);

		// calc relative banking targets
		Vector forward, right;
		GetVectors(&forward, &right, NULL);

		Vector m_vTargetBanking;

		m_vTargetBanking.x = 30 * DotProduct(forward, vecCurrentDir);
		m_vTargetBanking.z = 15 * DotProduct(right, vecCurrentDir);

		m_vCurrentBanking.x = (iRate * m_vCurrentBanking.x) + (1 - iRate)*(m_vTargetBanking.x);
		m_vCurrentBanking.z = (iRate * m_vCurrentBanking.z) + (1 - iRate)*(m_vTargetBanking.z);
		timeToUse = -0.1;
	}
	angles.x = m_vCurrentBanking.x;
	angles.z = m_vCurrentBanking.z;
	angles.y = 0;

	SetLocalAngles(angles);

	PlayFlySound();

	WalkMove(GetCurrentVelocity() * flInterval, MASK_NPCSOLID);
}

bool CNPC_Glider::OverridePathMove(CBaseEntity *pMoveTarget, float flInterval)
{
	// Save our last patrolling direction
	Vector lastPatrolDir = GetNavigator()->GetCurWaypointPos() - GetAbsOrigin();

	// Continue on our path
	if (ProgressFlyPath(flInterval, pMoveTarget, (MASK_NPCSOLID | CONTENTS_WATER), !IsCurSchedule(SCHED_SCANNER_PATROL)) == AINPP_COMPLETE)
	{
		if (IsCurSchedule(SCHED_SCANNER_PATROL))
		{
			m_vLastPatrolDir = lastPatrolDir;
			VectorNormalize(m_vLastPatrolDir);
		}

		return true;
	}

	return false;
}

bool CNPC_Glider::OverrideMove(float flInterval)
{
	Vector vMoveTargetPos(0, 0, 0);
	CBaseEntity *pMoveTarget = NULL;

	if (!GetNavigator()->IsGoalActive() || (GetNavigator()->GetCurWaypointFlags() & bits_WP_TO_PATHCORNER))
	{
		// Select move target 
		if (GetTarget() != NULL)
		{
			pMoveTarget = GetTarget();
			// Select move target position
			vMoveTargetPos = GetTarget()->GetAbsOrigin();
		}
		else if (GetEnemy() != NULL)
		{
			pMoveTarget = GetEnemy();
			// Select move target position
			vMoveTargetPos = GetEnemy()->GetAbsOrigin();
		}
	}
	else
		vMoveTargetPos = GetNavigator()->GetCurWaypointPos();

	ClearCondition(COND_SCANNER_FLY_CLEAR);
	ClearCondition(COND_SCANNER_FLY_BLOCKED);

	// See if we can fly there directly
	if (pMoveTarget)
	{
		trace_t tr;
		AI_TraceHull(GetAbsOrigin(), vMoveTargetPos, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

		if (tr.fraction != 1.0)
			SetCondition(COND_SCANNER_FLY_CLEAR);
		else
			SetCondition(COND_SCANNER_FLY_BLOCKED);
	}

	// If I have a route, keep it updated and move toward target
	if (GetNavigator()->IsGoalActive())
	{
		// -------------------------------------------------------------
		// If I'm on a path check LOS to my next node, and fail on path
		// if I don't have LOS.  Note this is the only place I do this,
		// so the manhack has to collide before failing on a path
		// -------------------------------------------------------------
		if (GetNavigator()->IsGoalActive() && !(GetNavigator()->GetCurWaypointFlags() & bits_WP_TO_PATHCORNER))
		{
			AIMoveTrace_t moveTrace;
			GetMoveProbe()->MoveLimit(NAV_FLY, GetAbsOrigin(), GetNavigator()->GetCurWaypointPos(), MASK_NPCSOLID, GetEnemy(), &moveTrace);

			if (IsMoveBlocked(moveTrace))
			{
				TaskFail(FAIL_NO_ROUTE);
				GetNavigator()->ClearGoal();
				return true;
			}
		}

		if (OverridePathMove(pMoveTarget, flInterval))
			return true;
	}
	// ----------------------------------------------
	//	If attacking
	// ----------------------------------------------
	else if (pMoveTarget)
	{
		MoveToAttack(flInterval);
	}
	// -----------------------------------------------------------------
	// If I don't have a route, just decelerate
	// -----------------------------------------------------------------
	else if (!GetNavigator()->IsGoalActive())
	{
		float myDecay = 9.5;
		Decelerate(flInterval, myDecay);

		// -------------------------------------
		// If I have an enemy, turn to face him
		// -------------------------------------
		if (GetEnemy())
			TurnHeadToTarget(flInterval, GetEnemy()->EyePosition());
	}

	MoveExecute_Alive(flInterval);

	return true;
}

void CNPC_Glider::MoveToTarget(float flInterval, const Vector& vecMoveTarget)
{
	if (flInterval <= 0)
		return;

	// Look at our inspection target if we have one
	if (GetEnemy() != NULL)
		// Otherwise at our enemy
		TurnHeadToTarget(flInterval, GetEnemy()->EyePosition());
	else
		// Otherwise face our motion direction
		TurnHeadToTarget(flInterval, vecMoveTarget);

	// -------------------------------------
	// Move towards our target
	// -------------------------------------
	float myAccel;
	float myZAccel = 300.0f;
	float myDecay = 0.15f;

	Vector vecCurrentDir;

	// Get the relationship between my current velocity and the way I want to be going.
	vecCurrentDir = GetCurrentVelocity();
	VectorNormalize(vecCurrentDir);

	Vector targetDir = vecMoveTarget - GetAbsOrigin();
	float flDist = VectorNormalize(targetDir);

	float flDot;
	flDot = DotProduct(targetDir, vecCurrentDir);

	if (flDot > 0.25)
		// If my target is in front of me, my flight model is a bit more accurate.
		myAccel = 250;
	else
		// Have a harder time correcting my course if I'm currently flying away from my target.
		myAccel = 128;

	// Clamp lateral acceleration
	if (myAccel > flDist / flInterval)
		myAccel = flDist / flInterval;

	// Clamp vertical movement
	if (myZAccel > flDist / flInterval)
		myZAccel = flDist / flInterval;

	MoveInDirection(flInterval, targetDir, myAccel, myZAccel, myDecay);
}

void CNPC_Glider::MoveToAttack(float flInterval)
{
	if (GetEnemy() == NULL)
		return;

	if (flInterval <= 0)
		return;

	TurnHeadToTarget(flInterval, GetEnemy()->EyePosition());

	// -------------------------------------
	// Move towards our target
	// -------------------------------------
	float myAccel;
	float myZAccel = 400.0f;
	float myDecay = 0.15f;

	Vector vecCurrentDir;

	// Get the relationship between my current velocity and the way I want to be going.
	vecCurrentDir = GetCurrentVelocity();
	VectorNormalize(vecCurrentDir);

	Vector targetDir = GetEnemy()->EyePosition() - GetAbsOrigin();
	float flDist = VectorNormalize(targetDir);

	float flDot;
	flDot = DotProduct(targetDir, vecCurrentDir);

	if (flDot > 0.25)
		// If my target is in front of me, my flight model is a bit more accurate.
		myAccel = 250;
	else
		// Have a harder time correcting my course if I'm currently flying away from my target.
		myAccel = 128;

	// Clamp lateral acceleration
	if (myAccel > flDist / flInterval)
		myAccel = flDist / flInterval;

	// Clamp vertical movement
	if (myZAccel > flDist / flInterval)
		myZAccel = flDist / flInterval;

	Vector idealPos;

	if (flDist < InnateRange1MinRange())
		idealPos = -targetDir;
	else if (flDist > InnateRange1MaxRange())
		idealPos = targetDir;

	MoveInDirection(flInterval, idealPos, myAccel, myZAccel, myDecay);
}

void CNPC_Glider::TurnHeadToTarget(float flInterval, const Vector &MoveTarget)
{
	float desYaw = UTIL_AngleDiff(VecToYaw(MoveTarget - GetLocalOrigin()), GetLocalAngles().y);

	// If I've flipped completely around, reverse angles
	float fYawDiff = m_fHeadYaw - desYaw;
	if (fYawDiff > 180)
		m_fHeadYaw -= 360;
	else if (fYawDiff < -180)
		m_fHeadYaw += 360;

	float iRate = 0.8;

	// Make frame rate independent
	float timeToUse = flInterval;
	while (timeToUse > 0)
	{
		m_fHeadYaw = (iRate * m_fHeadYaw) + (1 - iRate) * desYaw;
		timeToUse -= 0.1;
	}

	while (m_fHeadYaw > 360)
		m_fHeadYaw -= 360.0f;

	while (m_fHeadYaw < 0)
		m_fHeadYaw += 360.f;

	SetBoneController(0, m_fHeadYaw);
}

void CNPC_Glider::PlayFlySound(void)
{
/*
	if (IsMarkedForDeletion())
		return;

	if (CURTIME > m_fNextFlySoundTime)
	{
		EmitSound("NPC_Mortarsynth.Hover");

		// If we are hurting, play some scary hover sounds :O
		if (GetHealth() < (GetMaxHealth() / 3))
			EmitSound("NPC_Mortarsynth.HoverAlarm");

		m_fNextFlySoundTime = CURTIME + 1.0;
	}
*/
}
#else
bool CNPC_Glider::OverrideMove(float flInterval)
{
//	m_flGroundSpeed = GetGroundSpeed();

	if (m_bHasMoveTarget)
	{
		DoMovement(flInterval, m_vecLastMoveTarget);
	}
	else
	{
		DoMovement(flInterval, GetLocalOrigin());
	}

	return false;
}

void CNPC_Glider::DoMovement(float flInterval, const Vector &MoveTarget)
{
	Vector Steer, SteerAvoid, SteerRel;
	Vector forward, right, up;

	//Get our orientation vectors.
	GetVectors(&forward, &right, &up);

	SteerArrive(Steer, MoveTarget);

	//See if we need to avoid any obstacles.
	if (SteerAvoidObstacles(SteerAvoid, GetAbsVelocity(), forward, right, up))
	{
		//Take the avoidance vector
		Steer = SteerAvoid;
	}

	//Clamp our ideal steering vector to within our physical limitations.
	ClampSteer(Steer, SteerRel, forward, right, up);

	ApplyAbsVelocityImpulse(Steer * flInterval);

	Vector vecNewVelocity = GetAbsVelocity(); // FIXME: this is 0 for npcs!
	float flLength = vecNewVelocity.Length();

	//Clamp our final speed
	if (flLength > m_flGroundSpeed)
	{
		vecNewVelocity *= (m_flGroundSpeed / flLength);
		flLength = m_flGroundSpeed;
	}

	Vector	workVelocity = vecNewVelocity;

	AddHoverNoise(&workVelocity);
	
	//Move along the current velocity vector
	if (WalkMove(workVelocity * flInterval, MASK_NPCSOLID) == false)
	{
		//Attempt a half-step
		if (WalkMove((workVelocity*0.5f) * flInterval, MASK_NPCSOLID) == false)
		{
			//Restart the velocity
			//VectorNormalize( m_vecVelocity );
			vecNewVelocity *= 0.5f;
		}
		else
		{
			//Cut our velocity in half
			vecNewVelocity *= 0.5f;
		}
	}
	
	SetAbsVelocity(vecNewVelocity);


	// set up banking
	/*
	QAngle angles = GetLocalAngles();
	float	iRate = 0.5;
	float timeToUse = flInterval;

	while (timeToUse > 0)
	{
		// Get the relationship between my current velocity and the way I want to be going.
		Vector vecCurrentDir = GetGroundSpeedVelocity();
		VectorNormalize(vecCurrentDir);

		// calc relative banking targets
		Vector forward, right;
		GetVectors(&forward, &right, NULL);

		Vector m_vTargetBanking;

		m_vTargetBanking.x = 40 * DotProduct(forward, vecCurrentDir);
		m_vTargetBanking.z = 40 * DotProduct(right, vecCurrentDir);

		m_vCurrentBanking.x = (iRate * m_vCurrentBanking.x) + (1 - iRate)*(m_vTargetBanking.x);
		m_vCurrentBanking.z = (iRate * m_vCurrentBanking.z) + (1 - iRate)*(m_vTargetBanking.z);
		timeToUse = -0.1;
	}
	angles.x = m_vCurrentBanking.x;
	angles.z = m_vCurrentBanking.z;

	SetLocalAngles(angles);
	*/
	// create a danger sound beneath us
	trace_t tr;
	Vector vecBottom = GetAbsOrigin();

	// Uneven terrain causes us problems, so trace our box down
	AI_TraceEntity(this, vecBottom, vecBottom - Vector(0, 0, 4096), MASK_SOLID_BRUSHONLY, &tr);

	float flAltitude = (4096 * tr.fraction);

	vecBottom.z += WorldAlignMins().z;

	Vector vecSpot = vecBottom + Vector(0, 0, -1) * (flAltitude - 12);
	CSoundEnt::InsertSound(SOUND_DANGER, vecSpot, 200, 0.1, this, 0);
//	CSoundEnt::InsertSound(SOUND_PHYSICS_DANGER, vecSpot, 200, 0.1, this, 1);
//	NDebugOverlay::Cross3D( vecSpot, -Vector(4,4,4), Vector(4,4,4), 255, 0, 255, false, 10.0f );
//	CEffectData	data;

//	data.m_nEntIndex = entindex();
//	data.m_vOrigin = vecSpot;
//	data.m_flScale = RandomFloat(80,90) * m_flPlaybackRate;
//	DispatchEffect("ThumperDust", data);

	if (developer.GetInt() == 2)
	{
		NDebugOverlay::Box(GetNavigator()->GetGoalPos(), -Vector(2, 2, 2), Vector(2, 2, 2), 50, 255, 150, 200, 0.01f);
		NDebugOverlay::Line(GetAbsOrigin(), GetNavigator()->GetGoalPos(), 50, 150, 255, true, 0.01f);
	}
}

void CNPC_Glider::SteerArrive(Vector &Steer, const Vector &Target)
{
	Vector goalPos = Target;
	// Trace down and make sure we're not flying too low (96 = min ground dist).
	trace_t	tr;
	AI_TraceHull(goalPos, goalPos - Vector(0, 0, 96), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

	// Move up otherwise
	if (tr.fraction < 1.0f)
	{
		goalPos.z += (96 * (1.0f - tr.fraction));
	}

	// Do an additional trace for max flying height, too.
	AI_TraceHull(goalPos, goalPos - Vector(0, 0, 512), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

	// Move up otherwise
	if (tr.fraction == 1.0f)
	{
		goalPos.z -= 256;
	}

	Vector Offset = goalPos - GetAbsOrigin();
	float fTargetDistance = Offset.Length();

	float fIdealSpeed = m_flGroundSpeed * (fTargetDistance / 64.0f);
	float fClippedSpeed = MIN(fIdealSpeed, m_flGroundSpeed);

	Vector DesiredVelocity(0, 0, 0);

	if (fTargetDistance > 128.0f)
	{
		DesiredVelocity = (fClippedSpeed / fTargetDistance) * Offset;
	}
	
	Steer = DesiredVelocity - GetAbsVelocity();
}

void CNPC_Glider::SteerSeek(Vector &Steer, const Vector &Target)
{
	Vector offset = Target - GetLocalOrigin();

	VectorNormalize(offset);

	Vector DesiredVelocity = m_flGroundSpeed * offset;

	Steer = DesiredVelocity - GetAbsVelocity();
}

bool CNPC_Glider::SteerAvoidObstacles(Vector &Steer, const Vector &Velocity, const Vector &Forward, const Vector &Right, const Vector &Up)
{
	trace_t	tr;

	bool	collided = false;
	Vector	dir = Velocity;
	float	speed = VectorNormalize(dir);

	//Look ahead one second and avoid whatever is in our way.
	AI_TraceHull(GetAbsOrigin(), GetAbsOrigin() + (dir*speed), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

	Vector	forward;

	GetVectors(&forward, NULL, NULL);

	//If we're hitting our enemy, just continue on
	if ((GetEnemy() != NULL) && (tr.m_pEnt == GetEnemy()))
		return false;

	if (tr.fraction < 1.0f)
	{
		CBaseEntity *pBlocker = tr.m_pEnt;

		if ((pBlocker != NULL) && (pBlocker->MyNPCPointer() != NULL))
		{
			Vector HitOffset = tr.endpos - GetAbsOrigin();

			Vector SteerUp = CrossProduct(HitOffset, Velocity);
			Steer = CrossProduct(SteerUp, Velocity);
			VectorNormalize(Steer);

			if (tr.fraction > 0)
			{
				Steer = (Steer * Velocity.Length()) / tr.fraction;
			}
			else
			{
				Steer = (Steer * 1000 * Velocity.Length());
			}
		}
		else
		{
			if ((pBlocker != NULL) && (pBlocker == GetEnemy()))
			{
				return false;
			}
			
			Vector	steeringVector = tr.plane.normal;

			if (tr.fraction == 0.0f)
				return false;

			Steer = steeringVector * (Velocity.Length() / tr.fraction);
		}

		//return true;
		collided = true;
	}

	return collided;
}

void CNPC_Glider::ClampSteer(Vector &SteerAbs, Vector &SteerRel, Vector &forward, Vector &right, Vector &up)
{
	float fForwardSteer = DotProduct(SteerAbs, forward);
	float fRightSteer = DotProduct(SteerAbs, right);
	float fUpSteer = DotProduct(SteerAbs, up);

	if (fForwardSteer > 0)
	{
		fForwardSteer = MIN(fForwardSteer, m_vecAccelerationMax.x);
	}
	else
	{
		fForwardSteer = MAX(fForwardSteer, m_vecAccelerationMin.x);
	}

	if (fRightSteer > 0)
	{
		fRightSteer = MIN(fRightSteer, m_vecAccelerationMax.y);
	}
	else
	{
		fRightSteer = MAX(fRightSteer, m_vecAccelerationMin.y);
	}

	if (fUpSteer > 0)
	{
		fUpSteer = MIN(fUpSteer, m_vecAccelerationMax.z);
	}
	else
	{
		fUpSteer = MAX(fUpSteer, m_vecAccelerationMin.z);
	}

	SteerAbs = (fForwardSteer*forward) + (fRightSteer*right) + (fUpSteer*up);

	SteerRel.x = fForwardSteer;
	SteerRel.y = fRightSteer;
	SteerRel.z = fUpSteer;
}

void CNPC_Glider::MoveFlyExecute(CBaseEntity *pTargetEnt, const Vector &vecDir, float flDistance, float flInterval)
{
//	m_flGroundSpeed = GetGroundSpeed();

	Vector	moveGoal = GetNavigator()->GetCurWaypointPos();

	//See if we can move directly to our goal
	if ((GetEnemy() != NULL) && (GetNavigator()->GetGoalTarget() == (CBaseEntity *)GetEnemy()))
	{
		trace_t	tr;
		Vector	goalPos = GetEnemy()->GetAbsOrigin() + (GetEnemy()->GetSmoothedVelocity() * 0.5f);

		AI_TraceHull(GetAbsOrigin(), goalPos, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, GetEnemy(), COLLISION_GROUP_NONE, &tr);

		if (tr.fraction == 1.0f)
		{
			moveGoal = tr.endpos;
		}
	}

	//Move
	DoMovement(flInterval, moveGoal);

	//Save the info from that run
	m_vecLastMoveTarget = moveGoal;
	m_bHasMoveTarget = true;
}

void CNPC_Glider::AddHoverNoise(Vector *velocity)
{
	Vector	right, up;

	GetVectors(NULL, &right, &up);

	float	lNoise, vNoise;

	float maxLateralNoise = 4;
	float maxVerticalNoise = 6;

	if (IsMoving())
	{
		maxLateralNoise /= 2;
		maxVerticalNoise /= 2;
	}

	lNoise = maxLateralNoise * sin(CURTIME * 0.6f);
	vNoise = maxVerticalNoise * sin(CURTIME * 1.3f);

	(*velocity) += (right * lNoise) + (up * vNoise);
}

float CNPC_Glider::GetDefaultNavGoalTolerance()
{
	return GetHullWidth()*2.0f;
}

bool CNPC_Glider::OverrideMoveFacing(const AILocalMoveGoal_t &move, float flInterval)
{
	if (GetEnemy() && (HasCondition(COND_CAN_RANGE_ATTACK1) || CURTIME - m_last_attack_time <= 1 ))
	{
		AddFacingTarget(GetEnemy(), GetEnemy()->WorldSpaceCenter(), 1.0f, 0.2f);
		return BaseClass::OverrideMoveFacing(move, flInterval);
	}

#if 0
	if (GetEnemy() && GetNavigator()->GetMovementActivity() == ACT_RUN)
	{
		// FIXME: this will break scripted sequences that walk when they have an enemy
		Vector vecEnemyLKP = GetEnemyLKP();
		if (UTIL_DistApprox(vecEnemyLKP, GetAbsOrigin()) < 2048)
		{
			// Only start facing when we're close enough
			AddFacingTarget(GetEnemy(), vecEnemyLKP, 1.0, 0.2);
		}
	}
#endif
	return BaseClass::OverrideMoveFacing(move, flInterval);
}

/*
void CNPC_Glider::TurnHeadToTarget(float flInterval, const Vector &MoveTarget)
{
	float desYaw = UTIL_AngleDiff(VecToYaw(MoveTarget - GetLocalOrigin()), GetLocalAngles().y);

	// If I've flipped completely around, reverse angles
	float fYawDiff = m_poseMove_Yaw - desYaw;
	if (fYawDiff > 180)
		m_poseMove_Yaw -= 360;
	else if (fYawDiff < -180)
		m_poseMove_Yaw += 360;

	float iRate = 0.8;

	// Make frame rate independent
	float timeToUse = flInterval;
	while (timeToUse > 0)
	{
		m_poseMove_Yaw = (iRate * m_poseMove_Yaw) + (1 - iRate) * desYaw;
		timeToUse -= 0.1;
	}

	while (m_poseMove_Yaw > 360)
		m_poseMove_Yaw -= 360.0f;

	while (m_poseMove_Yaw < 0)
		m_poseMove_Yaw += 360.f;

	SetBoneController(0, m_poseMove_Yaw);
}
*/
#endif
BEGIN_DATADESC(CNPC_Glider)
DEFINE_FIELD(m_bHasMoveTarget, FIELD_BOOLEAN),
DEFINE_FIELD(m_bIgnoreSurface, FIELD_BOOLEAN),
DEFINE_FIELD(m_vecSaveProjectileVelocity, FIELD_VECTOR),
DEFINE_FIELD(m_vCurrentBanking, FIELD_VECTOR),
DEFINE_FIELD(m_dodge_activity, FIELD_INTEGER),
DEFINE_INPUTFUNC(FIELD_STRING, "LaunchProjectileAtTarget", InputLaunchProjectileAtTarget),
DEFINE_INPUTFUNC(FIELD_VOID, "ForceDodge", InputForceDodge),
DEFINE_SOUNDPATCH(m_pEngineSound),
END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_glider, CNPC_Glider);
LINK_ENTITY_TO_CLASS(npc_mortarsynth, CNPC_Glider);
LINK_ENTITY_TO_CLASS(npc_mortar_synth, CNPC_Glider);

AI_BEGIN_CUSTOM_NPC(npc_glider, CNPC_Glider)
DECLARE_ANIMEVENT(AE_LAUNCH_PROJECTILE)
DECLARE_TASK(TASK_GLIDER_FIND_DODGE_POSITION)
DECLARE_TASK(TASK_GLIDER_DODGE)
DEFINE_SCHEDULE
(
	SCHED_GLIDER_RANGE_ATTACK1,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
	"		TASK_RANGE_ATTACK1		0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_NO_PRIMARY_AMMO"
	"		COND_HEAVY_DAMAGE"
	"		COND_REPEATED_DAMAGE"
);

//=========================================================
// Sidestep in the air when damaged
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_GLIDER_DODGE,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_GLIDER_FAIL_DODGE"
	"		TASK_GLIDER_FIND_DODGE_POSITION			0"
	"		TASK_GLIDER_DODGE						0"
	""
	"	Interrupts"
)

//=========================================================
// Sidestep in the air when damaged
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_GLIDER_FAIL_DODGE,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
	"		TASK_FACE_ENEMY			0"
	""
	"	Interrupts"
)

AI_END_CUSTOM_NPC()

#endif // GLIDERSYNTH

//====================================
// Synth Scanner (Dark Interval)
//====================================
#if defined (SYNTHSCANNER)

//#include "npc_basescanner.h"
class CSynthScannerGasCloud;
class CNPC_SynthScanner : public CNPC_BaseScanner
{
	DECLARE_CLASS(CNPC_SynthScanner, CNPC_BaseScanner);
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
	DEFINE_CUSTOM_AI;
public:
	CNPC_SynthScanner();
	~CNPC_SynthScanner();
	void			Spawn(void);
	void			Activate();
	void			Precache(void);
	Class_T			Classify(void) { return CLASS_COMBINE; }
	Disposition_t	IRelationType(CBaseEntity *pTarget);
	bool			IsShieldActive(void) { return m_forcefieldshield_is_active_bool && m_shield_ehandle != NULL; }
	bool			IsWastelandScanner(void);

	int				DrawDebugTextOverlays(void);
	float			GetMaxSpeed(void);
	int				GetSoundInterests(void) { return (SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_DANGER); }
	char			*GetEngineSound(void) { return "NPC_SScanner.FlyLoop"; }
	bool			FValidateHintType(CAI_Hint *pHint)
	{
		switch (pHint->HintType())
		{
		case HINT_TACTICAL_SPAWN:
			return true;
			break;
		default:
			return false;
			break;
		}
	}
	float			MinGroundDist(void);

	void			NPCThink(void);
	
	void			TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr);
	int				OnTakeDamage_Alive(const CTakeDamageInfo &info);
	void			Event_Killed(const CTakeDamageInfo &info);
	void			Gib(void);
	void			UpdateOnRemove(void);
	bool			CanGibRagdoll(void) { return true; }

	void			BlindFlashTarget(CBaseEntity *pTarget);
	void			HandleAnimEvent(animevent_t *pEvent) {};
	Activity		NPC_TranslateActivity(Activity eNewActivity);

	int				MeleeAttack1Conditions(float flDot, float flDist);
	void			GatherConditions(void);
	void			PrescheduleThink(void);
	int				SelectSchedule(void);
	int				TranslateSchedule(int scheduleType);
	void			StartTask(const Task_t *pTask);
	void			RunTask(const Task_t *pTask);
	void			SpeakSentence(int nSentenceType);

	// ------------------------------
	//	Inspecting
	// ------------------------------
	CSprite*		m_pEyeFlash;
	Vector			m_vInspectPos;
	float			m_fInspectEndTime;

	void			SetInspectTargetToEnt(CBaseEntity *pEntity, float fInspectDuration);
	void			SetInspectTargetToPos(const Vector &vInspectPos, float fInspectDuration);
	void			SetInspectTargetToHint(CAI_Hint *pHint, float fInspectDuration);
	void			ClearInspectTarget(void);
	bool			HaveInspectTarget(void);
	Vector			InspectTargetPosition(void);
	bool			IsValidInspectTarget(CBaseEntity *pEntity);
	CBaseEntity*	BestInspectTarget(void);
	bool			m_bShouldInspect;
	float			m_fCheckHintTime;		// Time to look for hints to inspect
	float			m_fCheckNPCTime;
	
	void			InspectTarget(inputdata_t &inputdata, ScannerFlyMode_t eFlyMode);
	void			InputDisableSpotlight(inputdata_t &inputdata);
	void			InputSetFollowTarget(inputdata_t &inputdata);
	void			InputClearFollowTarget(inputdata_t &inputdata);
	void			InputInspectTargetSpotlight(inputdata_t &inputdata);
	void			InputShouldInspect(inputdata_t &inputdata);
	void			InputEnterAmbush(inputdata_t &inputdata);
	void			InputExitAmbush(inputdata_t &inputdata);

public:
	// ------------------------------
	//	Spotlight
	// ------------------------------
	Vector			m_vSpotlightTargetPos;
	Vector			m_vSpotlightCurrentPos;
	CHandle<CBeam>	m_hSpotlight;
	CHandle<CSpotlightEnd> m_hSpotlightTarget;
	Vector			m_vSpotlightDir;
	Vector			m_vSpotlightAngVelocity;
	float			m_flSpotlightCurLength;
	float			m_flSpotlightMaxLength;
	float			m_flSpotlightGoalWidth;
	float			m_fNextSpotlightTime;
	int				m_nHaloSprite;

	void			SpotlightUpdate(void);
	Vector			SpotlightTargetPos(void);
	Vector			SpotlightCurrentPos(void);
	void			SpotlightCreate(void);
	void			SpotlightDestroy(void);

	void			PlayFlySound(void);

private:
	bool			MovingToInspectTarget(void);
	float			GetGoalDistance(void);

	bool			OverrideMove(float flInterval);
	void			MoveToTarget(float flInterval, const Vector &MoveTarget);
	void			MoveToSpotlight(float flInterval);
	void			ClampMotorForces(Vector &linear, AngularImpulse &angular);

	void			AttackPreFlash(void); // FIXME: this should be PreGas, really
	void			AttackGas(void);
	void			AttackPreShock(void);
	void			AttackShock(void);
	void			AttackFlashBlind(void);

	void			CreateForceFieldShield(float radius);
	void			CollapseForceFieldShield(bool removeImmediately);
	void			InputCreateForceFieldShield(inputdata_t &data);
	void			InputCollapseForceFieldShield(inputdata_t &data);

	CAI_Sentence< CNPC_SynthScanner > m_Sentences;
	bool			m_bNoLight;
#ifdef DARKINTERVAL
	bool			m_bNoDLight;
#endif
	// Protective barrier
	bool			m_forcefieldshield_is_active_bool;
	float			m_forcefieldshield_creation_time_float;
	float			m_forcefieldshield_cooldown_time_float;
	int				m_forcefieldshield_integrity_int; // MICROTEST ARENA CHANGE - make scanner shield depletable by firing
	CHandle<CPropScalable>	m_shield_ehandle;

	CHandle<CToxicCloud> m_hGasCloud;

	// Custom interrupt conditions	
	enum
	{
		COND_SYSCANNER_HAVE_INSPECT_TARGET = BaseClass::NEXT_CONDITION,
		COND_SYSCANNER_INSPECT_DONE,
		COND_SYSCANNER_SPOT_ON_TARGET,
		COND_SYSCANNER_TOO_WEAK_TO_ATTACK,
		NEXT_CONDITION,
	};

	// Custom schedules
	enum
	{
		SCHED_SYSCANNER_SPOTLIGHT_HOVER = BaseClass::NEXT_SCHEDULE,
		SCHED_SYSCANNER_SPOTLIGHT_INSPECT_POS,
		//	SCHED_SYSCANNER_SPOTLIGHT_INSPECT_CIT,
		//	SCHED_SYSCANNER_PHOTOGRAPH_HOVER,
		//	SCHED_SYSCANNER_PHOTOGRAPH,
		SCHED_SYSCANNER_ATTACK_GAS,
		SCHED_SYSCANNER_ATTACK_SHOCK,
		SCHED_SYSCANNER_MOVE_TO_INSPECT,
		SCHED_SYSCANNER_PATROL,
		SCHED_SYSCANNER_MOVE_TO_ATTACK,
		SCHED_SYSCANNER_MOVE_ESCAPE,
		SCHED_SYSCANNER_ATTACK_HOVER,

		NEXT_SCHEDULE,
	};

	// Custom tasks
	enum
	{
		TASK_SYSCANNER_SET_FLY_SPOT = BaseClass::NEXT_TASK,
		TASK_SYSCANNER_ATTACK_PRE_FLASH,
		TASK_SYSCANNER_ATTACK_GAS,
		TASK_SYSCANNER_ATTACK_PRE_SHOCK,
		TASK_SYSCANNER_ATTACK_SHOCK,
		TASK_SYSCANNER_SPOT_INSPECT_ON,
		TASK_SYSCANNER_SPOT_INSPECT_WAIT,
		TASK_SYSCANNER_SPOT_INSPECT_OFF,
		TASK_SYSCANNER_CLEAR_INSPECT_TARGET,
		TASK_SYSCANNER_GET_PATH_TO_INSPECT_TARGET,
		TASK_SYSCANNER_SET_FLY_FAST,

		NEXT_TASK,
	};

	enum
	{
		SYSCANNER_SENTENCE_IDLE = 100,
		SYSCANNER_SENTENCE_INSPECT = 101,
		SYSCANNER_SENTENCE_ALERT = 102,
	};
};

//-----------------------------------------------------------------------------
// Parameters for how the scanner relates to citizens.
//-----------------------------------------------------------------------------
#define SY_SCANNER_HINT_INSPECT_DELAY		15		// Check for hint nodes this often
#define	SY_SPOTLIGHT_WIDTH					32
#define SY_SCANNER_SPOTLIGHT_NEAR_DIST		64
#define SY_SCANNER_SPOTLIGHT_FAR_DIST		256
#define SY_SCANNER_SPOTLIGHT_FLY_HEIGHT		72
#define SY_SCANNER_NOSPOTLIGHT_FLY_HEIGHT	72
#define SY_SCANNER_FLASH_MIN_DIST			900		// How far does flash effect enemy
#define SY_SCANNER_FLASH_MAX_DIST			1200	// How far does flash effect enemy
#define	SY_SCANNER_FLASH_MAX_VALUE			240		// How bright is maximum flash

#define SF_SYNTHSCANNER_WASTELAND (1<<18)
#define SF_SYNTHSCANNER_CALCULATE_Z_FROM_PLAYER_POS (1<<19)
#define SF_SYNTHSCANNER_AMBUSH (1<<20)

static ConVar npc_synthscanner_clamp_motor_forces_z_factor("npc_synthscanner_clamp_motor_forces_z_factor", "200");
static ConVar npc_synthscanner_altitude_threshold("npc_synthscanner_altitude_threshold", "320.0");
static ConVar npc_synthscanner_z_acceleration_factor("npc_synthscanner_z_acceleration_factor", "500");
static ConVar npc_synthscanner_distancethink("npc_synthscanner_distancethink", "1");
ConVar npc_synthscanner_shield_duration("sk_synthscanner_shield_duration", "10");
ConVar npc_synthscanner_shield_integrity("sk_synthscanner_shield_integrity", "50");
ConVar sk_scanner_shock_dmg_plr("sk_scanner_shock_dmg_plr", "0");
ConVar sk_scanner_shock_dmg_npc("sk_scanner_shock_dmg_npc", "0");
//-----------------------------------------------------------------------------
// Private activities.
//-----------------------------------------------------------------------------
Activity ACT_WSCANNER_FLINCH_BACK;
Activity ACT_WSCANNER_FLINCH_FRONT;
Activity ACT_WSCANNER_FLINCH_LEFT;
Activity ACT_WSCANNER_FLINCH_RIGHT;
Activity ACT_SCANNER_INSPECT;

int AE_SYNTHSCANNER_RANGEATTACK1;

//-----------------------------------------------------------------------------
// Attachment points
//-----------------------------------------------------------------------------
#define SY_SCANNER_ATTACHMENT_LIGHT	"light"
#define SY_SCANNER_ATTACHMENT_BASE		"base"

CNPC_SynthScanner::CNPC_SynthScanner()
{
	m_hNPCFileInfo = LookupNPCInfoScript("npc_synth_scanner");
	m_Sentences.Init(this, "NPC_SynthScanner.SentenceParameters");
	m_shield_ehandle = NULL;
}
CNPC_SynthScanner::~CNPC_SynthScanner()
{
	if (m_shield_ehandle != NULL) UTIL_Remove(m_shield_ehandle);
	m_shield_ehandle = NULL;

	// Remove spotlight
	if (m_hSpotlight)
	{
		SpotlightDestroy();
		m_hSpotlight = NULL;
	}
	// Remove sprite
	if (m_pEyeFlash)
	{
		UTIL_Remove(m_pEyeFlash);
		m_pEyeFlash = NULL;
	}
}
bool CNPC_SynthScanner::IsWastelandScanner(void)
{
	return HasSpawnFlags(SF_SYNTHSCANNER_WASTELAND);
}

void CNPC_SynthScanner::Precache(void)
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	BaseClass::Precache();

	// Models
	PrecacheModel("models/_Monsters/Combine/synth_scanner_gib_top.mdl");
	PrecacheModel("models/_Monsters/Combine/synth_scanner_gib_left.mdl");
	PrecacheModel("models/_Monsters/Combine/synth_scanner_gib_right.mdl");
	PrecacheModel("models/gibs/Shield_Scanner_Gib1.mdl");
	PrecacheModel("models/gibs/Shield_Scanner_Gib6.mdl");
	PrecacheModel("models/_Monsters/Combine/synth_scanner_shield.mdl");
	UTIL_PrecacheOther("prop_scalable");

	// Sounds
	PrecacheScriptSound("NPC_SynthScanner.PreAttack");
	PrecacheScriptSound("NPC_SynthScanner.AttackGas");
	PrecacheScriptSound("NPC_SynthScanner.IdleChatter");
	PrecacheScriptSound("NPC_SynthScanner.InspectChatter");
	PrecacheScriptSound("NPC_SynthScanner.AlertChatter");
	PrecacheScriptSound("NPC_SynthScanner.ShieldImpact");
	PrecacheScriptSound("NPC_SynthScanner.ShieldActivate");
	PrecacheScriptSound("NPC_SynthScanner.ShieldBlowUp");
	PrecacheScriptSound("NPC_SScanner.FlyLoop");

	// Sprites
	m_nHaloSprite = PrecacheModel("sprites/light_glow03.vmt");
	PrecacheModel("sprites/glow_test02.vmt");
	PrecacheModel("sprites/blueflare1.vmt");

	// Particles
	PrecacheParticleSystem("vortigaunt_beam");
	PrecacheParticleSystem("antlion_spit");
	PrecacheParticleSystem("blood_impact_antlion_01");
	PrecacheParticleSystem("blood_impact_synth_01");
	PrecacheParticleSystem("wastelandscanner_shield_impact");

	enginesound->PrecacheSentenceGroup("SYNTHSCANNER");
}

void CNPC_SynthScanner::Spawn(void)
{
	Precache();

	SetModel(STRING(GetModelName()));
	BaseClass::Spawn();
	if (HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH)) RemoveFlag(FL_FLY);
	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MOVETYPE_VPHYSICS);
	SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
	SetHullSizeNormal();
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;

	SetGravity(HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH) ? UTIL_ScaleForGravity(1) : UTIL_ScaleForGravity(0.5));

	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);

//	CapabilitiesClear();
//	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_Climb_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_CLIMB);
	if ((GetNPCScriptData().NPC_Capable_Doors_boolean) == true) CapabilitiesAdd(bits_CAP_DOORS_GROUP);

	CapabilitiesAdd(bits_CAP_LIVE_RAGDOLL);
	//	m_bShouldPatrol = GetNPCScriptData().bShouldPatrolAfterSpawn;

	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));

	//m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);
	m_nSkin = IsWastelandScanner() ? 3 : 1; 

	// ------------------------------------
	//	Init all class vars 
	// ------------------------------------
	m_vInspectPos = vec3_origin;
	m_fInspectEndTime = 0;
	m_fCheckNPCTime = CURTIME + 10.0;
	m_fCheckHintTime = CURTIME + FLT_MAX;

	m_vSpotlightTargetPos = vec3_origin;
	m_vSpotlightCurrentPos = vec3_origin;

	m_hSpotlight = NULL;
	m_hSpotlightTarget = NULL;

	AngleVectors(GetLocalAngles(), &m_vSpotlightDir);
	m_vSpotlightAngVelocity = vec3_origin;

	m_pEyeFlash = 0;
	m_fNextSpotlightTime = 0;
	m_nFlyMode = SCANNER_FLY_PATROL;
	m_vCurrentBanking = m_vSpotlightDir;
	m_flSpotlightCurLength = m_flSpotlightMaxLength;
	m_NPCState = NPC_STATE_IDLE;

	Activate();

	m_forcefieldshield_cooldown_time_float = 0;
	m_forcefieldshield_creation_time_float = 0;
	m_forcefieldshield_integrity_int = 0;
	m_forcefieldshield_is_active_bool = false;

	// Ignore entities that can't retaliate at all.
	// so far just barnacles... maybe the list will grow
	// Also scanners like getting stuck near barnacles,
	// so this prevents that too.
	AddClassRelationship(CLASS_BARNACLE, D_NU, 0);
	AddClassRelationship(CLASS_HEADCRAB, D_NU, 0); // not just headcrabs but also stingers

	if (HasSpawnFlags(SF_SYNTHSCANNER_WASTELAND)) m_nSkin = 3;

	m_OnSpawned.FireOutput(this, this);
}

void CNPC_SynthScanner::Activate()
{
	BaseClass::Activate();
	// Have to do this here because sprites do not go across level transitions
	m_pEyeFlash = CSprite::SpriteCreate("sprites/blueflare1.vmt", GetLocalOrigin(), FALSE);
	m_pEyeFlash->SetTransparency(kRenderGlow, 255, 175, 50, 0, kRenderFxNoDissipation);
	m_pEyeFlash->SetAttachment(this, LookupAttachment(SY_SCANNER_ATTACHMENT_LIGHT));
	m_pEyeFlash->SetBrightness(50);
	m_pEyeFlash->SetScale(1.0);
}

void CNPC_SynthScanner::NPCThink(void)
{
	BaseClass::NPCThink();
	StudioFrameAdvance();
	SpotlightUpdate();

	// See how close the player is
	CBasePlayer *pPlayerEnt = AI_GetSinglePlayer();
	float flDistToPlayerSqr = (GetAbsOrigin() - pPlayerEnt->GetAbsOrigin()).LengthSqr();

	if (npc_synthscanner_distancethink.GetInt())
	{
		float f = Square(2000);
		if (flDistToPlayerSqr > f)	SetNextThink(CURTIME + 0.25f);
		if (flDistToPlayerSqr > f * 2)	SetNextThink(CURTIME + 0.5f);
		if (flDistToPlayerSqr > f * 4)	SetNextThink(CURTIME + 0.75f);
		else SetNextThink(CURTIME + 0.1f);
	}
	else SetNextThink(CURTIME + 0.1f);
}

int CNPC_SynthScanner::DrawDebugTextOverlays(void)
{
	int nOffset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		char tempstr[512];
		Q_snprintf(tempstr, sizeof(tempstr), "Shield Integrity: %i", m_forcefieldshield_integrity_int);
		EntityText(nOffset, tempstr, 0);
		nOffset++;
	}

	return nOffset;
}

Disposition_t CNPC_SynthScanner::IRelationType(CBaseEntity *pTarget)
{
	Disposition_t disp = BaseClass::IRelationType(pTarget);

	if (pTarget == NULL)
		return disp;

	if (m_nFlyMode == SCANNER_FLY_FAST || HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH))
	{
		return D_NU;
	}

	// If the player's not a criminal, then we don't necessary hate him
	if (pTarget->Classify() == CLASS_PLAYER)
	{
		return GlobalEntity_GetState("gordon_precriminal") == 0 ? D_HT : D_NU;
	}

	return disp;
}

bool CNPC_SynthScanner::IsValidInspectTarget(CBaseEntity *pEntity)
{
	if (HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH)) return false;

	// If a citizen, make sure he can be inspected again
	if (Q_strcmp(pEntity->GetClassname(), "npc_bullseye") == 0)
	{
		return true;
	}

	if (m_pSquad)
	{
		AISquadIter_t iter;
		for (CAI_BaseNPC *pSquadMember = m_pSquad->GetFirstMember(&iter); pSquadMember; pSquadMember = m_pSquad->GetNextMember(&iter))
		{
			if (pSquadMember->GetTarget() == pEntity)
			{
				return false;
			}
		}
	}

	if (pEntity->IsNPC())
	{
		// Do not inspect friendly targets
		if (IRelationType(pEntity) == D_LI)
			return false;

		else
			return true;
	}

	return false;
}

CBaseEntity* CNPC_SynthScanner::BestInspectTarget(void)
{
	if (!m_bShouldInspect)
		return NULL;

	CBaseEntity*	pBestEntity = NULL;
	float			fBestDist = MAX_COORD_RANGE;
	float			fTestDist;

	CBaseEntity *pEntity = NULL;

	// If I have a spotlight, search from the spotlight position
	// otherwise search from my position
	Vector	vSearchOrigin;
	float	fSearchDist;
	if (m_hSpotlightTarget != NULL)
	{
		vSearchOrigin = m_hSpotlightTarget->GetAbsOrigin();
		fSearchDist = 512.0f;
	}
	else
	{
		vSearchOrigin = WorldSpaceCenter();
		fSearchDist = 1024.0f;
	}

	CUtlVector<CBaseEntity *> candidates;
	float fSearchDistSq = fSearchDist * fSearchDist;
	int i;

	// NPCs
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();

	for (i = 0; i < g_AI_Manager.NumAIs(); i++)
	{
		if (ppAIs[i] != this && vSearchOrigin.DistToSqr(ppAIs[i]->GetAbsOrigin()) < fSearchDistSq)
		{
			candidates.AddToTail(ppAIs[i]);
		}
	}

	for (i = 0; i < candidates.Count(); i++)
	{
		pEntity = candidates[i];
		Assert(pEntity != this && (pEntity->MyNPCPointer() || pEntity->IsPlayer()));

		CAI_BaseNPC *pNPC = pEntity->MyNPCPointer();
		if ((pNPC && pNPC->Classify() == CLASS_CITIZEN_PASSIVE) || pEntity->IsPlayer())
		{
			if (pEntity->GetFlags() & FL_NOTARGET)
				continue;

			if (pEntity->IsAlive() == false)
				continue;

			// Ensure it's within line of sight
			if (!FVisible(pEntity))
				continue;

			fTestDist = (GetAbsOrigin() - pEntity->EyePosition()).Length();
			if (fTestDist < fBestDist)
			{
				if (IsValidInspectTarget(pEntity))
				{
					fBestDist = fTestDist;
					pBestEntity = pEntity;
				}
			}
		}
	}
	return pBestEntity;
}

//------------------------------------------------------------------------------
// Purpose: Clears any previous inspect target and set inspect target to
//			 the given entity and set the durection of the inspection
//------------------------------------------------------------------------------
void CNPC_SynthScanner::SetInspectTargetToEnt(CBaseEntity *pEntity, float fInspectDuration)
{
	ClearInspectTarget();
	SetTarget(pEntity);

	m_fInspectEndTime = CURTIME + fInspectDuration;
}

//------------------------------------------------------------------------------
// Purpose: Clears any previous inspect target and set inspect target to
//			 the given hint node and set the durection of the inspection
//------------------------------------------------------------------------------
void CNPC_SynthScanner::SetInspectTargetToHint(CAI_Hint *pHint, float fInspectDuration)
{
	ClearInspectTarget();

	float yaw = pHint->Yaw();
	// --------------------------------------------
	// Figure out the location that the hint hits
	// --------------------------------------------
	Vector vHintDir = UTIL_YawToVector(yaw);

	Vector vHintOrigin;
	pHint->GetPosition(this, &vHintOrigin);

	Vector vHintEnd = vHintOrigin + (vHintDir * 512);

	trace_t tr;
	AI_TraceLine(vHintOrigin, vHintEnd, MASK_BLOCKLOS, this, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction == 1.0f)
	{
		DevMsg("ERROR: Scanner hint node not facing a surface!\n");
	}
	else
	{
		SetHintNode(pHint);
		m_vInspectPos = tr.endpos;
		pHint->Lock(this);

		m_fInspectEndTime = CURTIME + fInspectDuration;
	}
}

//------------------------------------------------------------------------------
// Purpose: Clears any previous inspect target and set inspect target to
//			 the given position and set the durection of the inspection
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CNPC_SynthScanner::SetInspectTargetToPos(const Vector &vInspectPos, float fInspectDuration)
{
	ClearInspectTarget();
	m_vInspectPos = vInspectPos;

	m_fInspectEndTime = CURTIME + fInspectDuration;
}

//------------------------------------------------------------------------------
// Purpose: Clears out any previous inspection targets
//------------------------------------------------------------------------------
void CNPC_SynthScanner::ClearInspectTarget(void)
{
	if (GetIdealState() != NPC_STATE_SCRIPT)
	{
		SetTarget(NULL);
	}

	ClearHintNode(5.0f);
	m_vInspectPos = vec3_origin;
}

//------------------------------------------------------------------------------
// Purpose: Returns true if there is a position to be inspected.
//------------------------------------------------------------------------------
bool CNPC_SynthScanner::HaveInspectTarget(void)
{
	if (GetTarget() != NULL)
		return true;

	if (m_vInspectPos != vec3_origin)
		return true;

	return false;
}

Vector CNPC_SynthScanner::InspectTargetPosition(void)
{
	// If we have a target, return an adjust position
	if (GetTarget() != NULL)
	{
		Vector	vEyePos = GetTarget()->EyePosition();

		// If in spotlight mode, aim for ground below target unless is client
		if (m_nFlyMode == SCANNER_FLY_SPOT && !(GetTarget()->GetFlags() & FL_CLIENT))
		{
			Vector vInspectPos;
			vInspectPos.x = vEyePos.x;
			vInspectPos.y = vEyePos.y;
			vInspectPos.z = GetFloorZ(vEyePos);

			// Let's take three-quarters between eyes and ground
			vInspectPos.z += (vEyePos.z - vInspectPos.z) * 0.75f;

			return vInspectPos;
		}
		else
		{
			// Otherwise aim for eyes
			return vEyePos;
		}
	}
	else if (m_vInspectPos != vec3_origin)
	{
		return m_vInspectPos;
	}
	else
	{
		DevMsg("InspectTargetPosition called with no target!\n");

		return m_vInspectPos;
	}
}

void CNPC_SynthScanner::InspectTarget(inputdata_t &inputdata, ScannerFlyMode_t eFlyMode)
{
	CBaseEntity *pEnt = gEntList.FindEntityGeneric(NULL, inputdata.value.String(), this, inputdata.pActivator);

	if (pEnt != NULL)
	{
		// Set and begin to inspect our target
		SetInspectTargetToEnt(pEnt, 5.0f);

		m_nFlyMode = eFlyMode;
		SetCondition(COND_SYSCANNER_HAVE_INSPECT_TARGET);

		// Stop us from any other navigation we were doing
		GetNavigator()->ClearGoal();
	}
	else
	{
		DevMsg("InspectTarget: target %s not found!\n", inputdata.value.String());
	}
}

bool CNPC_SynthScanner::MovingToInspectTarget(void)
{
	// If we're still on a path, then we're still moving
	if (HaveInspectTarget() && GetNavigator()->IsGoalActive())
		return true;
	return false;
}

float CNPC_SynthScanner::MinGroundDist(void)
{
	if (m_nFlyMode == SCANNER_FLY_SPOT && !GetHintNode())
	{
		return SY_SCANNER_SPOTLIGHT_FLY_HEIGHT;
	}

	trace_t tr;
	AI_TraceLine(GetLocalOrigin(), GetLocalOrigin() + Vector(0, 0, 32), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction < 1.0) // check for narrow spaces
		return 32;
	return 64;
}

float CNPC_SynthScanner::GetMaxSpeed(void)
{
	float speed = SCANNER_MAX_SPEED;

	if (m_nFlyMode == SCANNER_FLY_ATTACK)
		speed += 200;

	if (m_nFlyMode == SCANNER_FLY_FAST)
		speed += 150;

	if (IsWastelandScanner())
		speed *= 4;

	if (HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH)) speed = 0;

	return speed;
}

// TODO: Why don't the sentences work right? Currently have to just use EmitSound()...
void CNPC_SynthScanner::SpeakSentence(int nSentenceType)
{
	if (HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH)) return; 

	switch (nSentenceType)
	{
	case SYSCANNER_SENTENCE_IDLE:
	{
		EmitSound("NPC_Synthscanner.IdleChatter");
		//	m_Sentences.Speak("SYNTHSCANNER_IDLE", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_ALWAYS);
	}
	break;
	case SYSCANNER_SENTENCE_INSPECT:
	{
		EmitSound("NPC_Synthscanner.InspectChatter");
		//	m_Sentences.Speak("SYNTHSCANNER_INSPECT", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_ALWAYS);
	}
	break;
	case SYSCANNER_SENTENCE_ALERT:
	{
		EmitSound("NPC_Synthscanner.AlertChatter");
		//	m_Sentences.Speak("SYNTHSCANNER_ALERT", SENTENCE_PRIORITY_MEDIUM, SENTENCE_CRITERIA_ALWAYS);
	}
	break;
	}
}

int CNPC_SynthScanner::OnTakeDamage_Alive(const CTakeDamageInfo &info)
{ 
	// Turn off my spotlight when shot
	SpotlightDestroy();
	m_fNextSpotlightTime = CURTIME + 2.0f;

	CTakeDamageInfo infoCopy = info;

	if (IsShieldActive()) // the shield protects from bullets, burning, and fast projectiles.
	{
		if (infoCopy.GetDamageType() & DMG_BULLET)
		{
			infoCopy.SetDamage(0.1f);
		}
		if (infoCopy.GetDamageType() & DMG_BURN) infoCopy.SetDamage(0.1f);
	}
	else
	{
		// Respond to attacks - try to dodge
		if (info.GetAttacker()->IsPlayer() && VPhysicsGetObject())
		{
			Vector vRight, vUp;
			GetVectors(NULL, &vRight, &vUp);
			if (entindex() % 2)
			{
				VPhysicsGetObject()->ApplyForceCenter(vRight * 300 - vUp * 100);
			}
			else
			{
				VPhysicsGetObject()->ApplyForceCenter(-vRight * 300 - vUp * 100);
			}
		}
	}

	// don't take damage from toxic clouds.
	if (infoCopy.GetDamageType() & DMG_NERVEGAS) infoCopy.SetDamage(0);

	return (BaseClass::OnTakeDamage_Alive(infoCopy));
}

void CNPC_SynthScanner::Gib(void)
{
	if (IsMarkedForDeletion())
		return;

	// Spawn all gibs

	CGib::SpawnSpecificGibs(this, 1, 500, 250, "models/_Monsters/Combine/synth_scanner_gib_top.mdl", 120.0f, m_nSkin - 1);
	CGib::SpawnSpecificGibs(this, 1, 500, 250, "models/_Monsters/Combine/synth_scanner_gib_left.mdl", 120.0f, m_nSkin - 1);
	CGib::SpawnSpecificGibs(this, 1, 500, 250, "models/_Monsters/Combine/synth_scanner_gib_right.mdl", 120.0f, m_nSkin - 1);
	CGib::SpawnSpecificGibs(this, 1, 500, 250, "models/gibs/Shield_Scanner_Gib1.mdl");
	CGib::SpawnSpecificGibs(this, 1, 500, 250, "models/gibs/Shield_Scanner_Gib6.mdl");

	// Add a random chance of spawning a health nugget...
	if (!HasSpawnFlags(SF_NPC_NO_WEAPON_DROP) && random->RandomFloat(0.0f, 1.0f) < 0.3f)
	{
		CItem *pBattery = (CItem*)CreateEntityByName("item_healthvial");
		if (pBattery)
		{
			pBattery->SetAbsOrigin(GetAbsOrigin());
			pBattery->SetAbsVelocity(GetAbsVelocity());
			pBattery->SetLocalAngularVelocity(GetLocalAngularVelocity());
			pBattery->ActivateWhenAtRest();
			pBattery->Spawn();
		}
	}

	DispatchParticleEffect("antlion_spit", WorldSpaceCenter(), QAngle(0, 0, 0));
	DispatchParticleEffect("blood_impact_antlion_01", WorldSpaceCenter(), QAngle(0, 0, 0));
	DispatchParticleEffect("blood_impact_synth_01", WorldSpaceCenter(), QAngle(0, 0, 0));

	BaseClass::Gib();
}

void CNPC_SynthScanner::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr)
{
	// This is copied from BaseClass, 
	// because we don't do ricochet in 
	// BaseClass anymore. (didn't look
	// right on non-metal CBaseScanner derivatives.) 
	if (info.GetDamageType() & DMG_BULLET)
	{
		if (!IsShieldActive())
		{
			//	g_pEffects->Ricochet(ptr->endpos, ptr->plane.normal);
			UTIL_BloodImpact(ptr->endpos, ptr->plane.normal, BloodColor(), RandomInt(0, 2));
		}

		else
		{
			if (m_shield_ehandle != NULL)
			{
				EmitSound("NPC_SynthScanner.ShieldImpact");
				QAngle vecAngles;
				Vector traceDir = ptr->endpos - ptr->startpos;
				VectorNormalize(traceDir);
				VectorAngles(-vecDir, vecAngles);
				vecAngles[1] += 90;
				DispatchParticleEffect("wastelandscanner_shield_impact", ptr->endpos - traceDir * 4, vecAngles);
				if (m_forcefieldshield_integrity_int > 0) m_forcefieldshield_integrity_int -= (int)info.GetDamage(); // MICROTEST ARENA CHANGE - make scanner shield depletable by firing // TODO: explosions too?
			}
			return;
		}
	}
	/*
	if (info.GetDamageType() & DMG_BUCKSHOT)
	{
	if( RandomInt(0,3) == 0)
	EmitSound("NPC_SynthScanner.ShieldImpact");
	}
	*/
	BaseClass::TraceAttack(info, vecDir, ptr);
}

void CNPC_SynthScanner::Event_Killed(const CTakeDamageInfo &info)
{
	// Copy off the takedamage info that killed me, since we're not going to call
	// up into the base class's Event_Killed() until we gib. (gibbing is ultimate death)
	m_KilledInfo = info;
	ClearInspectTarget();
	// Interrupt whatever schedule I'm on
	SetCondition(COND_SCHEDULE_DONE);
	// Remove spotlight
	SpotlightDestroy();
	// Remove sprite
	UTIL_Remove(m_pEyeFlash);
	m_pEyeFlash = NULL;
	// Remove shield
	UTIL_Remove(m_shield_ehandle);
	m_shield_ehandle = NULL;

//	if (GetHealth() - info.GetDamage() <= -10)
		Gib();
//	else
//		SetSchedule(SCHED_DIE_RAGDOLL);
}

void CNPC_SynthScanner::GatherConditions(void)
{
	BaseClass::GatherConditions();

	// Clear out our old conditions
	ClearCondition(COND_SYSCANNER_INSPECT_DONE);
	ClearCondition(COND_SYSCANNER_HAVE_INSPECT_TARGET);
	ClearCondition(COND_SYSCANNER_SPOT_ON_TARGET);
	ClearCondition(COND_SYSCANNER_TOO_WEAK_TO_ATTACK);

	// combat confidence
	if (CombatConfidence() <= m_iMaxCombatConfidence / 2)
	{
		SetCondition(COND_SYSCANNER_TOO_WEAK_TO_ATTACK);
	}

	// Force field shield: create when notice enemy, lives for fixed amount of time, 
	// remove when no enemy present or shield duration time is over, cooldown after it goes down
	if (IsWastelandScanner())
	{
		if (GetEnemy())
		{
			if (!IsShieldActive() && CURTIME > m_forcefieldshield_cooldown_time_float)
			{
				if (!IsHeldByPhyscannon())
				{
					CreateForceFieldShield(25);
				}
			}

			if (IsShieldActive() && ((m_forcefieldshield_integrity_int <= 0) || (CURTIME > m_forcefieldshield_creation_time_float + npc_synthscanner_shield_duration.GetFloat())))
			{
				CollapseForceFieldShield(false);
				m_forcefieldshield_cooldown_time_float = CURTIME + npc_synthscanner_shield_duration.GetFloat();
			}

			if (IsShieldActive() && (CURTIME - GetEnemyLastTimeSeen() > 5.0f))
			{
				CollapseForceFieldShield(false);
				m_forcefieldshield_cooldown_time_float = CURTIME + 2.0f;
			}
		}
		else
		{
			if (IsShieldActive())
			{
				CollapseForceFieldShield(false);
				m_forcefieldshield_cooldown_time_float = CURTIME + 1.0f;
			}
		}
	}

	// We don't do any of these checks if we have an enemy
	if (GetEnemy())
		return;
	// --------------------------------------
	//  COND_SYSCANNER_INSPECT_DONE
	//
	// If my inspection over 
	// ---------------------------------------------------------

	// Refresh our timing if we're still moving to our inspection target
	if (MovingToInspectTarget())
	{
		m_fInspectEndTime = CURTIME + 5.0f;
	}

	// Update our follow times
	if (HaveInspectTarget() && CURTIME > m_fInspectEndTime && m_nFlyMode != SCANNER_FLY_FOLLOW)
	{
		SetCondition(COND_SYSCANNER_INSPECT_DONE);

		m_fCheckNPCTime = CURTIME + 10.0f;
		m_fCheckHintTime = CURTIME + SY_SCANNER_HINT_INSPECT_DELAY;
		ClearInspectTarget();
	}

	// ----------------------------------------------------------
	//  If I heard a sound and I don't have an enemy, inspect it
	// ----------------------------------------------------------
	if ((HasCondition(COND_HEAR_COMBAT) || HasCondition(COND_HEAR_DANGER)) && m_nFlyMode != SCANNER_FLY_ATTACK && m_nFlyMode != SCANNER_FLY_FOLLOW)
	{
		CSound *pSound = GetBestSound();

		if (pSound)
		{
			// Chase an owner if we can
			if (pSound->m_hOwner != NULL)
			{
				// Don't inspect sounds of things we like
				if (IRelationType(pSound->m_hOwner) != D_LI)
				{
					// Only bother if we can see it
					if (FVisible(pSound->m_hOwner))
					{
						SetInspectTargetToEnt(pSound->m_hOwner, 10.0f);
					}
				}
			}
			else
			{
				// Otherwise chase the specific sound
				Vector vSoundPos = pSound->GetSoundOrigin();
				SetInspectTargetToPos(vSoundPos, 10.0f);
			}

			m_nFlyMode = SCANNER_FLY_SPOT;
		}
	}

	// --------------------------------------
	//  COND_CSCANNER_HAVE_INSPECT_TARGET
	//
	// Look for a nearby citizen or player to hassle. 
	// ---------------------------------------------------------

	// Check for citizens to inspect
	if (CURTIME	> m_fCheckNPCTime && HaveInspectTarget() == false)
	{
		CBaseEntity *pBestEntity = BestInspectTarget();

		if (pBestEntity != NULL)
		{
			SetInspectTargetToEnt(pBestEntity, 5.0f);
			m_nFlyMode = SCANNER_FLY_SPOT;
			SetCondition(COND_SYSCANNER_HAVE_INSPECT_TARGET);
		}
	}
	/*
	// Check for hints to inspect
	if (CURTIME > m_fCheckHintTime && HaveInspectTarget() == false)
	{
	SetHintNode(CAI_HintManager::FindHint(this, HINT_WORLD_WINDOW, 0, 1024.0f));

	if (GetHintNode())
	{
	m_fCheckHintTime = CURTIME + SCANNER_HINT_INSPECT_DELAY;

	m_nFlyMode = SCANNER_FLY_SPOT;

	SetInspectTargetToHint(GetHintNode(), 5.0f);

	SetCondition(COND_SYSCANNER_HAVE_INSPECT_TARGET);
	}
	}
	*/
	// --------------------------------------
	//  COND_CSCANNER_SPOT_ON_TARGET
	//
	//  True when spotlight is on target ent
	// --------------------------------------

	if (m_hSpotlightTarget != NULL	&& HaveInspectTarget() && m_hSpotlightTarget->GetSmoothedVelocity().Length() < 25)
	{
		// If I have a target entity, check my spotlight against the
		// actual position of the entity
		if (GetTarget())
		{
			float fInspectDist = (m_vSpotlightTargetPos - m_vSpotlightCurrentPos).Length();
			if (fInspectDist < 100)
			{
				SetCondition(COND_SYSCANNER_SPOT_ON_TARGET);
			}
		}
		// Otherwise just check by beam direction
		else
		{
			Vector vTargetDir = SpotlightTargetPos() - GetLocalOrigin();
			VectorNormalize(vTargetDir);
			float dotpr = DotProduct(vTargetDir, m_vSpotlightDir);
			if (dotpr > 0.95)
			{
				SetCondition(COND_SYSCANNER_SPOT_ON_TARGET);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: For innate melee attack
//-----------------------------------------------------------------------------
int CNPC_SynthScanner::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (GetEnemy() == NULL)
	{
		return COND_NONE;
	}

	if (GetEnemy() && !GetEnemy()->IsAlive())
	{
		return COND_ENEMY_DEAD;
	}
	/*
	if (m_iHealth < (3 * sk_synth_scanner_health.GetFloat() / 4))
	{
	return COND_SYSCANNER_TOO_WEAK_TO_ATTACK;
	}
	*/
	// Check too far to attack with 2D distance
	//	float vEnemyDist2D = (GetEnemy()->GetLocalOrigin() - GetLocalOrigin()).Length2D();

	if (m_flNextAttack > CURTIME)
	{
		return COND_NONE;
	}
	else if (flDist > 256.0f)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.7)
	{
		return COND_NOT_FACING_ATTACK;
	}
	return COND_CAN_MELEE_ATTACK1;
}

Activity CNPC_SynthScanner::NPC_TranslateActivity(Activity eNewActivity)
{
	switch (eNewActivity)
	{
	case ACT_SMALL_FLINCH:
	{
		if (RandomInt(0, 3) == 0)
			return ACT_WSCANNER_FLINCH_BACK;
		else if (RandomInt(0, 3) == 1)
			return ACT_WSCANNER_FLINCH_FRONT;
		else if (RandomInt(0, 3) == 2)
			return ACT_WSCANNER_FLINCH_LEFT;
		else if (RandomInt(0, 3) == 3)
			return ACT_WSCANNER_FLINCH_RIGHT;
	}
	case ACT_IDLE:
	{
		if (HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH)) return ACT_IDLE_STEALTH;
		else return ACT_MELEE_ATTACK1;
	}
	}
	return BaseClass::NPC_TranslateActivity(eNewActivity);
}

int CNPC_SynthScanner::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_SCANNER_PATROL:
	{
		return SCHED_SYSCANNER_PATROL;
	}

	case SCHED_SCANNER_ATTACK:
	{
		if (RandomInt(0, 3) > 0)
			return SCHED_SYSCANNER_ATTACK_SHOCK;
		else
			return SCHED_SYSCANNER_ATTACK_GAS;
	}

	case SCHED_SCANNER_CHASE_ENEMY:
	{
		return SCHED_SYSCANNER_MOVE_TO_ATTACK;
	}

	case SCHED_SCANNER_ATTACK_HOVER:
	{
		return SCHED_SYSCANNER_ATTACK_HOVER;
	}
	}

	return BaseClass::TranslateSchedule(scheduleType);
}

void CNPC_SynthScanner::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();

	m_Sentences.UpdateSentenceQueue();

	if (HasCondition(COND_SYSCANNER_TOO_WEAK_TO_ATTACK))
	{
		{
			if (developer.GetInt() > 0)
				UTIL_CenterPrintAll("Synth Scanner Attempting Escape\n");
			BaseClass::PrescheduleThink();
		}
	}
	else
	{
		if (m_nFlyMode == SCANNER_FLY_FAST)
			m_nFlyMode = SCANNER_FLY_PATROL;
	}
}

int CNPC_SynthScanner::SelectSchedule(void)
{
	// Turn our flash off in case we were interrupted while it was on.
	if (m_pEyeFlash)
	{
		m_pEyeFlash->SetBrightness(0);
	}

	m_bNoLight = false;

	if (HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH))
		return SCHED_IDLE_STAND;

	// -------------------------------
	// If I'm in a script sequence
	// -------------------------------
	if (m_NPCState == NPC_STATE_SCRIPT)
		return(BaseClass::SelectSchedule());

	if (HasCondition(COND_SYSCANNER_TOO_WEAK_TO_ATTACK) && m_nFlyMode != SCANNER_FLY_FAST)
	{
		return SCHED_SYSCANNER_MOVE_ESCAPE; // take priority over any other
	}
	else
	{
		// -------------------------------
		// Flinch
		// -------------------------------
		if (HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE))
		{
			if (IsHeldByPhyscannon())
				return SCHED_SMALL_FLINCH;

			if (m_NPCState == NPC_STATE_IDLE)
				return SCHED_SMALL_FLINCH;

			if (m_NPCState == NPC_STATE_ALERT || NPC_STATE_COMBAT)
			{
				return SCHED_SMALL_FLINCH;
			}
			else
			{
				if (random->RandomInt(0, 10) < 4)
					return SCHED_SMALL_FLINCH;
			}
		}

		if (HasCondition(COND_SYSCANNER_TOO_WEAK_TO_ATTACK))
		{
			return SCHED_SYSCANNER_MOVE_ESCAPE;
		}

		// I'm being held by the physcannon... struggle!
		if (IsHeldByPhyscannon())
			return SCHED_SMALL_FLINCH;

		// ----------------------------------------------------------
		//  If I have an enemy
		// ----------------------------------------------------------
		if (GetEnemy() != NULL	&&
			GetEnemy()->IsAlive())
		{
			m_bNoLight = true;

			if (HasCondition(COND_LOST_ENEMY))
			{
				return SCHED_SCANNER_PATROL;
			}
			else if (HasCondition(COND_SCANNER_FLY_BLOCKED))
			{
				return SCHED_SCANNER_CHASE_ENEMY;
			}
			else if (HasCondition(COND_TOO_FAR_TO_ATTACK))
			{
				return SCHED_SYSCANNER_MOVE_TO_ATTACK;
			}
			else if (HasCondition(COND_CAN_MELEE_ATTACK1))
			{
				return SCHED_SCANNER_ATTACK;
			}
			else
			{
				return SCHED_SCANNER_ATTACK_HOVER;
			}
		}
		else
		{
			return SCHED_SCANNER_PATROL;
		}		
	}
	return BaseClass::SelectSchedule();
}

void CNPC_SynthScanner::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_SYSCANNER_GET_PATH_TO_INSPECT_TARGET:
	{
		if (HaveInspectTarget() == false)
		{
			TaskFail("No inspection target to fly to!\n");
			return;
		}

		if (GetTarget())
		{
			Vector idealPos = IdealGoalForMovement(InspectTargetPosition(), GetAbsOrigin(), 128.0f, 128.0f);

			AI_NavGoal_t goal(GOALTYPE_TARGETENT, vec3_origin);

			if (GetNavigator()->SetGoal(goal))
			{
				TaskComplete();
				return;
			}
		}
		else
		{
			AI_NavGoal_t goal(GOALTYPE_LOCATION, InspectTargetPosition());

			if (GetNavigator()->SetGoal(goal))
			{
				TaskComplete();
				return;
			}
		}
		TaskFail("No route to inspection target!\n");
		break;
	}

	case TASK_SYSCANNER_SPOT_INSPECT_ON:
	{
		if (GetTarget() == NULL)
		{
			TaskFail(FAIL_NO_TARGET);
		}
		else
		{
			CAI_BaseNPC* pNPC = GetTarget()->MyNPCPointer();
			if (!pNPC)
			{
				TaskFail(FAIL_NO_TARGET);
			}
			else
			{
				m_fInspectEndTime = CURTIME + 5.0f;
				TaskComplete();
			}
		}
		break;
	}

	case TASK_SYSCANNER_SPOT_INSPECT_WAIT:
	{
		if (GetTarget() == NULL)
		{
			TaskFail(FAIL_NO_TARGET);
		}
		else
		{
			CAI_BaseNPC* pNPC = GetTarget()->MyNPCPointer();
			if (!pNPC)
			{
				SetTarget(NULL);
				TaskFail(FAIL_NO_TARGET);
			}
			else
			{
			}
			TaskComplete();
		}
		break;
	}

	case TASK_SYSCANNER_SPOT_INSPECT_OFF:
	{
		if (GetTarget() == NULL)
		{
			TaskFail(FAIL_NO_TARGET);
		}
		else
		{
			CAI_BaseNPC* pNPC = GetTarget()->MyNPCPointer();
			if (!pNPC)
			{
				TaskFail(FAIL_NO_TARGET);
			}
			else
			{
				SetTarget(NULL);
				m_fCheckNPCTime = CURTIME + 10.0f;
				TaskComplete();
			}
		}
		break;
	}

	case TASK_SYSCANNER_CLEAR_INSPECT_TARGET:
	{
		ClearInspectTarget();
		TaskComplete();
		break;
	}

	case TASK_SYSCANNER_SET_FLY_SPOT:
	{
		m_nFlyMode = SCANNER_FLY_SPOT;
		TaskComplete();
		break;
	}

	case TASK_SYSCANNER_SET_FLY_FAST:
	{
		m_nFlyMode = SCANNER_FLY_FAST;
		TaskComplete();
		break;
	}

	case TASK_SYSCANNER_ATTACK_PRE_FLASH:
	{
		if (m_pEyeFlash)
		{
			AttackPreFlash();
			SetWait(0.5f);
		}
		else
		{
			TaskFail("No Flash");
		}
		break;
	}

	case TASK_SYSCANNER_ATTACK_GAS:
	{
		AttackGas();
		SetWait(0.05);
		break;
	}

	case TASK_SYSCANNER_ATTACK_PRE_SHOCK:
	{
		AttackPreShock();
		SetWait(0.25f);
		break;
	}

	case TASK_SYSCANNER_ATTACK_SHOCK:
	{
		AttackShock();
		SetWait(5.0f);
		break;
	}

	case TASK_GET_PATH_TO_TARGET:
	{
		if (!HaveInspectTarget())
		{
			TaskFail(FAIL_NO_TARGET);
		}
		else if (GetHintNode())
		{
			Vector vNodePos;
			GetHintNode()->GetPosition(this, &vNodePos);

			GetNavigator()->SetGoal(vNodePos);
		}
		else
		{
			AI_NavGoal_t goal((const Vector &)InspectTargetPosition());
			goal.pTarget = GetTarget();
			GetNavigator()->SetGoal(goal);
		}
		break;
	}

	case TASK_FIND_HINTNODE:
	{
		// Overloaded because we search over a greater distance.
		if (!GetHintNode())
		{
			SetHintNode(CAI_HintManager::FindHint(this, HINT_TACTICAL_SPAWN, bits_HINT_NODE_NEAREST | bits_HINT_NODE_USE_GROUP, 10000));
		}

		if (GetHintNode())
		{
			TaskComplete();
		}
		else
		{
			DevMsg("TASK_FIND_HINTNODE failed - no hintnode!\n");
			TaskFail(FAIL_NO_HINT_NODE);
		}
		break;
	}

	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

void CNPC_SynthScanner::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_SYSCANNER_ATTACK_PRE_FLASH:
	{
		if (IsWaitFinished())
		{
			TaskComplete();
		}
		break;
	}
	case TASK_SYSCANNER_ATTACK_GAS:
	{
		if (IsWaitFinished())
		{
			TaskComplete();
		}
		break;
	}
	case TASK_SYSCANNER_ATTACK_PRE_SHOCK:
	{
		if (IsWaitFinished())
		{
			TaskComplete();
		}
		break;
	}
	case TASK_SYSCANNER_ATTACK_SHOCK:
	{
		if (IsWaitFinished())
		{
			EntityMessageBegin(this, true);
			WRITE_BYTE(2);
			MessageEnd();
			TaskComplete();
		}
		break;
	}
	default:
	{
		BaseClass::RunTask(pTask);
	}
	}
}

void CNPC_SynthScanner::AttackPreFlash(void)
{
	EmitSound("NPC_SynthScanner.PreAttack");
	if (m_pEyeFlash->GetBrightness() == 0)
	{
		m_pEyeFlash->SetScale(0.5);
		m_pEyeFlash->SetBrightness(255);
		m_pEyeFlash->SetColor(110, 82, 9);
	}
	else
	{
		m_pEyeFlash->SetBrightness(0);
	}

//	if (IsShieldActive())
//	{
//	CollapseForceFieldShield(false); // MICROTEST ARENA CHANGE - make scanners lose shield as prerequisite to attack
//	m_forcefieldshield_cooldown_time_float = CURTIME + npc_synthscanner_shield_duration.GetFloat();
//	}
}

void CNPC_SynthScanner::AttackGas(void)
{
	EmitSound("NPC_SynthScanner.AttackGas");
	m_pEyeFlash->SetScale(0.8);
	m_pEyeFlash->SetBrightness(255);
	m_pEyeFlash->SetColor(110, 82, 9);

	if (GetEnemy() != NULL)
	{
		Vector	vEnemyPos = GetEnemy()->EyePosition() + GetEnemy()->GetLocalVelocity();
		Vector	vGasDirection = GetLocalOrigin() - vEnemyPos;
		//========================
		//Gas cloud			
		m_hGasCloud = CToxicCloud::CreateGasCloud(vEnemyPos, this, "synth_scanner_gas", CURTIME + 5.0f, TOXICCLOUD_ACID, 75);
		//========================
		// Compute attack dir and punch player's view
		Vector	vEnemyDir = GetLocalOrigin() - GetEnemy()->GetLocalOrigin();
		Vector gasDir = vGasDirection;
		VectorNormalize(vEnemyDir);
		VectorNormalize(gasDir);
		float	dotPr = DotProduct(vEnemyDir, gasDir);
		if (dotPr > 0.50)
		{
			if (GetEnemy()->IsPlayer())
			{
				GetEnemy()->ViewPunch(QAngle(random->RandomInt(-10, 10), 0, random->RandomInt(-10, 10)));

				color32 gas = { 110,82,9,75 };
				gas.a = (byte)((float)gas.a * 0.9f);
				float flFadeTime = 3.0f;
				UTIL_ScreenFade(GetEnemy(), gas, flFadeTime, 0.5, FFADE_IN);
			}
		}

		m_flNextAttack = CURTIME + (RandomFloat(6, 8));
	}
}

void CNPC_SynthScanner::AttackPreShock(void)
{
	EmitSound("NPC_SynthScanner.PreAttack");
	if (m_pEyeFlash)
	{
		if (m_pEyeFlash->GetBrightness() == 0)
		{
			m_pEyeFlash->SetScale(0.5);
			m_pEyeFlash->SetBrightness(255);
			m_pEyeFlash->SetColor(10, 255, 225);
		}
		else
		{
			m_pEyeFlash->SetBrightness(0);
		}
	}
}

void CNPC_SynthScanner::AttackShock(void)
{
	if (IsShieldActive())
	{
		CollapseForceFieldShield(false); // MICROTEST ARENA CHANGE - make scanners lose shield as prerequisite to attack
		m_forcefieldshield_cooldown_time_float = CURTIME + npc_synthscanner_shield_duration.GetFloat() / 2;
	}

	if (m_pEyeFlash) m_pEyeFlash->SetBrightness(0);
	Vector forward;
	GetVectors(&forward, NULL, NULL);
	int light = LookupAttachment("light");
	Vector vecSrc; // = GetLocalOrigin() + GetViewOffset();
	GetAttachment("light", vecSrc);
	Vector vecAim = GetShootEnemyDir(vecSrc, false);	// We want a clear shot to their core

	trace_t tr;
	AI_TraceLine(vecSrc, vecSrc + (vecAim * 256), MASK_SHOT, this, COLLISION_GROUP_INTERACTIVE, &tr);

	// Send a message to the client to create a "zap" beam
	unsigned char uchAttachment = light;
	EntityMessageBegin(this, true);
	WRITE_BYTE(0);
	WRITE_BYTE(uchAttachment);
	WRITE_VEC3COORD(tr.endpos);
	MessageEnd();

	CBaseEntity *pEntity = tr.m_pEnt;
	if (pEntity != NULL && m_takedamage)
	{
		if (pEntity->IsPlayer())
		{
			float yawKick = random->RandomFloat(-48, -24);
			pEntity->ViewPunch(QAngle(-16, yawKick, 2));
			color32 white = { 255,255,255,50 };
			white.a = (byte)((float)white.a * 0.9f);
			float flFadeTime = 1.0f;
			UTIL_ScreenFade(pEntity, white, flFadeTime, 0.5, FFADE_IN);
			pEntity->SetLocalAngles(QAngle(GetLocalAngles().x, GetLocalAngles().y, RandomInt(0, 180)));

			ClearMultiDamage();
			CTakeDamageInfo dmgInfo(this, this, sk_scanner_shock_dmg_plr.GetFloat(), DMG_SHOCK);
			dmgInfo.SetDamagePosition(tr.endpos);
			VectorNormalize(vecAim);
			dmgInfo.SetDamageForce(5 * 100 * 6 * vecAim);
			ApplyMultiDamage();

			float m_flDuration = 3.99f;
			EntityMessageBegin(this, true);
			WRITE_BYTE(1);
			WRITE_BYTE(m_flDuration);
			MessageEnd();

			m_flNextAttack = CURTIME + 4.0f;
		}
		else
		{
			ClearMultiDamage();
			CTakeDamageInfo dmgInfo(this, this, sk_scanner_shock_dmg_npc.GetFloat(), DMG_SHOCK);
			dmgInfo.SetDamagePosition(tr.endpos);
			VectorNormalize(vecAim);
			dmgInfo.SetDamageForce(5 * 100 * 2 * vecAim);
			pEntity->DispatchTraceAttack(dmgInfo, vecAim, &tr);
			ApplyMultiDamage();
		}
	}

	CPVSFilter filter(tr.endpos);
	te->GaussExplosion(filter, 0.0f, tr.endpos, Vector(0, 0, 1), 0);

	if (IsShieldActive())
	{
		m_forcefieldshield_creation_time_float -= 1.5f; // shorten the shield duration by 1 second for every zap.
	}
}

void CNPC_SynthScanner::BlindFlashTarget(CBaseEntity *pTarget)
{
	// Only bother with player
	if (pTarget->IsPlayer() == false)
		return;

	// Scale the flash value by how closely the player is looking at me
	Vector vFlashDir = GetAbsOrigin() - pTarget->EyePosition();
	VectorNormalize(vFlashDir);

	Vector vFacing;
	AngleVectors(pTarget->EyeAngles(), &vFacing);

	float dotPr = DotProduct(vFlashDir, vFacing);

	// Not if behind us
	if (dotPr > 0.5f)
	{
		// Make sure nothing in the way
		trace_t tr;
		AI_TraceLine(GetAbsOrigin(), pTarget->EyePosition(), MASK_OPAQUE, this, COLLISION_GROUP_NONE, &tr);

		if (tr.startsolid == false && tr.fraction == 1.0)
		{
			color32 yellow = { 240, 255, 50, (byte)(SY_SCANNER_FLASH_MAX_VALUE * dotPr) };

			yellow.a = (byte)((float)yellow.a * 0.9f);
			float flFadeTime = 1.5f;
			UTIL_ScreenFade(pTarget, yellow, flFadeTime, 0.5, FFADE_IN);
		}
	}
}

void CNPC_SynthScanner::AttackFlashBlind(void)
{
	if (GetEnemy())
	{
		BlindFlashTarget(GetEnemy());
	}

	m_pEyeFlash->SetBrightness(0);

	float fAttackDelay = random->RandomFloat(SCANNER_ATTACK_MIN_DELAY, SCANNER_ATTACK_MAX_DELAY);

	m_flNextAttack = CURTIME + fAttackDelay;
	m_fNextSpotlightTime = CURTIME + 1.0f;
}

void CNPC_SynthScanner::SpotlightDestroy(void)
{
	if (m_hSpotlight)
	{
		UTIL_Remove(m_hSpotlight);
		m_hSpotlight = NULL;

		UTIL_Remove(m_hSpotlightTarget);
		m_hSpotlightTarget = NULL;
	}
}

void CNPC_SynthScanner::SpotlightCreate(void)
{
	// Make sure we don't already have one
	if (m_hSpotlight != NULL)
		return;

	// Can we create a spotlight yet?
	if (CURTIME < m_fNextSpotlightTime)
		return;

	// No lights in ambush mode
	if (HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH))
		return;

	// If I have an enemy, start spotlight on my enemy
	if (GetEnemy() != NULL)
	{
		Vector vEnemyPos = GetEnemyLKP();
		Vector vTargetPos = vEnemyPos;
		vTargetPos.z = GetFloorZ(vEnemyPos);
		m_vSpotlightDir = vTargetPos - GetLocalOrigin();
		VectorNormalize(m_vSpotlightDir);
	}
	// If I have an target, start spotlight on my target
	else if (GetTarget() != NULL)
	{
		Vector vTargetPos = GetTarget()->GetLocalOrigin();
		vTargetPos.z = GetFloorZ(GetTarget()->GetLocalOrigin());
		m_vSpotlightDir = vTargetPos - GetLocalOrigin();
		VectorNormalize(m_vSpotlightDir);
	}
	// Otherwise just start looking down
	else
	{
		m_vSpotlightDir = Vector(0, 0, -1);
	}

	trace_t tr;
	AI_TraceLine(GetAbsOrigin(), GetAbsOrigin() + m_vSpotlightDir * 2024, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	m_hSpotlightTarget = (CSpotlightEnd*)CreateEntityByName("spotlight_end");
	m_hSpotlightTarget->Spawn();
	m_hSpotlightTarget->SetLocalOrigin(tr.endpos);
	m_hSpotlightTarget->SetOwnerEntity(this);
	if( m_bNoDLight ) m_hSpotlightTarget->m_flLightScale = 0.0f;
	// YWB:  Because the scanner only moves the target during think, make sure we interpolate over 0.1 sec instead of every tick!!!
	m_hSpotlightTarget->SetSimulatedEveryTick(true);

	// Using the same color as the beam...
	m_hSpotlightTarget->SetRenderColor(240, 255, 50);
	m_hSpotlightTarget->m_Radius = m_flSpotlightMaxLength;

	m_hSpotlight = CBeam::BeamCreate("sprites/glow_test02.vmt", SY_SPOTLIGHT_WIDTH);
	// Set the temporary spawnflag on the beam so it doesn't save (we'll recreate it on restore)
	m_hSpotlight->AddSpawnFlags(SF_BEAM_TEMPORARY);
	m_hSpotlight->SetColor(240, 255, 50);
	m_hSpotlight->SetHaloTexture(m_nHaloSprite);
	m_hSpotlight->SetHaloScale(32);
	m_hSpotlight->SetEndWidth(m_hSpotlight->GetWidth());
	m_hSpotlight->SetBeamFlags((FBEAM_SHADEOUT | FBEAM_NOTILE));
	m_hSpotlight->SetBrightness(64);
	m_hSpotlight->SetNoise(0);
	m_hSpotlight->EntsInit(this, m_hSpotlightTarget);
	m_hSpotlight->SetHDRColorScale(0.5f);	// Scale this back a bit on HDR maps
											// attach to light
	m_hSpotlight->SetStartAttachment(LookupAttachment(SY_SCANNER_ATTACHMENT_LIGHT));

	m_vSpotlightAngVelocity = vec3_origin;
}

Vector CNPC_SynthScanner::SpotlightTargetPos(void)
{
	// ----------------------------------------------
	//  If I have an enemy 
	// ----------------------------------------------
	if (GetEnemy() != NULL)
	{
		// If I can see my enemy aim for him
		if (HasCondition(COND_SEE_ENEMY))
		{
			// If its client aim for his eyes
			if (GetEnemy()->GetFlags() & FL_CLIENT)
			{
				m_vSpotlightTargetPos = GetEnemy()->EyePosition();
			}
			// Otherwise same for his feet
			else
			{
				m_vSpotlightTargetPos = GetEnemy()->GetLocalOrigin();
				m_vSpotlightTargetPos.z = GetFloorZ(GetEnemy()->GetLocalOrigin());
			}
		}
		// Otherwise aim for last known position if I can see LKP
		else
		{
			Vector vLKP = GetEnemyLKP();
			m_vSpotlightTargetPos.x = vLKP.x;
			m_vSpotlightTargetPos.y = vLKP.y;
			m_vSpotlightTargetPos.z = GetFloorZ(vLKP);
		}
	}
	else
	{
		// This creates a nice patrol spotlight sweep
		// in the direction that I'm travelling
		m_vSpotlightTargetPos = GetCurrentVelocity();
		m_vSpotlightTargetPos.z = 0;
		VectorNormalize(m_vSpotlightTargetPos);
		m_vSpotlightTargetPos *= 5;

		float noiseScale = 2.5;
		const Vector &noiseMod = GetNoiseMod();
		m_vSpotlightTargetPos.x += noiseScale*sin(noiseMod.x * CURTIME + noiseMod.x);
		m_vSpotlightTargetPos.y += noiseScale*cos(noiseMod.y* CURTIME + noiseMod.y);
		m_vSpotlightTargetPos.z -= fabs(noiseScale*cos(noiseMod.z* CURTIME + noiseMod.z));
		m_vSpotlightTargetPos = GetLocalOrigin() + m_vSpotlightTargetPos * 2024;
	}

	return m_vSpotlightTargetPos;
}

Vector CNPC_SynthScanner::SpotlightCurrentPos(void)
{
	Vector vTargetDir = SpotlightTargetPos() - GetLocalOrigin();
	VectorNormalize(vTargetDir);

	if (!m_hSpotlight)
	{
		DevMsg("Spotlight pos. called w/o spotlight!\n");
		return vec3_origin;
	}
	// -------------------------------------------------
	//  Beam has momentum relative to it's ground speed
	//  so sclae the turn rate based on its distance
	//  from the beam source
	// -------------------------------------------------
	float	fBeamDist = (m_hSpotlightTarget->GetLocalOrigin() - GetLocalOrigin()).Length();

	float	fBeamTurnRate = atan(50 / fBeamDist);
	Vector  vNewAngVelocity = fBeamTurnRate * (vTargetDir - m_vSpotlightDir);

	float	myDecay = 0.4;
	m_vSpotlightAngVelocity = (myDecay * m_vSpotlightAngVelocity + (1 - myDecay) * vNewAngVelocity);

	// ------------------------------
	//  Limit overall angular speed
	// -----------------------------
	if (m_vSpotlightAngVelocity.Length() > 1)
	{

		Vector velDir = m_vSpotlightAngVelocity;
		VectorNormalize(velDir);
		m_vSpotlightAngVelocity = velDir * 1;
	}

	// ------------------------------
	//  Calculate new beam direction
	// ------------------------------
	m_vSpotlightDir = m_vSpotlightDir + m_vSpotlightAngVelocity;
	m_vSpotlightDir = m_vSpotlightDir;
	VectorNormalize(m_vSpotlightDir);


	// ---------------------------------------------
	//	Get beam end point.  Only collide with
	//  solid objects, not npcs
	// ---------------------------------------------
	trace_t tr;
	Vector vTraceEnd = GetAbsOrigin() + (m_vSpotlightDir * 2 * m_flSpotlightMaxLength);
	AI_TraceLine(GetAbsOrigin(), vTraceEnd, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	return (tr.endpos);
}

void CNPC_SynthScanner::SpotlightUpdate(void)
{
	if (m_bNoLight || HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH))
	{
		if (m_hSpotlight)
		{
			SpotlightDestroy();
		}

		return;
	}

	if ((m_nFlyMode != SCANNER_FLY_SPOT) &&
		(m_nFlyMode != SCANNER_FLY_PATROL) &&
		(m_nFlyMode != SCANNER_FLY_FAST)
		|| m_flSpotlightMaxLength <= 0)
	{
		if (m_hSpotlight)
		{
			SpotlightDestroy();
		}
		return;
	}

	// If I don't have a spotlight attempt to create one

	if (m_hSpotlight == NULL && m_flSpotlightMaxLength > 0)
	{
		SpotlightCreate();

		if (m_hSpotlight == NULL)
			return;
	}

	// Calculate the new homing target position
	m_vSpotlightCurrentPos = SpotlightCurrentPos();

	// ------------------------------------------------------------------
	//  If I'm not facing the spotlight turn it off 
	// ------------------------------------------------------------------
	Vector vSpotDir = m_vSpotlightCurrentPos - GetAbsOrigin();
	VectorNormalize(vSpotDir);

	Vector	vForward;
	AngleVectors(GetAbsAngles(), &vForward);

	float dotpr = DotProduct(vForward, vSpotDir);

	if (dotpr < 0.0)
	{
		// Leave spotlight off for a while
		m_fNextSpotlightTime = CURTIME + 3.0f;

		SpotlightDestroy();
		return;
	}

	// --------------------------------------------------------------
	//  Update spotlight target velocity
	// --------------------------------------------------------------
	Vector vTargetDir = (m_vSpotlightCurrentPos - m_hSpotlightTarget->GetLocalOrigin());
	float  vTargetDist = vTargetDir.Length();

	Vector vecNewVelocity = vTargetDir;
	VectorNormalize(vecNewVelocity);
	vecNewVelocity *= (10 * vTargetDist);

	// If a large move is requested, just jump to final spot as we
	// probably hit a discontinuity
	if (vecNewVelocity.Length() > 200)
	{
		VectorNormalize(vecNewVelocity);
		vecNewVelocity *= 200;
		m_hSpotlightTarget->SetLocalOrigin(m_vSpotlightCurrentPos);
	}
	m_hSpotlightTarget->SetAbsVelocity(vecNewVelocity);

	m_hSpotlightTarget->m_vSpotlightOrg = GetAbsOrigin();

	// Avoid sudden change in where beam fades out when cross disconinuities
	m_hSpotlightTarget->m_vSpotlightDir = m_hSpotlightTarget->GetLocalOrigin() - m_hSpotlightTarget->m_vSpotlightOrg;
	float flBeamLength = VectorNormalize(m_hSpotlightTarget->m_vSpotlightDir);
	m_flSpotlightCurLength = (0.80*m_flSpotlightCurLength) + (0.2*flBeamLength);

	// Fade out spotlight end if past max length.  
	if (m_flSpotlightCurLength > 2 * m_flSpotlightMaxLength)
	{
		m_hSpotlightTarget->SetRenderColorA(0);
		m_hSpotlight->SetFadeLength(m_flSpotlightMaxLength);
	}
	else if (m_flSpotlightCurLength > m_flSpotlightMaxLength)
	{
		m_hSpotlightTarget->SetRenderColorA((1 - ((m_flSpotlightCurLength - m_flSpotlightMaxLength) / m_flSpotlightMaxLength)));
		m_hSpotlight->SetFadeLength(m_flSpotlightMaxLength);
	}
	else
	{
		m_hSpotlightTarget->SetRenderColorA(1.0);
		m_hSpotlight->SetFadeLength(m_flSpotlightCurLength);
	}

	// Adjust end width to keep beam width constant
	float flNewWidth = SY_SPOTLIGHT_WIDTH * (flBeamLength / m_flSpotlightMaxLength);

	m_hSpotlight->SetWidth(flNewWidth);
	m_hSpotlight->SetEndWidth(flNewWidth);

	m_hSpotlightTarget->m_flLightScale = m_bNoDLight ? 0 : flNewWidth * 1.1;
}

void CNPC_SynthScanner::PlayFlySound(void)
{
	if (IsMarkedForDeletion())
		return;

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	//Setup the sound if we're not already
	if (m_pEngineSound == NULL)
	{
		// Create the sound
		CPASAttenuationFilter filter(this);

		m_pEngineSound = controller.SoundCreate(filter, entindex(), CHAN_STATIC, GetEngineSound(), ATTN_NORM);

		Assert(m_pEngineSound);

		// Start the engine sound
		controller.Play(m_pEngineSound, 0.0f, 100.0f);
		controller.SoundChangeVolume(m_pEngineSound, 1.0f, 2.0f);
	}

	float	speed = GetCurrentVelocity().Length();
	float	flVolume = HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH) ? 0.01f : 0.25f + (0.75f*(speed / GetMaxSpeed()));
	int		iPitch = MIN(255, 80 + (20 * (speed / GetMaxSpeed())));

	//Update our pitch and volume based on our speed
	controller.SoundChangePitch(m_pEngineSound, iPitch, 0.1f);
	controller.SoundChangeVolume(m_pEngineSound, flVolume, 0.1f);
}

void CNPC_SynthScanner::UpdateOnRemove(void)
{
	CollapseForceFieldShield(true);
	SpotlightDestroy();
	BaseClass::UpdateOnRemove();
}

void CNPC_SynthScanner::CreateForceFieldShield(float radius)
{
	CollapseForceFieldShield(true);

	// Create the visuals (non-solid half-sphere)
	// forward shield
	m_shield_ehandle = (CPropScalable*)CreateEntityByName("prop_scalable");
	if (m_shield_ehandle)
	{
		m_shield_ehandle->SetModel("models/_Monsters/Combine/synth_scanner_shield.mdl");
		m_shield_ehandle->SetAbsOrigin(GetAbsOrigin());
		m_shield_ehandle->SetAbsVelocity(GetAbsVelocity());
		m_shield_ehandle->SetLocalAngularVelocity(GetLocalAngularVelocity());
		m_shield_ehandle->SetParent(this);
		m_shield_ehandle->SetParentAttachment("SetParentAttachment", "shield", false);
		m_shield_ehandle->SetOwnerEntity(this);
		m_shield_ehandle->m_nSkin = 1;
		m_shield_ehandle->Spawn();
		m_shield_ehandle->SetSolid(SOLID_VPHYSICS);

		variant_t varX;
		varX.SetVector3D(Vector(radius, 0.75, 0));
		m_shield_ehandle->AcceptInput("SetScaleX", this, this, varX, 0);
		m_shield_ehandle->AcceptInput("SetScaleY", this, this, varX, 0);
		m_shield_ehandle->AcceptInput("SetScaleZ", this, this, varX, 0);
	}
	
	if (m_shield_ehandle)
	{
		EmitSound("NPC_SynthScanner.ShieldActivate"); 
		
		IPhysicsObject *pPhys = VPhysicsGetObject();
		Assert(pPhys);
		if( pPhys)
			PhysSetGameFlags(pPhys, FVPHYSICS_NO_PLAYER_PICKUP); // make it impossible to use the gravity gun against shielded girls.
		SetBloodColor(DONT_BLEED); // temporarily prevent blood impacts

		m_forcefieldshield_is_active_bool = true;
		m_forcefieldshield_creation_time_float = CURTIME;
		m_forcefieldshield_integrity_int = npc_synthscanner_shield_integrity.GetInt(); // MICROTEST ARENA CHANGE - make scanner shield depletable by firing
	}
}

void CNPC_SynthScanner::CollapseForceFieldShield(bool removeImmediately)
{
	Vector origin = GetAbsOrigin();

	if(m_shield_ehandle != NULL)
	{
		if (removeImmediately)
		{
			m_shield_ehandle->SetThink(&CPropScalable::SUB_Remove);
			m_shield_ehandle->SetNextThink(CURTIME);
		}
		else
		{
			variant_t var;
			var.SetVector3D(Vector(0.1, 0.5, 0));
			m_shield_ehandle->AcceptInput("SetScaleX", this, this, var, 0);
			m_shield_ehandle->AcceptInput("SetScaleY", this, this, var, 0);
			m_shield_ehandle->AcceptInput("SetScaleZ", this, this, var, 0);
			m_shield_ehandle->SetThink(&CPropScalable::SUB_Remove);
			m_shield_ehandle->SetNextThink(CURTIME + 1.0f);

			if (m_forcefieldshield_integrity_int <= 0) EmitSound("NPC_SynthScanner.ShieldBlowUp"); // if depleted by aggressive means, make a loud energy plop
		}
		IPhysicsObject *pPhys = VPhysicsGetObject();
		Assert(pPhys);
		PhysClearGameFlags(pPhys, FVPHYSICS_NO_PLAYER_PICKUP); // once they lose the shield, these girls can be grabbed and punted again.

		SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int); // restore our blood color for impact fx
		m_forcefieldshield_is_active_bool = false;
		m_forcefieldshield_integrity_int = 0;
	}
}

void CNPC_SynthScanner::InputCreateForceFieldShield(inputdata_t &data)
{
	if( data.value.Float() > 0 )
		CreateForceFieldShield(data.value.Float());
}

void CNPC_SynthScanner::InputCollapseForceFieldShield(inputdata_t &data)
{
	CollapseForceFieldShield(false);
}

void CNPC_SynthScanner::InputDisableSpotlight(inputdata_t &inputdata)
{
	m_bNoLight = true;
}

void CNPC_SynthScanner::InputInspectTargetSpotlight(inputdata_t &inputdata)
{
	InspectTarget(inputdata, SCANNER_FLY_SPOT);
}

void CNPC_SynthScanner::InputShouldInspect(inputdata_t &inputdata)
{
	m_bShouldInspect = (inputdata.value.Int() != 0);

	if (!m_bShouldInspect)
	{
		if (GetEnemy() == GetTarget())
			SetEnemy(NULL);
		ClearInspectTarget();
		SetTarget(NULL);
		SpotlightDestroy();
	}
}

void CNPC_SynthScanner::InputSetFollowTarget(inputdata_t &inputdata)
{
	InspectTarget(inputdata, SCANNER_FLY_FOLLOW);
}

void CNPC_SynthScanner::InputClearFollowTarget(inputdata_t &inputdata)
{
	SetInspectTargetToEnt(NULL, 0);

	m_nFlyMode = SCANNER_FLY_PATROL;
}

void CNPC_SynthScanner::InputEnterAmbush(inputdata_t &inputdata)
{
	if (!HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH))
	{
		AddSpawnFlags(SF_SYNTHSCANNER_AMBUSH);
		ClearSchedule("forced clear schedule");
		RemoveFlag(FL_FLY);
		SetGravity(UTIL_ScaleForGravity(1));
	}
}

void CNPC_SynthScanner::InputExitAmbush(inputdata_t &inputdata)
{
	if (HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH))
	{
		RemoveSpawnFlags(SF_SYNTHSCANNER_AMBUSH);
		ClearSchedule("forced clear schedule");
		AddFlag(FL_FLY);
		SetGravity(UTIL_ScaleForGravity(0.5));
	}
}

bool CNPC_SynthScanner::OverrideMove(float flInterval)
{
	if (HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH)) return true;
	// ----------------------------------------------
	//	If dive bombing
	// ----------------------------------------------
	if (m_nFlyMode == SCANNER_FLY_DIVE)
	{
		MoveToDivebomb(flInterval);
	}
	else
	{
		Vector vMoveTargetPos(0, 0, 0);
		CBaseEntity *pMoveTarget = NULL;

		if (!GetNavigator()->IsGoalActive() || (GetNavigator()->GetCurWaypointFlags() & bits_WP_TO_PATHCORNER))
		{
			// Select move target 
			if (GetTarget() != NULL)
			{
				pMoveTarget = GetTarget();
			}
			else if (GetEnemy() != NULL)
			{
				pMoveTarget = GetEnemy();
			}

			// Select move target position 
			if (HaveInspectTarget())
			{
				vMoveTargetPos = InspectTargetPosition();
			}
			else if (GetEnemy() != NULL)
			{
				vMoveTargetPos = GetEnemy()->GetAbsOrigin();
			}
		}
		else
		{
			vMoveTargetPos = GetNavigator()->GetCurWaypointPos();
		}

		ClearCondition(COND_SCANNER_FLY_CLEAR);
		ClearCondition(COND_SCANNER_FLY_BLOCKED);

		// See if we can fly there directly
		if (pMoveTarget || HaveInspectTarget())
		{
			trace_t tr;
			AI_TraceHull(GetAbsOrigin(), vMoveTargetPos, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

			float fTargetDist = (1.0f - tr.fraction)*(GetAbsOrigin() - vMoveTargetPos).Length();

			if ((tr.m_pEnt == pMoveTarget) || (fTargetDist < 50))
			{
				SetCondition(COND_SCANNER_FLY_CLEAR);
			}
			else
			{
				SetCondition(COND_SCANNER_FLY_BLOCKED);
			}
		}

		// If I have a route, keep it updated and move toward target
		if (GetNavigator()->IsGoalActive())
		{
			if (OverridePathMove(pMoveTarget, flInterval))
			{
				BlendPhyscannonLaunchSpeed();
				return true;
			}
		}
		else if (m_nFlyMode == SCANNER_FLY_SPOT)
		{
			MoveToSpotlight(flInterval);
		}
		else if (m_nFlyMode == SCANNER_FLY_FOLLOW)
		{
			MoveToSpotlight(flInterval);
		}
		// ----------------------------------------------
		//	If attacking
		// ----------------------------------------------
		else if (m_nFlyMode == SCANNER_FLY_ATTACK)
		{
			if (m_hSpotlight)
			{
				SpotlightDestroy();
			}

			MoveToAttack(flInterval);
		}
		else if (m_nFlyMode == SCANNER_FLY_FAST)
		{
			MoveToTarget(flInterval, GetNavigator()->GetGoalPos());
		}
		// -----------------------------------------------------------------
		// If I don't have a route, just decelerate
		// -----------------------------------------------------------------
		else if (!GetNavigator()->IsGoalActive())
		{
			float	myDecay = 9.5;
			Decelerate(flInterval, myDecay);
		}
	}

	MoveExecute_Alive(flInterval);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Accelerates toward a given position.
// Input  : flInterval - Time interval over which to move.
//			vecMoveTarget - Position to move toward.
//-----------------------------------------------------------------------------
void CNPC_SynthScanner::MoveToTarget(float flInterval, const Vector &vecMoveTarget)
{
	// Don't move if stalling
	if (m_flEngineStallTime > CURTIME)
		return;

	// Look at our inspection target if we have one
	if (GetEnemy() != NULL)
	{
		// Otherwise at our enemy
		TurnHeadToTarget(flInterval, GetEnemy()->EyePosition());
	}
	else if (HaveInspectTarget())
	{
		TurnHeadToTarget(flInterval, InspectTargetPosition());
	}
	else
	{
		// Otherwise face our motion direction
		TurnHeadToTarget(flInterval, vecMoveTarget);
	}

	// -------------------------------------
	// Move towards our target
	// -------------------------------------
	float myAccel;
	float myZAccel = 400.0f;
	float myDecay = 0.15f;

	Vector vecCurrentDir;

	// Get the relationship between my current velocity and the way I want to be going.
	vecCurrentDir = GetCurrentVelocity();
	VectorNormalize(vecCurrentDir);

	Vector targetDir = vecMoveTarget - GetAbsOrigin();
	float flDist = VectorNormalize(targetDir);

	float flDot;
	flDot = DotProduct(targetDir, vecCurrentDir);

	if (flDot > 0.25)
	{
		// If my target is in front of me, my flight model is a bit more accurate.
		myAccel = 300;
	}
	else
	{
		// Have a harder time correcting my course if I'm currently flying away from my target.
		myAccel = 150;
	}

	if (IsWastelandScanner()) myAccel *= 3.0f;

	if (myAccel > flDist / flInterval)
	{
		myAccel = flDist / flInterval;
	}

	if (myZAccel > flDist / flInterval)
	{
		myZAccel = flDist / flInterval;
	}

	MoveInDirection(flInterval, targetDir, myAccel, myZAccel, myDecay);

	// calc relative banking targets
	Vector forward, right, up;
	GetVectors(&forward, &right, &up);

	m_vCurrentBanking.x = targetDir.x;
	m_vCurrentBanking.z = 120.0f * DotProduct(right, targetDir);
	m_vCurrentBanking.y = 0;

	float speedPerc = SimpleSplineRemapVal(GetCurrentVelocity().Length(), 0.0f, GetMaxSpeed(), 0.0f, 1.0f);

	speedPerc = clamp(speedPerc, 0.0f, 1.0f);

	m_vCurrentBanking *= speedPerc;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flInterval - 
//-----------------------------------------------------------------------------
void CNPC_SynthScanner::MoveToSpotlight(float flInterval)
{
	if (flInterval <= 0)
		return;

	Vector vTargetPos;

	if (HaveInspectTarget())
	{
		vTargetPos = InspectTargetPosition();
	}
	else if (GetEnemy() != NULL)
	{
		vTargetPos = GetEnemyLKP();
	}
	else
	{
		return;
	}

	//float flDesiredDist = SCANNER_SPOTLIGHT_NEAR_DIST + ( ( SCANNER_SPOTLIGHT_FAR_DIST - SCANNER_SPOTLIGHT_NEAR_DIST ) / 2 );

	float flIdealHeightDiff = SY_SCANNER_SPOTLIGHT_NEAR_DIST;
	if (IsEnemyPlayerInSuit())
	{
		flIdealHeightDiff *= 0.5;
	}

	Vector idealPos = IdealGoalForMovement(vTargetPos, GetAbsOrigin(), GetGoalDistance(), flIdealHeightDiff);

	MoveToTarget(flInterval, idealPos);

	//TODO: Re-implement?

	/*
	// ------------------------------------------------
	//  Also keep my distance from other squad members
	//  unless I'm inspecting
	// ------------------------------------------------
	if (m_pSquad &&
	CURTIME > m_fInspectEndTime)
	{
	CBaseEntity*	pNearest	= m_pSquad->NearestSquadMember(this);
	if (pNearest)
	{
	Vector			vNearestDir = (pNearest->GetLocalOrigin() - GetLocalOrigin());
	if (vNearestDir.Length() < SCANNER_SQUAD_FLY_DIST)
	{
	vNearestDir		= pNearest->GetLocalOrigin() - GetLocalOrigin();
	VectorNormalize(vNearestDir);
	vFlyDirection  -= 0.5*vNearestDir;
	}
	}
	}

	// ---------------------------------------------------------
	//  Add evasion if I have taken damage recently
	// ---------------------------------------------------------
	if ((m_flLastDamageTime + SCANNER_EVADE_TIME) > CURTIME)
	{
	vFlyDirection = vFlyDirection + VelocityToEvade(GetEnemyCombatCharacterPointer());
	}
	*/
}

float CNPC_SynthScanner::GetGoalDistance(void)
{
	if (m_flGoalOverrideDistance != 0.0f)
		return m_flGoalOverrideDistance;

	switch (m_nFlyMode)
	{
	case SCANNER_FLY_ATTACK:
	{
		return 135.0f;
	}
	break;
	case SCANNER_FLY_CHASE:
	{
		return 128.0f;
	}
	break;
	case SCANNER_FLY_SPOT:
	{
		float goalDist = (SY_SCANNER_SPOTLIGHT_NEAR_DIST + ((SY_SCANNER_SPOTLIGHT_FAR_DIST - SY_SCANNER_SPOTLIGHT_NEAR_DIST)));
		/*
		if (GetEnemy()->IsPlayer())
		{
		goalDist *= 0.5;
		}
		*/
		return goalDist;
	}
	break;

	case SCANNER_FLY_FOLLOW:
	{
		return (80.0f);
	}
	break;
	}

	return BaseClass::GetGoalDistance();
}

void CNPC_SynthScanner::ClampMotorForces(Vector &linear, AngularImpulse &angular)
{
	float zmodule = npc_synthscanner_clamp_motor_forces_z_factor.GetFloat();
	linear.x = clamp(linear.x, -GetMaxSpeed(), GetMaxSpeed());
	linear.y = clamp(linear.y, -GetMaxSpeed(), GetMaxSpeed());
	linear.z = clamp(linear.z, -zmodule, zmodule);

	float zThreshold;
	float zConvar = npc_synthscanner_altitude_threshold.GetFloat();

	if (HasSpawnFlags(SF_SYNTHSCANNER_CALCULATE_Z_FROM_PLAYER_POS))
	{
		CBasePlayer *player = UTIL_GetLocalPlayer();

		if (player)
			zThreshold = player->WorldSpaceCenter().z + zConvar;
		else
			zThreshold = zConvar;
	}
	else
		zThreshold = zConvar;

	if (HasSpawnFlags(SF_SYNTHSCANNER_AMBUSH))
	{
		linear = vec3_origin;
		angular = vec3_origin;
	}
	else
	{
		linear.z += (GetFloorDistance(WorldSpaceCenter()) > zThreshold)
			? 32 : npc_synthscanner_z_acceleration_factor.GetFloat();
	}

	/*
	float zmodule = 200;
	linear.x = clamp(linear.x, -200, 200);
	linear.y = clamp(linear.y, -200, 200);
	linear.z = clamp(linear.z, -zmodule, zmodule);

	linear.z += (GetFloorDistance(WorldSpaceCenter()) > 320.0f)
	? 100 : 500;
	*/
}

AI_BEGIN_CUSTOM_NPC(npc_synth_scanner, CNPC_SynthScanner)
DECLARE_TASK(TASK_SYSCANNER_SET_FLY_SPOT)
DECLARE_TASK(TASK_SYSCANNER_SET_FLY_FAST)
DECLARE_TASK(TASK_SYSCANNER_ATTACK_PRE_FLASH)
DECLARE_TASK(TASK_SYSCANNER_ATTACK_GAS)
DECLARE_TASK(TASK_SYSCANNER_ATTACK_PRE_SHOCK)
DECLARE_TASK(TASK_SYSCANNER_ATTACK_SHOCK)
DECLARE_TASK(TASK_SYSCANNER_SPOT_INSPECT_ON)
DECLARE_TASK(TASK_SYSCANNER_SPOT_INSPECT_WAIT)
DECLARE_TASK(TASK_SYSCANNER_SPOT_INSPECT_OFF)
DECLARE_TASK(TASK_SYSCANNER_CLEAR_INSPECT_TARGET)
DECLARE_TASK(TASK_SYSCANNER_GET_PATH_TO_INSPECT_TARGET)

DECLARE_CONDITION(COND_SYSCANNER_HAVE_INSPECT_TARGET)
DECLARE_CONDITION(COND_SYSCANNER_INSPECT_DONE)
DECLARE_CONDITION(COND_SYSCANNER_SPOT_ON_TARGET)
DECLARE_CONDITION(COND_SYSCANNER_TOO_WEAK_TO_ATTACK)

DECLARE_ACTIVITY(ACT_WSCANNER_FLINCH_BACK)
DECLARE_ACTIVITY(ACT_WSCANNER_FLINCH_FRONT)
DECLARE_ACTIVITY(ACT_WSCANNER_FLINCH_LEFT)
DECLARE_ACTIVITY(ACT_WSCANNER_FLINCH_RIGHT)
DECLARE_ACTIVITY(ACT_SCANNER_INSPECT)

//=========================================================
// > SCHED_CSCANNER_PATROL
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SYSCANNER_PATROL,

	"	Tasks"
	"		TASK_SPEAK_SENTENCE					100"	// Curious sound
	"		TASK_SYSCANNER_CLEAR_INSPECT_TARGET	0"
	"		TASK_SCANNER_SET_FLY_PATROL			0"
	"		TASK_SET_TOLERANCE_DISTANCE			32"
	"		TASK_SET_ROUTE_SEARCH_TIME			5"	// Spend 5 seconds trying to build a path if stuck
	"		TASK_GET_PATH_TO_RANDOM_NODE		2000"
	"		TASK_RUN_PATH						0"
	"		TASK_WAIT_FOR_MOVEMENT				0"
	"		TASK_SPEAK_SENTENCE					101"	// Curious sound
	"		TASK_WAIT							6.0"
	"		TASK_WAIT_RANDOM					3.0"
	"	"
	"	Interrupts"
	"		COND_GIVE_WAY"
	"		COND_NEW_ENEMY"
	"		COND_SEE_ENEMY"
	"		COND_SEE_FEAR"
	"		COND_HEAR_COMBAT"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_PLAYER"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_PROVOKED"
	"		COND_SYSCANNER_TOO_WEAK_TO_ATTACK"
	"		COND_SCANNER_GRABBED_BY_PHYSCANNON"
)

//=========================================================
// > SCHED_CSCANNER_SPOTLIGHT_HOVER
//
// Hover above target entity, trying to get spotlight
// on my target
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SYSCANNER_SPOTLIGHT_HOVER,

	"	Tasks"
	"		TASK_SYSCANNER_SET_FLY_SPOT			0"
	"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE  "
	"		TASK_WAIT							0.5"
	"	"
	"	Interrupts"
	"		COND_SYSCANNER_TOO_WEAK_TO_ATTACK"
	"		COND_SYSCANNER_SPOT_ON_TARGET"
	"		COND_SYSCANNER_INSPECT_DONE"
	"		COND_SCANNER_FLY_BLOCKED"
	"		COND_NEW_ENEMY"
	"		COND_SCANNER_GRABBED_BY_PHYSCANNON"
)

//=========================================================
// > SCHED_CSCANNER_SPOTLIGHT_INSPECT_POS
//
// Inspect a position once spotlight is on it
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SYSCANNER_SPOTLIGHT_INSPECT_POS,

	"	Tasks"
	"		TASK_SYSCANNER_SET_FLY_SPOT			0"
	"		TASK_SET_ACTIVITY					ACTIVITY:ACT_SCANNER_INSPECT"
	"		TASK_SPEAK_SENTENCE					101"	// Curious sound
	"		TASK_WAIT							5"
	"		TASK_SYSCANNER_CLEAR_INSPECT_TARGET	0"
	"	"
	"	Interrupts"
	"		COND_SYSCANNER_TOO_WEAK_TO_ATTACK"
	"		COND_SYSCANNER_INSPECT_DONE"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_COMBAT"
	"		COND_NEW_ENEMY"
	"		COND_SCANNER_GRABBED_BY_PHYSCANNON"
)

//=========================================================
// > SCHED_CSCANNER_ATTACK_FLASH
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SYSCANNER_ATTACK_GAS,

	"	Tasks"
	"		TASK_SCANNER_SET_FLY_ATTACK			0"
	"		TASK_SET_ACTIVITY					ACTIVITY:ACT_MELEE_ATTACK1"
	"		TASK_SYSCANNER_ATTACK_PRE_FLASH		0"
	"		TASK_SYSCANNER_ATTACK_GAS			0"
	"		TASK_WAIT							0.5"
	"	"
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_SYSCANNER_TOO_WEAK_TO_ATTACK"
	"		COND_SCANNER_GRABBED_BY_PHYSCANNON"
)

//=========================================================
// > SCHED_CSCANNER_ATTACK_FLASH
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SYSCANNER_ATTACK_SHOCK,

	"	Tasks"
	"		TASK_SCANNER_SET_FLY_ATTACK			0"
	"		TASK_SET_ACTIVITY					ACTIVITY:ACT_MELEE_ATTACK1"
	"		TASK_SYSCANNER_ATTACK_PRE_SHOCK		0"
	"		TASK_SYSCANNER_ATTACK_SHOCK			0"
	"		TASK_WAIT							0.5"
	"	"
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_SYSCANNER_TOO_WEAK_TO_ATTACK"
	"		COND_SCANNER_GRABBED_BY_PHYSCANNON"
)

//=========================================================
// > SCHED_CSCANNER_MOVE_TO_INSPECT
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SYSCANNER_MOVE_TO_INSPECT,

	"	Tasks"
	"		TASK_SPEAK_SENTENCE							101"	// Curious sound
	"		 TASK_SET_FAIL_SCHEDULE						SCHEDULE:SCHED_SCANNER_PATROL"
	"		 TASK_SET_TOLERANCE_DISTANCE				128"
	"		 TASK_SYSCANNER_GET_PATH_TO_INSPECT_TARGET	0"
	"		 TASK_RUN_PATH								0"
	"		 TASK_WAIT_FOR_MOVEMENT						0"
	"	"
	"	Interrupts"
	"		COND_SYSCANNER_TOO_WEAK_TO_ATTACK"
	"		COND_SCANNER_FLY_CLEAR"
	"		COND_NEW_ENEMY"
	"		COND_SCANNER_GRABBED_BY_PHYSCANNON"
)

//=========================================================
// > SCHED_CSCANNER_MOVE_TO_INSPECT
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SYSCANNER_MOVE_TO_ATTACK,

	"	Tasks"
	"		TASK_SPEAK_SENTENCE							102"	// Curious sound
	"		 TASK_SET_FAIL_SCHEDULE						SCHEDULE:SCHED_SCANNER_PATROL"
	"		 TASK_SCANNER_SET_FLY_CHASE					0"
	"		 TASK_SET_TOLERANCE_DISTANCE				256"
	"		 TASK_GET_PATH_TO_ENEMY						0"
	"		 TASK_RUN_PATH								0"
	"		 TASK_WAIT_FOR_MOVEMENT						0"
	"	"
	"	Interrupts"
	"		COND_SYSCANNER_TOO_WEAK_TO_ATTACK"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_SCANNER_GRABBED_BY_PHYSCANNON"
)

//=========================================================
// > SCHED_SCANNER_MOVE_EVADE
//=========================================================

DEFINE_SCHEDULE
(
	SCHED_SYSCANNER_MOVE_ESCAPE,

	"	Tasks"
	"		TASK_SYSCANNER_SET_FLY_FAST			0"
	"		TASK_FIND_HINTNODE					0"
	"		TASK_GET_PATH_TO_HINTNODE			0"
	"		TASK_RUN_PATH						0"
	"		TASK_WAIT_FOR_MOVEMENT				0"
	"		TASK_WAIT							9999"
	"	"
	"	Interrupts"
	"		COND_SCANNER_GRABBED_BY_PHYSCANNON"
)

//=========================================================
// > SCHED_SCANNER_ATTACK_HOVER
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_SYSCANNER_ATTACK_HOVER,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE				SCHED_SYSCANNER_MOVE_ESCAPE"
	"		TASK_SCANNER_SET_FLY_ATTACK			0"
	"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"
	"		TASK_WAIT							0.5"
	""
	"	Interrupts"
	"		COND_SYSCANNER_TOO_WEAK_TO_ATTACK"
	"		COND_REPEATED_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_TOO_FAR_TO_ATTACK"
	"		COND_SCANNER_FLY_BLOCKED"
	"		COND_LOST_ENEMY"
	"		COND_NEW_ENEMY"
	"		COND_SCANNER_GRABBED_BY_PHYSCANNON"
)

AI_END_CUSTOM_NPC()

BEGIN_DATADESC(CNPC_SynthScanner)

DEFINE_SOUNDPATCH(m_pEngineSound),
DEFINE_EMBEDDED(m_Sentences),
DEFINE_EMBEDDED(m_KilledInfo),
DEFINE_FIELD(m_flGoalOverrideDistance, FIELD_FLOAT),
DEFINE_FIELD(m_pEyeFlash, FIELD_CLASSPTR),
DEFINE_FIELD(m_vInspectPos, FIELD_VECTOR),
DEFINE_FIELD(m_fInspectEndTime, FIELD_TIME),
DEFINE_FIELD(m_vSpotlightTargetPos, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_vSpotlightCurrentPos, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_vSpotlightDir, FIELD_VECTOR),
DEFINE_FIELD(m_vSpotlightAngVelocity, FIELD_VECTOR),
DEFINE_FIELD(m_flSpotlightCurLength, FIELD_FLOAT),
DEFINE_FIELD(m_fNextSpotlightTime, FIELD_TIME),
DEFINE_FIELD(m_nHaloSprite, FIELD_INTEGER),
DEFINE_FIELD(m_fNextFlySoundTime, FIELD_TIME),
DEFINE_FIELD(m_nFlyMode, FIELD_INTEGER),
DEFINE_FIELD(m_fCheckHintTime, FIELD_TIME),
DEFINE_FIELD(m_fCheckNPCTime, FIELD_TIME),
DEFINE_FIELD(m_forcefieldshield_is_active_bool, FIELD_BOOLEAN),
DEFINE_FIELD(m_forcefieldshield_creation_time_float, FIELD_FLOAT),
DEFINE_FIELD(m_forcefieldshield_cooldown_time_float, FIELD_FLOAT),
DEFINE_FIELD(m_forcefieldshield_integrity_int, FIELD_INTEGER),
DEFINE_FIELD(m_shield_ehandle, FIELD_EHANDLE),

DEFINE_FIELD(m_pSmokeTrail, FIELD_CLASSPTR),
DEFINE_FIELD(m_flFlyNoiseBase, FIELD_FLOAT),
DEFINE_FIELD(m_flEngineStallTime, FIELD_TIME),

DEFINE_FIELD(m_hGasCloud, FIELD_EHANDLE),

DEFINE_KEYFIELD(m_flSpotlightMaxLength, FIELD_FLOAT, "SpotlightLength"),
DEFINE_KEYFIELD(m_flSpotlightGoalWidth, FIELD_FLOAT, "SpotlightWidth"),

// Physics Influence
DEFINE_FIELD(m_hPhysicsAttacker, FIELD_EHANDLE),
DEFINE_FIELD(m_flLastPhysicsInfluenceTime, FIELD_TIME),

DEFINE_KEYFIELD(m_bNoLight, FIELD_BOOLEAN, "SpotlightDisabled"),
DEFINE_KEYFIELD(m_bNoDLight, FIELD_BOOLEAN, "SpotlightNoDLight"),

DEFINE_INPUTFUNC(FIELD_VOID, "DisableSpotlight", InputDisableSpotlight),
DEFINE_INPUTFUNC(FIELD_STRING, "InspectTargetSpotlight", InputInspectTargetSpotlight),
DEFINE_INPUTFUNC(FIELD_INTEGER, "InputShouldInspect", InputShouldInspect),
DEFINE_INPUTFUNC(FIELD_STRING, "SetFollowTarget", InputSetFollowTarget),
DEFINE_INPUTFUNC(FIELD_VOID, "ClearFollowTarget", InputClearFollowTarget),
DEFINE_INPUTFUNC(FIELD_VOID, "CollapseForceFieldShield", InputCollapseForceFieldShield),
DEFINE_INPUTFUNC(FIELD_FLOAT, "CreateForceFieledShield", InputCreateForceFieldShield),
DEFINE_INPUTFUNC(FIELD_VOID, "EnterAmbush", InputEnterAmbush),
DEFINE_INPUTFUNC(FIELD_VOID, "ExitAmbush", InputExitAmbush),

END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_synth_scanner, CNPC_SynthScanner);

IMPLEMENT_SERVERCLASS_ST(CNPC_SynthScanner, DT_NPC_SynthScanner)
END_SEND_TABLE()

#endif // SYNTHSCANNER

//====================================
// Bipedal Stinger (Dark Interval)
//====================================
#if defined (STINGER)
ConVar sk_stinger_melee_dmg("sk_stinger_melee_dmg", "20");

class CNPC_Stinger : public CAI_BaseActorNoBlend
{
	DECLARE_CLASS(CNPC_Stinger, CAI_BaseActorNoBlend);
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
public:
	CNPC_Stinger();
	~CNPC_Stinger();
	Class_T Classify(void) { return CLASS_HEADCRAB; }
	int ObjectCaps(void)
	{
		return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE;
	}

	void Precache(void);
	void Spawn(void);
	void Activate(void) { BaseClass::Activate(); }
	bool CreateBehaviors(void);
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);

	void Event_Killed(const CTakeDamageInfo &info);

	void IdleSound(void);
	void AlertSound(void);
	void DeathSound(const CTakeDamageInfo &info);
	void PainSound(const CTakeDamageInfo &info);
	void UseSound(void);

	void HandleAnimEvent(animevent_t *pEvent);
	bool IsValidEnemy(CBaseEntity *pEnemy);
	int  MeleeAttack1Conditions(float flDot, float flDist);
	int  RangeAttack1Conditions(float flDot, float flDist);

	void BuildScheduleTestBits(void);
	void ClearAttackConditions(void);
	void PrescheduleThink(void);
	void PostscheduleThink(void);
	int  SelectSchedule(void);
	int  SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode);
	void StartTask(const Task_t *pTask); 

	int	GetSoundInterests(void) {
		return (BaseClass::GetSoundInterests() | SOUND_DANGER | SOUND_BULLET_IMPACT | SOUND_PHYSICS_DANGER | SOUND_PLAYER);
	};

	bool AllowedToIgnite(void) { return true; }
		
private:
	int m_stinger_pp_blur_enabled_bool;
	CAI_ActBusyBehavior m_ActBusyBehavior;
	float m_flDangerSoundTime;
	void CleanupVenom(void);
	float m_next_use_sound_time;
	COutputEvent m_OnPlayerUse;
};

// for special behavior with vorts
static bool IsVortigaunt(CBaseEntity *pRoller)
{
	return FClassnameIs(pRoller, "npc_vortigaunt");
}

CNPC_Stinger::CNPC_Stinger()
{
	m_hNPCFileInfo = LookupNPCInfoScript("npc_stinger");
}

CNPC_Stinger::~CNPC_Stinger()
{
}

void CNPC_Stinger::Precache(void)
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	PrecacheScriptSound("NPC_Stinger.Alert");
	PrecacheScriptSound("NPC_Stinger.Idle");
	PrecacheScriptSound("NPC_Stinger.Pain");
	PrecacheScriptSound("NPC_Stinger.Death");
	PrecacheScriptSound("NPC_Stinger.AttackStab");
	PrecacheScriptSound("NPC_Stinger.AttackHit");
	PrecacheScriptSound("NPC_Stinger.AttackSpray");
	PrecacheScriptSound("NPC_Stinger.AnnounceMeleeAttack");
	PrecacheScriptSound("NPC_Stinger.AnnounceRangeAttack");
	PrecacheScriptSound("NPC_Stinger.AttackSprayLong");
	PrecacheScriptSound("NPC_Stinger.AttackSprayShort");
	PrecacheScriptSound("NPC_Stinger.FootstepRight");
	PrecacheScriptSound("NPC_Stinger.FootstepLeft");
	PrecacheScriptSound("NPC_Stinger.Use");
	PrecacheParticleSystem("stinger_spit_stream");
	PrecacheParticleSystem("stinger_spit_cloud");
	BaseClass::Precache();
}

void CNPC_Stinger::Spawn(void)
{
	Precache();
	BaseClass::Spawn();
	SetModel(STRING(GetModelName()));

	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MoveType_t(GetNPCScriptData().NPC_Movement_MoveType_int));
	SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
	SetHullSizeNormal();
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;

	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));
	m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);

//	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);

	SetNavType(NAV_GROUND);
	m_NPCState = NPC_STATE_NONE;
	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);
	SetDistLook(1024);

	m_flDangerSoundTime = CURTIME;
	m_flNextAttack = CURTIME;
	m_next_use_sound_time = CURTIME;

	NPCInit();

	SetUse(&CNPC_Stinger::Use);

	m_stinger_pp_blur_enabled_bool = 0;
}

bool CNPC_Stinger::CreateBehaviors(void)
{
	AddBehavior(&m_ActBusyBehavior);
	return BaseClass::CreateBehaviors();
}

void CNPC_Stinger::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	int nRelationship = IRelationType(pActivator);
	if(nRelationship == D_LI || nRelationship == D_NU)
		UseSound();
	m_OnPlayerUse.FireOutput(pActivator, pCaller);
}

void CNPC_Stinger::Event_Killed(const CTakeDamageInfo &info)
{
	CleanupVenom();
	BaseClass::Event_Killed(info);
}

void CNPC_Stinger::UseSound(void)
{
	if( CURTIME >= m_next_use_sound_time)
		EmitSound("NPC_Stinger.Use");
}

void CNPC_Stinger::IdleSound(void)
{
	EmitSound("NPC_Stinger.Idle");
}

void CNPC_Stinger::AlertSound(void)
{
	EmitSound("NPC_Stinger.Alert");
}

void CNPC_Stinger::PainSound(const CTakeDamageInfo &info)
{
	if (CURTIME <= m_flNextFlinchTime)
	{
		EmitSound("NPC_Stinger.Pain");
		m_flNextFlinchTime = CURTIME + RandomFloat(1.3, 2);
	}
}

void CNPC_Stinger::DeathSound(const CTakeDamageInfo &info)
{
	EmitSound("NPC_Stinger.Death");
}

void CNPC_Stinger::HandleAnimEvent(animevent_t *pEvent)
{
	if (pEvent->event == AE_NPC_LEFTFOOT || pEvent->event == AE_NPC_RIGHTFOOT)
	{
		Vector footPosition;
		CPASAttenuationFilter filter(this);

		if (pEvent->event == AE_NPC_LEFTFOOT)
		{
			GetAttachment("left_foot", footPosition);
			EmitSound(filter, entindex(), "NPC_Stinger.FootstepLeft", &footPosition, pEvent->eventtime);
		}
		else
		{
			GetAttachment("right_foot", footPosition);
			EmitSound(filter, entindex(), "NPC_Stinger.FootstepRight", &footPosition, pEvent->eventtime);
		}
		
		trace_t tr;
		AI_TraceLine(footPosition, footPosition - Vector(0, 0, 100), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);
		float yaw = random->RandomInt(0, 120);
		for (int i = 0; i < 3; i++)
		{
			Vector dir = UTIL_YawToVector(yaw + i * 120) * 10;
			VectorNormalize(dir);
			dir.z = 0.25;
			VectorNormalize(dir);
			g_pEffects->Dust(tr.endpos, dir, 5, 50);
		}
		return;
	}
	
	if (pEvent->event == AE_LAUNCH_STREAM )
	{		
		Vector needle;
		GetAttachment("launch_point", needle);

		Vector mouth;
		GetAttachment("melee_contact", mouth);

		Vector forward, up, right;
		GetVectors(&forward, &right, &up);
		QAngle angles;
		AngleVectors(angles, &forward);

		UTIL_BloodSpray(mouth, forward, BLOOD_COLOR_RED, 4, FX_BLOODSPRAY_ALL);
		DispatchParticleEffect("stinger_spit_stream", PATTACH_POINT_FOLLOW, this, "launch_point", false);
	//	UTIL_BloodStream(needle, forward * 50, BLOOD_COLOR_GREEN, 50);
		
		// scare away whatever we're attacking
		CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin() + forward * 64, 300, 30, this);

		return;
	}

	if (pEvent->event == AE_LAUNCH_PROJECTILE) // used for applying venom overlay 
	{
		Vector mouth;
		GetAttachment("melee_contact", mouth);
		
		// if we hit the player with our disgusting spray, spawn an obscuring blurry vision cloud around their head
		if (GetEnemy() && GetEnemy()->IsPlayer())
		{
			trace_t tr;
			Vector dir = GetEnemy()->EyePosition() - mouth;
			VectorNormalize(dir);
			AI_TraceHull(mouth, mouth + dir * 256, -Vector(4, 4, 4), Vector(4, 4, 4), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
			if (tr.m_pEnt && tr.m_pEnt == GetEnemy())
			{
				//	float m_flDuration = sk_stinger_venom_pp_duration.GetFloat();
				//	float m_flScale = sk_stinger_venom_pp_scale.GetFloat();
				EntityMessageBegin(this, true);
				WRITE_BYTE(0);
				//	WRITE_BYTE(m_flDuration);
				//	WRITE_BYTE(m_flScale);
				MessageEnd();
				SetWait(10);

				m_stinger_pp_blur_enabled_bool = 1;
			}
		}

		return;
	}

	if (pEvent->event == AE_CEASE_STREAM)
	{
		DispatchParticleEffect("null", PATTACH_ABSORIGIN, this, -1, true);
		m_flNextAttack = CURTIME + 1.5f;

		return;
	}

	if (pEvent->event == AE_MELEE_CONTACT)
	{
	//	EmitSound("NPC_Stinger.AttackStab");

		Vector forward;
		GetVectors(&forward, NULL, NULL);

		Vector meleecontact;
		GetAttachment("melee_contact", meleecontact);

		float flAdjustedDamage;

		bool playedImpactSnd = false;

		CBaseEntity *pEntity = NULL;

		while ((pEntity = gEntList.FindEntityInSphere(pEntity, meleecontact, 32.0f)) != NULL)
		{
			if (pEntity->m_takedamage != DAMAGE_NO && pEntity != this)
			{				
				flAdjustedDamage = sk_stinger_melee_dmg.GetFloat();

				CBasePlayer *pPlayer = ToBasePlayer(pEntity);

				if (pPlayer != NULL)
				{
					//Kick the player angles
					pPlayer->ViewPunch(QAngle(20, 20, -30));

					Vector	dir = pPlayer->WorldSpaceCenter() - WorldSpaceCenter();
					VectorNormalize(dir);
					dir.z = 0.0f;

					Vector vecNewVelocity = dir * 250.0f;
					vecNewVelocity[2] += 128.0f;
					pPlayer->SetAbsVelocity(vecNewVelocity);

					color32 red = { 128,0,0,128 };
					UTIL_ScreenFade(pPlayer, red, 1.0f, 0.1f, FFADE_IN);
				}
								
				else
				{
					flAdjustedDamage = sk_stinger_melee_dmg.GetFloat() * 5;
				}

				Vector attackDir = (pEntity->WorldSpaceCenter() - this->WorldSpaceCenter());
				VectorNormalize(attackDir);
				Vector offset = RandomVector(-32, 32) + pEntity->WorldSpaceCenter();

				// Generate enough force to make a 75kg guy move away at 350 in/sec
				Vector vecForce = attackDir * ImpulseScale(75, 350);

				// Deal the damage
				CTakeDamageInfo	info(this, this, vecForce, offset, flAdjustedDamage, DMG_CLUB);
				pEntity->TakeDamage(info);

				if (playedImpactSnd == false)
				{
					EmitSound("NPC_Stinger.AttackHit");
					playedImpactSnd = true;
				}
			}
		}

		return;
	}

	BaseClass::HandleAnimEvent(pEvent);
}

bool CNPC_Stinger::IsValidEnemy(CBaseEntity *pEnemy)
{
	if (IsVortigaunt(pEnemy)) // vorts and stingers have mutual neutrality
	{
		return false;
	}

	return BaseClass::IsValidEnemy(pEnemy);
}

int CNPC_Stinger::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (flDist > 128.0 )
		return COND_NONE;
	if (flDot < 0.5 )
		return COND_NOT_FACING_ATTACK;

	return COND_CAN_MELEE_ATTACK1;
}

int CNPC_Stinger::RangeAttack1Conditions(float flDot, float flDist)
{
	if (GetEnemy() == NULL)
		return COND_NONE;
	
	if (flDist < 100.0 )
		return COND_TOO_CLOSE_TO_ATTACK;

	if (flDist > 400.0)
		return COND_TOO_FAR_TO_ATTACK;

	if (flDot < 0.5)
		return COND_NOT_FACING_ATTACK;
		
	return COND_CAN_RANGE_ATTACK1;
}

void CNPC_Stinger::BuildScheduleTestBits(void)
{
	BaseClass::BuildScheduleTestBits();

	if (IsCurSchedule(SCHED_MELEE_ATTACK1))
	{
		// Ignore enemy lost, new enemy, damage etc
		ClearCustomInterruptConditions();
	}
}

void CNPC_Stinger::ClearAttackConditions(void)
{
	//	bool brangeattack = HasCondition(COND_CAN_RANGE_ATTACK1);

	BaseClass::ClearAttackConditions();

	//	if (brangeattack) SetCondition(COND_CAN_RANGE_ATTACK1);
}

void CNPC_Stinger::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();
	
	if( IsWaitFinished() && m_stinger_pp_blur_enabled_bool)
		CleanupVenom();

	if (m_NPCState == NPC_STATE_COMBAT)
	{
		// Stingers should make random alert sounds in combat
		if (random->RandomInt(0, 30) == 0)
		{
			AlertSound();
		}
	}
}

void CNPC_Stinger::PostscheduleThink(void)
{
	BaseClass::PostscheduleThink();
}

int CNPC_Stinger::SelectSchedule(void)
{
	if (BehaviorSelectSchedule())
	{
		return BaseClass::BehaviorSelectSchedule();
	}

	if (m_NPCState == NPC_STATE_IDLE || m_NPCState == NPC_STATE_ALERT)
	{
		// Make stingers flee from grenades and other danger sounds
		if (m_flDangerSoundTime <= CURTIME)
		{
			if (HasCondition(COND_HEAR_DANGER) || HasCondition(COND_HEAR_COMBAT))
			{
				m_flDangerSoundTime = CURTIME + 5.0f;
				return SCHED_TAKE_COVER_FROM_BEST_SOUND;
			}
		}
		return SCHED_IDLE_STAND;
	}

	if (m_NPCState == NPC_STATE_COMBAT)
	{
		if (CURTIME - m_last_attack_time < 5)
			return SCHED_RUN_RANDOM;

		return SCHED_TAKE_COVER_FROM_ENEMY;
	}

	return SCHED_IDLE_STAND;
}

int CNPC_Stinger::SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode)
{
	if (failedSchedule == SCHED_TAKE_COVER_FROM_ENEMY)
	{
		if (HasCondition(COND_CAN_RANGE_ATTACK1))
			return	SCHED_RANGE_ATTACK1;
		else if (HasCondition(COND_CAN_MELEE_ATTACK1))
			return	SCHED_MELEE_ATTACK1;
		else
			return SCHED_COMBAT_FACE;
	}

	return BaseClass::SelectFailSchedule(failedSchedule, failedTask, taskFailCode);
}

void CNPC_Stinger::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{

	case TASK_ANNOUNCE_ATTACK:
	{
		if (IsCurSchedule(SCHED_MELEE_ATTACK1))
		{
			EmitSound("NPC_Stinger.AnnounceMeleeAttack");
		}
		else if (IsCurSchedule(SCHED_RANGE_ATTACK1))
		{
			EmitSound("NPC_Antlion.AnnounceRangeAttack");
		}
		TaskComplete();
		break;
	}
	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

void CNPC_Stinger::CleanupVenom(void)
{
	if ( m_stinger_pp_blur_enabled_bool == 1)
	{
		EntityMessageBegin(this, true);
		WRITE_BYTE(1);
		WRITE_FLOAT(1); // (GetWaitFinishTime() - CURTIME);
		MessageEnd();
		m_stinger_pp_blur_enabled_bool = 0;
	}
}

BEGIN_DATADESC(CNPC_Stinger)
DEFINE_FIELD(m_stinger_pp_blur_enabled_bool, FIELD_INTEGER),
DEFINE_FIELD(m_flDangerSoundTime, FIELD_TIME),
DEFINE_FIELD(m_next_use_sound_time, FIELD_TIME),
DEFINE_OUTPUT(m_OnPlayerUse, "OnPlayerUse"),
DEFINE_USEFUNC(Use),
END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_stinger, CNPC_Stinger)

IMPLEMENT_SERVERCLASS_ST(CNPC_Stinger, DT_NPC_Stinger)
END_SEND_TABLE()

AI_BEGIN_CUSTOM_NPC(npc_stinger, CNPC_Stinger)
DECLARE_ANIMEVENT(AE_LAUNCH_STREAM)
DECLARE_ANIMEVENT(AE_CEASE_STREAM)
DECLARE_ANIMEVENT(AE_LAUNCH_PROJECTILE) // used for applying venom overlay 
DECLARE_ANIMEVENT(AE_MELEE_CONTACT)
AI_END_CUSTOM_NPC()

#endif // STINGER

//====================================
// Smoke Floater (Dark Interval)
//====================================
#if defined (FLOATER)
class CNPC_Floater : public CNPC_BaseScanner
{
	DECLARE_CLASS(CNPC_Floater, CNPC_BaseScanner);
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
public:
	virtual bool AllowedToIgnite(void) { return true; }
	virtual bool AllowedToIgniteByFlare(void) { return true; }

	CNPC_Floater()
	{
		m_hNPCFileInfo = LookupNPCInfoScript("npc_floater");
	}

	void Precache(void)
	{
		const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
		SetModelName(MAKE_STRING(pModelName));
		PrecacheModel(STRING(GetModelName()));
		PrecacheScriptSound("NPC_Protozoa.FlyLoop");
		PrecacheScriptSound("NPC_Floater.Scared");
		PrecacheScriptSound("NPC_Floater.Idle");
		PrecacheScriptSound("NPC_Floater.Pain");
		PrecacheScriptSound("NPC_Floater.Explode");
		BaseClass::Precache();
	}

	void Spawn(void)
	{
		Precache();
		BaseClass::Spawn();
		SetModel(STRING(GetModelName()));

		SetSolid(SOLID_BBOX);
		AddSolidFlags(FSOLID_NOT_STANDABLE);
		SetMoveType(MoveType_t(GetNPCScriptData().NPC_Movement_MoveType_int));

		SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
		SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

		SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
		m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;

	//	CapabilitiesClear();

		SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));

		m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);

		UTIL_ScaleForGravity(0.5);

		m_nFlyMode = SCANNER_FLY_PATROL;
		m_vCurrentBanking = vec3_origin;
		m_NPCState = NPC_STATE_IDLE;
		StopLoopingSounds();

		m_flNextIdleSoundTime = CURTIME;

		// Efficiency option
		GetSenses()->AddSensingFlags(SENSING_FLAGS_DONT_LISTEN /*| SENSING_FLAGS_DONT_LOOK*/);
		
		NPCInit();
	};

	void Activate()
	{
		BaseClass::Activate();
	}

	void NPCThink(void)
	{
		BaseClass::NPCThink();
		if (CURTIME < m_flNextIdleSoundTime)
		{
			MakeIdleSounds();
		}

	//	if (CURTIME > GetLastDamageTime() + 10)
	//	{
	//		if (GetEnemy()) SetEnemy(NULL);
	//	}

		SetNextThink(CURTIME + 0.1f);
	}

	void MakeIdleSounds(void)
	{
		if (GetEnemy() && FindEntityRelationship(GetEnemy())->disposition == D_FR)
		{
			EmitSound("NPC_Floater.Scared");
			m_flNextIdleSoundTime = CURTIME + random->RandomFloat(1.5f, 4.0f);
		}
		else
		{
			EmitSound("NPC_Floater.Idle");
			m_flNextIdleSoundTime = CURTIME + random->RandomFloat(2.0f, 6.0f);
		}
	}

	void PrescheduleThink(void)
	{
		BaseClass::PrescheduleThink();
	}

	int SelectSchedule(void)
	{
		// -------------------------------
		// If I'm in a script sequence
		// -------------------------------
		if (m_NPCState == NPC_STATE_SCRIPT)
			return(BaseClass::SelectSchedule());

		// -------------------------------
		// Flinch
		// -------------------------------
		if (HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_HEAVY_DAMAGE))
		{
			return SCHED_SMALL_FLINCH;
		}

		// I'm being held by the physcannon... struggle!
		if (IsHeldByPhyscannon())
			return SCHED_SMALL_FLINCH;
				
		return SCHED_SCANNER_PATROL;

		return BaseClass::SelectSchedule();
	}

	int OnTakeDamage_Alive(const CTakeDamageInfo &info)
	{
		CTakeDamageInfo newInfo = info;
		if (newInfo.GetDamageType() == DMG_CLUB)
		{
			newInfo.SetDamage(GetHealth());
		}
		if (newInfo.GetDamageType() == DMG_NERVEGAS || newInfo.GetDamageType() == DMG_ACID)
		{
			newInfo.SetDamage(0); // floater are immune to acid
		}
		if (newInfo.GetAttacker() != NULL)
		{
			SetEnemy(newInfo.GetAttacker());
			AddEntityRelationship(GetEnemy(), D_FR, 9999);

			// A hurt floater tells every nearby floater to stay away from the attacker.
			// Scan the area for any of my class and tell them so.
			CBaseEntity *nearbyFloater = NULL;
			while (NULL != (nearbyFloater = gEntList.FindEntityByClassnameWithin(nearbyFloater, "npc_floater", WorldSpaceCenter(), 512)))
			{
				nearbyFloater->MyNPCPointer()->SetEnemy(newInfo.GetAttacker());
				nearbyFloater->MyNPCPointer()->AddEntityRelationship(GetEnemy(), D_FR, 9999);
			}
		}

		return (BaseClass::OnTakeDamage_Alive(newInfo));
	}

	void Event_Killed(const CTakeDamageInfo &info)
	{
		ParticleSmokeGrenade *pGren = (ParticleSmokeGrenade*)CBaseEntity::Create(PARTICLESMOKEGRENADE_ENTITYNAME, GetAbsOrigin(), QAngle(0, 0, 0), NULL);
		if (pGren)
		{
			pGren->SetFadeTime(10, 14);
			pGren->SetAbsOrigin(WorldSpaceCenter());
			pGren->SetVolumeColour(Vector(3.8, 1.3, 1.0));
			pGren->SetVolumeSize(Vector(50, 30, 30));
			pGren->SetVolumeEmissionRate(0.3);
			pGren->SetExpandTime(1.2f);
			pGren->FillVolume();
		}

		EmitSound("NPC_Floater.Explode");

		// Replace baseclass's Event_Killed (baseclass is scanner, 
		// and we don't want its explosive Event_Killed), with manual
		// ragdoll creation.

		CleanupOnDeath(info.GetAttacker());

		SetCondition(COND_LIGHT_DAMAGE);
		SetIdealState(NPC_STATE_DEAD);

		Vector forceVector = CalcDamageForceVector(info);

		// DMG_REMOVENORAGDOLL is used for example
		// by the barnacles. We want it to be handled
		// in a special way, since barnacles will probably
		// eat floaters in the future.
		if ((info.GetDamageType() & DMG_REMOVENORAGDOLL) == 0)
		{
			if (HasSpawnFlags(SF_ALWAYS_SERVER_RAGDOLL))
			{
				CreateServerRagdoll(GetBaseAnimating(), m_nForceBone, info, COLLISION_GROUP_DEBRIS);
				UTIL_Remove(this);
			}
			else
			{
				BecomeRagdoll(info, forceVector);
			}
		}
		else
		{
			if (!IsEFlagSet(EFL_IS_BEING_LIFTED_BY_BARNACLE))
			{
				// Go away
				RemoveDeferred();
			}
		}
		
		// no longer standing on a nav area
		ClearLastKnownArea();
	}

	void PainSound(const CTakeDamageInfo &info)
	{
		EmitSound("NPC_Floater.Pain");
	}

private:
	bool	OverrideMove(float flInterval)
	{
		// ----------------------------------------------
		//	If dive bombing
		// ----------------------------------------------
		if (m_nFlyMode == SCANNER_FLY_DIVE)
		{
			MoveToDivebomb(flInterval);
		}
		else
		{
			Vector vMoveTargetPos(0, 0, 0);
			CBaseEntity *pMoveTarget = NULL;

			if (GetEnemy() != NULL && FindEntityRelationship(GetEnemy())->disposition == D_FR)
			{
				Vector coverFromEnemy;
				Vector enemyDir;
				enemyDir = WorldSpaceCenter() - GetEnemy()->WorldSpaceCenter();
				VectorNormalize(enemyDir);
				if (!FindCoverPos(GetEnemy(), &coverFromEnemy))
					vMoveTargetPos = WorldSpaceCenter() + enemyDir * 512;
				else
					vMoveTargetPos = coverFromEnemy;
				vMoveTargetPos.z = WorldSpaceCenter().z + 64; //hacky number
			//	NDebugOverlay::Cross3D(vMoveTargetPos, 4, 100, 170, 25, true, 0.1f);
				
				GetMotor()->SetIdealYawAndUpdate(enemyDir);
				MoveToTarget(flInterval, vMoveTargetPos);
				return true;
			}
			else
			{

				if (!GetNavigator()->IsGoalActive() || (GetNavigator()->GetCurWaypointFlags() & bits_WP_TO_PATHCORNER))
				{
					// Select move target 
					if (GetTarget() != NULL)
					{
						pMoveTarget = GetTarget();
					}
					//	else if (GetEnemy() != NULL)
					//	{
					//		pMoveTarget = GetEnemy();
					//	}
				}
				else
				{
					vMoveTargetPos = GetNavigator()->GetCurWaypointPos();
				}
			}

			ClearCondition(COND_SCANNER_FLY_CLEAR);
			ClearCondition(COND_SCANNER_FLY_BLOCKED);

			// See if we can fly there directly
			if (pMoveTarget )
			{
				trace_t tr;
				AI_TraceHull(GetAbsOrigin(), vMoveTargetPos, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

				float fTargetDist = (1.0f - tr.fraction)*(GetAbsOrigin() - vMoveTargetPos).Length();

				if ((tr.m_pEnt == pMoveTarget) || (fTargetDist < 50))
				{
					SetCondition(COND_SCANNER_FLY_CLEAR);
				}
				else
				{
					SetCondition(COND_SCANNER_FLY_BLOCKED);
				}
			}

			// If I have a route, keep it updated and move toward target
			if (GetNavigator()->IsGoalActive())
			{
				if (OverridePathMove(pMoveTarget, flInterval))
				{
					BlendPhyscannonLaunchSpeed();
					return true;
				}
			}

			// ----------------------------------------------
			//	If attacking
			// ----------------------------------------------
			else if (m_nFlyMode == SCANNER_FLY_ATTACK)
			{
				MoveToAttack(flInterval);
			}
			else if (m_nFlyMode == SCANNER_FLY_FAST)
			{
				MoveToTarget(flInterval, GetNavigator()->GetGoalPos());
			}
			// -----------------------------------------------------------------
			// If I don't have a route, just decelerate
			// -----------------------------------------------------------------
			else if (!GetNavigator()->IsGoalActive())
			{
				float	myDecay = 9.5;
				Decelerate(flInterval, myDecay);
			}
		}

		MoveExecute_Alive(flInterval);

		return true;
	}

	void	MoveExecute_Alive(float flInterval)
	{
		if (m_lifeState == LIFE_DYING)
		{
			SetCurrentVelocity(vec3_origin);
			StopLoopingSounds();
		}

		else
		{
			// Amount of noise to add to flying
			float noiseScale = 1.5f;

			SetCurrentVelocity(GetCurrentVelocity() + VelocityToAvoidObstacles(flInterval));

			IPhysicsObject *pPhysics = VPhysicsGetObject();

			if (pPhysics && pPhysics->IsAsleep())
			{
				pPhysics->Wake();
			}

			// Add time-coherent noise to the current velocity so that it never looks bolted in place.
			AddNoiseToVelocity(noiseScale);

			AdjustScannerVelocity();

			float maxSpeed = (GetEnemy() && FindEntityRelationship(GetEnemy())->disposition == D_FR) ? (GetMaxSpeed() * 2.0f) : GetMaxSpeed();

			// Limit fall speed
			LimitSpeed(maxSpeed);

			// Blend out desired velocity when launched by the physcannon
			BlendPhyscannonLaunchSpeed();

			//	PlayFlySound();
		}
	};

	void			ClampMotorForces(Vector &linear, AngularImpulse &angular)
	{
		float zmodule = 200;
		if (GetHealth() < GetMaxHealth() / 2)
			zmodule = 150;
		linear.x = clamp(linear.x, -500, 500);
		linear.y = clamp(linear.y, -500, 500);
		linear.z = clamp(linear.z, -zmodule, zmodule);

		linear.z += (GetFloorDistance(WorldSpaceCenter()) > MinGroundDist())
			? 0 : 500;
	}

	void	StopLoopingSounds(void)
	{
		BaseClass::StopLoopingSounds();
	}

public:
	Class_T Classify() { return CLASS_NONE; }

	float			GetMaxSpeed(void) { return 100.0f; }
	int				GetSoundInterests(void) { return (SOUND_NONE); }
	char			*GetEngineSound(void) { return "NPC_Protozoa.FlyLoop"; }
	float			MinGroundDist(void) 
	{ 
		float h = GetHealth();
		float m = GetMaxHealth();
		float r = 96.0f;
		if (h > m * 0.75)
			r = 96.0f;
		else if (h > m * 0.25)
			r = 64.0f;
		else if (h <= m * 0.25)
			r = 45.0f;

		return r;
	}
	float			GetGoalDistance(void) { return 36.0f; }
private:

	float			m_flNextIdleSoundTime;
};

int	g_interactionFloaterLatchOn = 0;
int	g_interactionFloaterStartSuck = 0;
int	g_interactionFloaterFinishSuck = 0;

BEGIN_DATADESC(CNPC_Floater)
DEFINE_FIELD(m_flNextIdleSoundTime, FIELD_TIME),
END_DATADESC()
LINK_ENTITY_TO_CLASS(npc_floater, CNPC_Floater);

AI_BEGIN_CUSTOM_NPC(npc_floater, CNPC_Floater)

// Custom interaction declares are needed to direct DispatchInteraction() calls.
// In case of floater, it'll float towards its prey,
// then perform melee attack anim. On animevent cue, if prey
// is in range, g_interactionFloatLatchOn is called. It fixates
// the prey, while the floater begings its looping sucking anim (g_interactionFloaterStartSuck).
// Once it's done sucking out blood or plasma, it lets go (g_interactionFloaterFinishSuck).
// It lets go of the prey, and deals damage to it
// (it's easier to handle damage just on end of cycle, even though we could 
// do it during the sucking, damaging prey bit by bit... I prefer one damage call though.
// After that the floater should ignore that particular prey for a while.

// On prey side, it should stop moving on LatchOn, it starts custom context think
// on StartSuck, and on FinishSuck it becomes mobile again and gets scared by AI sound.
DECLARE_INTERACTION(g_interactionFloaterLatchOn)
DECLARE_INTERACTION(g_interactionFloaterStartSuck)
DECLARE_INTERACTION(g_interactionFloaterFinishSuck)

AI_END_CUSTOM_NPC()
#endif // FLOATER

//====================================
// Mobile Combine Mine (Dark Interval)
//====================================
#if defined(MOBILEMINE)
class CNPC_MobileMine : public CAI_BaseNPC
{
	DECLARE_CLASS(CNPC_MobileMine, CAI_BaseNPC);
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
public:
	CNPC_MobileMine();

	Class_T Classify(void);
	void	Precache(void);
	void	Spawn(void);
	void	MineTouch(CBaseEntity *pOther);
	void	Activate();	
	void	NPCThink();
	int		GetSoundInterests(void) { return SOUND_DANGER | SOUND_COMBAT | SOUND_PHYSICS_DANGER; };
	
	void	AlertSound(void);
	void	DeathSound(const CTakeDamageInfo &info);
	void	PainSound(const CTakeDamageInfo &info);
	void	IdleSound(void);

	void	Event_Killed(const CTakeDamageInfo &info);
	bool	ShouldGib(const CTakeDamageInfo &info) { return false; };
	void    HandleAnimEvent(animevent_t *pEvent);
	int		MeleeAttack1Conditions(float flDot, float flDist);
	int		SelectSchedule(void);

	void	SteerArrive(Vector &Steer, const Vector &Target);
	void	SteerSeek(Vector &Steer, const Vector &Target);
	void	ClampSteer(Vector &SteerAbs, Vector &SteerRel, Vector &forward, Vector &right, Vector &up);
	bool	SteerAvoidObstacles(Vector &Steer, const Vector &Velocity, const Vector &Forward, const Vector &Right, const Vector &Up);
	bool	OverrideMove(float flInterval);
	void	DoMovement(float flInterval, const Vector &MoveTarget, int eMoveType);

	static const Vector	m_vecAccelerationMax;
	static const Vector	m_vecAccelerationMin;

	int		m_memorytemp;
};

//Acceleration definitions
const Vector CNPC_MobileMine::m_vecAccelerationMax = Vector(128, 256, 128);
const Vector CNPC_MobileMine::m_vecAccelerationMin = Vector(-128, -256, -128);

Class_T	CNPC_MobileMine::Classify(void)
{
	return CLASS_COMBINE; // currently, don't react to anything. 
}

CNPC_MobileMine::CNPC_MobileMine(void)
{	
	m_memorytemp = 0;
	m_hNPCFileInfo = LookupNPCInfoScript("npc_mobile_mine");
}

void CNPC_MobileMine::Precache()
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	PrecacheScriptSound("NPC_Hopper.Idle");
	PrecacheScriptSound("NPC_Hopper.Squeak");
	PrecacheScriptSound("NPC_Hopper.Die_Pop");
	PrecacheScriptSound("NPC_Hopper.Footstep_Walk");
	PrecacheScriptSound("NPC_Hopper.Footstep_Jump");
	PrecacheParticleSystem("antlion_gib_01");
	BaseClass::Precache();
}

void CNPC_MobileMine::Spawn()
{
	Precache();
	SetModel(STRING(GetModelName()));

	BaseClass::Spawn();
	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

	SetSolid(SOLID_BBOX);
	SetSolidFlags(FSOLID_TRIGGER); // this NPC serves as a trigger - makes reactions possible
	SetTouch(&CNPC_MobileMine::MineTouch);
	// the size of the trigger. When the player touches this virtual box, stuff happens.
	UTIL_SetSize(this, Vector(-64, -64, 0), Vector(64, 64, 8));

	SetMoveType(MoveType_t(GetNPCScriptData().NPC_Movement_MoveType_int));
	AddFlag( FL_STEPMOVEMENT);
	m_flGroundSpeed = 150;

	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_Climb_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_CLIMB);
	if ((GetNPCScriptData().NPC_Capable_Doors_boolean) == true) CapabilitiesAdd(bits_CAP_DOORS_GROUP);

	//	m_bShouldPatrol = GetNPCScriptData().bShouldPatrolAfterSpawn;

	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));

	m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);

	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;
	SetDistLook(512);
	m_NPCState = NPC_STATE_NONE;
	m_flNextAttack = CURTIME;

	SetCollisionGroup(HL2COLLISION_GROUP_HEADCRAB);

	NPCInit();
}

void CNPC_MobileMine::Activate()
{
	BaseClass::Activate();
}

void CNPC_MobileMine::NPCThink()
{
	BaseClass::NPCThink();

	StudioFrameAdvance();

	bool InPVS = ((UTIL_FindClientInPVS(edict()) != NULL)
		|| (UTIL_ClientPVSIsExpanded()
			&& UTIL_FindClientInVisibilityPVS(edict())));

	// See how close the player is
	CBasePlayer *pPlayerEnt = AI_GetSinglePlayer();
	float flDistToPlayerSqr = (GetAbsOrigin() - pPlayerEnt->GetAbsOrigin()).LengthSqr();

	// If they're far enough away, just wait to think again
	if (flDistToPlayerSqr > Square(40 * 12) || !InPVS)
	{
		SetNextThink(CURTIME + 0.3f);
		return;
	}

	// scan for enemies in our detection radius and explode if something triggered us
	CBaseEntity *pTouch = NULL;

	while ((pTouch = gEntList.FindEntityInSphere(pTouch, GetAbsOrigin(), 64.0f)) != NULL)
	{
		if (GetEnemy() && pTouch == GetEnemy())
		{
			CTakeDamageInfo killMyself;
			killMyself.SetDamage(GetHealth());
			killMyself.SetDamageType(DMG_CRUSH | DMG_REMOVENORAGDOLL);
			killMyself.SetAttacker(GetWorldEntity());
			killMyself.SetInflictor(GetWorldEntity());
			TakeDamage(killMyself);
		}
	}

	SetNextThink(CURTIME + 0.1f);
}

void CNPC_MobileMine::MineTouch(CBaseEntity *pOther)
{
	if (GetEnemy() && pOther == GetEnemy()) // only react to the player
	{
		CTakeDamageInfo killMyself;
		killMyself.SetDamage(GetHealth());
		killMyself.SetDamageType(DMG_CRUSH | DMG_REMOVENORAGDOLL);
		killMyself.SetAttacker(GetWorldEntity());
		killMyself.SetInflictor(GetWorldEntity());
		TakeDamage(killMyself);
	}
}

void CNPC_MobileMine::IdleSound(void)
{
	EmitSound("NPC_Hopper.Idle");
}

void CNPC_MobileMine::AlertSound(void)
{
	EmitSound("NPC_Hopper.Squeak");
}

void CNPC_MobileMine::PainSound(const CTakeDamageInfo &info)
{
	EmitSound("NPC_Hopper.Squeak");
}

void CNPC_MobileMine::DeathSound(const CTakeDamageInfo &info)
{
	EmitSound("NPC_Hopper.Die_Pop");
}

void CNPC_MobileMine::Event_Killed(const CTakeDamageInfo &info)
{
	m_takedamage = DAMAGE_NO;

	CEffectData	data;
	data.m_nEntIndex = entindex();
	data.m_vOrigin = GetAbsOrigin();
	data.m_vAngles = GetAbsAngles();
	data.m_flScale = 512;
	DispatchEffect("ThumperDust", data);

	ExplosionCreate(GetAbsOrigin() + Vector(0, 0, 16), GetAbsAngles(), this, 50.0f, 150.0f,
		SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSPARKS, 1300.0f);

	// Dust explosion
//	AR2Explosion *pExplosion = AR2Explosion::CreateAR2Explosion(GetAbsOrigin() + Vector(0, 0, 16));

//	if (pExplosion)
	{
//		pExplosion->SetLifetime(10);
	}

	AddEffects(EF_NODRAW);
	AddSolidFlags(FSOLID_NOT_SOLID);

	SetThink(&CNPC_MobileMine::SUB_Remove);
	SetNextThink(CURTIME + 1.0f);
}

void CNPC_MobileMine::HandleAnimEvent(animevent_t *pEvent)
{
	BaseClass::HandleAnimEvent(pEvent);
}

int CNPC_MobileMine::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (CURTIME < m_flNextAttack)
		return 0;

	if ((GetFlags() & FL_ONGROUND) == false)
		return 0;
	
	if (flDist > 256.0f)
		return COND_TOO_FAR_TO_ATTACK;

	return COND_CAN_RANGE_ATTACK1;
}

// Just blank.
int CNPC_MobileMine::SelectSchedule(void)
{
	if (m_NPCState == NPC_STATE_IDLE)
	{
		return SCHED_PATROL_WALK;
	}
	else if (m_NPCState == NPC_STATE_COMBAT)
	{
		if (HasCondition(COND_CAN_RANGE_ATTACK1))
		{
			return SCHED_MELEE_ATTACK1;
			m_flNextAttack = CURTIME + 3.0f;
		}
		else
		{
			return SCHED_PATROL_WALK;
		}
	}

	return BaseClass::SelectSchedule();
}

bool CNPC_MobileMine::OverrideMove(float flInterval)
{
	m_flGroundSpeed = 150;

	if (GetEnemy())
	{
		m_flGroundSpeed = 300;
		DoMovement(flInterval, GetEnemy()->GetAbsOrigin(), MOVETYPE_STEP);
	}
	else
	{
		m_flGroundSpeed = 150;
		DoMovement(flInterval, GetLocalOrigin(), MOVETYPE_STEP);
	}
	return false;
}

void CNPC_MobileMine::DoMovement(float flInterval, const Vector &MoveTarget, int eMoveType)
{
	Vector Steer, SteerAvoid, SteerRel;
	Vector forward, right, up;

	//Get our orientation vectors.
	GetVectors(&forward, &right, &up);

	if ((GetEnemy() != NULL) /*&& (GetCurrentActivity() == ACT_MELEE_ATTACK1)*/)
	{
		SteerSeek(Steer, GetEnemy()->GetAbsOrigin() + (GetEnemy()->GetSmoothedVelocity() * 0.5f));
	}
	else
	{
		SteerSeek(Steer, MoveTarget);
	}

	//See if we need to avoid any obstacles.
	if (SteerAvoidObstacles(SteerAvoid, GetAbsVelocity(), forward, right, up))
	{
		//Take the avoidance vector
	//	Steer = SteerAvoid;
	}

	//Clamp our ideal steering vector to within our physical limitations.
	ClampSteer(Steer, SteerRel, forward, right, up);

	ApplyAbsVelocityImpulse(Steer * flInterval);
	
	Vector vecNewVelocity;
	vecNewVelocity = GetAbsVelocity();

	float flLength = vecNewVelocity.Length();

	//Clamp our final speed
	if (flLength > m_flGroundSpeed)
	{
		vecNewVelocity *= (m_flGroundSpeed / flLength);
		flLength = m_flGroundSpeed;
	}

	//Face our current velocity
	GetMotor()->SetIdealYawAndUpdate(UTIL_AngleMod(CalcIdealYaw(GetAbsOrigin() + GetAbsVelocity())), AI_KEEP_YAW_SPEED);
		
	//Move along the current velocity vector
	if (WalkMove(vecNewVelocity * flInterval, MASK_NPCSOLID) == false)
	{
		//Attempt a half-step
		if (WalkMove((vecNewVelocity*0.5f) * flInterval, MASK_NPCSOLID) == false)
		{
			//Restart the velocity
			VectorNormalize(vecNewVelocity);
			vecNewVelocity *= 0.5f;
		}
		else
		{
			//Cut our velocity in half
			vecNewVelocity *= 0.5f;
		}
	}

	SetAbsVelocity(vecNewVelocity);
}

void CNPC_MobileMine::SteerArrive(Vector &Steer, const Vector &Target)
{
	Vector Offset = Target - GetAbsOrigin();
	float fTargetDistance = Offset.Length();

	float fIdealSpeed = m_flGroundSpeed * (fTargetDistance / 64);
	float fClippedSpeed = MIN(fIdealSpeed, m_flGroundSpeed);

	Vector DesiredVelocity(0, 0, 0);

	if (fTargetDistance > 1)
	{
		DesiredVelocity = (fClippedSpeed / fTargetDistance) * Offset;
	}

	Steer = DesiredVelocity - GetAbsVelocity();
}

void CNPC_MobileMine::SteerSeek(Vector &Steer, const Vector &Target)
{
	Vector offset = Target - GetAbsOrigin();

	VectorNormalize(offset);

	Vector DesiredVelocity = m_flGroundSpeed * offset;

	Steer = DesiredVelocity - GetAbsVelocity();
}

bool CNPC_MobileMine::SteerAvoidObstacles(Vector &Steer, const Vector &Velocity, const Vector &Forward, const Vector &Right, const Vector &Up)
{
	trace_t	tr;

	bool	collided = false;
	Vector	dir = Velocity;
	float	speed = VectorNormalize(dir);

	//Look ahead one second and avoid whatever is in our way.
	AI_TraceHull(GetAbsOrigin(), GetAbsOrigin() + (dir*speed), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

	Vector	forward;

	GetVectors(&forward, NULL, NULL);

	//If we're hitting our enemy, just continue on
	if ((GetEnemy() != NULL) && (tr.m_pEnt == GetEnemy()))
		return false;

	if (tr.fraction < 1.0f)
	{
		CBaseEntity *pBlocker = tr.m_pEnt;

		if ((pBlocker != NULL) && (pBlocker->MyNPCPointer() != NULL))
		{
			Vector HitOffset = tr.endpos - GetAbsOrigin();

			Vector SteerUp = CrossProduct(HitOffset, Velocity);
			Steer = CrossProduct(SteerUp, Velocity);
			VectorNormalize(Steer);

			if (tr.fraction > 0)
			{
				Steer = (Steer * Velocity.Length()) / tr.fraction;
			}
			else
			{
				Steer = (Steer * 1000 * Velocity.Length());
			}
		}
		else
		{
			if ((pBlocker != NULL) && (pBlocker == GetEnemy()))
			{
				return false;
			}

			Vector	steeringVector = tr.plane.normal;

			if (tr.fraction == 0.0f)
				return false;

			Steer = steeringVector * (Velocity.Length() / tr.fraction);

		}

		//return true;
		collided = true;
	}

	//Try to remain 8 feet above the ground.
	AI_TraceLine(GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, -4), MASK_NPCSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction < 1.0f)
	{
		Steer += Vector(0, 0, m_vecAccelerationMax.z / tr.fraction);
		collided = true;
	}
	
	return collided;
}

void CNPC_MobileMine::ClampSteer(Vector &SteerAbs, Vector &SteerRel, Vector &forward, Vector &right, Vector &up)
{
	float fForwardSteer = DotProduct(SteerAbs, forward);
	float fRightSteer = DotProduct(SteerAbs, right);
	float fUpSteer = DotProduct(SteerAbs, up);

	if (fForwardSteer > 0)
	{
		fForwardSteer = MIN(fForwardSteer, m_vecAccelerationMax.x);
	}
	else
	{
		fForwardSteer = MAX(fForwardSteer, m_vecAccelerationMin.x);
	}

	if (fRightSteer > 0)
	{
		fRightSteer = MIN(fRightSteer, m_vecAccelerationMax.y);
	}
	else
	{
		fRightSteer = MAX(fRightSteer, m_vecAccelerationMin.y);
	}

	if (fUpSteer > 0)
	{
		fUpSteer = MIN(fUpSteer, m_vecAccelerationMax.z);
	}
	else
	{
		fUpSteer = MAX(fUpSteer, m_vecAccelerationMin.z);
	}

	SteerAbs = (fForwardSteer*forward) + (fRightSteer*right) + (fUpSteer*up);

	SteerRel.x = fForwardSteer;
	SteerRel.y = fRightSteer;
	SteerRel.z = fUpSteer;
}

BEGIN_DATADESC(CNPC_MobileMine)
DEFINE_FIELD(m_memorytemp, FIELD_EHANDLE),
DEFINE_ENTITYFUNC(MineTouch),
END_DATADESC()
LINK_ENTITY_TO_CLASS(npc_mobile_mine, CNPC_MobileMine);
LINK_ENTITY_TO_CLASS(npc_mine, CNPC_MobileMine);

AI_BEGIN_CUSTOM_NPC(npc_mobile_mine, CNPC_MobileMine)
DECLARE_USES_SCHEDULE_PROVIDER(CAI_BaseNPC)
AI_END_CUSTOM_NPC()
#endif // MOBILEMINE

#if defined BEETURRET
class CNPC_XenBee : public CAI_BaseNPC
{
	DECLARE_CLASS(CNPC_XenBee, CAI_BaseNPC);
public:

	void Spawn(void);
	void Precache(void);
	Class_T	 Classify(void);
	Disposition_t IRelationType(CBaseEntity *pTarget);
	bool IsValidEnemy(CBaseEntity *pEnemy);

	void StingTouch(CBaseEntity *pOther);
	void DartTouch(CBaseEntity *pOther);
	void TrackTouch(CBaseEntity *pOther);
	void TrackTarget(void);
	void StartDart(void);
	void IgniteTrail(void);
	void StartTrack(void);
	
	virtual unsigned int PhysicsSolidMaskForEntity(void) const;
	virtual bool ShouldGib(const CTakeDamageInfo &info) { return false; }
	
	float			m_flStopAttack;
	float			m_flFlySpeed;
	int				m_flDamage;

	DECLARE_DATADESC();

	Vector			m_vecEnemyLKP;
};

int iHornetPuff;

LINK_ENTITY_TO_CLASS(xen_bee, CNPC_XenBee);

BEGIN_DATADESC(CNPC_XenBee)
DEFINE_FIELD(m_flStopAttack, FIELD_TIME),
DEFINE_FIELD(m_flFlySpeed, FIELD_FLOAT),
DEFINE_FIELD(m_flDamage, FIELD_INTEGER),
DEFINE_FIELD(m_vecEnemyLKP, FIELD_POSITION_VECTOR),

DEFINE_ENTITYFUNC(StingTouch),
DEFINE_THINKFUNC(StartDart),
DEFINE_THINKFUNC(StartTrack),
DEFINE_ENTITYFUNC(DartTouch),
DEFINE_ENTITYFUNC(TrackTouch),
DEFINE_THINKFUNC(TrackTarget),
END_DATADESC()

ConVar xen_bee_speed("sk_npc_xen_bee_speed", "150");
ConVar xen_bee_maxdist("sk_npc_xen_bee_maxdist", "500");

void CNPC_XenBee::Precache()
{
	PrecacheModel("models/_Monsters/Xen/hornet.mdl");

	iHornetPuff = PrecacheModel("sprites/muz1.vmt");

	PrecacheScriptSound("XenBee.Die");
	PrecacheScriptSound("XenBee.Buzz");
}

void CNPC_XenBee::Spawn(void)
{
	Precache();

	SetMoveType(MOVETYPE_FLY);
	SetSolid(SOLID_NONE);
	m_takedamage = DAMAGE_YES;
	AddFlag(FL_NPC);
	m_iHealth = 3;
	m_bloodColor = BLOOD_COLOR_GREEN;

	m_flStopAttack = CURTIME + RandomFloat(28.0, 32.0);

	m_flFieldOfView = VIEW_FIELD_WIDE;

	m_flFlySpeed = xen_bee_speed.GetFloat() * RandomFloat(0.9f, 1.1f);

	SetModel("models/_Monsters/Xen/hornet.mdl");
	UTIL_SetSize(this, Vector(-6, -6, -6), Vector(6, 6, 6));

	SetTouch(&CNPC_XenBee::StingTouch);
	SetThink(&CNPC_XenBee::StartTrack);

	m_flDamage = 1;

	SetNextThink(CURTIME + 0.1f);
	ResetSequenceInfo();

	SetCollisionGroup(HL2COLLISION_GROUP_HOMING_MISSILE); // at first spawn as interactive debris, later we'll switch to npc

	m_vecEnemyLKP = vec3_origin;

	m_flNextAttack = CURTIME;
	m_flNextFlinchTime = CURTIME;
	
	SetBodygroup(1, 1); // wing blur
}

//=========================================================
// hornets will never get mad at each other, no matter who the owner is.
//=========================================================
Disposition_t CNPC_XenBee::IRelationType(CBaseEntity *pTarget)
{
	if (pTarget->GetModelIndex() == GetModelIndex())
	{
		return D_NU;
	}

	if (pTarget->ClassMatches("npc_hopper"))
	{
		return D_HT;
	}

	return BaseClass::IRelationType(pTarget);
}

// Make bees prioritise NPCs nearby instead of player, unless player got closer
bool CNPC_XenBee::IsValidEnemy(CBaseEntity *pEnemy)
{
	float playerDist = FLT_MAX;
	float npcDist = 0;

	CBasePlayer *player = UTIL_GetLocalPlayer();
	if (player)	playerDist = GetAbsOrigin().DistTo(player->GetAbsOrigin());

	if (pEnemy && pEnemy->IsNPC())
	{
		npcDist = GetAbsOrigin().DistTo(pEnemy->GetAbsOrigin());
		if (npcDist < playerDist) return true;
	}
	if (pEnemy && player && pEnemy == player)
	{
		if (playerDist > xen_bee_maxdist.GetFloat()) return false; // even if no other enemies, player that far away shouldn't become our enemies.
		if (playerDist < npcDist || npcDist == 0) return true; // if player is closer than any NPCs or there never were any, player can be our enemy.
	}

	return BaseClass::IsValidEnemy(pEnemy);
}

Class_T CNPC_XenBee::Classify(void)
{
	return	CLASS_BARNACLE;
}

//=========================================================
// StartDart - starts a hornet out just flying straight.
//=========================================================
void CNPC_XenBee::StartDart(void)
{
	IgniteTrail();

	SetTouch(&CNPC_XenBee::DartTouch);

//	SetThink(&CBaseEntity::SUB_Remove);
//	SetNextThink(CURTIME + 1.5);
}

void CNPC_XenBee::StingTouch(CBaseEntity *pOther)
{
	if (!pOther || !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS))
	{
		return;
	}

	if (m_flNextAttack < CURTIME)
	{
		CPASAttenuationFilter filter(this);
		EmitSound(filter, entindex(), "XenBee.Buzz");

		CTakeDamageInfo info(this, GetOwnerEntity(), m_flDamage, DMG_BULLET);
		CalculateBulletDamageForce(&info, GetAmmoDef()->Index("Pistol"), GetAbsVelocity(), GetAbsOrigin());
		pOther->TakeDamage(info);

		//	AddEffects(EF_NODRAW);

		//	AddSolidFlags(FSOLID_NOT_SOLID);// intangible

		if( pOther && pOther->IsPlayer())
			m_iHealth -= 1; // lose a bit of health with each sting. After a series of stings, hornet dies and disappears.
		m_flNextAttack = CURTIME + 2.0f;
	}

//	UTIL_Remove(this);
//	SetTouch(NULL);
}


//=========================================================
// StartTrack - starts a hornet out tracking its target
//=========================================================
void CNPC_XenBee::StartTrack(void)
{
	if (m_nDebugBits & bits_debugDisableAI)
	{
		SetNextThink(CURTIME + 0.1f);
		return;
	}
	IgniteTrail();

	SetTouch(&CNPC_XenBee::TrackTouch);
	SetThink(&CNPC_XenBee::TrackTarget);
	
	SetNextThink(CURTIME + 0.1f);
}

void TE_BeamFollow(IRecipientFilter& filter, float delay,
	int iEntIndex, int modelIndex, int haloIndex, float life, float width, float endWidth,
	float fadeLength, float r, float g, float b, float a);

void CNPC_XenBee::IgniteTrail(void)
{
}

unsigned int CNPC_XenBee::PhysicsSolidMaskForEntity(void) const
{
	unsigned int iMask = BaseClass::PhysicsSolidMaskForEntity();

	iMask &= ~CONTENTS_MONSTERCLIP;

	return iMask;
}

//=========================================================
// Tracking Hornet hit something
//=========================================================
void CNPC_XenBee::TrackTouch(CBaseEntity *pOther)
{
	if (!pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) 
		|| pOther->GetCollisionGroup() == COLLISION_GROUP_DEBRIS 
		|| pOther->GetCollisionGroup() == COLLISION_GROUP_INTERACTIVE_DEBRIS)
	{
		return;
	}

	if (pOther == GetOwnerEntity() || pOther->GetModelIndex() == GetModelIndex())
	{// bumped into the guy that shot it.
	 //SetSolid( SOLID_NOT );
		return;
	}

	if (pOther->GetFlags() & FL_NOTARGET || pOther->GetFlags() & FL_GODMODE)
	{
		return;
	}

	int nRelationship = IRelationType(pOther);
	if ((nRelationship == D_FR || nRelationship == D_NU || nRelationship == D_LI) || (pOther->m_takedamage != DAMAGE_YES))
	{
		// hit something we don't want to hurt, so turn around.
		Vector vecVel = GetAbsVelocity();

		VectorNormalize(vecVel);

		vecVel.x *= -1;
		vecVel.y *= -1;

		SetAbsOrigin(GetAbsOrigin() + vecVel * 4); // bounce the hornet off a bit.
		SetAbsVelocity(vecVel * m_flFlySpeed);

		return;
	}

	StingTouch(pOther);
}

void CNPC_XenBee::DartTouch(CBaseEntity *pOther)
{
	StingTouch(pOther);
}

//=========================================================
// Hornet is flying, gently tracking target
//=========================================================
void CNPC_XenBee::TrackTarget(void)
{
	Vector	vecFlightDir;
	Vector	vecDirToEnemy;
	Vector	vF, vU, vR; GetVectors(&vF, &vR, &vU);
	float	flDelta;

	StudioFrameAdvance();
	
	if (CURTIME > m_flStopAttack || m_iHealth <= 0)
	{
		EmitSound("XenBee.Die");
		SetTouch(NULL);
		SetThink(&CBaseEntity::SUB_Remove);
		SetNextThink(CURTIME + 0.1f);
		return;
	}

	if (m_flNextAttack > CURTIME)
	{
		SetCollisionGroup(COLLISION_GROUP_DEBRIS); // don't block the player while flying around them
	}
	else
	{
		SetCollisionGroup(HL2COLLISION_GROUP_HOMING_MISSILE);
	}

	// UNDONE: The player pointer should come back after returning from another level
	if (GetEnemy() == NULL || (GetEnemy()->IsPlayer()))
	{// enemy is dead.
		GetSenses()->Look(512);
		SetEnemy(BestEnemy());
	}

	if (GetEnemy() != NULL && FVisible(GetEnemy()))
	{
		m_vecEnemyLKP = GetEnemy()->BodyTarget(GetAbsOrigin());
	}
	else
	{
		if (m_flNextFlinchTime < CURTIME)
		{
			m_vecEnemyLKP = GetAbsOrigin() +
				(vF * RandomVector(-100, 100)) + (vU * RandomVector(-4, 4)) + (vR * RandomVector(-32, 32))
				* m_flFlySpeed * 0.1; // go in random direction // m_vecEnemyLKP + GetAbsVelocity() * m_flFlySpeed * 0.1;

			m_flNextFlinchTime = CURTIME + RandomFloat(0.5, 1.5);
		}
	}
/*
	if (RandomFloat(0, 1) < 0.5)
	{
		if (m_flNextFlinchTime < CURTIME)
		{
			Forget(bits_MEMORY_HAD_ENEMY | bits_MEMORY_HAD_PLAYER);
			m_vecEnemyLKP = GetAbsOrigin() +
				(vF * RandomVector(-100, 100)) + (vU * RandomVector(-4, 4)) + (vR * RandomVector(-32, 32))
				* m_flFlySpeed * 0.1; // go in random direction
		}
	}
*/
	vecDirToEnemy = m_vecEnemyLKP - GetAbsOrigin();
	VectorNormalize(vecDirToEnemy);

	if (GetAbsVelocity().Length() < 0.1)
		vecFlightDir = vecDirToEnemy;
	else
	{
		vecFlightDir = GetAbsVelocity();
		VectorNormalize(vecFlightDir);
	}

	SetAbsVelocity(vecFlightDir + vecDirToEnemy);

	// measure how far the turn is, the wider the turn, the slow we'll go this time.
	flDelta = DotProduct(vecFlightDir, vecDirToEnemy);

	if (flDelta < 0.8)
	{// hafta turn wide again. play sound
		CPASAttenuationFilter filter(this);
		EmitSound(filter, entindex(), "XenBee.Buzz");
	}

	if (flDelta <= 0 )
	{// no flying backwards, but we don't want to invert this, cause we'd go fast when we have to turn REAL far.
		flDelta = 0.25;
	}

	Vector vecVel = vecFlightDir + vecDirToEnemy;
	VectorNormalize(vecVel);
	
	vecVel.x += random->RandomFloat(-0.10, 0.10);// scramble the flight dir a bit.
	vecVel.y += random->RandomFloat(-0.10, 0.10);
	vecVel.z += random->RandomFloat(-0.10, 0.10);

	SetAbsVelocity(vecVel * (m_flFlySpeed * flDelta));// scale the dir by the ( speed * width of turn )
	SetNextThink(CURTIME + random->RandomFloat(0.05, 0.1));

	QAngle angNewAngles;
	VectorAngles(GetAbsVelocity(), angNewAngles);
	SetAbsAngles(angNewAngles);

	SetSolid(SOLID_BBOX);

	// if hornet is close to the enemy, jet in a straight line for a half second.
	// (only in the single player game)
	if (GetEnemy() != NULL)
	{
		if (flDelta >= 0.4 && (GetAbsOrigin() - m_vecEnemyLKP).Length() <= 100)
		{
			CPVSFilter filter(GetAbsOrigin());
			te->Sprite(filter, 0.0,
				&GetAbsOrigin(), // pos
				iHornetPuff,	// model
				0.2,				//size
				32				// brightness
			);

			CPASAttenuationFilter filter2(this);
			EmitSound(filter2, entindex(), "XenBee.Buzz");
			SetAbsVelocity(GetAbsVelocity() * 2);
			SetNextThink(CURTIME + 0.1f);
			// don't attack again
		//	m_flStopAttack = CURTIME;
		}
	}
}

class CNPC_BeeNest : public CAI_BaseNPC
{
	DECLARE_CLASS(CNPC_BeeNest, CAI_BaseNPC);
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
public:
	CNPC_BeeNest();
	~CNPC_BeeNest();

	Class_T Classify(void);
	void	Precache(void);
	void	Spawn(void);
	bool	CreateVPhysics();
	bool	ShouldSavePhysics() { return true; }
	void	Activate();
	void	NPCThink();

	Vector EyePosition()
	{
		return WorldSpaceCenter();
	}
	virtual unsigned int	PhysicsSolidMaskForEntity(void) const;

	void	AlertSound(void);
	void	DeathSound(const CTakeDamageInfo &info);
	void	PainSound(const CTakeDamageInfo &info);
	void	IdleSound(void);

	int		OnTakeDamage_Alive(const CTakeDamageInfo &info);
	void	Event_Killed(const CTakeDamageInfo &info);
	bool	ShouldGib(const CTakeDamageInfo &info) { return true; };
	bool	CorpseGib(const CTakeDamageInfo &info);
	void    HandleAnimEvent(animevent_t *pEvent);
	int		RangeAttack1Conditions(float flDot, float flDist);
	int		SelectSchedule(void);

	// The tree shouldn't take in sounds and try to face the enemy.
	int GetSoundInterests(void) { return	NULL; }
	float MaxYawSpeed(void) { return 0.0f; }
	
	int		m_beeamount_int;
	bool	m_beenestphysical_boolean;

	/*static*/ QAngle vecOriginalAngle;

private:
	CSoundPatch	*m_pBuzzSound;
};

//QAngle CNPC_BeeNest::vecOriginalAngle;

Class_T	CNPC_BeeNest::Classify(void)
{
	return CLASS_NONE; // currently, don't react to anything. 
}

CNPC_BeeNest::CNPC_BeeNest(void)
{
	m_beeamount_int = 10;
	m_hNPCFileInfo = LookupNPCInfoScript("npc_bee_turret");
	m_pBuzzSound = NULL;
//	m_beenestphysical_boolean = false;
}

CNPC_BeeNest::~CNPC_BeeNest(void)
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if (m_pBuzzSound != NULL)
	{
		controller.SoundDestroy(m_pBuzzSound);
		m_pBuzzSound = NULL;
	}
}

void CNPC_BeeNest::Precache()
{
	PrecacheModel(STRING(GetModelName()));
	PrecacheScriptSound("XenBeeTurret.IdleBuzz");
	PrecacheScriptSound("XenBeeTurret.AlertBuzz");
	PrecacheScriptSound("XenBeeTurret.HitBuzz");
	PrecacheScriptSound("XenBeeTurret.Break");
	PropBreakablePrecacheAll(GetModelName());
	BaseClass::Precache();
}

void CNPC_BeeNest::Spawn()
{
	char *szModel = (char *)STRING(GetModelName());
	if (!szModel || !*szModel)
	{
		szModel = "models/props_junk/watermelon01.mdl";
		SetModelName(AllocPooledString(szModel));
	}

	Precache();
	SetModel(szModel);

	BaseClass::Spawn();
	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

	SetSolid(SOLID_VPHYSICS);
	AddSolidFlags(/*FSOLID_FORCE_WORLD_ALIGNED |*/ FSOLID_NOT_STANDABLE);

	vecOriginalAngle = GetAbsAngles();

	SetMoveType(m_beenestphysical_boolean ? MOVETYPE_VPHYSICS : MOVETYPE_NONE);

	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);

	CapabilitiesClear();
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_Climb_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_CLIMB);
	if ((GetNPCScriptData().NPC_Capable_Doors_boolean) == true) CapabilitiesAdd(bits_CAP_DOORS_GROUP);

	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));

	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;
	SetDistLook(512);
	m_NPCState = NPC_STATE_NONE;
	m_flNextAttack = CURTIME;

//	SetCollisionGroup(COLLISION_GROUP_INTERACTIVE);

	// Efficiency option - trees don't need to see or listen, 
	// they have the touch trigger entity.
	GetSenses()->AddSensingFlags(SENSING_FLAGS_DONT_LISTEN | SENSING_FLAGS_DONT_LOOK);

	NPCInit();
}

unsigned int CNPC_BeeNest::PhysicsSolidMaskForEntity(void) const
{
	return MASK_SOLID;
}

bool CNPC_BeeNest::CreateVPhysics()
{
	if (!m_beenestphysical_boolean)
	{
		VPhysicsInitStatic();
		return true;
	}
	else
	{
		VPhysicsDestroyObject();

		RemoveSolidFlags(FSOLID_NOT_SOLID);

		//Setup the physics controller on the roller
		IPhysicsObject *pPhysicsObject = VPhysicsInitNormal(SOLID_VPHYSICS, GetSolidFlags(), false);

		if (pPhysicsObject == NULL)
			return false;
		
		SetMoveType(MOVETYPE_VPHYSICS);

		return true;
	}
}

void CNPC_BeeNest::Activate()
{
	vecOriginalAngle = GetAbsAngles();

//	if (m_pBuzzSound == NULL)
	{
		// Create the sound
	//	CPASAttenuationFilter filter(this);
	//	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	//	m_pBuzzSound = controller.SoundCreate(filter, entindex(), CHAN_STATIC, "XenBeeTurret.IdleBuzz", ATTN_NORM);

	//	Assert(m_pBuzzSound);

		// Start the engine sound
	//	controller.Play(m_pBuzzSound, 0.0f, 80.0f);
	//	controller.SoundChangeVolume(m_pBuzzSound, 0.5f, 0.5f);
	}

	IdleSound();

	if (m_beenestphysical_boolean)
	{
		CreateVPhysics();
	}

	BaseClass::Activate();
}

void CNPC_BeeNest::NPCThink()
{
	BaseClass::NPCThink();

//	QAngle localAngles = GetLocalAngles();

	StudioFrameAdvance();

	bool InPVS = ((UTIL_FindClientInPVS(edict()) != NULL)
		|| (UTIL_ClientPVSIsExpanded()
			&& UTIL_FindClientInVisibilityPVS(edict())));

	// See how close the player is
	CBasePlayer *pPlayerEnt = AI_GetSinglePlayer();
	float flDistToPlayerSqr = (GetAbsOrigin() - pPlayerEnt->GetAbsOrigin()).LengthSqr();

	// If they're far enough away, just wait to think again
	if (flDistToPlayerSqr > Square(40 * 12) || !InPVS)
	{
		SetNextThink(CURTIME + 0.3f);
		return;
	}
	
	// vibrate
	if (RandomFloat(0, 1) < 0.5)
	{
		if( m_beenestphysical_boolean )
			SetLocalAngularVelocity(QAngle(RandomFloat(-16, -16), RandomFloat(-16, -16),RandomFloat(-16, -16)));
		else
			SetAbsAngles(vecOriginalAngle + ((GetLastDamageTime() + 1.0 < CURTIME) ? RandomAngle(-0.10, 0.10) : RandomAngle(-0.3, 0.3)));
	}

	// buzz
	if (m_pBuzzSound != NULL)
	{		
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundChangePitch(m_pBuzzSound, ((GetLastDamageTime() + 1.0 < CURTIME) ? 100 : 80), 0.1f);
		controller.SoundChangeVolume(m_pBuzzSound, ((GetLastDamageTime() + 1.0 < CURTIME) ? 1 : 0.5), 0.1f);
	}
			
	SetNextThink(CURTIME + 0.02f);
}

void CNPC_BeeNest::IdleSound(void)
{
	EmitSound("XenBeeTurret.IdleBuzz");
	BaseClass::IdleSound();
}

void CNPC_BeeNest::AlertSound(void)
{
//	EmitSound("XenBeeTurret.AlertBuzz");
}

void CNPC_BeeNest::PainSound(const CTakeDamageInfo &info)
{
	EmitSound("XenBeeTurret.HitBuzz");
}

void CNPC_BeeNest::DeathSound(const CTakeDamageInfo &info)
{
	EmitSound("XenBeeTurret.Break");
}

int CNPC_BeeNest::OnTakeDamage_Alive(const CTakeDamageInfo &info)
{
	CTakeDamageInfo newInfo = info;
	if (newInfo.GetDamageType() == DMG_BLAST)
	{
		newInfo.SetDamage(GetHealth());
	}
	else
	{
		if (RandomFloat(0, 1) < 0.5 && GetLastDamageTime() + 0.5f < CURTIME) // don't let it spam too many bees
		{
			CBaseEntity *pHornet = CBaseEntity::Create("xen_bee", GetAbsOrigin() + RandomVector(-32, 32), QAngle(0, 0, 0), this);

			Vector vForward;
			RandomVectorInUnitSphere(&vForward);

			pHornet->SetAbsVelocity(vForward * 300);
			pHornet->SetOwnerEntity(this);

			if (info.GetDamageType() & DMG_BURN)
			{
				variant_t var;
				var.SetInt(0);
				pHornet->AcceptInput("Ignite", this, this, var, 0);
			}
		}
	}
	return (BaseClass::OnTakeDamage_Alive(newInfo));
}

void CNPC_BeeNest::Event_Killed(const CTakeDamageInfo &info)
{
//	QAngle angDir;

	if ((info.GetDamageType() & DMG_BLAST) == false) // blast damage kills all bees before they get out.
	{
		for (int i = 0; i < m_beeamount_int; i++)
		{
			CBaseEntity *pHornet = CBaseEntity::Create("xen_bee", GetAbsOrigin() + RandomVector(-32, 32), QAngle(0, 0, 0), this);

			Vector vForward;
			RandomVectorInUnitSphere(&vForward);

			pHornet->SetAbsVelocity(vForward * 300);
			pHornet->SetOwnerEntity(this);

			if (info.GetDamageType() & DMG_BURN)
			{
				variant_t var;
				var.SetInt(0);
				pHornet->AcceptInput("Ignite", this, this, var, 0);
			}
		}
	}
	BaseClass::Event_Killed(info);
}

bool CNPC_BeeNest::CorpseGib(const CTakeDamageInfo &info)
{
	Vector velocity = vec3_origin;
	AngularImpulse	angVelocity = RandomAngularImpulse(-150, 150);
	breakablepropparams_t params(GetAbsOrigin(), GetAbsAngles(), velocity, angVelocity);
	params.impactEnergyScale = 1.0f;
	params.defBurstScale = 150.0f;
	params.defCollisionGroup = COLLISION_GROUP_DEBRIS;
	PropBreakableCreateAll(GetModelIndex(), NULL, params, this, -1, true, true);
	return true;
}

void CNPC_BeeNest::HandleAnimEvent(animevent_t *pEvent)
{
	BaseClass::HandleAnimEvent(pEvent);
}

int CNPC_BeeNest::RangeAttack1Conditions(float flDot, float flDist)
{
	if (CURTIME < m_flNextAttack)
		return 0;

	if ((GetFlags() & FL_ONGROUND) == false)
		return 0;

	if (flDist > 512.0f)
		return COND_TOO_FAR_TO_ATTACK;

	return COND_CAN_RANGE_ATTACK1;
}

// Just blank.
int CNPC_BeeNest::SelectSchedule(void)
{
	if (m_NPCState == NPC_STATE_IDLE)
	{
		return SCHED_PATROL_WALK;
	}
	else if (m_NPCState == NPC_STATE_COMBAT)
	{
		if (HasCondition(COND_CAN_RANGE_ATTACK1))
		{
			return SCHED_MELEE_ATTACK1;
			m_flNextAttack = CURTIME + 3.0f;
		}
		else
		{
			return SCHED_PATROL_WALK;
		}
	}

	return BaseClass::SelectSchedule();
}

BEGIN_DATADESC(CNPC_BeeNest)
DEFINE_KEYFIELD(m_beeamount_int, FIELD_INTEGER, "BeeAmount"),
DEFINE_KEYFIELD(m_beenestphysical_boolean, FIELD_BOOLEAN, "NestIsPhysical"),
DEFINE_FIELD(vecOriginalAngle, FIELD_VECTOR),
END_DATADESC()
LINK_ENTITY_TO_CLASS(npc_bee_nest, CNPC_BeeNest);
LINK_ENTITY_TO_CLASS(npc_bee_turret, CNPC_BeeNest);
LINK_ENTITY_TO_CLASS(npc_beenest, CNPC_BeeNest);
LINK_ENTITY_TO_CLASS(npc_beeturret, CNPC_BeeNest);
LINK_ENTITY_TO_CLASS(npc_nest, CNPC_BeeNest);

AI_BEGIN_CUSTOM_NPC(npc_bee_turret, CNPC_BeeNest)
DECLARE_USES_SCHEDULE_PROVIDER(CAI_BaseNPC)
AI_END_CUSTOM_NPC()
#endif // BEETURRET

#if defined PSTORM
//========= Dark Interval, 2018. ==============================================//
//
// Purpose: The Particle Storm. Hovering cloud of deadly energy!
//
//=============================================================================//
#define SF_PSTORM_CHEAP 1 << 17
#define SF_PSTORM_NOPLAYERPULL 1 << 18

static const char *DYNAMIC_LIGHTS_THINK = "BoostDynamicLights";

//ConVar npc_pstorm_reducedparticles("npc_pstorm_reducedparticles", 0);
//ConVar npc_pstorm_beam_sprite("npc_pstorm_beam_sprite", "sprites/plasmabeam.vmt");
ConVar npc_pstorm_debug("npc_pstorm_debug", "0");
ConVar npc_pstorm_gravityscale_min("npc_pstorm_gravityscale_min", "0.25");
ConVar npc_pstorm_gravityscale_max("npc_pstorm_gravityscale_max", "25");
//ConVar npc_pstorm_clamp_motor_forces_z_factor("npc_pstorm_clamp_motor_forces_z_factor", "200");
//ConVar npc_pstorm_altitude_threshold("npc_pstorm_altitude_threshold", "90.0");
//ConVar npc_pstorm_z_acceleration_factor("npc_pstorm_z_acceleration_factor", "500");

CNPC_ParticleStorm::CNPC_ParticleStorm()
{
	m_hNPCFileInfo = LookupNPCInfoScript("npc_pstorm");
	//	m_Sentences.Init(this, "NPC_SynthScanner.SentenceParameters");

	m_flAttackNearDist = 300;
	m_flAttackFarDist = 600;
	m_flAttackRange = 350;
	m_z_acceleration_float = 500; // vertical speed when moving
	m_z_threshold_float = 90; // the distance to the ground the pstorm will try to maintain

	m_vortex_inner_radius_float = 64;
	m_vortex_outer_radius_float = 512;
}

CNPC_ParticleStorm::~CNPC_ParticleStorm()
{
	m_dynamiclightsColor = vec3_origin;
	m_hPseudopodia = NULL;
	if ( m_vortex_controller_handle ) UTIL_Remove(m_vortex_controller_handle);
}

void CNPC_ParticleStorm::SpeakSentence(int nSentenceType)
{
}

Disposition_t CNPC_ParticleStorm::IRelationType(CBaseEntity *pTarget)
{
	Disposition_t disp = BaseClass::IRelationType(pTarget);

	return disp;
}

void CNPC_ParticleStorm::InitCustomSchedules(void)
{
	INIT_CUSTOM_AI(CNPC_ParticleStorm);
	ADD_CUSTOM_TASK(CNPC_ParticleStorm, TASK_PSTORM_PROJECTILE_ATTACK);
	ADD_CUSTOM_SCHEDULE(CNPC_ParticleStorm, SCHED_PSTORM_PROJECTILE_ATTACK);
	AI_LOAD_SCHEDULE(CNPC_ParticleStorm, SCHED_PSTORM_PROJECTILE_ATTACK);
	ADD_CUSTOM_SCHEDULE(CNPC_ParticleStorm, SCHED_PSTORM_ATTACK_HOVER);
	AI_LOAD_SCHEDULE(CNPC_ParticleStorm, SCHED_PSTORM_ATTACK_HOVER);
}

int CNPC_ParticleStorm::attachmentParticleCenter = -1;
int CNPC_ParticleStorm::attachmentParticleCp1 = -1;
int CNPC_ParticleStorm::attachmentParticleCp2 = -1;
int CNPC_ParticleStorm::attachmentParticleCp3 = -1;
int CNPC_ParticleStorm::attachmentParticleCp4 = -1;
int CNPC_ParticleStorm::attachmentParticleCp5 = -1;
int CNPC_ParticleStorm::attachmentParticleCp6 = -1;
int CNPC_ParticleStorm::attachmentParticleCp7 = -1;
int CNPC_ParticleStorm::attachmentParticleCp8 = -1;
int CNPC_ParticleStorm::attachmentParticleCp9 = -1;
int CNPC_ParticleStorm::attachmentParticleCp10 = -1;
int CNPC_ParticleStorm::attachmentParticleCp11 = -1;
int CNPC_ParticleStorm::attachmentParticleCp12 = -1;
int CNPC_ParticleStorm::attachmentParticleCp13 = -1;

void CNPC_ParticleStorm::Precache(void)
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));

	BaseClass::Precache();

	if ( HasSpawnFlags(SF_PSTORM_CHEAP) )
	{
		PrecacheParticleSystem("pstorm_composite_alert_cheap");
		PrecacheParticleSystem("pstorm_composite_idle_cheap");
	} else
	{
		PrecacheParticleSystem("pstorm_composite_alert");
		PrecacheParticleSystem("pstorm_composite_idle");
	}
	PrecacheParticleSystem("pstorm_electrical_arc_01_system_damage");
	PrecacheParticleSystem("pstorm_dissipate_system");
	PrecacheParticleSystem("pstorm_form2_system");
	PrecacheModel("sprites/plasmabeam.vmt");
	PrecacheScriptSound("NPC_PStorm.FlyLoop");
	PrecacheScriptSound("NPC_PStorm.Form");
	PrecacheScriptSound("NPC_PStorm.Dissipate");
	PrecacheParticleSystem("vortigaunt_beam");
	enginesound->PrecacheSentenceGroup("SYNTHSCANNER");

	PrecacheModel("models/_Monsters/Xen/squid_spitball_large.mdl");
	if ( !HasSpawnFlags(SF_NPC_START_EFFICIENT) )
	{
		UTIL_PrecacheOther("grenade_homer");
		UTIL_PrecacheOther("env_gravity_vortex");
	}
}

void CNPC_ParticleStorm::Spawn(void)
{
	Precache();
	SetModel(STRING(GetModelName()));

	BaseClass::Spawn();

	AddEffects(EF_NOSHADOW | EF_NORECEIVESHADOW);

	m_hPseudopodia = NULL;
	m_bDissipating = false;
	m_flDissipateTime = FLT_MAX;
	m_flFormTime = FLT_MAX;

	m_dynamiclightsColor = Vector(0.005, 0.05, 0.007);

	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);

	SetHullType(Hull_t(GetNPCScriptData().NPC_Stats_HullType_int));
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;

	//	CapabilitiesClear();
	//	CapabilitiesAdd(bits_CAP_MOVE_FLY);
	CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);

	m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);

	UTIL_ScaleForGravity(1.0);

	m_nFlyMode = SCANNER_FLY_PATROL;
	m_vCurrentBanking = vec3_origin;
	m_NPCState = NPC_STATE_IDLE;

	m_nRenderMode = kRenderNone;
	m_pstorm_dormant_bool = true;
	StopLoopingSounds();

	attachmentParticleCenter = LookupAttachment("particle_origin");
	attachmentParticleCp1 = LookupAttachment("cp1");
	attachmentParticleCp2 = LookupAttachment("cp2");
	attachmentParticleCp3 = LookupAttachment("cp3");
	attachmentParticleCp4 = LookupAttachment("cp4");
	attachmentParticleCp5 = LookupAttachment("cp5");
	attachmentParticleCp6 = LookupAttachment("cp6");
	attachmentParticleCp7 = LookupAttachment("cp7");
	attachmentParticleCp8 = LookupAttachment("cp8");
	attachmentParticleCp9 = LookupAttachment("cp9");
	attachmentParticleCp10 = LookupAttachment("cp10");
	attachmentParticleCp11 = LookupAttachment("cp11");
	attachmentParticleCp12 = LookupAttachment("cp12");
	attachmentParticleCp13 = LookupAttachment("cp13");

	m_player_caught_bool = false;
}

void CNPC_ParticleStorm::Activate()
{
	// Get a handle to my vortex pull filter entity if there is one.
	if ( m_vortex_filter_name != NULL_STRING )
	{
		CBaseEntity *pFilter = gEntList.FindEntityByName(NULL, m_vortex_filter_name);
		if ( pFilter != NULL )
		{
			m_vortex_filter_handle = dynamic_cast<CBaseFilter*>( pFilter );
		}
	}
	BaseClass::Activate();
}

void CNPC_ParticleStorm::NPCThink(void)
{
	BaseClass::NPCThink();

	if ( m_flNextAttack < CURTIME && !m_pstorm_dormant_bool && m_enable_projectiles_bool )
	{
		if ( GetEnemy() )
		{
			LaunchProjectile(GetEnemy());
		}
	}

	//	Vector pos1, pos2; 
	//	GetAttachment(RandomInt(attachmentParticleCp1, attachmentParticleCp13), pos1, NULL);
	//	GetAttachment(RandomInt(attachmentParticleCp1, attachmentParticleCp13), pos2, NULL);
	//	NDebugOverlay::Line(pos1, pos2, 200, 200, 250, false, 0.1f);

	if ( m_lifeState == LIFE_ALIVE && !m_pstorm_dormant_bool && m_enabledlights_bool )
	{
		Vector pos = WorldSpaceCenter();
		CBroadcastRecipientFilter filter;
		te->DynamicLight(filter, 0.0, &pos, 25, 255, 50, RandomFloat(0.17, 0.22), RandomInt(900, 1000), 0.3, 50);
	}

	if ( npc_pstorm_debug.GetBool() && !m_pstorm_dormant_bool )
	{
		NDebugOverlay::Sphere(GetAbsOrigin(), m_vortex_inner_radius_float, 250, 50, 250, true, 0.1f);
		NDebugOverlay::Sphere(GetAbsOrigin(), m_vortex_outer_radius_float, 250, 150, 50, true, 0.1f);
	}
}

float CNPC_ParticleStorm::GetGoalDistance(void)
{
	if ( m_flGoalOverrideDistance != 0.0f )
		return m_flGoalOverrideDistance;

	switch ( m_nFlyMode )
	{
		case SCANNER_FLY_ATTACK:
		{
			float goalDist = ( m_flAttackNearDist + ( ( m_flAttackFarDist - m_flAttackNearDist ) / 2 ) );

			return goalDist;
		}
		break;
	}

	return 128.0f;
}

bool CNPC_ParticleStorm::FValidateHintType(CAI_Hint *pHint)
{
	switch ( pHint->HintType() )
	{
		case HINT_TACTICAL_SPAWN:
			return true;
			break;
		default:
			return false;
			break;
	}
}

void CNPC_ParticleStorm::PrescheduleThink(void)
{
	if ( m_pstorm_dormant_bool )
	{
		if ( npc_pstorm_debug.GetBool() )
			NDebugOverlay::Box(WorldSpaceCenter(), Vector(-32, -32, -32), Vector(32, 32, 32), 255, 0, 0, 150, 0.05f);

		m_nRenderMode = kRenderNone;

		if ( m_flFormTime < CURTIME )
		{
			FinishForming();
		}
	}

	else
	{
		if ( ( m_NPCState == NPC_STATE_ALERT ) && CURTIME - GetLastEnemyTime() > 5.0f )
		{
			m_NPCState = NPC_STATE_IDLE;
		}

		if ( m_lifeState == LIFE_ALIVE )
			UpdatePseudopodia();

		if ( m_lifeState == LIFE_DYING )
		{
			if ( npc_pstorm_debug.GetBool() )
				NDebugOverlay::Box(WorldSpaceCenter(), Vector(-32, -32, -32), Vector(32, 32, 32), 0, 255, 0, 150, 0.05f);
			if ( m_flDissipateTime < CURTIME )
			{
				FinishDissipation();
			}
		}

		BaseClass::PrescheduleThink();
	}
}

void CNPC_ParticleStorm::StartTask(const Task_t *pTask)
{
	switch ( pTask->iTask )
	{
		case TASK_PSTORM_PROJECTILE_ATTACK:
		{
			if ( GetEnemy() )
			{
				LaunchProjectile(GetEnemy());
				SetWait(0.3f);
			} else
			{
				TaskFail("no enemy");
			}
			break;
		}

		default:
			BaseClass::StartTask(pTask);
			break;
	}
}

void CNPC_ParticleStorm::RunTask(const Task_t *pTask)
{
	switch ( pTask->iTask )
	{
		case TASK_PSTORM_PROJECTILE_ATTACK:
		{
			if ( IsWaitFinished() )
			{
				TaskComplete();
			}
			break;
		}
		default:
		{
			BaseClass::RunTask(pTask);
		}
	}
}

int CNPC_ParticleStorm::SelectSchedule(void)
{
	return BaseClass::SelectSchedule();
}


int CNPC_ParticleStorm::TranslateSchedule(int scheduleType)
{
	if ( scheduleType == SCHED_RANGE_ATTACK1 )
	{
		return SCHED_PSTORM_PROJECTILE_ATTACK;
	}

	return BaseClass::TranslateSchedule(scheduleType);
}

int CNPC_ParticleStorm::RangeAttack1Conditions(float flDot, float flDist)
{
	if ( m_lifeState == LIFE_DYING ) return COND_NONE; // pstorm is dissipating 
	if ( flDist < InnateRange1MinRange() ) return COND_TOO_CLOSE_TO_ATTACK;
	if ( flDist > InnateRange1MaxRange() ) return COND_TOO_FAR_TO_ATTACK;
	return COND_CAN_RANGE_ATTACK1;
}

void CNPC_ParticleStorm::LaunchProjectile(CBaseEntity *pEnemy)
{
	if ( HasSpawnFlags(SF_NPC_START_EFFICIENT) ) return;

	if ( !GetEnemy() ) return;

	if ( m_lifeState == LIFE_DYING ) return;

	Vector vSpitPos = GetAbsOrigin();
	//	GetAttachment("launch_point", vSpitPos);
	Vector	vTarget = GetEnemy()->GetAbsOrigin();

	Vector vecDir;
	GetVectors(&vecDir, NULL, NULL);

	VectorNormalize(vecDir);
	vecDir.x += RandomFloat(-0.1, 0.1);
	vecDir.y += RandomFloat(-0.1, 0.1);

	Vector vecVelocity;
	float randomVel = RandomFloat(-25, 25);
	VectorMultiply(vecDir, 450 + randomVel, vecVelocity);

	CGrenadeHomer *pGrenade = CGrenadeHomer::CreateGrenadeHomer(MAKE_STRING("models/_Monsters/Xen/squid_spitball_large.mdl"), NULL_STRING, vSpitPos, vec3_angle, edict());
	pGrenade->Spawn();

	pGrenade->SetHoming(( 0.01 * 50 ),
		0.1f, // Delay
		0.25f, // Ramp up
		60.0f, // Duration
		1.0f);// Ramp down
	pGrenade->SetDamage(10);
	pGrenade->SetDamageRadius(50);
	pGrenade->Launch(this,
		GetEnemy(),
		vecVelocity,
		450,
		2,
		HOMER_SMOKE_TRAIL_ALIEN);

	pGrenade->SetRenderMode(kRenderNone);
	pGrenade->SetCollisionGroup(COLLISION_GROUP_NPC);
	//	DispatchParticleEffect("pstorm_electrical_arc_01_system_damage", PATTACH_ABSORIGIN_FOLLOW, pGrenade);

	m_flNextAttack = CURTIME + RandomFloat(1.5f, 2.0f);
}

int CNPC_ParticleStorm::OnTakeDamage_Alive(const CTakeDamageInfo &info)
{
	if ( m_pstorm_dormant_bool )
		return 0;
	if ( m_bDissipating )
		return 0;

	DispatchParticleEffect("pstorm_electrical_arc_01_system_damage", PATTACH_POINT_FOLLOW, this,
		LookupAttachment("particle_origin"), false);

	return ( BaseClass::OnTakeDamage_Alive(info) );
}

void CNPC_ParticleStorm::InputForm(inputdata_t &inputdata)
{
	BeginForming();
}
#if 0
ConVar npc_pstorm_light_fov("npc_pstorm_light_fov", "135");
void CNPC_ParticleStorm::CreateDynamicLights(void)
{
	Vector origin = WorldSpaceCenter();
	QAngle angles = QAngle(-90, 0, 0);
	Vector vAtten = Vector(1, 100, 0);
	Vector vColor = ( m_dynamiclightsColor * 10 );

	CProjectedLight *pFlashlightUp = dynamic_cast< CProjectedLight * >( CreateEntityByName("point_projectedlight") );
	pFlashlightUp->Teleport(&origin, &angles, NULL);
	pFlashlightUp->SetParent(this);
	pFlashlightUp->SetFOV(npc_pstorm_light_fov.GetFloat());
	pFlashlightUp->SetSpotlightColor(vColor);
	pFlashlightUp->SetSpotlightAttenuation(vAtten);
	pFlashlightUp->TurnOn();

	angles = QAngle(-90, 0, 0);
	CProjectedLight *pFlashlightDown = dynamic_cast< CProjectedLight * >( CreateEntityByName("point_projectedlight") );
	pFlashlightDown->Teleport(&origin, &angles, NULL);
	pFlashlightDown->SetParent(this);
	pFlashlightDown->SetFOV(npc_pstorm_light_fov.GetFloat());
	pFlashlightDown->SetSpotlightColor(vColor);
	pFlashlightDown->SetSpotlightAttenuation(vAtten);
	pFlashlightDown->TurnOn();

	angles = QAngle(0, 0, 0);
	CProjectedLight *pFlashlightFw = dynamic_cast< CProjectedLight * >( CreateEntityByName("point_projectedlight") );
	pFlashlightFw->Teleport(&origin, &angles, NULL);
	pFlashlightFw->SetParent(this);
	pFlashlightFw->SetFOV(npc_pstorm_light_fov.GetFloat());
	pFlashlightFw->SetSpotlightColor(vColor);
	pFlashlightFw->SetSpotlightAttenuation(vAtten);
	pFlashlightFw->TurnOn();

	angles = QAngle(0, 90, 0);
	CProjectedLight *pFlashlightRt = dynamic_cast< CProjectedLight * >( CreateEntityByName("point_projectedlight") );
	pFlashlightRt->Teleport(&origin, &angles, NULL);
	pFlashlightRt->SetParent(this);
	pFlashlightRt->SetFOV(npc_pstorm_light_fov.GetFloat());
	pFlashlightRt->SetSpotlightColor(vColor);
	pFlashlightRt->SetSpotlightAttenuation(vAtten);
	pFlashlightRt->TurnOn();

	angles = QAngle(0, 180, 0);
	CProjectedLight *pFlashlightBk = dynamic_cast< CProjectedLight * >( CreateEntityByName("point_projectedlight") );
	pFlashlightBk->Teleport(&origin, &angles, NULL);
	pFlashlightBk->SetParent(this);
	pFlashlightBk->SetFOV(npc_pstorm_light_fov.GetFloat());
	pFlashlightBk->SetSpotlightColor(vColor);
	pFlashlightBk->SetSpotlightAttenuation(vAtten);
	pFlashlightBk->TurnOn();

	angles = QAngle(0, 270, 0);
	CProjectedLight *pFlashlightLf = dynamic_cast< CProjectedLight * >( CreateEntityByName("point_projectedlight") );
	pFlashlightLf->Teleport(&origin, &angles, NULL);
	pFlashlightLf->SetParent(this);
	pFlashlightLf->SetFOV(npc_pstorm_light_fov.GetFloat());
	pFlashlightLf->SetSpotlightColor(vColor);
	pFlashlightLf->SetSpotlightAttenuation(vAtten);
	pFlashlightLf->TurnOn();

}
#endif
void CNPC_ParticleStorm::KillDynamicLights(void)
{
	m_dynamiclightsColor = vec3_origin;
}

void CNPC_ParticleStorm::DynamicLightsThink()
{
	m_dynamiclightsColor += Vector(0.001, 0.01, 0.0025);

	SetNextThink(CURTIME + 0.025f, DYNAMIC_LIGHTS_THINK);
}

void CNPC_ParticleStorm::BeginForming(void)
{
	SetCurrentVelocity(vec3_origin);
	if ( m_hPseudopodia != NULL ) m_hPseudopodia->Remove(); // Should never come to this. But let's just be extra careful.
	m_flFormTime = CURTIME + 2.4f; // hardcoded!
	EmitSound("NPC_Pstorm.Form");
	DispatchParticleEffect("pstorm_form2_system", PATTACH_POINT_FOLLOW, this,
		LookupAttachment("particle_origin"), true);

	m_dynamiclightsColor = Vector(0.005, 0.05, 0.007);
#if 0
	CreateDynamicLights();
#endif
	SetContextThink(&CNPC_ParticleStorm::DynamicLightsThink, CURTIME + 0.025, DYNAMIC_LIGHTS_THINK);

}

void CNPC_ParticleStorm::FinishForming(void)
{
	m_lifeState = LIFE_ALIVE;
	m_pstorm_dormant_bool = false;
	m_flFormTime = FLT_MAX;
	RemoveEFlags(EF_NODRAW);
	m_nRenderMode = kRenderNormal;
	//	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, 
	//		GetNPCScriptData().NPC_Model_ScaleMax_float), 0.5f);

	SetContextThink(NULL, 0, DYNAMIC_LIGHTS_THINK);

	m_OnFinishedForming.FireOutput(this, this, 0);

	m_flNextAttack = CURTIME + 0.5f;
}

void CNPC_ParticleStorm::InputDissipate(inputdata_t &inputdata)
{
	BeginDissipation();
}

void CNPC_ParticleStorm::BeginDissipation(void)
{
	if ( m_pstorm_dormant_bool ) return; // don't dissipate on repeat inputs

	SetCurrentVelocity(vec3_origin);
	m_lifeState = LIFE_DYING;
	if ( m_hPseudopodia != NULL ) m_hPseudopodia->Remove();
	m_flDissipateTime = CURTIME + 3.2f; // hardcoded!
	EmitSound("NPC_Pstorm.Form");
	DispatchParticleEffect("pstorm_dissipate_system", PATTACH_POINT_FOLLOW, this,
		LookupAttachment("particle_origin"), true);
	//	SetModelScale(0.1f, 1.5f);
}

void CNPC_ParticleStorm::FinishDissipation(void)
{
	m_pstorm_dormant_bool = true;
	m_flDissipateTime = FLT_MAX;
	KillDynamicLights();
	m_nRenderMode = kRenderNone;
	AddEFlags(EF_NODRAW);
	StopLoopingSounds();

	SetContextThink(NULL, 0, DYNAMIC_LIGHTS_THINK);

	m_OnFinishedDissipation.FireOutput(this, this, 0);
}

void CNPC_ParticleStorm::UpdatePseudopodia(void)
{
	if ( m_hPseudopodia == NULL && !HasSpawnFlags(SF_PSTORM_CHEAP) )
	{
		Vector vTargetPos = WorldSpaceCenter();

		m_hPseudopodia = ( CTesla* )CreateNoSpawn("point_tesla", vTargetPos, vec3_angle, this);

		m_hPseudopodia->SetParent(this);
		m_hPseudopodia->m_Color.g = 255;
		m_hPseudopodia->m_Color.b = m_hPseudopodia->m_Color.g = 25;
		m_hPseudopodia->m_Color.a = 255;
		m_hPseudopodia->m_flRadius = 256;
		m_hPseudopodia->m_NumBeams[ 0 ] = m_hPseudopodia->m_NumBeams[ 1 ] = 1;
		m_hPseudopodia->m_iszSpriteName = AllocPooledString("sprites/plasmabeam.vmt");

		m_hPseudopodia->Spawn();

		if ( m_hPseudopodia == NULL )
			return;
	}

	if ( m_hPseudopodia != NULL )
	{
		inputdata_t in;
		in.pActivator = this;
		in.pCaller = this;

		if ( !m_hPseudopodia->m_bOn )
			m_hPseudopodia->InputTurnOn(in);
		m_hPseudopodia->InputDoSpark(in);
	}

	switch ( m_NPCState )
	{
		case NPC_STATE_IDLE:
		{
			if ( HasSpawnFlags(SF_PSTORM_CHEAP) )
			{
				DispatchParticleEffect("pstorm_composite_idle_cheap", PATTACH_POINT_FOLLOW, this,
					LookupAttachment("particle_origin"), true);
			} else
			{
				DispatchParticleEffect("pstorm_composite_idle", PATTACH_POINT_FOLLOW, this,
					LookupAttachment("particle_origin"), true);
			}
		}
		break;
		case NPC_STATE_ALERT:
		case NPC_STATE_COMBAT:
		{
			if ( HasSpawnFlags(SF_PSTORM_CHEAP) )
			{
				DispatchParticleEffect("pstorm_composite_alert_cheap", PATTACH_POINT_FOLLOW, this,
					LookupAttachment("particle_origin"), true);
			} else
			{
				DispatchParticleEffect("pstorm_composite_alert", PATTACH_POINT_FOLLOW, this,
					LookupAttachment("particle_origin"), false);
			}
		}
		break;
		//	case NPC_STATE_COMBAT:
		//	{
		//		DispatchParticleEffect("pstorm_electrical_arc_01_system", PATTACH_POINT_FOLLOW, this,
		//			LookupAttachment("particle_origin"), false);
		//	}
		//	break;
		default:
			break;
	}
}

void CNPC_ParticleStorm::InputStartVortex(inputdata_t &inputdata)
{
	if ( !m_vortex_controller_handle )
	{
		m_vortex_controller_handle = CEnvGravityVortex::Create(GetAbsOrigin(), m_vortex_inner_radius_float, m_vortex_outer_radius_float, 150, FLT_MAX, this);
		if ( m_vortex_filter_handle.Get() )
		{
			if ( HasSpawnFlags(SF_PSTORM_NOPLAYERPULL) ) m_vortex_controller_handle->SetVortexPlayerPull(false);
			else m_vortex_controller_handle->SetVortexPlayerPull(true);
			m_vortex_controller_handle->SetVortexPullFilter(m_vortex_filter_handle);
		}
	}
}

void CNPC_ParticleStorm::InputStopVortex(inputdata_t &inputdata)
{
	if ( m_vortex_controller_handle )
	{
		UTIL_Remove(m_vortex_controller_handle);
	}
}

void CNPC_ParticleStorm::InputSetVortexInnerRadius(inputdata_t &inputdata)
{
	if ( m_vortex_controller_handle )
	{
		m_vortex_controller_handle->SetVortexInnerRadius(inputdata.value.Float());
	}
}

void CNPC_ParticleStorm::InputSetVortexOuterRadius(inputdata_t &inputdata)
{
	if ( m_vortex_controller_handle )
	{
		m_vortex_controller_handle->SetVortexOuterRadius(inputdata.value.Float());
	}
}

void CNPC_ParticleStorm::InputEnableProjectiles(inputdata_t &inputdata)
{
	m_enable_projectiles_bool = true;
}

void CNPC_ParticleStorm::InputDisableProjectiles(inputdata_t &inputdata)
{
	m_enable_projectiles_bool = false;
}

void CNPC_ParticleStorm::InputSetVortexFilter(inputdata_t &inputdata)
{
	CBaseEntity *pFilter = gEntList.FindEntityByName(NULL, inputdata.value.StringID());
	m_vortex_filter_handle = dynamic_cast<CBaseFilter*>( pFilter );

	// if already have a vortex, change its filter too
	if ( m_vortex_controller_handle && m_vortex_filter_handle.Get() != NULL )
	{
		m_vortex_controller_handle->SetVortexPullFilter(m_vortex_filter_handle);
	}
}

void CNPC_ParticleStorm::StopLoopingSounds(void)
{
	BaseClass::StopLoopingSounds();
}

bool CNPC_ParticleStorm::OverrideMove(float flInterval)
{
	//	return BaseClass::OverrideMove(flInterval);

	Vector vMoveTargetPos(0, 0, 0);
	CBaseEntity *pMoveTarget = NULL;

	// If I have a route, keep it updated and move toward target
	if ( GetNavigator()->IsGoalActive() )
	{
		if ( OverridePathMove(pMoveTarget, flInterval) )
		{
			return true;
		}
	}

	bool bFlyPathClear = false;

	// The original line of code was, due to the accidental use of '|' instead of
	// '&', always true. Replacing with 'true' to suppress the warning without changing
	// the (long-standing) behavior.
	if ( !GetNavigator()->IsGoalActive() || ( GetNavigator()->GetCurWaypointFlags() ) )
	{
		// Select move target 
		if ( GetTarget() != NULL )
		{
			pMoveTarget = GetTarget();
		} else if ( GetEnemy() != NULL )
		{
			pMoveTarget = GetEnemy();
		}

		// Select move target position 
		if ( GetEnemy() != NULL )
		{
			vMoveTargetPos = GetEnemy()->GetAbsOrigin();
		}
	} else
	{
		vMoveTargetPos = GetNavigator()->GetCurWaypointPos();
	}

	bFlyPathClear = false;

	// See if we can fly there directly
	if ( pMoveTarget )
	{
		trace_t tr;
		AI_TraceHull(GetAbsOrigin(), vMoveTargetPos, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

		float fTargetDist = ( 1.0f - tr.fraction )*( GetAbsOrigin() - vMoveTargetPos ).Length();

		if ( ( tr.m_pEnt == pMoveTarget ) || ( fTargetDist < 50 ) )
		{
			if ( npc_pstorm_debug.GetBool() )
			{
				NDebugOverlay::Line(GetLocalOrigin(), vMoveTargetPos, 0, 255, 0, true, 0);
				NDebugOverlay::Cross3D(tr.endpos, Vector(-5, -5, -5), Vector(5, 5, 5), 0, 255, 0, true, 0.1);
			}

			bFlyPathClear = true;
		} else
		{
			//HANDY DEBUG TOOL	
			if ( npc_pstorm_debug.GetBool() )
			{
				NDebugOverlay::Line(GetLocalOrigin(), vMoveTargetPos, 255, 0, 0, true, 0);
				NDebugOverlay::Cross3D(tr.endpos, Vector(-5, -5, -5), Vector(5, 5, 5), 255, 0, 0, true, 0.1);
			}

			bFlyPathClear = false;
		}
	}
	// ----------------------------------------------
	//	If attacking
	// ----------------------------------------------
	else if ( m_nFlyMode == SCANNER_FLY_ATTACK )
	{
		MoveToAttack(flInterval);
	}
	// -----------------------------------------------------------------
	// If I don't have a route, just decelerate
	// -----------------------------------------------------------------
	else if ( !GetNavigator()->IsGoalActive() )
	{
		float	myDecay = 9.5;
		Decelerate(flInterval, myDecay);
	}

	MoveExecute_Alive(flInterval);

	return true;
}

void CNPC_ParticleStorm::MoveToAttack(float flInterval)
{
	if ( GetEnemy() == NULL )
		return;

	if ( flInterval <= 0 )
		return;

	Vector vTargetPos = GetEnemyLKP();

	Vector idealPos = IdealGoalForMovement(vTargetPos, GetAbsOrigin(), GetGoalDistance(), m_flAttackNearDist);

	MoveToTarget(flInterval, idealPos);
}

void CNPC_ParticleStorm::MoveExecute_Alive(float flInterval)
{
	if ( GetTarget() ) Msg("move target is %s\n", GetTarget()->GetDebugName());

	if ( m_pstorm_dormant_bool || m_lifeState == LIFE_DYING )
	{
		SetCurrentVelocity(vec3_origin);
		StopLoopingSounds();
	}

	else
	{
		// Amount of noise to add to flying
		float noiseScale = 3.0f;

		SetCurrentVelocity(GetCurrentVelocity() + VelocityToAvoidObstacles(flInterval));

		IPhysicsObject *pPhysics = VPhysicsGetObject();

		if ( pPhysics && pPhysics->IsAsleep() )
		{
			pPhysics->Wake();
		}

		// Add time-coherent noise to the current velocity so that it never looks bolted in place.
		AddNoiseToVelocity(noiseScale);

		AdjustScannerVelocity();

		float maxSpeed = GetEnemy() ? ( GetMaxSpeed() * 2.0f ) : GetMaxSpeed();

		// Limit fall speed
		LimitSpeed(maxSpeed);

		// Blend out desired velocity when launched by the physcannon
		BlendPhyscannonLaunchSpeed();

		PlayFlySound();
	}
}

void CNPC_ParticleStorm::ClampMotorForces(Vector &linear, AngularImpulse &angular)
{
	if ( IsInAScript() )
	{
		linear.x = 0;
		linear.y = 0;
		linear.z = clamp(linear.z, -32, 32);
	} else
	{
		float zmodule = m_z_acceleration_float;
		linear.x = clamp(linear.x, -100, 100);
		linear.y = clamp(linear.y, -100, 100);
		linear.z = clamp(linear.z, -zmodule, zmodule);

		linear.z += ( GetFloorDistance(WorldSpaceCenter()) > m_z_threshold_float )
			? 0 : m_z_acceleration_float;
	}
}

//-----------------------------------------------------------------------------
// Save/restore
//-----------------------------------------------------------------------------
BEGIN_DATADESC(CNPC_ParticleStorm)
	DEFINE_FIELD(m_hPseudopodia, FIELD_EHANDLE),
	DEFINE_FIELD(m_bNoLight, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bDissipating, FIELD_BOOLEAN),
	DEFINE_FIELD(m_flDissipateTime, FIELD_TIME),
	DEFINE_FIELD(m_flFormTime, FIELD_TIME),
	DEFINE_FIELD(m_pstorm_dormant_bool, FIELD_BOOLEAN),
	DEFINE_FIELD(m_dynamiclightsColor, FIELD_VECTOR),
	DEFINE_FIELD(m_z_acceleration_float, FIELD_FLOAT),
	DEFINE_FIELD(m_z_threshold_float, FIELD_FLOAT),
	DEFINE_FIELD(m_vortex_controller_handle, FIELD_EHANDLE),
	DEFINE_FIELD(m_vortex_filter_handle, FIELD_EHANDLE),

	DEFINE_KEYFIELD(m_enabledlights_bool, FIELD_BOOLEAN, "enabledlights"),
	DEFINE_KEYFIELD(m_vortex_filter_name, FIELD_STRING, "vortexfilter"),
	DEFINE_KEYFIELD(m_vortex_inner_radius_float, FIELD_FLOAT, "VortexInnerRadius"),
	DEFINE_KEYFIELD(m_vortex_outer_radius_float, FIELD_FLOAT, "VortexOuterRadius"),

	DEFINE_INPUTFUNC(FIELD_VOID, "Dissipate", InputDissipate),
	DEFINE_INPUTFUNC(FIELD_VOID, "Form", InputForm),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetZAcceleration", InputSetZAccelerationFactor),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetAltitudeGoal", InputSetZThresholdFactor),
	DEFINE_INPUTFUNC(FIELD_VOID, "StartVortex", InputStartVortex),
	DEFINE_INPUTFUNC(FIELD_VOID, "StopVortex", InputStopVortex),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetVortexInnerRadius", InputSetVortexInnerRadius),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetVortexOuterRadius", InputSetVortexOuterRadius),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableProjectiles", InputEnableProjectiles),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableProjectiles", InputDisableProjectiles),
	DEFINE_INPUTFUNC(FIELD_STRING, "SetVortexFilter", InputSetVortexFilter),

	DEFINE_OUTPUT(m_OnFinishedForming, "OnFinishedForming"),
	DEFINE_OUTPUT(m_OnFinishedDissipation, "OnFinishedDissipation"),
	DEFINE_OUTPUT(m_OnPlayerCaughtInVortex, "OnPlayerCaught"),

	DEFINE_THINKFUNC(DynamicLightsThink),
END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_pstorm, CNPC_ParticleStorm);
IMPLEMENT_CUSTOM_AI(npc_pstorm, CNPC_ParticleStorm);
PRECACHE_REGISTER(npc_pstorm);

//=========================================================
// > SCHED_CSCANNER_PATROL
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_PSTORM_ATTACK_HOVER,

	"	Tasks"
	"		TASK_SCANNER_SET_FLY_ATTACK			0"
	"		TASK_SET_ACTIVITY					ACTIVITY:ACT_WALK  "
	"		TASK_WAIT							1"
	""
	"	Interrupts"
	"		COND_CSCANNER_SPOT_ON_TARGET"
	"		COND_CSCANNER_INSPECT_DONE"
	"		COND_SCANNER_FLY_BLOCKED"
	"		COND_NEW_ENEMY"
);

//=========================================================
// > SCHED_CSCANNER_ATTACK_FLASH
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_PSTORM_PROJECTILE_ATTACK,

	"	Tasks"
	"		TASK_SCANNER_SET_FLY_ATTACK			0"
	"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"
	"		TASK_PSTORM_PROJECTILE_ATTACK		0"
	"		TASK_WAIT							0.5"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
);
#endif // PSTORM

#include "tier0\memdbgoff.h"