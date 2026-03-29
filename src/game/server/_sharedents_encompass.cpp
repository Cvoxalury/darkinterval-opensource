#include "cbase.h"
#include "_sharedents_encompass.h"
#include "darkinterval\grenade_toxiccloud.h"
#include "ai_basenpc.h"
#include "basegrenade_shared.h"
#include "beam_shared.h"
#include "env_explosion.h"
#include "fire.h"
#include "gamestats.h"
#include "globalstate.h"
#include "hl2_shareddefs.h"
#include "ieffects.h"
#include "func_brush.h"
#include "model_types.h"
#include "env_particlesmokegrenade.h"
#include "particle_system.h"
#include "soundent.h"
#include "soundenvelope.h"
#include "rumble_shared.h"
#include "smoke_trail.h"
#include "te_effect_dispatch.h"
#include "world.h"

#include "sheetsimulator.h"

#include "tier0\memdbgon.h"

extern ConVar npc_glider_clusterbomb_z;
extern ConVar npc_glider_clusterbomb_g;

// indeces and such
extern short g_sModelIndexFireball; // regular explosion
extern short g_sModelIndexWExplosion; // underwater explosion

									  // class CSharedProjectile;
									  // TODO: Add proper art for each type
static const char *PROJECTILE_ACIDIC = "models/_Monsters/Xen/squid_spitball_large.mdl";
static const char *PROJECTILE_PLASMA = "models/Effects/combineball.mdl";
static const char *PROJECTILE_INFERNO = "models/_Weapons/pr_fireball.mdl";
static const char *PROJECTILE_LIGHTNING = "models/_Weapons/pr_lightning.mdl";
static const char *PROJECTILE_SPORE = "models/Weapons/w_bugbait.mdl";
static const char *PROJECTILE_SMOKE = "models/Weapons/w_bugbait.mdl";
static const char *PROJECTILE_BLAST = "models/_Weapons/oicw_ammo_grenade.mdl";
static const char *PROJECTILE_NEEDLE = "models/_Monsters/Xen/hydra_needle.mdl";
static const char *PROJECTILE_MUDBALL = "models/props_junk/rock001a.mdl"; // TEMP!

static const char *PROJECTILE_CONTEXT_LIFETIME = "LifetimeFinite";
static const char *PROJECTILE_CONTEXT_DOPPLER = "DopplerSound";

static const char *PROJECTILE_DEFAULT_SPRITE = "sprites/physcannon_bluecore2b.vmt";
static const char *PROJECTILE_DEFAULT_TRAIL = "sprites/bluelaser1.vmt";

BEGIN_DATADESC(CSharedProjectile)
DEFINE_FIELD(m_projectilesize_int, FIELD_INTEGER),
DEFINE_FIELD(m_projectiletype_int, FIELD_INTEGER),
DEFINE_FIELD(m_projectilebehavior_int, FIELD_INTEGER),
DEFINE_FIELD(m_projectiledelaytime_float, FIELD_FLOAT),
DEFINE_FIELD(m_projectilesensordist_float, FIELD_FLOAT),
DEFINE_FIELD(m_projectileticktime_time, FIELD_TIME),
DEFINE_FIELD(m_projectilecreation_time, FIELD_FLOAT),
// Function pointers
//DEFINE_THINKFUNC(Think_Homer), // d'oh!
DEFINE_THINKFUNC(Think_Timer),
DEFINE_THINKFUNC(Think_Sensor),

DEFINE_THINKFUNC(ContextThink_Lifetime),
DEFINE_THINKFUNC(ContextThink_Doppler),
DEFINE_ENTITYFUNC(ProjectileTouch),
END_DATADESC()

LINK_ENTITY_TO_CLASS(shared_projectile, CSharedProjectile);

void CSharedProjectile::Precache(void)
{
	PrecacheModel(PROJECTILE_ACIDIC);
	PrecacheModel(PROJECTILE_PLASMA);
	PrecacheModel(PROJECTILE_INFERNO);
	PrecacheModel(PROJECTILE_LIGHTNING);
	PrecacheModel(PROJECTILE_SPORE);
	PrecacheModel(PROJECTILE_SMOKE);
	PrecacheModel(PROJECTILE_BLAST);
	PrecacheModel(PROJECTILE_NEEDLE);
	PrecacheModel(PROJECTILE_MUDBALL);

	PrecacheModel(PROJECTILE_DEFAULT_SPRITE);
	PrecacheModel(PROJECTILE_DEFAULT_TRAIL);

	PrecacheScriptSound("SharedProjectile.Detonation_Acid");
	PrecacheScriptSound("SharedProjectile.Detonation_Plasma");
	PrecacheScriptSound("SharedProjectile.Detonation_Inferno");
	PrecacheScriptSound("SharedProjectile.Detonation_Lightning");
	PrecacheScriptSound("SharedProjectile.Detonation_Spore");
	PrecacheScriptSound("SharedProjectile.Detonation_Smoke");
	PrecacheScriptSound("SharedProjectile.Detonation_Blast");
	PrecacheScriptSound("SharedProjectile.Detonation_Mudball");
	PrecacheScriptSound("SharedProjectile.Timer_Blip");

	PrecacheScriptSound("NPC_Hunter.FlechetteHitBody"); // todo: rename
	PrecacheScriptSound("NPC_Hunter.FlechetteHitWorld");

	PrecacheParticleSystem("hunter_flechette_trail");
	PrecacheParticleSystem("hunter_projectile_explosion_1");
	PrecacheParticleSystem("lightningball_trail");
	PrecacheParticleSystem("antlion_spit_player");
	PrecacheParticleSystem("antlion_spit");
	PrecacheParticleSystem("fireball_burst");
	PrecacheParticleSystem("fireball_trail");
	PrecacheParticleSystem("fireball_muzzle");
	PrecacheParticleSystem("smacker_mudball_trail");
	PrecacheParticleSystem("smoke_trail");
	PrecacheParticleSystem("biospore_explosion_core");
}

void CSharedProjectile::Spawn(void)
{
	Precache();
	SetSolid(SOLID_BBOX);
	SetMoveType(MOVETYPE_FLYGRAVITY);
	SetSolidFlags(FSOLID_NOT_STANDABLE);

	m_takedamage = DAMAGE_NO;
	m_iHealth = 1;

	SetCollisionGroup(HL2COLLISION_GROUP_SPIT);

	AddEFlags(EFL_FORCE_CHECK_TRANSMIT);

	// We're self-illuminating, so we don't take or give shadows
	AddEffects(EF_NOSHADOW | EF_NORECEIVESHADOW);

	m_projectilecreation_time = CURTIME;

	// Create sprite, zero brightness, particular types will override it
	if (m_projectile_sprite == NULL)
	{
		m_projectile_sprite = CSprite::SpriteCreate(PROJECTILE_DEFAULT_SPRITE, GetAbsOrigin(), true);
		m_projectile_sprite->SetParent(this);
		//	m_projectile_sprite->SetAttachment(this, LookupAttachment("sprite"));

		m_projectile_sprite->SetTransparency(kRenderWorldGlow, 255, 255, 255, 190, kRenderFxNoDissipation);
		m_projectile_sprite->SetColor(1, 1, 1);

		m_projectile_sprite->SetBrightness(0, 0.1f);
		m_projectile_sprite->SetScale(0.75f, 0.1f);
		m_projectile_sprite->SetAsTemporary();
	}
}

void CSharedProjectile::OnRestore()
{
	if (GetProjectileType() == Projectile_CONTACT
		|| GetProjectileType() == Projectile_CLUSTER)
	{
		// resume lifetime tracking & doppler
		SetContextThink(&CSharedProjectile::ContextThink_Lifetime,
			CURTIME + 0.1f, PROJECTILE_CONTEXT_LIFETIME);

		SetContextThink(&CSharedProjectile::ContextThink_Doppler,
			CURTIME + 0.05f, PROJECTILE_CONTEXT_DOPPLER);
	}
	BaseClass::OnRestore();
}

