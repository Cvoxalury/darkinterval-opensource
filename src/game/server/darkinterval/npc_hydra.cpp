#include "cbase.h"
#include "_sharedents_encompass.h"
#include "ai_baseactor.h" // hydra "dummy" model is a flex-capable actor
#include "sceneentity.h" // needed for dummy's flex scenes
#include "ai_hint.h"
#include "ai_interactions.h"
#include "ai_moveprobe.h"
#include "ai_senses.h"
#include "ai_waypoint.h"
#include "ammodef.h"
#include "bone_setup.h" // needed for vphysics setup for small hydra
#include "hl2_shareddefs.h" // for hl2 collision groups, see SmallHydra::Spawn
#include "grenade_frag.h"
#include "ieffects.h"
#include "npcevent.h"
#include "pathtrack.h"
#include "props_shared.h"
#include "physconstraint.h"
#include "physics_prop_ragdoll.h" // to spawn separate ragdoll when the a hydra dies
#include "phys_ragdollmagnet.h"
//#include "npc_wscanner.h"
#include "physics_saverestore.h" // for the impale constraint
#include "saverestore_utlvector.h"
#include "scripted.h"
#include "soundenvelope.h"
#include "te_effect_dispatch.h"
#include "vphysics/constraints.h"
#include "entitylist.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	CHAIN_LINKS 32 // 16
#define hide_the_crap 1
#define USE_NEW_FAKE_COLLISIONS

static const char *HYDRA_MODEL = "models/_Monsters/Xen/Hydra.mdl";
static const char *HYDRA_RAGDOLL_MODEL = "models/_Monsters/Xen/Hydra_ragdoll.mdl";
static const char *HYDRA_HITBOX_MODEL = "models/_Monsters/Xen/Hydra_head.mdl";
ConVar hydra_show_collision_objects("hydra_show_collision_objects", "0");

extern float	GetFloorZ(const Vector &origin);
#if (hide_the_crap)
// Hydra segment collision
class hydraCollision : public CBaseAnimating // todo: make them server-side only, no edict
{
	DECLARE_CLASS(hydraCollision, CBaseAnimating);
	DECLARE_DATADESC();
public:
	void Precache(void)
	{
		PrecacheModel(HYDRA_HITBOX_MODEL);
	}

	void Spawn(void)
	{
		BaseClass::Spawn();
		Precache();
		SetModel(HYDRA_HITBOX_MODEL);
		SetCollisionGroup(COLLISION_GROUP_DEBRIS);
		if (hydra_show_collision_objects.GetBool() == 0)
		{
			SetRenderMode(kRenderNone);
		}
		AddEffects(EF_NOSHADOW);
		m_takedamage = DAMAGE_YES;
		m_iHealth = m_iMaxHealth = 1000;
		CreateVPhysics();
	}
		
	bool CreateVPhysics(void)
	{
		SetSolid(SOLID_VPHYSICS);
		IPhysicsObject *pPhysicsObject = VPhysicsInitShadow(false, false);

		if (!pPhysicsObject || GetParent()->m_target != NULL_STRING)
		{
			SetSolid(SOLID_NONE);
			SetMoveType(MOVETYPE_NONE);
			Warning("ERROR!: Can't create physics object for %s\n", STRING(GetModelName()));
		}

		return true;
	}
	
	int OnTakeDamage(const CTakeDamageInfo &info)
	{
		AssertMsg(GetParent() != NULL, "Hydra collision object lost its parent!");
		CBaseEntity *parenthydra = GetParent();
		AssertMsg(parenthydra != NULL, "Hydra collision object can't find its parent Hydra!");

		CTakeDamageInfo newInfo = info;

		if (newInfo.GetDamage() > GetHealth())
		{
			newInfo.SetDamage(GetHealth() - 1);
		}

		int tookdamage = BaseClass::OnTakeDamage(newInfo); 
		
		if (parenthydra->m_lifeState == LIFE_ALIVE)
		{
			parenthydra->TakeDamage(newInfo);
		}
#ifndef USE_NEW_FAKE_COLLISIONS		
		if (GetHealth() <= GetMaxHealth() * 0.1f)
		{
			if (parenthydra->m_lifeState == LIFE_ALIVE)
			{
				inputdata_t input;
				input.pActivator = info.GetAttacker();
				input.pCaller = info.GetAttacker();
				input.value.SetInt(m_segindex_int);

				parenthydra->AcceptInput("DestroyNumberedHitbox", this, this, input.value, 0);
			}

			return 0;
		}
#endif
		Vector bloodDir;
		RandomVectorInUnitSphere(&bloodDir);
		UTIL_BloodSpray(GetAbsOrigin(), bloodDir * 2.0f, BLOOD_COLOR_HYDRA, 2, FX_BLOODSPRAY_ALL);
		
		return tookdamage;
	}

	int m_segindex_int;
};

BEGIN_DATADESC(hydraCollision)
	DEFINE_KEYFIELD(m_segindex_int, FIELD_INTEGER, "segIndex"),
END_DATADESC()

LINK_ENTITY_TO_CLASS(hydra_hitbox, hydraCollision);

class hydraSeg
{
public:
	hydraSeg(void)
	{
		Pos = Dir = vec3_origin;
//		Weight = 0.0f;
	}

	Vector Pos;
	Vector GoalPos;
	Vector Dir;
	float  Weight;
	float  Length;
	float  nextUpdate;
	bool Stuck;

	DECLARE_SIMPLE_DATADESC()
};

BEGIN_SIMPLE_DATADESC(hydraSeg)
DEFINE_FIELD(Pos, FIELD_POSITION_VECTOR),
DEFINE_FIELD(GoalPos, FIELD_POSITION_VECTOR),
DEFINE_FIELD(Dir, FIELD_VECTOR),
DEFINE_FIELD(Weight, FIELD_FLOAT),
DEFINE_FIELD(Length, FIELD_FLOAT),
DEFINE_FIELD(Stuck, FIELD_BOOLEAN),
END_DATADESC()

//------------------------------------------------------------------
// Dummy model entity that receives bone vectors from CHydra and 
// links them with client logic. 
//------------------------------------------------------------------
class hydraDummy : public CAI_BaseActorNoBlend //CAI_BaseNPC
{
private:
#ifdef USE_NEW_FAKE_COLLISIONS
	CUtlVector	 <EHANDLE>	m_collisions;
#endif
public:
	DECLARE_CLASS(hydraDummy, CAI_BaseActorNoBlend)
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	hydraDummy() 
	{
		m_proxy_time_to_attack_float = FLT_MAX;
		m_proxy_material_intensity_float = 0;
		m_anim_lerp_start_time_float = 0;
		m_anim_lerp_end_time_float = 0;
		m_anim_sequence_int = -1;
	}

	~hydraDummy() 
	{
#ifdef USE_NEW_FAKE_COLLISIONS
		m_collisions.RemoveAll();
#endif
	}

	int		GetSoundInterests(void) { return (SOUND_NONE); }
	float	MaxYawSpeed(void) { return 0; }
	
	Class_T Classify() { return CLASS_NONE;	}

	void	Precache()
	{
		BaseClass::Precache();
		PrecacheInstancedScene("scenes/di_npc/hydra_pulsating.vcd");
		PrecacheModel(HYDRA_MODEL);
	}

	void	Spawn()
	{
		Precache();
		BaseClass::Spawn();
		SetModel(HYDRA_MODEL);
		SetHullType(HULL_TINY);
		SetHullSizeNormal();
		SetMoveType(MOVETYPE_NONE);
		
		SetSolid(SOLID_BBOX);
		AddSolidFlags(FSOLID_NOT_STANDABLE|FSOLID_CUSTOMBOXTEST|FSOLID_CUSTOMRAYTEST);
		SetBloodColor(BLOOD_COLOR_HYDRA);
		SetCollisionGroup(HL2COLLISION_GROUP_GUNSHIP);
	//	if( CollisionProp()) CollisionProp()->SetSurroundingBoundsType(USE_BEST_COLLISION_BOUNDS);

		m_iHealth = m_iMaxHealth = 10000;

		m_vecHeadDir = Vector(0, 0, 1);
		m_vecRoot = Vector(0, 0, 1);
		m_vecChain.Set(0, GetAbsOrigin());
		m_vecChain.Set(1, WorldSpaceCenter());

	//	m_iszIdleExpression = MAKE_STRING("scenes/di_npc/hydra_pulsating.vcd");
		m_iszAlertExpression = MAKE_STRING("scenes/di_npc/hydra_pulsating.vcd");
		m_iszCombatExpression = MAKE_STRING("scenes/di_npc/hydra_pulsating.vcd");

		m_NPCState = NPC_STATE_NONE;
		NPCInit();
		GetSenses()->AddSensingFlags(SENSING_FLAGS_DONT_LISTEN | SENSING_FLAGS_DONT_LOOK);

		InvalidateBoneCache();

#ifdef USE_NEW_FAKE_COLLISIONS
		CreateCollisionObjects();
#endif
		m_suppressProcedural = false;

		m_proxy_time_to_attack_float = FLT_MAX;
		m_proxy_material_intensity_float = 0;

		SetFlexWeight("body_distend", RandomFloat(0.2, 0.5)); // a sort of tunic-ish look 
	}

	int		UpdateTransmitState()
	{
		return SetTransmitState(FL_EDICT_ALWAYS);
	}

	virtual void GetBonePosition(int iBone, Vector &origin, QAngle &angles)
	{
		CStudioHdr *pStudioHdr = GetModelPtr();
		if (!pStudioHdr)
		{
			Assert(!"CNPC_Hydra::GetBonePosition: model missing");
			return;
		}

		if (iBone < 0 || iBone >= pStudioHdr->numbones())
		{
			Assert(!"CNPC_Hydra::GetBonePosition: invalid bone index");
			return;
		}

		if (iBone < 0 || iBone >= m_vecChain.Count())
		{
			Assert(!"CNPC_Hydra::GetBonePosition: bone index invalid or larger than m_vecChain");
			return;
		}

		if (iBone == 0)
		{
			VectorCopy(m_vecRoot, origin);
			Vector chainDir = m_vecRoot.Get() - m_vecChain[1];
			VectorAngles(chainDir, angles);
			return;
		}

		else if (iBone == GetNumBones())
		{
			VectorCopy(m_vecHeadDir, origin);
			VectorAngles(m_vecHeadDir, angles);
			return;
		}

		VectorCopy(m_vecChain[iBone], origin);
		Vector chainDir = m_vecChain[iBone] - m_vecChain[iBone + 1];
		VectorAngles(chainDir, angles);

	//	matrix3x4_t bonetoworld;
	//	GetBoneTransform(iBone, bonetoworld);

	//	MatrixAngles(bonetoworld, angles, origin);
	}

#if 0
	bool ComputeHitboxSurroundingBox(Vector *pVecWorldMins, Vector *pVecWorldMaxs)
	{
		CStudioHdr *pStudioHdr = GetModelPtr();
		if (!pStudioHdr)
			return false;

		mstudiohitboxset_t *set = pStudioHdr->pHitboxSet(m_nHitboxSet);
		if (!set || !set->numhitboxes)
			return false;

		//CBoneCache *pCache = GetBoneCache();

		// Compute a box in world space that surrounds this entity
		pVecWorldMins->Init(GetAbsOrigin().x - 512, GetAbsOrigin().y - 512, GetAbsOrigin().z - 512);
		pVecWorldMaxs->Init(GetAbsOrigin().x + 512, GetAbsOrigin().y + 512, GetAbsOrigin().z + 512);

		/*
		Vector vecBoxAbsMins, vecBoxAbsMaxs;
		for (int i = 0; i < set->numhitboxes; i++)
		{
			mstudiobbox_t *pbox = set->pHitbox(i);
			matrix3x4_t *pMatrix = pCache->GetCachedBone(pbox->bone);

			if (pMatrix)
			{
				TransformAABB(*pMatrix, pbox->bbmin * GetModelScale(), pbox->bbmax * GetModelScale(), vecBoxAbsMins, vecBoxAbsMaxs);
				VectorMin(*pVecWorldMins, vecBoxAbsMins, *pVecWorldMins);
				VectorMax(*pVecWorldMaxs, vecBoxAbsMaxs, *pVecWorldMaxs);
			}
		}
		*/
		return true;
	}
	
	bool ComputeEntitySpaceHitboxSurroundingBox(Vector *pVecWorldMins, Vector *pVecWorldMaxs)
	{
		CStudioHdr *pStudioHdr = GetModelPtr();
		if (!pStudioHdr)
			return false;

		mstudiohitboxset_t *set = pStudioHdr->pHitboxSet(m_nHitboxSet);
		if (!set || !set->numhitboxes)
			return false;

		//CBoneCache *pCache = GetBoneCache();
		//matrix3x4_t *hitboxbones[MAXSTUDIOBONES];
		//pCache->ReadCachedBonePointers(hitboxbones, pStudioHdr->numbones());

		// Compute a box in world space that surrounds this entity
		pVecWorldMins->Init(GetAbsOrigin().x - 512, GetAbsOrigin().y - 512, GetAbsOrigin().z - 512);
		pVecWorldMaxs->Init(GetAbsOrigin().x + 512, GetAbsOrigin().y + 512, GetAbsOrigin().z + 512);

		/*
		matrix3x4_t worldToEntity, boneToEntity;
		MatrixInvert(EntityToWorldTransform(), worldToEntity);

		Vector vecBoxAbsMins, vecBoxAbsMaxs;
		for (int i = 0; i < set->numhitboxes; i++)
		{
			mstudiobbox_t *pbox = set->pHitbox(i);

			ConcatTransforms(worldToEntity, *hitboxbones[pbox->bone], boneToEntity);
			TransformAABB(boneToEntity, pbox->bbmin, pbox->bbmax, vecBoxAbsMins, vecBoxAbsMaxs);
			VectorMin(*pVecWorldMins, vecBoxAbsMins, *pVecWorldMins);
			VectorMax(*pVecWorldMaxs, vecBoxAbsMaxs, *pVecWorldMaxs);
		}
		*/
		return true;
	}
#endif

	bool TestHitboxes(const Ray_t &ray, unsigned int fContentsMask, trace_t& tr)
	{
		CStudioHdr *pStudioHdr = GetModelPtr();
		if (!pStudioHdr)
		{
			Assert(!"CHydra::GetBonePosition: model missing");
			return false;
		}

		mstudiohitboxset_t *set = pStudioHdr->pHitboxSet(m_nHitboxSet);
		if (!set || !set->numhitboxes)
		{
			return false;
		}
		CBoneCache *pcache = GetBoneCache();

		matrix3x4_t *hitboxbones[MAXSTUDIOBONES];
		pcache->ReadCachedBonePointers(hitboxbones, pStudioHdr->numbones());

		if (TraceToStudio(physprops, ray, pStudioHdr, set, hitboxbones, fContentsMask, GetAbsOrigin(), GetModelScale(), tr))
		{
			mstudiobbox_t *pbox = set->pHitbox(tr.hitbox);
			mstudiobone_t *pBone = pStudioHdr->pBone(pbox->bone);
			tr.surface.name = "**studio**";
			tr.surface.flags = SURF_HITBOX;
			tr.surface.surfaceProps = physprops->GetSurfaceIndex(pBone->pszSurfaceProp());
		}
		return true;
	}

	virtual void SetupBones(matrix3x4_t *pBoneToWorld, int boneMask)
	{
		BaseClass::SetupBones(pBoneToWorld, boneMask);
	}

#ifdef USE_NEW_FAKE_COLLISIONS
	void	CreateCollisionObjects(void)
	{
		// place a bunch of fake damage-taking spheres on our bones
		for (int i = GetNumBones(); i > 4; i--)
		{
			if ((i % 2 == 0) || (i == GetNumBones())) // only place it on half of the bones
			{
				hydraCollision *piece = (hydraCollision *)/*CBaseEntity::*/CreateNoSpawn("hydra_hitbox", GetAbsOrigin(), GetAbsAngles(), this);
				piece->SetParent(GetMoveParent() ? GetMoveParent() : this, -1);
				piece->SetAbsOrigin(GetAbsOrigin());
				piece->SetAbsAngles(GetAbsAngles());
				DispatchSpawn(piece);
				piece->Activate();

				m_collisions.AddToTail(piece);
			}
		}
	}
#endif
	
	void OnRestore()
	{
		BaseClass::OnRestore();

#ifdef USE_NEW_FAKE_COLLISIONS
	//	CreateCollisionObjects(); // these are purposely not remembered and recreated to ease the save/restore burden.
#endif
	}

	void	RunAI(void)
	{
		BaseClass::RunAI();
		
		if (m_anim_sequence_int != -1)
		{
			SetIdealActivity(GetSequenceActivity(m_anim_sequence_int));
		}

//		SetCollisionBounds(Vector(-512, -512, -512), Vector(512, 512, 512));
		
//		matrix3x4_t pBoneToWorld[MAXSTUDIOBONES];

//		SetupBones(pBoneToWorld, BONE_USED_BY_ANYTHING); // FIXME: shouldn't this be a subset of the bones

//		if( hydra_show_collision_objects.GetBool())
//			DrawServerHitboxes(0.1f);

#ifdef USE_NEW_FAKE_COLLISIONS
		// Adjust collisions on bones that have collision entities	
		if (m_collisions.Count() > 0)
		{
			for (int i = GetNumBones(); i > 4; i--)
			{
				CBaseEntity *piece = m_collisions[i];
				if (piece)
				{
					Vector bonePos;
					QAngle boneAng;
					GetBonePosition(i, bonePos, boneAng);
					piece->SetAbsOrigin(bonePos);
					piece->SetAbsAngles(boneAng);
				}
			}
		}
#endif
		SetNextThink(CURTIME + 0.1f);
	}

	void SetAnimLerpEndTime(float fStartTime, float fEndTime)
	{
		m_anim_lerp_start_time_float = fStartTime;
		m_anim_lerp_end_time_float = CURTIME + fEndTime;
	}

	float GetAnimLerpEndTime(void)
	{
		return m_anim_lerp_end_time_float;
	}

	int OnTakeDamage_Alive(const CTakeDamageInfo &info)
	{
		if (GetOwnerEntity()) // the owner should be our hydra. Relay the damage to it.
		{
			GetOwnerEntity()->TakeDamage(info);
		}
		return 0;
	}

	int SelectSchedule(void)
	{
		return SCHED_WAIT_FOR_SCRIPT; // don't override animations and things like that.
	}
	
	static hydraDummy *CreateDummyModel(const Vector &vecOrigin, const QAngle &vecAngles);
	
	CNetworkArray(Vector, m_vecChain, CHAIN_LINKS);
	CNetworkVar(float,  m_lengthRelaxed);
	CNetworkVector( m_vecRoot);
	CNetworkVector( m_vecHeadDir);
	CNetworkVar(float, m_anim_lerp_start_time_float);
	CNetworkVar(float, m_anim_lerp_end_time_float);
	CNetworkVar(bool, m_suppressProcedural); // tell the model to animate traditionally instead of calculating bone setups
	CNetworkVar(float, m_proxy_time_to_attack_float);
	CNetworkVar(float, m_proxy_material_intensity_float);

	int m_anim_sequence_int;
};

BEGIN_DATADESC(hydraDummy)
DEFINE_AUTO_ARRAY(m_vecChain, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_anim_sequence_int, FIELD_INTEGER),
#ifdef USE_NEW_FAKE_COLLISIONS
DEFINE_UTLVECTOR(m_collisions, FIELD_EHANDLE),
#endif
END_DATADESC()

hydraDummy *hydraDummy::CreateDummyModel(const Vector &vecOrigin, const QAngle &vecAngles)
{
	hydraDummy *dummy = (hydraDummy *)CBaseEntity::Create("hydra_dummy", vecOrigin, vecAngles);
	if (!dummy)
		return NULL;

//	dummy->AddSolidFlags(FSOLID_NOT_SOLID);
	// Disable movement on the root, we'll move this thing manually.
	// dummy->VPhysicsInitShadow(false, false);
	dummy->SetMoveType(MOVETYPE_NONE);
	return dummy;
};
#endif 
//------------------------------------------------------------------
// BaseHydra: abstract class that does the bulk of the work.
//------------------------------------------------------------------
ConVar sk_hydra_stab_dmg("sk_hydra_stab_dmg", "10");
ConVar sk_hydra_health("sk_hydra_health", "150");
ConVar sk_hydra_memory_time("sk_hydra_memory_time", "10", 0, "How long the Hydra remembers her enemies after losing sight of them");
ConVar hydra_showvectors("hydra_debug_showvectors", "0", 0, "0 = off, 1 = primary skeletone vectors, 2 = spline segments vectors");
//ConVar hydra_base_length("hydra_base_length", "120.0f", 0);
//ConVar hydra_relaxed_length("hydra_length_relaxed", "500.0f", 0 );
//ConVar hydra_coil_enable("hydra_coil_enable", "1");
//ConVar hydra_coil_radius("hydra_coil_factor", "1000.0f");
//ConVar hydra_coil_mult("hydra_coil_mult", "0.2");
//ConVar hydra_attack_momentum("hydra_attack_momentum", "15.0f");

ConVar hydra_segment_gravity("hydra_segment_gravity", "0.5", FCVAR_ARCHIVE, "");
static ConVar hydra_segment_speed("hydra_segment_speed", "0.3", FCVAR_ARCHIVE, "");
static ConVar hydra_bend_sin_max("hydra_bend_sin_max", "2.0f");
static ConVar hydra_bend_sin_min("hydra_bend_sin_min", "0.2f");
static ConVar hydra_bend_sin_time("hydra_bend_sin_time", "10.0f");
static ConVar hydra_bend_tension("hydra_bend_tension", "0.4", FCVAR_ARCHIVE, "Hydra Slack");
static ConVar hydra_bend_delta("hydra_bend_delta", "200", FCVAR_ARCHIVE, "Hydra Slack");
static ConVar hydra_goal_tension("hydra_goal_tension", "0.5", FCVAR_ARCHIVE, "Hydra Slack");
static ConVar hydra_goal_delta("hydra_goal_delta", "400", FCVAR_ARCHIVE, "Hydra Slack");
static ConVar hydra_max_velocity("hydra_max_velocity", "1000", FCVAR_ARCHIVE, "Terminal velocity during attacks");

static ConVar hydra_hover_frequency("hydra_hover_frequency", "2", FCVAR_ARCHIVE, "");
static ConVar hydra_hover_max_vert("hydra_hover_max_vert", "8", FCVAR_ARCHIVE, "");
static ConVar hydra_hover_max_hor("hydra_hover_max_hor", "4", FCVAR_ARCHIVE, "");

Activity ACT_HYDRA_PIPE_UP;
Activity ACT_HYDRA_PIPE_RIGHT_A;
Activity ACT_HYDRA_PIPE_RIGHT_B;
Activity ACT_HYDRA_PIPE_RIGHT_C;
Activity ACT_HYDRA_PIPE_DOWN;

class CHydra : public CAI_BaseNPC
{
	DECLARE_CLASS(CHydra, CAI_BaseNPC)
public:
	DECLARE_DATADESC();

	CHydra(void) 
	{
	//	m_override_path_handle = NULL;
	//	m_hydra_forced_along_path = false;
		m_pIdleSound = NULL;
		m_hover_horizontal_float = 0;
		m_hover_vertical_float = 0;
	};
	~CHydra(void) 
	{
	//	m_override_path_handle = NULL;
		m_segs.RemoveAll();
#ifndef USE_NEW_FAKE_COLLISIONS
		m_collisions.RemoveAll();
#endif
		if (m_dummyModel) UTIL_Remove(m_dummyModel);
	};

	virtual void	Precache();
	virtual void	Spawn();
	Class_T Classify(void) { return CLASS_BARNACLE; }
	virtual void	ComputeWorldSpaceSurroundingBox(Vector *pVecWorldMins, Vector *pVecWorldMaxs);
	CSound			*GetBestSound(int validTypes = ALL_SOUNDS);
	virtual void	Activate();
	virtual void    OnRestore();
	virtual void	RunAI();
	virtual void	SearchSound(void);
	virtual void	PreAttackSound(void);
	virtual void	PainSound(const CTakeDamageInfo &info);
	virtual int		OnTakeDamage_Alive(const CTakeDamageInfo &info);
	virtual void	StartTask(const Task_t *pTask);
	virtual void	RunTask(const Task_t *pTask);
	Activity		NPC_TranslateActivity(Activity activity);
	virtual void	PrescheduleThink(void);
	virtual int		TranslateSchedule(int scheduleType);
	virtual int		SelectSchedule(void);
	virtual void	GatherConditions(); 
	bool			IsValidEnemy(CBaseEntity *pEnemy);
	virtual bool	OverrideMove(float flInterval);
	virtual void	DoMovement(float flInterval, const Vector &MoveTarget);
	virtual void	SteerArrive(Vector &Steer, const Vector &Target);
	virtual void	SteerSeek(Vector &Steer, const Vector &Target);
//	virtual int		GetSoundInterests(void) { return (SOUND_BUGBAIT); }
	virtual int		UpdateTransmitState()
	{
		return SetTransmitState(FL_EDICT_ALWAYS);
	}

	float	m_lengthRelaxed;
	float	m_lengthTotal;
	Vector	m_vecHeadDir;
	Vector	m_vecRoot;
	void	MapDummyModelToVectors(void);
	bool	m_suppressProcedural;

#ifndef USE_NEW_FAKE_COLLISIONS
	void	CreateCollisionObjects(void);
	void	InputCollisionObjectDestroyed(inputdata_t &inputdata);
#endif
	void	InputHideAndReappear(inputdata_t &inputdata);
	void	InputHideAndDestroy(inputdata_t &inputdata);
	void	InputBecomeRagdoll(inputdata_t &inputdata);
	void	InputSetRootEntity(inputdata_t &inputdata);
	void	InputForceBodyAlongPath(inputdata_t &inputdata);
	void	InputClearForcedPath(inputdata_t &inputdata);
	void	InputSetBodyThickness(inputdata_t &inputdata); 
	void	InputSetMaxLength(inputdata_t &inputdata);
	void	InputSetIdealSegLength(inputdata_t &inputdata);
	void	InputSetAnimLerpEndTime(inputdata_t &inputdata);
	void	InputSetAnimation(inputdata_t &inputdata);
	void	InputFlinch(inputdata_t &inputdata);
	void	InputSetHeadInfluence(inputdata_t &inputdata);

	static const Vector	m_vecAccelerationMax;
	static const Vector	m_vecAccelerationMin;

