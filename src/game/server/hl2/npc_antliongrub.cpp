//========================================================================//
//
// Purpose:		Antlion Grub - cannon fodder
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#ifdef DARKINTERVAL
// Beta includes
#include "AI_BaseNPC.h"
#include "soundenvelope.h"
#include "bitstring.h"
#include "activitylist.h"
#include "game.h"
#include "items.h"
#include "item_dynamic_resupply.h"
#include "NPCEvent.h"
#include "ai_senses.h"
#include "Player.h"
#include "EntityList.h"
#include "AI_Interactions.h"
#include "soundent.h"
#include "Gib.h"
#include "materialsystem/imesh.h"
#include "particle_parse.h"
#include "soundenvelope.h"
#include "env_shake.h"
#include "sprite.h"
#include "vstdlib/random.h"
#include "engine/ienginesound.h"
#include "npc_antliongrub.h"
#else
// Episodic includes
#include "gib.h"
#include "sprite.h"
#include "te_effect_dispatch.h"
#include "npc_antliongrub.h"
#include "ai_utils.h"
#include "particle_parse.h"
#include "items.h"
#include "item_dynamic_resupply.h"
#endif // DARKINTERVAL

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef DARKINTERVAL
int	g_interactionAntlionGrubAlert = 0;
#endif

class CSprite;

ConVar	sk_grubnugget_health_small("sk_grubnugget_health_small", "1");
ConVar	sk_grubnugget_health_medium("sk_grubnugget_health_medium", "4");
ConVar	sk_grubnugget_health_large("sk_grubnugget_health_large", "6");
ConVar	sk_grubnugget_enabled("sk_grubnugget_enabled", "1");

enum
{
	NUGGET_NONE,
	NUGGET_SMALL = 1,
	NUGGET_MEDIUM,
	NUGGET_LARGE
};

//
//  Grub nugget
//

class CGrubNugget : public CItem
{
public:
	DECLARE_CLASS(CGrubNugget, CItem);

	virtual void Spawn(void);
	virtual void Precache(void);
	virtual void VPhysicsCollision(int index, gamevcollisionevent_t *pEvent);
	virtual void Event_Killed(const CTakeDamageInfo &info);
	virtual bool VPhysicsIsFlesh(void);

	bool	MyTouch(CBasePlayer *pPlayer);
	void	SetDenomination(int nSize) { Assert(nSize <= NUGGET_LARGE && nSize >= NUGGET_SMALL); m_nDenomination = nSize; }

	DECLARE_DATADESC();

private:
	int		m_nDenomination;	// Denotes size and health amount given
};

BEGIN_DATADESC(CGrubNugget)
DEFINE_FIELD(m_nDenomination, FIELD_INTEGER),
END_DATADESC()

LINK_ENTITY_TO_CLASS(item_grubnugget, CGrubNugget);
#ifdef DARKINTERVAL
#define	ANTLIONGRUB_MODEL				"models/_Monsters/Antlions/antlion_grub.mdl"

#define	SF_ANTLIONGRUB_STATIC	(1<<16) // if a beta grub does have it, it'll behave normally. Otherwise it'll be the Ep2 grub, static

class CNPC_AntlionGrub : public CAI_BaseNPC
{
public:
	DECLARE_CLASS(CNPC_AntlionGrub, CAI_BaseNPC);
	DECLARE_DATADESC();

	CNPC_AntlionGrub(void);
	~CNPC_AntlionGrub(void);

	bool			IsValidEnemy(CBaseEntity *pEnemy);
	virtual int		SelectSchedule(void);
	virtual int		TranslateSchedule(int type);
	int				MeleeAttack1Conditions(float flDot, float flDist);

	bool			AllowedToIgnite(void) { return true; }
	bool			AllowedToIgniteByFlare(void) { return true; }

	bool			IsLightDamage(float flDamage, int bitsDamageType) { return (flDamage > 0.0f); }
	bool			HandleInteraction(int interactionType, void *data, CBaseCombatCharacter *sourceEnt);

	void			Precache(void);
	void			Spawn(void);
	void			UpdateOnRemove(void);
protected:
	void			MakeIdleSounds(void);
	void			MakeSquashDecals(const Vector &vecOrigin);
	void			AttachToSurface(void);
	void			CreateGlow(void);
	void			FadeGlow(void);
	inline bool		ProbeSurface(const Vector &vecTestPos, const Vector &vecDir, Vector *vecResult, Vector *vecNormal);
public:
	void			StartTask(const Task_t *pTask);
	void			RunTask(const Task_t *pTask);
	void			GrubTouch(CBaseEntity *pOther);
	void			EndTouch(CBaseEntity *pOther);
	void			StickyTouch(CBaseEntity *pOther);
	void			IdleSound(void);
	void			PainSound(const CTakeDamageInfo &info);
	void			PrescheduleThink(void);
	void			HandleAnimEvent(animevent_t *pEvent);
	void			Event_Killed(const CTakeDamageInfo &info);
	void			TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr);
	void			BuildScheduleTestBits(void);
	void			StopLoopingSounds();
	
	virtual bool	CanStandOn(CBaseEntity *pSurface) const;

	float			MaxYawSpeed(void) { return HasSpawnFlags(SF_ANTLIONGRUB_STATIC) ? 0 : 4.0f; }
	float			StepHeight() const { return 12.0f; }	//NOTENOTE: We don't want them to move up too high

	Class_T			Classify(void) { return CLASS_ANTLION; }

	Activity		GetFollowActivity(float flDistance) { return ACT_RUN; }

	void			InputSquash(inputdata_t &data);

	bool			m_createnugget_bool; // if a beta grub does have it, it'll spawn the healing nugget on death
	float			m_broadcast_radius_float; // the range in which grubs alert other grubs and antlions

private:
	void			Squash(CBaseEntity *pOther);
	void			BroadcastAlert(void);
	int				GetNuggetDenomination(void);
	void			CreateNugget(void);

	CSoundPatch		*m_pMovementSound;
	CSoundPatch		*m_pVoiceSound;
	CSoundPatch		*m_pHealSound;

	CHandle<CSprite> m_hGlowSprite;

	float			m_flNextVoiceChange;	//The next point to alter our voice
	float			m_flSquashTime;			//Amount of time we've been stepped on
	float			m_flNearTime;			//Amount of time the player has been near enough to be considered for healing
	float			m_flHealthTime;			//Amount of time until the next piece of health is given
	float			m_flEnemyHostileTime;	//Amount of time the enemy should be avoided (because they displayed aggressive behavior)

	bool			m_bMoving;
	bool			m_bSquashed;
	bool			m_bSquashValid;
	bool			m_bHealing;

	int				m_nHealthReserve;
	int				m_nGlowSpriteHandle;

	DEFINE_CUSTOM_AI;
};

ConVar	sk_antliongrub_health("sk_antliongrub_health", "15");

#define	ANTLIONGRUB_SQUASH_TIME			1.0f
#define	ANTLIONGRUB_HEAL_RANGE			256.0f
#define	ANTLIONGRUB_HEAL_CONE			0.5f
#define	ANTLIONGRUB_ENEMY_HOSTILE_TIME	8.0f

//Sharp fast attack
envelopePoint_t envFastAttack[] =
{
	{ 0.3f, 0.5f,	//Attack
	0.5f, 1.0f,
	},
	{ 0.0f, 0.1f, //Decay
	0.1f, 0.2f,
	},
	{ -1.0f, -1.0f, //Sustain
	1.0f, 2.0f,
	},
	{ 0.0f, 0.0f,	//Release
	0.5f, 1.0f,
	}
};

//Slow start to fast end attack
envelopePoint_t envEndAttack[] =
{
	{ 0.0f, 0.1f,
	0.5f, 1.0f,
	},
	{ -1.0f, -1.0f,
	0.5f, 1.0f,
	},
	{ 0.3f, 0.5f,
	0.2f, 0.5f,
	},
	{ 0.0f, 0.0f,
	0.5f, 1.0f,
	},
};

//rise, long sustain, release
envelopePoint_t envMidSustain[] =
{
	{ 0.2f, 0.4f,
	0.1f, 0.5f,
	},
	{ -1.0f, -1.0f,
	0.1f, 0.5f,
	},
	{ 0.0f, 0.0f,
	0.5f, 1.0f,
	},
};

//Scared
envelopePoint_t envScared[] =
{
	{ 0.8f, 1.0f,
	0.1f, 0.2f,
	},
	{
		-1.0f, -1.0f,
		0.25f, 0.5f
	},
	{ 0.0f, 0.0f,
	0.5f, 1.0f,
	},
};

//Grub voice envelopes
envelopeDescription_t grubVoiceEnvelopes[] =
{
	{ envFastAttack,	ARRAYSIZE(envFastAttack) },
	{ envEndAttack,		ARRAYSIZE(envEndAttack) },
	{ envMidSustain,	ARRAYSIZE(envMidSustain) },
};

//Data description
BEGIN_DATADESC(CNPC_AntlionGrub)

DEFINE_SOUNDPATCH(m_pMovementSound),
DEFINE_SOUNDPATCH(m_pVoiceSound),
DEFINE_SOUNDPATCH(m_pHealSound),
DEFINE_FIELD(m_hGlowSprite, FIELD_EHANDLE),
DEFINE_FIELD(m_flNextVoiceChange, FIELD_TIME),
DEFINE_FIELD(m_flSquashTime, FIELD_TIME),
DEFINE_FIELD(m_flNearTime, FIELD_TIME),
DEFINE_FIELD(m_flHealthTime, FIELD_TIME),
DEFINE_FIELD(m_flEnemyHostileTime, FIELD_TIME),
DEFINE_FIELD(m_bSquashed, FIELD_BOOLEAN),
DEFINE_FIELD(m_bMoving, FIELD_BOOLEAN),
DEFINE_FIELD(m_bSquashValid, FIELD_BOOLEAN),
DEFINE_FIELD(m_bHealing, FIELD_BOOLEAN),
DEFINE_FIELD(m_nHealthReserve, FIELD_INTEGER),
DEFINE_FIELD(m_nGlowSpriteHandle, FIELD_INTEGER),
DEFINE_KEYFIELD(m_broadcast_radius_float, FIELD_FLOAT, "broadcastradius"),