void CSharedProjectile::ContextThink_Lifetime()
{
	//	Msg("lifetime %.1f curtime %.1f\n", m_projectilecreation_time, CURTIME);
	if (m_projectiledelaytime_float < 0.0f) // sort of hacky, but not really. If specified negative delay time = N, 
		// it means we want our projectile to live for |N| seconds and then detonate.
	{
		if (CURTIME > m_projectilecreation_time + abs(m_projectiledelaytime_float))
		{
			Detonate();
		}
	}
	else
	{
		if (CURTIME > (m_projectilecreation_time + 2.0f) ) // Allow it to live for no more than 2 seconds total
		{
			if (developer.GetInt())
			{
				NDebugOverlay::Cross3D(GetAbsOrigin(), 6.0f, 255, 75, 0, true, 300);
			}
			UTIL_Remove(this);
			SetThink(NULL);
		}
	}
	
	if (GetProjectileType() == Projectile_LIGHTNING)
	{
		DispatchParticleEffect("lightningball_trail", PATTACH_ABSORIGIN_FOLLOW, this);
	}

	if (GetProjectileType() == Projectile_NEEDLE)
	{
		DispatchParticleEffect("hunter_flechette_trail", PATTACH_ABSORIGIN_FOLLOW, this);	
	}

	if( GetProjectileType() == Projectile_MUDBALL)
	{
	//	DispatchParticleEffect("smacker_mudball_trail", PATTACH_ABSORIGIN_FOLLOW, this);
	}

	SetNextThink(CURTIME + 0.1f, PROJECTILE_CONTEXT_LIFETIME);
}

void CSharedProjectile::ContextThink_Doppler()
{
	if (m_projectiletravel_sound)
	{
		//	Msg("dopplertime %.1f\n", CURTIME);
		// Add a doppler effect to the balls as they travel
		CBaseEntity *pPlayer = AI_GetSinglePlayer();
		if (pPlayer != NULL)
		{
			Vector dir;
			VectorSubtract(pPlayer->GetAbsOrigin(), GetAbsOrigin(), dir);
			VectorNormalize(dir);

			float velReceiver = DotProduct(pPlayer->GetAbsVelocity(), dir);
			float velTransmitter = -DotProduct(GetAbsVelocity(), dir);

			// speed of sound == 13049in/s
			int iPitch = 100 * ((1 - velReceiver / 13049) / (1 + velTransmitter / 13049));

			// clamp pitch shifts
			if (iPitch > 250)
			{
				iPitch = 250;
			}
			if (iPitch < 50)
			{
				iPitch = 50;
			}
			// Set the pitch we've calculated
			CSoundEnvelopeController::GetController().SoundChangePitch(m_projectiletravel_sound, iPitch, 0.1f);
		}
	}

	if (m_projectile_sprite)
	{
		m_projectile_sprite->SetScale(RandomFloat(0.4, 0.65), 0.1f);
	}
	// Set us up to think again shortly
	SetNextThink(CURTIME + 0.05f, PROJECTILE_CONTEXT_DOPPLER);
}

void CSharedProjectile::Think_Timer()
{
	if (CURTIME > m_flDetonateTime)
	{
		Detonate();
		return;
	}

	if (!m_bHasWarnedAI && CURTIME >= m_flWarnAITime)
	{
#if !defined( CLIENT_DLL )
		CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin(), 400, 1.5, this);
#endif
		m_bHasWarnedAI = true;
	}

	if (CURTIME > m_projectileticktime_time)
	{
		TimerFX();
		if ((m_projectileticktime_time - CURTIME) < 1.2f)
		{
			m_projectileticktime_time = CURTIME + 0.2f;
		}
		else
		{
			m_projectileticktime_time = CURTIME + 0.6f;
		}
	}

	SetNextThink(CURTIME + 0.1f);
}

void CSharedProjectile::Think_Sensor()
{
	if (CURTIME < m_projectileticktime_time)
	{
		SetTouch(&CSharedProjectile::Touch_Sensor);
	}
	TimerFX();
	SetNextThink(CURTIME + 0.25f);
}

void CSharedProjectile::Touch_Sensor(CBaseEntity *pOther)
{
	if (pOther->IsPlayer())
	{
		SetTouch(NULL);
		m_flDetonateTime = CURTIME + 0.25f;
		SetRenderColor(50, 255, 50);
		SetTouch(NULL);
		SetThink(&CSharedProjectile::Think_Timer);
		SetNextThink(CURTIME);
	}
	else if (pOther->IsNPC())
	{
		if (pOther->Classify() != GetOwnerEntity()->Classify())
		{
			SetTouch(NULL);
			m_flDetonateTime = CURTIME + 0.3f;
			SetRenderColor(255, 255, 50);
			SetThink(&CSharedProjectile::Think_Timer);
			SetNextThink(CURTIME);
		}
	}
	else
	{
		return;
	}
}

void CSharedProjectile::Detonate(void)
{
	DetonationFX();
	DetonationDamage();
	KillEffects();
	SetContextThink(NULL, 0, PROJECTILE_CONTEXT_LIFETIME);
	SetContextThink(NULL, 0, PROJECTILE_CONTEXT_DOPPLER);
	UTIL_Remove(this);
}

void CSharedProjectile::ProjectileTouch(CBaseEntity *pOther)
{
	CBasePlayer *player = ToBasePlayer(pOther);
	trace_t tracecheck;
	tracecheck = CBaseEntity::GetTouchTrace();

	if (pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER))
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ((pOther->m_takedamage == DAMAGE_NO) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY))
			return;
	}

	// Don't hit other spit
	if (pOther->GetCollisionGroup() == HL2COLLISION_GROUP_SPIT)
		return;

	// Disappear if we touch the sky surface.
	if (tracecheck.surface.flags & SURF_SKY)
	{
		UTIL_Remove(this);
		SetTouch(NULL);
		SetThink(NULL);
	}

#if defined SHARED_PROJECTILE_WATER_COLLIDE
	// We want to collide with water... if we're contact/homing type.
	const trace_t *pTrace = &CBaseEntity::GetTouchTrace();

	// copy out some important things about this trace, because the first TakeDamage
	// call below may cause another trace that overwrites the one global pTrace points
	// at.
	bool bHitWater = ((pTrace->contents & CONTENTS_WATER) != 0);
	//	CBaseEntity *const pTraceEnt = pTrace->m_pEnt;
	const Vector tracePlaneNormal = pTrace->plane.normal;

	if (bHitWater && (GetProjectileType() == Projectile_ACID
		|| GetProjectileType() == Projectile_INFERNO
		|| GetProjectileType() == Projectile_PLASMA)
		&& (GetProjectileBehavior() == Projectile_CONTACT
			|| GetProjectileBehavior() == Projectile_HOMING));
	{
		// spawn fire on contact w/ water
		trace_t trace;
		UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, -128), MASK_SOLID_BRUSHONLY,
			this, COLLISION_GROUP_NONE, &trace);

		// Pull out of the wall a bit
		if (trace.fraction != 1.0)
		{
			SetLocalOrigin(trace.endpos + (trace.plane.normal * 0.6));
		}

		int i;
		QAngle vecTraceAngles;
		Vector vecTraceDir;
		trace_t firetrace;

		for (i = 0; i < 1; i++)
		{
			// build a little ray
			vecTraceAngles[PITCH] = random->RandomFloat(45, 135);
			vecTraceAngles[YAW] = random->RandomFloat(0, 360);
			vecTraceAngles[ROLL] = 0.0f;

			AngleVectors(vecTraceAngles, &vecTraceDir);

			Vector vecStart, vecEnd;

			vecStart = GetAbsOrigin() + (trace.plane.normal * 16);
			vecEnd = vecStart + vecTraceDir * 32;

			UTIL_TraceLine(vecStart, vecEnd, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &firetrace);

			if (firetrace.fraction != 1.0)
			{
				FireSystem_StartFire(firetrace.endpos, 0.2, 0.20, 5.0, (SF_FIRE_SMOKELESS), (CBaseEntity*) this, FIRE_NATURAL);
				//	DispatchParticleEffect( "env_fire_tiny", GetAbsOrigin(), GetAbsAngles(), this );
				//  UTIL_DecalTrace(&firetrace, "SmallScorch");
			}
		}

		SetThink(NULL);
		UTIL_Remove(this);
	}