	DEFINE_CUSTOM_AI;
private:
	bool	m_hydra_is_dead_bool;
//	bool	m_hydra_forced_along_path;
//	int		m_hydra_forced_path_length; 
	float	m_hydra_max_length_float;
	float	m_hydra_ideal_seglength_float;
	float	m_first_seen_enemy_float; // not necessary to remember so it's not in datadesc
//	EHANDLE m_override_path_handle;
	Vector  m_vecBase;
	Vector  m_vecNeck;
	Vector	m_vecHead;
	Vector  m_vecStab;  // Look for detail in StartTask() (TASK_MELEE_ATTACK1)
	Vector  m_vecSaved;  // Look for detail in StartTask() (TASK_MELEE_ATTACK1)
	float	m_headsegweight_float;
	float	m_maxStabDist;
	float	m_mainBendCoeff;
	float   m_rndBendMin_float;
	float   m_rndBendMax_float;
	float   m_rndBendDelta_float;
	bool	m_suppressBodyMovement;
	int		m_rndBendDir_int;
	float   m_nextpainsound_time;
	float   m_nextpreattacksound_time;
	float	m_hover_horizontal_float; // -64 (viewer's left) to 64
	float	m_hover_vertical_float; // -64 (viewer's down) to 64
	CUtlVector <hydraSeg> m_segs;
#ifndef USE_NEW_FAKE_COLLISIONS
	CUtlVector	 <EHANDLE>	m_collisions;
#endif
	CHandle <hydraDummy> m_dummyModel;
	void	CalcBodyVectors(void);
	void	MoveBody(void);
	void	DrawDebugOverlays(void);
	void	UndulateThink(void);
	bool	CheckHeadCollision(void);
	int		SelectCombatSchedule(void);	
	CSoundPatch	*m_pIdleSound;

	bool	GetGoalDirection(Vector *vOut);

	enum
	{
		SCHED_HYDRA_IDLE_PATROL = LAST_SHARED_SCHEDULE,
		SCHED_HYDRA_ATTACK_STAB,
		SCHED_HYDRA_HIDE_AND_REAPPEAR,
		SCHED_HYDRA_HIDE_AND_DESTROY,
		SCHED_HYDRA_DIE_RAGDOLL,
		SCHED_HYDRA_HOVER,

		LAST_SHARED_SCHEDULE,
	};

	enum
	{
		TASK_MYCUSTOMTASK = LAST_SHARED_TASK,
		TASK_HYDRA_STAB,
		TASK_HYDRA_RETRACT,
		TASK_HYDRA_GET_RETRACT_POS,
		TASK_HYDRA_CREATE_DEATH_RAGDOLL,
		TASK_HYDRA_HIDE,
		TASK_HYDRA_FIND_REAPPEAR_POINT,
		TASK_HYDRA_REAPPEAR,
		TASK_HYDRA_DESTROY,
		TASK_HYDRA_DEPLOY_ALONG_PATH,
		TASK_HYDRA_HOVER,

		LAST_SHARED_TASK,
	};

	enum
	{
		COND_MYCUSTOMCONDITION = LAST_SHARED_CONDITION,
		COND_HYDRA_CAN_ATTACK,
	};
};

//static const char *UNDULATE_CONTEXT = "undulate_context";

BEGIN_DATADESC(CHydra)
DEFINE_KEYFIELD(m_vecRoot, FIELD_VECTOR, "vecRoot"),
DEFINE_KEYFIELD(m_hydra_max_length_float, FIELD_FLOAT, "maxlength"),
DEFINE_KEYFIELD(m_hydra_ideal_seglength_float, FIELD_FLOAT, "idealseglength"),
DEFINE_FIELD(m_headsegweight_float, FIELD_FLOAT),
//DEFINE_FIELD(m_hydra_forced_along_path, FIELD_BOOLEAN),
//DEFINE_FIELD(m_override_path_handle, FIELD_EHANDLE),
DEFINE_FIELD(m_vecBase, FIELD_VECTOR),
// no need to remember rest of the body vectors - they're calculated on runtime
DEFINE_FIELD(m_vecStab, FIELD_VECTOR),
DEFINE_FIELD(m_vecSaved, FIELD_VECTOR),
DEFINE_FIELD(m_maxStabDist, FIELD_FLOAT),
DEFINE_FIELD(m_hydra_is_dead_bool, FIELD_BOOLEAN),
DEFINE_FIELD(m_dummyModel, FIELD_EHANDLE),
DEFINE_UTLVECTOR(m_segs, FIELD_EMBEDDED),
#ifndef USE_NEW_FAKE_COLLISIONS
DEFINE_UTLVECTOR(m_collisions, FIELD_EHANDLE),
#endif
DEFINE_FIELD(m_mainBendCoeff, FIELD_FLOAT),
DEFINE_FIELD(m_suppressBodyMovement, FIELD_BOOLEAN),
DEFINE_FIELD(m_nextpainsound_time, FIELD_TIME),
DEFINE_FIELD(m_nextpreattacksound_time, FIELD_TIME),
DEFINE_FIELD(m_suppressProcedural, FIELD_BOOLEAN),
DEFINE_FIELD(m_hover_vertical_float, FIELD_FLOAT),
DEFINE_FIELD(m_hover_horizontal_float, FIELD_FLOAT),

#ifndef USE_NEW_FAKE_COLLISIONS
DEFINE_INPUTFUNC(FIELD_INTEGER, "DestroyNumberedHitbox", InputCollisionObjectDestroyed),
#endif
DEFINE_INPUTFUNC(FIELD_VOID, "HideAndReappear", InputHideAndReappear),
DEFINE_INPUTFUNC(FIELD_VOID, "HideAndDestroy", InputHideAndDestroy),
DEFINE_INPUTFUNC(FIELD_VOID, "BecomeRagdoll", InputBecomeRagdoll),
DEFINE_INPUTFUNC(FIELD_STRING, "SetRootEnity", InputSetRootEntity),
DEFINE_INPUTFUNC(FIELD_STRING, "ForceBodyAlongPath", InputForceBodyAlongPath),
DEFINE_INPUTFUNC(FIELD_VOID, "ClearForcedPath", InputClearForcedPath),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetBodyThickness", InputSetBodyThickness),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetMaxLength", InputSetMaxLength),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetIdealSegmentLength", InputSetIdealSegLength),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetAnimLerpEndTime", InputSetAnimLerpEndTime),
DEFINE_INPUTFUNC(FIELD_STRING, "SetAnimation", InputSetAnimation),
DEFINE_INPUTFUNC(FIELD_VOID, "Flinch", InputFlinch),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetHeadInfluence", InputSetHeadInfluence),

DEFINE_THINKFUNC(UndulateThink),
DEFINE_SOUNDPATCH(m_pIdleSound),
END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_hydra, CHydra);
AI_BEGIN_CUSTOM_NPC(npc_hydra, CHydra)

DECLARE_TASK(TASK_HYDRA_STAB)
DECLARE_TASK(TASK_HYDRA_RETRACT)
DECLARE_TASK(TASK_HYDRA_GET_RETRACT_POS)
DECLARE_TASK(TASK_HYDRA_HIDE)
DECLARE_TASK(TASK_HYDRA_CREATE_DEATH_RAGDOLL)
DECLARE_TASK(TASK_HYDRA_FIND_REAPPEAR_POINT)
DECLARE_TASK(TASK_HYDRA_REAPPEAR)
DECLARE_TASK(TASK_HYDRA_DESTROY)
DECLARE_TASK(TASK_HYDRA_DEPLOY_ALONG_PATH)
DECLARE_TASK(TASK_HYDRA_HOVER)
DECLARE_CONDITION(COND_MYCUSTOMCONDITION)
DECLARE_CONDITION(COND_HYDRA_CAN_ATTACK)
DECLARE_ACTIVITY( ACT_HYDRA_PIPE_UP)
DECLARE_ACTIVITY( ACT_HYDRA_PIPE_RIGHT_A)
DECLARE_ACTIVITY( ACT_HYDRA_PIPE_RIGHT_B)
DECLARE_ACTIVITY( ACT_HYDRA_PIPE_RIGHT_C)
DECLARE_ACTIVITY( ACT_HYDRA_PIPE_DOWN)

DEFINE_SCHEDULE
(
	SCHED_HYDRA_HOVER,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_HYDRA_HOVER		0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_HYDRA_CAN_ATTACK"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
);

DEFINE_SCHEDULE
(
	SCHED_HYDRA_ATTACK_STAB,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
	"		TASK_MELEE_ATTACK1		0"
	"		TASK_HYDRA_RETRACT		0"
	""
	"	Interrupts"
	"		COND_NO_CUSTOM_INTERRUPTS"
);

DEFINE_SCHEDULE
(
	SCHED_HYDRA_IDLE_PATROL,

	"	Tasks"
	"		TASK_SET_TOLERANCE_DISTANCE		48"
	"		TASK_SET_ROUTE_SEARCH_TIME		5"	// Spend 5 seconds trying to build a path if stuck
	"		TASK_GET_PATH_TO_RANDOM_NODE	256"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
);

DEFINE_SCHEDULE
(
	SCHED_HYDRA_DIE_RAGDOLL,

	"	Tasks"
	"		TASK_STOP_MOVING				0"
	"		TASK_HYDRA_CREATE_DEATH_RAGDOLL	0"
	"		TASK_HYDRA_DESTROY				0"
	""
	"	Interrupts"
	"		COND_NO_CUSTOM_INTERRUPTS"
);

DEFINE_SCHEDULE
(
	SCHED_HYDRA_HIDE_AND_REAPPEAR,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_HYDRA_RETRACT		0"
	"		TASK_HYDRA_HIDE			0"
	"		TASK_WAIT				5"
	"		TASK_HYDRA_FIND_REAPPEAR_POINT 0"
	"		TASK_HYDRA_REAPPEAR		0"
	""
	"	Interrupts"
	"		COND_NO_CUSTOM_INTERRUPTS"
);

DEFINE_SCHEDULE
(
	SCHED_HYDRA_HIDE_AND_DESTROY,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_HYDRA_DESTROY	0"
	""
	"	Interrupts"
	"		COND_NO_CUSTOM_INTERRUPTS"
);

AI_END_CUSTOM_NPC()

const Vector CHydra::m_vecAccelerationMax = Vector(1024, 1024, 128);
const Vector CHydra::m_vecAccelerationMin = Vector(-1024, -1024, -512);

void CHydra::Precache(void)
{
	BaseClass::Precache();
	PrecacheModel(HYDRA_MODEL);
	PrecacheModel(HYDRA_RAGDOLL_MODEL);
	PrecacheModel("models/_Monsters/Xen/Hydra_head.mdl");
	PrecacheScriptSound("NPC_Hydra.Attack");
	PrecacheScriptSound("NPC_Hydra.Bump");
	PrecacheScriptSound("NPC_Hydra.ExtendTentacle");
	PrecacheScriptSound("NPC_Hydra.HeartbeatIdle");
	PrecacheScriptSound("NPC_Hydra.HeartbeatFast");
	PrecacheScriptSound("NPC_Hydra.Pain");
	PrecacheScriptSound("NPC_Hydra.Search");
	PrecacheScriptSound("NPC_Hydra.AttackCue");
	PrecacheScriptSound("NPC_Hydra.Strike");
	PrecacheScriptSound("NPC_Hydra.Stab");
	PrecacheParticleSystem("blood_hydra_split");
	PrecacheParticleSystem("blood_hydra_split_spray");
	PrecacheParticleSystem("blood_impact_blue_01"); 
	UTIL_PrecacheOther("hydra_dummy");
}

void CHydra::Spawn(void)
{
	Precache();
//	BaseClass::Spawn();

	SetModel("models/_Monsters/Xen/Hydra_head.mdl");
		
	SetHullType(HULL_TINY_CENTERED); 
	SetHullSizeNormal();
	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE); // | FSOLID_CUSTOMBOXTEST | FSOLID_CUSTOMRAYTEST);
	SetCollisionGroup(HL2COLLISION_GROUP_GUNSHIP); // fixme

	SetMoveType(MOVETYPE_FLYGRAVITY);
	SetNavType(NAV_FLY);
	AddFlag(FL_FLY);
	SetGravity(UTIL_ScaleForGravity(300));
	m_flGroundSpeed = 200.0f;

	m_iHealth = m_iMaxHealth = sk_hydra_health.GetInt();
	m_flFieldOfView = VIEW_FIELD_FULL;
	m_NPCState = NPC_STATE_NONE;
	SetBloodColor(BLOOD_COLOR_HYDRA);
	SetRenderMode(kRenderNone);
	AddEffects(EF_NOSHADOW);
	
	CapabilitiesAdd(bits_CAP_MOVE_FLY);
	CapabilitiesAdd(bits_CAP_SQUAD);
//	CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
//	CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);

	m_flNextAttack = CURTIME; 
	m_vecStab = vec3_origin;
		
	NPCInit();
					
	GetAttachment(1, m_vecHead, NULL); // FIXME: make attachment looked up and cached, not hardcoded
	m_vecNeck = m_vecHead - Vector(0, 0, 16);

	// if parented to something, take the root ouf of the origin of the parent
	if (m_target != NULL_STRING)
	{
		string_t strTarget = m_target;
		CBaseEntity *pRootTarget = NULL;
		while((pRootTarget = gEntList.FindEntityGeneric(pRootTarget, STRING(strTarget), this, this)) != NULL)
		{
			m_vecRoot = pRootTarget->WorldSpaceCenter();
		}
	}
	// base is elevated a bit above the root.
	// This works under any orientation because it takes
	// root position, which we set in Hammer, and spawn origin,
	// and this is calculated at spawn, i. e. once for the NPC. 
	m_vecBase = VectorLerp(m_vecRoot, m_vecHead, 0.05);
	
	hydraSeg seg;
	seg.Pos = m_vecRoot;	//root; seg #1
	seg.nextUpdate = CURTIME;
	m_segs.AddToTail(seg);

	seg.Pos = m_vecBase;		//base; seg #2
	m_segs.AddToTail(seg);

	for (int i = 0; i < CHAIN_LINKS - 4; i++) // 27 body segs; #3-30
	{
		seg.Pos = m_vecBase;		
		m_segs.AddToTail(seg);
	}

	seg.Pos = m_vecNeck;	// nec; seg #15
	m_segs.AddToTail(seg);

	seg.Pos = m_vecHead;	// head; seg #16
	m_segs.AddToTail(seg);

	// Extra blank seg in front of the head. // seg #17
	seg.Pos = m_vecRoot;
	m_segs.AddToTail(seg);
	
	for (int i = 0; i < CHAIN_LINKS - 3; i++)
	{
		Catmull_Rom_Spline(m_vecRoot, m_vecBase, m_vecNeck, m_vecHead, 0.03571 * i, m_segs[i + 2].Pos);
	}

#ifndef USE_NEW_FAKE_COLLISIONS
	// Each object will create the collision entity which follow their segments
	CreateCollisionObjects();
#endif
	m_mainBendCoeff = 1.0f;
	m_lengthRelaxed = m_hydra_max_length_float;
	m_vecHeadDir = Vector(0, 0, 1);
	m_headsegweight_float = 1.0f;

	m_dummyModel = hydraDummy::CreateDummyModel(m_vecRoot, QAngle(0, 0, 0));
	
	m_dummyModel->m_vecHeadDir = m_vecHeadDir;
	m_dummyModel->m_vecRoot = m_vecRoot;
	
	m_dummyModel->m_vecChain.Set(0, m_vecRoot);
	m_dummyModel->m_vecChain.Set(1, m_vecHead);

	m_dummyModel->SetOwnerEntity(this);

	m_suppressBodyMovement = false;

	m_rndBendMin_float = hydra_bend_sin_min.GetFloat() + RandomFloat(-0.1f, 0.25f);
	m_rndBendMax_float = hydra_bend_sin_max.GetFloat() + RandomFloat(-0.25f, 0.25f);
	m_rndBendDelta_float = hydra_bend_sin_time.GetFloat() + RandomFloat(-1.0f, 1.0f);
	m_rndBendDir_int = RandomInt(0, 1);
	
	m_nextpainsound_time = CURTIME;
	m_nextpreattacksound_time = CURTIME;

	m_suppressProcedural = false;
//	m_hydra_forced_path_length = 1;

	if (m_hydra_max_length_float <= 0) m_hydra_max_length_float = 500;
	if (m_hydra_ideal_seglength_float <= 0) m_hydra_ideal_seglength_float = 3;

	RegisterThinkContext("UndulateContext");
	SetContextThink(&CHydra::UndulateThink, CURTIME + RandomFloat(0.1f, 1.0f), "UndulateContext");

	SetIdealActivity(ACT_IDLE);

	m_OnSpawned.FireOutput(this, this);
}

void CHydra::ComputeWorldSpaceSurroundingBox(Vector *pVecWorldMins, Vector *pVecWorldMaxs)
{
	Vector origin;
	GetAttachment(1, origin);
	const Vector &vecAbsOrigin = origin;
	
	VectorAdd(GetHullMins(), vecAbsOrigin, *pVecWorldMins);
	VectorAdd(GetHullMaxs(), vecAbsOrigin, *pVecWorldMaxs);
}

CSound *CHydra::GetBestSound(int validTypes)
{
	AISoundIter_t iter;

	CSound *pCurrentSound = GetSenses()->GetFirstHeardSound(&iter);
	while (pCurrentSound)
	{
		// the npc cares about this sound, and it's close enough to hear.
		if (pCurrentSound->FIsSound())
		{
			if (pCurrentSound->SoundContext() & SOUND_CONTEXT_GUNFIRE)
			{
				return pCurrentSound;
			}
		}

		pCurrentSound = GetSenses()->GetNextHeardSound(&iter);
	}

	return BaseClass::GetBestSound(validTypes);
}

void CHydra::Activate(void)
{
	BaseClass::Activate();
}

void CHydra::OnRestore()
{
	BaseClass::OnRestore();

	// No need to memorise these. It's ok if a Hydra gets new values after
	// reloading a save. 
	m_rndBendMin_float = hydra_bend_sin_min.GetFloat() + RandomFloat(-0.1f, 0.25f);
	m_rndBendMax_float = hydra_bend_sin_max.GetFloat() + RandomFloat(-0.25f, 0.25f);
	m_rndBendDelta_float = hydra_bend_sin_time.GetFloat() + RandomFloat(-1.0f, 1.0f);
	m_rndBendDir_int = RandomInt(0, 1);
}

void CHydra::RunAI(void)
{
	BaseClass::RunAI();

	if (UTIL_FindClientInPVS(edict()))
	{
		CalcBodyVectors();
		MoveBody();
		DrawDebugOverlays();
	}
	
	MapDummyModelToVectors();

#ifndef USE_NEW_FAKE_COLLISIONS
	// Adjust collisions on bones that have collision entities	
	if (m_collisions.Count() > 0)
	{
		for (int i = 2; i < m_segs.Count() - 1; i++)
		{
			CBaseEntity *piece = m_collisions[i - 2];
			if (!piece)	continue;	// The prop has been broken or doesn't exist
			piece->SetAbsOrigin(m_segs[i].Pos);

		//	Vector pieceDir = m_segs[i - 1].Pos - m_segs[i].Pos; // we're not using directional hitboxes atm (they're all spherical), skip
		//	QAngle pieceAng;
		//	VectorAngles(pieceDir, pieceAng);
		//	piece->SetAbsAngles(pieceAng);
		}
	}
#endif

	if( m_dummyModel) m_dummyModel->InvalidateBoneCache();
	SetNextThink(CURTIME + 0.05f);
}

void CHydra::MapDummyModelToVectors(void)
{
	Vector targetDir;
	if (GetEnemy())
	{
		targetDir = GetEnemy()->WorldSpaceCenter() - m_vecHead;
	}
	else
	{
		targetDir = m_segs[m_segs.Count() - 3].Pos - m_vecHead;
	}
	VectorNormalize(targetDir);

	m_dummyModel->SetAbsOrigin(m_vecRoot); // hack for dummies that fell down on ground (despite! movetype_none)
	m_vecHeadDir = /*m_vecHead +*/ targetDir /** 30*/;
	m_dummyModel->m_vecHeadDir = /*m_dummyModel->GetAnimLerpEndTime() > 0 ? (m_vecHead - m_vecRoot) :*/ m_vecHeadDir;	
	m_dummyModel->m_lengthRelaxed = m_lengthRelaxed;

	for (int y = 1; y <= m_segs.Count() - 1; y++)
	{
		m_dummyModel->m_vecChain.Set(y - 1, m_segs[y].Pos);
	}

	m_dummyModel->m_suppressProcedural = m_suppressProcedural;

	m_dummyModel->m_nSkin = m_nSkin;

	m_dummyModel->DispatchUpdateTransmitState();
}

void CHydra::CalcBodyVectors()
{
#if 1 // new body math
	
	// Four vectors out of total CHAIN_LINKS (32) are not procedural.
	GetAttachment(1, m_vecHead, NULL); // Head follows the floating origin entity/npc // FIXME: make attachment looked up and cached, not hardcoded 
	Vector up = Vector(0, 0, 1);
	Vector front;
	GetVectors(&front, NULL, NULL);
//	Msg("m_mainBendCoeff is %.1f\n", m_mainBendCoeff);
	m_vecNeck = m_vecHead + up * 8.0f * m_mainBendCoeff;
	if (GetEnemy())
	{
		Vector enemyDir = GetAbsOrigin() - GetEnemy()->GetAbsOrigin();
		VectorNormalize(enemyDir);
		m_vecNeck = m_vecNeck + enemyDir * 64;
	}

//	VectorLerp(m_vecBase, m_vecNeck, 0.9, m_vecNeck); // make neck more dynamic, dependent not only on head, but body too

	m_segs[m_segs.Count() - 1].Pos = m_vecHead;
	m_segs[m_segs.Count() - 2].Pos = m_vecNeck;

	if (m_target != NULL_STRING)
	{
		string_t strTarget = m_target;
		CBaseEntity *pRootTarget = NULL;
		while ((pRootTarget = gEntList.FindEntityGeneric(pRootTarget, STRING(strTarget), this, this)) != NULL)
		{
			m_vecRoot = pRootTarget->WorldSpaceCenter();
			m_vecBase = VectorLerp(m_vecRoot, m_vecHead, 0.05);
		}
	}

	// root and base are either predefined or taken out of parent entity
	m_segs[0].Pos = m_vecRoot;
	m_segs[1].Pos = m_vecRoot; //m_vecBase;

	int i;

	int iFirst = 2;
	int iLast = m_segs.Count() - 3;

	// keep head segment straight
	m_segs[iLast].GoalPos = m_vecNeck; // + m_vecHeadDir * m_body[iLast-1].flActualLength;
	m_segs[iLast].Weight = m_headsegweight_float; // m_flHeadGoalInfluence;

	m_segs[iLast - 1].GoalPos = m_vecNeck - up * m_hydra_ideal_seglength_float;
	m_segs[iLast - 1].Weight = 1.0;

	// momentum?
	for (i = iFirst; i <= iLast; i++)
	{
		m_segs[i].Dir = m_segs[i].Dir * hydra_segment_speed.GetFloat();
	}

	float flGoalSegmentLength = m_mainBendCoeff * m_hydra_ideal_seglength_float * (m_hydra_max_length_float / m_lengthTotal);

	// goal forces
	for (i = iFirst; i <= iLast; i++)
	{
		float flInfluence = m_segs[i].Weight;
		if (flInfluence > 0)
		{
			m_segs[i].Weight = 0.0;

			Vector v0 = (m_segs[i].GoalPos - m_segs[i].Pos);
			float length = v0.Length();
			if (length > hydra_goal_delta.GetFloat())
			{
				v0 = v0 * hydra_goal_delta.GetFloat() / length;
			}
			m_segs[i].Dir += v0 * flInfluence * hydra_goal_tension.GetFloat();
		}
	}

	// bending forces
	for (i = iFirst - 1; i <= iLast - 1; i++)
	{
		Vector v3 = m_segs[i + 1].Pos - m_segs[i - 1].Pos;
		VectorNormalize(v3);

		Vector delta;
		float length;

		if (i + 1 <= iLast)
		{
			// towards head
			delta = (m_segs[i].Pos + v3 * flGoalSegmentLength - m_segs[i + 1].Pos) * hydra_bend_tension.GetFloat();
			length = delta.Length();
			if (length > hydra_bend_delta.GetFloat())
			{
				delta = delta * (hydra_bend_delta.GetFloat() / length) * m_mainBendCoeff;
			}
			m_segs[i + 1].Dir += delta;
		}

		if (i - 1 >= iFirst)
		{
			// towards tail
			delta = (m_segs[i].Pos - v3 * flGoalSegmentLength - m_segs[i - 1].Pos) * hydra_bend_tension.GetFloat();
			length = delta.Length();
			if (length > hydra_bend_delta.GetFloat())
			{
				delta = delta * (hydra_bend_delta.GetFloat() / length) * m_mainBendCoeff;
			}
			m_segs[i - 1].Dir += delta * 0.8;
		}
	}

	m_segs[0].Dir = Vector(0, 0, 0);
	m_segs[1].Dir = Vector(0, 0, 0);

	// normal gravity forces
	for (i = iFirst; i <= iLast; i++)
	{
		if (!m_segs[i].Stuck)
		{
			m_segs[i].Dir.z -= hydra_segment_gravity.GetFloat();
		}
	}
	
	// prevent stretching
	int maxChecks = m_segs.Count() * 4;
	i = iLast;
	while (i > iFirst && maxChecks > 0)
	{
		bool didStretch = false;
		Vector stretch = (m_segs[i].Pos + m_segs[i].Dir) - (m_segs[i - 1].Pos + m_segs[i - 1].Dir);
		float t = VectorNormalize(stretch);
		if (t > flGoalSegmentLength)
		{
			float f0 = DotProduct(m_segs[i].Dir, stretch);
			float f1 = DotProduct(m_segs[i - 1].Dir, stretch);
			if (f0 > 0 && f0 > f1)
			{
				Vector limit = stretch * (t - flGoalSegmentLength);
				// propagate pulling back down the chain
				m_segs[i].Dir -= limit * 0.5;
				m_segs[i - 1].Dir += limit * 0.5;
				didStretch = true;
			}
		}
		if (didStretch)
		{
			if (i < iLast)
			{
				i++;
			}
		}
		else
		{
			i--;
		}
		maxChecks--;
	}
	
	// Limit the effective used length of the hydra
	// so that a lot of the model's length is 'reserved' at the root
	// Adjust it because it otherwise means that only
	// a few bones after the head are effectively used.
#if 0
	m_lengthTotal = 0;
	for (int k = 1; k < m_segs.Count() - 1; k++)
	{
		float length = (m_segs[k].Pos - m_segs[k - 1].Pos).Length();

		AssertMsg(m_segs[k + 1].Pos.IsValid(), "Invalid pos vector at Hydra seg %i", k + 1);
		AssertMsg(m_segs[k].Pos.IsValid(), "Invalid pos vector at Hydra seg %i", k);
		AssertMsg(IsFinite(length), "Non-finite length between two Hydra segs!");

		m_segs[k].Length = length;
		m_lengthTotal += length;
		m_lengthRelaxed = m_lengthTotal + length / (m_segs.Count() - 1);
	}
#endif
	m_lengthTotal = (m_vecRoot - m_vecBase).Length() + (m_vecBase - m_vecNeck).Length() + (m_vecNeck - m_vecHead).Length();
	m_lengthRelaxed = m_lengthTotal * 1.5f;

#if 0
		
	// this radial arc will let us establish the coiling spiral
	Vector vecOffset;
	float flSin, flCos;
	float flRad = 0; // +/- 30 deg

	for (int j = 2; j < m_segs.Count() - 3; j++)
	{
		continue;
		// use bodyVectors here to prevent "feedback loop" from using segments' .Pos
		Vector segLine = m_segs[j - 1].Pos - m_segs[j + 1].Pos;
		m_segs[j].Dir = up + segLine * 0.1;

		// each segment step, increase the radial angle by 5 x step
		float coilMult = hydra_coil_mult.GetFloat();
		flRad += M_PI / 30 * coilMult * j;
		SinCos(flRad, &flSin, &flCos);

		// Rotate the directional vector of each segment
		Vector vecArc;
		QAngle vecAngles;
		VectorAngles(m_segs[j].Dir, vecAngles);
		VectorRotate(Vector(0.0f, flCos, flSin), vecAngles, vecArc);

		// now use that rotated vector to project goal pos from each segment.
		// Tying coil radius to linear body length makes it so when
		// stretching out, hydra becomes less coiled.
		// And tying coilFactor to bend coefficient makes it look dynamic.
		float lengthFactor = (m_vecBase - m_vecNeck).Length();
		float coilFactor = hydra_coil_radius.GetFloat() / lengthFactor;
		
		if (m_lifeState == LIFE_DYING) coilFactor = 0;
		if (hydra_coil_enable.GetBool())
			m_segs[j].GoalPos = m_segs[j].Pos + vecArc * coilFactor * m_mainBendCoeff;

		Vector segGoalDir = m_segs[j].Pos - m_segs[j].GoalPos;

		if (m_segs[j].Pos.DistTo(m_segs[j].GoalPos) > hydra_segment_range.GetFloat())
		{
			m_segs[j].Pos = m_segs[j].Pos + segGoalDir * hydra_segment_speed.GetFloat();
		}
	}
#endif
#endif
}