DEFINE_KEYFIELD( m_createnugget_bool, FIELD_BOOLEAN, "CreateNugget"),

// Functions
DEFINE_ENTITYFUNC( GrubTouch),

// Inputs
DEFINE_INPUTFUNC(FIELD_VOID, "Squash", InputSquash),

END_DATADESC()

//Schedules
enum AntlionGrubSchedules
{
	SCHED_ANTLIONGRUB_SQUEAL = LAST_SHARED_SCHEDULE,
	SCHED_ANTLIONGRUB_SQUIRM,
	SCHED_ANTLIONGRUB_STAND,
	SCHED_ANTLIONGRUB_GIVE_HEALTH,
	SCHED_ANTLIONGUARD_RETREAT,
};

//Tasks
enum AntlionGrubTasks
{
	TASK_ANTLIONGRUB_SQUIRM = LAST_SHARED_TASK,
	TASK_ANTLIONGRUB_GIVE_HEALTH,
	TASK_ANTLIONGRUB_MOVE_TO_TARGET,
	TASK_ANTLIONGRUB_FIND_RETREAT_GOAL,
};

//Conditions
enum AntlionGrubConditions
{
	COND_ANTLIONGRUB_HEARD_SQUEAL = LAST_SHARED_CONDITION,
	COND_ANTLIONGRUB_BEING_SQUASHED,
	COND_ANTLIONGRUB_IN_HEAL_RANGE,
};

//Activities
int	ACT_ANTLIONGRUB_SQUIRM;
int ACT_ANTLIONGRUB_HEAL;

//Animation events
#define ANTLIONGRUB_AE_START_SQUIRM	11	//Start squirming portion of animation
#define ANTLIONGRUB_AE_END_SQUIRM	12	//End squirming portion of animation

#define	REGISTER_INTERACTION( a )	{ a = CBaseCombatCharacter::GetInteractionID(); }

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_AntlionGrub::CNPC_AntlionGrub(void)
{
	m_hGlowSprite = NULL;
	m_createnugget_bool = true; // default to true so Ep2 grubs w/o override can spawn nuggets
	m_broadcast_radius_float = 512;
}