#endif

	CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin(), m_DmgRadius * 2.0f, 0.5f, GetThrower());

	if (GetProjectileBehavior() == Projectile_CONTACT
		|| GetProjectileBehavior() == Projectile_HOMING
		|| GetProjectileBehavior() == Projectile_CLUSTER)
	{
		// Well... this part originated from the bullsquid spits.
		// When the projectile hits a living target, the thrower
		// is notified to take action (according to its AI).
		// But this looks universal and useful enough to just 
		// make it general.
		if ((pOther->IsPlayer()) || (pOther->IsNPC() && pOther->IsAlive()))
		{
			// Blow up immediately on contact.
			Detonate(); // BUGBUG - for some reason it's necessary here or the bomb will deflect??
			if (GetThrower() == NULL) return;
			if (GetThrower() == pOther) return;
			if (GetThrower()->IsPlayer()) return;
			CAI_BaseNPC *thrower = (CAI_BaseNPC*)(GetThrower());
			if( thrower ) thrower->SetCondition(COND_ENEMY_VULNERABLE);
		}
		else if( !pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS))
			// Blow up immediately on contact.
			Detonate();
	}

	else
	{
		// Sticky and bouncy grenades don't detonate immediately; they
		// use their first collide detect to set timers/feelers.
		SetTouch(NULL);
		switch (GetProjectileBehavior())
		{
		case Projectile_STICKYDELAY:
		{
			// Don't stick if already stuck
			if (GetMoveType() == MOVETYPE_FLYGRAVITY)
			{
				SetGravity(1.0f);
				SetAbsVelocity(vec3_origin);
				SetMoveType(MOVETYPE_NONE);
				if (!pOther->IsWorld())
				{
					// set up notification if the parent is deleted before we explode
					g_pNotify->AddEntity(this, pOther);
					if ((tracecheck.surface.flags & SURF_HITBOX) && modelinfo->GetModelType(pOther->GetModel()) == mod_studio)
					{
						CBaseAnimating *pOtherAnim = dynamic_cast<CBaseAnimating *>(pOther);
						if (pOtherAnim)
						{
							if (pOtherAnim && pOtherAnim->m_takedamage)
							{
								pOtherAnim->TakeDamage(CTakeDamageInfo(player, pOther, 1, DMG_CLUB));
							}
						}
					}
					SetParent(pOther);
				}
				else
				{
					// If sticky bomb landed on world, delay is a bit bigger,
					// unless it's so small that it was clearly intentional
					if (m_projectiledelaytime_float > 0.01f)
						m_projectiledelaytime_float += 0.5f;
				}

				if (m_projectiledelaytime_float > 0.01f)
					TimerFX(); // blip once right before going to timer... unless told to explode right away
				m_projectileticktime_time = CURTIME + 0.5f;
				m_flDetonateTime = CURTIME + m_projectiledelaytime_float;
				SetThink(&CSharedProjectile::Think_Timer);
				SetNextThink(CURTIME);
			}
		}
		break;
		case Projectile_STICKYSENSOR:
		{
			// TODO: Spawn a trigger box, and a think function tracking the trigger.
			// Don't stick if already stuck
			if (GetMoveType() == MOVETYPE_FLYGRAVITY && !(GetSolidFlags() & FSOLID_NOT_SOLID))
			{
				SetGravity(1.0f);
				SetAbsVelocity(vec3_origin);
				SetMoveType(MOVETYPE_NONE);

				TimerFX();
				// Create the trigger
				float halfradius = m_projectilesensordist_float / 2;
				UTIL_SetSize(this, Vector(-halfradius, -halfradius, 0), Vector(halfradius, halfradius, halfradius));
				AddSolidFlags(FSOLID_NOT_SOLID | FSOLID_TRIGGER);

				SetRenderColor(255, 50, 50);

				m_projectileticktime_time = CURTIME + 0.5f;
				SetThink(&CSharedProjectile::Think_Sensor);
				SetNextThink(CURTIME + 0.1f);
			}
		}
		break;
		case Projectile_BOUNCEDELAY:
		{
			// Start the countdown after first bounce.
			if (GetAbsVelocity().Length() > 0.0)
			{
				TimerFX(); // blip once right before going to timer
				m_projectileticktime_time = CURTIME + 0.5f;
				m_flDetonateTime = CURTIME + m_projectiledelaytime_float;
				SetThink(&CSharedProjectile::Think_Timer);
				SetNextThink(CURTIME);
			}
		}
		break;
		case Projectile_BOUNCESENSOR:
		{
			// TODO: Spawn a trigger box, and a think function tracking the trigger.
			// Start the countdown after first bounce.
			if (GetAbsVelocity().Length() > 0.0 && !(GetSolidFlags() & FSOLID_NOT_SOLID))
			{
				TimerFX();
				// Create the trigger
				float halfradius = m_projectilesensordist_float / 2;
				UTIL_SetSize(this, Vector(-halfradius, -halfradius, 0), Vector(halfradius, halfradius, halfradius));
				AddSolidFlags(FSOLID_NOT_SOLID | FSOLID_TRIGGER);

				SetRenderColor(255, 50, 50);

				m_projectileticktime_time = CURTIME + 0.5f;
				SetThink(&CSharedProjectile::Think_Sensor);
				SetNextThink(CURTIME + 0.1f);
			}
		}
		break;
		default:
			break;
		}
	}
}

void CSharedProjectile::DetonationFX(void)
{
	switch (GetProjectileType())
	{
	case Projectile_ACID:
	{
		EmitSound("SharedProjectile.Detonation_Acid");
		if (GetProjectileSize() == Projectile_LARGE)
		{
			DispatchParticleEffect("antlion_spit", GetAbsOrigin(), vec3_angle);
		}
		else if (GetProjectileSize() == Projectile_SMALL)
		{
			DispatchParticleEffect("antlion_spit_player", GetAbsOrigin(), GetAbsAngles());
		}
		else
		{
		}
	}
	break;

	case Projectile_PLASMA:
	{
		EmitSound("SharedProjectile.Detonation_Plasma");

	}
	break;

	case Projectile_INFERNO:
	{
		DispatchParticleEffect("fireball_burst", GetAbsOrigin(), vec3_angle);
		EmitSound("SharedProjectile.Detonation_Inferno");

	}
	break;

	case Projectile_LIGHTNING:
	{
		EmitSound("SharedProjectile.Detonation_Lightning");
		DispatchParticleEffect("hunter_projectile_explosion_1", GetAbsOrigin(), vec3_angle);
	}
	break;

	case Projectile_SPORE:
	{
		EmitSound("SharedProjectile.Detonation_Spore");
		DispatchParticleEffect("antlion_spit_player", GetAbsOrigin(), GetAbsAngles());

		trace_t	tr;
		Vector traceDir = GetAbsVelocity();

		VectorNormalize(traceDir);

		UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + traceDir * 64, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		if (tr.fraction < 1.0f)
		{
			UTIL_DecalTrace(&tr, "BeerSplash");	//TODO: Make new art
		}
	}
	break;

	case Projectile_SMOKE:
	{
		EmitSound("SharedProjectile.Detonation_Smoke");

	}
	break;

	case Projectile_BLAST:
	{
		EmitSound("SharedProjectile.Detonation_Blast");

	}
	break;

	case Projectile_MUDBALL:
	{
		EmitSound("SharedProjectile.Detonation_Mudball");
		DispatchParticleEffect("biospore_explosion_core", GetAbsOrigin(), GetAbsAngles());
	}

	default:
	{
	}
	break;
	}

	if (m_projectile_sprite)
	{
		m_projectile_sprite->FadeAndDie(0.25f);
		m_projectile_sprite = NULL;
	}
}

void CSharedProjectile::TimerFX(void)
{
	// bio projectiles don't blip.
	// TODO: add proper effects instead.
	if (GetProjectileType() == Projectile_ACID || GetProjectileType() == Projectile_SPORE 
		|| GetProjectileType() == Projectile_NEEDLE || GetProjectileType() == Projectile_MUDBALL)
		return;

	//	Vector pos = GetAbsOrigin();
	//	pos.z += 2.0;
	//	CBroadcastRecipientFilter filter;

	//	filter.MakeReliable();
	//	filter.MakeInitMessage();

	//	te->DynamicLight(filter, 0.0, &pos, 255, 25, 25, 1.0, 256, 0.4, 50);
	// TODO: replace the dlights it with sprites?

	EmitSound("SharedProjectile.Timer_Blip");
}

inline void CSharedProjectile::StickTo(CBaseEntity *pOther, trace_t &tr)
{
	EmitSound("NPC_Hunter.FlechetteHitWorld");

	SetMoveType(MOVETYPE_NONE);

	if (!pOther->IsWorld())
	{
		SetParent(pOther);
		SetSolid(SOLID_NONE);
		SetSolidFlags(FSOLID_NOT_SOLID);
	}

	Vector vecVelocity = GetAbsVelocity();

	SetTouch(NULL);
	
	DangerSoundThink();
	
	static int s_nImpactCount = 0;
	s_nImpactCount++;
	if (s_nImpactCount & 0x01)
	{
		UTIL_ImpactTrace(&tr, DMG_BULLET);

		// Shoot some sparks
		if (UTIL_PointContents(GetAbsOrigin()) != CONTENTS_WATER)
		{
			g_pEffects->Sparks(GetAbsOrigin());
		}
	}
}