void CHydra::MoveBody()
{
	// if in override animation, move all segments to animated bone positions.
	if (false) //(m_dummyModel != NULL && m_dummyModel->GetAnimLerpEndTime() > 0)
	{
		for (int i = 0; i < m_dummyModel->GetNumBones(); i++)
		{
			if (i == 0) m_segs[i].Pos = m_vecRoot; continue;
			if (i > m_segs.Count()) continue;
			QAngle dummyAng;
			m_dummyModel->GetBonePosition(i, m_segs[i].Pos, dummyAng);
		}
		return;
	}
	int i;

	int iFirst = 2;
	int iLast = m_segs.Count() - 1;

	// clear stuck flags
	for (i = 0; i <= iLast; i++)
	{
		m_segs[i].Stuck = false;
	}

	// try to move all the nodes
	for (i = iFirst; i <= iLast; i++)
	{
		trace_t tr;

		// check direct movement
		AI_TraceHull(m_segs[i].Pos, m_segs[i].Pos + m_segs[i].Dir,
			Vector(-2, -2, -2), Vector(2, 2, 2),
			MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

		Vector direct = tr.endpos;
		Vector delta = Vector(0, 0, 0);

		Vector slide = m_segs[i].Dir;
		if (tr.fraction != 1.0)
		{
			// slow down and remove all motion in the direction of the plane
			direct += tr.plane.normal;
			Vector impactSpeed = (slide * tr.plane.normal) * tr.plane.normal;

			slide = (slide - impactSpeed) * 0.8;

			if (tr.m_pEnt)
			{
				if (tr.m_pEnt->IsSolid())
				{
					EmitSound("NPC_Hydra.Bump");
					if (FClassnameIs(tr.m_pEnt, "func_breakable"))
					{
						if (tr.m_pEnt->m_takedamage == DAMAGE_YES)
						{
							CTakeDamageInfo info(this, this, tr.m_pEnt->m_iHealth + 1, DMG_SLASH);
							CalculateMeleeDamageForce(&info, (tr.endpos - tr.startpos), tr.endpos);
							tr.m_pEnt->TakeDamage(info);
						}
					}
					else if (FClassnameIs(tr.m_pEnt, "prop_physics") || FClassnameIs(tr.m_pEnt, "prop_dynamic"))
					{
						if (tr.m_pEnt->m_iHealth > 0)
						{
							if (tr.m_pEnt->m_takedamage == DAMAGE_YES)
							{
								CTakeDamageInfo info(this, this, tr.m_pEnt->m_iHealth + 1, DMG_SLASH);
								CalculateMeleeDamageForce(&info, (tr.endpos - tr.startpos), tr.endpos);
								tr.m_pEnt->TakeDamage(info);
							}
						}
						else
						{
							Vector	dir = tr.m_pEnt->GetAbsOrigin() - GetAbsOrigin();
							VectorNormalize(dir);

							QAngle angles;
							VectorAngles(dir, angles);
							Vector forward, right;
							AngleVectors(angles, &forward, &right, NULL);

							tr.m_pEnt->ApplyAbsVelocityImpulse(-right * 500.0f + forward * 500.0f);
						}
					}
				}
			}

			// slow down and remove all motion in the direction of the plane
			slide = (slide - (slide * tr.plane.normal) * tr.plane.normal) * 0.8;

			// try to move the remaining distance anyways
			AI_TraceHull(direct, direct + slide * (1 - tr.fraction),
				Vector(-2, -2, -2), Vector(2, 2, 2),
				MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

			// NDebugOverlay::Line( m_body[i].vecPos, tr.endpos, 255, 255, 0, true, 1);

			direct = tr.endpos;

			m_segs[i].Stuck = true;

		}

		// make sure the new segment doesn't intersect the world
		AI_TraceHull(direct, m_segs[i - 1].Pos,
			Vector(-2, -2, -2), Vector(2, 2, 2),
			MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

		if (tr.fraction == 1.0)
		{
			if (i + 1 < iLast)
			{
				AI_TraceHull(direct, m_segs[i + 1].Pos,
					Vector(-2, -2, -2), Vector(2, 2, 2),
					MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);
			}

			if (tr.fraction == 1.0)
			{
				m_segs[i].Pos = direct;
				delta = slide;
			}
			else
			{
				// FIXME: compute nudge force
				m_segs[i].Stuck = true;
				//m_body[i+1].bStuck = true;
			}
		}
		else
		{
			// FIXME: compute nudge force
			m_segs[i].Stuck = true;
			//m_body[i-1].bStuck = true;
		}

	//	m_segs[i-1].Dir += (m_segs[i].Dir - delta) * 0.25;
	//	m_segs[i+1].Dir += (m_segs[i].Dir - delta) * 0.25;
		m_segs[i].Dir = delta;
	}
}

#ifndef USE_NEW_FAKE_COLLISIONS
void CHydra::CreateCollisionObjects(void)
{
	// begin with first segment after the base, end at the head segment. 
	for (int i = 4; i < m_segs.Count() - 2; i++)
	{
	//	if (i % 3 == 1) // only place it on every third segment
		{
			hydraCollision *piece = (hydraCollision *)/*CBaseEntity::*/CreateNoSpawn("hydra_hitbox", GetAbsOrigin(), GetAbsAngles(), this);
			piece->SetParent(this, -1);
			piece->SetAbsOrigin(vec3_origin);
			piece->SetAbsAngles(vec3_angle);
			DispatchSpawn(piece);
			piece->Activate();
			char buf[64];
			Q_snprintf(buf, sizeof(buf), "%i", i);
			piece->KeyValue("segIndex", buf);

			m_collisions.AddToTail(piece);
		}
	}
}
#endif

void CHydra::UndulateThink(void)
{
	// calcualte between 0 and 1
//	if (!m_suppressBodyMovement) // don't recalculate during attack - otherwise when resume after attack, unwanted "snap to new pos" effect occures
	{
		float coeff = (sin(2.0f * M_PI * (CURTIME) / m_rndBendDelta_float) * 0.5f) + 0.5f;

		if (coeff == 0 || coeff == 1)
		{
			EmitSound("NPC_Hydra.ExtendTentacle");
		}

		// now set min and max coeffs outside 0-1 range
		coeff = (m_rndBendMax_float - m_rndBendMin_float) * coeff + m_rndBendMin_float;
		
		m_mainBendCoeff = coeff;
	}
	SetNextThink(CURTIME, "UndulateContext");
}

void CHydra::DrawDebugOverlays()
{
	if (hydra_showvectors.GetInt() == 1)
	{
		NDebugOverlay::Box(m_vecRoot, Vector(-4, -4, -4), Vector(4, 4, 4), 225, 50, 200, 30, 0.06f);
		NDebugOverlay::Box(m_vecBase, Vector(-1, -1, -1), Vector(1, 1, 1), 255, 50, 50, 30, 0.06f);
		NDebugOverlay::Box(m_vecNeck, Vector(-1, -1, -1), Vector(1, 1, 1), 0, 225, 255, 30, 0.06f);
		NDebugOverlay::Box(m_vecHead, Vector(-2, -2, -2), Vector(2, 2, 2), 0, 50, 255, 20, 0.06f);
		
		NDebugOverlay::Line(m_vecRoot, m_vecBase, 100, 100, 100, true, 0.06f);
		NDebugOverlay::Line(m_vecBase, m_vecNeck, 1700, 100, 170, true, 0.06f);
		NDebugOverlay::Line(m_vecNeck, m_vecHead, 100, 100, 100, true, 0.06f);
	}
	else if (hydra_showvectors.GetInt() == 2)
	{
		for (int i = m_segs.Count() - 1; i >= 0; i--)
		{
			if (i == 0 || i == m_segs.Count() - 1)
				NDebugOverlay::Sphere(m_segs[i].Pos, 4, 0, 255, 0, true, 0.05f);
			else
			{
				NDebugOverlay::Sphere(m_segs[i].Pos, 2, 0, 100, 100, true, 0.05f);
			}

			if (i > 0)
			{
				NDebugOverlay::Line(m_segs[i].Pos, m_segs[i - 1].Pos, 15, 205, 255, true, 0.05f);
			}
		}
	}
	else if (hydra_showvectors.GetInt() == 3)
	{
		for (int i = 0; i < m_segs.Count() - 1; i++)
		{
			NDebugOverlay::Box(m_segs[i].Pos, Vector(-0.5, -0.5, -0.5), Vector(0.5, 0.5, 0.5), 100, 30, 10, 100, 0.05f);

			if (i > 0)
			{
				NDebugOverlay::Line(m_segs[i].Pos, m_segs[i - 1].Pos, 50, 15, 5, true, 0.05f);
			}

			NDebugOverlay::Line(m_segs[i].Pos, m_segs[i].Pos + m_segs[i].Dir * 8, 100, 100, 5, true, 0.05f);
		}
	}
	else if (hydra_showvectors.GetInt() == 5)
	{
		for (int i = 0; i < m_dummyModel->GetNumBones(); i++)
		{
			Vector p1, p2;
			QAngle angle;
			m_dummyModel->GetBonePosition(i, p1, angle);
			m_dummyModel->GetBonePosition(i+1, p2, angle);
			NDebugOverlay::Line(p1, p2, 200, 200, 50, true, 0.1f);
		}
	}
}

bool CHydra::CheckHeadCollision(void)
{
	// if the attack has just started, give a little bit of time
	// when being stuck doesn't stop the attack task.
	if (CURTIME < m_last_attack_time + 0.1f) return false;

	Vector			checkDir = GetAbsVelocity().Length() > 0 ? GetAbsVelocity() : m_vecHeadDir;
	VectorNormalize(checkDir);
	trace_t			tr, pr;
	CBaseEntity*	prHitEntity = NULL;
	CBaseEntity*	trHitEntity = NULL;
	AI_TraceHull(m_vecHead - checkDir * 32, m_vecHead + checkDir * 32, GetHullMins() * 1.5, GetHullMaxs() * 1.5, MASK_SHOT, this, COLLISION_GROUP_NONE, &pr);
	
	if (pr.m_pEnt)
	{
		prHitEntity = pr.m_pEnt;

		if ( prHitEntity && prHitEntity->IsSolid() )
		{
			EmitSound("NPC_Hydra.Bump");
			if (FClassnameIs(prHitEntity, "func_breakable"))
			{

				if (prHitEntity->m_takedamage == DAMAGE_YES)
				{
					CTakeDamageInfo info(this, this, prHitEntity->m_iHealth + 1, DMG_SLASH);
					CalculateMeleeDamageForce(&info, (pr.endpos - pr.startpos), pr.endpos);
					prHitEntity->TakeDamage(info);
				}
			}
			else if (FClassnameIs(prHitEntity, "prop_physics") || FClassnameIs(prHitEntity, "prop_dynamic"))
			{
			//	if (prHitEntity->m_iHealth > 0)
			//	{
					if (prHitEntity->m_takedamage == DAMAGE_YES)
					{
						CTakeDamageInfo info(this, this, prHitEntity->m_iHealth + 1, DMG_SLASH);
						CalculateMeleeDamageForce(&info, (pr.endpos - pr.startpos), pr.endpos);
						prHitEntity->TakeDamage(info);
					}
			//	}
				else
				{
					Vector	dir = prHitEntity->GetAbsOrigin() - GetAbsOrigin();
					VectorNormalize(dir);

					QAngle angles;
					VectorAngles(dir, angles);
					Vector forward, right;
					AngleVectors(angles, &forward, &right, NULL);

					prHitEntity->ApplyAbsVelocityImpulse(-right * 500.0f + forward * 500.0f);
				}
			}
		}

		if (prHitEntity->IsWorld())
		{
			EmitSound("NPC_Hydra.Bump");
			g_pEffects->Dust(GetAbsOrigin(), Vector(0,0,1), 64, 50);
		}
		
		//Shake the screen
		UTIL_ScreenShake(GetAbsOrigin(), 4.0f, 80.0f, 1.0f, 256.0f, SHAKE_START);
	}

	AI_TraceHull(GetAbsOrigin(), GetAbsOrigin() + checkDir * 32, GetHullMins() * 1.5, GetHullMaxs() * 1.5, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

	if (tr.m_pEnt)
	{
		trHitEntity = tr.m_pEnt;

		// Did I hit an entity that isn't another hydra?
		if (trHitEntity && !FClassnameIs(trHitEntity, this->GetClassname()))
		{
			if (!FClassnameIs(trHitEntity, "hydra_dummy"))
			{
				if (trHitEntity->IsWorld()) // FIXME: didn't we deal with the world in the previous trace?
				{
					EmitSound("NPC_Hydra.Bump");
				}
				else if (trHitEntity->m_takedamage == DAMAGE_YES)
				{
					if (trHitEntity->IsAlive())
					{
						Vector vecBloodDelta = m_vecHeadDir;
						vecBloodDelta.z = 0; // effect looks better 
						VectorNormalize(vecBloodDelta);
						UTIL_BloodSpray(tr.endpos + vecBloodDelta * 4, vecBloodDelta, trHitEntity->BloodColor(), 4, FX_BLOODSPRAY_ALL);
						UTIL_BloodSpray(tr.endpos + vecBloodDelta * 4, vecBloodDelta, trHitEntity->BloodColor(), 6, FX_BLOODSPRAY_DROPS);
					}

					EmitSound("NPC_Hydra.Stab");

					float dmg = sk_hydra_stab_dmg.GetFloat();

					if (trHitEntity->IsNPC() && IRelationType(trHitEntity) == D_HT)
						dmg = trHitEntity->GetHealth();

					CTakeDamageInfo info(this, this, dmg, DMG_SLASH);
					CalculateMeleeDamageForce(&info, (tr.endpos - tr.startpos), tr.endpos);
					trHitEntity->TakeDamage(info);

					// Pull the player toward us.
					// TODO: make this better, move it to retract task
					if (trHitEntity->IsPlayer())
					{
						trHitEntity->ApplyAbsVelocityImpulse(-m_vecHeadDir * 200);
					}
				}
				return true;
			}
		}
	}

	return false;
}

void CHydra::PainSound(const CTakeDamageInfo &info)
{
	if (CURTIME < m_nextpainsound_time)
	{
		EmitSound("NPC_Hydra.Pain"); 
		m_nextpainsound_time = CURTIME + RandomFloat(0.75f, 1.5f);
	}
}

// for when the hydra is ready to attack, but doesn't have a target
void CHydra::SearchSound(void)
{
	if (CURTIME >= m_flNextWeaponSearchTime) // hack: reusing preexisting variable
	{
		EmitSound("NPC_Hydra.Search");
		m_flNextWeaponSearchTime = CURTIME + RandomFloat(4,6);
	}
}

void CHydra::PreAttackSound(void)
{
	if (CURTIME >= m_nextpreattacksound_time)
	{
		EmitSound("NPC_Hydra.AttackCue");
		m_nextpreattacksound_time = CURTIME + 10.0f;
	}
}

int CHydra::OnTakeDamage_Alive(const CTakeDamageInfo &info)
{
	CTakeDamageInfo newInfo = info;

//	if (!(newInfo.GetInflictor()->ClassMatches("hydra_hitbox")))
//	{
//		newInfo.SetDamage(0); // only take damage from our hitboxes
//	}

	if (newInfo.GetDamage() > GetHealth())
	{
		newInfo.SetDamage(GetHealth() - 1);
	}

	int tookdamage = BaseClass::OnTakeDamage_Alive(newInfo);

	m_OnDamaged.FireOutput(this, this);
	if (newInfo.GetAttacker()->IsPlayer() || newInfo.GetInflictor()->IsPlayer())
		m_OnDamagedByPlayer.FireOutput(this, this);
		
	if (GetHealth() <= GetMaxHealth() * 0.1f)
	{
		m_takedamage = DAMAGE_NO;
		m_lifeState = LIFE_DYING;

		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		if (m_pIdleSound != NULL)
		{
			controller.SoundDestroy(m_pIdleSound);
			m_pIdleSound = NULL;
		}

#ifndef USE_NEW_FAKE_COLLISIONS
		// collapse all our collisions
		for (int i = 2; i < m_segs.Count() - 2; i++)
		{
			if (m_collisions.Count() > 0)
			{
				CTakeDamageInfo dmg;
				dmg.SetAttacker(this);
				dmg.SetInflictor(this);
				dmg.SetDamage(100);
				dmg.SetDamageType(DMG_GENERIC);
				if (m_collisions[i - 2])
				{
					m_collisions[i - 2]->TakeDamage(dmg);
				}
			}

		//	if (i % 2)
		//	{
		//		DispatchParticleEffect("blood_impact_blue_01", m_segs[i].Pos, RandomAngle(0, 180));
		//	}
		}
#endif

		ClearSchedule("im ded");

		if (info.GetAttacker()->IsPlayer())
		{
			m_OnDeathByPlayer.FireOutput(this, this);
		}

		m_OnDeath.FireOutput(this, this);

		//	TODO: start restore part think after a delay?
		return 0;
	}
	else
	{
		m_flNextAttack -= 0.5; // Hydra is likely to promptly attack after being damaged
	}
	
	// TODO: flinches
	// temp flinch using poseparameters
	float hor = RandomFloat(4, 10);
	// FIXME: this method doesn't update the hitbox, making it detach from where the model really is
	if (RandomInt(0, 2) == 0) hor *= -1;
	float ver = RandomFloat(4, 7);
		SetPoseParameter("horizontal", hor);
	if (RandomInt(0, 1) == 0)
		SetPoseParameter("vertical", ver);
	m_flLastDamageTime = CURTIME;

	PainSound(newInfo);
	return tookdamage;
}

#ifndef USE_NEW_FAKE_COLLISIONS
void CHydra::InputCollisionObjectDestroyed(inputdata_t &inputdata)
{
	if (m_collisions.Count() < 1) return;
	if (m_lifeState == LIFE_ALIVE)
	{
		int segindex = inputdata.value.Int();
		AssertMsg(segindex > 0, "Hydra collision object relays improper bone index through input");
		if (segindex > m_collisions.Count()) return;

		DispatchParticleEffect("blood_impact_blue_01", m_segs[segindex].Pos, GetAbsAngles());
	//	m_segs[segindex].segmentDestroyed_bool = true;

		// Take damage to our health
		CTakeDamageInfo dmg;
		dmg.SetAttacker(inputdata.pActivator);
		dmg.SetInflictor(inputdata.pActivator);
		dmg.SetDamage(GetMaxHealth() * 0.1f + 1); // 10% for each destroyed segment
		dmg.SetDamageType(DMG_GENERIC);
		TakeDamage(dmg);
	}
}
#endif
void CHydra::InputHideAndReappear(inputdata_t &inputdata)
{
	if (m_lifeState != LIFE_ALIVE ) return;
	ClearSchedule("forced to hide and reappear");
	SetSchedule(SCHED_HYDRA_HIDE_AND_REAPPEAR);
}

void CHydra::InputHideAndDestroy(inputdata_t &inputdata)
{
	if (m_lifeState != LIFE_ALIVE) return;
	ClearSchedule("forced to hide and die");
	SetSchedule(SCHED_HYDRA_HIDE_AND_DESTROY);
}

void CHydra::InputBecomeRagdoll(inputdata_t &inputdata)
{
	if (m_lifeState != LIFE_ALIVE) return;
	ClearSchedule("forced to hide and die");
	SetSchedule(SCHED_HYDRA_DIE_RAGDOLL);
}

void CHydra::InputSetRootEntity(inputdata_t &inputdata)
{
	string_t strTarget = inputdata.value.StringID();

	if (strTarget != NULL_STRING)
	{
		m_target = strTarget;
	}
}

void CHydra::InputForceBodyAlongPath(inputdata_t &inputdata)
{
	return;
	/*
	string_t strTarget = inputdata.value.StringID();

	if (strTarget != NULL_STRING)
	{
		CBaseEntity *pForcedTarget = NULL;
		while ((pForcedTarget = gEntList.FindEntityGeneric(pForcedTarget, STRING(strTarget), this, this)) != NULL)
		{
			m_override_path_handle = pForcedTarget;
			m_hydra_forced_along_path = true;
		}
	}
	*/
}

void CHydra::InputClearForcedPath(inputdata_t &inputdata)
{
	return;
	/*
	m_override_path_handle = NULL;
	m_hydra_forced_along_path = false;
	*/
}

void CHydra::InputSetBodyThickness(inputdata_t &inputdata)
{
	if (m_dummyModel != NULL)
	{
		m_dummyModel->SetFlexWeight("head_distend", inputdata.value.Float());
		m_dummyModel->SetFlexWeight("body_distend", inputdata.value.Float());
	}
}

void CHydra::InputSetMaxLength(inputdata_t &inputdata)
{
	m_hydra_max_length_float = inputdata.value.Float();
}

void CHydra::InputSetIdealSegLength(inputdata_t &inputdata)
{
	m_hydra_ideal_seglength_float = inputdata.value.Float();
}

// This input works as curtime + input float value.
void CHydra::InputSetAnimLerpEndTime(inputdata_t &inputdata)
{
	if (m_dummyModel != NULL)
	{
		float starttime = m_dummyModel->GetAnimLerpEndTime() > 0 ? 0 : CURTIME;
		m_dummyModel->SetAnimLerpEndTime(starttime, inputdata.value.Float());
	}
}

void CHydra::InputSetAnimation(inputdata_t &inputdata)
{
	if (m_dummyModel != NULL)
	{
		m_dummyModel->m_anim_sequence_int = m_dummyModel->LookupSequence(inputdata.value.String());
	}
}

void CHydra::InputFlinch(inputdata_t &inputdata)
{	
}

void CHydra::InputSetHeadInfluence(inputdata_t &inputdata)
{
	m_headsegweight_float = inputdata.value.Float();
}

#define HYDRA_HOVER_DIST 64
void CHydra::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_HYDRA_HOVER:
	{
		// Work out hovering poseparameters
		if (CURTIME > m_flLastDamageTime + 2)
		{
			float freq = hydra_hover_frequency.GetFloat();
			
			if (m_NPCState == NPC_STATE_ALERT) freq *= 1.2f;

			else if (m_NPCState == NPC_STATE_COMBAT) freq *= 1.4f;

			float maxH, maxV;
			maxH = hydra_hover_max_hor.GetFloat();
			maxV = hydra_hover_max_vert.GetFloat();

			if (m_flNextAttack < CURTIME + 2)
			{
				maxH *= 0.5; maxV *= 0.5;
			}

			m_hover_horizontal_float = sin(CURTIME * freq);
			m_hover_vertical_float = sin(CURTIME * freq);
			SetPoseParameter("horizontal", maxH * m_hover_horizontal_float);
			SetPoseParameter("vertical", maxV * m_hover_vertical_float);
		}
	}
	return;
	case TASK_MELEE_ATTACK1:
	{
		if (GetEnemy() == NULL)
		{
			return;
			TaskComplete("No enemy");
		}

		// Extract and store stab position once during StartTask().
		// Then during RunTask(), move towards this position, rather than
		// moving towards the enemy.
		// This way, the Hydra is more fair, and avoidable - it will strike
		// where you were but if you're quick you'll get out of its path
		// and it won't renew the strike position during RunTask().
		// Previous version just used GetEnemy()->GetAbsPosition() or EyePosition()
		// during RunTask() and it made it impossible to dodge the stab. 		
		m_vecStab = GetEnemy()->EyePosition();

		// Save current head position right before the attack. Same purpose as m_vecStab.
		m_vecSaved = m_vecHead;

		m_maxStabDist = GetAbsOrigin().DistTo(GetEnemy()->GetAbsOrigin());
		m_maxStabDist += min(128, m_maxStabDist * 0.25);

		EmitSound("NPC_Hydra.Strike");
		m_last_attack_time = CURTIME;
		
		//Shake the screen
		UTIL_ScreenShake(m_vecRoot, 1.5f, 80.0f, 1.0f, 1024.0f, SHAKE_START);
				
		m_suppressBodyMovement = true; // halt body animation; only move the head during attack

		// reset speed
		SetAbsVelocity(vec3_origin);

		SetPoseParameter("horizontal", 0); // aim the hydra directly at the target.
		SetPoseParameter("vertical", 0);
	}
	return;
	case TASK_HYDRA_DEPLOY_ALONG_PATH:
	{
		if (m_vecStoredPathGoal == NULL)
		{
			return;
			TaskComplete("No path");
		}
		
		m_vecStab = m_vecStoredPathGoal;
	}
	return;
	case TASK_HYDRA_RETRACT:
	{
		SetAbsVelocity(vec3_origin);
	}
	return;
	case TASK_HYDRA_CREATE_DEATH_RAGDOLL:
	{
		m_lifeState = LIFE_DEAD;

		if (!m_dummyModel)
		{
			TaskFail("no dummy");
			return;
		}

		m_dummyModel->SetRenderMode(kRenderNone);
		Vector base = m_dummyModel->m_vecChain[1];
		Vector head = m_dummyModel->m_vecChain[30];
		Vector dir = head - base;
		QAngle ang = vec3_angle;
		VectorAngles(dir, ang);

		DispatchParticleEffect("blood_hydra_split", base, ang);
		DispatchParticleEffect("blood_hydra_split_spray", base, ang);

		//	CreateRagGib(HYDRA_RAGDOLL_MODEL, base, ang, base + dir * 100, 10, false, 0);
		CRagdollProp *pRagdollGib = (CRagdollProp *)CBaseEntity::CreateNoSpawn("prop_ragdoll", base, ang, NULL);

		// Calculate death force
		Vector forceVector = Vector(0, 0, -600);

		// See if there's a ragdoll magnet that should influence our force.
		CRagdollMagnet *pMagnet = CRagdollMagnet::FindBestMagnet(this);
		if (pMagnet)
		{
			forceVector += pMagnet->GetForceVector(this);
		}
		
		pRagdollGib->GetRagdoll()->allowStretch = true;
		pRagdollGib->CopyAnimationDataFrom(m_dummyModel);
		pRagdollGib->SetModelName(MAKE_STRING("models/_Monsters/Xen/hydra_ragdoll.mdl"));
		pRagdollGib->SetOwnerEntity(NULL);
		pRagdollGib->Spawn();
		pRagdollGib->SetCollisionGroup(COLLISION_GROUP_DEBRIS);
		pRagdollGib->ApplyAbsVelocityImpulse(forceVector);

		pRagdollGib->ThinkSet(&CBaseEntity::SUB_FadeOut);
		pRagdollGib->SetNextThink(CURTIME + 10);

		// Send the ragdoll into spasmatic agony
		inputdata_t input;
		input.pActivator = this;
		input.pCaller = this;
		input.value.SetFloat(RandomFloat(3, 5)); // N seconds of spasms
		pRagdollGib->InputStartRadgollBoogie(input);

		m_OnDeath.FireOutput(this, this);
		
		EmitSound("NPC_Hydra.Pain");
		TaskComplete();
	}
	return;
	case TASK_HYDRA_GET_RETRACT_POS:
	{
		GetSenses()->AddSensingFlags(SENSING_FLAGS_DONT_LISTEN | SENSING_FLAGS_DONT_LOOK);
		if (GetEnemy()) ClearEnemyMemory();
		m_vecSaved = GetMoveParent() ? vec3_origin : m_vecRoot;
		EmitSound("NPC_Hydra.ExtendTentacle");
		TaskComplete();
	}
	return;
	case TASK_HYDRA_DESTROY:
	{
		m_hydra_is_dead_bool = true; 

		m_dummyModel->SetThink(&hydraDummy::SUB_Remove);
		m_dummyModel->SetNextThink(CURTIME + 0.1f);

		m_OnDeath.FireOutput(this, this);

		SetThink(&CHydra::SUB_Remove);
		SetNextThink(CURTIME + 0.1f);

		TaskComplete();
	}
	return;

	case TASK_HYDRA_HIDE:
	{
		if (m_dummyModel)
		{
			m_dummyModel->SetRenderMode(kRenderNone);
		}
		DispatchParticleEffect("blood_impact_blue_01", m_vecHead, vec3_angle);
		EmitSound("NPC_Hydra.Bump");
		TaskComplete();
	}
	return;

	case TASK_HYDRA_FIND_REAPPEAR_POINT:
	{
		m_vecRoot.x += RandomFloat(-100, 100);
		m_vecRoot.z += RandomFloat(-100, 100);

		m_vecHead.x += RandomFloat(-100, 100);
		m_vecHead.z += RandomFloat(-100, 100);
		m_vecHead.y = m_vecRoot.y + 32;
		
		TaskComplete();
	}
	return;

	case TASK_HYDRA_REAPPEAR:
	{
		if (m_dummyModel)
		{
			m_dummyModel->SetRenderMode(kRenderNormal);
		}
		DispatchParticleEffect("blood_impact_blue_01", m_vecHead, vec3_angle);
		EmitSound("NPC_Hydra.Bump");

		m_flNextAttack = CURTIME + 1.0f;

		TaskComplete();
	}
	return;
		
	default:
		BaseClass::StartTask(pTask);
		return;
	}
}

void CHydra::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_HYDRA_HOVER:
	{
		// Work out hovering poseparameters
		if (CURTIME > m_flLastDamageTime + 2)
		{
			float freq = hydra_hover_frequency.GetFloat();
			
			if (m_NPCState == NPC_STATE_ALERT) freq *= 1.2f;

			else if (m_NPCState == NPC_STATE_COMBAT) freq *= 1.4f;

			float maxH, maxV;
			maxH = hydra_hover_max_hor.GetFloat();
			maxV = hydra_hover_max_vert.GetFloat();

			if (m_flNextAttack < CURTIME + 2)
			{
				maxH *= 0.5; maxV *= 0.5;
			}

			m_hover_horizontal_float = sin(CURTIME * freq);
			m_hover_vertical_float = sin(CURTIME * freq);
			SetPoseParameter("horizontal", maxH * m_hover_horizontal_float);
			SetPoseParameter("vertical", maxV * m_hover_vertical_float);
		}
	}
	return;
	case TASK_ANNOUNCE_ATTACK:
	{
		if (GetEnemy() && m_vecBase.DistTo(GetEnemy()->WorldSpaceCenter()) > m_hydra_max_length_float)
		{
			Msg("Hydra %s would overstretch in this attack (dist to enemy %.0f)!\n", m_vecBase.DistTo(GetEnemy()->WorldSpaceCenter()));
			SetWait(0.1f, 0.3f); // don't try to stab targets that are guaranteed to be out of our reach
			return;
		}
		else
		{
			TaskComplete();
			return;
		}
	}
	return;
	case TASK_MELEE_ATTACK1:
	{
		if (GetEnemy() == NULL)
		{
			TaskComplete("No enemy");
			m_flNextAttack = CURTIME + 3.0f;
			return;
		}

		// balancing. See if updating m_vecStab during RunTask
		// (previously thought to be too unfair) will make Hydras
		// more worthy enemies. Because without it, they barely 
		// score hits on the player...
		m_vecStab = GetEnemy()->EyePosition();

		Vector up;
		GetVectors(NULL, NULL, &up);
		Vector goal = m_vecStab;
		Vector stab = goal - m_vecHead;
		Vector repel = -(stab);
		VectorNormalize(stab); VectorNormalize(repel);
		
		if (true) // add some check to see if space ahead is clear
		{
			if (GetAbsVelocity().Length() <= hydra_max_velocity.GetFloat())
			{
				SetAbsVelocity(VectorLerp(GetAbsVelocity(), stab * hydra_max_velocity.GetFloat(), 0.5));
			}
		}
		else
		{
			SetAbsVelocity(VectorLerp(GetAbsVelocity(), vec3_origin, 0.5));
		}
		
		// Keep head and neck straight and pointed at the target
		m_vecHead = m_vecHead + EyeDirection3D() * 1.0f;
		m_vecHeadDir = EyeDirection3D();
		m_vecNeck = m_vecHead - EyeDirection3D() * 8.0f;
		
		// First check is 
		if (CheckHeadCollision() || GetAbsOrigin().DistTo(goal) < 8.0f)
		{
			TaskComplete("Stab stuck or reached goal");
			SetAbsVelocity(vec3_origin);
			m_flNextAttack = CURTIME + 10; // this is kinda bogus. m_flNextAttack will be reset to something shorter after hydra retracts back.

			return;
		}

		if (CURTIME > m_last_attack_time + 5.0f)
		{
			TaskComplete("Stab timeout");
			SetAbsVelocity(vec3_origin);
			m_flNextAttack = CURTIME + RandomFloat(2.5f, 4.0f); // again bogus. We do this to avoid hydra glow intensity for the proxy going up while retracting (because this timer may ran out if it's been far away retract).
			return;
		}

		if (m_lengthTotal > m_hydra_max_length_float)
		{
			EmitSound("NPC_Hydra.Pain");
			TaskComplete("Stab overstretch");
			SetAbsVelocity(vec3_origin);
			return;
		}

		if (GetAbsOrigin().DistTo(m_vecSaved) > m_maxStabDist)
		{
			EmitSound("NPC_Hydra.Pain"); // todo: replace with some annoyed grunt or something
			TaskComplete("Stab exceeded planned stab length");
			SetAbsVelocity(vec3_origin);
			return;
		}
	}
	return;
	case TASK_HYDRA_RETRACT:
	// todo: need to redo this task so the hydra... ideally
	// moves its head back to a point between its neck vectors, which
	// progressively gets moved itself, because it's shortening.
	// and the cutoff point would be somewhere in the middle of the body,
	// remembered, so the hydra doesn't get stuck perpetually 
	// moving back...
	{
		// retract along our body (the goal is going to be moving itself as the hydra shortens).
		Vector goal = m_vecSaved; //m_segs[m_segs.Count() / 2].Pos;
				
	//	NDebugOverlay::Box(goal, Vector(-1, -1, -1), Vector(1, 1, 1), 255, 200, 100, 255, 0.1f);
		
		Vector retract = goal - m_vecHead;
		VectorNormalize(retract);

		if (true) // add some check to see if space ahead is clear
		{
			if (GetAbsVelocity().Length() <= hydra_max_velocity.GetFloat() / 2)
			{
				//	ApplyAbsVelocityImpulse(stab * hydra_attack_momentum.GetFloat());
				SetAbsVelocity(VectorLerp(GetAbsVelocity(), retract * hydra_max_velocity.GetFloat() / 2, 0.5));
			}
		}
		if (GetAbsOrigin().DistTo(goal) <= 256.0f)
		{
			SetAbsVelocity(VectorLerp(GetAbsVelocity(), vec3_origin, 0.5));

			if (GetAbsOrigin().DistTo(goal) <= 16.0f || GetAbsVelocity().Length() <= 0)
			{
				SetAbsVelocity(vec3_origin);
				m_suppressBodyMovement = false; // resume body movement.
				TaskComplete();
				m_flNextAttack = CURTIME + RandomFloat(2.0, 5.0f);
				m_flNextWeaponSearchTime = CURTIME; // reset this for SearchSound() when it's used within combat
				SetPoseParameter("horizontal", 0); // reset hovering
				SetPoseParameter("vertical", 0);
				return;
			}
		}

		if (CURTIME > m_last_attack_time + 5.0f)
		{
			SetAbsVelocity(vec3_origin);
			m_suppressBodyMovement = false; // resume body movement.
			TaskComplete("Stab timeout");
			m_flNextAttack = CURTIME + RandomFloat(2.5f, 4.0f);
			m_flNextWeaponSearchTime = CURTIME; // reset this for SearchSound() when it's used within combat
			SetPoseParameter("horizontal", 0); // reset hovering
			SetPoseParameter("vertical", 0);
			return;
		}
	}
	return;
	
	default:
		BaseClass::RunTask(pTask);
		return;
	}
}