CNPC_AntlionGrub::~CNPC_AntlionGrub(void)
{
	if (m_hGlowSprite != NULL) UTIL_Remove(m_hGlowSprite);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::InitCustomSchedules(void)
{
	INIT_CUSTOM_AI(CNPC_AntlionGrub);

	//Schedules
	ADD_CUSTOM_SCHEDULE(CNPC_AntlionGrub, SCHED_ANTLIONGRUB_SQUEAL);
	ADD_CUSTOM_SCHEDULE(CNPC_AntlionGrub, SCHED_ANTLIONGRUB_SQUIRM);
	ADD_CUSTOM_SCHEDULE(CNPC_AntlionGrub, SCHED_ANTLIONGRUB_STAND);
	ADD_CUSTOM_SCHEDULE(CNPC_AntlionGrub, SCHED_ANTLIONGRUB_GIVE_HEALTH);
	ADD_CUSTOM_SCHEDULE(CNPC_AntlionGrub, SCHED_ANTLIONGUARD_RETREAT);

	//Tasks
	ADD_CUSTOM_TASK(CNPC_AntlionGrub, TASK_ANTLIONGRUB_SQUIRM);
	ADD_CUSTOM_TASK(CNPC_AntlionGrub, TASK_ANTLIONGRUB_GIVE_HEALTH);
	ADD_CUSTOM_TASK(CNPC_AntlionGrub, TASK_ANTLIONGRUB_MOVE_TO_TARGET);
	ADD_CUSTOM_TASK(CNPC_AntlionGrub, TASK_ANTLIONGRUB_FIND_RETREAT_GOAL);

	//Conditions
	ADD_CUSTOM_CONDITION(CNPC_AntlionGrub, COND_ANTLIONGRUB_HEARD_SQUEAL);
	ADD_CUSTOM_CONDITION(CNPC_AntlionGrub, COND_ANTLIONGRUB_BEING_SQUASHED);
	ADD_CUSTOM_CONDITION(CNPC_AntlionGrub, COND_ANTLIONGRUB_IN_HEAL_RANGE);

	//Activities
	ADD_CUSTOM_ACTIVITY(CNPC_AntlionGrub, ACT_ANTLIONGRUB_SQUIRM);
	ADD_CUSTOM_ACTIVITY(CNPC_AntlionGrub, ACT_ANTLIONGRUB_HEAL);

	AI_LOAD_SCHEDULE(CNPC_AntlionGrub, SCHED_ANTLIONGRUB_SQUEAL);
	AI_LOAD_SCHEDULE(CNPC_AntlionGrub, SCHED_ANTLIONGRUB_SQUIRM);
	AI_LOAD_SCHEDULE(CNPC_AntlionGrub, SCHED_ANTLIONGRUB_STAND);
	AI_LOAD_SCHEDULE(CNPC_AntlionGrub, SCHED_ANTLIONGRUB_GIVE_HEALTH);
	AI_LOAD_SCHEDULE(CNPC_AntlionGrub, SCHED_ANTLIONGUARD_RETREAT);
}

LINK_ENTITY_TO_CLASS(npc_antlion_grub, CNPC_AntlionGrub);
IMPLEMENT_CUSTOM_AI(npc_antlion_grub, CNPC_AntlionGrub);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::Precache(void)
{
	engine->PrecacheModel(ANTLIONGRUB_MODEL);

	m_nGlowSpriteHandle = PrecacheModel("sprites/grubflare1.vmt");

	PrecacheScriptSound("NPC_Antlion_Grub.Idle");
	PrecacheScriptSound("NPC_Antlion_Grub.Alert");
	PrecacheScriptSound("NPC_Antlion_Grub.Stimulated");
	PrecacheScriptSound("NPC_Antlion_Grub.Die");
	PrecacheScriptSound("NPC_Antlion_Grub.Squish");

	enginesound->PrecacheSound("npc/antlion_grub/movement.wav");
	enginesound->PrecacheSound("npc/antlion_grub/voice.wav");
	enginesound->PrecacheSound("npc/antlion_grub/heal.wav");

	if(m_createnugget_bool) UTIL_PrecacheOther("item_grubnugget");

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::Spawn(void)
{
	Precache();
	
	SetModel(ANTLIONGRUB_MODEL);

	CreateGlow();

	SetHullType(HULL_TINY);

	Vector	vecMins, vecMaxs;

	vecMins = GetHullMins();
	vecMaxs = GetHullMaxs();

	//Just the perfect size for being stepped on
	vecMaxs[2] -= 14.0f;

	UTIL_SetSize(this, vecMins, vecMaxs);

	SetNavType(NAV_GROUND);
	m_NPCState = NPC_STATE_NONE;
	SetBloodColor(BLOOD_COLOR_YELLOW);
	m_iHealth = sk_antliongrub_health.GetFloat();
	m_iMaxHealth = m_iHealth;
	m_flFieldOfView = -1.0f;
//	m_flModelScale = random->RandomFloat(0.1f, 1.0f);

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	
	CapabilitiesClear();

	bool isEp2 = false;
	char szMapName[256];
	Q_strncpy(szMapName, STRING(gpGlobals->mapname), sizeof(szMapName));
	Q_strlower(szMapName);
	if (!Q_strnicmp(szMapName, "ep2", 3)) isEp2 = true;

	if (!HasSpawnFlags(SF_ANTLIONGRUB_STATIC) 
		|| (HasSpawnFlags(1<<0) && isEp2) == true)
	{
		SetMoveType(MOVETYPE_STEP);
		CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1);
	}
	else // Stick to the nearest surface		
	{
		SetMoveType(MOVETYPE_NONE);
		AddSpawnFlags(SF_NPC_START_EFFICIENT);
		GetSenses()->AddSensingFlags(SENSING_FLAGS_DONT_LISTEN | SENSING_FLAGS_DONT_LOOK);
		AttachToSurface();
	}

	m_flNextVoiceChange = CURTIME;
	m_flSquashTime = CURTIME;
	m_flNearTime = CURTIME;
	m_flHealthTime = CURTIME;
	m_flEnemyHostileTime = CURTIME;

	m_bMoving = false;
	m_bSquashed = false;
	m_bSquashValid = false;
	m_bHealing = false;

	m_nHealthReserve = 10;

	SetTouch(&CNPC_AntlionGrub::GrubTouch);

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	CPASAttenuationFilter filter(this);
	m_pMovementSound = controller.SoundCreate(filter, entindex(), CHAN_BODY, "npc/antlion_grub/movement.wav", 3.9f);
	m_pVoiceSound = controller.SoundCreate(filter, entindex(), CHAN_VOICE, "npc/antlion_grub/voice.wav", 3.9f);
	m_pHealSound = controller.SoundCreate(filter, entindex(), CHAN_STATIC, "npc/antlion_grub/heal.wav", 3.9f);

	controller.Play(m_pMovementSound, 0.0f, 100);
	controller.Play(m_pVoiceSound, 0.0f, 100);
	controller.Play(m_pHealSound, 0.0f, 100);
	
	NPCInit();

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------

bool CNPC_AntlionGrub::CanStandOn(CBaseEntity *pSurface) const
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecTestPos - 
//			*vecResult - 
//			*flDist - 
// Output : inline bool
//-----------------------------------------------------------------------------
inline bool CNPC_AntlionGrub::ProbeSurface(const Vector &vecTestPos, const Vector &vecDir, Vector *vecResult, Vector *vecNormal)
{
	// Trace down to find a surface
	trace_t tr;
	UTIL_TraceLine(vecTestPos, vecTestPos + (vecDir*256.0f), MASK_NPCSOLID&(~CONTENTS_MONSTER), this, COLLISION_GROUP_NONE, &tr);

	if (vecResult)
	{
		*vecResult = tr.endpos;
	}

	if (vecNormal)
	{
		*vecNormal = tr.plane.normal;
	}

	return (tr.fraction < 1.0f);
}

//-----------------------------------------------------------------------------
// Purpose: Attaches the grub to the surface underneath its abdomen
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::AttachToSurface(void)
{
	// Get our downward direction
	Vector vecForward, vecRight, vecDown;
	GetVectors(&vecForward, &vecRight, &vecDown);
	vecDown.Negate();

	Vector vecOffset = (vecDown * -8.0f);

	// Middle
	Vector vecMid, vecMidNormal;
	if (ProbeSurface(WorldSpaceCenter() + vecOffset, vecDown, &vecMid, &vecMidNormal) == false)
	{
		// A grub was left hanging in the air, it must not be near any valid surfaces!
		Warning("Antlion grub stranded in space at (%.02f, %.02f, %.02f) : REMOVED\n", GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
		UTIL_Remove(this);
		return;
	}

	// Sit at the mid-point
	UTIL_SetOrigin(this, vecMid);

	Vector vecPivot;
	Vector vecPivotNormal;

	bool bNegate = true;

	// First test our tail (more crucial that it doesn't interpenetrate with the world)
	if (ProbeSurface(WorldSpaceCenter() - (vecForward * 12.0f) + vecOffset, vecDown, &vecPivot, &vecPivotNormal) == false)
	{
		// If that didn't find a surface, try the head
		if (ProbeSurface(WorldSpaceCenter() + (vecForward * 12.0f) + vecOffset, vecDown, &vecPivot, &vecPivotNormal) == false)
		{
			// Worst case, just site at the middle
			UTIL_SetOrigin(this, vecMid);

			QAngle vecAngles;
			VectorAngles(vecForward, vecMidNormal, vecAngles);
			SetAbsAngles(vecAngles);
			return;
		}

		bNegate = false;
	}

	// Find the line we'll lay on if these two points are connected by a line
	Vector vecLieDir = (vecPivot - vecMid);
	VectorNormalize(vecLieDir);
	if (bNegate)
	{
		// We need to try and maintain our facing
		vecLieDir.Negate();
	}

	// Use the average of the surface normals to be our "up" direction
	Vector vecPseudoUp = (vecMidNormal + vecPivotNormal) * 0.5f;

	QAngle vecAngles;
	VectorAngles(vecLieDir, vecPseudoUp, vecAngles);

	SetAbsAngles(vecAngles);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::CreateGlow(void)
{
	// Create the glow sprite
	m_hGlowSprite = CSprite::SpriteCreate("sprites/grubflare1.vmt", GetLocalOrigin(), false);
	Assert(m_hGlowSprite);
	if (m_hGlowSprite == NULL)
		return;

	m_hGlowSprite->TurnOn();
	m_hGlowSprite->SetTransparency(kRenderWorldGlow, 156, 169, 121, 164, kRenderFxNoDissipation);
	m_hGlowSprite->SetHDRColorScale(0.5);
	m_hGlowSprite->SetScale(1.0f);
	m_hGlowSprite->SetGlowProxySize(16.0f);
	int nAttachment = LookupAttachment("glow");
	m_hGlowSprite->SetParent(this, nAttachment);
	m_hGlowSprite->SetLocalOrigin(vec3_origin);

	// Don't uselessly animate, we're a static sprite!
	m_hGlowSprite->SetThink(NULL);
	m_hGlowSprite->SetNextThink(TICK_NEVER_THINK);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::FadeGlow(void)
{
	if (m_hGlowSprite)
	{
		m_hGlowSprite->FadeAndDie(0.25f);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::UpdateOnRemove(void)
{
	FadeGlow();

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::HandleAnimEvent(animevent_t *pEvent)
{
	switch (pEvent->event)
	{
	case ANTLIONGRUB_AE_START_SQUIRM:
	{
		if (!m_bSquashed && m_lifeState == LIFE_ALIVE)
		{
			float duration = random->RandomFloat(0.1f, 0.3f);

			CSoundEnvelopeController::GetController().SoundChangePitch(m_pMovementSound, random->RandomInt(100, 120), duration);
			CSoundEnvelopeController::GetController().SoundChangeVolume(m_pMovementSound, random->RandomFloat(0.6f, 0.8f), duration);
		}
	}
	break;

	case ANTLIONGRUB_AE_END_SQUIRM:
	{
		if (!m_bSquashed && m_lifeState == LIFE_ALIVE)
		{
			float duration = random->RandomFloat(0.1f, 0.3f);

			CSoundEnvelopeController::GetController().SoundChangePitch(m_pMovementSound, random->RandomInt(80, 100), duration);
			CSoundEnvelopeController::GetController().SoundChangeVolume(m_pMovementSound, random->RandomFloat(0.0f, 0.1f), duration);
		}
	}
	break;

	default:
		BaseClass::HandleAnimEvent(pEvent);
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_AntlionGrub::IsValidEnemy(CBaseEntity *pEnemy)
{
	if (HasSpawnFlags(SF_ANTLIONGRUB_STATIC))
	{
		return false;
	}

	return BaseClass::IsValidEnemy(pEnemy);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_AntlionGrub::SelectSchedule(void)
{
	//Don't do anything if we're dead
	if (m_NPCState == NPC_STATE_DEAD)
		return BaseClass::SelectSchedule();

	if (HasSpawnFlags(SF_ANTLIONGRUB_STATIC))
	{
		return SCHED_IDLE_STAND;
	}

	//If we've heard someone else squeal, we should too
	if (HasCondition(COND_ANTLIONGRUB_HEARD_SQUEAL) || HasCondition(COND_ANTLIONGRUB_BEING_SQUASHED))
	{
		m_flEnemyHostileTime = CURTIME + ANTLIONGRUB_ENEMY_HOSTILE_TIME;
		EmitSound("NPC_Antlion_Grub.Stimulated");
		return SCHED_SMALL_FLINCH;
	}

	//See if we need to run away from our enemy
	if (m_flEnemyHostileTime > CURTIME)
		return SCHED_ANTLIONGUARD_RETREAT;

	if (HasCondition(COND_ANTLIONGRUB_IN_HEAL_RANGE))
	{
		SetTarget(GetEnemy());
		return SCHED_ANTLIONGRUB_GIVE_HEALTH;
	}

	//If we've taken any damage, squirm and squeal
	if (HasCondition(COND_LIGHT_DAMAGE) && SelectWeightedSequence(ACT_SMALL_FLINCH) != -1)
		return SCHED_SMALL_FLINCH;

	//Randomly stand still
	if (random->RandomInt(0, 3) == 0)
		return SCHED_IDLE_STAND;

	//Otherwise just walk around a little
	return SCHED_PATROL_WALK;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_ANTLIONGRUB_FIND_RETREAT_GOAL:
	{
		if (GetEnemy() == NULL)
		{
			TaskFail(FAIL_NO_ENEMY);
			return;
		}

		Vector	testPos, testPos2, threatDir;
		trace_t	tr;

		//Find the direction to our enemy
		threatDir = (GetAbsOrigin() - GetEnemy()->GetAbsOrigin());
		VectorNormalize(threatDir);

		//Find a position farther out away from our enemy
		VectorMA(GetAbsOrigin(), random->RandomInt(32, 128), threatDir, testPos);
		testPos[2] += StepHeight()*2.0f;

		testPos2 = testPos;
		testPos2[2] -= StepHeight()*2.0f;

		//Check the position
		AI_TraceLine(testPos, testPos2, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);

		//Must be clear
		if ((tr.startsolid) || (tr.allsolid) || (tr.fraction == 1.0f))
		{
			TaskFail(FAIL_NO_ROUTE);
			return;
		}

		//Save the position and go
		m_vSavePosition = tr.endpos;
		TaskComplete();
	}
	break;

	case TASK_ANTLIONGRUB_MOVE_TO_TARGET:

		if (GetEnemy() == NULL)
		{
			TaskFail(FAIL_NO_TARGET);
		}
		else if ((GetEnemy()->GetLocalOrigin() - GetLocalOrigin()).Length() < pTask->flTaskData)
		{
			TaskComplete();
		}

		break;

	case TASK_ANTLIONGRUB_GIVE_HEALTH:

		m_bHealing = true;

		SetActivity((Activity)ACT_ANTLIONGRUB_HEAL);

		if (!m_bSquashed && m_lifeState == LIFE_ALIVE)
		{
			CSoundEnvelopeController::GetController().SoundChangeVolume(m_pHealSound, 0.5f, 2.0f);
		}

		//Must have a target
		if (GetEnemy() == NULL)
		{
			TaskFail(FAIL_NO_ENEMY);
			return;
		}

		//Must be within range
		if ((GetEnemy()->GetLocalOrigin() - GetLocalOrigin()).Length() > 92)
		{
			TaskFail(FAIL_NO_ENEMY);
		}

		break;

	case TASK_ANTLIONGRUB_SQUIRM:
	{
		EmitSound("NPC_Antlion_Grub.Idle");

		//Pick a squirm movement to perform
		Vector	vecStart;

		//Move randomly around, and start a step's height above our current position
		vecStart.Random(-32.0f, 32.0f);
		vecStart[2] = StepHeight();
		vecStart += GetLocalOrigin();

		//Look straight down for the ground
		Vector	vecEnd = vecStart;
		vecEnd[2] -= StepHeight()*2.0f;

		trace_t	tr;

		//Check the position
		//FIXME: Trace by the entity's hull size?
		AI_TraceLine(vecStart, vecEnd, MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

		//See if we can move there
		if ((tr.fraction == 1.0f) || (tr.startsolid) || (tr.allsolid))
		{
			TaskFail(FAIL_NO_ROUTE);
			return;
		}

		m_vSavePosition = tr.endpos;

		TaskComplete();
	}
	break;

	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_ANTLIONGRUB_MOVE_TO_TARGET:
	{
		//Must have a target entity
		if (GetEnemy() == NULL)
		{
			TaskFail(FAIL_NO_TARGET);
			return;
		}

		float distance = (GetNavigator()->GetGoalPos() - GetLocalOrigin()).Length2D();

		if ((GetNavigator()->GetGoalPos() - GetEnemy()->GetLocalOrigin()).Length() > (pTask->flTaskData * 0.5f))
		{
			distance = (GetEnemy()->GetLocalOrigin() - GetLocalOrigin()).Length2D();
			GetNavigator()->UpdateGoalPos(GetEnemy()->GetLocalOrigin());
		}

		//See if we've arrived
		if (distance < pTask->flTaskData)
		{
			TaskComplete();
			GetNavigator()->ClearGoal();
		}
	}
	break;

	case TASK_ANTLIONGRUB_GIVE_HEALTH:

		//Validate the enemy
		if (GetEnemy() == NULL)
		{
			TaskFail(FAIL_NO_ENEMY);
			return;
		}

		//Are we done giving health?
		if ((GetEnemy()->m_iHealth == GetEnemy()->m_iMaxHealth) || (m_nHealthReserve <= 0) || ((GetEnemy()->GetLocalOrigin() - GetLocalOrigin()).Length() > 64))
		{
			m_bHealing = false;
			if (!m_bSquashed && m_lifeState == LIFE_ALIVE)
			{
				CSoundEnvelopeController::GetController().SoundChangeVolume(m_pHealSound, 0.0f, 0.5f);
			}
			TaskComplete();
			return;
		}

		//Is it time to heal again?
		if (m_flHealthTime < CURTIME)
		{
			m_flHealthTime = CURTIME + 0.5f;

			//Update the health
			if (GetEnemy()->m_iHealth < GetEnemy()->m_iMaxHealth)
			{
				GetEnemy()->m_iHealth++;
				m_nHealthReserve--;
			}
		}

		break;

	default:
		BaseClass::RunTask(pTask);
		break;
	}
}

#define TRANSLATE_SCHEDULE( type, in, out ) { if ( type == in ) return out; }

//-----------------------------------------------------------------------------
// Purpose: override/translate a schedule by type
// Input  : Type - schedule type
// Output : int - translated type
//-----------------------------------------------------------------------------
int CNPC_AntlionGrub::TranslateSchedule(int type)
{
	TRANSLATE_SCHEDULE(type, SCHED_IDLE_STAND, SCHED_ANTLIONGRUB_STAND);
	TRANSLATE_SCHEDULE(type, SCHED_PATROL_WALK, SCHED_ANTLIONGRUB_SQUIRM);

	return BaseClass::TranslateSchedule(type);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::GrubTouch(CBaseEntity *pOther)
{
	//Don't consider the world
	if (FClassnameIs(pOther, "worldspawn"))
		return;

	//Allow our allies to touch us
	if (pOther->Classify() == this->Classify() || pOther->Classify() == CLASS_ANTLION)
		return; // note: for the moment, the grubs are CLASS_ANTLION too. But what if that changes in the future? 

	//Allow a crusing velocity to kill them in one go (or they're already dead)
	if ((pOther->GetAbsVelocity()[2] < -200) || (IsAlive() == false))
	{
		Squash(pOther);
		return;
	}

	//Otherwise, the pressure must be maintained to squash
	if (m_bSquashValid == false)
	{
		//Shake the screen
		UTIL_ScreenShake(GetAbsOrigin(), 0.01f, 50.0f, 0.1f, 128, SHAKE_START);

		m_bSquashValid = true;
		m_flSquashTime = CURTIME + ANTLIONGRUB_SQUASH_TIME;

		if (!m_bSquashed && m_lifeState == LIFE_ALIVE)
		{
			CSoundEnvelopeController::GetController().CommandClear(m_pVoiceSound);
			CSoundEnvelopeController::GetController().SoundChangePitch(m_pVoiceSound, 150, ANTLIONGRUB_SQUASH_TIME*0.5f);
			CSoundEnvelopeController::GetController().SoundChangeVolume(m_pVoiceSound, 1.0f, 0.5f);
		}
	}
	else
	{
		//Wiggle faster the longer the player is squashing us
		m_flPlaybackRate = 3.0f * (1.0f - (m_flSquashTime - CURTIME));

		//Shake the screen for effect
		UTIL_ScreenShake(GetAbsOrigin(), 0.05f, 0.1f, 0.5f, 128, SHAKE_START);

		//Taken enough damage?
		if (m_flSquashTime < CURTIME)
		{
			EmitSound("NPC_Antlion_Grub.Alert");
			Squash(pOther);
			return;
		}
	}

	//Need to know we're being squashed
	SetCondition(COND_ANTLIONGRUB_BEING_SQUASHED);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::EndTouch(CBaseEntity *pOther)
{
	if (!m_bSquashed )
	{
		ClearCondition(COND_ANTLIONGRUB_BEING_SQUASHED);

		m_bSquashValid = false;

		if (!m_bSquashed && m_lifeState == LIFE_ALIVE)
		{
			CSoundEnvelopeController::GetController().SoundChangePitch(m_pVoiceSound, 100, 0.5f);
			CSoundEnvelopeController::GetController().SoundChangeVolume(m_pVoiceSound, 0.0f, 1.0f);
		}

		m_flPlaybackRate = 1.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::BroadcastAlert(void)
{
	CBaseEntity *pEntity = NULL;
	CAI_BaseNPC *pNPC;

	//Look in a radius for potential listeners
	for (CEntitySphereQuery sphere(GetAbsOrigin(), m_broadcast_radius_float); pEntity = sphere.GetCurrentEntity(); sphere.NextEntity())
	{
		if (!(pEntity->GetFlags() & FL_NPC))
			continue;

		pNPC = pEntity->MyNPCPointer();

		//Only antlions care
//		if (pNPC->ClassMatches(GetClassname()) || pNPC->ClassMatches("npc_antlion"))
		{
			if (pNPC->HandleInteraction(g_interactionAntlionGrubAlert, NULL, this)) {};
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find what size of nugget to spawn
//-----------------------------------------------------------------------------
int CNPC_AntlionGrub::GetNuggetDenomination(void)
{
	// Find the desired health perc we want to be at
	float flDesiredHealthPerc = DynamicResupply_GetDesiredHealthPercentage();

	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if (pPlayer == NULL)
		return -1;

	// Get the player's current health percentage
	float flPlayerHealthPerc = (float)pPlayer->GetHealth() / (float)pPlayer->GetMaxHealth();

	// If we're already maxed out, return the small nugget
	if (flPlayerHealthPerc >= flDesiredHealthPerc)
	{
		return NUGGET_SMALL;
	}

	// Find where we fall in the desired health's range
	float flPercDelta = flPlayerHealthPerc / flDesiredHealthPerc;

	// The larger to discrepancy, the higher the chance to move quickly to close it
	float flSeed = random->RandomFloat(0.0f, 1.0f);
	float flRandomPerc = Bias(flSeed, (1.0f - flPercDelta));

	int nDenomination;
	if (flRandomPerc < 0.25f)
	{
		nDenomination = NUGGET_SMALL;
	}
	else if (flRandomPerc < 0.625f)
	{
		nDenomination = NUGGET_MEDIUM;
	}
	else
	{
		nDenomination = NUGGET_LARGE;
	}

	// Msg("Player: %.02f, Desired: %.02f, Seed: %.02f, Perc: %.02f, Result: %d\n", flPlayerHealthPerc, flDesiredHealthPerc, flSeed, flRandomPerc, nDenomination );

	return nDenomination;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::CreateNugget(void)
{
	CGrubNugget *pNugget = (CGrubNugget *)CreateEntityByName("item_grubnugget");
	if (pNugget == NULL)
		return;

	Vector vecOrigin;
	Vector vecForward;
	GetAttachment(LookupAttachment("glow"), vecOrigin, &vecForward);

	// Find out what size to make this nugget!
	int nDenomination = GetNuggetDenomination();
	pNugget->SetDenomination(nDenomination);

	pNugget->SetAbsOrigin(vecOrigin);
	pNugget->SetAbsAngles(RandomAngle(0, 360));
	DispatchSpawn(pNugget);

	IPhysicsObject *pPhys = pNugget->VPhysicsGetObject();
	if (pPhys)
	{
		Vector vecForward;
		GetVectors(&vecForward, NULL, NULL);

		Vector vecVelocity = RandomVector(-35.0f, 35.0f) + (vecForward * -RandomFloat(50.0f, 75.0f));
		AngularImpulse vecAngImpulse = RandomAngularImpulse(-100.0f, 100.0f);

		pPhys->AddVelocity(&vecVelocity, &vecAngImpulse);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::IdleSound(void)
{
//	EmitSound("NPC_Antlion_Grub.Idle");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::PainSound(const CTakeDamageInfo &info)
{
	EmitSound("NPC_Antlion_Grub.Alert");
	BroadcastAlert();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : interactionType - 
//			*data - 
//			*sourceEnt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_AntlionGrub::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter *sourceEnt)
{
	//Handle squeals from our peers
	if (interactionType == g_interactionAntlionGrubAlert)
	{
		SetCondition(COND_ANTLIONGRUB_HEARD_SQUEAL);

		if (!m_bSquashed && m_lifeState == LIFE_ALIVE)
		{
			//float envDuration = PlayEnvelope( m_pVoiceSound, SOUNDCTRL_CHANGE_VOLUME, envScared, ARRAYSIZE(envScared) );
			float envDuration = CSoundEnvelopeController::GetController().SoundPlayEnvelope(m_pVoiceSound, SOUNDCTRL_CHANGE_VOLUME, envMidSustain, ARRAYSIZE(envMidSustain));
			m_flNextVoiceChange = CURTIME + envDuration + random->RandomFloat(4.0f, 8.0f);
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::Squash(CBaseEntity *pOther)
{
	m_bSquashed = true;
	
	//if ( bSpawnBlood )
	{
		// Temp squash effect
		Vector vecForward, vecUp;
		AngleVectors(GetAbsAngles(), &vecForward, NULL, &vecUp);

		// Start effects at either end of the grub
		Vector vecSplortPos = GetAbsOrigin() + vecForward * 14.0f;
		DispatchParticleEffect("GrubSquashBlood", vecSplortPos, GetAbsAngles());

		vecSplortPos = GetAbsOrigin() - vecForward * 16.0f;
		Vector vecDir = -vecForward;
		QAngle vecAngles;
		VectorAngles(vecDir, vecAngles);
		DispatchParticleEffect("GrubSquashBlood", vecSplortPos, vecAngles);

	//	MakeSquashDecals(GetAbsOrigin() + vecForward * 32.0f);
	//	MakeSquashDecals(GetAbsOrigin() - vecForward * 32.0f);
	}

	EmitSound("NPC_Antlion_Grub.Squish");

	if( pOther && ( pOther->IsNPC() || pOther->IsPlayer())) SetEnemy(pOther);
	BroadcastAlert();

	trace_t	tr;
	Vector vecDir(0, 0, -1.0f);

	tr.endpos = GetLocalOrigin();
	tr.endpos[2] += 8.0f;

	MakeDamageBloodDecal(4, 0.8f, &tr, vecDir);
		
	Event_Killed(CTakeDamageInfo(pOther, pOther, m_iHealth + 10, DMG_CRUSH));

//	Relink();
	SetTouch(NULL);
}

//-----------------------------------------------------------------------------
// Purpose: Squish the grub!
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::InputSquash(inputdata_t &data)
{
	Squash(data.pActivator);
}

void CNPC_AntlionGrub::StopLoopingSounds()
{
	CSoundEnvelopeController::GetController().SoundDestroy(m_pMovementSound);
	CSoundEnvelopeController::GetController().SoundDestroy(m_pVoiceSound);
	CSoundEnvelopeController::GetController().SoundDestroy(m_pHealSound);

	m_pMovementSound = NULL;
	m_pVoiceSound = NULL;
	m_pHealSound = NULL;

	BaseClass::StopLoopingSounds();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pInflictor - 
//			*pAttacker - 
//			flDamage - 
//			bitsDamageType - 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::Event_Killed(const CTakeDamageInfo &info)
{
	// Stop being attached to us
	if (m_hGlowSprite)
	{
		FadeGlow();
		m_hGlowSprite->SetParent(NULL);
	}

	if (sk_grubnugget_enabled.GetBool() && m_createnugget_bool)
	{
		CreateNugget();
	}

	EmitSound("NPC_Antlion_Grub.Die");

	BaseClass::Event_Killed(info);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//			&vecDir - 
//			*ptr - 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr)
{
	QAngle vecAngles;
	VectorAngles(-vecDir, vecAngles);
	DispatchParticleEffect("GrubBlood", ptr->endpos, vecAngles);

	BaseClass::TraceAttack(info, vecDir, ptr);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();

	//Add a fail-safe for the glow sprite display
	if (!IsCurSchedule(SCHED_ANTLIONGRUB_GIVE_HEALTH))
	{
		if (m_bHealing && !m_bSquashed && m_lifeState == LIFE_ALIVE)
		{
			CSoundEnvelopeController::GetController().SoundChangeVolume(m_pHealSound, 0.0f, 0.5f);
		}

		m_bHealing = false;
	}

#if 0
	trace_t tr;
	Vector vecOrigin = GetAbsOrigin();

	UTIL_TraceLine(vecOrigin, vecOrigin - Vector(0, 0, 16), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction < 1.0f)
	{
		QAngle angles;

		VectorAngles(tr.plane.normal, angles);

		float flSwap = angles.x;

		angles.x = -angles.y;
		angles.y = flSwap;

		SetAbsAngles(angles);
	}
#endif
	//Deal with the healing glow
	if (m_hGlowSprite != NULL)
	{
		if (m_bHealing)
		{
			m_hGlowSprite->SetScale(random->RandomFloat(0.75f, 1.0f));
			m_hGlowSprite->SetHDRColorScale(0.75);
		}
		else
		{
			m_hGlowSprite->SetScale(random->RandomFloat(0.35f, 0.5f));
			m_hGlowSprite->SetHDRColorScale(0.25);
		}
	}

	if (!m_bSquashed && m_lifeState == LIFE_ALIVE)
	{
		//Check for movement sounds
		if (m_flGroundSpeed > 0.0f)
		{
			if (!m_bMoving)
			{
				CSoundEnvelopeController::GetController().SoundChangePitch(m_pMovementSound, 100, 0.1f);
				CSoundEnvelopeController::GetController().SoundChangeVolume(m_pMovementSound, 0.4f, 1.0f);

				m_bMoving = true;
			}
		}
		else if (m_bMoving)
		{
			CSoundEnvelopeController::GetController().SoundChangePitch(m_pMovementSound, 80, 0.5f);
			CSoundEnvelopeController::GetController().SoundChangeVolume(m_pMovementSound, 0.0f, 1.0f);

			m_bMoving = false;
		}

		//Check for a voice change
		if (m_flNextVoiceChange < CURTIME)
		{
			float envDuration = CSoundEnvelopeController::GetController().SoundPlayEnvelope(m_pVoiceSound, SOUNDCTRL_CHANGE_VOLUME, &grubVoiceEnvelopes[rand() % ARRAYSIZE(grubVoiceEnvelopes)]);
			m_flNextVoiceChange = CURTIME + envDuration + random->RandomFloat(1.0f, 8.0f);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flDot - 
//			flDist - 
// Output : int
//-----------------------------------------------------------------------------
int CNPC_AntlionGrub::MeleeAttack1Conditions(float flDot, float flDist)
{
	ClearCondition(COND_ANTLIONGRUB_IN_HEAL_RANGE);

	//If we're outside the heal range, then reset our timer
	if (flDist > ANTLIONGRUB_HEAL_RANGE)
	{
		m_flNearTime = CURTIME + 2.0f;
		return COND_TOO_FAR_TO_ATTACK;
	}

	//Otherwise if we've been in range for long enough signal it
	if (m_flNearTime < CURTIME)
	{
		if ((m_nHealthReserve > 0) && (GetEnemy()->m_iHealth < GetEnemy()->m_iMaxHealth))
		{
			SetCondition(COND_ANTLIONGRUB_IN_HEAL_RANGE);
		}
	}

	return COND_CAN_MELEE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_AntlionGrub::BuildScheduleTestBits(void)
{
	if (HasSpawnFlags(SF_ANTLIONGRUB_STATIC))
	{
		if (IsCurSchedule(SCHED_ANTLIONGRUB_STAND))
		{
			ClearCustomInterruptConditions();
		}
	}
	else
	{
		//Always squirm if we're being squashed
		if (!IsCurSchedule(SCHED_SMALL_FLINCH))
		{
			SetCustomInterruptCondition(COND_ANTLIONGRUB_BEING_SQUASHED);
		}
	}
}


//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------


//==================================================
// SCHED_ANTLIONGRUB_SQUEAL
//==================================================

AI_DEFINE_SCHEDULE
(
	SCHED_ANTLIONGRUB_SQUEAL,

	"	Tasks"
	"		TASK_FACE_ENEMY	0"
	"	"
	"	Interrupts"
	"		COND_ANTLIONGRUB_BEING_SQUASHED	"
	"		COND_NEW_ENEMY"
);

//==================================================
// SCHED_ANTLIONGRUB_STAND
//==================================================

AI_DEFINE_SCHEDULE
(
	SCHED_ANTLIONGRUB_STAND,

	"	Tasks"
	"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_IDLE"
	"	"
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_ANTLIONGRUB_HEARD_SQUEAL"
	"		COND_ANTLIONGRUB_BEING_SQUASHED"
	"		COND_NEW_ENEMY"
);

//==================================================
// SCHED_ANTLIONGRUB_SQUIRM
//==================================================

AI_DEFINE_SCHEDULE
(
	SCHED_ANTLIONGRUB_SQUIRM,

	"	Tasks"
	"		TASK_ANTLIONGRUB_SQUIRM	0"
	"		TASK_SET_GOAL			GOAL:SAVED_POSITION"
	"		TASK_GET_PATH_TO_GOAL	PATH:TRAVEL"
	"		TASK_WALK_PATH			0"
	"		TASK_WAIT_FOR_MOVEMENT	0"
	"	"
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_LIGHT_DAMAGE"
	"		COND_ANTLIONGRUB_HEARD_SQUEAL"
	"		COND_ANTLIONGRUB_BEING_SQUASHED"
	"		COND_NEW_ENEMY"
);

//==================================================
// SCHED_ANTLIONGRUB_GIVE_HEALTH
//==================================================

AI_DEFINE_SCHEDULE
(
	SCHED_ANTLIONGRUB_GIVE_HEALTH,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ANTLIONGRUB_STAND"
	"		TASK_STOP_MOVING				0"
	"		TASK_SET_GOAL					GOAL:ENEMY"
	"		TASK_GET_PATH_TO_GOAL			PATH:TRAVEL"
	"		TASK_RUN_PATH					0"
	"		TASK_ANTLIONGRUB_MOVE_TO_TARGET	48"
	"		TASK_STOP_MOVING				0"
	"		TASK_FACE_ENEMY					0"
	"		TASK_ANTLIONGRUB_GIVE_HEALTH	0"
	"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_ANTLIONGRUB_SQUIRM"
	"		"
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_NEW_ENEMY"
	"		COND_ANTLIONGRUB_BEING_SQUASHED"
	"		COND_ANTLIONGRUB_HEARD_SQUEAL"
);

//==================================================
// SCHED_ANTLIONGUARD_RETREAT
//==================================================

AI_DEFINE_SCHEDULE
(
	SCHED_ANTLIONGUARD_RETREAT,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_ANTLIONGRUB_STAND"
	"		TASK_STOP_MOVING					0"
	"		TASK_ANTLIONGRUB_FIND_RETREAT_GOAL	0"
	"		TASK_SET_GOAL						GOAL:SAVED_POSITION"
	"		TASK_GET_PATH_TO_GOAL				PATH:TRAVEL"
	"		TASK_RUN_PATH						0"
	"		TASK_WAIT_FOR_MOVEMENT				0"
	"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_ANTLIONGRUB_STAND"
	"	"
	"	Interrupts"
	"		COND_TASK_FAILED"
	"		COND_NEW_ENEMY"
	"		COND_ANTLIONGRUB_BEING_SQUASHED"
	"		COND_ANTLIONGRUB_HEARD_SQUEAL"
);
#endif // DARKINTERVAL
//
//  Simple grub
//
#ifndef DARKINTERVAL
enum GrubState_e
{
	GRUB_STATE_IDLE,
	GRUB_STATE_AGITATED,
};

#define	ANTLIONGRUB_MODEL				"models/antlion_grub.mdl"
#define	ANTLIONGRUB_SQUASHED_MODEL		"models/antlion_grub_squashed.mdl"

#define	SF_ANTLIONGRUB_NO_AUTO_PLACEMENT	(1<<0)

class CAntlionGrub : public CBaseAnimating
{
public:
	DECLARE_CLASS( CAntlionGrub, CBaseAnimating );

	virtual void	Activate( void );
	virtual void	Spawn( void );
	virtual void	Precache( void );
	virtual void	UpdateOnRemove( void );
	virtual void	Event_Killed( const CTakeDamageInfo &info );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );

	void	InputSquash( inputdata_t &data );

	void	IdleThink( void );
	void	FlinchThink( void );
	void	GrubTouch( CBaseEntity *pOther );

	DECLARE_DATADESC();

protected:

	inline bool InPVS( void );
	void		SetNextThinkByDistance( void );

	int		GetNuggetDenomination( void );
	void	CreateNugget( void );
	void	MakeIdleSounds( void );
	void	MakeSquashDecals( const Vector &vecOrigin );
	void	AttachToSurface( void );
	void	CreateGlow( void );
	void	FadeGlow( void );
	void	Squash( CBaseEntity *pOther, bool bDealDamage, bool bSpawnBlood );
	void	SpawnSquashedGrub( void );
	void	InputAgitate( inputdata_t &inputdata );

	inline bool ProbeSurface( const Vector &vecTestPos, const Vector &vecDir, Vector *vecResult, Vector *vecNormal );

	CHandle<CSprite>	m_hGlowSprite;
	int					m_nGlowSpriteHandle;
	float				m_flFlinchTime;
	float				m_flNextIdleSoundTime;
	float				m_flNextSquealSoundTime;
	bool				m_bOutsidePVS;
	GrubState_e			m_State;

	COutputEvent		m_OnAgitated;
	COutputEvent		m_OnDeath;
	COutputEvent		m_OnDeathByPlayer;
};

BEGIN_DATADESC( CAntlionGrub )

	DEFINE_FIELD( m_hGlowSprite, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flFlinchTime,	FIELD_TIME ),
	DEFINE_FIELD( m_flNextIdleSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextSquealSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_State, FIELD_INTEGER ),

	DEFINE_INPUTFUNC( FIELD_FLOAT, "Agitate", InputAgitate ),

	DEFINE_OUTPUT( m_OnAgitated, "OnAgitated" ),
	DEFINE_OUTPUT( m_OnDeath, "OnDeath" ),
	DEFINE_OUTPUT( m_OnDeathByPlayer, "OnDeathByPlayer" ),

	// Functions
	DEFINE_ENTITYFUNC( GrubTouch ),
	DEFINE_ENTITYFUNC( IdleThink ),
	DEFINE_ENTITYFUNC( FlinchThink ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Squash", InputSquash ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_antlion_grub, CAntlionGrub );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::CreateGlow( void )
{
	// Create the glow sprite
	m_hGlowSprite = CSprite::SpriteCreate( "sprites/grubflare1.vmt", GetLocalOrigin(), false );
	Assert( m_hGlowSprite );
	if ( m_hGlowSprite == NULL )
		return;

	m_hGlowSprite->TurnOn();
	m_hGlowSprite->SetTransparency( kRenderWorldGlow, 156, 169, 121, 164, kRenderFxNoDissipation );
	m_hGlowSprite->SetScale( 0.5f );
	m_hGlowSprite->SetGlowProxySize( 16.0f );
	int nAttachment = LookupAttachment( "glow" );
	m_hGlowSprite->SetParent( this, nAttachment );
	m_hGlowSprite->SetLocalOrigin( vec3_origin );
	
	// Don't uselessly animate, we're a static sprite!
	m_hGlowSprite->SetThink( NULL );
	m_hGlowSprite->SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::FadeGlow( void )
{
	if ( m_hGlowSprite )
	{
		m_hGlowSprite->FadeAndDie( 0.25f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::UpdateOnRemove( void )
{
	FadeGlow();

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Find what size of nugget to spawn
//-----------------------------------------------------------------------------
int CAntlionGrub::GetNuggetDenomination( void )
{
	// Find the desired health perc we want to be at
	float flDesiredHealthPerc = DynamicResupply_GetDesiredHealthPercentage();
	
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( pPlayer == NULL )
		return -1;

	// Get the player's current health percentage
	float flPlayerHealthPerc = (float) pPlayer->GetHealth() / (float) pPlayer->GetMaxHealth();

	// If we're already maxed out, return the small nugget
	if ( flPlayerHealthPerc >= flDesiredHealthPerc )
	{
		return NUGGET_SMALL;
	}

	// Find where we fall in the desired health's range
	float flPercDelta = flPlayerHealthPerc / flDesiredHealthPerc;

	// The larger to discrepancy, the higher the chance to move quickly to close it
	float flSeed = random->RandomFloat( 0.0f, 1.0f );
	float flRandomPerc = Bias( flSeed, (1.0f-flPercDelta) );
	
	int nDenomination;
	if ( flRandomPerc < 0.25f )
	{
		nDenomination = NUGGET_SMALL;
	}
	else if ( flRandomPerc < 0.625f )
	{
		nDenomination = NUGGET_MEDIUM;
	}
	else
	{
		nDenomination = NUGGET_LARGE;
	}

	// Msg("Player: %.02f, Desired: %.02f, Seed: %.02f, Perc: %.02f, Result: %d\n", flPlayerHealthPerc, flDesiredHealthPerc, flSeed, flRandomPerc, nDenomination );

	return nDenomination;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::CreateNugget( void )
{
	CGrubNugget *pNugget = (CGrubNugget *) CreateEntityByName( "item_grubnugget" );
	if ( pNugget == NULL )
		return;

	Vector vecOrigin;
	Vector vecForward;
	GetAttachment( LookupAttachment( "glow" ), vecOrigin, &vecForward );

	// Find out what size to make this nugget!
	int nDenomination = GetNuggetDenomination();
	pNugget->SetDenomination( nDenomination );
	
	pNugget->SetAbsOrigin( vecOrigin );
	pNugget->SetAbsAngles( RandomAngle( 0, 360 ) );
	DispatchSpawn( pNugget );

	IPhysicsObject *pPhys = pNugget->VPhysicsGetObject();
	if ( pPhys )
	{
		Vector vecForward;
		GetVectors( &vecForward, NULL, NULL );
		
		Vector vecVelocity = RandomVector( -35.0f, 35.0f ) + ( vecForward * -RandomFloat( 50.0f, 75.0f ) );
		AngularImpulse vecAngImpulse = RandomAngularImpulse( -100.0f, 100.0f );

		pPhys->AddVelocity( &vecVelocity, &vecAngImpulse );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
void CAntlionGrub::Event_Killed( const CTakeDamageInfo &info )
{
	// Fire our output only if the player is the one that killed us
	if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() )
	{
		m_OnDeathByPlayer.FireOutput( info.GetAttacker(), info.GetAttacker() );
	}

	m_OnDeath.FireOutput( info.GetAttacker(), info.GetAttacker() );
	SendOnKilledGameEvent( info );

	// Crush and crowbar damage hurt us more than others
	bool bSquashed = ( info.GetDamageType() & (DMG_CRUSH|DMG_CLUB)) ? true : false;
	Squash( info.GetAttacker(), false, bSquashed );

	m_takedamage = DAMAGE_NO;

	if ( sk_grubnugget_enabled.GetBool() )
	{
		CreateNugget();
	}

	// Go away
	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( CURTIME + 0.1f );

	// we deliberately do not call BaseClass::EventKilled
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
int CAntlionGrub::OnTakeDamage( const CTakeDamageInfo &info )
{
	// Animate a flinch of pain if we're dying
	bool bSquashed = ( ( GetEffects() & EF_NODRAW ) != 0 );
	if ( bSquashed == false )
	{
		SetSequence( SelectWeightedSequence( ACT_SMALL_FLINCH ) );
		m_flFlinchTime = CURTIME + random->RandomFloat( 0.5f, 1.0f );

		SetThink( &CAntlionGrub::FlinchThink );
		SetNextThink( CURTIME + 0.05f );
	}

	return BaseClass::OnTakeDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: Whether or not we're in the PVS
//-----------------------------------------------------------------------------
inline bool CAntlionGrub::InPVS( void )
{
	return ( UTIL_FindClientInPVS( edict() ) != NULL ) || (UTIL_ClientPVSIsExpanded() && UTIL_FindClientInVisibilityPVS( edict() ));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::SetNextThinkByDistance( void )
{
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( pPlayer == NULL )
	{
		SetNextThink( CURTIME + random->RandomFloat( 0.5f, 3.0f ) );
		return;
	}

	float flDistToPlayerSqr = ( GetAbsOrigin() - pPlayer->GetAbsOrigin() ).LengthSqr();
	float scale = RemapValClamped( flDistToPlayerSqr, Square( 400 ), Square( 5000 ), 1.0f, 5.0f );
	float time = random->RandomFloat( 1.0f, 3.0f );
	SetNextThink( CURTIME + ( time * scale ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::Spawn( void )
{
	Precache();
	BaseClass::Spawn();

	SetModel( ANTLIONGRUB_MODEL );
	
	// FIXME: This is a big perf hit with the number of grubs we're using! - jdw
	CreateGlow();

	SetSolid( SOLID_BBOX );
	SetSolidFlags( FSOLID_TRIGGER );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	AddEffects( EF_NOSHADOW );

	CollisionProp()->UseTriggerBounds(true,1);

	SetTouch( &CAntlionGrub::GrubTouch );

	SetHealth( 1 );
	m_takedamage = DAMAGE_YES;

	// Stick to the nearest surface
	if ( HasSpawnFlags( SF_ANTLIONGRUB_NO_AUTO_PLACEMENT ) == false )
	{
		AttachToSurface();
	}

	// At this point, alter our bounds to make sure we're within them
	Vector vecMins, vecMaxs;
	RotateAABB( EntityToWorldTransform(), CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs(), vecMins, vecMaxs );

	UTIL_SetSize( this, vecMins, vecMaxs );

	// Start our idle activity
	SetSequence( SelectWeightedSequence( ACT_IDLE ) );
	SetCycle( random->RandomFloat( 0.0f, 1.0f ) );
	ResetSequenceInfo();

	m_State = GRUB_STATE_IDLE;

	// Reset
	m_flFlinchTime = 0.0f;
	m_flNextIdleSoundTime = CURTIME + random->RandomFloat( 4.0f, 8.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::Activate( void )
{
	BaseClass::Activate();

	// Idly think
	SetThink( &CAntlionGrub::IdleThink );
	SetNextThinkByDistance();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecTestPos - 
//			*vecResult - 
//			*flDist - 
// Output : inline bool
//-----------------------------------------------------------------------------
inline bool CAntlionGrub::ProbeSurface( const Vector &vecTestPos, const Vector &vecDir, Vector *vecResult, Vector *vecNormal )
{
	// Trace down to find a surface
	trace_t tr;
	UTIL_TraceLine( vecTestPos, vecTestPos + (vecDir*256.0f), MASK_NPCSOLID&(~CONTENTS_MONSTER), this, COLLISION_GROUP_NONE, &tr );

	if ( vecResult )
	{
		*vecResult = tr.endpos;
	}

	if ( vecNormal )
	{
		*vecNormal = tr.plane.normal;
	}

	return ( tr.fraction < 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Attaches the grub to the surface underneath its abdomen
//-----------------------------------------------------------------------------
void CAntlionGrub::AttachToSurface( void )
{
	// Get our downward direction
	Vector vecForward, vecRight, vecDown;
	GetVectors( &vecForward, &vecRight, &vecDown );
	vecDown.Negate();
	
	Vector vecOffset = ( vecDown * -8.0f );

	// Middle
	Vector vecMid, vecMidNormal;
	if ( ProbeSurface( WorldSpaceCenter() + vecOffset, vecDown, &vecMid, &vecMidNormal ) == false )
	{
		// A grub was left hanging in the air, it must not be near any valid surfaces!
		Warning("Antlion grub stranded in space at (%.02f, %.02f, %.02f) : REMOVED\n", GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		UTIL_Remove( this );
		return;
	}

	// Sit at the mid-point
	UTIL_SetOrigin( this, vecMid );

	Vector vecPivot;
	Vector vecPivotNormal;

	bool bNegate = true;

	// First test our tail (more crucial that it doesn't interpenetrate with the world)
	if ( ProbeSurface( WorldSpaceCenter() - ( vecForward * 12.0f ) + vecOffset, vecDown, &vecPivot, &vecPivotNormal ) == false )
	{
		// If that didn't find a surface, try the head
		if ( ProbeSurface( WorldSpaceCenter() + ( vecForward * 12.0f ) + vecOffset, vecDown, &vecPivot, &vecPivotNormal ) == false )
		{
			// Worst case, just site at the middle
			UTIL_SetOrigin( this, vecMid );

			QAngle vecAngles;
			VectorAngles( vecForward, vecMidNormal, vecAngles );
			SetAbsAngles( vecAngles );
			return;
		}

		bNegate = false;
	}
	
	// Find the line we'll lay on if these two points are connected by a line
	Vector vecLieDir = ( vecPivot - vecMid );
	VectorNormalize( vecLieDir );
	if ( bNegate )
	{
		// We need to try and maintain our facing
		vecLieDir.Negate();
	}

	// Use the average of the surface normals to be our "up" direction
	Vector vecPseudoUp = ( vecMidNormal + vecPivotNormal ) * 0.5f;

	QAngle vecAngles;
	VectorAngles( vecLieDir, vecPseudoUp, vecAngles );

	SetAbsAngles( vecAngles );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::MakeIdleSounds( void )
{
	if ( m_State == GRUB_STATE_AGITATED )
	{
		if ( m_flNextSquealSoundTime < CURTIME )
		{
			EmitSound( "NPC_Antlion_Grub.Stimulated" );
			m_flNextSquealSoundTime = CURTIME + random->RandomFloat( 1.5f, 3.0f );
			m_flNextIdleSoundTime = CURTIME + random->RandomFloat( 4.0f, 8.0f );
		}
	}
	else
	{
		if ( m_flNextIdleSoundTime < CURTIME )
		{
			EmitSound( "NPC_Antlion_Grub.Idle" );
			m_flNextIdleSoundTime = CURTIME + random->RandomFloat( 8.0f, 12.0f );
		}
	}
}

#define DEBUG_GRUB_THINK_TIMES 0

#if DEBUG_GRUB_THINK_TIMES
	int nFrame = 0;
	int nNumThinks = 0;
#endif // DEBUG_GRUB_THINK_TIMES

//-----------------------------------------------------------------------------
// Purpose: Advance our thinks
//-----------------------------------------------------------------------------
void CAntlionGrub::IdleThink( void )
{
#if DEBUG_GRUB_THINK_TIMES
	// Test for a new frame
	if ( gpGlobals->framecount != nFrame )
	{
		if ( nNumThinks > 10 )
		{
			Msg("%d npc_antlion_grubs thinking per frame!\n", nNumThinks );
		}

		nFrame = gpGlobals->framecount;
		nNumThinks = 0;
	}

	nNumThinks++;
#endif // DEBUG_GRUB_THINK_TIMES

	// Check the PVS status
	if ( InPVS() == false )
	{
		// Push out into the future until they're in our PVS
		SetNextThinkByDistance();
		m_bOutsidePVS = true;
		return;
	}

	// Stagger our sounds if we've just re-entered the PVS
	if ( m_bOutsidePVS )
	{
		m_flNextIdleSoundTime = CURTIME + random->RandomFloat( 1.0f, 4.0f );
		m_bOutsidePVS = false;
	}

	// See how close the player is
	CBasePlayer *pPlayerEnt = AI_GetSinglePlayer();
	float flDistToPlayerSqr = ( GetAbsOrigin() - pPlayerEnt->GetAbsOrigin() ).LengthSqr();

	bool bFlinching = ( m_flFlinchTime > CURTIME );

	// If they're far enough away, just wait to think again
	if ( flDistToPlayerSqr > Square( 40*12 ) && bFlinching == false )
	{
		SetNextThinkByDistance();
		return;
	}
	
	// At this range, the player agitates us with his presence
	bool bPlayerWithinAgitationRange = ( flDistToPlayerSqr <= Square( (6*12) ) );
	bool bAgitated = (bPlayerWithinAgitationRange || bFlinching );

	// If we're idle and the player has come close enough, get agry
	if ( ( m_State == GRUB_STATE_IDLE ) && bAgitated )
	{
		SetSequence( SelectWeightedSequence( ACT_SMALL_FLINCH ) );
		m_State = GRUB_STATE_AGITATED;
	}
	else if ( IsSequenceFinished() )
	{
		// See if it's time to choose a new sequence
		ResetSequenceInfo();
		SetCycle( 0.0f );

		// If we're near enough, we want to play an "alert" animation
		if ( bAgitated )
		{
			SetSequence( SelectWeightedSequence( ACT_SMALL_FLINCH ) );
			m_State = GRUB_STATE_AGITATED;
		}
		else
		{
			// Just idle
			SetSequence( SelectWeightedSequence( ACT_IDLE ) );
			m_State = GRUB_STATE_IDLE;
		}

		// Add some variation because we're often in large bunches
		SetPlaybackRate( random->RandomFloat( 0.8f, 1.2f ) );
	}

	// Idle normally
	StudioFrameAdvance();
	MakeIdleSounds();
	SetNextThink( CURTIME + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::FlinchThink( void )
{
	StudioFrameAdvance();
	SetNextThink( CURTIME + 0.1f );

	// See if we're done
	if ( m_flFlinchTime < CURTIME )
	{
		SetSequence( SelectWeightedSequence( ACT_IDLE ) );
		SetThink( &CAntlionGrub::IdleThink );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::GrubTouch( CBaseEntity *pOther )
{
	// We can be squished by the player, Vort, or flying heavy things.
	IPhysicsObject *pPhysOther = pOther->VPhysicsGetObject(); // bool bThrown = ( pTarget->VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_WAS_THROWN ) != 0;
	if ( pOther->IsPlayer() || FClassnameIs(pOther,"npc_vortigaunt") || ( pPhysOther && (pPhysOther->GetGameFlags() & FVPHYSICS_WAS_THROWN )) )
	{
		m_OnAgitated.FireOutput( pOther, pOther );
		Squash( pOther, true, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::Precache( void )
{
	PrecacheModel( ANTLIONGRUB_MODEL );
	PrecacheModel( ANTLIONGRUB_SQUASHED_MODEL );

	m_nGlowSpriteHandle = PrecacheModel("sprites/grubflare1.vmt");

	PrecacheScriptSound( "NPC_Antlion_Grub.Idle" );
	PrecacheScriptSound( "NPC_Antlion_Grub.Alert" );
	PrecacheScriptSound( "NPC_Antlion_Grub.Stimulated" );
	PrecacheScriptSound( "NPC_Antlion_Grub.Die" );
	PrecacheScriptSound( "NPC_Antlion_Grub.Squish" );

	PrecacheParticleSystem( "GrubSquashBlood" );
	PrecacheParticleSystem( "GrubBlood" );

	UTIL_PrecacheOther( "item_grubnugget" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Squish the grub!
//-----------------------------------------------------------------------------
void CAntlionGrub::InputSquash( inputdata_t &data )
{
	Squash( data.pActivator, true, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::SpawnSquashedGrub( void )
{
	// If we're already invisible, we're done
	if ( GetEffects() & EF_NODRAW )
		return;

	Vector vecUp;
	GetVectors( NULL, NULL, &vecUp );
	CBaseEntity *pGib = CreateRagGib( ANTLIONGRUB_SQUASHED_MODEL, GetAbsOrigin(), GetAbsAngles(), vecUp * 16.0f );
	if ( pGib )
	{
		pGib->AddEffects( EF_NOSHADOW );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::MakeSquashDecals( const Vector &vecOrigin )
{
	trace_t tr;
	Vector	vecStart;
	Vector	vecTraceDir;

	GetVectors( NULL, NULL, &vecTraceDir );
	vecTraceDir.Negate();

	for ( int i = 0 ; i < 8; i++ )
	{
		vecStart.x = vecOrigin.x + random->RandomFloat( -16.0f, 16.0f );
		vecStart.y = vecOrigin.y + random->RandomFloat( -16.0f, 16.0f );
		vecStart.z = vecOrigin.z + 4;

		UTIL_TraceLine( vecStart, vecStart + ( vecTraceDir * (5*12) ), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_YELLOW );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAntlionGrub::Squash( CBaseEntity *pOther, bool bDealDamage, bool bSpawnBlood )
{
	// If we're already squashed, then don't bother doing it again!
	if ( GetEffects() & EF_NODRAW )
		return;

	SpawnSquashedGrub();

	AddEffects( EF_NODRAW );
	AddSolidFlags( FSOLID_NOT_SOLID );
	
	// Stop being attached to us
	if ( m_hGlowSprite )
	{
		FadeGlow();
		m_hGlowSprite->SetParent( NULL );
	}

	EmitSound( "NPC_Antlion_Grub.Die" );
	EmitSound( "NPC_Antlion_Grub.Squish" );

#ifndef DARKINTERVAL // this is for the episodic vort, DI uses HL2 vorts
	// if vort stepped on me, maybe he wants to say something
	if ( pOther && FClassnameIs(pOther, "npc_vortigaunt") )
	{
		Assert(dynamic_cast<CNPC_Vortigaunt *>( pOther ));
		static_cast<CNPC_Vortigaunt *>( pOther )->OnSquishedGrub(this);
	}
#endif

	SetTouch( NULL );

	//if ( bSpawnBlood )
	{
		// Temp squash effect
		Vector vecForward, vecUp;
		AngleVectors( GetAbsAngles(), &vecForward, NULL, &vecUp );

		// Start effects at either end of the grub
		Vector vecSplortPos = GetAbsOrigin() + vecForward * 14.0f;
		DispatchParticleEffect( "GrubSquashBlood", vecSplortPos, GetAbsAngles() );

		vecSplortPos = GetAbsOrigin() - vecForward * 16.0f;
		Vector vecDir = -vecForward;
		QAngle vecAngles;
		VectorAngles( vecDir, vecAngles );
		DispatchParticleEffect( "GrubSquashBlood", vecSplortPos, vecAngles );
		
		MakeSquashDecals( GetAbsOrigin() + vecForward * 32.0f );
		MakeSquashDecals( GetAbsOrigin() - vecForward * 32.0f );
	}

	// Deal deadly damage to ourself
	if ( bDealDamage )
	{
		CTakeDamageInfo info( pOther, pOther, Vector( 0, 0, -1 ), GetAbsOrigin(), GetHealth()+1, DMG_CRUSH );
		TakeDamage( info );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//			&vecDir - 
//			*ptr - 
//-----------------------------------------------------------------------------
void CAntlionGrub::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	QAngle vecAngles;
	VectorAngles( -vecDir, vecAngles );
	DispatchParticleEffect( "GrubBlood", ptr->endpos, vecAngles );

	BaseClass::TraceAttack( info, vecDir, ptr );
}

//-----------------------------------------------------------------------------
// Purpose: Make the grub angry!
//-----------------------------------------------------------------------------
void CAntlionGrub::InputAgitate( inputdata_t &inputdata )
{
	SetSequence( SelectWeightedSequence( ACT_SMALL_FLINCH ) );
	m_State = GRUB_STATE_AGITATED;
	m_flNextSquealSoundTime = CURTIME;

	m_flFlinchTime = CURTIME + inputdata.value.Float();

	SetNextThink( CURTIME );
}
#endif // DARKINTERVAL

// =====================================================================
//
//  Tasty grub nugget!
//
// =====================================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrubNugget::Spawn( void )
{
	Precache();
	
	if ( m_nDenomination == NUGGET_LARGE )
	{
		SetModel( "models/grub_nugget_large.mdl" );
	}
	else if ( m_nDenomination == NUGGET_MEDIUM )
	{
		SetModel( "models/grub_nugget_medium.mdl" );	
	}
	else
	{
		SetModel( "models/grub_nugget_small.mdl" );
	}

	// We're self-illuminating, so we don't take or give shadows
	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );

	m_iHealth = 1;

	BaseClass::Spawn();

	m_takedamage = DAMAGE_YES;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrubNugget::Precache( void )
{
	PrecacheModel("models/grub_nugget_small.mdl");
	PrecacheModel("models/grub_nugget_medium.mdl");
	PrecacheModel("models/grub_nugget_large.mdl");

	PrecacheScriptSound( "GrubNugget.Touch" );
	PrecacheScriptSound( "NPC_Antlion_Grub.Explode" );

	PrecacheParticleSystem( "antlion_spit_player" );
}

//-----------------------------------------------------------------------------
// Purpose: Let us be picked up by the gravity gun, regardless of our material
//-----------------------------------------------------------------------------
bool CGrubNugget::VPhysicsIsFlesh( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGrubNugget::MyTouch( CBasePlayer *pPlayer )
{
	//int nHealthToGive = sk_grubnugget_health.GetFloat() * m_nDenomination;
	int nHealthToGive;
	switch (m_nDenomination)
	{
	case NUGGET_SMALL:
		nHealthToGive = sk_grubnugget_health_small.GetInt();
		break;
	case NUGGET_LARGE:
		nHealthToGive = sk_grubnugget_health_large.GetInt();
		break;
	default:
		nHealthToGive = sk_grubnugget_health_medium.GetInt();
	}

	// Attempt to give the player health
	if ( pPlayer->TakeHealth( nHealthToGive, DMG_GENERIC ) == 0 )
		return false;

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	UserMessageBegin( user, "ItemPickup" );
	WRITE_STRING( GetClassname() );
	MessageEnd();

	CPASAttenuationFilter filter( pPlayer, "GrubNugget.Touch" );
	EmitSound( filter, pPlayer->entindex(), "GrubNugget.Touch" );

	UTIL_Remove( this );	

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//			*pEvent - 
//-----------------------------------------------------------------------------
void CGrubNugget::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	int damageType;
	float damage = CalculateDefaultPhysicsDamage( index, pEvent, 1.0f, true, damageType );
	if ( damage > 5.0f )
	{
		CBaseEntity *pHitEntity = pEvent->pEntities[!index];
		if ( pHitEntity == NULL )
		{
			// hit world
			pHitEntity = GetContainingEntity( INDEXENT(0) );
		}
		
		Vector damagePos;
		pEvent->pInternalData->GetContactPoint( damagePos );
		Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass();
		if ( damageForce == vec3_origin )
		{
			// This can happen if this entity is motion disabled, and can't move.
			// Use the velocity of the entity that hit us instead.
			damageForce = pEvent->postVelocity[!index] * pEvent->pObjects[!index]->GetMass();
		}

		// FIXME: this doesn't pass in who is responsible if some other entity "caused" this collision
		PhysCallbackDamage( this, CTakeDamageInfo( pHitEntity, pHitEntity, damageForce, damagePos, damage, damageType ), *pEvent, index );
	}

	BaseClass::VPhysicsCollision( index, pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
void CGrubNugget::Event_Killed( const CTakeDamageInfo &info )
{
	AddEffects( EF_NODRAW );
	DispatchParticleEffect( "antlion_spit_player", GetAbsOrigin(), QAngle( -90, 0, 0 ) );
	EmitSound( "NPC_Antlion_Grub.Explode" );

	BaseClass::Event_Killed( info );
}