void CSharedProjectile::DetonationDamage(void)
{
	// Acidic projectile doesn't deal damage, 
	// but spawns a toxic cloud.
	// Plasma and Inferno start a fire, also deal some 
	// initial damage from explosion.
	// Lightning produces a big "cloud" of momentary
	// damage.
	// And spore grenade does... something. Undecided...

	// in case the center of the projectile is clipping through geometry for any reason, offset:
	Vector fixedOrigin = GetAbsOrigin() + Vector(0, 0, 4);

	switch (GetProjectileType())
	{
	case Projectile_ACID:
	{
		if (GetProjectileSize() == Projectile_LARGE)
		{
			Vector gasmins, gasmaxs;
			Vector gasSrc = fixedOrigin;
			gasSrc.z += 16.0f;
			gasmins = Vector(-32, -32, -32); gasmaxs = Vector(32, 32, 32);
			CToxicCloud *m_hGasCloud;
			m_hGasCloud = CToxicCloud::CreateGasCloud(gasSrc, this,
				"squid_spit_gas_alt", CURTIME + 8.0f, TOXICCLOUD_ACID, 64);
		}
		else
		{
			RadiusDamage(CTakeDamageInfo(this, GetThrower(), m_flDamage / 3, DMG_ACID), fixedOrigin, m_DmgRadius, CLASS_NONE, NULL);
		}

		/*
		ParticleSmokeGrenade *acidCloudCore = (ParticleSmokeGrenade*)CBaseEntity::Create(PARTICLESMOKEGRENADE_ENTITYNAME, gasSrc, QAngle(0, 0, 0), NULL);
		if (acidCloudCore)
		{
		acidCloudCore->SetFadeTime(6.5f, 8.0f);
		acidCloudCore->SetAbsOrigin(gasSrc);
		acidCloudCore->SetVolumeColour(Vector(1.5, 3.3, 0.7));
		acidCloudCore->SetVolumeSize(Vector(16, 16, 16));
		acidCloudCore->SetVolumeEmissionRate(15);
		acidCloudCore->SetExpandTime(2.5f);
		acidCloudCore->FillVolume();
		}
		*/
		/*
		ParticleSmokeGrenade *acidCloudA = (ParticleSmokeGrenade*)CBaseEntity::Create(PARTICLESMOKEGRENADE_ENTITYNAME, gasSrc, QAngle(0, 0, 0), NULL);
		if (acidCloudA)
		{
		acidCloudA->SetFadeTime(4.5f, 7.5f);
		acidCloudA->SetAbsOrigin(gasSrc + Vector(RandomFloat(16, 32), RandomFloat(-4, 4), RandomFloat(-6, -12)));
		acidCloudA->SetVolumeColour(Vector(1.6, 3.0, 0.7));
		acidCloudA->SetVolumeSize(Vector(4, 4, 4));
		acidCloudCore->SetVolumeEmissionRate(10.0f);
		acidCloudA->SetExpandTime(2.5f);
		acidCloudA->FillVolume();
		}

		ParticleSmokeGrenade *acidCloudB = (ParticleSmokeGrenade*)CBaseEntity::Create(PARTICLESMOKEGRENADE_ENTITYNAME, gasSrc, QAngle(0, 0, 0), NULL);
		if (acidCloudB)
		{
		acidCloudB->SetFadeTime(5.5f, 7.25f);
		acidCloudB->SetAbsOrigin(gasSrc + Vector(RandomFloat(-8, -12), RandomFloat(-12, -20), RandomFloat(-6, -12)));
		acidCloudB->SetVolumeColour(Vector(1.8f, 3.3f, 0.2f));
		acidCloudB->SetVolumeSize(Vector(4, 4, 4));
		acidCloudCore->SetVolumeEmissionRate(10.0f);
		acidCloudB->SetExpandTime(2.5f);
		acidCloudB->FillVolume();
		}

		ParticleSmokeGrenade *acidCloudC = (ParticleSmokeGrenade*)CBaseEntity::Create(PARTICLESMOKEGRENADE_ENTITYNAME, gasSrc, QAngle(0, 0, 0), NULL);
		if (acidCloudC)
		{
		acidCloudC->SetFadeTime(4.0f, 8.0f);
		acidCloudC->SetAbsOrigin(gasSrc + Vector(RandomFloat(10, 20), RandomFloat(-15, 15), RandomFloat(-6, -12)));
		acidCloudC->SetVolumeColour(Vector(1.7f, 3.1f, 0.4f));
		acidCloudC->SetVolumeSize(Vector(4, 4, 4));
		acidCloudCore->SetVolumeEmissionRate(10.0f);
		acidCloudC->SetExpandTime(2.5f);
		acidCloudC->FillVolume();
		}
		*/
	}
	break;
	case Projectile_PLASMA:
	{
		RadiusDamage(CTakeDamageInfo(this, GetThrower(), m_flDamage, DMG_PLASMA), fixedOrigin, m_DmgRadius, CLASS_NONE, NULL);
	}
	break;
	case Projectile_INFERNO:
	{
		ExplosionCreate(fixedOrigin, GetAbsAngles(), GetOwnerEntity(), m_flDamage, m_DmgRadius,
			SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_SURFACEONLY | SF_ENVEXPLOSION_NOSOUND,
			0.0f, GetThrower(), DMG_CRUSH);
		
		CFire *pFires[64];
		int fireCount = FireSystem_GetFiresInSphere(pFires, ARRAYSIZE(pFires), false, GetAbsOrigin(), m_DmgRadius);

		for (int i = 0; i < fireCount; i++)
		{
			pFires[i]->AddHeat(100);
		}

		CBaseEntity *pEntity = NULL;

		while ((pEntity = gEntList.FindEntityInSphere(pEntity, GetAbsOrigin(), m_DmgRadius)) != NULL)
		{
			if (!pEntity->IsPlayer())
			{
				if (pEntity->m_takedamage == DAMAGE_YES || pEntity->ClassMatches("physics_prop_ragdoll") || pEntity->ClassMatches("prop_ragdoll"))
				{
					int damageMask = DMG_BURN | DMG_DISSOLVE_BURN;
					// Extra check to see if target is behind cover. If yes, halve the damage.
					trace_t cover;
					UTIL_TraceLine(fixedOrigin, pEntity->GetAbsOrigin(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &cover);

					if (cover.DidHitWorld())
					{
						m_flDamage /= 2;
						damageMask &= ~DMG_DISSOLVE_BURN; // don't dissolve thru objects - only ignite
					}

					CTakeDamageInfo info(GetThrower(), GetThrower(), m_flDamage, DMG_BURN | DMG_DISSOLVE_BURN);
					pEntity->TakeDamage(info);

					if (pEntity->IsNPC())
					{
						CBaseCombatCharacter *entityPointer = ToBaseCombatCharacter(pEntity);
						if (entityPointer != NULL)
						{
							entityPointer->Ignite(entityPointer->GetHealth());
						}
					}
				}
			}
		}

		//	RadiusDamage(CTakeDamageInfo(this, GetThrower(), m_flDamage, DMG_BURN), GetAbsOrigin(), m_DmgRadius, CLASS_NONE, NULL);
	}
	break;
	case Projectile_LIGHTNING:
	{
		RadiusDamage(CTakeDamageInfo(this, GetThrower(), m_flDamage, DMG_SHOCK), fixedOrigin, m_DmgRadius, CLASS_NONE, NULL);
	}
	break;
	case Projectile_SPORE:
	{
		SporeExplosion *pSporeExplosion = SporeExplosion::CreateSporeExplosion();

		if (pSporeExplosion)
		{
			Vector	dir = -GetAbsVelocity();
			VectorNormalize(dir);

			QAngle	angles;
			VectorAngles(dir, angles);

			pSporeExplosion->SetLocalAngles(angles);
			pSporeExplosion->SetLocalOrigin(GetAbsOrigin());
			pSporeExplosion->m_flSpawnRate = 8.0f;
			pSporeExplosion->m_flParticleLifetime = 2.0f;
			pSporeExplosion->SetRenderColor(0.0f, 0.5f, 0.25f, 0.15f);

			pSporeExplosion->m_flStartSize = 32.0f;
			pSporeExplosion->m_flEndSize = 64.0f;
			pSporeExplosion->m_flSpawnRadius = 32.0f;

			pSporeExplosion->SetLifetime(7.0f);
		}


	}
	break;
	case Projectile_SMOKE:
	{
		ParticleSmokeGrenade *pGren = (ParticleSmokeGrenade*)CBaseEntity::Create(PARTICLESMOKEGRENADE_ENTITYNAME, GetAbsOrigin(), QAngle(0, 0, 0), NULL);
		if (pGren)
		{
			pGren->SetFadeTime(17, 22);
			pGren->SetAbsOrigin(GetAbsOrigin());
			pGren->SetVolumeColour(Vector(1.5, 1.3, 0.7));
			pGren->SetVolumeSize(Vector(60, 80, 80));
			pGren->SetVolumeEmissionRate(15);
			pGren->SetExpandTime(0.7f);
			pGren->FillVolume();
		}
	}
	break;
	case Projectile_BLAST:
	{
		CPASFilter filter(fixedOrigin);

		//	filter.MakeReliable();
		//	filter.MakeInitMessage();

		te->Explosion(filter, 0.0,
			&fixedOrigin,
			g_sModelIndexFireball,
			2.0,
			15,
			TE_EXPLFLAG_NONE, //TODO: Consider explosion w/o visuals, use particles in DetonationFX()?
			m_DmgRadius,
			m_flDamage);

		Vector vecForward = GetAbsVelocity();
		VectorNormalize(vecForward);
		trace_t		tr;
		UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + 60 * vecForward, MASK_SHOT,
			this, COLLISION_GROUP_NONE, &tr);

		if ((tr.m_pEnt != GetWorldEntity()) || (tr.hitbox != 0))
		{
			// non-world needs smaller decals
			if (tr.m_pEnt && !tr.m_pEnt->IsNPC())
			{
				UTIL_DecalTrace(&tr, "SmallScorch");
			}
		}
		else
		{
			UTIL_DecalTrace(&tr, "Scorch");
		}

		UTIL_ScreenShake(GetAbsOrigin(), m_flDamage * 0.5, 150.0, 1.0, m_DmgRadius * 3, SHAKE_START);
		RadiusDamage(CTakeDamageInfo(this, GetThrower(), m_flDamage, DMG_BLAST), fixedOrigin, m_DmgRadius, CLASS_NONE, NULL);

	}
	break;
	case Projectile_NEEDLE:
	{
		Vector vecForward;
		GetVectors(&vecForward, NULL, NULL);
		trace_t		tr;
		UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + 16 * vecForward, MASK_SHOT,
			this, COLLISION_GROUP_NONE, &tr);

		if (tr.m_pEnt)
		{

			if (tr.m_pEnt->m_takedamage != DAMAGE_NO)
			{
				Vector	vecNormalizedVel = GetAbsVelocity();

				ClearMultiDamage();
				VectorNormalize(vecNormalizedVel);

				float flDamage = 4;

				CTakeDamageInfo	dmgInfo(this, GetOwnerEntity(), flDamage, DMG_DISSOLVE | DMG_NEVERGIB);
				CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
				dmgInfo.SetDamagePosition(tr.endpos);
				tr.m_pEnt->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);

				ApplyMultiDamage();

				// Keep going through breakable glass.
				if (tr.m_pEnt->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS)
					return;

				SetAbsVelocity(Vector(0, 0, 0));

				// play body "thwack" sound
				EmitSound("NPC_Hunter.FlechetteHitBody");

				StopParticleEffects(this);

				Vector vForward;
				AngleVectors(GetAbsAngles(), &vForward);
				VectorNormalize(vForward);

				trace_t	tr2;
				UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + vForward * 128, MASK_BLOCKLOS, tr.m_pEnt, COLLISION_GROUP_NONE, &tr2);

				if (tr2.fraction != 1.0f)
				{
					if (tr2.m_pEnt == NULL || (tr2.m_pEnt && tr2.m_pEnt->GetMoveType() == MOVETYPE_NONE))
					{
						CEffectData	data;

						data.m_vOrigin = tr2.endpos;
						data.m_vNormal = vForward;
						data.m_nEntIndex = tr2.fraction != 1.0f;
					}
				}

				if (((tr.m_pEnt->GetMoveType() == MOVETYPE_VPHYSICS) || (tr.m_pEnt->GetMoveType() == MOVETYPE_PUSH)) && ((tr.m_pEnt->GetHealth() > 0) || (tr.m_pEnt->m_takedamage == DAMAGE_EVENTS_ONLY)))
				{
					CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(tr.m_pEnt);
					if (pProp)
					{
						pProp->SetInteraction(PROPINTER_PHYSGUN_NOTIFY_CHILDREN);
					}

					// We hit a physics object that survived the impact. Stick to it.
					StickTo(tr.m_pEnt, tr);
				}
				else
				{
					SetTouch(NULL);
					SetThink(NULL);
					SetContextThink(NULL, 0, s_szHunterFlechetteBubbles);

					UTIL_Remove(this);
				}
			}
			else
			{
				// See if we struck the world
				if (tr.m_pEnt->GetMoveType() == MOVETYPE_NONE && !(tr.surface.flags & SURF_SKY))
				{
					// We hit a physics object that survived the impact. Stick to it.
					StickTo(tr.m_pEnt, tr);
				}
				else if (tr.m_pEnt->GetMoveType() == MOVETYPE_PUSH && FClassnameIs(tr.m_pEnt, "func_breakable"))
				{
					// We hit a func_breakable, stick to it.
					// The MOVETYPE_PUSH is a micro-optimization to cut down on the classname checks.
					StickTo(tr.m_pEnt, tr);
				}
				else
				{
					// Put a mark unless we've hit the sky
					if ((tr.surface.flags & SURF_SKY) == false)
					{
						UTIL_ImpactTrace(&tr, DMG_BULLET);
					}

					UTIL_Remove(this);
				}
			}
		}
	}
	case Projectile_MUDBALL:
	{
		RadiusDamage(CTakeDamageInfo(this, GetThrower(), m_flDamage, DMG_CLUB), fixedOrigin, m_DmgRadius, CLASS_NONE, NULL);
	}
	break;
	break;
	default:
	{
	}
	break;
	}

	if (GetProjectileBehavior() == Projectile_CLUSTER)
	{
		// Spawn six mini-copies of myself (except they'll be _CONTACT)
		for (int i = 0; i < 6; i++)
		{
			CSharedProjectile *pMiniBomb = (CSharedProjectile*)CreateEntityByName("shared_projectile");
			pMiniBomb->SetAbsOrigin(GetAbsOrigin() + Vector(0, 0, 4));
			pMiniBomb->SetAbsAngles(vec3_angle);
			DispatchSpawn(pMiniBomb);

			pMiniBomb->SetThrower(GetThrower());
			pMiniBomb->SetOwnerEntity(GetOwnerEntity());

			pMiniBomb->SetProjectileStats(Projectile_SMALL, GetProjectileType(),
				Projectile_BOUNCEDELAY, GetDamage() * 2, GetDamageRadius(), npc_glider_clusterbomb_g.GetFloat(), 128.0f, RandomFloat(0.7, 1.2));

			Vector miniBombToss = Vector(0, 0, npc_glider_clusterbomb_z.GetFloat());

			// Begin angles at 0 (i == 0 -> n == 0),
			// increment by 60, 
			// then convert to radiants
			float n = 60;
			n *= i;
			n *= M_PI;
			n /= 180;

			// Now caulculate the directional forces from the
			// angle (radiant value). We want the minibombs
			// to drop out in a circle, 60 degrees apart, 
			// so we calculate x/y coords of the forces' vectors
			// by the usual formula to find points on a circle.
			// 250-300 is therefore the radius of that circle,
			// how far we want to push the minibombs.
			miniBombToss.x = RandomFloat(250, 300) * cos(n);
			miniBombToss.y = RandomFloat(250, 300) * sin(n);
			miniBombToss.z += RandomFloat(-20, 60);

			pMiniBomb->SetAbsVelocity(miniBombToss);

			pMiniBomb->CreateProjectileEffects();
			pMiniBomb->SetModelScale(1.0);

			pMiniBomb->SetCollisionGroup(HL2COLLISION_GROUP_GUNSHIP);

			// Tumble through the air
			pMiniBomb->SetLocalAngularVelocity(
				QAngle(random->RandomFloat(-250, -500),
					random->RandomFloat(-250, -500),
					random->RandomFloat(-250, -500)));
		}
	}
}