Activity CHydra::NPC_TranslateActivity(Activity activity)
{
	if (activity == ACT_FLY)
	{			
		return m_NPCState == NPC_STATE_IDLE ? ACT_WALK : ACT_RUN;
	}
	
	return BaseClass::NPC_TranslateActivity(activity);
}

void CHydra::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	if (m_pIdleSound == NULL)
	{
		// Create the sound
		CPASAttenuationFilter filter(this);

		m_pIdleSound = controller.SoundCreate(filter, entindex(), CHAN_STATIC, "NPC_Hydra.HeartbeatIdle", ATTN_NORM);

		Assert(m_pIdleSound);

		// Start the engine sound
		controller.Play(m_pIdleSound, 0.0f, 100.0f);
		controller.SoundChangeVolume(m_pIdleSound, 1.0f, 0.25f);
	}

	if (GetEnemy())
	{
		controller.SoundChangePitch(m_pIdleSound, 130, 0.5f);
	}
	else
	{
		controller.SoundChangePitch(m_pIdleSound, 90, 2.0f);
	}

	if (m_lifeState == LIFE_DYING)
	{
		controller.SoundChangePitch(m_pIdleSound, 50, 0.5f);
		controller.SoundChangeVolume(m_pIdleSound, 0, 0.5f);
	}
}

int CHydra::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_PATROL_WALK:	
		return SCHED_HYDRA_IDLE_PATROL;
		break;
		
	case SCHED_IDLE_STAND:
	case SCHED_ALERT_STAND:
	case SCHED_COMBAT_STAND:
		return SCHED_HYDRA_HOVER;
		break;

	case SCHED_MELEE_ATTACK1: 
		return SCHED_HYDRA_ATTACK_STAB;
		break;

	case SCHED_TAKE_COVER_FROM_ENEMY:
		return SCHED_HYDRA_HIDE_AND_REAPPEAR;
		break;

	case SCHED_DIE:
		return SCHED_HYDRA_DIE_RAGDOLL;
		break;

	default: return BaseClass::TranslateSchedule(scheduleType);
	}
}

int CHydra::SelectSchedule(void)
{
	if (m_lifeState == LIFE_DYING)
	{
		return SCHED_DIE;
	}

	if (m_NPCState == NPC_STATE_IDLE)
		return SCHED_IDLE_STAND;

	if (m_NPCState == NPC_STATE_ALERT)
		return SCHED_ALERT_STAND;

	if( m_NPCState == NPC_STATE_COMBAT )
	{
		return SelectCombatSchedule();
	}

//	return SCHED_IDLE_STAND;

	return BaseClass::SelectSchedule();
}

int CHydra::SelectCombatSchedule(void)
{
	if (HasCondition(COND_HYDRA_CAN_ATTACK))
		return SCHED_MELEE_ATTACK1;
	else
	{
		return SCHED_COMBAT_STAND;
	}

	return BaseClass::SelectSchedule();
}

void CHydra::GatherConditions()
{
	BaseClass::GatherConditions();

	ClearCondition(COND_HYDRA_CAN_ATTACK);

	if (HasCondition(COND_NEW_ENEMY))
	{
		m_first_seen_enemy_float = CURTIME;
	}

	if (GetEnemy() && (CURTIME - GetEnemyLastTimeSeen()) > sk_hydra_memory_time.GetFloat())
	{
		SetEnemy(NULL);
		SetIdealState(NPC_STATE_ALERT);
		m_flNextWeaponSearchTime = CURTIME; // reset this for SearchSound() when not in combat
	}
	
	if (GetEnemy() != NULL 
		&& CURTIME >= m_flNextAttack 
		&& CURTIME >= m_first_seen_enemy_float + 2
		&& m_dummyModel != NULL && m_dummyModel->GetAnimLerpEndTime() < CURTIME)
	{
		if ((GetAbsOrigin() - (GetEnemy()->GetAbsOrigin())).Length() < m_hydra_max_length_float)
		{
			// Make sure not trying to kick through a window or something. 
		//	trace_t tr;
		//	Vector vecSrc, vecEnd;

		//	vecSrc = WorldSpaceCenter();
		//	vecEnd = GetEnemy()->WorldSpaceCenter();

		//	AI_TraceLine(vecSrc, vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
		//	if (tr.m_pEnt == GetEnemy() && tr.m_pEnt->GetMoveParent() == GetEnemy())
			{
				if (!HasCondition(COND_HYDRA_CAN_ATTACK))
				{
					SetCondition(COND_HYDRA_CAN_ATTACK);
				}
			}
		}
	}
	
	if (!m_dummyModel) return;

	float intensity, attacktime, flex;
	intensity = m_dummyModel->m_proxy_material_intensity_float;
	attacktime = m_dummyModel->m_proxy_time_to_attack_float;
	flex = 0;
		
	if (CURTIME - m_last_attack_time > 0.5 && intensity >= 0)
	{
		intensity -= 0.1;
	}

	if (!GetEnemy())
	{
		intensity = 0;
		m_dummyModel->m_proxy_material_intensity_float = intensity;
		return;
	}

	if ((GetAbsOrigin() - (GetEnemy()->GetAbsOrigin())).Length() >= m_hydra_max_length_float)
	{
		intensity -= 0.05; // if the enemy is too far, reduce intensity even if next attack is now.
	}
	else
	{
		if (m_flNextAttack - CURTIME <= 2) // otherwise, raise intensity, but only when next attack is soon or now.
		{
			intensity += 0.05;
		}
	}

	if (m_flNextAttack - CURTIME <= 2.5)
	{ // the attack is soon. Sound the alert...
		if (m_NPCState == NPC_STATE_COMBAT)
		{
			if (GetEnemy()) PreAttackSound();
			else SearchSound();
		}
		else if (m_NPCState == NPC_STATE_ALERT)
			SearchSound();

		attacktime = MAX(m_flNextAttack - CURTIME, 0.05f); // ramp up the syne proxy value...
		
		intensity += 0.1;

		flex = intensity;
		if (flex > 1) flex = 1;

		if (intensity > 2) // when attack is soon, intensity is allowed to get up to 2x
		{
			intensity = 2; // clamp intensity.
		}
	}
	else
	{
		attacktime += 0.5;

		intensity -= 0.02; // make the hydra lose its glow while commencing attack.

		if (intensity < 0)
		{
			intensity = 0; // clamp min value.
		}

		flex = intensity;
	}

	if (intensity < 0)
	{
		intensity = 0; // clamp min value.
	}

	m_dummyModel->m_proxy_material_intensity_float = intensity;
	m_dummyModel->m_proxy_time_to_attack_float = attacktime;
	m_dummyModel->SetFlexWeight("head_distend", flex);
}

bool CHydra::IsValidEnemy(CBaseEntity *pEnemy)
{
	if (GetEnemy() && GetEnemy()->ClassMatches("npc_bullseye"))
		return false; // if given a specific enemy to hit, ignore all others

	return BaseClass::IsValidEnemy(pEnemy);
}
// Movement
bool CHydra::OverrideMove(float flInterval)
{
	return false;
}