void CSharedProjectile::KillEffects(void)
{
	if (m_projectileparticle_handle)
	{
		UTIL_Remove(m_projectileparticle_handle);
	}

	// Stop our hissing sound
	if (m_projectiletravel_sound != NULL)
	{
		CSoundEnvelopeController::GetController().SoundDestroy(m_projectiletravel_sound);
		m_projectiletravel_sound = NULL;
	}
}

void CSharedProjectile::SetProjectileStats(int nSize, int nType, int nBehavior, float damage, float radius, float gravity, float sensordist, float delaytime)
{
	m_flDamage = damage;
	m_DmgRadius = radius;
	SetGravity(UTIL_ScaleForGravity(gravity));

	switch (nType)
	{
	case Projectile_ACID:
	{
		SetModel(PROJECTILE_ACIDIC);
	}
	break;
	case Projectile_PLASMA:
	{
		SetModel(PROJECTILE_PLASMA);
	}
	break;
	case Projectile_INFERNO:
	{
		SetModel(PROJECTILE_INFERNO);
	}
	break;
	case Projectile_LIGHTNING:
	{
		SetModel(PROJECTILE_LIGHTNING);
		//	SetRenderColor(20, 100, 255);
	}
	break;
	case Projectile_SPORE:
	{
		SetModel(PROJECTILE_SPORE);
	}
	case Projectile_SMOKE:
	{
		SetModel(PROJECTILE_SMOKE);
	}
	break;
	case Projectile_BLAST:
	{
		SetModel(PROJECTILE_BLAST);
	}
	break;
	case Projectile_NEEDLE:
	{
		SetModel(PROJECTILE_NEEDLE);
	}
	break;
	case Projectile_MUDBALL:
	{
		SetModel(PROJECTILE_MUDBALL);
	}
	default:
		break;
	}

	if (nSize == Projectile_LARGE)
	{
		m_projectilesize_int = Projectile_LARGE;
		SetModelScale(RandomFloat(0.95, 1.05));
		UTIL_SetSize(this, Vector(-8, -8, -8), Vector(8, 8, 8));
	}
	else
	{
		m_projectilesize_int = Projectile_SMALL;
		SetModelScale(RandomFloat(0.70, 0.85));
		UTIL_SetSize(this, Vector(-2, -2, -2), Vector(2, 2, 2));
	}

	// All grenades use these - then the functions themselves
	// used to differentiate between types.
	// Except for the Use, because they all explode on use.
	SetUse(&CBaseGrenade::DetonateUse);
	SetTouch(&CSharedProjectile::ProjectileTouch);
	SetNextThink(CURTIME + 0.1f);

	switch (nBehavior)
	{
	case Projectile_CONTACT:
	case Projectile_CLUSTER:
	{
		// Contact grenades follow an arc and affected by gravity.
		SetMoveType(MOVETYPE_FLYGRAVITY);
		SetContextThink(&CSharedProjectile::ContextThink_Lifetime, CURTIME + 0.1f, PROJECTILE_CONTEXT_LIFETIME);
		SetContextThink(&CSharedProjectile::ContextThink_Doppler, CURTIME + 0.1f, PROJECTILE_CONTEXT_DOPPLER);
	}
	break;
	case Projectile_HOMING:
	{
		// Homing grenades aren't affected by gravity.
		SetMoveType(MOVETYPE_FLY);
		SetContextThink(&CSharedProjectile::ContextThink_Doppler, CURTIME + 0.1f, PROJECTILE_CONTEXT_DOPPLER);
		// TODO: Do we want homers to have limited lifetime?
	}
	break;
	case Projectile_STICKYDELAY:
	{
		// Sticky grenades behave like contact, but they stick to the surface
		// instead of immediately exploding.
		SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM);
		// Use a special collision group so it goes through the player.
		SetCollisionGroup(DI_COLLISION_GROUP_STICKYPROJECTILE);
	}
	break;
	case Projectile_STICKYSENSOR:
	{
		SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM);
		SetCollisionGroup(DI_COLLISION_GROUP_STICKYPROJECTILE);
	}
	break;
	case Projectile_BOUNCEDELAY:
	{
		// Bouncing grenades bounce off, obviously,
		// before that they follow an arc and pulled by gravity.
		SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE);
		SetElasticity(1.4f);
		// Use a special collision group so it goes through the player.
		SetCollisionGroup(DI_COLLISION_GROUP_STICKYPROJECTILE);
	}
	break;
	case Projectile_BOUNCESENSOR:
	{
		SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE);
		SetElasticity(1.4f);
		SetCollisionGroup(DI_COLLISION_GROUP_STICKYPROJECTILE);
	}
	break;
	default:
	{
		// We got here because something improperly specified the grenade type.
		SetThink(NULL);
		SetNextThink(TICK_NEVER_THINK);
	}
	break;
	}

	m_projectiletype_int = nType;
	m_projectilebehavior_int = nBehavior;
	m_projectiledelaytime_float = delaytime;
	m_projectilesensordist_float = sensordist;
}

void CSharedProjectile::CreateProjectileEffects(void)
{
	// Create the dust effect in place
	m_projectileparticle_handle = (CParticleSystem *)CreateEntityByName("info_particle_system");

	if (m_projectileparticle_handle != NULL)
	{
		// Setup our basic parameters
		m_projectileparticle_handle->KeyValue("start_active", "1");
		switch (GetProjectileType())
		{
		case Projectile_ACID:
		case Projectile_SPORE:
		{
			m_projectileparticle_handle->KeyValue("effect_name", "antlion_spit_trail");
		}
		break;
		case Projectile_PLASMA:
		case Projectile_INFERNO:
		{
			m_projectileparticle_handle->KeyValue("effect_name", "fireball_trail");
		}
		break;
		case Projectile_LIGHTNING:
		{
			m_projectileparticle_handle->KeyValue("effect_name", "NULL");

			if (m_projectile_sprite)
			{
				m_projectile_sprite->SetColor(110, 170, 255);
				m_projectile_sprite->SetBrightness(150, 0.1f);
				m_projectile_sprite->SetScale(0.75f, 0.1f);
			}

			CSpriteTrail *miniBombtrail = CSpriteTrail::SpriteTrailCreate(PROJECTILE_DEFAULT_TRAIL,
				GetAbsOrigin(), true);

			if (miniBombtrail != NULL)
			{
				miniBombtrail->FollowEntity(this);
				miniBombtrail->SetTransparency(kRenderTransAdd, 25, 200, 195, 200, kRenderFxNone);
				if (GetProjectileSize() == Projectile_LARGE)
				{
					miniBombtrail->SetStartWidth(40.0f);
					miniBombtrail->SetEndWidth(10.0f);
					miniBombtrail->SetLifeTime(0.5f);
				}
				else
				{
					miniBombtrail->SetStartWidth(20.0f);
					miniBombtrail->SetEndWidth(4.0f);
					miniBombtrail->SetLifeTime(0.25f);
				}
			}
		}
		break;
		case Projectile_SMOKE:
		{
			m_projectileparticle_handle->KeyValue("effect_name", "NULL");
		}
		break;
		case Projectile_BLAST:
		{
			m_projectileparticle_handle->KeyValue("effect_name", "smoke_trail");
		}
		break;
		default:
		{
			m_projectileparticle_handle->KeyValue("effect_name", "NULL");
		}
		break;
		}
		m_projectileparticle_handle->SetParent(this);
		m_projectileparticle_handle->SetLocalOrigin(vec3_origin);
		DispatchSpawn(m_projectileparticle_handle);
		if (CURTIME > 0.5f)
			m_projectileparticle_handle->Activate();
	}

	EmitSound_t eSound;
	switch (GetProjectileType())
	{
	case Projectile_ACID:
	//	eSound.m_pSoundName = "NPC_Antlion.PoisonBall";
		eSound.m_pSoundName = "General.StopBurning";
		break;
	case Projectile_PLASMA:
	case Projectile_INFERNO:
	case Projectile_LIGHTNING:
	case Projectile_SPORE:
	case Projectile_BLAST:
		eSound.m_pSoundName = "General.BurningObject";
		break;
	default:
		eSound.m_pSoundName = "General.StopBurning";
		break;
	}
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if (m_projectiletravel_sound == NULL)
	{
		CPASAttenuationFilter filter(this);
		//	filter.MakeReliable();
		//	filter.MakeInitMessage();
		m_projectiletravel_sound = controller.SoundCreate(filter, entindex(), eSound);
		controller.Play(m_projectiletravel_sound, 1.0f, 100);
	}
}