void CHydra::DoMovement(float flInterval, const Vector &MoveTarget)
{
	// dvs: something is setting this bit, causing us to stop moving and get stuck that way
	Forget(bits_MEMORY_TURNING);

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

void CHydra::SteerArrive(Vector &Steer, const Vector &Target)
{
	Vector goalPos = Target;
	// Trace down and make sure we're not flying too low (96 = min ground dist).
	trace_t	tr;
	AI_TraceHull(goalPos, goalPos - Vector(0, 0, 48), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

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

	goalPos.z *= m_mainBendCoeff;

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

void CHydra::SteerSeek(Vector &Steer, const Vector &Target)
{
	Vector offset = Target - GetLocalOrigin();

	VectorNormalize(offset);

	Vector DesiredVelocity = m_flGroundSpeed * offset;

	Steer = DesiredVelocity - GetAbsVelocity();
}

IMPLEMENT_SERVERCLASS_ST(hydraDummy, DT_hydraDummy)
SendPropArray(SendPropVector(SENDINFO_ARRAY(m_vecChain), -1, SPROP_COORD), m_vecChain),
SendPropVector(SENDINFO(m_vecRoot), -1, SPROP_COORD),
SendPropVector(SENDINFO(m_vecHeadDir), -1, SPROP_NORMAL),
SendPropFloat(SENDINFO(m_lengthRelaxed), 12, 0, 0.0, 500 * 1.5),
SendPropFloat(SENDINFO(m_anim_lerp_start_time_float), 0, SPROP_NOSCALE),
SendPropFloat(SENDINFO(m_anim_lerp_end_time_float), 0, SPROP_NOSCALE),
SendPropBool(SENDINFO(m_suppressProcedural)),
SendPropFloat(SENDINFO(m_proxy_time_to_attack_float), 0, SPROP_NOSCALE),
SendPropFloat(SENDINFO(m_proxy_material_intensity_float), 0, SPROP_NOSCALE),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(hydra_dummy, hydraDummy);

// Trace filters for Hydra physbox hitter + Hydra stuck checker
class CTraceFilterHydraStuck : public CTraceFilterSimple
{
public:
	DECLARE_CLASS(CTraceFilterHydraStuck, CTraceFilterSimple);

	CTraceFilterHydraStuck(const IHandleEntity *passentity, int collisionGroup)
		: CTraceFilterSimple(NULL, collisionGroup), m_pTraceOwner(passentity) {	}

	virtual TraceType_t	GetTraceType() const { return TRACE_EVERYTHING; }

	virtual bool ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask)
	{
		// Skip ourselves
		if (pHandleEntity == m_pTraceOwner)
			return false;

		// Get the entity referenced by this handle
		CBaseEntity *pEntity = EntityFromEntityHandle(pHandleEntity);
		if (pEntity == NULL)
			return false;

		// Skip all debris
		if (pEntity->GetCollisionGroup() == COLLISION_GROUP_DEBRIS)
			return false;
		
		// This is our own collision helper.
		if (FClassnameIs(pEntity, "hydra_hitbox") || FClassnameIs(pEntity, "hydra_collision"))
		{
			return false;

			// Somehow had a classname that didn't match the class!
			Assert(0);
		}

		// See if it's a breakable physics prop
		if (FClassnameIs(pEntity, "prop_physics"))
		{
			return false; 

			// Somehow had a classname that didn't match the class!
			Assert(0);
		}

		// See if it's a breakable physics brush
		if (FClassnameIs(pEntity, "func_physbox"))
		{
			return false;

			// Somehow had a classname that didn't match the class!
			Assert(0);
		}

		// Skip breakables entirely, for now
	//	if (FClassnameIs(pEntity, "func_breakable"))
	//	{
	//		return false;

			// Somehow had a classname that didn't match the class!
	//		Assert(0);
	//	}

		if (pEntity->IsPlayer())
		{
			return true;
		}

		//		if (pEntity->IsNPC())
		//		{
		//			return false; // skip NPCs - we kill them before we get stuck in them
		//		}

		// Use the default rules
		return false;//BaseClass::ShouldHitEntity(pHandleEntity, contentsMask);
	}

protected:
	const IHandleEntity *m_pTraceOwner;
};

class CTraceFilterHydraPhysics : public CTraceFilterSimple
{
public:
	DECLARE_CLASS(CTraceFilterHydraPhysics, CTraceFilterSimple);

	CTraceFilterHydraPhysics(const IHandleEntity *passentity, int collisionGroup)
		: CTraceFilterSimple(NULL, collisionGroup), m_pTraceOwner(passentity) {	}

	// For this test, we only test against entities (then world brushes afterwards)
	virtual TraceType_t	GetTraceType() const { return TRACE_ENTITIES_ONLY; }

	virtual bool ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask)
	{
		// Skip ourselves
		if (pHandleEntity == m_pTraceOwner)
			return false;

		// Get the entity referenced by this handle
		CBaseEntity *pEntity = EntityFromEntityHandle(pHandleEntity);
		if (pEntity == NULL)
			return false;

		// This is our own collision helper.
		if (FClassnameIs(pEntity, "hydra_hitbox") || FClassnameIs(pEntity, "hydra_collision"))
		{
			return false;

			// Somehow had a classname that didn't match the class!
			Assert(0);
		}
		
		if (pEntity->IsNPC())
		{
			return false; // skip NPCs - we kill them before we get stuck in them
		}

		if (pEntity->IsPlayer())
		{
			return false; // skip player - he's not physical
		}

		// Use the default rules
		return BaseClass::ShouldHitEntity(pHandleEntity, contentsMask);
	}

protected:
	const IHandleEntity *m_pTraceOwner;
};

ConVar sk_hydra_small_dmg("sk_hydra_small_dmg", "5");

int AE_HYDRA_ATTACK_BEGIN;
int AE_HYDRA_ATTACK_HIT;
int AE_HYDRA_ATTACK_END;
int AE_HYDRA_BURROW_IN;
int AE_HYDRA_BURROW_OUT;
int AE_HYDRA_INTERACTION_STAB;
int AE_HYDRA_FEEDING_BEGIN;
int AE_HYDRA_FEEDING_END;

int ACT_HYDRA_BURROW_IN;
int ACT_HYDRA_BURROW_OUT;
int ACT_HYDRA_BURROW_IDLE;

int AE_HYDRA_ATTACK_SPIT;

#define HYDRA_BURROW_IN	0
#define	HYDRA_BURROW_OUT 1

enum
{
	HYDRA_ON_FLOOR = 0,
	HYDRA_ON_CEILING,
	HYDRA_ON_WALL
};

#define	SF_HYDRA_BURROW_ON_ELUDED ( 1 << 16 )
#define	SF_HYDRA_NEVER_BURROW ( 1 << 17 )

class CHydraUnburrowPoint : public CServerOnlyPointEntity
{
	DECLARE_CLASS(CHydraUnburrowPoint, CServerOnlyPointEntity);
public:
	DECLARE_DATADESC();
	void Unlock(void)
	{
		m_disabled_bool = false;
	}
	void Lock(void)
	{
		m_disabled_bool = true;
	}
	void	InputUnlock(inputdata_t &inputdata)
	{
		Unlock();
	}
	void	InputLock(inputdata_t &inputdata)
	{
		Lock();
	}
	bool	IsLocked(void)
	{
		return m_disabled_bool;
	}
	int		GetOrientation(void)
	{
		return m_orientation_int;
	}
	void	HydraSwitchedToTarget(void)
	{
		m_OnTakenByHydra.FireOutput(this, this);
	}

private:
	bool			m_disabled_bool;
	int				m_orientation_int;
	COutputEvent	m_OnTakenByHydra;
};

LINK_ENTITY_TO_CLASS(info_target_hydra_unburrow, CHydraUnburrowPoint);

BEGIN_DATADESC(CHydraUnburrowPoint)
DEFINE_KEYFIELD(m_disabled_bool, FIELD_BOOLEAN, "disabled"),
DEFINE_KEYFIELD(m_orientation_int, FIELD_INTEGER, "orientation"), // floor, ceiling, or [to be implemented]

// Inputs
DEFINE_INPUTFUNC(FIELD_VOID, "Unlock", InputUnlock),
DEFINE_INPUTFUNC(FIELD_VOID, "Lock", InputLock),

// Outputs
DEFINE_OUTPUT(m_OnTakenByHydra, "OnTakenByHydra"),
END_DATADESC()

class CHydraImpale;
class CSmallHydra : public CAI_BaseNPC
{
	DECLARE_CLASS(CSmallHydra, CAI_BaseNPC);
	DECLARE_SERVERCLASS();
public:
	CSmallHydra()
	{
		m_ragdollModelName = "models/_Monsters/Xen/hydra_small_ragdoll.mdl";
		m_hNPCFileInfo = LookupNPCInfoScript("npc_hydra_small");
		m_alertradius_float = 256.0f;
		m_hydra_unburrow_time = 0.0f;
		m_hydra_burrow_time = 0.0f;
		m_hydra_burrow_attempts_int = 0;
		m_startburrowed_bool = false;
		m_isburrowed_bool = false;
		m_unburrowedfirsttime_bool = false;
		m_hydraplacement_int = 0;
		m_lastusedburrowtarget_ehandle = NULL;
		m_impaleEnt_ehandle = NULL;
		m_proxy_blood_visual_float = 0;
		m_hydra_attacks_since_unburrowing_int = 0;
		m_hydra_last_unburrow_time_float = 0;
	};

	~CSmallHydra()
	{
		m_proxy_blood_visual_float = 0;

		if (m_lastusedburrowtarget_ehandle != NULL)
		{
			CHydraUnburrowPoint *pLastPoint = assert_cast<CHydraUnburrowPoint*>(m_lastusedburrowtarget_ehandle.Get());

			if (pLastPoint)
			{
				pLastPoint->Unlock(); // let other hydras use this point
				m_lastusedburrowtarget_ehandle = NULL;
			}
		}

		if (m_impaleEnt_ehandle != NULL)
		{
			UTIL_Remove(m_impaleEnt_ehandle);
			m_impaleEnt_ehandle = NULL;
		}
	}

	Vector	EyeOffset(Activity nActivity)
	{
		return WorldSpaceCenter();
	}

	Vector	EyePosition(void)
	{
		return WorldSpaceCenter();
	}

	void Precache(void);
	void Spawn(void);
	//	virtual void ComputeWorldSpaceSurroundingBox(Vector *pVecWorldMins, Vector *pVecWorldMaxs);
	void SetupVPhysicsHull();
	void SetupHydraBounds(void);
	void Activate();

	virtual void OnRestore();
	virtual void NPCThink(void);
	void RunAI(void);
	void BeginFeedingThink(void);
	void EndFeedingThink(void);

	void Touch(CBaseEntity *pOther);
	void TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr);
	int OnTakeDamage_Alive(const CTakeDamageInfo &info);
	void Event_Killed(const CTakeDamageInfo &info);
	void InputRagdoll(inputdata_t &inputdata);
	void InputSetAlertRadius(inputdata_t &inputdata);

	void BuildScheduleTestBits(void);
	void GatherConditions(void);
	void PrescheduleThink(void);
	virtual int SelectSchedule(void);
	virtual int TranslateSchedule(int scheduleType);
	void StartTask(const Task_t *pTask);
	void RunTask(const Task_t *pTask);
	void HandleAnimEvent(animevent_t *pEvent);

	int MeleeAttack1Conditions(float flDot, float flDist);
	int MeleeAttack2Conditions(float flDot, float flDist);
	void MeleeAttack(float distance, float damage, QAngle& viewPunch, Vector& shove);
	void RangeAttack(void);
	void Attack(void);

	bool ValidBurrowPoint(const Vector &point);
	void Burrow(void);
	void Unburrow(void);
	void ClearBurrowPoint(const Vector &origin);
	void BurrowUse(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	bool IsBurrowed(void);

	void InputAttack(inputdata_t &data);
	void InputUnburrow(inputdata_t &inputdata);
	void InputBurrow(inputdata_t &inputdata);
	void InputBurrowAway(inputdata_t &inputdata);

	void IdleSound(void);
	void AlertSound(void);
	void DeathSound(const CTakeDamageInfo &info);
	void PainSound(const CTakeDamageInfo &info);
	virtual void AttackSound(void);

	Class_T Classify(void) { return CLASS_BARNACLE; }

	float MaxYawSpeed(void)
	{
		switch (GetActivity())
		{
		case ACT_IDLE:
			return 30;

		case ACT_RUN:
		case ACT_WALK:
			return 20;

		case ACT_TURN_LEFT:
		case ACT_TURN_RIGHT:
			return 90;

		case ACT_RANGE_ATTACK1:
			return 30;

		default:
			return 30;
		}

		return BaseClass::MaxYawSpeed();
	}
	bool m_startburrowed_bool;

	const impactdamagetable_t &GetPhysicsImpactDamageTable(void);

	CNetworkVar(float, m_proxy_blood_visual_float);

	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();
protected:
	int m_poseAttackDist, m_poseAttackPitch;
	virtual void PopulatePoseParameters(void);
	//private:
	float m_alertradius_float;
	float m_hydra_unburrow_time;
	int m_hydra_burrow_attempts_int;
	float m_hydra_burrow_time;
	int m_hydraplacement_int;
	bool m_upsidedown_bool;
	bool m_isburrowed_bool;
	bool m_hasspitattack_bool;
	EHANDLE m_lastusedburrowtarget_ehandle; // the info_target_hydra_burrow that I've occupied last
	bool m_unburrowedfirsttime_bool; // the first unburrow should happen at spawn position. Once it's done this bool gets set to 1.
	COutputEvent m_OnBurrowedIn;
	COutputEvent m_OnUnBurrowed;
	COutputEvent m_OnPlayerInAlertRadius;	//Player has entered within m_alertradius_float sphere
	COutputEvent m_OnObjectInAlertRadius;	//Some object has entered within m_alertradius_float sphere

	const char *m_ragdollModelName;

private:
	EHANDLE m_impaleEnt_ehandle;
	float m_bloodsuckingtime_float;
	float m_hydra_last_unburrow_time_float; // keep track of our unburrow to determine if we're allowed to hide again from damage.
	int m_hydra_attacks_since_unburrowing_int; // keep track of number of attacks performed for same purposes.

};

enum
{
	SCHED_HYDRA_BURROW_IN = LAST_SHARED_SCHEDULE,
	SCHED_HYDRA_BURROW_WAIT,
	SCHED_HYDRA_BURROW_OUT,
	SCHED_HYDRA_WAIT_FOR_UNBURROW_TRIGGER,
	SCHED_HYDRA_WAIT_FOR_CLEAR_UNBURROW,
	SCHED_HYDRA_WAIT_UNBURROW,
	SCHED_HYDRA_BURROW_AWAY,
	SCHED_HYDRA_INTERACTION_STAB,
};

enum
{
	TASK_HYDRA_BURROW = LAST_SHARED_TASK,
	TASK_HYDRA_UNBURROW,
	TASK_HYDRA_WAIT_FOR_TRIGGER,
	TASK_HYDRA_VANISH,
	TASK_HYDRA_BURROW_WAIT,
	TASK_HYDRA_CHECK_FOR_UNBURROW,
	TASK_HYDRA_FACE_INTERACTION_STAB,
	TASK_HYDRA_PLAY_INTERACTION_STAB,
	TASK_HYDRA_SUCK_BLOOD,
};
//-----------------------------------------------------------------------------
// Interactions
//-----------------------------------------------------------------------------
int	g_interactionSmallHydraStab = 0;
int	g_interactionSmallHydraStabFail = 0;
int	g_interactionSmallHydraStabHit = 0;


// constraint entity for the stabbed ragdolls
class CHydraImpale : public CBaseAnimating
{
	DECLARE_CLASS(CHydraImpale, CBaseAnimating);
public:
	DECLARE_DATADESC();

	void	Spawn(void);
	void	Precache(void);
	void	ImpaleThink(void);

	IPhysicsConstraint *CreateConstraint(CSmallHydra *pHydra, IPhysicsObject *pTargetPhys, IPhysicsConstraintGroup *pGroup);
	static CHydraImpale *Create(CSmallHydra *pHydra, CBaseEntity *pObject2);

public:
	IPhysicsConstraint *m_pConstraint;
	CHandle<CSmallHydra> m_hHydra;
};

BEGIN_DATADESC(CHydraImpale)
DEFINE_PHYSPTR(m_pConstraint),
DEFINE_FIELD(m_hHydra, FIELD_EHANDLE),
DEFINE_THINKFUNC(ImpaleThink),
END_DATADESC()

LINK_ENTITY_TO_CLASS(hydra_impale, CHydraImpale);

//-----------------------------------------------------------------------------
// Purpose: To by usable by the constraint system, this needs to have a phys model.
//-----------------------------------------------------------------------------
void CHydraImpale::Spawn(void)
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
void CHydraImpale::Precache(void)
{
	PrecacheModel("models/props_junk/cardboard_box001a.mdl");
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Update the impale entity's position to the hydra's desired
//-----------------------------------------------------------------------------
void CHydraImpale::ImpaleThink(void)
{
	if (!m_hHydra)
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

	// Ask the Hydra where it'd like the ragdoll, and move ourselves there
	Vector vecOrigin;
	QAngle vecAngles;
	m_hHydra->GetAttachment(m_hHydra->LookupAttachment("needle"), vecOrigin, vecAngles);
	SetAbsOrigin(vecOrigin);
	SetAbsAngles(vecAngles);

	//	NDebugOverlay::Cross3D( vecOrigin, Vector( -5, -5, -5 ), Vector( 5, 5, 5 ), 255, 0, 0, 20, .1);

	// drip the blood occasionally. DRAMATIC!!!
	if (RandomFloat(0, 1) > 0.95)
	{
		//UTIL_BloodStream(vecOrigin, Vector(0, 0, -64), BLOOD_COLOR_YELLOW, RandomInt(1, 4));
		UTIL_BloodDrips( vecOrigin, Vector(0, 0, -1), BLOOD_COLOR_YELLOW, RandomInt(1, 3) );
	}

	SetNextThink(CURTIME);
}

//-----------------------------------------------------------------------------
// Purpose: Activate/create the constraint
//-----------------------------------------------------------------------------
IPhysicsConstraint *CHydraImpale::CreateConstraint(CSmallHydra *pHydra, IPhysicsObject *pTargetPhys, IPhysicsConstraintGroup *pGroup)
{
	m_hHydra = pHydra;

	IPhysicsObject *pImpalePhysObject = VPhysicsGetObject();
	Assert(pImpalePhysObject);


	constraint_ragdollparams_t constraint;
	constraint.Defaults();
	constraint.axes[0].SetAxisFriction(-15, 15, 20);//(-2, 2, 20);
	constraint.axes[1].SetAxisFriction(-15, 15, 20);//(0, 0, 0);
	constraint.axes[2].SetAxisFriction(-15, 15, 20);

	// Exaggerate the bone's ability to pull the mass of the ragdoll around
	constraint.constraint.bodyMassScale[1] = 5.0f;
	m_pConstraint = physenv->CreateRagdollConstraint(pImpalePhysObject, pTargetPhys, pGroup, constraint);


	/*
	constraint_fixedparams_t constraint;
	constraint.Defaults();
	constraint.InitWithCurrentObjectState(pImpalePhysObject, pTargetPhys);
	constraint.constraint.Defaults();
	constraint.constraint.bodyMassScale[1] = 500.0f;
	constraint.constraint.strength = 1.0f;

	m_pConstraint = physenv->CreateFixedConstraint(pImpalePhysObject, pTargetPhys, pGroup, constraint);
	*/

	if (m_pConstraint)
	{
		m_pConstraint->SetGameData((void *)this);
	}

	SetThink(&CHydraImpale::ImpaleThink);
	SetNextThink(CURTIME);
	//	SetParent(m_hHydra->GetBaseEntity(), m_hHydra->LookupAttachment("needle"));
	return m_pConstraint;
}

//-----------------------------------------------------------------------------
// Purpose: Create a Hydra Impale between the hydra and the entity passed in
//-----------------------------------------------------------------------------
CHydraImpale *CHydraImpale::Create(CSmallHydra *pHydra, CBaseEntity *pTarget)
{
	if (!pHydra || !pTarget) return NULL;

	Vector vecOrigin;
	QAngle vecAngles;
	int attachment = pHydra->LookupAttachment("needle");
	pHydra->GetAttachment(attachment, vecOrigin, vecAngles);
	pTarget->Teleport(&vecOrigin, &vecAngles, &vec3_origin);

	CHydraImpale *pImpale = (CHydraImpale *)CBaseEntity::Create("hydra_impale", vecOrigin, vecAngles);
	if (!pImpale)
		return NULL;

	IPhysicsObject *pTargetPhysObject = pTarget->VPhysicsGetObject();
	if (!pTargetPhysObject)
	{
		DevMsg(" Error: Tried to hydra_impale an entity without a physics model.\n");
		return NULL;
	}

	IPhysicsConstraintGroup *pGroup = NULL;

	// Ragdoll? If so, use it's constraint group
	CRagdollProp *pRagdoll = dynamic_cast<CRagdollProp*>(pTarget);
	if (pRagdoll)
	{
		pTargetPhysObject = pRagdoll->GetRagdoll()->list->pObject;

		// add a bunch of dampening to the ragdoll
		for (int i = 0; i < pRagdoll->GetRagdoll()->listCount; i++)
		{
			float damping, rotdamping;
			pRagdoll->GetRagdoll()->list[i].pObject->GetDamping(&damping, &rotdamping);
			damping *= 50;
			rotdamping *= 50;
			pRagdoll->GetRagdoll()->list[i].pObject->SetDamping(&damping, &rotdamping);
		}

		pGroup = pRagdoll->GetRagdoll()->pGroup;
	}

	if (!pImpale->CreateConstraint(pHydra, pTargetPhysObject, pGroup))
		return NULL;

	return pImpale;
}

void CSmallHydra::Precache(void)
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	PrecacheModel(m_ragdollModelName);
	PrecacheScriptSound("NPC_Hydra.Strike");
	PrecacheScriptSound("NPC_Hydra.Attack");
	PrecacheScriptSound("NPC_Hydra.Alert");
	PrecacheScriptSound("NPC_Hydra.Pain");
	PrecacheScriptSound("NPC_Hydra.BurrowIn");
	PrecacheScriptSound("NPC_Hydra.BurrowOut");
	PrecacheScriptSound("NPC_Hydra.ExtendTentacle");
	PrecacheParticleSystem("blood_impact_blue_01");
	UTIL_PrecacheOther("hydra_impale");
	BaseClass::Precache();
}

void CSmallHydra::Spawn(void)
{
	Precache();
	SetModel(STRING(GetModelName()));

	SetSolid(SOLID_BBOX);
	SetSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MoveType_t(GetNPCScriptData().NPC_Movement_MoveType_int)); // movetype_push

	CollisionProp()->SetSurroundingBoundsType(USE_BEST_COLLISION_BOUNDS);
	SetupHydraBounds(); // custom hull bounds instead of classic hulltype - necessary for hydras placed on walls and ceiling

	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;
	SetDistLook(256);
	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);
	m_NPCState = NPC_STATE_NONE;
	m_takedamage = DAMAGE_YES;

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_TURN_HEAD);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_Climb_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_CLIMB);
	if ((GetNPCScriptData().NPC_Capable_Doors_boolean) == true) CapabilitiesAdd(bits_CAP_DOORS_GROUP);
	NPCInit();

	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));
	m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);

	SetCycle(RandomFloat());

	//See if we're supposed to start burrowed
	if (m_startburrowed_bool)
	{
		AddEffects(EF_NODRAW);
		AddFlag(FL_NOTARGET);
		m_spawnflags |= SF_NPC_GAG;
		m_isburrowed_bool = true;

		SetState(NPC_STATE_IDLE);
		SetActivity((Activity)ACT_HYDRA_BURROW_IDLE);
		SetSchedule(SCHED_HYDRA_WAIT_FOR_UNBURROW_TRIGGER);

		SetUse(&CSmallHydra::BurrowUse);
	}

	// the hydra is luminous, it makes little sense to make it cast shadows.
	AddEffects(EF_NOSHADOW);

	if( ClassMatches("hydra_small"))
		RemoveSpawnFlags(SF_ALWAYS_SERVER_RAGDOLL); // we're creating the ragdoll manually, this flag doesn't matter.

	SetCollisionGroup(HL2COLLISION_GROUP_GUNSHIP);	// This is a hack - we're piggybacking this cgroup 
													// to make hydras clip through each other (they're allowed to do that).

	// hydra feeding blood fx
	RegisterThinkContext("BeginFeedingContext");
	RegisterThinkContext("EndFeedingContext");
	m_proxy_blood_visual_float = 0;
}

#if 0
void CSmallHydra::ComputeWorldSpaceSurroundingBox(Vector *pVecWorldMins, Vector *pVecWorldMaxs)
{
	SetupHydraBounds();

	// Extend our bounding box downwards the length of the tongue
	CollisionProp()->WorldSpaceAABB(pVecWorldMins, pVecWorldMaxs);

	// We really care about the tongue tip. The altitude is not really relevant.
	VectorMin(*pVecWorldMins, *pVecWorldMins, *pVecWorldMins);
	VectorMax(*pVecWorldMaxs, *pVecWorldMaxs, *pVecWorldMaxs);

	//	pVecWorldMins->z -= m_flAltitude;
}
#endif

// This is needed because CAI_BaseNPC prevents this being done
// on entities with MOVETYPE_NONE. As a result, we wouldn't
// be able to hurt the hydra with physical objects.
void CSmallHydra::SetupVPhysicsHull()
{
	if (VPhysicsGetObject())
	{
		// Disable collisions to get 
		VPhysicsGetObject()->EnableCollisions(false);
		VPhysicsDestroyObject();
	}
	VPhysicsInitShadow(true, false);
	IPhysicsObject *pPhysObj = VPhysicsGetObject();
	if (pPhysObj)
	{
		float mass = Studio_GetMass(GetModelPtr());
		if (mass > 0)
		{
			pPhysObj->SetMass(mass);
		}
#if _DEBUG
		else
		{
			DevMsg("Warning: %s has no physical mass\n", STRING(GetModelName()));
		}
#endif
		IPhysicsShadowController *pController = pPhysObj->GetShadowController();
		float avgsize = (WorldAlignSize().x + WorldAlignSize().y) * 0.5;
		pController->SetTeleportDistance(avgsize * 0.5);
		//		m_bCheckContacts = true;
	}
}

void CSmallHydra::SetupHydraBounds(void)
{
	Vector forward, right, up;
	AngleVectors(GetLocalAngles(), &forward, &right, &up);

	Vector mins = (forward * -16.0f) + (right * -16.0f);
	Vector maxs = (forward *  16.0f) + (right *  16.0f) + (up * 128);

	if (mins.x > maxs.x)
	{
		V_swap(mins.x, maxs.x);
	}

	if (mins.y > maxs.y)
	{
		V_swap(mins.y, maxs.y);
	}

	if (mins.z > maxs.z)
	{
		m_upsidedown_bool = true;
		V_swap(mins.z, maxs.z);
	}
	else m_upsidedown_bool = false;

	SetCollisionBounds(mins, maxs);
}

void CSmallHydra::Activate()
{
	BaseClass::Activate();

	SetupVPhysicsHull();
}

void CSmallHydra::OnRestore()
{
	BaseClass::OnRestore();

	if (m_isburrowed_bool)
	{
		AddEffects(EF_NODRAW);
		AddFlag(FL_NOTARGET);
		m_spawnflags |= SF_NPC_GAG;
		m_isburrowed_bool = true;

		SetState(NPC_STATE_IDLE);
		SetActivity((Activity)ACT_HYDRA_BURROW_IDLE);
	}
}

void CSmallHydra::NPCThink(void)
{
	BaseClass::NPCThink();

	if (m_lifeState == LIFE_ALIVE)
	{
		SetupHydraBounds();

		// See how close the player is
		if (GetEnemy())
		{
			float flDistToEnemySqr = (GetAbsOrigin() - GetEnemy()->WorldSpaceCenter()).LengthSqr();
			float flDistThreshold = 100;
			float flDistCheck = MIN(flDistToEnemySqr, flDistThreshold * flDistThreshold);
			Vector vecDistToPose(0, 0, 0);
			vecDistToPose.x = flDistCheck;
			VectorNormalize(vecDistToPose);

			// set hit distance according to distance to enemy
			SetPoseParameter(m_poseAttackDist, sqrt(flDistCheck));

			// check the angle, which way is our enemy
			Vector vecEnemyDir = GetShootEnemyDir(GetAbsOrigin(), false);

			if (/*m_hydraplacement_int == HYDRA_ON_CEILING*/ m_upsidedown_bool) VectorNegate(vecEnemyDir); // ceiling hydras need to aim their pitch opposite of normal way
																										   // todo: handle wall placement too		

			QAngle angDir;
			VectorAngles(vecEnemyDir, angDir);
			float curPitch = GetPoseParameter(m_poseAttackPitch);
			float newPitch;

			// clamp and dampen movement
			newPitch = curPitch + 0.8 * UTIL_AngleDiff(UTIL_ApproachAngle(angDir.x, curPitch, 20), curPitch);
			newPitch = AngleNormalize(newPitch);

			SetPoseParameter(m_poseAttackPitch, newPitch);
		}
		else
		{
			SetPoseParameter(m_poseAttackDist, 64);
			SetPoseParameter(m_poseAttackPitch, 0);
		}
	}
	else
	{
		SetPoseParameter("strike_dist", 64);
		SetPoseParameter("strike_pitch", 0);
	}

	// You're going to make decisions based on this info.  So bump the bone cache after you calculate everything
	InvalidateBoneCache();

	// hyda blood feeding fx - clean up the float in case we were interrupted
	// the activities are a hacky reuse of existing ones.
	// in the qc they correspond with enter/loop/exit for feeding.
	if (GetCurrentActivity() != ACT_JUMP
		&& GetCurrentActivity() != ACT_GLIDE
		&& GetCurrentActivity() != ACT_LAND
		&& m_proxy_blood_visual_float > 0.0f)
		m_proxy_blood_visual_float -= 0.2f;

	SetNextThink(CURTIME + 0.1f);
}

void CSmallHydra::RunAI(void)
{
	BaseClass::RunAI();
	if (m_alertradius_float > 0)
	{
		CBaseEntity *pEntity = NULL;
		if ((IsCurSchedule(SCHED_HYDRA_WAIT_FOR_UNBURROW_TRIGGER) || m_hCine) && UTIL_FindClientInPVS(edict()))
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
	}
}

// hydra feeding blood fx
void CSmallHydra::BeginFeedingThink()
{
	if (m_proxy_blood_visual_float < 1.0f)
	{
		m_proxy_blood_visual_float += 0.05f;
		SetNextThink(CURTIME + 0.05f, "BeginFeedingContext");
	}
	else
	{
		m_proxy_blood_visual_float = 1.0f;
		SetContextThink(NULL, CURTIME, "BeginFeedingContext");
	}
}

// hydra feeding blood fx
void CSmallHydra::EndFeedingThink()
{
	if (m_proxy_blood_visual_float > 0.0f)
	{
		m_proxy_blood_visual_float -= 0.05f;
		SetNextThink(CURTIME + 0.05f, "EndFeedingContext");
	}
	else
	{
		m_proxy_blood_visual_float = 0.0f;
		SetContextThink(NULL, CURTIME, "EndFeedingContext");
	}
}

void CSmallHydra::Touch(CBaseEntity *pOther)
{
	if (GetHealth() > 0) // somehow this check is needed?
		Attack();
}

void CSmallHydra::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr)
{
	CTakeDamageInfo infoCopy = info;

	BaseClass::TraceAttack(infoCopy, vecDir, ptr);
}

int CSmallHydra::OnTakeDamage_Alive(const CTakeDamageInfo &info)
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

	// Explosions are guaranteed to kill the tree
	if (infoCopy.GetDamageType() & DMG_BLAST)
	{
		infoCopy.SetDamage(GetHealth() + 10);
		infoCopy.ScaleDamageForce(0.5);
	}

	// Make it harder to kill with the crowbar.
	if (infoCopy.GetDamageType() & DMG_CLUB)
	{
		infoCopy.ScaleDamage(0.3);
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

	if (infoCopy.GetAmmoType() == GetAmmoDef()->Index("SMG1"))
	{
		infoCopy.ScaleDamage(1.25);
	}

	if (GetCurrentActivity() == ACT_HYDRA_BURROW_IN)
	{
		if (!(infoCopy.GetAmmoType() == GetAmmoDef()->Index("SMG1")))
		{
			infoCopy.ScaleDamage(0.5);
		}
		else 
		{
			infoCopy.ScaleDamage(0.625);
		}
	}

	if ( GetCurrentActivity() == ACT_HYDRA_BURROW_OUT)
	{
		infoCopy.ScaleDamage(0.1);
	}

	// Fixate our tweaked damage at this point. Below we either 
	// detect slash/blast cut and kill the tree in a special way,
	// or if not just pass this value onto base function.
	int tookDamage = BaseClass::OnTakeDamage_Alive(infoCopy);

	return tookDamage;
}

ConVar hydra_small_constrain_ragdoll("hydra_small_constrain_ragdoll", "0");
void CSmallHydra::Event_Killed(const CTakeDamageInfo &info)
{
	m_proxy_blood_visual_float = 0;

	if (IsCurSchedule(SCHED_HYDRA_BURROW_IN) || IsCurSchedule(SCHED_HYDRA_BURROW_OUT))
	{
		AddEFlags(EF_NOSHADOW);
	}

	Vector vecUp;
	GetVectors(NULL, NULL, &vecUp);
	VectorNormalize(vecUp);

	UTIL_BloodImpact(GetAbsOrigin() + Vector(0, 0, 8), vecUp, BLOOD_COLOR_HYDRA, 32);

	//	CEffectData data;
	//	data.m_vOrigin = GetAbsOrigin() + Vector(0, 0, 8);
	//	data.m_vNormal = vecUp;
	//	DispatchEffect("headcrabdust", data); // temp art
	DispatchParticleEffect("blood_hydra_split_spray", GetAbsOrigin(), GetAbsAngles());

	CTakeDamageInfo infoNew = info;

//	if (ClassMatches("hydra_small")) // (infoNew.GetDamageType() & DMG_BUCKSHOT || infoNew.GetDamageType() & DMG_BLAST || ClassMatches("hydra_small"))
	{
		AddEffects(EF_NODRAW);
		AddSolidFlags(FSOLID_NOT_SOLID);
		infoNew.SetDamageType(DMG_REMOVENORAGDOLL);

		// Spawn ragdoll gib (manually) and boogie it
		CBaseAnimating *pGibAnimating = dynamic_cast<CBaseAnimating*>(this);
		Vector vecGibForce;
		vecGibForce.x = random->RandomFloat(-400, 400);
		vecGibForce.y = random->RandomFloat(-400, 400);
		vecGibForce.z = random->RandomFloat(0, 300);

		CRagdollProp *pRagdollGib = (CRagdollProp *)CBaseEntity::CreateNoSpawn("prop_ragdoll", GetAbsOrigin(), GetAbsAngles(), NULL);
		pRagdollGib->CopyAnimationDataFrom(pGibAnimating);
		pRagdollGib->SetModelName(MAKE_STRING(m_ragdollModelName));
		pRagdollGib->SetOwnerEntity(NULL);
		pRagdollGib->RemoveEffects(EF_NODRAW); // need to clear it, because original entity has it
		pRagdollGib->Spawn();
		pRagdollGib->SetCollisionGroup(COLLISION_GROUP_DEBRIS);

		if (hydra_small_constrain_ragdoll.GetBool())
		{
			CPhysFixed *ragdollConstraint = (CPhysFixed *)CreateNoSpawn("phys_constraint", GetAbsOrigin(), vec3_angle);

			hl_constraint_info_t info;
			info.pObjects[0] = pRagdollGib->VPhysicsGetObject();
			info.pObjects[1] = g_PhysWorldObject;
			info.pGroup = NULL;

		//	constraint_ragdollparams_t constraint;
		//	constraint.Defaults();
		//	constraint.axes[0].SetAxisFriction(-15, 15, 20);//(-2, 2, 20);
		//	constraint.axes[1].SetAxisFriction(-15, 15, 20);//(0, 0, 0);
		//	constraint.axes[2].SetAxisFriction(-15, 15, 20);

			ragdollConstraint->CreateConstraint(NULL, info);
			ragdollConstraint->Spawn();
			ragdollConstraint->Activate();
		}

		// Send the ragdoll into spasmatic agony
		inputdata_t input;
		input.pActivator = this;
		input.pCaller = this;
		input.value.SetFloat(RandomFloat(1, 2)); // N seconds of spasms
		pRagdollGib->InputStartRadgollBoogie(input);
	}
	BaseClass::Event_Killed(infoNew);
}

void CSmallHydra::InputRagdoll(inputdata_t &inputdata)
{
	if (IsAlive() == false)
		return;

	//Set us to nearly dead so the velocity from death is minimal
	SetHealth(1);

	CTakeDamageInfo info(this, this, GetHealth(), DMG_CRUSH);
	BaseClass::TakeDamage(info);
}

void CSmallHydra::InputSetAlertRadius(inputdata_t &inputdata)
{
	m_alertradius_float = inputdata.value.Float();
}

void CSmallHydra::BuildScheduleTestBits(void)
{
	//Don't allow any modifications when scripted
	if (m_NPCState == NPC_STATE_SCRIPT)
		return;

	//Interrupt any schedule unless already fleeing, burrowing, burrowed, or unburrowing.
	if (!IsCurSchedule(SCHED_HYDRA_BURROW_IN) &&
		!IsCurSchedule(SCHED_HYDRA_WAIT_UNBURROW) &&
		!IsCurSchedule(SCHED_HYDRA_BURROW_OUT) &&
		!IsCurSchedule(SCHED_HYDRA_BURROW_WAIT) &&
		!IsCurSchedule(SCHED_HYDRA_WAIT_FOR_UNBURROW_TRIGGER) &&
		!IsCurSchedule(SCHED_HYDRA_WAIT_FOR_CLEAR_UNBURROW) &&
		!IsCurSchedule(SCHED_HYDRA_WAIT_UNBURROW) &&
		(GetFlags() & FL_ONGROUND))
	{
		if (GetEnemy() == NULL)
		{
			SetCustomInterruptCondition(COND_HEAR_PHYSICS_DANGER);
		}

		//	SetCustomInterruptCondition(COND_HEAR_THUMPER);
		//	SetCustomInterruptCondition(COND_HEAR_BUGBAIT);
	}
}

void CSmallHydra::GatherConditions(void)
{
	BaseClass::GatherConditions();
}

void CSmallHydra::PrescheduleThink(void)
{
	Activity eActivity = GetActivity();

	// Make sure we've turned off our burrow state if we're not in it
	if (IsEffectActive(EF_NODRAW) &&
		(eActivity != ACT_HYDRA_BURROW_IDLE) &&
		(eActivity != ACT_HYDRA_BURROW_OUT) &&
		(eActivity != ACT_HYDRA_BURROW_IN))
	{
		DevMsg("Hydra failed to unburrow properly!\n");
		Assert(0);
		RemoveEffects(EF_NODRAW);
		RemoveSolidFlags(FSOLID_NOT_SOLID);
		m_takedamage = DAMAGE_YES;
		RemoveFlag(FL_NOTARGET);
		m_spawnflags &= ~SF_NPC_GAG;
	}

	BaseClass::PrescheduleThink();
}

int CSmallHydra::SelectSchedule(void)
{
	// If we're supposed to be burrowed, stay there
	if (m_startburrowed_bool)
		return SCHED_HYDRA_WAIT_FOR_UNBURROW_TRIGGER;

	if (m_isburrowed_bool)
	{
		return SCHED_HYDRA_BURROW_WAIT;
	}

	// If we're flagged to burrow away when eluded, do so
	if ((m_spawnflags & SF_HYDRA_BURROW_ON_ELUDED) && (HasCondition(COND_ENEMY_UNREACHABLE) || HasCondition(COND_ENEMY_TOO_FAR)))
		return SCHED_HYDRA_BURROW_IN;

	if (((GetHealth() <= GetMaxHealth() * 0.33 || HasCondition(COND_HEAVY_DAMAGE)) && !HasSpawnFlags(SF_HYDRA_NEVER_BURROW)) 
		&& (m_hydra_attacks_since_unburrowing_int > 0 || CURTIME > m_hydra_last_unburrow_time_float + 5))
		return SCHED_HYDRA_BURROW_IN;

	return BaseClass::SelectSchedule();
}

int CSmallHydra::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_MELEE_ATTACK1:
		if (FClassnameIs(GetEnemy(), "npc_headcrab")
			|| FClassnameIs(GetEnemy(), "npc_headcrab_fast")
			|| FClassnameIs(GetEnemy(), "npc_headcrab_poison")
			|| FClassnameIs(GetEnemy(), "npc_headcrab_black")
			|| FClassnameIs(GetEnemy(), "npc_hopper")) // todo: add this interaction to hoppers
		{
			return SCHED_HYDRA_INTERACTION_STAB;
		}
		break;
	}
	return BaseClass::TranslateSchedule(scheduleType);
}

void CSmallHydra::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_ANNOUNCE_ATTACK:
	{
		AttackSound();
		TaskComplete();
		break;
	}

	case TASK_HYDRA_WAIT_FOR_TRIGGER:
	{
		m_hydra_unburrow_time = CURTIME + FLT_MAX;

		break;
	}

	case TASK_HYDRA_CHECK_FOR_UNBURROW:
	{
		m_hydra_burrow_attempts_int = 0;
		Vector check = GetAbsOrigin();
		if (m_lastusedburrowtarget_ehandle && m_lastusedburrowtarget_ehandle.Get())
		{
			check = m_lastusedburrowtarget_ehandle.Get()->GetAbsOrigin();
		}
		if (ValidBurrowPoint(check))
		{
			m_spawnflags &= ~SF_NPC_GAG;
			RemoveSolidFlags(FSOLID_NOT_SOLID);
			TaskComplete();
		}

		break;
	}

	case TASK_HYDRA_BURROW_WAIT:
	{
		AddSolidFlags(FSOLID_NOT_SOLID);

		if (pTask->flTaskData == 1.0f)
		{
			//Set our next burrow time
			m_hydra_burrow_time = CURTIME + random->RandomFloat(1, 6);
		}

		if (m_unburrowedfirsttime_bool)
		{
			CUtlVector<CHydraUnburrowPoint *> pointsList;
			CHydraUnburrowPoint *pFoundBurrow = NULL;
			do
			{
				pFoundBurrow = static_cast<CHydraUnburrowPoint*>(gEntList.FindEntityByClassname(pFoundBurrow, "info_target_hydra_unburrow"));
				if (pFoundBurrow)
				{
					if (!(pointsList.HasElement(pFoundBurrow))
						&& !(pFoundBurrow->IsLocked()) // must be available
						&& pFoundBurrow->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= (m_alertradius_float * m_alertradius_float * 2) // must be close
						&& pFoundBurrow->GetOrientation() == (int)m_upsidedown_bool) // must be oriented same as we are
					{
						pointsList.AddToTail(pFoundBurrow);
					}
				}
			} while (pFoundBurrow);

			if (pointsList.Count() > 1)
			{
				int randomInt = RandomInt(0, pointsList.Count() - 1);
				RemoveEffects(EF_NODRAW);
				Teleport(&(pointsList[randomInt]->GetAbsOrigin()), NULL, NULL);
				pointsList[randomInt]->Lock();
				pointsList[randomInt]->HydraSwitchedToTarget();
				m_lastusedburrowtarget_ehandle = pointsList[randomInt];
			}
		}

		break;
	}

	case TASK_HYDRA_BURROW:
	{
		Burrow();

		if (m_lastusedburrowtarget_ehandle != NULL)
		{
			CHydraUnburrowPoint *pLastPoint = assert_cast<CHydraUnburrowPoint*>(m_lastusedburrowtarget_ehandle.Get());

			if (pLastPoint)
			{
				pLastPoint->Unlock(); // let other hydras use this point
				m_lastusedburrowtarget_ehandle = NULL;
			}
		}

		TaskComplete();
		break;
	}

	case TASK_HYDRA_UNBURROW:
	{
		Unburrow();
		TaskComplete();

		break;
	}

	case TASK_HYDRA_VANISH:
	{
		AddEffects(EF_NODRAW);
		AddFlag(FL_NOTARGET);
		m_spawnflags |= SF_NPC_GAG;
		m_isburrowed_bool = true;

		if (m_impaleEnt_ehandle != NULL)
		{
			//	Msg("removing attached ragdoll %s\n", m_impaleEnt_ehandle->GetDebugName());
			UTIL_Remove(m_impaleEnt_ehandle);
			m_impaleEnt_ehandle = NULL;
		} // todo: possibly drop the ragdoll if harmed enough?

		// If the task parameter is non-zero, remove us when we vanish
		if (pTask->flTaskData)
		{
			CBaseEntity *pOwner = GetOwnerEntity();

			if (pOwner != NULL)
			{
				pOwner->DeathNotice(this);
				SetOwnerEntity(NULL);
			}

			// NOTE: We can't UTIL_Remove here, because we're in the middle of running our AI, and
			//		 we'll crash later in the bowels of the AI. Remove ourselves next frame.
			SetThink(&CSmallHydra::SUB_Remove);
			SetNextThink(CURTIME + 0.1);
		}

		SetHealth(GetMaxHealth()); // hydra restores health on burrow

		TaskComplete();

		break;
	}


	case TASK_HYDRA_FACE_INTERACTION_STAB:
	{
		if (GetEnemy() != NULL)
		{
			GetMotor()->SetIdealYaw(CalcIdealYaw(GetEnemy()->GetLocalOrigin()));
			SetTurnActivity();
		}
		else
		{
			TaskFail(FAIL_NO_ENEMY);
		}
		break;
	}

	case TASK_HYDRA_PLAY_INTERACTION_STAB:
	{
		m_last_attack_time = CURTIME;
		ResetIdealActivity((Activity)ACT_MELEE_ATTACK2);
		break;
	}

	case TASK_HYDRA_SUCK_BLOOD:
	{
		ResetIdealActivity((Activity)ACT_IDLE);
		m_bloodsuckingtime_float = CURTIME + 2.0f;
		break;
	}

	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

void CSmallHydra::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_HYDRA_CHECK_FOR_UNBURROW:
	{
		//Must wait for our next check time
		if (m_hydra_burrow_time > CURTIME)
			return;

		//See if we can pop up
		Vector check = GetAbsOrigin();
		if (m_lastusedburrowtarget_ehandle && m_lastusedburrowtarget_ehandle.Get())
		{
			check = m_lastusedburrowtarget_ehandle.Get()->GetAbsOrigin();
		}
		if (ValidBurrowPoint(check))
		{
			m_spawnflags &= ~SF_NPC_GAG;
			RemoveSolidFlags(FSOLID_NOT_SOLID);

			TaskComplete();
			return;
		}

		//Try again in a couple of seconds
		m_hydra_burrow_time = CURTIME + random->RandomFloat(0.5f, 1.0f);
		m_hydra_burrow_attempts_int++;

		if (m_hydra_burrow_attempts_int >= 10)
		{
			m_takedamage = DAMAGE_YES;
			OnTakeDamage(CTakeDamageInfo(this, this, m_iHealth + 1, DMG_REMOVENORAGDOLL | DMG_GENERIC));
		}

		break;
	}

	case TASK_HYDRA_WAIT_FOR_TRIGGER:
	{
		if ((m_hydra_unburrow_time > CURTIME) || GetEntityName() != NULL_STRING)
			return;

		TaskComplete();

		break;
	}

	case TASK_HYDRA_BURROW_WAIT:
	{
		//See if enough time has passed
		if (m_hydra_burrow_time < CURTIME)
		{
			TaskComplete();
		}

		break;
	}

	case TASK_HYDRA_FACE_INTERACTION_STAB:
	{
		if (GetEnemy() != NULL)
		{
			GetMotor()->SetIdealYawAndUpdate(CalcIdealYaw(GetEnemy()->GetLocalOrigin()));

			if (FacingIdeal())
			{
				if (GetAbsOrigin().DistTo(GetEnemy()->GetAbsOrigin()) <= 150)
				{
					TaskComplete();
				}
			}
		}
		else
		{
			TaskFail(FAIL_NO_ENEMY);
		}
		break;
	}

	case TASK_HYDRA_PLAY_INTERACTION_STAB:
	{
		// Face enemy slightly off center 
		if (GetEnemy() != NULL)
		{
			GetMotor()->SetIdealYawAndUpdate(CalcIdealYaw(GetEnemy()->GetLocalOrigin()), AI_KEEP_YAW_SPEED);
		}

		if (IsActivityFinished())
		{
			TaskComplete();
		}
		break;
	}

	case TASK_HYDRA_SUCK_BLOOD:
	{
		if (CURTIME > m_bloodsuckingtime_float) TaskComplete();
		break;
	}

	default:
		BaseClass::RunTask(pTask);
		break;
	}
}

void CSmallHydra::HandleAnimEvent(animevent_t *pEvent)
{
	if (pEvent->event == AE_HYDRA_ATTACK_BEGIN)
	{
		CPASAttenuationFilter filter(this);
		EmitSound(filter, entindex(), "NPC_Hydra.Strike");
		return;
	}

	if (pEvent->event == AE_HYDRA_ATTACK_HIT)
	{
		if (IsCurSchedule(SCHED_MELEE_ATTACK1))
		{
			QAngle viewPunch(20.0f, 0.0f, -12.0f);
			Vector shove(-250.0f, 1.0f, 1.0f);
			MeleeAttack(150, sk_hydra_small_dmg.GetFloat(), viewPunch, shove);
		}
		else if (IsCurSchedule(SCHED_MELEE_ATTACK2))
			RangeAttack();
		return;
	}

	if (pEvent->event == AE_HYDRA_ATTACK_END)
	{
		SetIdealActivity(ACT_IDLE);
		m_flPlaybackRate = random->RandomFloat(0.8f, 1.4f);
		m_flNextAttack = CURTIME + 0.5f;
		return;
	}

	if (pEvent->event == AE_HYDRA_BURROW_IN)
	{
		//Burrowing sound
		EmitSound("NPC_Hydra.BurrowIn");

		//Shake the screen
		UTIL_ScreenShake(GetAbsOrigin(), 0.5f, 80.0f, 1.0f, 256.0f, SHAKE_START);

		if (GetHintNode())
		{
			GetHintNode()->Unlock(2.0f);
		}

		return;
	}

	if (pEvent->event == AE_HYDRA_BURROW_OUT)
	{
		EmitSound("NPC_Hydra.BurrowOut");

		//Shake the screen
		UTIL_ScreenShake(GetAbsOrigin(), 0.5f, 80.0f, 1.0f, 256.0f, SHAKE_START);

		RemoveEffects(EF_NODRAW);
		RemoveFlag(FL_NOTARGET);

		return;
	}

	if (pEvent->event == AE_HYDRA_INTERACTION_STAB)
	{
		CBaseCombatCharacter* pBCC = GetEnemyCombatCharacterPointer();

		if (pBCC)
		{
			float fDist = (pBCC->GetLocalOrigin() - GetLocalOrigin()).Length();
			if (fDist > 150 || m_impaleEnt_ehandle != NULL || !(pBCC->IsAlive()))
			{
				pBCC->DispatchInteraction(g_interactionSmallHydraStabFail, NULL, this);
				ClearSchedule("failed interaction stab schedule"); // abandon this schedule, don't retreat underground
				m_flNextAttack = CURTIME + 1.0f;
				return;
			}
			else
			{
				//	if (!pBCC->ClassMatches("npc_citizen"))
				{
					CTakeDamageInfo info;
					info.SetDamageType(DMG_SLASH);
					info.SetDamage(sk_hydra_stab_dmg.GetFloat());
					info.SetAttacker(this);
					info.SetInflictor(this);
					CalculateMeleeDamageForce(&info, GetAbsOrigin(), EyeDirection3D(), 1.0f);
					CRagdollProp *pRagdoll = NULL;
					pRagdoll = assert_cast<CRagdollProp *>(CreateServerRagdoll(pBCC, m_nForceBone, info, COLLISION_GROUP_NONE));
					//CRagdollProp *pRagdoll = CreateServerRagdollAttached(pBCC, vec3_origin, -1, COLLISION_GROUP_NONE, pTonguePhysObject, m_hTongueTip, 0, vecBonePos, m_iGrabbedBoneIndex, vec3_origin);
					if (pRagdoll)
					{
						pRagdoll->AddEffects(EF_NODRAW);
						pRagdoll->DisableAutoFade();
						pRagdoll->SetThink(NULL);

						PhysDisableEntityCollisions(pRagdoll->VPhysicsGetObject(), g_PhysWorldObject);
						PhysDisableEntityCollisions(pRagdoll->VPhysicsGetObject(), this->VPhysicsGetObject());

						// Create our impale entity, make the ragdoll follow it
						CHydraImpale::Create(this, pRagdoll);

						if (!m_impaleEnt_ehandle) m_impaleEnt_ehandle = pRagdoll;
					}

					pBCC->DispatchInteraction(g_interactionSmallHydraStabHit, NULL, this);

					UTIL_BloodImpact( pBCC->GetAbsOrigin(), EyeDirection3D(), BLOOD_COLOR_YELLOW, RandomInt(4, 8) );
					if (pRagdoll)
					{
						pRagdoll->RemoveEffects(EF_NODRAW);
					}
				}
				//	else
				//	{
				//		pBCC->DispatchInteraction(g_interactionSmallHydraStabHit, NULL, this);					
				//	}
			}
		}

		return;
	}


	if (pEvent->event == AE_HYDRA_FEEDING_BEGIN)
	{
		SetContextThink(&CSmallHydra::BeginFeedingThink, CURTIME + 0.1f, "BeginFeedingContext");
		return;
	}


	if (pEvent->event == AE_HYDRA_FEEDING_END)
	{
		SetContextThink(&CSmallHydra::EndFeedingThink, CURTIME + 0.1f, "EndFeedingContext");
		return;
	}

	BaseClass::HandleAnimEvent(pEvent);
}

int CSmallHydra::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (flDot < 0.5f)
		return COND_NOT_FACING_ATTACK;

	float flAdjustedDist = 200;

	//	if (GetEnemy())
	//	{
	//		// Give us extra space if our enemy is in a vehicle
	//		CBaseCombatCharacter *pCCEnemy = GetEnemy()->MyCombatCharacterPointer();
	//		if (pCCEnemy != NULL && pCCEnemy->IsInAVehicle())
	//		{
	//			flAdjustedDist *= 2.0f;
	//		}
	//	}

	if (flDist > flAdjustedDist)
		return COND_TOO_FAR_TO_ATTACK;

	trace_t	tr;
	AI_TraceHull(WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), -Vector(8, 8, 8), Vector(8, 8, 8), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction < 1.0f)
		return 0;

	return COND_CAN_MELEE_ATTACK1;
}

int CSmallHydra::MeleeAttack2Conditions(float flDot, float flDist)
{
	if (flDot < 0.5f)
		return COND_NOT_FACING_ATTACK;

	if (flDist <= 200)
		return COND_TOO_CLOSE_TO_ATTACK;

	if (flDist > 1000)
		return COND_TOO_FAR_TO_ATTACK;

	//	trace_t	tr;
	//	AI_TraceHull(WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), -Vector(8, 8, 8), Vector(8, 8, 8), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	//	if (tr.fraction < 1.0f)
	//		return 0; // don't check for it - makes hydras look less sluggish this way

	return COND_CAN_MELEE_ATTACK2;
}

void CSmallHydra::MeleeAttack(float distance, float damage, QAngle &viewPunch, Vector &shove)
{
	Vector vecForceDir;
	Vector vecEnemyDir = GetShootEnemyDir(WorldSpaceCenter(), false);
	Vector vecStart = WorldSpaceCenter();

	// Always hurt bullseyes for now
	if ((GetEnemy() != NULL) && (GetEnemy()->Classify() == CLASS_BULLSEYE))
	{
		vecForceDir = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
		CTakeDamageInfo info(this, this, damage, DMG_SLASH);
		CalculateMeleeDamageForce(&info, vecForceDir, GetEnemy()->GetAbsOrigin());
		GetEnemy()->TakeDamage(info);
		return;
	}

	CBaseEntity *pHurt = CheckTraceHullAttack(vecStart, vecStart + vecEnemyDir * distance, -Vector(16, 16, 16), Vector(16, 16, 16), damage, DMG_SLASH, 5.0f);
	if (pHurt)
	{
		if (FClassnameIs(pHurt, "npc_hydra"))
		{
			return;
		}
		if (FClassnameIs(pHurt, "npc_small_hydra"))
		{
			return;
		}
		if (FClassnameIs(pHurt, "npc_hydra_small"))
		{
			return;
		}
		if (FClassnameIs(pHurt, "hydra_dummy"))
		{
			return;
		}
		if (FClassnameIs(pHurt, "hydra_hitbox"))
		{
			return;
		}

		vecForceDir = (pHurt->WorldSpaceCenter() - WorldSpaceCenter());



		// boost the damage against certain NPCs
		if (FClassnameIs(pHurt, "npc_combine_s"))
		{
			CTakeDamageInfo	dmgInfo(this, this, pHurt->GetMaxHealth() / 2, DMG_CLUB); // soldiers can take 2 hits - armour
			CalculateMeleeDamageForce(&dmgInfo, vecForceDir, pHurt->GetAbsOrigin());
			pHurt->TakeDamage(dmgInfo);
			return;
		}

		if (FClassnameIs(pHurt, "npc_citizen"))
		{
			CTakeDamageInfo	dmgInfo(this, this, pHurt->GetMaxHealth() / 2, DMG_CLUB); // citizens can take 2 hits - agile
			CalculateMeleeDamageForce(&dmgInfo, vecForceDir, pHurt->GetAbsOrigin());
			pHurt->TakeDamage(dmgInfo);
			return;
		}

		if (FClassnameIs(pHurt, "npc_metropolice"))
		{
			CTakeDamageInfo	dmgInfo(this, this, pHurt->GetMaxHealth(), DMG_CLUB); // kill cops in one hit - dumb and clumsy
			CalculateMeleeDamageForce(&dmgInfo, vecForceDir, pHurt->GetAbsOrigin());
			pHurt->TakeDamage(dmgInfo);
			return;
		}

		if (FClassnameIs(pHurt, "npc_headcrab"))
		{
			CTakeDamageInfo	dmgInfo(this, this, pHurt->GetMaxHealth(), DMG_CLUB); // crabs should die instantly from hydras
			CalculateMeleeDamageForce(&dmgInfo, vecForceDir, pHurt->GetAbsOrigin());
			pHurt->TakeDamage(dmgInfo);
			return;
		}

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
		EmitSound("NPC_Hydra.Attack");
	}
	//	else
	//		EmitSound("NPC_Hydra.ExtendTentacle");
}

ConVar sk_hydra_needle_speed("sk_hydra_needle_speed", "600");

void CSmallHydra::RangeAttack(void)
{
	Vector vNeedle;
	Vector	vTarget = GetEnemy()->EyePosition(); // TODO: consider body center?
	GetAttachment(LookupAttachment("needle"), vNeedle, NULL);
	//	Vector vecEnemyDir = GetShootEnemyDir(vNeedle, false);
	Vector vThrow;
	CBaseEntity* pBlocker;
	float flGravity = sk_hydra_needle_speed.GetFloat();
	ThrowLimit(vNeedle, vTarget, flGravity, 3, Vector(0, 0, 0), Vector(0, 0, 0), GetEnemy(), &vThrow, &pBlocker);
	QAngle qNeedle = vec3_angle;
	VectorAngles(vThrow, qNeedle);

	CSharedProjectile *pGrenade = (CSharedProjectile*)CreateEntityByName("shared_projectile");
	pGrenade->SetAbsOrigin(vNeedle);
	pGrenade->SetAbsAngles(qNeedle);
	DispatchSpawn(pGrenade);
	pGrenade->SetThrower(this);
	pGrenade->SetOwnerEntity(this);

	pGrenade->SetProjectileStats(Projectile_LARGE, Projectile_NEEDLE, Projectile_CONTACT, 10.0f, 32.0f, flGravity);
	//	pGrenade->SetModelScale(2);
	pGrenade->SetAbsVelocity(vThrow);
	pGrenade->SetAbsVelocity((vThrow + RandomVector(-0.135f, 0.135f)));

	pGrenade->CreateProjectileEffects();

	// Play a random attack hit sound
	EmitSound("NPC_Hydra.Attack");
}

void CSmallHydra::Attack(void)
{
	if (m_flNextAttack > CURTIME) return;

//	if (GetActivity() == ACT_IDLE)
	{
		ResetIdealActivity(ACT_MELEE_ATTACK1);
		m_flPlaybackRate = random->RandomFloat(1.0, 1.4);
	}

	// disable immediate attacks in a row
	m_flNextAttack = CURTIME + SequenceDuration() + 0.5f;

	// keep track of our attacks. We must perform at least one
	// attack after unburrowing to be allowed to burrow back in.
	m_hydra_attacks_since_unburrowing_int++;
}

bool NPC_CheckBrushExclude(CBaseEntity *pEntity, CBaseEntity *pBrush);
class CTraceFilterSimpleNPCExclude : public CTraceFilterSimple
{
public:
	DECLARE_CLASS(CTraceFilterSimpleNPCExclude, CTraceFilterSimple);

	CTraceFilterSimpleNPCExclude(const IHandleEntity *passentity, int collisionGroup)
		: CTraceFilterSimple(passentity, collisionGroup)
	{
	}

	bool ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask)
	{
		Assert(dynamic_cast<CBaseEntity*>(pHandleEntity));
		CBaseEntity *pTestEntity = static_cast<CBaseEntity*>(pHandleEntity);

		if (GetPassEntity())
		{
			CBaseEntity *pEnt = gEntList.GetBaseEntity(GetPassEntity()->GetRefEHandle());

			if (pEnt->IsNPC())
			{
				if (NPC_CheckBrushExclude(pEnt, pTestEntity) == true)
					return false;
			}
		}
		return BaseClass::ShouldHitEntity(pHandleEntity, contentsMask);
	}
};

bool CSmallHydra::ValidBurrowPoint(const Vector &point)
{
	trace_t	tr;
	Vector localUp;
	GetVectors(NULL, NULL, &localUp);
	CTraceFilterSimpleNPCExclude filter(this, COLLISION_GROUP_NONE);
	if (m_upsidedown_bool)
	{
	//	AI_TraceHull(point, point - localUp * 32, GetHullMins(), GetHullMaxs(),
	//		MASK_NPCSOLID, &filter, &tr);
		return true; // FIXME let's hope for the best
	}
	else
	{
		AI_TraceHull(point, point + localUp * 32, GetHullMins(), GetHullMaxs(),
			MASK_NPCSOLID, &filter, &tr);
	}

	//See if we were able to get there
	if (/*(tr.startsolid) ||*/ (tr.allsolid) || (tr.fraction < 1.0f))
	{
		CBaseEntity *pEntity = tr.m_pEnt;

		//If it's a physics object, attempt to knock is away, unless it's a car
		if ((pEntity) && (pEntity->VPhysicsGetObject()) && (pEntity->GetServerVehicle() == NULL))
		{
			ClearBurrowPoint(point);
		}

		return false;
	}

	return true;
}

void CSmallHydra::Burrow(void)
{
	//Stop us from taking damage and being solid
	m_spawnflags |= SF_NPC_GAG;
	//	m_takedamage = DAMAGE_NO;
	SetCollisionGroup(COLLISION_GROUP_DEBRIS);
	DispatchParticleEffect("blood_impact_blue_01", GetAbsOrigin(), RandomAngle(0, 180));

	m_hydra_attacks_since_unburrowing_int = 0;

	//fire output upon burrowing
	m_OnBurrowedIn.FireOutput(this, this);
}