///// FLETCHETTES

LINK_ENTITY_TO_CLASS(hunter_flechette, CHunterFlechette);

BEGIN_DATADESC(CHunterFlechette)

DEFINE_THINKFUNC(BubbleThink),
DEFINE_THINKFUNC(DangerSoundThink),
DEFINE_THINKFUNC(ExplodeThink),
DEFINE_THINKFUNC(DopplerThink),
DEFINE_THINKFUNC(SeekThink),

DEFINE_ENTITYFUNC(FlechetteTouch),

DEFINE_FIELD(m_vecShootPosition, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_hSeekTarget, FIELD_EHANDLE),
DEFINE_FIELD(m_bThrownBack, FIELD_BOOLEAN),

END_DATADESC()

CHunterFlechette *CHunterFlechette::FlechetteCreate(const Vector &vecOrigin, const QAngle &angAngles, CBaseEntity *pentOwner)
{
	// Create a new entity with CHunterFlechette private data
	CHunterFlechette *pFlechette = (CHunterFlechette *)CreateEntityByName("hunter_flechette");
	UTIL_SetOrigin(pFlechette, vecOrigin);
	pFlechette->SetAbsAngles(angAngles);
	pFlechette->Spawn();
	pFlechette->Activate();
	pFlechette->SetOwnerEntity(pentOwner);

	return pFlechette;
}

void CC_Hunter_Shoot_Flechette(const CCommand& args)
{
	MDLCACHE_CRITICAL_SECTION();

	bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache(true);

	CBasePlayer *pPlayer = UTIL_GetCommandClient();

	QAngle angEye = pPlayer->EyeAngles();
	CHunterFlechette *entity = CHunterFlechette::FlechetteCreate(pPlayer->EyePosition(), angEye, pPlayer);
	if (entity)
	{
		entity->Precache();
		DispatchSpawn(entity);

		// Shoot the flechette.		
		Vector forward;
		pPlayer->EyeVectors(&forward);
		Vector velocity = forward * FLECHETTE_AIR_VELOCITY;
		entity->Shoot(velocity, false);
	}

	CBaseEntity::SetAllowPrecache(allowPrecache);
}

static ConCommand ent_create("hunter_shoot_flechette", CC_Hunter_Shoot_Flechette, "Fires a hunter flechette where the player is looking.", FCVAR_GAMEDLL);

CHunterFlechette::CHunterFlechette()
{
	UseClientSideAnimation();
}

CHunterFlechette::~CHunterFlechette()
{
}

void CHunterFlechette::SetSeekTarget(CBaseEntity *pTargetEntity)
{
	if (pTargetEntity)
	{
		m_hSeekTarget = pTargetEntity;
		SetContextThink(&CHunterFlechette::SeekThink, CURTIME, s_szHunterFlechetteSeekThink);
	}
}

bool CHunterFlechette::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal(SOLID_BBOX, FSOLID_NOT_STANDABLE, false);

	return true;
}

unsigned int CHunterFlechette::PhysicsSolidMaskForEntity() const
{
	return (BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX) & ~CONTENTS_GRATE;
}

void CHunterFlechette::OnParentCollisionInteraction(parentCollisionInteraction_t eType, int index, gamevcollisionevent_t *pEvent)
{
	if (eType == COLLISIONINTER_PARENT_FIRST_IMPACT)
	{
		m_bThrownBack = true;
		Explode();
	}
}

void CHunterFlechette::OnParentPhysGunDrop(CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason)
{
	m_bThrownBack = true;
}

bool CHunterFlechette::CreateSprites(bool bBright)
{
	if (bBright)
	{
		DispatchParticleEffect("hunter_flechette_trail_striderbuster", PATTACH_ABSORIGIN_FOLLOW, this);
	}
	else
	{
		DispatchParticleEffect("hunter_flechette_trail", PATTACH_ABSORIGIN_FOLLOW, this);
	}

	return true;
}

void CHunterFlechette::Spawn()
{
	Precache();

	SetModel(HUNTER_FLECHETTE_MODEL);
	SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM);
	UTIL_SetSize(this, -Vector(1, 1, 1), Vector(1, 1, 1));
	SetSolid(SOLID_BBOX);
	SetGravity(0.05f);
	SetCollisionGroup(COLLISION_GROUP_PROJECTILE);

	// Make sure we're updated if we're underwater
	UpdateWaterState();

	SetTouch(&CHunterFlechette::FlechetteTouch);

	// Make us glow until we've hit the wall
	m_nSkin = 1;
}

void CHunterFlechette::Activate()
{
	BaseClass::Activate();
	SetupGlobalModelData();
}

void CHunterFlechette::SetupGlobalModelData()
{
	if (s_nHunterFlechetteImpact == -2)
	{
		s_nHunterFlechetteImpact = LookupSequence("impact");
		s_nFlechetteFuseAttach = LookupAttachment("attach_fuse");
	}
}

void CHunterFlechette::Precache()
{
	PrecacheModel(HUNTER_FLECHETTE_MODEL);
	PrecacheModel("sprites/light_glow02_noz.vmt");

	PrecacheScriptSound("NPC_Hunter.FlechetteNearmiss");
	PrecacheScriptSound("NPC_Hunter.FlechetteHitBody");
	PrecacheScriptSound("NPC_Hunter.FlechetteHitWorld");
	PrecacheScriptSound("NPC_Hunter.FlechettePreExplode");
	PrecacheScriptSound("NPC_Hunter.FlechetteExplode");

	PrecacheParticleSystem("hunter_flechette_trail_striderbuster");
	PrecacheParticleSystem("hunter_flechette_trail");
	PrecacheParticleSystem("hunter_projectile_explosion_1");
}

void CHunterFlechette::StickTo(CBaseEntity *pOther, trace_t &tr)
{
	EmitSound("NPC_Hunter.FlechetteHitWorld");

	SetMoveType(MOVETYPE_NONE);

	if (!pOther->IsWorld())
	{
		SetParent(pOther);
		SetSolid(SOLID_NONE);
		SetSolidFlags(FSOLID_NOT_SOLID);
	}

	Vector vecVelocity = GetAbsVelocity();

	SetTouch(NULL);

	// We're no longer flying. Stop checking for water volumes.
	SetContextThink(NULL, 0, s_szHunterFlechetteBubbles);

	// Stop seeking.
	m_hSeekTarget = NULL;
	SetContextThink(NULL, 0, s_szHunterFlechetteSeekThink);

	DangerSoundThink();

	// Play our impact animation.
	ResetSequence(s_nHunterFlechetteImpact);

	static int s_nImpactCount = 0;
	s_nImpactCount++;
	if (s_nImpactCount & 0x01)
	{
		UTIL_ImpactTrace(&tr, DMG_BULLET);

		// Shoot some sparks
		if (UTIL_PointContents(GetAbsOrigin()) != CONTENTS_WATER)
		{
			g_pEffects->Sparks(GetAbsOrigin());
		}
	}
}

void CHunterFlechette::FlechetteTouch(CBaseEntity *pOther)
{
	if (pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER))
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ((pOther->m_takedamage == DAMAGE_NO) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY))
			return;
	}

	if (FClassnameIs(pOther, "hunter_flechette"))
		return;

	trace_t	tr;
	tr = BaseClass::GetTouchTrace();

	if (pOther->m_takedamage != DAMAGE_NO)
	{
		Vector	vecNormalizedVel = GetAbsVelocity();

		ClearMultiDamage();
		VectorNormalize(vecNormalizedVel);

		float flDamage = 4;

		CTakeDamageInfo	dmgInfo(this, GetOwnerEntity(), flDamage, DMG_DISSOLVE | DMG_NEVERGIB);
		CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
		dmgInfo.SetDamagePosition(tr.endpos);
		pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);

		ApplyMultiDamage();

		// Keep going through breakable glass.
		if (pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS)
			return;

		SetAbsVelocity(Vector(0, 0, 0));

		// play body "thwack" sound
		EmitSound("NPC_Hunter.FlechetteHitBody");

		StopParticleEffects(this);

		Vector vForward;
		AngleVectors(GetAbsAngles(), &vForward);
		VectorNormalize(vForward);

		trace_t	tr2;
		UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + vForward * 128, MASK_BLOCKLOS, pOther, COLLISION_GROUP_NONE, &tr2);

		if (tr2.fraction != 1.0f)
		{
			if (tr2.m_pEnt == NULL || (tr2.m_pEnt && tr2.m_pEnt->GetMoveType() == MOVETYPE_NONE))
			{
				CEffectData	data;

				data.m_vOrigin = tr2.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = tr2.fraction != 1.0f;
			}
		}

		if (((pOther->GetMoveType() == MOVETYPE_VPHYSICS) || (pOther->GetMoveType() == MOVETYPE_PUSH)) && ((pOther->GetHealth() > 0) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY)))
		{
			CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(pOther);
			if (pProp)
			{
				pProp->SetInteraction(PROPINTER_PHYSGUN_NOTIFY_CHILDREN);
			}

			// We hit a physics object that survived the impact. Stick to it.
			StickTo(pOther, tr);
		}
		else
		{
			SetTouch(NULL);
			SetThink(NULL);
			SetContextThink(NULL, 0, s_szHunterFlechetteBubbles);

			UTIL_Remove(this);
		}
	}
	else
	{
		// See if we struck the world
		if (pOther->GetMoveType() == MOVETYPE_NONE && !(tr.surface.flags & SURF_SKY))
		{
			// We hit a physics object that survived the impact. Stick to it.
			StickTo(pOther, tr);
		}
		else if (pOther->GetMoveType() == MOVETYPE_PUSH && FClassnameIs(pOther, "func_breakable"))
		{
			// We hit a func_breakable, stick to it.
			// The MOVETYPE_PUSH is a micro-optimization to cut down on the classname checks.
			StickTo(pOther, tr);
		}
		else
		{
			// Put a mark unless we've hit the sky
			if ((tr.surface.flags & SURF_SKY) == false)
			{
				UTIL_ImpactTrace(&tr, DMG_BULLET);
			}

			UTIL_Remove(this);
		}
	}
}

void CHunterFlechette::SeekThink()
{
	if (m_hSeekTarget)
	{
		Vector vecBodyTarget = m_hSeekTarget->BodyTarget(GetAbsOrigin());

		Vector vecClosest;
		CalcClosestPointOnLineSegment(GetAbsOrigin(), m_vecShootPosition, vecBodyTarget, vecClosest, NULL);

		Vector vecDelta = vecBodyTarget - m_vecShootPosition;
		VectorNormalize(vecDelta);

		QAngle angShoot;
		VectorAngles(vecDelta, angShoot);

		float flSpeed = 2000;

		Vector vecVelocity = vecDelta * flSpeed;
		Teleport(&vecClosest, &angShoot, &vecVelocity);

		SetNextThink(CURTIME, s_szHunterFlechetteSeekThink);
	}
}

void CHunterFlechette::DopplerThink()
{
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if (!pPlayer)
		return;

	Vector vecVelocity = GetAbsVelocity();
	VectorNormalize(vecVelocity);

	float flMyDot = DotProduct(vecVelocity, GetAbsOrigin());
	float flPlayerDot = DotProduct(vecVelocity, pPlayer->GetAbsOrigin());

	if (flPlayerDot <= flMyDot)
	{
		EmitSound("NPC_Hunter.FlechetteNearMiss");

		// We've played the near miss sound and we're not seeking. Stop thinking.
		SetThink(NULL);
	}
	else
	{
		SetNextThink(CURTIME);
	}
}

void CHunterFlechette::BubbleThink()
{
	SetNextThink(CURTIME + 0.1f, s_szHunterFlechetteBubbles);

	if (GetWaterLevel() == 0)
		return;

	UTIL_BubbleTrail(GetAbsOrigin() - GetAbsVelocity() * 0.1f, GetAbsOrigin(), 5);
}

void CHunterFlechette::Shoot(Vector &vecVelocity, bool bBrightFX)
{
	CreateSprites(bBrightFX);

	m_vecShootPosition = GetAbsOrigin();

	SetAbsVelocity(vecVelocity);

	SetThink(&CHunterFlechette::DopplerThink);
	SetNextThink(CURTIME);

	SetContextThink(&CHunterFlechette::BubbleThink, CURTIME + 0.1, s_szHunterFlechetteBubbles);
}

void CHunterFlechette::DangerSoundThink()
{
	EmitSound("NPC_Hunter.FlechettePreExplode");

	CSoundEnt::InsertSound(SOUND_DANGER | SOUND_CONTEXT_EXCLUDE_COMBINE, GetAbsOrigin(), 150.0f, 0.5, this);
	SetThink(&CHunterFlechette::ExplodeThink);
	SetNextThink(CURTIME + 1.0f);
}

void CHunterFlechette::ExplodeThink()
{
	Explode();
}

void CHunterFlechette::Explode()
{
	SetSolid(SOLID_NONE);

	// Don't catch self in own explosion!
	m_takedamage = DAMAGE_NO;

	EmitSound("NPC_Hunter.FlechetteExplode");

	// Move the explosion effect to the tip to reduce intersection with the world.
	Vector vecFuse;
	GetAttachment(s_nFlechetteFuseAttach, vecFuse);
	DispatchParticleEffect("hunter_projectile_explosion_1", vecFuse, GetAbsAngles(), NULL);

	int nDamageType = DMG_DISSOLVE;

	// Perf optimization - only every other explosion makes a physics force. This is
	// hardly noticeable since flechettes usually explode in clumps.
	static int s_nExplosionCount = 0;
	s_nExplosionCount++;

	RadiusDamage(CTakeDamageInfo(this, GetOwnerEntity(), 12, nDamageType), GetAbsOrigin(), 3, CLASS_NONE, NULL);

	AddEffects(EF_NODRAW);

	SetThink(&CBaseEntity::SUB_Remove);
	SetNextThink(CURTIME + 0.1f);
}

///// FLETCHETTES

// sheet simulator test
//-----------------------------------------------------------------------------
// Purpose: Shield
//-----------------------------------------------------------------------------
class CTestSheetEntity : public CBaseAnimating
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS(CTestSheetEntity, CBaseAnimating);
#if 1
	DECLARE_SERVERCLASS();
#endif
public:
	void Spawn(void);
	void Precache(void);

public:
	static CTestSheetEntity* Create(CBaseEntity *pentOwner);
};
#if 1
//EXTERN_SEND_TABLE(DT_BaseAnimating)

IMPLEMENT_SERVERCLASS_ST(CTestSheetEntity, DT_TestSheetEntity)
END_SEND_TABLE()
#endif
LINK_ENTITY_TO_CLASS(prop_sheet, CTestSheetEntity);

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC(CTestSheetEntity)
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTestSheetEntity::Precache(void)
{
	PrecacheModel("models/props_junk/cardboard_box001a.mdl");
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTestSheetEntity::Spawn(void)
{
	Precache();

	SetModel("models/props_junk/cardboard_box001a.mdl");
	SetSolid(SOLID_NONE);
	SetMoveType(MOVETYPE_NONE);
//	UTIL_SetSize(this, -Vector(32,32,32), Vector(32,32,32));

	BaseClass::Spawn();
	
	// Think function
	SetNextThink(CURTIME + 0.1f);
}

//-----------------------------------------------------------------------------
// Purpose: Create an energy wave
//-----------------------------------------------------------------------------
CTestSheetEntity* CTestSheetEntity::Create(CBaseEntity *pentOwner)
{
	CTestSheetEntity *pEWave = (CTestSheetEntity*)CreateEntityByName("prop_sheet");

	UTIL_SetOrigin(pEWave, pentOwner->GetLocalOrigin());
	pEWave->SetOwnerEntity(pentOwner);
	pEWave->SetLocalAngles(pentOwner->GetLocalAngles());
	pEWave->Spawn();

	return pEWave;
}

void CreatePropSheetCallback()
{
	CBasePlayer *player = UTIL_GetLocalPlayer();
	if (!player) return;

	CTestSheetEntity *pEWave = (CTestSheetEntity*)CreateEntityByName("prop_sheet");
	UTIL_SetOrigin(pEWave, player->WorldSpaceCenter());
	pEWave->SetOwnerEntity(player);
	pEWave->SetLocalAngles(player->GetLocalAngles());
	pEWave->Spawn();
}

ConCommand cc_CreatePropSheet("CreateSheet", CreatePropSheetCallback, 0, FCVAR_NONE);