void CSmallHydra::Unburrow(void)
{
	m_startburrowed_bool = false;
	m_unburrowedfirsttime_bool = true;
	//Become solid again and visible
	m_spawnflags &= ~SF_NPC_GAG;
	SetCollisionGroup(HL2COLLISION_GROUP_GUNSHIP);
	//	m_takedamage = DAMAGE_YES;

	SetGroundEntity(NULL);

	//If we have an enemy, come out facing them
	if (GetEnemy())
	{
		Vector	dir = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
		VectorNormalize(dir);

		QAngle angles = GetAbsAngles();
		angles[YAW] = UTIL_VecToYaw(dir);
		SetLocalAngles(angles);
	}

	DispatchParticleEffect("blood_impact_blue_01", GetAbsOrigin(), RandomAngle(0, 180)); 
	
	m_hydra_last_unburrow_time_float = CURTIME;
	m_isburrowed_bool = false;

	//fire output upon unburrowing
	m_OnUnBurrowed.FireOutput(this, this);
}

void CSmallHydra::ClearBurrowPoint(const Vector &origin)
{
	CBaseEntity *pEntity = NULL;
	float		flDist;
	Vector		vecSpot, vecCenter, vecForce;
	Vector localUp;
	GetVectors(NULL, NULL, &localUp);

	bool bPlayerInSphere = false;

	//Iterate on all entities in the vicinity.
	for (CEntitySphereQuery sphere(origin, 128); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity())
	{
		if (pEntity->Classify() == CLASS_PLAYER)
		{
			bPlayerInSphere = true;
			continue;
		}

		if (pEntity->m_takedamage != DAMAGE_NO && pEntity->Classify() != CLASS_PLAYER && pEntity->VPhysicsGetObject())
		{
			vecSpot = pEntity->BodyTarget(origin);
			vecForce = (vecSpot - origin) + localUp * 16;

			// decrease damage for an ent that's farther from the bomb.
			flDist = VectorNormalize(vecForce);

			//float mass = pEntity->VPhysicsGetObject()->GetMass();
			CollisionProp()->RandomPointInBounds(vec3_origin, Vector(1.0f, 1.0f, 1.0f), &vecCenter);

			if (flDist <= 128.0f)
			{
				pEntity->VPhysicsGetObject()->Wake();
				pEntity->VPhysicsGetObject()->ApplyForceOffset(vecForce * 250.0f, vecCenter);
			}
		}
	}

	//	if (bPlayerInSphere == false)
	{
		//Cause a ruckus
		//		UTIL_ScreenShake(origin, 1.0f, 80.0f, 1.0f, 256.0f, SHAKE_START);
	}
}

void CSmallHydra::BurrowUse(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	//Don't allow us to do this again
	SetUse(NULL);

	//Allow idle sounds again
	m_spawnflags &= ~SF_NPC_GAG;

	//If the player activated this, then take them as an enemy
	if ((pCaller != NULL) && (pCaller->IsPlayer()))
	{
		SetEnemy(pActivator);
	}

	//Start trying to surface
	SetSchedule(SCHED_HYDRA_WAIT_UNBURROW);
}

bool CSmallHydra::IsBurrowed(void)
{
	return m_isburrowed_bool;
}

void CSmallHydra::InputAttack(inputdata_t &data)
{
	Attack();
}

void CSmallHydra::InputUnburrow(inputdata_t &inputdata)
{
	if (IsAlive() == false)
		return;

	if (IsBurrowed())
	{
		SetSchedule(SCHED_HYDRA_BURROW_OUT);
		m_hydra_unburrow_time = CURTIME;
	}
}

void CSmallHydra::InputBurrow(inputdata_t &inputdata)
{
	if (IsAlive() == false)
		return;

	SetSchedule(SCHED_HYDRA_BURROW_IN);
}

void CSmallHydra::InputBurrowAway(inputdata_t &inputdata)
{
	if (IsAlive() == false)
		return;

	SetSchedule(SCHED_HYDRA_BURROW_AWAY);
}

void CSmallHydra::IdleSound(void)
{
	EmitSound("NPC_Hydra.ExtendTentacle");
}

void CSmallHydra::AlertSound(void)
{
	EmitSound("NPC_Hydra.Alert");
}

void CSmallHydra::PainSound(const CTakeDamageInfo &info)
{
	if (info.GetAttacker() != this) // it can be "this" for cleaning up unburrowed stuck hydras
	{
		EmitSound("NPC_Hydra.Pain");
		m_flNextPainSoundTime = CURTIME + RandomFloat(1.0f, 2.0f);
	}
}

void CSmallHydra::DeathSound(const CTakeDamageInfo &info)
{
	if (info.GetAttacker() != this) // it can be "this" for cleaning up unburrowed stuck hydras
	{
		EmitSound("NPC_Hydra.Pain");
	}
}

void CSmallHydra::AttackSound(void)
{
	EmitSound("NPC_Hydra.Attack");
};

void CSmallHydra::PopulatePoseParameters(void)
{
	m_poseAttackPitch = LookupPoseParameter("strike_pitch");
	m_poseAttackDist = LookupPoseParameter("strike_dist");

	BaseClass::PopulatePoseParameters();
}

// This hydra has custom impact damage tables, so it can take grave damage from sawblades.
static impactentry_t smallHydraLinearTable[] =
{
	{ 150 * 150, 5 },
	{ 250 * 250, 10 },
	{ 350 * 350, 50 },
	{ 500 * 500, 100 },
	{ 1000 * 1000, 500 },
};

static impactentry_t smallHydraAngularTable[] =
{
	{ 100 * 100, 35 },  // Sawblade always kills.
	{ 200 * 200, 50 },
	{ 250 * 250, 500 },
};

static impactdamagetable_t gSmallHydraImpactDamageTable =
{
	smallHydraLinearTable,
	smallHydraAngularTable,

	ARRAYSIZE(smallHydraLinearTable),
	ARRAYSIZE(smallHydraAngularTable),

	24 * 24,		// minimum linear speed squared
	360 * 360,	// minimum angular speed squared (360 deg/s to cause spin/slice damage)
	10,			// can't take damage from anything under 10kg

	50,			// anything less than 50kg is "small"
	5,			// never take more than 5 pts of damage from anything under 5kg
	36 * 36,		// <5kg objects must go faster than 36 in/s to do damage

	VPHYSICS_LARGE_OBJECT_MASS,		// large mass in kg 
	4,			// large mass scale (anything over 500kg does 4X as much energy to read from damage table)
	5,			// large mass falling scale (emphasize falling/crushing damage over sideways impacts since the stress will kill you anyway)
	0.0f,		// min vel
};

const impactdamagetable_t &CSmallHydra::GetPhysicsImpactDamageTable(void)
{
	return gSmallHydraImpactDamageTable;
}

BEGIN_DATADESC(CSmallHydra)
DEFINE_FIELD(m_hydra_unburrow_time, FIELD_TIME),
DEFINE_FIELD(m_hydra_burrow_time, FIELD_TIME),
DEFINE_FIELD(m_hydra_burrow_attempts_int, FIELD_INTEGER),
DEFINE_FIELD(m_isburrowed_bool, FIELD_BOOLEAN),
DEFINE_FIELD(m_lastusedburrowtarget_ehandle, FIELD_EHANDLE),
DEFINE_FIELD(m_impaleEnt_ehandle, FIELD_EHANDLE),
DEFINE_FIELD(m_unburrowedfirsttime_bool, FIELD_BOOLEAN),
DEFINE_FIELD(m_bloodsuckingtime_float, FIELD_FLOAT),
DEFINE_FIELD(m_proxy_blood_visual_float, FIELD_FLOAT),
// These are purposefully not saved - save up on memory,
// if players want to bother saving in front of hydras
// to reset their burrowing rights, then let them bother
//DEFINE_FIELD(m_hydra_attacks_since_unburrowing_int, FIELD_INTEGER),
//DEFINE_FIELD(m_hydra_last_unburrow_time_float, FIELD_VOID),

DEFINE_INPUTFUNC(FIELD_VOID, "Attack", InputAttack),
DEFINE_INPUTFUNC(FIELD_VOID, "Unburrow", InputUnburrow),
DEFINE_INPUTFUNC(FIELD_VOID, "Burrow", InputBurrow),
DEFINE_INPUTFUNC(FIELD_VOID, "BurrowAndRemove", InputBurrowAway),
DEFINE_INPUTFUNC(FIELD_VOID, "Ragdoll", InputRagdoll),
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetAlertRadius", InputSetAlertRadius),

DEFINE_USEFUNC(BurrowUse),

DEFINE_KEYFIELD(m_startburrowed_bool, FIELD_BOOLEAN, "startburrowed"),
DEFINE_KEYFIELD(m_alertradius_float, FIELD_FLOAT, "radius"),
DEFINE_KEYFIELD(m_hydraplacement_int, FIELD_INTEGER, "hydraplacement"),
DEFINE_KEYFIELD(m_hasspitattack_bool, FIELD_BOOLEAN, "hasspitattack"),

DEFINE_OUTPUT(m_OnBurrowedIn, "OnUnBurrowedIn"),
DEFINE_OUTPUT(m_OnUnBurrowed, "OnUnBurrowed"),
DEFINE_OUTPUT(m_OnPlayerInAlertRadius, "OnPlayerInAlertRadius"),

DEFINE_THINKFUNC(BeginFeedingThink), // hydra feeding blood fx
DEFINE_THINKFUNC(EndFeedingThink),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CSmallHydra, DT_SmallHydra)
SendPropFloat(SENDINFO(m_proxy_blood_visual_float), 0, SPROP_NOSCALE),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(npc_hydra_small, CSmallHydra);
LINK_ENTITY_TO_CLASS(npc_small_hydra, CSmallHydra);
AI_BEGIN_CUSTOM_NPC(npc_hydra_small, CSmallHydra)

DECLARE_TASK(TASK_HYDRA_BURROW)
DECLARE_TASK(TASK_HYDRA_UNBURROW)
DECLARE_TASK(TASK_HYDRA_WAIT_FOR_TRIGGER)
DECLARE_TASK(TASK_HYDRA_VANISH)
DECLARE_TASK(TASK_HYDRA_BURROW_WAIT)
DECLARE_TASK(TASK_HYDRA_CHECK_FOR_UNBURROW)
DECLARE_TASK(TASK_HYDRA_FACE_INTERACTION_STAB)
DECLARE_TASK(TASK_HYDRA_PLAY_INTERACTION_STAB)
DECLARE_TASK(TASK_HYDRA_SUCK_BLOOD)

DECLARE_ACTIVITY(ACT_HYDRA_BURROW_IN)
DECLARE_ACTIVITY(ACT_HYDRA_BURROW_OUT)
DECLARE_ACTIVITY(ACT_HYDRA_BURROW_IDLE)

DECLARE_ANIMEVENT(AE_HYDRA_ATTACK_BEGIN)
DECLARE_ANIMEVENT(AE_HYDRA_ATTACK_HIT)
DECLARE_ANIMEVENT(AE_HYDRA_ATTACK_END)
DECLARE_ANIMEVENT(AE_HYDRA_BURROW_IN)
DECLARE_ANIMEVENT(AE_HYDRA_BURROW_OUT)
DECLARE_ANIMEVENT(AE_HYDRA_ATTACK_SPIT)
DECLARE_ANIMEVENT(AE_HYDRA_INTERACTION_STAB)
DECLARE_ANIMEVENT(AE_HYDRA_FEEDING_BEGIN)
DECLARE_ANIMEVENT(AE_HYDRA_FEEDING_END)

DECLARE_INTERACTION(g_interactionSmallHydraStab)
DECLARE_INTERACTION(g_interactionSmallHydraStabFail)
DECLARE_INTERACTION(g_interactionSmallHydraStabHit)

//==================================================
// Burrow In
//==================================================

DEFINE_SCHEDULE
(
	SCHED_HYDRA_BURROW_IN,

	" Tasks"
	" TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
	" TASK_HYDRA_BURROW					0"
	" TASK_PLAY_SEQUENCE				ACTIVITY:ACT_HYDRA_BURROW_IN"
	" TASK_HYDRA_VANISH					0"
	" TASK_SET_SCHEDULE					SCHEDULE:SCHED_HYDRA_BURROW_WAIT"
	""
	" Interrupts"
	" COND_TASK_FAILED"
)

//==================================================
// Burrow Wait
//==================================================

DEFINE_SCHEDULE
(
	SCHED_HYDRA_BURROW_WAIT,

	" Tasks"
	" TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_HYDRA_BURROW_WAIT"
	" TASK_HYDRA_BURROW_WAIT			1"
	" TASK_SET_SCHEDULE					SCHEDULE:SCHED_HYDRA_WAIT_FOR_CLEAR_UNBURROW"
	""
	" Interrupts"
	" COND_TASK_FAILED"
)

//==================================================
// Burrow Out
//==================================================

DEFINE_SCHEDULE
(
	SCHED_HYDRA_BURROW_OUT,

	" Tasks"
	" TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_HYDRA_BURROW_WAIT"
	" TASK_HYDRA_UNBURROW				0"
	" TASK_PLAY_SEQUENCE				ACTIVITY:ACT_HYDRA_BURROW_OUT"
	""
	" Interrupts"
	" COND_TASK_FAILED"
)

//==================================================
// Wait for unborrow (triggered)
//==================================================

DEFINE_SCHEDULE
(
	SCHED_HYDRA_WAIT_FOR_UNBURROW_TRIGGER,

	" Tasks"
	" TASK_HYDRA_WAIT_FOR_TRIGGER		0"
	" TASK_PLAY_SEQUENCE				ACTIVITY:ACT_HYDRA_BURROW_OUT"
	""
	" Interrupts"
	" COND_TASK_FAILED"
)

//==================================================
// Wait for clear burrow spot (triggered)
//==================================================

DEFINE_SCHEDULE
(
	SCHED_HYDRA_WAIT_FOR_CLEAR_UNBURROW,

	" Tasks"
	" TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_HYDRA_BURROW_WAIT"
	" TASK_HYDRA_CHECK_FOR_UNBURROW		1"
	" TASK_SET_SCHEDULE					SCHEDULE:SCHED_HYDRA_BURROW_OUT"
	""
	" Interrupts"
	" COND_TASK_FAILED"
)

DEFINE_SCHEDULE
(
	SCHED_HYDRA_WAIT_UNBURROW,

	" Tasks"
	" TASK_HYDRA_BURROW_WAIT			0"
	" TASK_SET_SCHEDULE					SCHEDULE:SCHED_HYDRA_WAIT_FOR_CLEAR_UNBURROW"
	""
	" Interrupts"
	" COND_TASK_FAILED"
)

DEFINE_SCHEDULE
(
	SCHED_HYDRA_BURROW_AWAY,

	" Tasks"
	" TASK_STOP_MOVING					0"
	" TASK_HYDRA_BURROW					0"
	" TASK_PLAY_SEQUENCE				ACTIVITY:ACT_HYDRA_BURROW_IN"
	" TASK_PLAY_SEQUENCE				ACTIVITY:ACT_HYDRA_BURROW_IDLE"
	" TASK_HYDRA_VANISH					1"
	""
	" Interrupts"
	" COND_TASK_FAILED"
)

DEFINE_SCHEDULE
(
	SCHED_HYDRA_INTERACTION_STAB,

	"	Tasks"
	"		TASK_STOP_MOVING					0"
	"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"
	"		TASK_HYDRA_FACE_INTERACTION_STAB	0"
	"		TASK_HYDRA_PLAY_INTERACTION_STAB	0"
	"		TASK_HYDRA_SUCK_BLOOD				0" // spend a few seconds sucking blood out. Todo: actual fx/animation for this!
	"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_HYDRA_BURROW_IN"
	""
	"	Interrupts"
	// New_Enemy	Don't interrupt, finish current attack first
	//"		COND_ENEMY_DEAD" // don't want this - when the target dies by design on task 4, hydra won't go to task 5
	"		COND_TASK_FAILED"
	"		COND_HEAVY_DAMAGE"
	"		COND_ENEMY_OCCLUDED"
)


AI_END_CUSTOM_NPC()

//====================================

class CSmallHydraSeer : public CSmallHydra
{
	DECLARE_CLASS(CSmallHydraSeer, CSmallHydra);
	//	DECLARE_SERVERCLASS();
public:
	CSmallHydraSeer()
	{
		m_ragdollModelName = "models/_Monsters/Xen/hydra_seer_ragdoll.mdl";
		m_hNPCFileInfo = LookupNPCInfoScript("npc_hydra_seer");
		m_alertradius_float = 256.0f;
		m_lastusedburrowtarget_ehandle = NULL;
	};

	~CSmallHydraSeer()
	{
		if (m_lastusedburrowtarget_ehandle != NULL)
		{
			CHydraUnburrowPoint *pLastPoint = assert_cast<CHydraUnburrowPoint*>(m_lastusedburrowtarget_ehandle.Get());

			if (pLastPoint)
			{
				pLastPoint->Unlock(); // let other hydras use this point
				m_lastusedburrowtarget_ehandle = NULL;
			}
		}
	}

	void Precache(void);
	void Spawn(void);
	void NPCThink(void);
	int SelectSchedule(void);
	int TranslateSchedule(int scheduleType);
	virtual void AttackSound(void);

	COutputEvent m_OnPlayerInAlertRadius;	//Player has entered within m_alertradius_float sphere
	COutputEvent m_OnPlayerLeftAlertRadius;	//Player has left the m_alertradius_float sphere
//	COutputEvent m_OnObjectInAlertRadius;	//Some object has entered within m_alertradius_float sphere

	bool m_playerIsInAlertRadius_bool;

	DECLARE_DATADESC();
};

void CSmallHydraSeer::Precache(void)
{
	BaseClass::Precache();

}

void CSmallHydraSeer::Spawn(void)
{
	Precache();
	BaseClass::Spawn();
}

void CSmallHydraSeer::NPCThink(void)
{
	BaseClass::NPCThink();
	
	if (m_lifeState == LIFE_ALIVE)
	{
		CBaseEntity *pEntity = NULL;

		// iterate on all entities in the vicinity.
		while ((pEntity = gEntList.FindEntityInSphere(pEntity, GetAbsOrigin(), m_alertradius_float)) != NULL)
		{
			if (pEntity->IsPlayer() && (pEntity->GetFlags() & FL_NOTARGET) == false)
			{
				if (!m_playerIsInAlertRadius_bool)
				{
					m_OnPlayerInAlertRadius.FireOutput(this, this);
					m_playerIsInAlertRadius_bool = true;
				}
			}
		}

		if (m_playerIsInAlertRadius_bool)
		{
			CBasePlayer *player = UTIL_GetLocalPlayer();
			if ((player->GetAbsOrigin() - GetAbsOrigin()).Length() > m_alertradius_float)
			{
				m_OnPlayerLeftAlertRadius.FireOutput(this, this);
				m_playerIsInAlertRadius_bool = false;
			}
		}

	//		if (GetEnemy())
	//		{
	//			CSoundEnt::InsertSound(SOUND_BUGBAIT, GetEnemy()->GetAbsOrigin(), 1024, 1.5, this);
	//		}
	}
}

int CSmallHydraSeer::SelectSchedule(void)
{

	return BaseClass::SelectSchedule();
}

int CSmallHydraSeer::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_HYDRA_BURROW_WAIT:
		return SCHED_HYDRA_WAIT_FOR_UNBURROW_TRIGGER;
		break;

	default: return BaseClass::TranslateSchedule(scheduleType);
	}
}

void CSmallHydraSeer::AttackSound(void)
{
	return;
}



LINK_ENTITY_TO_CLASS(npc_hydra_seer, CSmallHydraSeer);
//#include "tier0/memdbgoff.h"

BEGIN_DATADESC(CSmallHydraSeer)
DEFINE_FIELD(m_playerIsInAlertRadius_bool, FIELD_BOOLEAN),
DEFINE_KEYFIELD(m_alertradius_float, FIELD_FLOAT, "radius"),
//DEFINE_KEYFIELD(m_startburrowed_bool, FIELD_BOOLEAN, "startburrowed"),
//DEFINE_FIELD(m_unburrowedfirsttime_bool, FIELD_BOOLEAN),
DEFINE_FIELD(m_lastusedburrowtarget_ehandle, FIELD_EHANDLE),
//DEFINE_FIELD(m_hydra_burrow_time, FIELD_TIME),
DEFINE_OUTPUT(m_OnPlayerInAlertRadius, "OnPlayerInAlertRadius"),
DEFINE_OUTPUT(m_OnPlayerLeftAlertRadius, "OnPlayerLeftAlertRadius"),
END_DATADESC()
//====================================

class CSmallHydraSmacker : public CAI_BaseNPC
{
	DECLARE_CLASS(CSmallHydraSmacker, CAI_BaseNPC);
//	DECLARE_SERVERCLASS();
public:
	CSmallHydraSmacker()
	{
		m_hNPCFileInfo = LookupNPCInfoScript("npc_hydra_smacker");
		m_alertradius_float = 256.0f;
		m_numthrownprops_int = 0;
	};

	~CSmallHydraSmacker()
	{
		if (m_grabbed_object_handle.Get())
		{
			UTIL_Remove(m_grabbed_object_handle);
		}
		if (m_projectile_handle.Get())
		{
			UTIL_Remove(m_projectile_handle);
		}
	}

	Vector	EyeOffset(Activity nActivity)
	{
		return WorldSpaceCenter();
	}

	Vector	EyePosition(void)
	{
		return WorldSpaceCenter();
	}

	void Precache(void);
	void Spawn(void);
	void Activate();
	void SetupVPhysicsHull();
	void SetupHydraBounds(void);

	void NPCThink(void);
	void RunAI(void);

	void Touch(CBaseEntity *pOther);
	void TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr);
	int OnTakeDamage_Alive(const CTakeDamageInfo &info);
	void Event_Killed(const CTakeDamageInfo &info);

	void BuildScheduleTestBits(void);
	void GatherConditions(void);
	void PrescheduleThink(void);
	int TranslateSchedule(int scheduleType);
	int SelectSchedule(void);
	void StartTask(const Task_t *pTask);
	void RunTask(const Task_t *pTask);
	void HandleAnimEvent(animevent_t *pEvent);

	int MeleeAttack1Conditions(float flDot, float flDist);
	int MeleeAttack2Conditions(float flDot, float flDist);
	int RangeAttack1Conditions(float flDot, float flDist);
	
	void IdleSound(void);
	void AlertSound(void);
	void DeathSound(const CTakeDamageInfo &info);
	void PainSound(const CTakeDamageInfo &info);
	void AttackSound(void);

	Class_T Classify(void) { return CLASS_BARNACLE; }

	float MaxYawSpeed(void)
	{
		switch (GetActivity())
		{
		case ACT_IDLE:
			return 30;

		case ACT_RUN:
		case ACT_WALK:
			return 20;

		case ACT_TURN_LEFT:
		case ACT_TURN_RIGHT:
			return 90;

		case ACT_MELEE_ATTACK1:
			return 0;

		case ACT_MELEE_ATTACK2:
			return 50;

		default:
			return 30;
		}

		return BaseClass::MaxYawSpeed();
	}

	const impactdamagetable_t &GetPhysicsImpactDamageTable(void);

	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();
protected:
	int m_poseAttackDist, m_poseAttackPitch;
	virtual void PopulatePoseParameters(void);
	//private:
	float m_alertradius_float;
	int m_numthrownprops_int; // counter of real prop physics thrown by this smacker. Prevents them from spamming too many edict-taking props.
	COutputEvent m_OnPlayerInAlertRadius;	//Player has entered within m_alertradius_float sphere
	COutputEvent m_OnObjectInAlertRadius;	//Some object has entered within m_alertradius_float sphere
	CHandle<CPhysicsProp> m_grabbed_object_handle;
	CHandle<CSharedProjectile> m_projectile_handle;
};

enum
{
	SCHED_HYDRA_PICKUP_AND_TOSS = LAST_SHARED_SCHEDULE,
	SCHED_HYDRA_ABANDON_TOSS // used if lost enemy during toss. Just throw the object forward and return to idle.
};

enum
{
	TASK_HYDRA_PICKUP_OBJECT = LAST_SHARED_TASK,
	TASK_HYDRA_WAIT_HOLDING_OBJECT,
	TASK_HYDRA_THROW_OBJECT,
};

#define SMACKER_NUM_THROWABLE_PROPS 3
const char *pszThrowableObjects[SMACKER_NUM_THROWABLE_PROPS] = {
	"models/props_debris/concrete_chunk05g.mdl",
	"models/props_debris/concrete_cynderblock001.mdl",
	"models/props_junk/propanecanister001a.mdl",
//	"models/props_junk/propane_tank001a.mdl",
};

void CSmallHydraSmacker::Precache(void)
{
	const char *pModelName = GetNPCScriptData().NPC_Model_Path_char;
	SetModelName(MAKE_STRING(pModelName));
	PrecacheModel(STRING(GetModelName()));
	PrecacheScriptSound("NPC_Hydra.Strike");
	PrecacheScriptSound("NPC_Hydra.Attack");
	PrecacheScriptSound("NPC_Hydra.Alert");
	PrecacheScriptSound("NPC_Hydra.Pain");
	PrecacheScriptSound("NPC_Hydra.ExtendTentacle");

	for (int i = 0; i < SMACKER_NUM_THROWABLE_PROPS - 1; ++i)
	{
		PrecacheModel(pszThrowableObjects[i]);
	}

	BaseClass::Precache();
}

void CSmallHydraSmacker::Spawn(void)
{
	Precache();
	SetModel(STRING(GetModelName()));

	SetSolid(SOLID_BBOX);
	SetSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MoveType_t(GetNPCScriptData().NPC_Movement_MoveType_int)); // movetype_push

	CollisionProp()->SetSurroundingBoundsType(USE_BEST_COLLISION_BOUNDS); 
	SetupHydraBounds();

	SetBloodColor(GetNPCScriptData().NPC_Stats_BloodColor_int);
	m_flFieldOfView = GetNPCScriptData().NPC_Stats_FOV_float;
	SetDistLook(1024);
	SetHealth(GetNPCScriptData().NPC_Stats_Health_int);
	SetMaxHealth(GetNPCScriptData().NPC_Stats_MaxHealth_int);
	m_NPCState = NPC_STATE_NONE;
	m_takedamage = DAMAGE_YES;

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_TURN_HEAD);
	if ((GetNPCScriptData().NPC_Capable_Jump_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	if ((GetNPCScriptData().NPC_Capable_Squadslots_boolean) == true) CapabilitiesAdd(bits_CAP_SQUAD);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_MeleeAttack2_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_MELEE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	if ((GetNPCScriptData().NPC_Capable_InnateRangeAttack1_boolean) == true) CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK2);
	if ((GetNPCScriptData().NPC_Capable_Climb_boolean) == true) CapabilitiesAdd(bits_CAP_MOVE_CLIMB);
	if ((GetNPCScriptData().NPC_Capable_Doors_boolean) == true) CapabilitiesAdd(bits_CAP_DOORS_GROUP);
	NPCInit();
	
	SetModelScale(RandomFloat(GetNPCScriptData().NPC_Model_ScaleMin_float, GetNPCScriptData().NPC_Model_ScaleMax_float));
	m_nSkin = RandomInt(GetNPCScriptData().NPC_Model_SkinMin_int, GetNPCScriptData().NPC_Model_SkinMax_int);

	SetCycle(RandomFloat());
	
	// the hydra is luminous, it makes little sense to make it cast shadows.
	AddEffects(EF_NOSHADOW);
	
	m_grabbed_object_handle.Set(NULL);
	m_projectile_handle.Set(NULL);

	m_OnSpawned.FireOutput(this, this);
}

void CSmallHydraSmacker::Activate()
{
	BaseClass::Activate();

	SetupVPhysicsHull();
}

// This is needed because CAI_BaseNPC prevents this being done
// on entities with MOVETYPE_NONE. As a result, we wouldn't
// be able to hurt the hydra with physical objects.
void CSmallHydraSmacker::SetupVPhysicsHull()
{
	if (VPhysicsGetObject())
	{
		// Disable collisions to get 
		VPhysicsGetObject()->EnableCollisions(false);
		VPhysicsDestroyObject();
	}
	VPhysicsInitShadow(true, false);
	IPhysicsObject *pPhysObj = VPhysicsGetObject();
	if (pPhysObj)
	{
		float mass = Studio_GetMass(GetModelPtr());
		if (mass > 0)
		{
			pPhysObj->SetMass(mass);
		}
#if _DEBUG
		else
		{
			DevMsg("Warning: %s has no physical mass\n", STRING(GetModelName()));
		}
#endif
		IPhysicsShadowController *pController = pPhysObj->GetShadowController();
		float avgsize = (WorldAlignSize().x + WorldAlignSize().y) * 0.5;
		pController->SetTeleportDistance(avgsize * 0.5);
	}
}

void CSmallHydraSmacker::SetupHydraBounds(void)
{
	Vector forward, right, up;
	AngleVectors(GetLocalAngles(), &forward, &right, &up);

	Vector mins = (forward * -16.0f) + (right * -16.0f);
	Vector maxs = (forward *  16.0f) + (right *  16.0f) + (up * 128);

	if (mins.x > maxs.x)
	{
		V_swap(mins.x, maxs.x);
	}

	if (mins.y > maxs.y)
	{
		V_swap(mins.y, maxs.y);
	}

	if (mins.z > maxs.z)
	{
	//	m_upsidedown_bool = true;
		V_swap(mins.z, maxs.z);
	}
//	else m_upsidedown_bool = false;

	SetCollisionBounds(mins, maxs);
}

void CSmallHydraSmacker::NPCThink(void)
{
	BaseClass::NPCThink();

	if (m_lifeState == LIFE_ALIVE)
	{
		SetupHydraBounds();

		// See how close the player is
		if (GetEnemy())
		{
			float flDistToEnemySqr = (GetAbsOrigin() - GetEnemy()->WorldSpaceCenter()).LengthSqr();
			float flDistThreshold = 100;
			float flDistCheck = MIN(flDistToEnemySqr, flDistThreshold * flDistThreshold);
			Vector vecDistToPose(0, 0, 0);
			vecDistToPose.x = flDistCheck;
			VectorNormalize(vecDistToPose);

			// set hit distance according to distance to enemy
			SetPoseParameter(m_poseAttackDist, sqrt(flDistCheck));

			// check the angle, which way is our enemy
			Vector vecEnemyDir = GetShootEnemyDir(GetAbsOrigin(), false);

			QAngle angDir;
			VectorAngles(vecEnemyDir, angDir);
			float curPitch = GetPoseParameter(m_poseAttackPitch);
			float newPitch;

			// clamp and dampen movement
			newPitch = curPitch + 0.8 * UTIL_AngleDiff(UTIL_ApproachAngle(angDir.x, curPitch, 20), curPitch);
			newPitch = AngleNormalize(newPitch);

			SetPoseParameter(m_poseAttackPitch, newPitch);
		}
		else
		{
			SetPoseParameter(m_poseAttackDist, 64);
			SetPoseParameter(m_poseAttackPitch, 0);
		}
	}
	else
	{
		SetPoseParameter("strike_dist", 64);
		SetPoseParameter("strike_pitch", 0);
	}

	// You're going to make decisions based on this info.  So bump the bone cache after you calculate everything
	InvalidateBoneCache();

	SetNextThink(CURTIME + 0.1f);
}

void CSmallHydraSmacker::RunAI(void)
{
	BaseClass::RunAI();
}

void CSmallHydraSmacker::Touch(CBaseEntity *pOther)
{
}

void CSmallHydraSmacker::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr)
{
	CTakeDamageInfo infoCopy = info;

	BaseClass::TraceAttack(infoCopy, vecDir, ptr);
}

int CSmallHydraSmacker::OnTakeDamage_Alive(const CTakeDamageInfo &info)
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
	//	ThrowObject();
	}

	if (infoCopy.GetAmmoType() == GetAmmoDef()->Index("SMG1"))
	{
		infoCopy.ScaleDamage(1.25);
	}
	
	// Fixate our tweaked damage at this point. Below we either 
	// detect slash/blast cut and kill the tree in a special way,
	// or if not just pass this value onto base function.
	int tookDamage = BaseClass::OnTakeDamage_Alive(infoCopy);

	return tookDamage;
}

void CSmallHydraSmacker::Event_Killed(const CTakeDamageInfo &info)
{
	// lose our grabbed objects, if any.
	if (m_grabbed_object_handle.Get())
	{
		Vector forward, up;
		GetVectors(&forward, NULL, &up);

		m_grabbed_object_handle->SetRenderMode(kRenderNone);
		m_grabbed_object_handle->SetSolid(SOLID_NONE);

		CPhysicsProp *throwProp = assert_cast<CPhysicsProp*>(CreateEntityByName("prop_physics"));
		throwProp->SetModel(m_grabbed_object_handle->GetModelName().ToCStr());
		throwProp->SetAbsOrigin(m_grabbed_object_handle->GetAbsOrigin());
		throwProp->SetAbsAngles(m_grabbed_object_handle->GetAbsAngles());
		throwProp->SetMoveType(MOVETYPE_VPHYSICS);
		throwProp->SetCollisionGroup(COLLISION_GROUP_INTERACTIVE);
		throwProp->Spawn();

		throwProp->ApplyAbsVelocityImpulse(forward * 1 + up * 20);

		UTIL_Remove(m_grabbed_object_handle);
		m_grabbed_object_handle.Set(NULL);
	}

	// if holding a projectile, explode it
	if (m_projectile_handle.Get())
	{
//		m_projectile_handle->Detonate();
	}

	Vector vecUp;
	GetVectors(NULL, NULL, &vecUp);
	VectorNormalize(vecUp);

	UTIL_BloodImpact(GetAbsOrigin() + Vector(0, 0, 8), vecUp, BLOOD_COLOR_HYDRA, 32);

	DispatchParticleEffect("blood_hydra_split_spray", GetAbsOrigin(), GetAbsAngles());

	CTakeDamageInfo infoNew = info;

	BaseClass::Event_Killed(infoNew);
}

void CSmallHydraSmacker::BuildScheduleTestBits(void)
{
	//Don't allow any modifications when scripted
	if (m_NPCState == NPC_STATE_SCRIPT)
		return;

	if (IsCurSchedule(SCHED_MELEE_ATTACK1))
	{
		ClearCustomInterruptCondition(COND_LIGHT_DAMAGE);
		ClearCustomInterruptCondition(COND_REPEATED_DAMAGE);
		ClearCustomInterruptCondition(COND_HEAVY_DAMAGE);
	}
}

void CSmallHydraSmacker::GatherConditions(void)
{
	BaseClass::GatherConditions();
}

void CSmallHydraSmacker::PrescheduleThink(void)
{
//	Activity eActivity = GetActivity();	
	BaseClass::PrescheduleThink();

	// Lost an enemy? lose our grabbed objects, if any.
	if (!GetEnemy() && m_grabbed_object_handle.Get())
	{
		Vector forward, up;
		GetVectors(&forward, NULL, &up);

		m_grabbed_object_handle->SetRenderMode(kRenderNone);
		m_grabbed_object_handle->SetSolid(SOLID_NONE);

		CPhysicsProp *throwProp = assert_cast<CPhysicsProp*>(CreateEntityByName("prop_physics"));
		throwProp->SetModel(m_grabbed_object_handle->GetModelName().ToCStr());
		throwProp->SetAbsOrigin(m_grabbed_object_handle->GetAbsOrigin());
		throwProp->SetAbsAngles(m_grabbed_object_handle->GetAbsAngles());
		throwProp->SetMoveType(MOVETYPE_VPHYSICS);
		throwProp->SetCollisionGroup(COLLISION_GROUP_INTERACTIVE);
		throwProp->Spawn();

		throwProp->ApplyAbsVelocityImpulse(forward * 1 + up * 20);

		UTIL_Remove(m_grabbed_object_handle);
		m_grabbed_object_handle.Set(NULL);
	}

	// if holding a projectile, explode it
	if (!GetEnemy() && m_projectile_handle.Get())
	{
		m_projectile_handle->Detonate();
	}
}

int CSmallHydraSmacker::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_MELEE_ATTACK2:
		return SCHED_HYDRA_PICKUP_AND_TOSS;
		break;
		
	default: return BaseClass::TranslateSchedule(scheduleType);
	}
}

int CSmallHydraSmacker::SelectSchedule(void)
{	
	return BaseClass::SelectSchedule();
}

void CSmallHydraSmacker::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_ANNOUNCE_ATTACK:
	{
		AlertSound();
		TaskComplete();
		break;
	}
	
	case TASK_HYDRA_PICKUP_OBJECT:
	{
		SetActivity(ACT_MELEE_ATTACK2);
	}
	break;

	case TASK_HYDRA_WAIT_HOLDING_OBJECT:
	{
		SetIdealActivity(ACT_IDLE_AIM_STIMULATED);
		IdleSound();
		m_flNextAttack = CURTIME + 2;
	}
	break;

	case TASK_HYDRA_THROW_OBJECT:
	{
		IdleSound();
		SetActivity(ACT_RANGE_ATTACK1);
	}
	break;

	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

void CSmallHydraSmacker::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_HYDRA_PICKUP_OBJECT:
	{
		if (IsActivityFinished())
		{
			TaskComplete();
			return;
		}
	}
	case TASK_HYDRA_THROW_OBJECT:
	{
		if (IsActivityFinished())
		{
			TaskComplete();
			m_flNextAttack = CURTIME + RandomFloat(1, 2.5);
			ResetIdealActivity(ACT_IDLE);
			return;
		}
	}
	break;
	case TASK_HYDRA_WAIT_HOLDING_OBJECT:
	{
		if (m_grabbed_object_handle.Get() == NULL && m_projectile_handle.Get() == NULL)
		{
			Assert(0);
			TaskComplete();
			ResetIdealActivity(ACT_IDLE);
			return;
		}

		if (GetEnemy() == NULL)
		{
			TaskComplete();
			return;
		}

		CalcIdealYaw(GetEnemy()->GetAbsOrigin());
		
		if (CURTIME < m_flNextAttack && FVisible(GetEnemy()))
		{
			TaskComplete();
			return;
		}
	}
	break;
		
	default:
		BaseClass::RunTask(pTask);
		break;
	}
}

void CSmallHydraSmacker::HandleAnimEvent(animevent_t *pEvent)
{
	if( pEvent->event == AE_NPC_LEFTFOOT )
	{
		Vector vecForceDir;
		Vector vecEnemyDir = GetShootEnemyDir(WorldSpaceCenter(), false);
		Vector vecStart = WorldSpaceCenter();

		// Always hurt bullseyes for now
		if ((GetEnemy() != NULL) && (GetEnemy()->Classify() == CLASS_BULLSEYE))
		{
			vecForceDir = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
			CTakeDamageInfo info(this, this, 20, DMG_CLUB);
			CalculateMeleeDamageForce(&info, vecForceDir, GetEnemy()->GetAbsOrigin());
			GetEnemy()->TakeDamage(info);
			return;
		}

		CBaseEntity *pHurt = CheckTraceHullAttack(vecStart, vecStart + vecEnemyDir * 192, -Vector(32, 32, 32), Vector(32, 32, 32), 20, DMG_CLUB, 5.0f);
		if (pHurt)
		{
			if (FClassnameIs(pHurt, "npc_hydra"))
			{
				return;
			}
			if (FClassnameIs(pHurt, "npc_small_hydra"))
			{
				return;
			}
			if (FClassnameIs(pHurt, "npc_hydra_small"))
			{
				return;
			}
			if (FClassnameIs(pHurt, "npc_hydra_seer"))
			{
				return;
			}
			if (FClassnameIs(pHurt, "npc_hydra_smacker"))
			{
				return;
			}
			if (FClassnameIs(pHurt, "hydra_dummy"))
			{
				return;
			}
			if (FClassnameIs(pHurt, "hydra_hitbox"))
			{
				return;
			}

			vecForceDir = (pHurt->WorldSpaceCenter() - WorldSpaceCenter());


			// boost the damage against certain NPCs
			if (FClassnameIs(pHurt, "npc_combine_s"))
			{
				CTakeDamageInfo	dmgInfo(this, this, pHurt->m_iHealth / 2, DMG_CLUB); // soldiers can take 2 hits - armour
				CalculateMeleeDamageForce(&dmgInfo, vecForceDir, pHurt->GetAbsOrigin());
				pHurt->TakeDamage(dmgInfo);
				return;
			}

			if (FClassnameIs(pHurt, "npc_citizen"))
			{
				CTakeDamageInfo	dmgInfo(this, this, pHurt->m_iHealth / 2, DMG_CLUB); // citizens can take 2 hits - agile
				CalculateMeleeDamageForce(&dmgInfo, vecForceDir, pHurt->GetAbsOrigin());
				pHurt->TakeDamage(dmgInfo);
				return;
			}

			if (FClassnameIs(pHurt, "npc_metropolice"))
			{
				CTakeDamageInfo	dmgInfo(this, this, pHurt->m_iHealth, DMG_CLUB); // kill cops in one hit - dumb and clumsy
				CalculateMeleeDamageForce(&dmgInfo, vecForceDir, pHurt->GetAbsOrigin());
				pHurt->TakeDamage(dmgInfo);
				return;
			}

			if (FClassnameIs(pHurt, "npc_headcrab"))
			{
				CTakeDamageInfo	dmgInfo(this, this, pHurt->m_iHealth, DMG_CLUB); // crabs should die instantly from hydras
				CalculateMeleeDamageForce(&dmgInfo, vecForceDir, pHurt->GetAbsOrigin());
				pHurt->TakeDamage(dmgInfo);
				return;
			}
			
			QAngle punch = QAngle(20.0f, 0.0f, -12.0f); 
			Vector shove = Vector(1000.0f, 1.0f, 100.0f);

			CBasePlayer *pPlayer = ToBasePlayer(pHurt);

			if (pPlayer != NULL)
			{
				//Kick the player angles
				if (!(pPlayer->GetFlags() & FL_GODMODE) && pPlayer->GetMoveType() != MOVETYPE_NOCLIP)
				{
					pPlayer->ViewPunch(punch);

					Vector	dir = pHurt->GetAbsOrigin() - GetAbsOrigin();
					VectorNormalize(dir);

					QAngle angles;
					VectorAngles(dir, angles);
					Vector forward, up;
					AngleVectors(angles, &forward, NULL, &up);

					//Push the target back
					pHurt->ApplyAbsVelocityImpulse(forward * shove[0] + up * shove[2]);
				}
			}

			// Play a random attack hit sound
			AttackSound();
		}
	}

	if (pEvent->event == AE_NPC_ITEM_PICKUP)
	{
		if (m_grabbed_object_handle.Get())
		{
			Assert(0);
			UTIL_Remove(m_grabbed_object_handle.Get());
			m_grabbed_object_handle.Set(NULL);
		}

		Vector mouth;
		QAngle mouthAng;
		GetAttachment(LookupAttachment("mouth"), mouth, mouthAng);

		if (RandomFloat(0, 1) < 0.5 || m_numthrownprops_int >= 8)
		{
			CSharedProjectile *mudball = (CSharedProjectile*)CreateEntityByName("shared_projectile");
			mudball->SetAbsOrigin(mouth);
			mudball->SetAbsAngles(mouthAng);
			DispatchSpawn(mudball);
			mudball->SetThrower(this);
			mudball->SetOwnerEntity(this);
			mudball->SetProjectileStats(Projectile_LARGE, Projectile_MUDBALL, Projectile_CONTACT, 10.0f, 32.0f, 400);
			mudball->SetModelScale(1.5);
			mudball->SetParent(this, LookupAttachment("mouth"));
			mudball->SetSolid(SOLID_NONE);
			mudball->SetMoveType(MOVETYPE_NONE);
			mudball->SetAbsVelocity(vec3_origin);

			mudball->CreateProjectileEffects();
			m_projectile_handle = mudball;
		}
		else
		{
			m_grabbed_object_handle = assert_cast<CPhysicsProp*>(CreateEntityByName("prop_physics"));
			m_grabbed_object_handle->SetModel(pszThrowableObjects[RandomInt(0, SMACKER_NUM_THROWABLE_PROPS - 1)]);
			m_grabbed_object_handle->SetAbsOrigin(mouth);
			m_grabbed_object_handle->SetAbsAngles(mouthAng);
			m_grabbed_object_handle->SetMoveType(MOVETYPE_NONE);
			m_grabbed_object_handle->SetCollisionGroup(COLLISION_GROUP_INTERACTIVE);
			m_grabbed_object_handle->SetParent(this, LookupAttachment("mouth"));
			m_grabbed_object_handle->Spawn();
		}
		
		if (UTIL_PointContents(GetAbsOrigin()) & MASK_WATER)
		{
			float flWaterZ = UTIL_FindWaterSurface(GetAbsOrigin(), GetAbsOrigin().z - 128, GetAbsOrigin().z + 128);

			CEffectData	data;
			data.m_fFlags = 0;
			data.m_vOrigin = GetAbsOrigin();
			data.m_vOrigin.z = flWaterZ;
			data.m_vNormal = Vector(0, 0, 1);
			data.m_flScale = RandomFloat(5, 8.0);

			DispatchEffect("watersplash", data);
		}

			IdleSound();

			return;
	}

	if (pEvent->event == AE_NPC_SWISHSOUND)
	{
		Vector forward, up;
		GetVectors(&forward, NULL, &up);
		Vector mouth;
		QAngle mouthAng;
		GetAttachment(LookupAttachment("mouth"), mouth, mouthAng);

		if (m_projectile_handle.Get() != NULL)
		{
			m_projectile_handle->SetMoveType(MOVETYPE_FLYGRAVITY);
			m_projectile_handle->SetSolid(SOLID_BBOX);
			m_projectile_handle->SetAbsOrigin(mouth);
			m_projectile_handle->SetAbsAngles(mouthAng);

			// tumble
			m_projectile_handle->SetLocalAngularVelocity(
				QAngle(random->RandomFloat(-250, -500),
					random->RandomFloat(-250, -500),
					random->RandomFloat(-250, -500)));

			if (GetEnemy())
			{
				m_projectile_handle->ApplyAbsVelocityImpulse(forward * 850 + up * 10);
			}
			else
			{
				m_projectile_handle->ApplyAbsVelocityImpulse(forward * 50 - up * 10);
			}

			m_projectile_handle->m_bIsLive = true;

			m_projectile_handle->SetParent(NULL);

			m_projectile_handle.Set(NULL);

			AttackSound();
		}
		if (m_grabbed_object_handle.Get() != NULL)
		{
			m_grabbed_object_handle->SetRenderMode(kRenderNone);
			m_grabbed_object_handle->SetSolid(SOLID_NONE);

			CPhysicsProp *throwProp = assert_cast<CPhysicsProp*>(CreateEntityByName("prop_physics"));
			throwProp->SetModel(m_grabbed_object_handle->GetModelName().ToCStr());
			throwProp->SetAbsOrigin(m_grabbed_object_handle->GetAbsOrigin());
			throwProp->SetAbsAngles(m_grabbed_object_handle->GetAbsAngles());
			throwProp->SetMoveType(MOVETYPE_VPHYSICS);
			throwProp->SetCollisionGroup(COLLISION_GROUP_INTERACTIVE);
			throwProp->Spawn();
			m_numthrownprops_int++;

			// tumble
			throwProp->SetLocalAngularVelocity(
				QAngle(random->RandomFloat(-250, -500),
					random->RandomFloat(-250, -500),
					random->RandomFloat(-250, -500)));

			if (GetEnemy())
			{
				throwProp->ApplyAbsVelocityImpulse(forward * 850 + up * 10);
			}
			else
			{
				throwProp->ApplyAbsVelocityImpulse(forward * 50 - up * 10);
			}

			UTIL_Remove(m_grabbed_object_handle);
			m_grabbed_object_handle.Set(NULL);

			AttackSound();
		}
		return;
	}
	
//	BaseClass::HandleAnimEvent(pEvent);
}

int CSmallHydraSmacker::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (m_flNextAttack > CURTIME)
		return COND_NONE;
	
	if (flDot < 0.5f)
		return COND_NOT_FACING_ATTACK;

	float flAdjustedDist = 250;

	if (flDist > flAdjustedDist)
		return 0;

	trace_t	tr;
	AI_TraceHull(WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), -Vector(32, 32, 32), Vector(32, 32, 32), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

	if (tr.fraction < 1.0f)
		return 0;

	return COND_CAN_MELEE_ATTACK1;
}

int CSmallHydraSmacker::MeleeAttack2Conditions(float flDot, float flDist)
{
	if (m_flNextAttack > CURTIME)
		return COND_NONE;

	if (m_grabbed_object_handle.Get())
		return COND_NONE; // don't attempt if we're already holding something in our mouth

	if (m_projectile_handle.Get())
		return COND_NONE; // don't attempt if we're already holding something in our mouth

	if (flDot < 0.5f)
		return COND_NOT_FACING_ATTACK;

	float flAdjustedDist = 1000;
	
	if (flDist > flAdjustedDist )
		return COND_TOO_FAR_TO_ATTACK;
	
	return COND_CAN_MELEE_ATTACK2;
}

int CSmallHydraSmacker::RangeAttack1Conditions(float flDot, float flDist)
{
	if (m_grabbed_object_handle.Get() == NULL && m_projectile_handle.Get() == NULL)
		return COND_NONE; // we got nothing to throw!

	if (flDot < 0.5f)
		return COND_NOT_FACING_ATTACK;
		
	return COND_CAN_RANGE_ATTACK1;
}

void CSmallHydraSmacker::IdleSound(void)
{
	EmitSound("NPC_Hydra.ExtendTentacle");
}

void CSmallHydraSmacker::AlertSound(void)
{
	EmitSound("NPC_Hydra.Alert");
}

void CSmallHydraSmacker::PainSound(const CTakeDamageInfo &info)
{
	EmitSound("NPC_Hydra.Pain");
	m_flNextPainSoundTime = CURTIME + RandomFloat(1.0f, 2.0f);
}

void CSmallHydraSmacker::DeathSound(const CTakeDamageInfo &info)
{
	EmitSound("NPC_Hydra.Pain");
}

void CSmallHydraSmacker::AttackSound(void)
{
	EmitSound("NPC_Hydra.Attack");
};

void CSmallHydraSmacker::PopulatePoseParameters(void)
{
	m_poseAttackPitch = LookupPoseParameter("strike_pitch");
	m_poseAttackDist = LookupPoseParameter("strike_dist");

	BaseClass::PopulatePoseParameters();
}

// This hydra has custom impact damage tables, so it can take grave damage from sawblades.
static impactentry_t smackerHydraLinearTable[] =
{
	{ 150 * 150, 5 },
	{ 250 * 250, 10 },
	{ 350 * 350, 50 },
	{ 500 * 500, 100 },
	{ 1000 * 1000, 500 },
};

static impactentry_t smackerHydraAngularTable[] =
{
	{ 100 * 100, 35 },  // Sawblade always kills.
	{ 200 * 200, 50 },
	{ 250 * 250, 500 },
};

static impactdamagetable_t gSmackerHydraImpactDamageTable =
{
	smackerHydraLinearTable,
	smackerHydraAngularTable,

	ARRAYSIZE(smackerHydraLinearTable),
	ARRAYSIZE(smackerHydraAngularTable),

	24 * 24,		// minimum linear speed squared
	360 * 360,	// minimum angular speed squared (360 deg/s to cause spin/slice damage)
	10,			// can't take damage from anything under 10kg

	50,			// anything less than 50kg is "small"
	5,			// never take more than 5 pts of damage from anything under 5kg
	36 * 36,		// <5kg objects must go faster than 36 in/s to do damage

	VPHYSICS_LARGE_OBJECT_MASS,		// large mass in kg 
	4,			// large mass scale (anything over 500kg does 4X as much energy to read from damage table)
	5,			// large mass falling scale (emphasize falling/crushing damage over sideways impacts since the stress will kill you anyway)
	0.0f,		// min vel
};

const impactdamagetable_t &CSmallHydraSmacker::GetPhysicsImpactDamageTable(void)
{
	return gSmackerHydraImpactDamageTable;
}

BEGIN_DATADESC(CSmallHydraSmacker)
DEFINE_FIELD(m_grabbed_object_handle, FIELD_CLASSPTR),
DEFINE_FIELD(m_projectile_handle, FIELD_CLASSPTR),
DEFINE_KEYFIELD(m_alertradius_float, FIELD_FLOAT, "radius"),
DEFINE_FIELD(m_numthrownprops_int, FIELD_INTEGER),
DEFINE_OUTPUT(m_OnPlayerInAlertRadius, "OnPlayerInAlertRadius"),
END_DATADESC()

//IMPLEMENT_SERVERCLASS_ST(CSmallHydra, DT_SmallHydra)
//END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(npc_hydra_smacker, CSmallHydraSmacker);
LINK_ENTITY_TO_CLASS(npc_smacker_hydra, CSmallHydraSmacker);
AI_BEGIN_CUSTOM_NPC(npc_hydra_smacker, CSmallHydraSmacker)

DECLARE_TASK(TASK_HYDRA_PICKUP_OBJECT)
DECLARE_TASK(TASK_HYDRA_WAIT_HOLDING_OBJECT)
DECLARE_TASK(TASK_HYDRA_THROW_OBJECT)

//==================================================
// Burrow In
//==================================================

DEFINE_SCHEDULE
(
	SCHED_HYDRA_PICKUP_AND_TOSS,

	" Tasks"
	" TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_HYDRA_ABANDON_TOSS" //SCHEDULE:SCHED_IDLE_STAND"
	" TASK_STOP_MOVING					0"
	" TASK_FACE_ENEMY					0"
	" TASK_WAIT_FOR_MOVEMENT			0"
	" TASK_HYDRA_PICKUP_OBJECT			0"
	" TASK_FACE_ENEMY					0"
	" TASK_WAIT_FOR_MOVEMENT			0"
	" TASK_HYDRA_WAIT_HOLDING_OBJECT	0"
	" TASK_FACE_ENEMY					0"
	" TASK_WAIT_FOR_MOVEMENT			0"
	" TASK_HYDRA_THROW_OBJECT			0"
	""
	" Interrupts"
	" COND_TASK_FAILED"
)

DEFINE_SCHEDULE
(
	SCHED_HYDRA_ABANDON_TOSS,

	" Tasks"
	" TASK_STOP_MOVING					0"
	" TASK_HYDRA_THROW_OBJECT			0"
	""
	" Interrupts"
	""
)

AI_END_CUSTOM_NPC()
