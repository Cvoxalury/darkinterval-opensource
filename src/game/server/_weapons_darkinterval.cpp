// Includes weapons exclusive to Dark Interval and
// Half-Life 2 weapons that were modified for the mod in some way.

#include "cbase.h"
#include "_sharedents_encompass.h"
#include "_weapons_darkinterval.h"
#include "ai_basenpc.h"
#include "ammodef.h"
#include "basegrenade_shared.h"
#include "beam_shared.h"
#include "decals.h"
#include "env_explosion.h"
#include "env_spritetrail.h"
#include "fire.h"
#include "func_break.h"
#include "gamerules.h"
#include "gamestats.h"
#include "globalstate.h"
#include "grenade_ar2.h"
#include "grenade_frag.h"
#include "hl2_player.h"
#include "hl2_shareddefs.h"
#include "ieffects.h"
#include "in_buttons.h"
#include "func_brush.h"
#include "model_types.h"
#include "npcevent.h"
#include "env_particlesmokegrenade.h"
#include "particle_system.h"
#include "prop_combine_ball.h"
#include "rumble_shared.h"
#include "shot_manipulator.h"
#include "soundent.h"
#include "soundenvelope.h"
#include "sprite.h"
#include "smoke_trail.h"
#include "te_effect_dispatch.h"
#include "world.h"

#include "tier0\memdbgon.h"

// extern convars - may be shared between several wpns
extern ConVar sk_plr_dmg_smg1_grenade;
extern ConVar sk_plr_dmg_tau;
extern ConVar sk_auto_reload_time;
extern ConVar sk_plr_num_shotgun_pellets;
extern ConVar sk_auto_reload_enabled;
#ifdef DARKINTERVAL // add a skill convar controlling altire number of pellets
extern ConVar sk_plr_num_shotgun_pellets_doublefire;
#endif

// indeces and such
extern short g_sModelIndexFireball; // regular explosion
extern short g_sModelIndexWExplosion; // underwater explosion

#ifdef DARKINTERVAL
#define OICW
#define TAU
#define IMMOLATOR
#define SHOTGUN_VIRT
#define SMG1_VIRT
#define SNIPER_ELITE
#define PLASMA_RIFLE
#define SLAM_NEW
#endif
#define PISTOL
#define REVOLVER
#define SMG1
#define SHOTGUN
#define FLARE
#define FLAREGUN
#define CROSSBOW
#define FRAG_GRENADE

// Shared Projectile Types
class SmokeTrail;
#define GRENADE_MAX_DANGER_RADIUS	300
extern ConVar sk_plr_dmg_oicw_alt;
extern ConVar sk_npc_dmg_oicw_alt;
ConVar weapon_oicw_cluster_size("weapon_oicw_cluster_size", "5");
ConVar weapon_oicw_cluster_flytime("weapon_oicw_cluster_flytime", "1.0f");
//ConVar weapon_oicw_single_grenade_dmg("weapon_oicw_single_grenade_smg", "50");
//ConVar weapon_oicw_single_grenade_radius("weapon_oicw_single_grenade_radius", "250");
ConVar weapon_oicw_cluster_grenade_dmg("weapon_oicw_cluster_grenade_smg", "40");
ConVar weapon_oicw_cluster_grenade_radius("weapon_oicw_cluster_grenade_radius", "300");
class CClusterGrenade : public CBaseGrenade
{
public:
	DECLARE_CLASS(CClusterGrenade, CBaseGrenade);
	DECLARE_DATADESC();

	CClusterGrenade(void)
	{
		m_smoke_trail_handle = NULL;
	}
	CHandle< SmokeTrail >	m_smoke_trail_handle;
	float	m_spawn_time;
	float	m_danger_radius_float;
	int		m_touch_count_int;
	Vector  m_impact_dir_vec3;
	bool	m_player_launched_boolean;

	void	Precache(void)
	{
		PrecacheModel("models/Items/ar2_grenade.mdl");
		PrecacheModel("models/_Weapons/oicw_grenade.mdl");
		PrecacheParticleSystem("smoke_trail");
	}

	void	Spawn(void)
	{
		Precache();
		SetSolid(SOLID_BBOX);
		SetMoveType(MOVETYPE_FLYGRAVITY);

		// Hits everything but debris
		SetCollisionGroup(COLLISION_GROUP_PROJECTILE);

		SetModel("models/Items/ar2_grenade.mdl");
		UTIL_SetSize(this, Vector(-4, -4, -4), Vector(4, 4, 4));
		SetCollisionBounds(Vector(-4, -4, -4), Vector(4, 4, 4));

		SetUse(&CClusterGrenade::DetonateUse);
		SetTouch(&CClusterGrenade::ClusterGrenadeTouch);
		SetThink(&CClusterGrenade::ClusterGrenadeThink);
		SetNextThink(CURTIME + 0.1f);

		if (GetOwnerEntity() && GetOwnerEntity()->IsPlayer())
		{
			m_flDamage = sk_plr_dmg_oicw_alt.GetFloat();
		}
		else
		{
			m_flDamage = sk_npc_dmg_oicw_alt.GetFloat();
		}

		m_DmgRadius = 100;
		m_takedamage = DAMAGE_YES;
		m_bIsLive = false;
		m_touch_count_int = 0;
		m_iHealth = 1;
		m_impact_dir_vec3 = Vector(0, 0, 0);

		SetGravity(UTIL_ScaleForGravity(200));	// use a lower gravity for grenades to make them easier to see
		SetFriction(0.8);
		SetSequence(0);

		m_danger_radius_float = 100;

		m_spawn_time = CURTIME;

		// -------------
		// Smoke trail.
		// -------------
	//	m_smoke_trail_handle = SmokeTrail::CreateSmokeTrail();

		if (m_smoke_trail_handle)
		{
			m_smoke_trail_handle->m_SpawnRate = 48;
			m_smoke_trail_handle->m_ParticleLifetime = 1;
			m_smoke_trail_handle->m_StartColor.Init(0.1f, 0.1f, 0.1f);
			m_smoke_trail_handle->m_EndColor.Init(0, 0, 0);
			m_smoke_trail_handle->m_StartSize = 12;
			m_smoke_trail_handle->m_EndSize = m_smoke_trail_handle->m_StartSize * 4;
			m_smoke_trail_handle->m_SpawnRadius = 4;
			m_smoke_trail_handle->m_MinSpeed = 4;
			m_smoke_trail_handle->m_MaxSpeed = 24;
			m_smoke_trail_handle->m_Opacity = 0.2f;

			m_smoke_trail_handle->SetLifetime(10.0f);
			m_smoke_trail_handle->FollowEntity(this);
		}
	}

	void	ClusterGrenadeTouch(CBaseEntity *pOther)
	{
		Assert(pOther);
		if (!pOther->IsSolid())
			return;

		// If I'm not live, only blow up if I'm hitting an character that
		// is not the owner of the weapon
		Vector fixedOrigin = GetAbsOrigin();
		CBaseCombatCharacter *pBCC = ToBaseCombatCharacter(pOther);
		if (pBCC && GetThrower() != pBCC)
		{
			//	m_bIsLive = true;
			//	Detonate();
			CPASFilter filter(fixedOrigin);
			te->Explosion(filter, 0.0,
				&fixedOrigin,
				g_sModelIndexFireball,
				2.0,
				15,
				TE_EXPLFLAG_NONE, //TODO: Consider explosion w/o visuals, use particles in DetonationFX()?
				m_DmgRadius,
				m_flDamage);

			UTIL_ScreenShake(GetAbsOrigin(), m_flDamage * 2.0, 150.0, 1.0, m_DmgRadius * 3, SHAKE_START);
			RadiusDamage(CTakeDamageInfo(this, GetThrower(), m_flDamage * weapon_oicw_cluster_size.GetInt(), DMG_BLAST), fixedOrigin, m_DmgRadius, CLASS_NONE, NULL);

			UTIL_Remove(this);
		}
		else
		{
			m_bIsLive = true;
			Detonate();
		}
	}

	void	ClusterGrenadeThink(void)
	{
		SetNextThink(CURTIME + 0.05f);

		DispatchParticleEffect("smoke_trail", GetAbsOrigin(), GetAbsAngles(), this);

		if (CURTIME > m_spawn_time + weapon_oicw_cluster_flytime.GetFloat())
		{
			m_bIsLive = true;
			Detonate();
			return;
		}
		else
		{
			if (m_danger_radius_float <= GRENADE_MAX_DANGER_RADIUS)
			{
				m_danger_radius_float += (GRENADE_MAX_DANGER_RADIUS * 0.05);
			}
		}

		if (GetAbsVelocity().Length() < 10)
		{
			m_bIsLive = true;
			Detonate();
			return;
		}
		CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * 0.5, m_danger_radius_float, 0.2, this, SOUNDENT_CHANNEL_REPEATED_DANGER);
	}

	void	Event_Killed(const CTakeDamageInfo &info)
	{
		Detonate();
	}

	void EXPORT	Detonate(void)
	{
		if (!m_bIsLive)
		{
			return;
		}

		m_bIsLive = false;
		m_takedamage = DAMAGE_NO;

		if (m_smoke_trail_handle)
		{
			UTIL_Remove(m_smoke_trail_handle);
			m_smoke_trail_handle = NULL;
		}
				
		Vector vecThrow, vecSrc;
	//	RandomVectorInUnitSphere(&vecThrow);
	//	vecThrow *= RandomFloat(300, 330);
		vecSrc = GetAbsOrigin() + Vector(0, 0, 4);

		for (int g = 0; g < weapon_oicw_cluster_size.GetInt(); g++)
		{
			CSharedProjectile *pMiniBomb = (CSharedProjectile*)CreateEntityByName("shared_projectile");
			pMiniBomb->SetAbsOrigin(vecSrc );
			pMiniBomb->SetAbsAngles(vec3_angle);
			DispatchSpawn(pMiniBomb);

			pMiniBomb->SetThrower(m_player_launched_boolean ? UTIL_GetLocalPlayer() : ToBasePlayer(GetOwnerEntity()));
			pMiniBomb->SetOwnerEntity(GetOwnerEntity());

			pMiniBomb->SetProjectileStats(Projectile_SMALL, Projectile_BLAST,
				Projectile_BOUNCEDELAY, m_flDamage, m_DmgRadius * 3, 400, 128.0f, -RandomFloat(0.8, 1.0));

			Vector miniBombToss = Vector(0, 0, 200);

			// Begin angles at 0 (i == 0 -> n == 0),
			// increment by 60, 
			// then convert to radiants
			float n = 72;
			n *= g;
			n *= M_PI;
			n /= 180;

			// Now caulculate the directional forces from the
			// angle (radiant value). We want the minibombs
			// to drop out in a circle, 60 degrees apart, 
			// so we calculate x/y coords of the forces' vectors
			// by the usual formula to find points on a circle.
			// 250-300 is therefore the radius of that circle,
			// how far we want to push the minibombs.
			miniBombToss.x = m_DmgRadius * cos(n);
			miniBombToss.y = m_DmgRadius * sin(n);
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
			/*
			CSharedProjectile *pGrenade = (CSharedProjectile*)CreateEntityByName("shared_projectile");
			pGrenade->SetAbsOrigin(vecSrc);
			pGrenade->SetAbsAngles(vec3_angle);
			DispatchSpawn(pGrenade);
			pGrenade->SetThrower(m_player_launched_boolean ? UTIL_GetLocalPlayer() : ToBasePlayer(GetOwnerEntity()));
			pGrenade->SetOwnerEntity(GetOwnerEntity());
			UTIL_SetSize(pGrenade, -Vector(.5, .5, .5), Vector(.5, .5, .5));

			pGrenade->SetProjectileStats(Projectile_LARGE, Projectile_BLAST, Projectile_BOUNCEDELAY,
				m_flDamage, m_DmgRadius, 300, 128.0, RandomFloat(1.2, 1.8));
			pGrenade->SetAbsVelocity(vecThrow);

			pGrenade->CreateProjectileEffects();
			pGrenade->SetModel("models/_Weapons/oicw_grenade.mdl");
			pGrenade->SetModelScale(1.0f);

			// Tumble through the air
			pGrenade->SetLocalAngularVelocity(RandomAngle(-500, 500));
			*/
		}
		UTIL_ScreenShake(GetAbsOrigin(), 5.0, 150.0, 0.5, 250, SHAKE_START);

		RadiusDamage(CTakeDamageInfo(this, GetThrower(), 5, DMG_GENERIC), GetAbsOrigin(), 32, CLASS_NONE, NULL);

		UTIL_Remove(this);
	}
};

BEGIN_DATADESC(CClusterGrenade)
DEFINE_FIELD(m_smoke_trail_handle, FIELD_EHANDLE),
DEFINE_FIELD(m_spawn_time, FIELD_TIME),
DEFINE_FIELD(m_danger_radius_float, FIELD_FLOAT),
DEFINE_FIELD(m_touch_count_int, FIELD_INTEGER),
DEFINE_FIELD(m_impact_dir_vec3, FIELD_VECTOR),
DEFINE_FIELD(m_player_launched_boolean, FIELD_BOOLEAN),
DEFINE_ENTITYFUNC(ClusterGrenadeTouch),
DEFINE_THINKFUNC(ClusterGrenadeThink),
END_DATADESC()

LINK_ENTITY_TO_CLASS(cluster_grenade, CClusterGrenade);

//====================================
// OICW (Dark Interval)
//====================================
#if defined(OICW)
static const char	*ALTFIRE_GLOW_SPRITE = "sprites/redglow4.vmt";
static const char	*ALTFIRE_GLOW_SPRITE2 = "sprites/blueflare1.vmt";
static ConVar sk_oicw_physblast_mag("sk_oicw_physblast_mag", "2");
static ConVar sk_oicw_physblast_rad("sk_oicw_physblast_rad", "64");
class CWeaponOICW : public CHLMachineGun
{
public:
	DECLARE_CLASS(CWeaponOICW, CHLMachineGun);
	DECLARE_SERVERCLASS();

public:
	DECLARE_ACTTABLE();
	DECLARE_DATADESC();

	Vector	m_toss_velocity_vec3;
	float	m_next_grenade_check_float;
	float	m_altfire_charge_amount_float;

private:
	enum AltfireState_t
	{
		ALTFIRE_STATE_START_LOAD,
		ALTFIRE_STATE_START_CHARGE,
		ALTFIRE_STATE_READY,
		ALTFIRE_STATE_DISCHARGE,
		ALTFIRE_STATE_OFF,
	};

	// Charger effects
	AltfireState_t		m_altfire_state_int;
	CHandle<CSprite>	m_altfire_sprite_handle;
	CSoundPatch			*m_altfire_sound_patch;

private:
	float	m_soonest_primary_attack_time;
	float	m_last_attack_time;
	float	m_accuracy_penalty_float;
//	int		m_num_shots_fired_int;

public:
	CWeaponOICW()
	{
		m_fMinRange1 = 65;
		m_fMaxRange1 = 2048;

		m_fMinRange2 = 256;
		m_fMaxRange2 = 1024;

		m_shots_fired_int = 0;
		m_last_attack_time = 0;
		m_accuracy_penalty_float = 0;

		m_can_altfire_underwater_boolean = true;

		m_altfire_charge_amount_float = 0.0f;

	}
	~CWeaponOICW()
	{
		m_altfire_charge_amount_float = 0.0f;

		if (m_altfire_sound_patch != NULL)
		{
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			controller.SoundDestroy(m_altfire_sound_patch);
			m_altfire_sound_patch = NULL;
		}
	}
	//	bool	ShouldDisplayAltFireHUDHint() { return true; }
	int		GetMinBurst(void) { return 3; }
	int		GetMaxBurst(void) { return 5; }
	float	GetMinRestTime() { return 0.8; }
	float	GetMaxRestTime() { return 1.8; }
	float	GetFireRate(void) { return 0.1f; }
	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	//	const char *GetTracerType(void) { return "AR2Tracer"; }
	virtual const Vector& GetBulletSpread(void) { static Vector cone; cone = VECTOR_CONE_1DEGREES; return cone; }
	//-----------------------------------------------------------------------------------------------------------------	
	bool	Reload(void) { return BaseClass::Reload(); }
	//-----------------------------------------------------------------------------------------------------------------	
	bool	CanHolster(void) { return BaseClass::CanHolster(); }
	//-----------------------------------------------------------------------------------------------------------------	
	void	Precache(void)
	{
		UTIL_PrecacheOther("cluster_grenade");
		UTIL_PrecacheOther("shared_projectile");
		PrecacheModel(ALTFIRE_GLOW_SPRITE);
		PrecacheModel(ALTFIRE_GLOW_SPRITE2);
		PrecacheModel("models/_Weapons/oicw_ammo_grenade.mdl");
		BaseClass::Precache();
	}
	//-----------------------------------------------------------------------------------------------------------------
	void UpdatePenaltyTime(void)
	{
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());

		if (pOwner == NULL)
			return;

		// Check our penalty time decay
		if (((pOwner->m_nButtons & IN_ATTACK) == false) && (m_soonest_primary_attack_time < CURTIME))
		{
			m_accuracy_penalty_float -= gpGlobals->frametime;
			m_accuracy_penalty_float = clamp(m_accuracy_penalty_float, 0.0f, 4.0f);
		}
	}
	//-----------------------------------------------------------------------------------------------------------------	

	void ItemPreFrame(void)
	{
		UpdatePenaltyTime();

		BaseClass::ItemPreFrame();
	}

	void ItemBusyFrame(void)
	{
		UpdatePenaltyTime();

		BaseClass::ItemBusyFrame();
	}
	void	ItemPostFrame(void)
	{
		CBasePlayer *player = ToBasePlayer(GetOwner());
		if (!player) return;

		if (player->m_nButtons & IN_ATTACK2)
		{
			if (m_flNextSecondaryAttack > CURTIME) return;

			SecondaryAttack();

			if ((player->GetAmmoCount(m_iSecondaryAmmoType) > 0))
			{
				m_altfire_charge_amount_float += 0.04f;

				if (m_altfire_charge_amount_float > 0.01f && m_altfire_state_int < ALTFIRE_STATE_START_CHARGE)
				{
					SetAltfireState(ALTFIRE_STATE_START_CHARGE);
				}

				if (m_altfire_charge_amount_float > 1.00f)
				{
					if (m_altfire_state_int != ALTFIRE_STATE_READY)
					{
						SetAltfireState(ALTFIRE_STATE_READY);
						WeaponSound(SPECIAL2);
					}

					m_altfire_charge_amount_float = 1.00f;
				}
			}
		}
		else if (player->m_afButtonReleased & IN_ATTACK2)
		{
			if (m_altfire_sound_patch != NULL)
				(CSoundEnvelopeController::GetController()).SoundFadeOut(m_altfire_sound_patch, 0.1f);
			ShootAltfire();
			m_altfire_charge_amount_float = 0.0f;
		}
		else
		{
			if (m_altfire_sound_patch != NULL)
				(CSoundEnvelopeController::GetController()).SoundFadeOut(m_altfire_sound_patch, 0.1f);
		}

		BaseClass::ItemPostFrame();
	}
	//-----------------------------------------------------------------------------------------------------------------
	// OICW overrides FireBullets() for its penetration effects.
	void FireBullets(const FireBulletsInfo_t &info)
	{
		if (CBasePlayer *player = ToBasePlayer(GetOwner()))
		{
			FireBulletsInfo_t newInfo = info;
			if (m_shots_fired_int < 3)
				newInfo.m_vecSpread = VECTOR_CONE_1DEGREES;
			else newInfo.m_vecSpread = GetBulletSpread() * (1 + m_accuracy_penalty_float);
			newInfo.m_iShots = 1;
			player->FireBullets(newInfo);
			
			// Penetrate thin geometry
			//Shoot a shot straight out
			bool penetrated = false;

			CShotManipulator Manipulator(info.m_vecDirShooting);

			Vector vecDir = newInfo.m_vecDirShooting;

			vecDir = Manipulator.ApplySpread(newInfo.m_vecSpread);

			trace_t	tr;
			UTIL_TraceLine(newInfo.m_vecSrc, newInfo.m_vecSrc + vecDir * 4096, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
			
			ClearMultiDamage();
			
			//Look for wall penetration
			if (tr.DidHitWorld() && !(tr.surface.flags & SURF_SKY))
			{
				//Try wall penetration
				UTIL_ImpactTrace(&tr, DMG_BULLET);
				
				Vector	testPos = tr.endpos + (vecDir * 28.0f);

				UTIL_TraceLine(testPos, tr.endpos, MASK_SHOT, player, COLLISION_GROUP_NONE, &tr);

				if (tr.allsolid == false)
				{
					penetrated = true;
					UTIL_ImpactTrace(&tr, DMG_BULLET); // create exit bullet hole
				}
			}

			if (penetrated)
			{
				// tr.endpos here is the endpos from UTIL_TraceLine above which tells us
				// if it was tr.allsolid or not - this endpos is from the 
				// other side of the penetrated brush in case of penetration
				player->FireBullets(1, tr.endpos, vecDir, vec3_origin, 512, m_iPrimaryAmmoType, 0);
			}
			
			// Send the railgun effect
		//	DispatchParticleEffect("Weapon_Combine_Ion_Cannon", player->Weapon_TrueShootPosition_Front(), tr.endpos, vec3_angle, NULL);

			ApplyMultiDamage();
		}
	}
	//-----------------------------------------------------------------------------------------------------------------
	bool Holster(CBaseCombatWeapon *pSwitchingTo)
	{
		if (GetOwner() && GetOwner()->IsPlayer())
		{
			if (m_altfire_charge_amount_float > 0.00f) m_altfire_charge_amount_float = 0;
			SetAltfireState(ALTFIRE_STATE_OFF);
			if (m_altfire_sound_patch != NULL)
				(CSoundEnvelopeController::GetController()).SoundFadeOut(m_altfire_sound_patch, 0.1f);
		}
		return BaseClass::Holster(pSwitchingTo);
	}
	//-----------------------------------------------------------------------------------------------------------------	
	void PrimaryAttack(void)
	{
	//	if ((CURTIME - m_last_attack_time) > 1.0f)
	//	{
	//		m_num_shots_fired_int = 0;
	//	}
	//	else
	//	{
	//		m_num_shots_fired_int++;
	//	}

		m_last_attack_time = CURTIME;
		m_soonest_primary_attack_time = CURTIME + GetFireRate();
		CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 2000, 0.2, GetOwner());

		CBasePlayer *pOwner = ToBasePlayer(GetOwner());

		if (pOwner)
		{
			// Reset the view punch on each shot
			pOwner->ViewPunchReset();
		}

		BaseClass::PrimaryAttack();

		// Add an accuracy penalty which can move past our maximum penalty time if we're really spastic
		m_accuracy_penalty_float += 0.1f;

		DoMuzzlePhysExplosion(sk_oicw_physblast_mag.GetFloat(), sk_oicw_physblast_rad.GetFloat());

		m_iPrimaryAttacks++;
		gamestats->Event_WeaponFired(pOwner, true, GetClassname());
	}
	void	SecondaryAttack(void)
	{
		CBasePlayer *player = ToBasePlayer(GetOwner());
		if (player == NULL) return;

		if (m_flNextSecondaryAttack > CURTIME) return;

		//Must have ammo
		if ((player->GetAmmoCount(m_iSecondaryAmmoType) <= 0))
		{
			SendWeaponAnim(ACT_VM_DRYFIRE);
			BaseClass::WeaponSound(EMPTY);
			m_flNextSecondaryAttack = CURTIME + 1.5f;
			return;
		}

		if (m_altfire_charge_amount_float == 0.0f && m_altfire_state_int == ALTFIRE_STATE_START_LOAD)
		{
			return;
		}
	}
	void	ShootAltfire(void)
	{
		// Only the player fires this way so we can cast
		CBasePlayer *player = ToBasePlayer(GetOwner());
		if (player == NULL) return;

		//Must have ammo
		if ((player->GetAmmoCount(m_iSecondaryAmmoType) <= 0))
		{
			SendWeaponAnim(ACT_VM_DRYFIRE);
			BaseClass::WeaponSound(EMPTY);
			m_flNextSecondaryAttack = CURTIME + 1.5f;
			return;
		}

		if (m_bInReload) m_bInReload = false;

		// MUST call sound before removing a round from the clip of a CMachineGun
		BaseClass::WeaponSound(WPN_DOUBLE);

		player->RumbleEffect(RUMBLE_357, 0, RUMBLE_FLAGS_NONE);
		
		Vector vecAiming = player->GetAutoaimVector(0);
		Vector vecSrc = player->Weapon_TrueShootPosition_Front();

		QAngle angAiming;
		VectorAngles(vecAiming, angAiming);

		vecAiming *= 3000;
				
		if (m_altfire_state_int == ALTFIRE_STATE_READY)
		{
			CClusterGrenade *pGrenade = (CClusterGrenade*)Create("cluster_grenade", vecSrc, player->EyeAngles(), player);
			pGrenade->SetAbsVelocity(vecAiming);
			pGrenade->SetMoveType(MOVETYPE_FLYGRAVITY);
			pGrenade->SetGravity(UTIL_ScaleForGravity(400));
			pGrenade->SetThrower(GetOwner());
			pGrenade->SetOwnerEntity(GetOwner());
			pGrenade->SetDamage(sk_plr_dmg_oicw_alt.GetInt());
			if (GetOwner()->IsPlayer()) pGrenade->m_player_launched_boolean = true;

			DoMachineGunKick(player, 0.5f, 1.0f, 0.5f, 2.0f);

			// Decrease ammo
			if (!(GetOwner()->GetFlags() & FL_GODMODE))	player->RemoveAmmo(weapon_oicw_cluster_size.GetInt(), m_iSecondaryAmmoType);

		}
		else
		{
			//Create single "blast" projectile
			CSharedProjectile *pGrenade2 = (CSharedProjectile*)CreateEntityByName("shared_projectile");
			pGrenade2->SetAbsOrigin(vecSrc);
			pGrenade2->SetAbsAngles(player->EyeAngles());
			DispatchSpawn(pGrenade2);
			pGrenade2->SetThrower(GetOwner());
			pGrenade2->SetOwnerEntity(GetOwner());

			pGrenade2->SetProjectileStats(
				Projectile_SMALL,
				Projectile_BLAST,
				Projectile_CONTACT,
				sk_plr_dmg_oicw_alt.GetInt(),
				150.0f,
				200, 128.0f, -1.0f ); 
			pGrenade2->SetAbsVelocity(vecAiming);
			pGrenade2->SetModel("models/_Weapons/oicw_ammo_grenade.mdl");
			pGrenade2->CreateProjectileEffects();
			pGrenade2->SetModelScale(0.5);

			DoMachineGunKick(player, 0.5f, 3.0f, 1.0f, 2.0f);

			// Decrease ammo
			if (!(GetOwner()->GetFlags() & FL_GODMODE))	player->RemoveAmmo(1, m_iSecondaryAmmoType);
		}

		SendWeaponAnim(ACT_VM_SECONDARYATTACK);

		CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 1000, 0.2, GetOwner(), SOUNDENT_CHANNEL_WEAPON);

		// player "shoot" animation
		player->SetAnimation(PLAYER_ATTACK1);

		// Can shoot again immediately
		m_flNextPrimaryAttack = CURTIME + 0.5f;

		// Can blow up after a short delay (so have time to release mouse button)
		m_flNextSecondaryAttack = CURTIME + 1.5f;

		// Register a muzzleflash for the AI.
		player->SetMuzzleFlashTime(CURTIME + 0.5);

		m_iSecondaryAttacks++;
		gamestats->Event_WeaponFired(player, false, GetClassname());

		SetAltfireState(ALTFIRE_STATE_DISCHARGE);
	}
	//-----------------------------------------------------------------------------------------------------------------	
	void SetAltfireState(AltfireState_t state)
	{
		// Make sure we're setup
		CreateAltfireEffects();

		// Don't do this twice
		if (state == m_altfire_state_int)
			return;

		m_altfire_state_int = state;

		switch (m_altfire_state_int)
		{
		case ALTFIRE_STATE_START_LOAD:

			WeaponSound(SPECIAL1);

			// Shoot some sparks and draw a beam between the two outer points
			DoAltfireEffect();

			break;

		case ALTFIRE_STATE_START_CHARGE:
		{
			if (m_altfire_sprite_handle == NULL)
				break;

			m_altfire_sprite_handle->SetAsTemporary();
			m_altfire_sprite_handle->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone);
			m_altfire_sprite_handle->SetBrightness(32, 0.5f);
			m_altfire_sprite_handle->SetScale(0.025f, 0.5f);
			m_altfire_sprite_handle->TurnOff();
		}

		break;

		case ALTFIRE_STATE_READY:
		{
			// Get fully charged
			if (m_altfire_sprite_handle == NULL)
				break;

			m_altfire_sprite_handle->SetAsTemporary();
			m_altfire_sprite_handle->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone);
			m_altfire_sprite_handle->SetBrightness(255, 0.0f);
			m_altfire_sprite_handle->SetScale(0.25f, 0.1f);
			m_altfire_sprite_handle->TurnOn();

			DoAltfireEffect();

			WeaponSound(SPECIAL1);
		}

		break;

		case ALTFIRE_STATE_DISCHARGE:
		{
			if (m_altfire_sprite_handle == NULL)
				break;

			m_altfire_sprite_handle->SetAsTemporary();
			m_altfire_sprite_handle->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone);
			m_altfire_sprite_handle->SetBrightness(0);
			m_altfire_sprite_handle->TurnOff();
		}

		break;

		case ALTFIRE_STATE_OFF:
		{
			if (m_altfire_sprite_handle == NULL)
				break;

			m_altfire_sprite_handle->SetBrightness(0);
			m_altfire_sprite_handle->TurnOff();
		}
		break;

		default:
			break;
		}
	}
	//-----------------------------------------------------------------------------------------------------------------	
	void CreateAltfireEffects(void)
	{
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());

		if (m_altfire_sprite_handle != NULL)
			return;

		m_altfire_sprite_handle = CSprite::SpriteCreate(ALTFIRE_GLOW_SPRITE, GetAbsOrigin(), false);

		if (m_altfire_sprite_handle)
		{
			m_altfire_sprite_handle->SetAsTemporary();
			m_altfire_sprite_handle->SetAttachment(pOwner->GetViewModel(), LookupAttachment("indicator"));
			m_altfire_sprite_handle->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone);
			m_altfire_sprite_handle->SetBrightness(0);
			m_altfire_sprite_handle->SetScale(0.05f);
			m_altfire_sprite_handle->TurnOff();
		}
	}
	//-----------------------------------------------------------------------------------------------------------------	
	void DoAltfireEffect(void)
	{
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());
		
		if (pOwner == NULL)
			return;

		CBaseViewModel *pViewModel = pOwner->GetViewModel();

		if (pViewModel == NULL)
			return;

		CEffectData	data;

		data.m_nEntIndex = pViewModel->entindex();
		data.m_nAttachmentIndex = 1;
	}
	//-----------------------------------------------------------------------------------------------------------------	
	void	AddViewKick(void)
	{
		CBasePlayer *player = ToBasePlayer(GetOwner());
		if (!player) return;
		float flDuration = m_fFireDuration;
		if (g_pGameRules->GetAutoAimMode() == AUTOAIM_ON_CONSOLE) { flDuration = MIN(flDuration, 0.75f); }
		DoMachineGunKick(player, 0.5f, 4.0f, flDuration, 5.0f);
	}
	//-----------------------------------------------------------------------------------------------------------------	
	void	FireNPCPrimaryAttack(CBaseCombatCharacter *pOperator, bool bUseWeaponAngles) {
		Vector vecShootOrigin, vecShootDir;
		CAI_BaseNPC *npc = pOperator->MyNPCPointer();
		ASSERT(npc != NULL);

		if (bUseWeaponAngles) {
			QAngle	angShootDir;
			GetAttachment(LookupAttachment("muzzle"), vecShootOrigin, angShootDir);
			AngleVectors(angShootDir, &vecShootDir);
		}
		else {
			vecShootOrigin = pOperator->Weapon_ShootPosition();
			vecShootDir = npc->GetActualShootTrajectory(vecShootOrigin);
		}

		WeaponSoundRealtime(SINGLE_NPC);
		CSoundEnt::InsertSound(SOUND_COMBAT | SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(),
			SOUNDENT_VOLUME_MACHINEGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy());
		pOperator->FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED,
			MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2);

		m_iClip1 = m_iClip1 - 1;
	}
	//-----------------------------------------------------------------------------------------------------------------	
	void	FireNPCSecondaryAttack(CBaseCombatCharacter *pOperator, bool bUseWeaponAngles) {}
	//-----------------------------------------------------------------------------------------------------------------	
	void	Operator_ForceNPCFire(CBaseCombatCharacter  *pOperator, bool bSecondary) {
		if (bSecondary)	FireNPCSecondaryAttack(pOperator, true);
		else { m_iClip1++;	FireNPCPrimaryAttack(pOperator, true); }
	}
	//-----------------------------------------------------------------------------------------------------------------	
	void	Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator) {
		switch (pEvent->event)
		{
		case EVENT_WEAPON_AR2:
		{
			FireNPCPrimaryAttack(pOperator, false);
		}
		break;

		case EVENT_WEAPON_AR2_GRENADE:
		{
			FireNPCSecondaryAttack(pOperator, false);
		}
		break;

		default:
			CBaseCombatWeapon::Operator_HandleAnimEvent(pEvent, pOperator);
			break;
		}
	}
	//-----------------------------------------------------------------------------------------------------------------	
	Activity GetPrimaryAttackActivity(void) {
		if (m_shots_fired_int < 5)	return ACT_VM_PRIMARYATTACK;
		if (m_shots_fired_int < 15)	return ACT_VM_HITLEFT;
		return ACT_VM_HITLEFT2;
	}
	//-----------------------------------------------------------------------------------------------------------------	
	/*
	const WeaponProficiencyInfo_t *GetProficiencyValues() {
	static WeaponProficiencyInfo_t proficiencyTable[] =
	{
	{ 7.0,		0.75 },
	{ 5.00,		0.75 },
	{ 3.0,		0.85 },
	{ 5.0 / 3.0,	0.75 },
	{ 1.00,		1.0 },
	};

	COMPILE_TIME_ASSERT(ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);
	return proficiencyTable;
	}
	*/
};

BEGIN_DATADESC(CWeaponOICW)
DEFINE_FIELD(m_toss_velocity_vec3, FIELD_VECTOR),
DEFINE_FIELD(m_next_grenade_check_float, FIELD_TIME),
DEFINE_FIELD(m_altfire_state_int, FIELD_INTEGER),
DEFINE_FIELD(m_altfire_sprite_handle, FIELD_EHANDLE),
DEFINE_FIELD(m_altfire_charge_amount_float, FIELD_FLOAT),
DEFINE_FIELD(m_soonest_primary_attack_time, FIELD_TIME),
//DEFINE_FIELD(m_last_attack_time, FIELD_TIME), // no need to save them - 
//DEFINE_FIELD(m_accuracy_penalty_float, FIELD_FLOAT), // if player wants to bother saving
//DEFINE_FIELD(m_num_shots_fired_int, FIELD_INTEGER), // and reloading while firing, then be my guest.
END_DATADESC()
IMPLEMENT_SERVERCLASS_ST(CWeaponOICW, DT_WeaponOICW)
END_SEND_TABLE()
LINK_ENTITY_TO_CLASS(weapon_oicw, CWeaponOICW);
PRECACHE_WEAPON_REGISTER(weapon_oicw);

acttable_t	CWeaponOICW::m_acttable[] =
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_AR2,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,				true },
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG1,			true },
	{ ACT_WALK,						ACT_WALK_RIFLE,					true },
	// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG1_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG1_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims
																			// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
																			//End readiness activities
	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN,						ACT_RUN_RIFLE,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_AR2,	false },
	{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_AR2_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG1_LOW,		true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },
	//	{ ACT_RANGE_ATTACK2, ACT_RANGE_ATTACK_AR2_GRENADE, true },
};

IMPLEMENT_ACTTABLE(CWeaponOICW);
#endif

//====================================
// Tau Cannon (Dark Interval)
//====================================
#if defined (TAU)
#define TAU_BEAM_MODEL "sprites/laserbeam.vmt"
#define TAU_BEAM_DECAL "sprites/redglow1.vmt"
#define TAU_MAXDIST 4096
#define TAU_REFLECTDIST 2000
#define MAX_TAU_CHARGE_TIME 5

class CWeaponTau : public CBaseHLCombatWeapon {
private:
	DECLARE_DATADESC();
public:
	bool m_bCharging;
	float m_flChargeStartTime;
	float m_flNextAmmoTake;
	float m_flChargeAmount;
	CSoundPatch *m_sndChargeSound;
	DECLARE_CLASS(CWeaponTau, CBaseHLCombatWeapon);
	DECLARE_SERVERCLASS();
	// Constructor
	CWeaponTau(void) {
		m_can_fire_underwater_boolean = false;
		m_can_altfire_underwater_boolean = false;
		m_flChargeAmount = 0;
		m_bCharging = 0;
	}
	// Destructor - release our cached looping sound
	~CWeaponTau(void) {
		if (m_sndChargeSound != NULL)
		{
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			controller.SoundDestroy(m_sndChargeSound);
			m_sndChargeSound = NULL;
		}
	}
	int CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	// Doesn't affect anything atm
	//	const Vector& GetBulletSpread( void ) {
	//		static const Vector cone = VECTOR_CONE_3DEGREES;
	//		return cone;
	//	};
	float GetFireRate(void) { return 0.2; } // 400 rpm
	void Precache(void) {
		PrecacheModel(TAU_BEAM_MODEL);
		PrecacheModel(TAU_BEAM_DECAL);
		PrecacheScriptSound("Jeep.GaussCharge");
		BaseClass::Precache();
	}
	void PrimaryAttack(void) { return; }
	void MainAttack(void) {
		CBasePlayer *player = ToBasePlayer(GetOwner());
		if (!player)
			return;

		BaseClass::WeaponSound(SINGLE);
		SendWeaponAnim(ACT_VM_PRIMARYATTACK);
		player->SetAnimation(PLAYER_ATTACK1);
		player->DoMuzzleFlash(); // FIXME: doesn't do anything?..

		Vector vecSrc = player->Weapon_ShootPosition();
		Vector vecAim = player->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

		trace_t tr;
		UTIL_TraceLine(vecSrc, vecSrc + vecAim * TAU_MAXDIST, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
		CPVSFilter filter(tr.endpos);

		ClearMultiDamage();

		float flDamage = sk_plr_dmg_tau.GetFloat(); // regular damage
		if (tr.DidHit() && !(tr.surface.flags & SURF_SKY)) // don't create decals or sparks on skybox
		{
			float hitAngle = -DotProduct(tr.plane.normal, vecAim);

			if (hitAngle < 0.5f) // hitting at an obtuse angle; reflect
			{
				Vector vecReflection = 2.0 * tr.plane.normal * hitAngle + vecAim;
				// draw beam from the weapon to point of reflection (PoR)
				DrawBeam(tr.startpos, tr.endpos, 1.6f, false);
				te->GaussExplosion(filter, 0.01f, tr.endpos, tr.plane.normal, 0);
				// new trace from PoR
				UTIL_TraceLine(tr.endpos, tr.endpos + (vecReflection * TAU_REFLECTDIST), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
				if (tr.DidHit()) // reflected beam hit something, draw decal and sparks at the end
				{
					//	UTIL_DecalTrace(&tr, "RedGlowFade");
					CPVSFilter filter(tr.endpos);
					te->GaussExplosion(filter, 0.01f, tr.endpos, tr.plane.normal, 0);
				}
				// draw second beam from PoR to end of traceline 
				DrawBeam(tr.startpos, tr.endpos, 1.0f, true);
			}
			else
			{
				// didnt' reflect, draw a regular beam from the weapon
				DrawBeam(tr.startpos, tr.endpos, 1.6f, false);
				// place decal in the world
				//	UTIL_DecalTrace(&tr, "RedGlowFade");
				// create sparks
				te->GaussExplosion(filter, 0.01f, tr.endpos, tr.plane.normal, 0);
			}
			if (tr.m_pEnt && tr.m_pEnt->m_takedamage) // hit someone, or something; damage it
			{
				CTakeDamageInfo dmgInfo(this, this, flDamage, DMG_SHOCK, 0);
				CalculateBulletDamageForce(&dmgInfo, m_iPrimaryAmmoType, vecAim, tr.endpos, flDamage);
				tr.m_pEnt->DispatchTraceAttack(dmgInfo, vecAim, &tr);
			}
		}
		else // didn't hit anything. Just draw a beam.
		{
			DrawBeam(tr.startpos, tr.endpos, 1.6f, false);
		}

		ApplyMultiDamage();

		player->RemoveAmmo(1, m_iPrimaryAmmoType); // decrease our ammo

		AddViewKick();

		m_flNextPrimaryAttack = CURTIME + GetFireRate();
		m_flNextSecondaryAttack = CURTIME + 0.5f;
		m_iPrimaryAttacks++;
		gamestats->Event_WeaponFired(player, true, GetClassname());
	}
	void Charge(void) {
		CBasePlayer *player = ToBasePlayer(GetOwner());
		if (!player)
			return;

		if (m_bCharging == false)
		{
			m_flChargeStartTime = CURTIME;
			m_flNextAmmoTake = CURTIME;
			m_bCharging = true;
			//Start charging sound
			CPASAttenuationFilter filter(this);
			m_sndChargeSound = (CSoundEnvelopeController::GetController()).SoundCreate(filter, entindex(),
				CHAN_STATIC, "Jeep.GaussCharge", ATTN_NORM);
			assert(m_sndChargeSound != NULL);
			if (m_sndChargeSound != NULL)
			{
				CSoundEnvelopeController::GetController().Play(m_sndChargeSound, 1.0f, 50);
				CSoundEnvelopeController::GetController().SoundChangePitch(m_sndChargeSound, 250, 3.0f);
			}
			// Send charge animation
			SendWeaponAnim(ACT_VM_PULLBACK);
			return;
		}
		else
		{
			m_flChargeAmount = (CURTIME - m_flChargeStartTime) / MAX_TAU_CHARGE_TIME;
			if (m_flChargeAmount > 1.0f) { m_flChargeAmount = 1.0f; }
			else
			{
				if (m_flNextAmmoTake < CURTIME)
				{
					// remove ammo every once in a while. NOTE: don't use m_iClip1 here, because Gauss gun doesn't use clips.
					player->RemoveAmmo(1, m_iPrimaryAmmoType);
					m_flNextAmmoTake = CURTIME + 0.3f;
				}
			}
		}
	}
	void ChargeFire(void) {
		CBasePlayer *player = ToBasePlayer(GetOwner());
		if (!player)
			return;

		StopChargeSound(); // stop looping charge sound
		BaseClass::WeaponSound(WPN_DOUBLE); // BLAM
		SendWeaponAnim(ACT_VM_SECONDARYATTACK);
		player->SetAnimation(PLAYER_ATTACK1);

		Vector vecSrc = player->Weapon_ShootPosition();
		Vector vecAim = player->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

		trace_t tr;
		UTIL_TraceLine(vecSrc, vecSrc + vecAim * TAU_MAXDIST, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
		// minimum width is 2.0, width with 100% charge is 8.0
		DrawBeam(vecSrc, tr.endpos, MAX(2.0f, (8.0f*m_flChargeAmount)), false);

		ClearMultiDamage();

		if (m_flChargeAmount > 1.0f)
			m_flChargeAmount = 1.0f; // clamp charge value

		float flDamage = MAX(sk_plr_dmg_tau.GetFloat() * 4.0f, (sk_plr_dmg_tau.GetFloat() * (5 * m_flChargeAmount)));
		if (tr.DidHit() && !(tr.surface.flags & SURF_SKY)) // dont create explosions on skybox
		{
			if (tr.m_pEnt && tr.m_pEnt->m_takedamage) // hit someone
			{
				CTakeDamageInfo dmgInfo(this, this, flDamage, DMG_BLAST, 0);
				CalculateBulletDamageForce(&dmgInfo, m_iPrimaryAmmoType, vecAim, tr.endpos, MAX(30, flDamage));
				tr.m_pEnt->DispatchTraceAttack(dmgInfo, vecAim, &tr);
			}
			if (m_flChargeAmount > 0.25f) // create explosives if sufficient charge is accumulated
			{
				CPVSFilter filter(tr.endpos);
				te->Explosion(filter, 0.1f, &tr.endpos,
					(tr.contents & MASK_WATER) ? g_sModelIndexFireball : g_sModelIndexWExplosion,
					MAX(0.25, m_flChargeAmount), 10,
					/*TE_EXPLFLAG_NODLIGHTS |*/ TE_EXPLFLAG_NOFIREBALLSMOKE,
					100, (100 + (50 * m_flChargeAmount))); // underwater explosions should have corresponding model
				UTIL_DecalTrace(&tr, "Rollermine.Crater"); // FIXME: this decal doesn't work properly on models!
			}
			else { UTIL_DecalTrace(&tr, "RedGlowFade"); } // just place a melt decal

														  // don't damage the player unless it's the charged explosion. Yes, it does allow players to boost themselves with gauss!
			RadiusDamage(CTakeDamageInfo(this, this, flDamage, (m_flChargeAmount > 0.25f) ? DMG_BLAST : DMG_SHOCK),
				tr.endpos, (200.0f * (1 + m_flChargeAmount)), CLASS_NONE, (m_flChargeAmount > 0.25f) ? NULL : player);
		}

		ApplyMultiDamage();

		AddViewKick();

		m_bCharging = false;
		m_flNextSecondaryAttack = CURTIME + 1.5f;
		m_iSecondaryAttacks++;
		gamestats->Event_WeaponFired(player, false, GetClassname());
	}
	void StopChargeSound(void) {
		if (m_sndChargeSound != NULL)
			(CSoundEnvelopeController::GetController()).SoundFadeOut(m_sndChargeSound, 0.1f);
	}
	void ItemPostFrame(void) {
		CBasePlayer *player = ToBasePlayer(GetOwner());
		if (!player)
			return;

		if (player->GetAmmoCount(m_iPrimaryAmmoType) < 1) {
			if (m_bCharging) { ChargeFire(); } // fire prematurely if we spent all our ammo on the charge
			return;
		}
		if ((player->m_nButtons & IN_ATTACK) && m_flNextPrimaryAttack < CURTIME && !m_bCharging) {
			MainAttack();
			return;
		}

		if ((player->m_nButtons & IN_ATTACK2) && (m_flNextSecondaryAttack < CURTIME)) {
			if (player->GetWaterLevel() == 3)
			{
				BaseClass::WeaponSound(EMPTY);
				return; // no altfire underwater
			}
			Charge();
		}
		else if ((player->m_nButtons & IN_ATTACK2) == 0 && m_bCharging) {
			ChargeFire(); // the player released the key (or interrupted charging by using +zoom), fire off charged shot
			player->m_afButtonReleased = IN_ATTACK2; // force un-press
		}

		BaseClass::ItemPostFrame();
	}
	void DrawBeam(const Vector& startPos, const Vector& endPos, float width, bool IsReflected) {
		CBasePlayer *player = ToBasePlayer(GetOwner());
		if (!player)
			return;

		CBeam *beam = CBeam::BeamCreate(TAU_BEAM_MODEL, width); // main beam
		beam->SetStartPos(startPos);
		if (!IsReflected) // if it's a regular beam, start it at the muzzle.
		{
			beam->PointEntInit(endPos, this);
			beam->SetEndAttachment(LookupAttachment("muzzle"));
			beam->SetWidth(width*0.25f);
			beam->SetEndWidth(width);
		}
		else // if it's a reflected beam, take start and end positions from the arg list.
		{
			beam->SetEndPos(endPos);
			beam->SetWidth(width);
			beam->SetEndWidth(width*0.25f);
		}
		beam->LiveForTime((GetFireRate() / 2));
		beam->SetColor(255, 190, RandomInt(75, 95));
		beam->SetBrightness(255);
		beam->RelinkBeam();
		for (int i = 0; i < 3; i++) // secondary beams
		{
			beam = CBeam::BeamCreate(TAU_BEAM_MODEL, width*0.5f);
			beam->SetStartPos(startPos);
			if (!IsReflected) // if it's a regular beam, start it at the muzzle.
			{
				beam->PointEntInit(endPos, this);
				beam->SetEndAttachment(LookupAttachment("muzzle"));
				beam->SetWidth(width*0.3f);
				beam->SetEndWidth(width*0.3f);
			}
			else // if it's a reflected beam, take start and end positions from the arg list.
			{
				beam->SetEndPos(endPos);
				beam->SetWidth(width*0.3f);
				beam->SetEndWidth(width*0.3f);
			}
			beam->LiveForTime((GetFireRate() / 2));
			beam->SetColor(150, 85, RandomInt(120, 150));
			beam->SetBrightness(RandomInt(150, 200));
			beam->RelinkBeam();
			beam->SetNoise(1.6f * i);
		}
	}
	void AddViewKick(void) {
		CBasePlayer *player = ToBasePlayer(GetOwner());
		if (!player)
			return;

		QAngle	viewPunch;
		if (!m_bCharging) // regular kick
		{
			viewPunch.x = random->RandomFloat(-1.25, 1.25);
			viewPunch.y = random->RandomFloat(-1.0, 1.0);
		}
		else // altfire kicks more
		{
			viewPunch.x = random->RandomFloat(-7.0, 7.0);
			viewPunch.y = random->RandomFloat(-2.0, 2.0);
			Vector forward;
			AngleVectors(EyeAngles(), &forward, NULL, NULL);
			player->ApplyAbsVelocityImpulse(forward * -100); // phys. punch the player
		}
		viewPunch.z = 0.0f;
		player->ViewPunch(viewPunch); // do the punch


	}
};
IMPLEMENT_SERVERCLASS_ST(CWeaponTau, DT_WeaponTau)
END_SEND_TABLE()

BEGIN_DATADESC(CWeaponTau)
DEFINE_FIELD(m_bCharging, FIELD_BOOLEAN),
DEFINE_FIELD(m_flChargeAmount, FIELD_FLOAT),
DEFINE_FIELD(m_flChargeStartTime, FIELD_TIME),
DEFINE_FIELD(m_flNextAmmoTake, FIELD_TIME),
END_DATADESC()

LINK_ENTITY_TO_CLASS(weapon_tau, CWeaponTau);
PRECACHE_WEAPON_REGISTER(weapon_tau);
#endif

//====================================
// Cremator gun (Dark Interval)
//====================================
#if defined(IMMOLATOR)
#define immolator_animevent_startfx 64
#define immolator_animevent_stopfx 65
ConVar sk_immolator_dmg("sk_immolator_dmg", "24");
ConVar sk_immolator_heatdmg("sk_immolator_heatdmg", "8");
class CWeaponImmolator : public CBaseHLCombatWeapon
{
	DECLARE_CLASS(CWeaponImmolator, CBaseHLCombatWeapon);
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
protected:
	CSoundPatch *m_sndPlasmaSound;

public:
	CNetworkVar(bool, m_bLight);
	bool m_bDamageActive;
	bool m_bFx;
public:

	CWeaponImmolator()
	{
		m_bDamageActive = false;
		m_bFx = false;
	}

	~CWeaponImmolator()
	{
		if (m_sndPlasmaSound != NULL)
		{
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			controller.SoundDestroy(m_sndPlasmaSound);
			m_sndPlasmaSound = NULL;
		}

		StopFX();
		m_bDamageActive = false;
		m_bFx = false;
	}

	int CapabilitiesGet(void)
	{
		return bits_CAP_WEAPON_RANGE_ATTACK1 | bits_CAP_WEAPON_RANGE_ATTACK2;
	}

	float GetFireRate(void) { return 1.33f; }

	void Spawn(void)
	{
		Precache();
		BaseClass::Spawn();
	}

	void Precache(void)
	{
		BaseClass::Precache();
		PrecacheParticleSystem("Flamethrower_Viewmodel_FX");
		PrecacheScriptSound("DI_Immolator.Loop");
	}

	bool EntityInDamageCone(CBaseEntity *pEntity)
	{
		CBasePlayer *player = ToBasePlayer(GetOwner());

		Vector los = (pEntity->WorldSpaceCenter() - player->Weapon_ShootPosition());
		VectorNormalize(los);

		Vector facingDir = player->EyeDirection3D();

		float flDot = DotProduct(los, facingDir);

		if (flDot <= 0.9f)
			return false;

		return true;
	}

	void PrimaryAttack(void)
	{
		// empty body. We're calling attacks from ItemPostFrame() directly.
	}

	void MainAttack(void)
	{
		SendWeaponAnim(ACT_VM_PRIMARYATTACK);
		DoDamage(GetOwner());
		m_flNextPrimaryAttack = CURTIME + GetFireRate();
	}

	void SecondaryAttack(void)
	{
		if (m_bDamageActive) return; // no alt-firing while holding down attack1
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());

		if (pOwner == NULL)
			return;

		//if (!pOwner->IsSuitEquipped()) // for Part 2
		//	return;

		SendWeaponAnim(ACT_VM_SECONDARYATTACK);
		WeaponSound(WPN_DOUBLE);
		pOwner->RumbleEffect(RUMBLE_PISTOL, 0, RUMBLE_FLAG_RESTART);

		Vector vecAiming = pOwner->GetAutoaimVector(0);
		Vector vecSrc = pOwner->Weapon_TrueShootPosition_Front();
		//	GetAttachment("muzzle", vecSrc);

		DispatchParticleEffect("fireball_muzzle", PATTACH_POINT_FOLLOW,
			pOwner->GetViewModel(), "light", true);

		QAngle angAiming;
		VectorAngles(vecAiming, angAiming);

		vecAiming *= 2000;

		CSharedProjectile *pGrenade = (CSharedProjectile*)CreateEntityByName("shared_projectile");
		pGrenade->SetAbsOrigin(vecSrc);
		pGrenade->SetAbsAngles(vec3_angle);
		DispatchSpawn(pGrenade);
		pGrenade->SetThrower(pOwner);
		pGrenade->SetOwnerEntity(pOwner);

		pGrenade->SetProjectileStats(Projectile_LARGE, Projectile_INFERNO, Projectile_STICKYDELAY, sk_immolator_dmg.GetFloat() * 2, 100.0f, 300.0f, 128.0f, 0.01f);
		pGrenade->SetAbsVelocity(vecAiming);

		pGrenade->CreateProjectileEffects();

		// Tumble through the air
		pGrenade->SetLocalAngularVelocity(
			QAngle(random->RandomFloat(-250, -500),
				random->RandomFloat(-250, -500),
				random->RandomFloat(-250, -500)));

		m_flNextPrimaryAttack = CURTIME + 0.5f;
		m_flNextSecondaryAttack = CURTIME + 1.2f;
	}

	void DoDamage(CBaseCombatCharacter *pOwner)
	{
		float flAdjustedDamage, flDist;
		CBasePlayer *player = ToBasePlayer(pOwner);
		CBaseEntity *pEntity = NULL;

		if (m_bDamageActive)
		{
			while ((pEntity = gEntList.FindEntityInSphere(pEntity, player->GetAbsOrigin(), 400.0f)) != NULL)
			{
				if (EntityInDamageCone(pEntity) && (!pEntity->IsPlayer()))
				{
					flAdjustedDamage = 0.0f;
					flDist = (pEntity->WorldSpaceCenter() - GetAbsOrigin()).Length();

					if (pEntity->m_takedamage == DAMAGE_YES)
					{
						flAdjustedDamage = sk_immolator_dmg.GetFloat();
					}

					if (flAdjustedDamage > 0)
					{
						CTakeDamageInfo info(GetOwner(), GetOwner(), flAdjustedDamage, DMG_BURN | DMG_DISSOLVE_BURN);
						pEntity->TakeDamage(info);
					}

					// test if the entity is a fire and add heat to it
					CFire *pFire = dynamic_cast<CFire*>(pEntity);
					if (pFire)
					{
						pFire->AddHeat(sk_immolator_heatdmg.GetFloat(), false);
					}

				//	NDebugOverlay::Box(pEntity->WorldSpaceCenter(), pEntity->WorldAlignMins(), pEntity->WorldAlignMaxs(), 25, 255, 25, 50, 0.1f);
				}
				else
				{
				//	if (!pEntity->IsPlayer())
				//	{
				//		NDebugOverlay::Box(pEntity->WorldSpaceCenter(), pEntity->WorldAlignMins(), pEntity->WorldAlignMaxs(), 255, 25, 25, 50, 0.1f);
				//	}
				}
			}
		}
	}

	void ItemPostFrame()
	{
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());

		if (pOwner == NULL)
			return;

		if ((pOwner && pOwner->m_nButtons & IN_ATTACK) && m_flNextPrimaryAttack < CURTIME)
		{
			m_bDamageActive = true;
			MainAttack();
			return;
		}

		if ((pOwner->m_afButtonReleased & IN_ATTACK))
		{
			m_bDamageActive = false;
			StopFX();
			m_flNextSecondaryAttack = CURTIME + 0.5f;
		}

		BaseClass::ItemPostFrame();
	}

	void StartFX(void)
	{
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());

		if (pOwner == NULL)
			return;

		if (!m_bFx)
		{
			WeaponSound(SINGLE);

			CPASAttenuationFilter filter(this);
			m_sndPlasmaSound = (CSoundEnvelopeController::GetController()).SoundCreate(filter, entindex(),
				CHAN_STATIC, "DI_Immolator.Loop", ATTN_NORM);
			assert(m_sndPlasmaSound != NULL);
			if (m_sndPlasmaSound != NULL)
			{
				CSoundEnvelopeController::GetController().Play(m_sndPlasmaSound, 1.0f, 50);
				CSoundEnvelopeController::GetController().SoundChangePitch(m_sndPlasmaSound, 250, 3.0f);
			}

			EntityMessageBegin(this, true);
			WRITE_BYTE(0);
			MessageEnd();

			DispatchParticleEffect("Flamethrower_Viewmodel_FX", PATTACH_POINT_FOLLOW,
				pOwner->GetViewModel(), "muzzle", true);

			pOwner->AddEffects(EF_BRIGHTLIGHT);
			m_bLight = true;
			m_bFx = true;
		}
	}

	void StopFX(void)
	{
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());

		if (pOwner == NULL)
			return;
		if (m_bFx)
		{
			if (m_sndPlasmaSound != NULL)
				(CSoundEnvelopeController::GetController()).SoundFadeOut(m_sndPlasmaSound, 0.1f);

			WeaponSound(RELOAD);
			SendWeaponAnim(ACT_VM_IDLE);

			//	StopParticleEffects(pOwner->GetViewModel());
			DispatchParticleEffect("null", PATTACH_ABSORIGIN, pOwner->GetViewModel(), -1, true);

			EntityMessageBegin(this, true);
			WRITE_BYTE(1);
			MessageEnd();

			m_bLight = false;
			m_bFx = false;
		}
	}

	void Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
	{
		switch (pEvent->event)
		{
		case immolator_animevent_startfx:
			StartFX();
			break;

		case immolator_animevent_stopfx:
			m_bDamageActive = false;
			StopFX();
			break;

		default:
			BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
			break;
		}
	}
};

BEGIN_DATADESC(CWeaponImmolator)
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeaponImmolator, DT_WeaponImmolator)
SendPropInt(SENDINFO(m_bLight), 1, SPROP_UNSIGNED),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_immolator, CWeaponImmolator);
PRECACHE_WEAPON_REGISTER(weapon_immolator);
#endif

//====================================
// Shotgun (HL2)
// && Versus mini-game virtual SG
//====================================
#if defined(SHOTGUN)
class CWeaponShotgun : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS(CWeaponShotgun, CBaseHLCombatWeapon);

	DECLARE_SERVERCLASS();

private:
	bool	m_bNeedPump;		// When emptied completely
//	bool	m_pumping_after_doubleshot_bool; // if pumping after a secondary attack (double-shot), throw out 2 shells instead of 1 // undone, needs more work on model and code side
	bool	m_bDelayedFire1;	// Fire primary when finished reloading
	bool	m_bDelayedFire2;	// Fire secondary when finished reloading

public:
	void	Precache(void);

	int CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread(void)
	{
#ifdef DARKINTERVAL
		static Vector cone10 = VECTOR_CONE_10DEGREES;
		static Vector cone6 = VECTOR_CONE_6DEGREES;
		static Vector cone4 = VECTOR_CONE_4DEGREES;

		if (GetOwner() && (GetOwner()->ClassMatches("npc_combine_s")))
		{
			return cone6;
		}

		if (GetOwner() && (GetOwner()->ClassMatches("npc_combine_elite")))
		{
			return cone4;
		}

		return cone10;
#else
		static Vector vitalAllyCone = VECTOR_CONE_3DEGREES;
		static Vector cone = VECTOR_CONE_10DEGREES;

		if ( GetOwner() && ( GetOwner()->Classify() == CLASS_PLAYER_ALLY_VITAL ) )
		{
			// Give Alyx's shotgun blasts more a more directed punch. She needs
			// to be at least as deadly as she would be with her pistol to stay interesting (sjb)
			return vitalAllyCone;
		}

		return cone;
#endif // DARKINTERVAL
	}

	virtual int				GetMinBurst() { return 1; }
	virtual int				GetMaxBurst() { return 3; }

	virtual float			GetMinRestTime();
	virtual float			GetMaxRestTime();

	virtual float			GetFireRate(void);

	bool StartReload(void);
	bool Reload(void);
	void FillClip(void);
	void FinishReload(void);
	void CheckHolsterReload(void);
	void Pump(void);
	//	void WeaponIdle( void );
	void ItemHolsterFrame(void);
	void ItemPostFrame(void);
	void PrimaryAttack(void);
	void SecondaryAttack(void);
	void DryFire(void);

	void FireNPCPrimaryAttack(CBaseCombatCharacter *pOperator, bool bUseWeaponAngles);
	void Operator_ForceNPCFire(CBaseCombatCharacter  *pOperator, bool bSecondary);
	void Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);

	DECLARE_ACTTABLE();

	CWeaponShotgun(void);
};
#ifdef DARKINTERVAL
static ConVar sk_shotgun_altfire_method("sk_shotgun_altfire_method", "1", 0, "0 if using HL2 double-barrel fire, 1 if using DI's rapid firing");
#endif
acttable_t	CWeaponShotgun::m_acttable[] =
{
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },	// FIXME: hook to shotgun unique

	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SHOTGUN,		true },
	{ ACT_RELOAD,					ACT_RELOAD_SHOTGUN,				false },
	{ ACT_WALK,						ACT_WALK_RIFLE,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SHOTGUN,			true },

	// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SHOTGUN_RELAXED,		false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SHOTGUN_STIMULATED,	false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_SHOTGUN_AGITATED,		false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

	// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
																			//End readiness activities

	{ ACT_WALK_AIM,					ACT_WALK_AIM_SHOTGUN,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,				true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,			true },
	{ ACT_RUN,						ACT_RUN_RIFLE,						true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_SHOTGUN,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,				true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,			true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SHOTGUN,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SHOTGUN_LOW,		true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SHOTGUN_LOW,				false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SHOTGUN,			false },
};

IMPLEMENT_ACTTABLE(CWeaponShotgun);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponShotgun::CWeaponShotgun(void)
{
	m_reloads_singly_boolean = true;

	m_bNeedPump = false;
	//	m_pumping_after_doubleshot_bool = false;
	m_bDelayedFire1 = false;
	m_bDelayedFire2 = false;
#ifdef DARKINTERVAL
	m_fMinRange1 = 64.0;
	m_fMaxRange1 = 1000;
	m_fMinRange2 = 64.0;
	m_fMaxRange2 = 500;
#else
	m_fMinRange1 = 0.0;
	m_fMaxRange1 = 500;
	m_fMinRange2 = 0.0;
	m_fMaxRange2 = 200;
#endif
}

void CWeaponShotgun::Precache(void)
{
	CBaseCombatWeapon::Precache();
}

void CWeaponShotgun::FireNPCPrimaryAttack(CBaseCombatCharacter *pOperator, bool bUseWeaponAngles)
{
	Vector vecShootOrigin, vecShootDir;
	CAI_BaseNPC *npc = pOperator->MyNPCPointer();
	ASSERT(npc != NULL);
	WeaponSound(SINGLE_NPC);
	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;

	if (bUseWeaponAngles)
	{
		QAngle	angShootDir;
		GetAttachment(LookupAttachment("muzzle"), vecShootOrigin, angShootDir);
		AngleVectors(angShootDir, &vecShootDir);
	}
	else
	{
		vecShootOrigin = pOperator->Weapon_ShootPosition();
		vecShootDir = npc->GetActualShootTrajectory(vecShootOrigin);
	}
#ifdef DARKINTERVAL
	pOperator->FireBullets(8, vecShootOrigin, vecShootDir, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 1);
#else
	pOperator->FireBullets(8, vecShootOrigin, vecShootDir, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0);
#endif
}

void CWeaponShotgun::Operator_ForceNPCFire(CBaseCombatCharacter *pOperator, bool bSecondary)
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	FireNPCPrimaryAttack(pOperator, true);
}

void CWeaponShotgun::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_SHOTGUN_FIRE:
	{
		FireNPCPrimaryAttack(pOperator, false);
	}
	break;

	default:
		CBaseCombatWeapon::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

float CWeaponShotgun::GetMinRestTime()
{
#ifdef DARKINTERVAL
	if (GetOwner() && GetOwner()->Classify() == CLASS_COMBINE)
	{
		return 1.2f;
	}
	else
	{
		return 1.6f;
	}
#else
	if ( hl2_episodic.GetBool() && GetOwner() && GetOwner()->Classify() == CLASS_COMBINE )
	{
		return 1.2f;
	}
#endif
}

float CWeaponShotgun::GetMaxRestTime()
{
#ifdef DARKINTERVAL
	if (GetOwner() && GetOwner()->Classify() == CLASS_COMBINE)
	{
		return 1.5f;
	}
	else
	{
		return 1.9f;
	}
#else
	if ( hl2_episodic.GetBool() && GetOwner() && GetOwner()->Classify() == CLASS_COMBINE )
	{
		return 1.5f;
	}
#endif
}

float CWeaponShotgun::GetFireRate()
{
#ifdef DARKINTERVAL
	return 0.8f;
#else
	if ( hl2_episodic.GetBool() && GetOwner() && GetOwner()->Classify() == CLASS_COMBINE )
	{
		return 0.8f;
	}
	return 0.7;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Override so only reload one shell at a time
//-----------------------------------------------------------------------------
bool CWeaponShotgun::StartReload(void)
{
	CBaseCombatCharacter *pOwner = GetOwner();

	if (pOwner == NULL)
		return false;

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		return false;

	if (m_iClip1 >= GetMaxClip1())
		return false;

	// If shotgun totally emptied then a pump animation is needed

	//NOTENOTE: This is kinda lame because the player doesn't get strong feedback on when the reload has finished,
	//			without the pump.  Technically, it's incorrect, but it's good for feedback...
#ifndef DARKINTERVAL
	if (m_iClip1 <= 0)
#endif
	{
		m_bNeedPump = true;
	}

	int j = MIN(1, pOwner->GetAmmoCount(m_iPrimaryAmmoType));

	if (j <= 0)
		return false;

	SendWeaponAnim(ACT_SHOTGUN_RELOAD_START);

	// Make shotgun shell visible
	SetBodygroup(1, 0);

	pOwner->m_flNextAttack = CURTIME;
	m_flNextPrimaryAttack = CURTIME + SequenceDuration();

	m_bInReload = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Override so only reload one shell at a time
//-----------------------------------------------------------------------------
bool CWeaponShotgun::Reload(void)
{
	// Check that StartReload was called first
	if (!m_bInReload)
	{
		Warning("ERROR: Shotgun Reload called incorrectly!\n");
	}

	CBaseCombatCharacter *pOwner = GetOwner();

	if (pOwner == NULL)
		return false;

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		return false;

	if (m_iClip1 >= GetMaxClip1())
		return false;

	int j = MIN(1, pOwner->GetAmmoCount(m_iPrimaryAmmoType));

	if (j <= 0)
		return false;

	FillClip();
	// Play reload on different channel as otherwise steals channel away from fire sound
	WeaponSound(RELOAD);
	SendWeaponAnim(ACT_VM_RELOAD);

	pOwner->m_flNextAttack = CURTIME;
	m_flNextPrimaryAttack = CURTIME + SequenceDuration();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Play finish reload anim and fill clip
//-----------------------------------------------------------------------------
void CWeaponShotgun::FinishReload(void)
{
	// Make shotgun shell invisible
	SetBodygroup(1, 1);

	CBaseCombatCharacter *pOwner = GetOwner();

	if (pOwner == NULL)
		return;

	m_bInReload = false;

	// Finish reload animation
	SendWeaponAnim(ACT_SHOTGUN_RELOAD_FINISH);

	pOwner->m_flNextAttack = CURTIME;
	m_flNextPrimaryAttack = CURTIME + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: Play finish reload anim and fill clip
//-----------------------------------------------------------------------------
void CWeaponShotgun::FillClip(void)
{
	CBaseCombatCharacter *pOwner = GetOwner();

	if (pOwner == NULL)
		return;

	// Add them to the clip
	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) > 0)
	{
		if (Clip1() < GetMaxClip1())
		{
			m_iClip1++;
			pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
		}
	}
}

void CWeaponShotgun::Pump(void)
{
	CBaseCombatCharacter *pOwner = GetOwner();

	if (pOwner == NULL)
		return;

	m_bNeedPump = false;
	
	WeaponSound(SPECIAL1);

	// Finish reload animation
	SendWeaponAnim(ACT_SHOTGUN_PUMP);

//	m_pumping_after_doubleshot_bool = false;

	pOwner->m_flNextAttack = CURTIME + SequenceDuration();
	m_flNextPrimaryAttack = CURTIME + SequenceDuration();
}

void CWeaponShotgun::DryFire(void)
{
	WeaponSound(EMPTY);
	SendWeaponAnim(ACT_VM_DRYFIRE);

	m_flNextPrimaryAttack = CURTIME + SequenceDuration();
}

void CWeaponShotgun::PrimaryAttack(void)
{
	// Only the player fires this way so we can cast
	CBasePlayer *player = ToBasePlayer(GetOwner());

	if (!player)
	{
		return;
	}

	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);

	player->DoMuzzleFlash();

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);

	// player "shoot" animation
	player->SetAnimation(PLAYER_ATTACK1);

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = CURTIME + SequenceDuration();
#ifdef DARKINTERVAL // DI doesn't decrement ammo in godmode
	if (!(GetOwner()->GetFlags() & FL_GODMODE))
#endif
	{
		m_iClip1 -= 1;
	}

	Vector	vecSrc = player->Weapon_ShootPosition();
	Vector	vecAiming = player->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

	player->SetMuzzleFlashTime(CURTIME + 1.0);

	// Fire the bullets, and force the first shot to be perfectly accuracy
	player->FireBullets(sk_plr_num_shotgun_pellets.GetInt(), vecSrc, vecAiming, 
#ifdef DARKINTERVAL
		GetBulletSpread() / 3,
#else
		GetBulletSpread(),
#endif
		MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 1, -1, -1, 0, NULL, true, true);

	player->ViewPunch(QAngle(random->RandomFloat(-2, -1), random->RandomFloat(-2, 2), 0));

	CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2, GetOwner());

	if (!m_iClip1 && player->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
#ifdef DARKINTERVAL
		player->SetSuitUpdate("!HEVVOXNEW_AMMODEPLETED", true, 0);
#else
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
#endif
	}

	if (m_iClip1)
	{
		// pump so long as some rounds are left.
		m_bNeedPump = true;
	//	m_pumping_after_doubleshot_bool = false;
	}

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(player, true, GetClassname());
}

void CWeaponShotgun::SecondaryAttack(void)
{
	// Only the player fires this way so we can cast
	CBasePlayer *player = ToBasePlayer(GetOwner());

	if (!player)
	{
		return;
	}
#ifdef DARKINTERVAL
	if (sk_shotgun_altfire_method.GetInt() == 0)
#endif
	{
		player->m_nButtons &= ~IN_ATTACK2;
		// MUST call sound before removing a round from the clip of a CMachineGun
		WeaponSound(WPN_DOUBLE);

		player->DoMuzzleFlash();

		SendWeaponAnim(ACT_VM_SECONDARYATTACK);

		// player "shoot" animation
		player->SetAnimation(PLAYER_ATTACK1);

		// Don't fire again until fire animation has completed
		m_flNextPrimaryAttack = CURTIME + SequenceDuration();
#ifdef DARKINTERVAL // DI doesn't decrement ammo in godmode
		if ( !( GetOwner()->GetFlags() & FL_GODMODE ) )
#endif
		{
			m_iClip1 -= 2;	// Shotgun uses same clip for primary and secondary attacks
		}
		Vector vecSrc = player->Weapon_ShootPosition();
		Vector vecAiming = player->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

		// Fire the bullets
#ifdef DARKINTERVAL // add a skill convar controlling altire number of pellets
		player->FireBullets(sk_plr_num_shotgun_pellets_doublefire.GetInt(), vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0, -1, -1, 0, NULL, false, false);
#else
		player->FireBullets(12, vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0, -1, -1, 0, NULL, false, false);
#endif
		player->ViewPunch(QAngle(random->RandomFloat(-5, 5), 0, 0));

		player->SetMuzzleFlashTime(CURTIME + 1.0);

		CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2);

		if (!m_iClip1 && player->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		{
			// HEV suit - indicate out of ammo condition
#ifdef DARKINTERVAL
			player->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
#else
			player->SetSuitUpdate("!HEVVOXNEW_AMMODEPLETED", true, 0);
#endif
		}

		if (m_iClip1)
		{
			// pump so long as some rounds are left.
			m_bNeedPump = true;
		//	m_pumping_after_doubleshot_bool = true;
		}
	}
#ifdef DARKINTERVAL // "semi-auto" quick firing mode
	else //if(sk_shotgun_altfire_method.GetInt() == 1)
	{
		// MUST call sound before removing a round from the clip of a CMachineGun
		WeaponSound(SINGLE);

		player->DoMuzzleFlash();

		SendWeaponAnim(ACT_VM_PRIMARYATTACK);

		// player "shoot" animation
		player->SetAnimation(PLAYER_ATTACK1);

		// Don't fire again until fire animation has completed
		m_flNextPrimaryAttack = CURTIME + SequenceDuration();
#ifdef DARKINTERVAL // DI doesn't decrement ammo in godmode
		if ( !( GetOwner()->GetFlags() & FL_GODMODE ) )
#endif
		{
			m_iClip1 -= 1;
		}

		Vector	vecSrc = player->Weapon_ShootPosition();
		Vector	vecAiming = player->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

		player->SetMuzzleFlashTime(CURTIME + 1.0);

		// Fire the bullets, and force the first shot to be perfectly accuracy

		player->FireBullets(sk_plr_num_shotgun_pellets.GetInt(),
			vecSrc,
			vecAiming,
			GetBulletSpread() * 1.5f,
			MAX_TRACE_LENGTH,
			m_iPrimaryAmmoType,
			0,
			-1,
			-1,
			GetAmmoDef()->PlrDamage(m_iPrimaryAmmoType) * 1.2f,
			NULL, true, true);

		player->ViewPunch(QAngle(random->RandomFloat(-2, -1), random->RandomFloat(-2, 2), 0));

		CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_SHOTGUN, 0.2, GetOwner());

		if (!m_iClip1 && player->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		{
			// HEV suit - indicate out of ammo condition
			player->SetSuitUpdate("!HEVVOXNEW_AMMODEPLETED", true, 0);
		}
	}
#endif // DARKINTERVAL
	m_iSecondaryAttacks++;
	gamestats->Event_WeaponFired(player, false, GetClassname());
}

//-----------------------------------------------------------------------------
// Purpose: Override so shotgun can do mulitple reloads in a row
//-----------------------------------------------------------------------------
void CWeaponShotgun::ItemPostFrame(void)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());
	if (!pOwner)
	{
		return;
	}

	if (m_bInReload)
	{
		// If I'm primary firing and have one round stop reloading and fire
		if ((pOwner->m_nButtons & IN_ATTACK) && (m_iClip1 >= 1))
		{
			m_bInReload = false;
			m_bNeedPump = false;
			m_bDelayedFire1 = true;
		}
		// If I'm secondary firing and have one round stop reloading and fire
		else if ((pOwner->m_nButtons & IN_ATTACK2) && (m_iClip1 >= 2))
		{
			m_bInReload = false;
			m_bNeedPump = false;
			m_bDelayedFire2 = true;
		}
		else if (m_flNextPrimaryAttack <= CURTIME)
		{
			// If out of ammo end reload
			if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
			{
				FinishReload();
				return;
			}
			// If clip not full reload again
			if (m_iClip1 < GetMaxClip1())
			{
				Reload();
				return;
			}
			// Clip full, stop reloading
			else
			{
				FinishReload();
				return;
			}
		}
	}
	else
	{
		// Make shotgun shell invisible
		SetBodygroup(1, 1);
	}

	if ((m_bNeedPump) && (m_flNextPrimaryAttack <= CURTIME))
	{
		Pump();
		return;
	}

	// Shotgun uses same timing and ammo for secondary attack
	if ((m_bDelayedFire2 || pOwner->m_nButtons & IN_ATTACK2) && (m_flNextPrimaryAttack <= CURTIME))
	{
		m_bDelayedFire2 = false;

		if ((m_iClip1 <= 1 && UsesClipsForAmmo1()))
		{
			// If only one shell is left, do a single shot instead	
			if (m_iClip1 == 1)
			{
				PrimaryAttack();
			}
			else if (!pOwner->GetAmmoCount(m_iPrimaryAmmoType))
			{
				DryFire();
			}
			else
			{
				StartReload();
			}
		}

		// Fire underwater?
		else if (GetOwner()->GetWaterLevel() == 3 && m_can_fire_underwater_boolean == false)
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = CURTIME + 0.2;
			return;
		}
		else
		{
			// If the firing button was just pressed, reset the firing time
			if (pOwner->m_afButtonPressed & IN_ATTACK)
			{
				m_flNextPrimaryAttack = CURTIME;
			}
			SecondaryAttack();
		}
	}
	else if ((m_bDelayedFire1 || pOwner->m_nButtons & IN_ATTACK) && m_flNextPrimaryAttack <= CURTIME)
	{
		m_bDelayedFire1 = false;
		if ((m_iClip1 <= 0 && UsesClipsForAmmo1()) || (!UsesClipsForAmmo1() && !pOwner->GetAmmoCount(m_iPrimaryAmmoType)))
		{
			if (!pOwner->GetAmmoCount(m_iPrimaryAmmoType))
			{
				DryFire();
			}
			else
			{
				StartReload();
			}
		}
		// Fire underwater?
		else if (pOwner->GetWaterLevel() == 3 && m_can_fire_underwater_boolean == false)
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = CURTIME + 0.2;
			return;
		}
		else
		{
			// If the firing button was just pressed, reset the firing time
			CBasePlayer *player = ToBasePlayer(GetOwner());
			if (player && player->m_afButtonPressed & IN_ATTACK)
			{
				m_flNextPrimaryAttack = CURTIME;
			}
			PrimaryAttack();
		}
	}

	if (pOwner->m_nButtons & IN_RELOAD && UsesClipsForAmmo1() && !m_bInReload)
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		StartReload();
	}
	else
	{
		// no fire buttons down
		m_bFireOnEmpty = false;

		if (!HasAnyAmmo() && m_flNextPrimaryAttack < CURTIME)
		{
			// weapon isn't useable, switch.
			if (!(GetWeaponFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && pOwner->SwitchToNextBestWeapon(this))
			{
				m_flNextPrimaryAttack = CURTIME + 0.3;
				return;
			}
		}
		else
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if (m_iClip1 <= 0 && !(GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < CURTIME)
			{
				if (StartReload())
				{
					// if we've successfully started to reload, we're done
					return;
				}
			}
		}

		WeaponIdle();
		return;
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponShotgun::ItemHolsterFrame(void)
{
	// Must be player held
	if (GetOwner() && GetOwner()->IsPlayer() == false)
		return;

	// We can't be active
	if (GetOwner()->GetActiveWeapon() == this)
		return;
#ifdef DARKINTERVAL
	if (sk_auto_reload_enabled.GetBool())
#endif
	{
		// If it's been longer than three seconds, reload
		if ((CURTIME - m_flHolsterTime) > sk_auto_reload_time.GetFloat())
		{
			// Reset the timer
			m_flHolsterTime = CURTIME;

			if (GetOwner() == NULL)
				return;

			if (m_iClip1 == GetMaxClip1())
				return;

			// Just load the clip with no animations
			int ammoFill = MIN((GetMaxClip1() - m_iClip1), GetOwner()->GetAmmoCount(GetPrimaryAmmoType()));

			GetOwner()->RemoveAmmo(ammoFill, GetPrimaryAmmoType());
			m_iClip1 += ammoFill;
		}
	}
}

//-----------------------------------------------------------------------------
// Save/restore
//-----------------------------------------------------------------------------
BEGIN_DATADESC(CWeaponShotgun)
	DEFINE_FIELD(m_bNeedPump, FIELD_BOOLEAN),
	//DEFINE_FIELD(m_pumping_after_doubleshot_bool, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bDelayedFire1, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bDelayedFire2, FIELD_BOOLEAN),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeaponShotgun, DT_WeaponShotgun)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_shotgun, CWeaponShotgun);
PRECACHE_WEAPON_REGISTER(weapon_shotgun);

#endif // SHOTGUN
#if defined(SHOTGUN_VIRT)
#ifdef DARKINTERVAL
class CWeaponShotgunVirt : public CWeaponShotgun
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS(CWeaponShotgunVirt, CWeaponShotgun);

	CWeaponShotgunVirt() {};

	DECLARE_SERVERCLASS();

	void	Precache(void)
	{
		BaseClass::Precache();
	}

	void BreakWeapon(void)
	{
		const char *name = "weapon_broken";
		SetName(AllocPooledString(name));
		GetOwner()->Weapon_Drop(this, NULL, NULL);
		UTIL_Remove(this);
	}
};

IMPLEMENT_SERVERCLASS_ST(CWeaponShotgunVirt, DT_WeaponShotgunVirt)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_shotgun_virt, CWeaponShotgunVirt);
PRECACHE_WEAPON_REGISTER(weapon_shotgun_virt);

BEGIN_DATADESC(CWeaponShotgunVirt)
END_DATADESC()
#endif // DARKINTERVAL
#endif

//====================================
// Revolver (HL2). Modified in DI
// to allow NPC usage.
//====================================
#if defined(REVOLVER)
class CWeapon357 : public CBaseHLCombatWeapon
{
	DECLARE_CLASS(CWeapon357, CBaseHLCombatWeapon);
public:

	CWeapon357(void);

	void	PrimaryAttack(void);
	void	Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);
#ifdef DARKINTERVAL
	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
#endif
	float	WeaponAutoAimScale() { return 0.6f; }
#ifdef DARKINTERVAL // for NPC operators
	virtual const Vector& GetBulletSpread(void)
	{
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_4DEGREES;
		if (GetOwner() && GetOwner()->IsNPC())
			return npcCone;

		static Vector cone;

		cone = VECTOR_CONE_1DEGREES;

		return cone;
	}

	const WeaponProficiencyInfo_t *GetProficiencyValues() {
		static WeaponProficiencyInfo_t proficiencyTable[] =
		{
			{ 2.00, 1.0 }, // = WEAPON_PROFICIENCY_POOR
			{ 1.50, 1.0 }, // = WEAPON_PROFICIENCY_AVERAGE
			{ 1.00, 1.0 }, // = WEAPON_PROFICIENCY_GOOD
			{ 0.90, 1.0 }, // = WEAPON_PROFICIENCY_VERY_GOOD
			{ 0.50, 1.0 }, // = WEAPON_PROFICIENCY_PERFECT
		};

		COMPILE_TIME_ASSERT(ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);
		return proficiencyTable;
	}

	virtual int	GetMinBurst()
	{
		return 1;
	}

	virtual int	GetMaxBurst()
	{
		return 1;
	}

	virtual float GetFireRate(void)
	{
		return 1.0f;
	}

	virtual float			GetMinRestTime() { return 0.5f; }
	virtual float			GetMaxRestTime() { return 0.9f; }
#endif
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
#ifdef DARKINTERVAL
	DECLARE_ACTTABLE();
#endif
};
#ifdef DARKINTERVAL
acttable_t	CWeapon357::m_acttable[] =
{
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				false },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			false },

	// Readiness activities
	{ ACT_IDLE_RELAXED,				ACT_IDLE_PISTOL,				false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_PISTOL,				false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_PISTOL,			false },//always aims
	
	{ ACT_WALK_RELAXED,				ACT_WALK_PISTOL,				false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_PISTOL,				false },
	{ ACT_WALK_AGITATED,			ACT_WALK_PISTOL,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_PISTOL,					false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_PISTOL,					false },
	{ ACT_RUN_AGITATED,				ACT_RUN_PISTOL,					false },//always aims
	
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_AIM_PISTOL,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_PISTOL,			false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_PISTOL,			false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_AIM_PISTOL,				false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_PISTOL,				false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_PISTOL,				false },//always aims
	//End readiness activities

	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			false },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				false },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_PISTOL_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_WALK,						ACT_WALK_PISTOL,				false },
	{ ACT_RUN,						ACT_RUN_PISTOL,					false },
};

IMPLEMENT_ACTTABLE(CWeapon357);
#endif
CWeapon357::CWeapon357(void)
{
	m_reloads_singly_boolean = false;
	m_can_fire_underwater_boolean = false;
#ifdef DARKINTERVAL // for NPC operators
	m_fMinRange1 = 48;
	m_fMaxRange1 = 1500;
	m_fMinRange2 = 48;
	m_fMaxRange2 = 200;

	m_flNextPrimaryAttack = 0;
#endif
}

void CWeapon357::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	switch (pEvent->event)
	{
#ifdef DARKINTERVAL // for NPC operators
	case EVENT_WEAPON_PISTOL_FIRE:
	{
		//	Msg("event\n");
		Vector vecShootOrigin, vecShootDir;
		vecShootOrigin = pOperator->Weapon_ShootPosition();

		CAI_BaseNPC *npc = pOperator->MyNPCPointer();
		ASSERT(npc != NULL);

		if (m_flNextPrimaryAttack <= CURTIME)
		{
			vecShootDir = npc->GetActualShootTrajectory(vecShootOrigin);

			CSoundEnt::InsertSound(SOUND_COMBAT | SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy());

			WeaponSound(SINGLE_NPC);
			pOperator->FireBullets(1, vecShootOrigin, vecShootDir, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2);
			pOperator->DoMuzzleFlash();
			pOperator->m_flNextAttack = CURTIME + RandomFloat(GetMinRestTime(), GetMaxRestTime());
			m_flNextPrimaryAttack = CURTIME + RandomFloat(GetMinRestTime(), GetMaxRestTime());
			m_iClip1 = m_iClip1 - 1;
		}
		break;
	}
#endif
	case EVENT_WEAPON_RELOAD:
	{
		CEffectData data;
		// Emit up to six spent shells
#ifdef DARKINTERVAL
		for (int i = 0; i < (6 - m_iClip1); i++)
#else
		for ( int i = 0; i < 6; i++ )
#endif
		{
			data.m_vOrigin = pOwner->WorldSpaceCenter() + RandomVector(-4, 4);
			data.m_vAngles = QAngle(90, random->RandomInt(0, 360), 0);
			data.m_nEntIndex = entindex();
#ifdef DARKINTERVAL
			DispatchEffect("EjectShell_Revolver", data);
#else
			DispatchEffect("ShellEject", data);
#endif
		}
		break;
	}
	}
}

void CWeapon357::PrimaryAttack(void)
{
	// Only the player fires this way so we can cast
	CBasePlayer *player = ToBasePlayer(GetOwner());

	if (!player)
	{
		return;
	}

	if (m_iClip1 <= 0)
	{
		if (!m_bFireOnEmpty)
		{
			Reload();
		}
		else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(player, true, GetClassname());

	WeaponSound(SINGLE);
	player->DoMuzzleFlash();

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	player->SetAnimation(PLAYER_ATTACK1);

	m_flNextPrimaryAttack = CURTIME + 0.75;
	m_flNextSecondaryAttack = CURTIME + 0.75;
#ifdef DARKINTERVAL // DI doesn't decrement ammo in godmode
	if (!(GetOwner()->GetFlags() & FL_GODMODE))
#endif
	{
		m_iClip1--;
	}

	Vector vecSrc = player->Weapon_ShootPosition();
	Vector vecAiming = player->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

	player->FireBullets(1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0);
#ifdef DARKINTERVAL
	// Penetrate thin geometry
	//Shoot a shot straight out
	bool penetrated = false;

	trace_t	tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecAiming * 4096, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

	ClearMultiDamage();

	//Look for wall penetration
	if (tr.DidHitWorld() && !(tr.surface.flags & SURF_SKY))
	{
		//Try wall penetration
		UTIL_ImpactTrace(&tr, DMG_BULLET);
		
		Vector	testPos = tr.endpos + (vecAiming * 20.0f);

		UTIL_TraceLine(testPos, tr.endpos, MASK_SHOT, player, COLLISION_GROUP_NONE, &tr);

		if (tr.allsolid == false)
		{
			penetrated = true;
			UTIL_ImpactTrace(&tr, DMG_BULLET); // create exit bullet hole
		}
	}

	if (penetrated)
	{
		// tr.endpos here is the endpos from UTIL_TraceLine above which tells us
		// if it was tr.allsolid or not - this endpos is from the 
		// other side of the penetrated brush in case of penetration
		player->FireBullets(1, tr.endpos, vecAiming, vec3_origin, 512, m_iPrimaryAmmoType, 0);
	}

	ApplyMultiDamage();
#endif // DARKINTERVAL
	player->SetMuzzleFlashTime(CURTIME + 0.5);

	//Disorient the player
	QAngle angles = player->GetLocalAngles();

	angles.x += random->RandomInt(-1, 1);
	angles.y += random->RandomInt(-1, 1);
	angles.z = 0;

	player->SnapEyeAngles(angles);

	player->ViewPunch(QAngle(-8, random->RandomFloat(-2, 2), 0));

	CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 600, 0.2, GetOwner());

	if (!m_iClip1 && player->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
#ifdef DARKINTERVAL
		player->SetSuitUpdate("!HEVVOXNEW_AMMODEPLETED", true, 0);
#else
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
#endif
	}
}

//-----------------------------------------------------------------------------
// Save/restore
//-----------------------------------------------------------------------------
BEGIN_DATADESC(CWeapon357)
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeapon357, DT_Weapon357)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_357, CWeapon357);
PRECACHE_WEAPON_REGISTER(weapon_357);
#endif

//====================================
// Pistol (HL2) + worker version (DI)
//====================================
#if defined(PISTOL)

#define	PISTOL_FASTEST_REFIRE_TIME				0.1f
#define	PISTOL_FASTEST_DRY_REFIRE_TIME			0.2f
#define	PISTOL_ACCURACY_SHOT_PENALTY_TIME		0.2f	// Applied amount of time each shot adds to the time we must recover from
#define	PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME	1.5f	// Maximum penalty to deal out

ConVar	pistol_use_new_accuracy("pistol_use_new_accuracy", "1");
class CWeaponPistol : public CBaseHLCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS(CWeaponPistol, CBaseHLCombatWeapon);
	CWeaponPistol(void);
	DECLARE_SERVERCLASS();
#ifdef DARKINTERVAL
	void	UpdateViewModel(void);
	bool	Deploy(void);
#endif
	void	Precache(void);
	void	ItemPostFrame(void);
	void	ItemPreFrame(void);
	void	ItemBusyFrame(void);
	virtual void	PrimaryAttack(void);
	void	AddViewKick(void);
	void	DryFire(void);
	void	Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);

	void	UpdatePenaltyTime(void);

	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	Activity	GetPrimaryAttackActivity(void);

	virtual bool Reload(void);

	virtual const Vector& GetBulletSpread(void)
	{
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if (GetOwner() && GetOwner()->IsNPC())
		{
#ifdef DARKINTERVAL
			if (GetOwner() && GetOwner()->Classify() == CLASS_CITIZEN_PASSIVE || GetOwner()->Classify() == CLASS_CITIZEN_REBEL)
				npcCone = VECTOR_CONE_6DEGREES;
#endif
			return npcCone;
		}
		static Vector cone;

		if (pistol_use_new_accuracy.GetBool())
		{
			float ramp = RemapValClamped(m_accuracy_penalty_float,
				0.0f,
				PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME,
				0.0f,
				1.0f);

			// We lerp from very accurate to inaccurate over time
			VectorLerp(VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone);
		}
		else
		{
			// Old value
			cone = VECTOR_CONE_4DEGREES;
		}

		return cone;
	}

	virtual int	GetMinBurst()
	{
		return 1;
	}

	virtual int	GetMaxBurst()
	{
#ifdef DARKINTERVAL
		if (GetOwner() && GetOwner()->Classify() == CLASS_CITIZEN_PASSIVE || GetOwner()->Classify() == CLASS_CITIZEN_REBEL)
			return 1;
		else
#endif
			return 3;
	}

	virtual float GetFireRate(void)
	{
		return 0.5f;
	}

	DECLARE_ACTTABLE();

private:
	float	m_soonest_primary_attack_time;
	float	m_last_attack_time;
	float	m_accuracy_penalty_float;
	int		m_num_shots_fired_int;
};
#ifdef DARKINTERVAL
acttable_t	CWeaponPistol::m_acttable[] =
{
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				false },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			false },
	
	// Readiness activities
	{ ACT_IDLE_RELAXED,				ACT_IDLE_PISTOL,				false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_PISTOL,				false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_PISTOL,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_PISTOL,				false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_PISTOL,				false },
	{ ACT_WALK_AGITATED,			ACT_WALK_PISTOL,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_PISTOL,					false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_PISTOL,					false },
	{ ACT_RUN_AGITATED,				ACT_RUN_PISTOL,					false },//always aims

	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_AIM_PISTOL,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_PISTOL,			false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_PISTOL,			false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_AIM_PISTOL,				false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_PISTOL,				false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_PISTOL,				false },//always aims
	//End readiness activities

	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			false },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				false },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_PISTOL_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_WALK,						ACT_WALK_PISTOL,				false },
	{ ACT_RUN,						ACT_RUN_PISTOL,					false },
};
#else
acttable_t	CWeaponPistol::m_acttable[] =
{
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			true },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_PISTOL_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_WALK,						ACT_WALK_PISTOL,				false },
	{ ACT_RUN,						ACT_RUN_PISTOL,					false },
};
#endif // DARKINTERVAL
IMPLEMENT_ACTTABLE(CWeaponPistol);

CWeaponPistol::CWeaponPistol(void)
{
	m_soonest_primary_attack_time = CURTIME;
	m_accuracy_penalty_float = 0.0f;
#ifdef DARKINTERVAL
	m_fMinRange1 = 48;
	m_fMinRange2 = 48;
#else
	m_fMinRange1 = 24;
	m_fMinRange2 = 24;
#endif
	m_fMaxRange1 = 1500;
	m_fMaxRange2 = 200;
	m_can_fire_underwater_boolean = true;
}

void CWeaponPistol::Precache(void)
{
	BaseClass::Precache();
}
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: handle player's pistol under different circumstances, such as
// being in worker suit without the HEV
//-----------------------------------------------------------------------------
void CWeaponPistol::UpdateViewModel(void)
{
	if (GetOwner() && GetOwner()->IsPlayer())
	{
		CBasePlayer *owner = ToBasePlayer(GetOwner());
		if (GetOwner() != NULL && GetOwner()->IsPlayer())
		{
			CBaseViewModel *viewmodel = owner->GetViewModel(0);
			if (viewmodel != NULL)
			{
				if (!owner->IsSuitEquipped())
				{
					viewmodel->SetBodygroup(1, 1);
					viewmodel->m_nSkin = 1;
				}
				else
				{
					viewmodel->SetBodygroup(1, 0);
					viewmodel->m_nSkin = 0;
				}
			}
		}
	}
}

bool CWeaponPistol::Deploy(void)
{
	UpdateViewModel();
	return BaseClass::Deploy();
}
#endif // DARKINTERVAL
void CWeaponPistol::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_PISTOL_FIRE:
	{
		Vector vecShootOrigin, vecShootDir;
		vecShootOrigin = pOperator->Weapon_ShootPosition();

		CAI_BaseNPC *npc = pOperator->MyNPCPointer();
		ASSERT(npc != NULL);

		vecShootDir = npc->GetActualShootTrajectory(vecShootOrigin);

		CSoundEnt::InsertSound(SOUND_COMBAT | SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy());

		WeaponSound(SINGLE_NPC);
		pOperator->DoMuzzleFlash();
		pOperator->FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 1);
		m_iClip1 = m_iClip1 - 1;
	}
	break;
	default:
		BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

void CWeaponPistol::DryFire(void)
{
	WeaponSound(EMPTY);
	SendWeaponAnim(ACT_VM_DRYFIRE);

	m_soonest_primary_attack_time = CURTIME + PISTOL_FASTEST_DRY_REFIRE_TIME;
	m_flNextPrimaryAttack = CURTIME + SequenceDuration();
}

void CWeaponPistol::PrimaryAttack(void)
{
	if ((CURTIME - m_last_attack_time) > 0.5f)
	{
		m_num_shots_fired_int = 0;
	}
	else
	{
		m_num_shots_fired_int++;
	}

	m_last_attack_time = CURTIME;
	m_soonest_primary_attack_time = CURTIME + PISTOL_FASTEST_REFIRE_TIME;
	CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_PISTOL, 0.2, GetOwner());

	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if (pOwner)
	{
		// Each time the player fires the pistol, reset the view punch. This prevents
		// the aim from 'drifting off' when the player fires very quickly. This may
		// not be the ideal way to achieve this, but it's cheap and it works, which is
		// great for a feature we're evaluating. (sjb)
		pOwner->ViewPunchReset();
	}

	BaseClass::PrimaryAttack();

	// Add an accuracy penalty which can move past our maximum penalty time if we're really spastic
	m_accuracy_penalty_float += PISTOL_ACCURACY_SHOT_PENALTY_TIME;

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(pOwner, true, GetClassname());
}

void CWeaponPistol::UpdatePenaltyTime(void)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	// Check our penalty time decay
	if (((pOwner->m_nButtons & IN_ATTACK) == false) && (m_soonest_primary_attack_time < CURTIME))
	{
		m_accuracy_penalty_float -= gpGlobals->frametime;
		m_accuracy_penalty_float = clamp(m_accuracy_penalty_float, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME);
	}	
}

void CWeaponPistol::ItemPreFrame(void)
{
	UpdatePenaltyTime();
#ifdef DARKINTERVAL
	UpdateViewModel();
#endif
	BaseClass::ItemPreFrame();
}

void CWeaponPistol::ItemBusyFrame(void)
{
	UpdatePenaltyTime();
#ifdef DARKINTERVAL
	UpdateViewModel();
#endif
	BaseClass::ItemBusyFrame();
}

void CWeaponPistol::ItemPostFrame(void)
{
	BaseClass::ItemPostFrame();
#ifdef DARKINTERVAL
	UpdateViewModel();
#endif
	if (m_bInReload)
		return;

	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	//Allow a refire as fast as the player can click
	if (((pOwner->m_nButtons & IN_ATTACK) == false) && (m_soonest_primary_attack_time < CURTIME))
	{
		m_flNextPrimaryAttack = CURTIME - 0.1f;
	}
	else if ((pOwner->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack < CURTIME) && (m_iClip1 <= 0))
	{
		DryFire();
	}
}

Activity CWeaponPistol::GetPrimaryAttackActivity(void)
{
	if (m_num_shots_fired_int < 1)
		return ACT_VM_PRIMARYATTACK;

	if (m_num_shots_fired_int < 2)
		return ACT_VM_RECOIL1;

	if (m_num_shots_fired_int < 3)
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

bool CWeaponPistol::Reload(void)
{
	bool fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
	if (fRet)
	{
		WeaponSound(RELOAD);
		m_accuracy_penalty_float = 0.0f;
	}
	return fRet;
}

void CWeaponPistol::AddViewKick(void)
{
	CHL2_Player *player = ToHL2Player(GetOwner());

	if (player == NULL)
		return;

	QAngle	viewPunch;

	if (player->IsZooming())
	{
		viewPunch.x = random->RandomFloat(0.2f, 0.37f);
		viewPunch.y = random->RandomFloat(-.4f, .4f);
		viewPunch.z = 0.0f;
	}
	else
	{
		viewPunch.x = random->RandomFloat(0.25f, 0.5f);
		viewPunch.y = random->RandomFloat(-.6f, .6f);
	}
	viewPunch.z = 0.0f;

	//Add it to the view punch
	player->ViewPunch(viewPunch);
}

//-----------------------------------------------------------------------------
// Save/restore
//-----------------------------------------------------------------------------
BEGIN_DATADESC(CWeaponPistol)
	DEFINE_FIELD(m_soonest_primary_attack_time, FIELD_TIME),
	DEFINE_FIELD(m_last_attack_time, FIELD_TIME),
	DEFINE_FIELD(m_accuracy_penalty_float, FIELD_FLOAT), //NOTENOTE: This is NOT tracking game time
	DEFINE_FIELD(m_num_shots_fired_int, FIELD_INTEGER),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeaponPistol, DT_WeaponPistol)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_pistol, CWeaponPistol);
PRECACHE_WEAPON_REGISTER(weapon_pistol);
#endif

//====================================
// SMG1 (HL2) & Versus mini-game version
//====================================
#if defined(SMG1)
#ifdef DARKINTERVAL
class CWeaponSMG1 : public CHLMachineGun
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS(CWeaponSMG1, CHLMachineGun);
#else
class CWeaponSMG1 : public CHLSelectFireMachineGun
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS(CWeaponSMG1, CHLSelectFireMachineGun);
#endif
	CWeaponSMG1();

	DECLARE_SERVERCLASS();

	void	Precache(void);
	void	AddViewKick(void);
	void	PrimaryAttack(void);
	void	SecondaryAttack(void);
#ifdef DARKINTERVAL
	void	UpdateViewModel(void);
#endif
#ifdef DARKINTERVAL
	int		GetMinBurst() { return 3; }
#else
	int		GetMinBurst() { return 2; }
#endif
	int		GetMaxBurst() { return 5; }

	virtual void Equip(CBaseCombatCharacter *pOwner);
	bool	Reload(void);

	float	GetFireRate(void) { return 0.075f; }	// 13.3hz
	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	int		WeaponRangeAttack2Condition(float flDot, float flDist);
	Activity	GetPrimaryAttackActivity(void);

	virtual const Vector& GetBulletSpread(void)
	{
#ifdef DARKINTERVAL
		static const Vector cone = Vector(0.018f, 0.021f, 0.018f); // VECTOR_CONE_3DEGREES;
#else
		static const Vector cone = VECTOR_CONE_5DEGREES;
#endif
		return cone;
	}

	//	const WeaponProficiencyInfo_t *GetProficiencyValues();

	void FireNPCPrimaryAttack(CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir);
	void Operator_ForceNPCFire(CBaseCombatCharacter  *pOperator, bool bSecondary);
	void Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);

	DECLARE_ACTTABLE();

protected:

	Vector	m_toss_velocity_vec3;
	float	m_next_grenade_check_float;
};

acttable_t	CWeaponSMG1::m_acttable[] =
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SMG1,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,				true },
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG1,			true },

	{ ACT_WALK,						ACT_WALK_RIFLE,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },

	// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG1_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG1_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

	// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
	//End readiness activities

	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN,						ACT_RUN_RIFLE,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SMG1,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG1_LOW,		true },
	{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_SMG1_LOW,			false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },
};
IMPLEMENT_ACTTABLE(CWeaponSMG1);

CWeaponSMG1::CWeaponSMG1()
{
	m_fMinRange1 = 0;// No minimum range. 
#ifdef DARKINTERVAL
	m_fMaxRange1 = 2000;
#else
	m_fMaxRange1 = 1400;
#endif
	m_can_altfire_underwater_boolean = false;
}

void CWeaponSMG1::Precache(void)
{
	UTIL_PrecacheOther("grenade_ar2");

	BaseClass::Precache();
}

void CWeaponSMG1::Equip(CBaseCombatCharacter *pOwner)
{
	BaseClass::Equip(pOwner);
}
#ifdef DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: handle player's SMG under different circumstances, such as
// being in worker suit without the HEV
//-----------------------------------------------------------------------------
void CWeaponSMG1::UpdateViewModel(void)
{
	if (GetOwner() && GetOwner()->IsPlayer())
	{
		CBasePlayer *owner = ToBasePlayer(GetOwner());
		if (GetOwner() != NULL && GetOwner()->IsPlayer())
		{
			CBaseViewModel *viewmodel = owner->GetViewModel(0);
			if (viewmodel != NULL)
			{
				if (!owner->IsSuitEquipped())
				{
					viewmodel->SetBodygroup(1, 1);
					viewmodel->m_nSkin = 1;
				}
				else
				{
					viewmodel->SetBodygroup(1, 0);
					viewmodel->m_nSkin = 0;
				}
			}
		}
	}
}
#endif // DARKINTERVAL
void CWeaponSMG1::FireNPCPrimaryAttack(CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir)
{
	// FIXME: use the returned number of bullets to account for >10hz firerate
	WeaponSoundRealtime(SINGLE_NPC);

	CSoundEnt::InsertSound(SOUND_COMBAT | SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy());
	pOperator->FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED,
		MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2, entindex(), 0);

	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;
}

void CWeaponSMG1::Operator_ForceNPCFire(CBaseCombatCharacter *pOperator, bool bSecondary)
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	Vector vecShootOrigin, vecShootDir;
	QAngle	angShootDir;
	GetAttachment(LookupAttachment("muzzle"), vecShootOrigin, angShootDir);
	AngleVectors(angShootDir, &vecShootDir);
	FireNPCPrimaryAttack(pOperator, vecShootOrigin, vecShootDir);
}

void CWeaponSMG1::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_SMG1:
	{
		Vector vecShootOrigin, vecShootDir;
		QAngle angDiscard;

		// Support old style attachment point firing
		if ((pEvent->options == NULL) || (pEvent->options[0] == '\0') || (!pOperator->GetAttachment(pEvent->options, vecShootOrigin, angDiscard)))
		{
			vecShootOrigin = pOperator->Weapon_ShootPosition();
		}

		CAI_BaseNPC *npc = pOperator->MyNPCPointer();
		ASSERT(npc != NULL);
		vecShootDir = npc->GetActualShootTrajectory(vecShootOrigin);

		FireNPCPrimaryAttack(pOperator, vecShootOrigin, vecShootDir);
	}
	break;

	/*//FIXME: Re-enable
	case EVENT_WEAPON_AR2_GRENADE:
	{
	CAI_BaseNPC *npc = pOperator->MyNPCPointer();

	Vector vecShootOrigin, vecShootDir;
	vecShootOrigin = pOperator->Weapon_ShootPosition();
	vecShootDir = npc->GetShootEnemyDir( vecShootOrigin );

	Vector vecThrow = m_toss_velocity_vec3;

	CGrenadeAR2 *pGrenade = (CGrenadeAR2*)Create( "grenade_ar2", vecShootOrigin, vec3_angle, npc );
	pGrenade->SetAbsVelocity( vecThrow );
	pGrenade->SetLocalAngularVelocity( QAngle( 0, 400, 0 ) );
	pGrenade->SetMoveType( MOVETYPE_FLYGRAVITY );
	pGrenade->m_hOwner			= npc;
	pGrenade->m_pMyWeaponAR2	= this;
	pGrenade->SetDamage(sk_npc_dmg_ar2_grenade.GetFloat());

	// FIXME: arrgg ,this is hard coded into the weapon???
	m_next_grenade_check_float = CURTIME + 6;// wait six seconds before even looking again to see if a grenade can be thrown.

	m_iClip2--;
	}
	break;
	*/

	default:
		BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

Activity CWeaponSMG1::GetPrimaryAttackActivity(void)
{
	if (m_shots_fired_int < 2)
		return ACT_VM_PRIMARYATTACK;

	if (m_shots_fired_int < 3)
		return ACT_VM_RECOIL1;

	if (m_shots_fired_int < 4)
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

bool CWeaponSMG1::Reload(void)
{
	bool fRet;
	float fCacheTime = m_flNextSecondaryAttack;

	fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
	if (fRet)
	{
		// Undo whatever the reload process has done to our secondary
		// attack timer. We allow you to interrupt reloading to fire
		// a grenade.
		m_flNextSecondaryAttack = GetOwner()->m_flNextAttack = fCacheTime;

		WeaponSound(RELOAD);
#ifdef DARKINTERVAL // virtual version
		if (GetOwner()->IsPlayer() && GlobalEntity_GetState("playing_arcade") == GLOBAL_ON)
		{
			GetOwner()->GiveAmmo(255, GetPrimaryAmmoType(), true);
		}
#endif
	}

	return fRet;
}

void CWeaponSMG1::AddViewKick(void)
{
	//Get the view kick
	CBasePlayer *player = ToBasePlayer(GetOwner());

	if (player == NULL)
		return;

	DoMachineGunKick(player, 0.5f, 1.0f, m_fFireDuration, 2.0f);
}

void CWeaponSMG1::PrimaryAttack(void)
{
	BaseClass::PrimaryAttack();
}

void CWeaponSMG1::SecondaryAttack(void)
{
	// Only the player fires this way so we can cast
	CBasePlayer *player = ToBasePlayer(GetOwner());

	if (player == NULL)
		return;

	//Must have ammo
	if ((player->GetAmmoCount(m_iSecondaryAmmoType) <= 0) || (player->GetWaterLevel() == 3))
	{
		SendWeaponAnim(ACT_VM_DRYFIRE);
		BaseClass::WeaponSound(EMPTY);
#ifdef DARKINTERVAL
		m_flNextSecondaryAttack = CURTIME + 1.5f;
#else
		m_flNextSecondaryAttack = CURTIME + 0.5f;
#endif
		return;
	}

	if (m_bInReload)
		m_bInReload = false;

	// MUST call sound before removing a round from the clip of a CMachineGun
	BaseClass::WeaponSound(WPN_DOUBLE);

	player->RumbleEffect(RUMBLE_357, 0, RUMBLE_FLAGS_NONE);

	Vector vecSrc = player->Weapon_ShootPosition();
	Vector	vecThrow;
	// Don't autoaim on grenade tosses
	AngleVectors(player->EyeAngles() + player->GetPunchAngle(), &vecThrow);
	VectorScale(vecThrow, 1000.0f, vecThrow);

	//Create the grenade
	QAngle angles;
	VectorAngles(vecThrow, angles);
	CGrenadeAR2 *pGrenade = (CGrenadeAR2*)Create("grenade_ar2", vecSrc, angles, player);
	pGrenade->SetAbsVelocity(vecThrow);

	pGrenade->SetLocalAngularVelocity(RandomAngle(-400, 400));
	pGrenade->SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE);
	pGrenade->SetThrower(GetOwner());
	pGrenade->SetDamage(sk_plr_dmg_smg1_grenade.GetFloat());

	SendWeaponAnim(ACT_VM_SECONDARYATTACK);

	CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 1000, 0.2, GetOwner(), SOUNDENT_CHANNEL_WEAPON);

	// player "shoot" animation
	player->SetAnimation(PLAYER_ATTACK1);

	// Decrease ammo
#ifdef DARKINTERVAL // DI doesn't decrement ammo in godmode
	if (!(GetOwner()->GetFlags() & FL_GODMODE))
#endif
	{
		player->RemoveAmmo(1, m_iSecondaryAmmoType);
	}

	// Can shoot again immediately
	m_flNextPrimaryAttack = CURTIME + 0.5f;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextSecondaryAttack = CURTIME + 1.0f;

	// Register a muzzleflash for the AI.
	player->SetMuzzleFlashTime(CURTIME + 0.5);

	m_iSecondaryAttacks++;
	gamestats->Event_WeaponFired(player, false, GetClassname());
}

#define	COMBINE_MIN_GRENADE_CLEAR_DIST 256

int CWeaponSMG1::WeaponRangeAttack2Condition(float flDot, float flDist)
{
	CAI_BaseNPC *npcOwner = GetOwner()->MyNPCPointer();

	return COND_NONE;

	/*
	// --------------------------------------------------------
	// Assume things haven't changed too much since last time
	// --------------------------------------------------------
	if (CURTIME < m_next_grenade_check_float )
	return m_lastGrenadeCondition;
	*/

	// -----------------------
	// If moving, don't check.
	// -----------------------
	if (npcOwner->IsMoving())
		return COND_NONE;

	CBaseEntity *pEnemy = npcOwner->GetEnemy();

	if (!pEnemy)
		return COND_NONE;

	Vector vecEnemyLKP = npcOwner->GetEnemyLKP();
	if (!(pEnemy->GetFlags() & FL_ONGROUND) && pEnemy->GetWaterLevel() == 0 && vecEnemyLKP.z > (GetAbsOrigin().z + WorldAlignMaxs().z))
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to 
		// be grenaded.
		// don't throw grenades at anything that isn't on the ground!
		return COND_NONE;
	}

	// --------------------------------------
	//  Get target vector
	// --------------------------------------
	Vector vecTarget;
	if (random->RandomInt(0, 1))
	{
		// magically know where they are
		vecTarget = pEnemy->WorldSpaceCenter();
	}
	else
	{
		// toss it to where you last saw them
		vecTarget = vecEnemyLKP;
	}
	// vecTarget = m_vecEnemyLKP + (pEnemy->BodyTarget( GetLocalOrigin() ) - pEnemy->GetLocalOrigin());
	// estimate position
	// vecTarget = vecTarget + pEnemy->m_vecVelocity * 2;


	if ((vecTarget - npcOwner->GetLocalOrigin()).Length2D() <= COMBINE_MIN_GRENADE_CLEAR_DIST)
	{
		// crap, I don't want to blow myself up
		m_next_grenade_check_float = CURTIME + 1; // one full second.
		return (COND_NONE);
	}

	// ---------------------------------------------------------------------
	// Are any friendlies near the intended grenade impact area?
	// ---------------------------------------------------------------------
	CBaseEntity *pTarget = NULL;

	while ((pTarget = gEntList.FindEntityInSphere(pTarget, vecTarget, COMBINE_MIN_GRENADE_CLEAR_DIST)) != NULL)
	{
		//Check to see if the default relationship is hatred, and if so intensify that
		if (npcOwner->IRelationType(pTarget) == D_LI)
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_next_grenade_check_float = CURTIME + 1; // one full second.
			return (COND_WEAPON_BLOCKED_BY_FRIEND);
		}
	}

	// ---------------------------------------------------------------------
	// Check that throw is legal and clear
	// ---------------------------------------------------------------------
	// FIXME: speed is based on difficulty...

	Vector vecToss = VecCheckThrow(this, npcOwner->GetLocalOrigin() + Vector(0, 0, 60), vecTarget, 600.0, 0.5);
	if (vecToss != vec3_origin)
	{
		m_toss_velocity_vec3 = vecToss;

		// don't check again for a while.
		// JAY: HL1 keeps checking - test?
		//m_next_grenade_check_float = CURTIME;
		m_next_grenade_check_float = CURTIME + 0.3; // 1/3 second.
		return COND_CAN_RANGE_ATTACK2;
	}
	else
	{
		// don't check again for a while.
		m_next_grenade_check_float = CURTIME + 1; // one full second.
		return COND_WEAPON_SIGHT_OCCLUDED;
	}
}

#ifndef DARKINTERVAL
const WeaponProficiencyInfo_t *CWeaponSMG1::GetProficiencyValues()
{
static WeaponProficiencyInfo_t proficiencyTable[] =
{
{ 7.0,		0.75 },
{ 5.00,		0.75 },
{ 10.0 / 3.0, 0.75 },
{ 5.0 / 3.0,	0.75 },
{ 1.00,		1.0 },
};
COMPILE_TIME_ASSERT(ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);
return proficiencyTable;
}
#endif

//-----------------------------------------------------------------------------
// Save/restore
//-----------------------------------------------------------------------------
BEGIN_DATADESC(CWeaponSMG1)
	DEFINE_FIELD(m_toss_velocity_vec3, FIELD_VECTOR),
	DEFINE_FIELD(m_next_grenade_check_float, FIELD_TIME),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeaponSMG1, DT_WeaponSMG1)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_smg1, CWeaponSMG1);
PRECACHE_WEAPON_REGISTER(weapon_smg1);
#endif

#ifdef DARKINTERVAL
#if defined(SMG1_VIRT)
class CWeaponSMG1Virt : public CHLMachineGun
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS(CWeaponSMG1Virt, CHLMachineGun);

	CWeaponSMG1Virt() {};

	DECLARE_SERVERCLASS();

	void	Precache(void)
	{
		BaseClass::Precache();
	}

	virtual void	Equip(CBaseCombatCharacter *pOwner)
	{
		pOwner->GiveAmmo(255, GetPrimaryAmmoType(), true);
		BaseClass::Equip(pOwner);
	}

	void	AddViewKick(void)
	{
		//Get the view kick
		CBasePlayer *player = ToBasePlayer(GetOwner());

		if (player == NULL)
			return;

		DoMachineGunKick(player, 0.5f, 1.0f, m_fFireDuration, 2.0f);
	}

	bool CWeaponSMG1Virt::Reload(void)
	{
		bool fRet;
		fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
		if (fRet)
		{
			GetOwner()->GiveAmmo(255, GetPrimaryAmmoType(), true);
		}
		return fRet;
	}

	void	PrimaryAttack(void)
	{
		BaseClass::PrimaryAttack();
	}

	void	SecondaryAttack(void)
	{
		PrimaryAttack();
	}

	float	GetFireRate(void) { return 0.075f; }	// 13.3hz
	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	virtual const Vector& GetBulletSpread(void)
	{
		static const Vector cone = VECTOR_CONE_5DEGREES;
		return cone;
	}

	void BreakWeapon(void)
	{
		const char *name = "weapon_broken";
		SetName(AllocPooledString(name));
		GetOwner()->Weapon_Drop(this, NULL, NULL);
		UTIL_Remove(this);
	}
};

//-----------------------------------------------------------------------------
// Save/restore
//-----------------------------------------------------------------------------
BEGIN_DATADESC(CWeaponSMG1Virt)
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeaponSMG1Virt, DT_WeaponSMG1Virt)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_smg1virt, CWeaponSMG1Virt);
PRECACHE_WEAPON_REGISTER(weapon_smg1virt);
#endif
#endif // DARKINTERVAL
//====================================
// Flare (env_flare) (HL2)
//====================================
#if defined(FLARE)

#define	SF_FLARE_NO_DLIGHT	0x00000001
#define	SF_FLARE_NO_SMOKE	0x00000002
#define	SF_FLARE_INFINITE	0x00000004
#define	SF_FLARE_START_OFF	0x00000008

#define	FLARE_DURATION		30.0f
#define FLARE_DECAY_TIME	10.0f
#define FLARE_BLIND_TIME	6.0f
#ifdef DARKINTERVAL
#define	FLARE_LAUNCH_SPEED	1500
#endif
CFlare::CFlare()
{
	m_flScale		= 1.0f;
	m_nBounces		= 0;
	m_bFading		= false;
	m_bLight		= true;
#ifndef DARKINTERVAL // old client-side smoke trail has been replaced with a smoke trail entity
	m_bSmoke	= true;
#endif
	m_flNextDamage	= CURTIME;
	m_lifeState		= LIFE_ALIVE;
	m_iHealth		= 100;
	m_bPropFlare	= false;
	m_bInActiveList = false;
	m_pNextFlare	= NULL;
#ifdef DARKINTERVAL
	m_pFlareTrail	= NULL;
	m_pFlareGlow	= NULL;
	m_pStartSmokeTrail = NULL;
	// so flares interact with fire system and can add heat to them
	m_flMaxHeatLevel		= 0;
	m_flCurrentHeatLevel	= 0;
	m_flHeatRadius			= 64;
#endif
}

CFlare::~CFlare()
{
#ifdef DARKINTERVAL
	if (m_pBurnSound != NULL)
#endif
	{
		CSoundEnvelopeController::GetController().SoundDestroy(m_pBurnSound);
		m_pBurnSound = NULL;
	}

	RemoveFromActiveFlares();
}
#ifdef DARKINTERVAL // so flares interact with fire system and can add heat to them
CFlare *CFlare::Create(Vector vecOrigin,
	QAngle vecAngles,
	CBaseEntity *pOwner,
	float lifetime, float activationDelay, float goalHeatLevel)
#else
	CFlare *CFlare::Create(Vector vecOrigin, QAngle vecAngles, CBaseEntity *pOwner, float lifetime)
#endif
{
	CFlare *pFlare = (CFlare *)CreateEntityByName("env_flare");

	if (pFlare == NULL)
		return NULL;

	UTIL_SetOrigin(pFlare, vecOrigin);

	pFlare->SetLocalAngles(vecAngles);
	pFlare->Spawn();
	pFlare->SetTouch(&CFlare::FlareTouch);
	pFlare->SetThink(&CFlare::FlareThink);
	
	//Start up the flare
	pFlare->Start(lifetime);

	//Don't start sparking immediately
#ifdef DARKINTERVAL 
	pFlare->SetNextThink(CURTIME + activationDelay);
#else
	pFlare->SetNextThink(CURTIME + 0.5f);
#endif

#ifdef DARKINTERVAL // so flares interact with fire system and can add heat to them
	// Goal heat level
	pFlare->SetGoalHeatLevel(goalHeatLevel);
#endif
	//Burn out time
	pFlare->m_flTimeBurnOut = CURTIME + lifetime;

	pFlare->RemoveSolidFlags(FSOLID_NOT_SOLID);
	pFlare->AddSolidFlags(FSOLID_NOT_STANDABLE);

	pFlare->SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE);

	pFlare->SetOwnerEntity(pOwner);
	pFlare->m_pOwner = pOwner;

	return pFlare;
}
	
void CFlare::Precache(void)
{
#ifdef DARKINTERVAL
	PrecacheModel("sprites/blueflare1.vmt");
	PrecacheModel("sprites/bluelaser1.vmt");
#endif
	PrecacheModel("models/weapons/flare.mdl");
	PrecacheScriptSound("Weapon_FlareGun.Burn");
	// FIXME: needed to precache the fire model.  Shouldn't have to do this.
	UTIL_PrecacheOther("_firesmoke");
}

void CFlare::Spawn(void)
{
	Precache();
	SetModel("models/weapons/flare.mdl");

	UTIL_SetSize(this, Vector(-2, -2, -2), Vector(2, 2, 2));

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_SOLID);

	SetMoveType(MOVETYPE_NONE);
	SetFriction(0.6f);
	SetGravity(UTIL_ScaleForGravity(400));
	m_flTimeBurnOut = CURTIME + 30;

	AddEffects(EF_NOSHADOW | EF_NORECEIVESHADOW);
#ifndef DARKINTERVAL
	if (m_spawnflags & SF_FLARE_NO_DLIGHT)
	{
	m_bLight = false;
	}

	if (m_spawnflags & SF_FLARE_NO_SMOKE)
	{
		m_bSmoke = false;
	}

	if (HasSpawnFlags(SF_FLARE_INFINITE))
	{
		m_flTimeBurnOut = -1.0f;
	}

	if (m_spawnflags & SF_FLARE_START_OFF)
	{
		AddEffects(EF_NODRAW);
	}
	else
	{
		// Start up the eye glow
		m_pFlareGlow = CSprite::SpriteCreate("sprites/blueflare1.vmt", GetAbsOrigin(), false);

		if (m_pFlareGlow != NULL)
		{
			m_pFlareGlow->FollowEntity(this);
			m_pFlareGlow->SetTransparency(kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation);
			m_pFlareGlow->SetScale(0.2f);
			m_pFlareGlow->SetGlowProxySize(4.0f);
		}

		// Start up the eye trail
		m_pFlareTrail = CSpriteTrail::SpriteTrailCreate("sprites/bluelaser1.vmt", GetAbsOrigin(), false);

		if (m_pFlareTrail != NULL)
		{
			m_pFlareTrail->FollowEntity(this);
			m_pFlareTrail->SetTransparency(kRenderTransAdd, 255, 0, 0, 0, kRenderFxNone);
			m_pFlareTrail->SetStartWidth(8.0f);
			m_pFlareTrail->SetEndWidth(2.0f);
			m_pFlareTrail->SetLifeTime(0.8f);
		}

		// Start up dark smoke trail
		m_pStartSmokeTrail = SmokeTrail::CreateSmokeTrail();

		if (m_pStartSmokeTrail != NULL)
		{
			m_pStartSmokeTrail->m_SpawnRate = 32;
			m_pStartSmokeTrail->m_ParticleLifetime = 1;
			m_pStartSmokeTrail->m_StartColor.Init(0.1f, 0.1f, 0.1f);
			m_pStartSmokeTrail->m_EndColor.Init(0, 0, 0);
			m_pStartSmokeTrail->m_StartSize = 2;
			m_pStartSmokeTrail->m_EndSize = m_pStartSmokeTrail->m_StartSize * 4;
			m_pStartSmokeTrail->m_SpawnRadius = 2;
			m_pStartSmokeTrail->m_MinSpeed = 4;
			m_pStartSmokeTrail->m_MaxSpeed = 24;
			m_pStartSmokeTrail->m_Opacity = 0.2f;

			m_pStartSmokeTrail->SetLifetime(2.0f);
			m_pStartSmokeTrail->FollowEntity(this);
		}
	}
#else
	m_bLight = false;
#endif // DARKINTERVAL
	if (!HasSpawnFlags(SF_FLARE_START_OFF))
	{
		Start(m_flTimeBurnOut);
	}
	else
	{
		AddEffects(EF_NODRAW);
	}

	AddFlag(FL_VISIBLE_TO_NPCS);
}

int CFlare::Restore(IRestore &restore)
{
	int result = BaseClass::Restore(restore);

	if (HasSpawnFlags(m_spawnflags & SF_FLARE_NO_DLIGHT))
	{
		m_bLight = false;
	}
#ifndef DARKINTERVAL
	if (m_spawnflags & SF_FLARE_NO_SMOKE)
	{
		m_bSmoke = false;
	}
#endif
	return result;
}

void CFlare::Activate(void)
{
	BaseClass::Activate();

	// Start the burning sound if we're already on
	if (!HasSpawnFlags(SF_FLARE_START_OFF))
	{
		StartBurnSound();
	}
}

void CFlare::StartBurnSound(void)
{
	if (m_pBurnSound == NULL)
	{
		CPASAttenuationFilter filter(this);
		m_pBurnSound = CSoundEnvelopeController::GetController().SoundCreate(
			filter, entindex(), CHAN_WEAPON, "Weapon_FlareGun.Burn", 3.0f);
	}
}
	
void CFlare::Start(float lifeTime)
{
	StartBurnSound();

	if (m_pBurnSound != NULL)
	{
		CSoundEnvelopeController::GetController().Play(m_pBurnSound, 0.0f, 60);
		CSoundEnvelopeController::GetController().SoundChangeVolume(m_pBurnSound, 0.8f, 2.0f);
		CSoundEnvelopeController::GetController().SoundChangePitch(m_pBurnSound, 100, 2.0f);
	}

	if (lifeTime > 0)
	{
		m_flTimeBurnOut = CURTIME + lifeTime;
	}
	else
	{
		m_flTimeBurnOut = -1.0f;
	}

	RemoveEffects(EF_NODRAW);

	SetThink(&CFlare::FlareThink);
	SetNextThink(CURTIME + 0.1f);
}

void CFlare::Die(float fadeTime)
{
	m_flTimeBurnOut = CURTIME + fadeTime;
	SetThink(&CFlare::FlareThink);
	SetNextThink(CURTIME + 0.1f);
}

void CFlare::Launch(const Vector &direction, float speed)
{
	// Make sure we're visible
	if (HasSpawnFlags(SF_FLARE_INFINITE))
	{
		Start(-1);
	}
	else
	{
		Start(8.0f);
	}

	SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE);

	// Punch our velocity towards our facing
	SetAbsVelocity(direction * speed);

	SetGravity(1.0f);
}

void CFlare::FlareTouch(CBaseEntity *pOther)
{
	Assert(pOther);
	if (!pOther->IsSolid())
		return;

	if ((m_nBounces < 10) && (GetWaterLevel() < 1))
	{
		// Throw some real chunks here
		g_pEffects->Sparks(GetAbsOrigin());
	}
#ifdef DARKINTERVAL
	//If in contact with an oil barrel, ignite it.
	if (pOther && strcmp((pOther->GetEntityName().ToCStr()), "ignitable_prop") == 0) // DI todo - replace hardcoded check with better one
	{
		CBaseAnimating *pAnim;

		pAnim = dynamic_cast<CBaseAnimating*>(pOther);
		if (pAnim)
		{
			pAnim->Ignite(5.0f);
		}

		Vector vecNewVelocity = GetAbsVelocity();
		vecNewVelocity *= 0.1f;
		SetAbsVelocity(vecNewVelocity);

		SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE);
		SetGravity(1.0f);

		Die(0.5);

		return;
	}

	//If the flare hit a person or NPC, do damage here.
	if (pOther && pOther->IsNPC())
	{
		CAI_BaseNPC *pBCC = pOther->MyNPCPointer();

		// DI HACK, prevents flare dmg from disrupting sequences
		if (pBCC->IsInAScript())
		{
			return;
		}

		//Damage is a function of how fast the flare is flying.
		int iDamage = GetAbsVelocity().Length() / 50.0f;

		if (iDamage < 5)
		{
			//Clamp minimum damage
			iDamage = 5;
		}

		//Use m_pOwner, not GetOwnerEntity()
		pOther->TakeDamage(CTakeDamageInfo(this, m_pOwner, iDamage, (DMG_BULLET | DMG_BURN)));
		m_flNextDamage = CURTIME + 1.0f;

		CBaseAnimating *pAnim;

		pAnim = dynamic_cast<CBaseAnimating*>(pOther);
		if (pAnim)
		{
			pAnim->Ignite(15.0f);
		}

		Vector vecNewVelocity = GetAbsVelocity();
		vecNewVelocity *= 0.1f;
		SetAbsVelocity(vecNewVelocity);

		SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE);
		SetGravity(1.0f);

		Die(0.5);

		return;
	}
#else
	if ( pOther && pOther->m_takedamage )
	{
		/*
		The Flare is the iRifle round right now. No damage, just ignite. (sjb)

		//Damage is a function of how fast the flare is flying.
		int iDamage = GetAbsVelocity().Length() / 50.0f;

		if ( iDamage < 5 )
		{
		//Clamp minimum damage
		iDamage = 5;
		}

		//Use m_pOwner, not GetOwnerEntity()
		pOther->TakeDamage( CTakeDamageInfo( this, m_pOwner, iDamage, (DMG_BULLET|DMG_BURN) ) );
		m_flNextDamage = CURTIME + 1.0f;
		*/

		CBaseAnimating *pAnim;

		pAnim = dynamic_cast<CBaseAnimating*>( pOther );
		if ( pAnim )
		{
			pAnim->Ignite(30.0f);
		}

		Vector vecNewVelocity = GetAbsVelocity();
		vecNewVelocity *= 0.1f;
		SetAbsVelocity(vecNewVelocity);

		SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE);
		SetGravity(1.0f);


		Die(0.5);

		return;
	}
#endif // DARKINTERVAL
	else
	{
		// hit the world, check the material type here, see if the flare should stick.
		trace_t tr;
		tr = CBaseEntity::GetTouchTrace();

		//Only do this on the first bounce
		if (m_nBounces == 0)
		{
			surfacedata_t *pdata = physprops->GetSurfaceData(tr.surface.surfaceProps);

			if (pdata != NULL)
			{
#ifdef DARKINTERVAL
				//Only embed into soft enough materials - concrete, brick, dirt, wood
				if ((pdata->game.material == CHAR_TEX_ALIENFLESH)
					|| (pdata->game.material == CHAR_TEX_ANTLION)
					|| (pdata->game.material == CHAR_TEX_BLOODYFLESH)
					|| (pdata->game.material == CHAR_TEX_CONCRETE)
					|| (pdata->game.material == CHAR_TEX_DIRT)
					|| (pdata->game.material == CHAR_TEX_EGGSHELL)
					|| (pdata->game.material == CHAR_TEX_FLESH)
					|| (pdata->game.material == CHAR_TEX_HYDRA)
					|| (pdata->game.material == CHAR_TEX_ICE)
					|| (pdata->game.material == CHAR_TEX_PLASTIC)
					|| (pdata->game.material == CHAR_TEX_SAND)
					|| (pdata->game.material == CHAR_TEX_SLOSH)
					|| (pdata->game.material == CHAR_TEX_SNOW)
					|| (pdata->game.material == CHAR_TEX_WOOD)
					)
#endif
				{
					Vector	impactDir = (tr.endpos - tr.startpos);
					VectorNormalize(impactDir);

					float	surfDot = tr.plane.normal.Dot(impactDir);

					//Do not stick to ceilings or on shallow impacts
					if ((tr.plane.normal.z > -0.5f) && (surfDot < -0.9f))
					{
						RemoveSolidFlags(FSOLID_NOT_SOLID);
						AddSolidFlags(FSOLID_TRIGGER);
						UTIL_SetOrigin(this, tr.endpos + (tr.plane.normal * 2.0f));
						SetAbsVelocity(vec3_origin);
						SetMoveType(MOVETYPE_NONE);

						SetTouch(&CFlare::FlareBurnTouch);
#ifndef DARKINTERVAL // disabled because that decal looks broken half the time in modern Source // DI todo - fix
						int index = decalsystem->GetDecalIndexForName("SmallScorch");
						if (index >= 0)
						{
							CBroadcastRecipientFilter filter;
							te->Decal(filter, 0.0, &tr.endpos, &tr.startpos, ENTINDEX(tr.m_pEnt), tr.hitbox, index);
						}
#endif
						CPASAttenuationFilter filter2(this, "Flare.Touch");
						EmitSound(filter2, entindex(), "Flare.Touch");

						return;
					}
				}
			}
		}

		//Scorch decal
		if (GetAbsVelocity().LengthSqr() > (250 * 250))
		{
			int index = decalsystem->GetDecalIndexForName("FadingScorch");
			if (index >= 0)
			{
				CBroadcastRecipientFilter filter;
				te->Decal(filter, 0.0, &tr.endpos, &tr.startpos, ENTINDEX(tr.m_pEnt), tr.hitbox, index);
			}
		}

		// Change our flight characteristics
		SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE);
		SetGravity(UTIL_ScaleForGravity(640));

		m_nBounces++;

		//After the first bounce, smacking into whoever fired the flare is fair game
#ifndef DARKINTERVAL // don't want this
		SetOwnerEntity( this );	
#endif
		// Slow down
		Vector vecNewVelocity = GetAbsVelocity();
		vecNewVelocity.x *= 0.8f;
		vecNewVelocity.y *= 0.8f;
		SetAbsVelocity(vecNewVelocity);

		//Stopped?
		if (GetAbsVelocity().Length() < 64.0f)
		{
			SetAbsVelocity(vec3_origin);
			SetMoveType(MOVETYPE_NONE);
			RemoveSolidFlags(FSOLID_NOT_SOLID);
			AddSolidFlags(FSOLID_TRIGGER);
			SetTouch(&CFlare::FlareBurnTouch);
		}
	}
}

void CFlare::FlareBurnTouch(CBaseEntity *pOther)
{
	if (pOther && !pOther->IsPlayer() && pOther->m_takedamage && (m_flNextDamage < CURTIME))
	{
		pOther->TakeDamage(CTakeDamageInfo(this, m_pOwner, 1, (DMG_BULLET | DMG_BURN)));
		m_flNextDamage = CURTIME + 1.0f;
	}
}

void CFlare::FlareThink(void)
{
	float	deltaTime = (m_flTimeBurnOut - CURTIME);

	if (!m_bInActiveList && ((deltaTime > FLARE_BLIND_TIME) || (m_flTimeBurnOut == -1.0f)))
	{
		AddToActiveFlares();
	}

	if (m_flTimeBurnOut != -1.0f)
	{
		//Fading away
		if ((deltaTime <= FLARE_DECAY_TIME) && (m_bFading == false))
		{
			m_bFading = true;
			CSoundEnvelopeController::GetController().SoundChangePitch(m_pBurnSound, 60, deltaTime);
			CSoundEnvelopeController::GetController().SoundFadeOut(m_pBurnSound, deltaTime);

			if (m_flCurrentHeatLevel > 0.1f)
				m_flCurrentHeatLevel *= 0.7f;
			else
				m_flCurrentHeatLevel = 0;
		}

		// if flare is no longer bright, remove it from active flare list
		if (m_bInActiveList && (deltaTime <= FLARE_BLIND_TIME))
		{
			RemoveFromActiveFlares();
		}

		//Burned out
		if (m_flTimeBurnOut < CURTIME)
		{
			m_flCurrentHeatLevel = 0;
			UTIL_Remove(this);
			return;
		}
	}
#ifdef DARKINTERVAL
	Vector vecVelocity = GetAbsVelocity();
	// draw fancy effects
	if (m_pFlareGlow)
	{
		m_pFlareGlow->SetTransparency(kRenderGlow, 255, 50, 50, 255, kRenderFxNoDissipation);
		m_pFlareGlow->SetScale(RandomFloat(0.8f, 1.2f));
	}
	if (m_pFlareTrail && vecVelocity.Length() > 60)
	{
		m_pFlareTrail->SetTransparency(kRenderTransAdd, 255, 0, 0, 255, kRenderFxNone);
		m_pFlareTrail->SetStartWidth(RandomFloat(10.0f, 16.0f));
		m_pFlareTrail->SetEndWidth(RandomFloat(2.5f, 4.0f));
	}

	if (!(m_spawnflags & SF_FLARE_NO_DLIGHT))
	{
		m_bLight = true;
	}

	// old client-side smoke trail has been replaced with a smoke trail entity
	if (!(m_spawnflags & SF_FLARE_NO_SMOKE))
	{
		// change smoke trail appearance
		if (m_pStartSmokeTrail)
		{
			m_pStartSmokeTrail->m_SpawnRate = 48;
			m_pStartSmokeTrail->m_ParticleLifetime = 2;
			m_pStartSmokeTrail->m_StartColor.Init(1.0f, 0.3f, 0.2f);
			m_pStartSmokeTrail->m_EndColor.Init(0, 0, 0);
			m_pStartSmokeTrail->m_StartSize = 16;
			m_pStartSmokeTrail->m_EndSize = m_pStartSmokeTrail->m_StartSize * 4;
			m_pStartSmokeTrail->m_SpawnRadius = 4;
			m_pStartSmokeTrail->SetLifetime(2.0f);
			m_pStartSmokeTrail->FollowEntity(this);
		}
	}
#endif
	//Act differently underwater
	if (GetWaterLevel() > 1)
	{
		UTIL_Bubbles(GetAbsOrigin() + Vector(-2, -2, -2), GetAbsOrigin() + Vector(2, 2, 2), 1);
#ifndef DARKINTERVAL  // old client-side smoke trail has been replaced with a smoke trail entity
		m_bSmoke = false;
#endif
	}
	else
	{
		//Shoot sparks
#ifdef DARKINTERVAL
		if (GetGroundEntity())
		{
			if (random->RandomInt(0, 3) == 1)
#else
		if ( random->RandomInt(0, 8) == 1 )
		{
#endif
			{
				g_pEffects->Sparks(GetAbsOrigin());
			}
		}
	}
#ifdef DARKINTERVAL // so flares interact with fire system and can add heat to them
	if (m_flMaxHeatLevel > 0 && m_flCurrentHeatLevel < m_flMaxHeatLevel)
	{
		m_flCurrentHeatLevel += m_flMaxHeatLevel * 0.2f;
	}
	if (m_flCurrentHeatLevel > m_flMaxHeatLevel) m_flCurrentHeatLevel = m_flMaxHeatLevel;

	CFire *pFires[128];
	int fireCount = FireSystem_GetFiresInSphere(pFires, ARRAYSIZE(pFires), false, GetAbsOrigin(), m_flHeatRadius);

	for (int i = 0; i < fireCount; i++)
	{
		pFires[i]->AddHeat(m_flCurrentHeatLevel * 0.05f); // multiplied by the NextThink
	}

	SetGravity(UTIL_ScaleForGravity(400));
#endif
	//Next update
	SetNextThink(CURTIME + 0.05f);
}

void CFlare::InputStart(inputdata_t &inputdata)
{
	Start(inputdata.value.Float());
}

void CFlare::InputDie(inputdata_t &inputdata)
{
	Die(inputdata.value.Float());
}

void CFlare::InputLaunch(inputdata_t &inputdata)
{
	Vector	direction;
	AngleVectors(GetAbsAngles(), &direction);

	float	speed = inputdata.value.Float();

	if (speed == 0)
	{
		speed = FLARE_LAUNCH_SPEED;
	}

	Launch(direction, speed);
}

void CFlare::RemoveFromActiveFlares(void)
{
	CFlare *pFlare;
	CFlare *pPrevFlare;

	if (!m_bInActiveList)
		return;

	pPrevFlare = NULL;
	for (pFlare = CFlare::activeFlares; pFlare != NULL; pFlare = pFlare->m_pNextFlare)
	{
		if (pFlare == this)
		{
			if (pPrevFlare)
			{
				pPrevFlare->m_pNextFlare = m_pNextFlare;
			}
			else
			{
				activeFlares = m_pNextFlare;
			}
			break;
		}
		pPrevFlare = pFlare;
	}

	m_pNextFlare = NULL;
	m_bInActiveList = false;
}

void CFlare::AddToActiveFlares(void)
{
	if (!m_bInActiveList)
	{
		m_pNextFlare = CFlare::activeFlares;
		CFlare::activeFlares = this;
		m_bInActiveList = true;
	}
}

CFlare *CFlare::activeFlares = NULL;
#ifdef DARKINTERVAL // so flares interact with fire system and can add heat to them
CBaseEntity *CreateFlare(Vector vOrigin, QAngle Angles, CBaseEntity *pOwner, float flDuration, float goalHeatLevel)
{
	CFlare *pFlare = CFlare::Create(vOrigin, Angles, pOwner, flDuration, 0.1f, goalHeatLevel);
#else
CBaseEntity *CreateFlare(Vector vOrigin, QAngle Angles, CBaseEntity *pOwner, float flDuration)
{
	CFlare *pFlare = CFlare::Create(vOrigin, Angles, pOwner, flDuration);
#endif
	if (pFlare)
	{
		pFlare->m_bPropFlare = true;
	}

	return pFlare;
}

void KillFlare(CBaseEntity *pOwnerEntity, CBaseEntity *pEntity, float flKillTime)
{
	CFlare *pFlare = dynamic_cast< CFlare *>(pEntity);

	if (pFlare)
	{
		float flDieTime = (pFlare->m_flTimeBurnOut - CURTIME) - flKillTime;

		if (flDieTime > 1.0f)
		{
			pFlare->Die(flDieTime);
			pOwnerEntity->SetNextThink(CURTIME + flDieTime + 3.0f);
		}
	}
}

BEGIN_DATADESC(CFlare)
	DEFINE_FIELD(m_pOwner, FIELD_CLASSPTR),
	DEFINE_FIELD(m_nBounces, FIELD_INTEGER),
	DEFINE_FIELD(m_flTimeBurnOut, FIELD_TIME),
	DEFINE_KEYFIELD(m_flScale, FIELD_FLOAT, "scale"),
	DEFINE_KEYFIELD(m_flDuration, FIELD_FLOAT, "duration"),
#ifdef DARKINTERVAL // so flares interact with fire system and can add heat to them
	DEFINE_KEYFIELD(m_flMaxHeatLevel, FIELD_FLOAT, "goalheat"),
	DEFINE_KEYFIELD(m_flHeatRadius, FIELD_FLOAT, "fireradius"),
	DEFINE_FIELD(m_flCurrentHeatLevel, FIELD_TIME),
#endif
	DEFINE_FIELD(m_flNextDamage, FIELD_TIME),
	DEFINE_SOUNDPATCH(m_pBurnSound),
	DEFINE_FIELD(m_bFading, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bLight, FIELD_BOOLEAN),
#ifndef DARKINTERVAL // old client-side smoke trail has been replaced with a smoke trail entity
	DEFINE_FIELD(m_bSmoke, FIELD_BOOLEAN),
#endif
	DEFINE_FIELD(m_bPropFlare, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bInActiveList, FIELD_BOOLEAN),
	DEFINE_FIELD(m_pNextFlare, FIELD_CLASSPTR),
#ifdef DARKINTERVAL
	DEFINE_FIELD(m_pFlareGlow, FIELD_EHANDLE),
	DEFINE_FIELD(m_pFlareTrail, FIELD_EHANDLE),
	DEFINE_FIELD(m_pStartSmokeTrail, FIELD_EHANDLE),
#endif
	//Input functions
	DEFINE_INPUTFUNC(FIELD_FLOAT, "Start", InputStart),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "Die", InputDie),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "Launch", InputLaunch),
#ifdef DARKINTERVAL // so flares interact with fire system and can add heat to them
	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetGoalHeatLevel", InputSetGoalHeatLevel),
#endif
	// Function Pointers
	DEFINE_FUNCTION(FlareTouch),
	DEFINE_FUNCTION(FlareBurnTouch),
	DEFINE_FUNCTION(FlareThink),
END_DATADESC()
//Data-tables
IMPLEMENT_SERVERCLASS_ST(CFlare, DT_Flare)
	SendPropFloat(SENDINFO(m_flTimeBurnOut), 0, SPROP_NOSCALE),
	SendPropFloat(SENDINFO(m_flScale), 0, SPROP_NOSCALE),
	SendPropInt(SENDINFO(m_bLight), 1, SPROP_UNSIGNED),
#ifndef DARKINTERVAL // old client-side smoke trail has been replaced with a smoke trail entity
	SendPropInt(SENDINFO(m_bSmoke), 1, SPROP_UNSIGNED),
#endif
	SendPropInt(SENDINFO(m_bPropFlare), 1, SPROP_UNSIGNED),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(env_flare, CFlare);
#endif

//====================================
// Flaregun (Dark Interval)
//====================================
#if defined(FLAREGUN)
class CFlaregun :public CBaseHLCombatWeapon
{
public:
	DECLARE_CLASS(CFlaregun, CBaseHLCombatWeapon);

	DECLARE_SERVERCLASS();

	void Precache(void)
	{
		BaseClass::Precache();

		PrecacheScriptSound("Flare.Touch");

		PrecacheScriptSound("Weapon_FlareGun.Burn");

		UTIL_PrecacheOther("env_flare");
	}

	void PrimaryAttack(void)
	{
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());

		if (pOwner == NULL)
			return;

		if (m_iClip1 <= 0)
		{
			SendWeaponAnim(ACT_VM_DRYFIRE);
			pOwner->m_flNextAttack = CURTIME + SequenceDuration();
			return;
		}
#ifdef DARKINTERVAL // DI doesn't decrement ammo in godmode
		if (!(GetOwner()->GetFlags() & FL_GODMODE))
#endif
		{
			m_iClip1 = m_iClip1 - 1;
		}

		SendWeaponAnim(ACT_VM_PRIMARYATTACK);
		pOwner->m_flNextAttack = CURTIME + 1;
#ifdef DARKINTERVAL
		CFlare *pFlare = CFlare::Create(pOwner->Weapon_TrueShootPosition_Back(), pOwner->EyeAngles(), pOwner, FLARE_DURATION, 1.0f, 32.0f); // 32 is goal heat level
#else
		CFlare *pFlare = CFlare::Create(pOwner->Weapon_ShootPosition(), pOwner->EyeAngles(), pOwner, FLARE_DURATION);
#endif
		if (pFlare == NULL)
			return;

		Vector forward;
		pOwner->EyeVectors(&forward);
#ifdef DARKINTERVAL
		pFlare->SetAbsVelocity(forward * 2000);
#else
		pFlare->SetAbsVelocity(forward * 1500);
#endif
		WeaponSound(SINGLE);
	}
#ifndef DARKINTERVAL // no secondary attack, existing old one considered too puny and impractical
	void SecondaryAttack(void)
	{
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());

		if ( pOwner == NULL )
			return;

		if ( m_iClip1 <= 0 )
		{
			SendWeaponAnim(ACT_VM_DRYFIRE);
			pOwner->m_flNextAttack = CURTIME + SequenceDuration();
			return;
		}

		m_iClip1 = m_iClip1 - 1;

		SendWeaponAnim(ACT_VM_PRIMARYATTACK);
		pOwner->m_flNextAttack = CURTIME + 1;

		CFlare *pFlare = CFlare::Create(pOwner->Weapon_ShootPosition(), pOwner->EyeAngles(), pOwner, FLARE_DURATION);

		if ( pFlare == NULL )
			return;

		Vector forward;
		pOwner->EyeVectors(&forward);

		pFlare->SetAbsVelocity(forward * 500);
		pFlare->SetGravity(1.0f);
		pFlare->SetFriction(0.85f);
		pFlare->SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE);

		WeaponSound(SINGLE);
	}
#endif // DARKINTERVAL

#ifdef DARKINTERVAL
	virtual bool Reload(void)
	{
		bool fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
		if (fRet)
		{
			WeaponSound(RELOAD);
		}
		return fRet;
	}
#endif
};
IMPLEMENT_SERVERCLASS_ST(CFlaregun, DT_Flaregun)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_flaregun, CFlaregun);
PRECACHE_WEAPON_REGISTER(weapon_flaregun);
#endif

//====================================
// Sniper rifle (Dark Interval)
// NPC only weapon for Combine Elites.
//====================================
#if defined(SNIPER_ELITE)
#include "npc_combine.h"
class CWeaponSniper : public CBaseHLCombatWeapon
{
public:
	DECLARE_CLASS(CWeaponSniper, CBaseHLCombatWeapon);

	CWeaponSniper();

	DECLARE_SERVERCLASS();

	void	ItemPostFrame(void);
	void	Precache(void);
	bool	Deploy(void)
	{
		m_pBeam = NULL;
		m_pBeam2 = NULL;
		m_pScopeGlow = NULL;
		return BaseClass::Deploy();
	};


	void    PrimaryAttack(void);
	void	DelayedAttack(void);

	const char *GetTracerType(void) { return "AR2Tracer"; }

	void	LaserOff(void);
	void	LaserOn(const Vector &vecTarget, const Vector &vecDeviance);
	bool	IsLaserOn(void)
	{
		return m_pBeam != NULL && m_pBeam2 != NULL;
	}

	void	AddViewKick(void);
	void	Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);
	void	Operator_FrameUpdate(CBaseCombatCharacter  *pOperator);

	float	GetFireRate(void) { return 3.0f; }
	int		GetMinBurst() { return 1; }
	int		GetMaxBurst() { return 1; }
	float	GetMinRestTime() { return 1.5; }
	float	GetMaxRestTime() { return 3.0; }

	bool	CanHolster(void);
	bool	Reload(void);
	void	Detach();

	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	Activity	GetPrimaryAttackActivity(void);

	void	DoImpactEffect(trace_t &tr, int nDamageType);

	virtual const Vector& GetBulletSpread(void)
	{
		static Vector cone;

		cone = VECTOR_CONE_PRECALCULATED;

		return cone;
	}

	const	WeaponProficiencyInfo_t *GetProficiencyValues();

private:
	CBeam	*m_pBeam;
	CBeam	*m_pBeam2; // it's a reversed beam that's there to draw the halo on the *sniper*, on the beginning of the beam
	CSprite *m_pScopeGlow;

protected:

	float	m_flDelayedFire;
	bool	m_bShotDelayed;

	DECLARE_ACTTABLE();
	DECLARE_DATADESC();
};

BEGIN_DATADESC(CWeaponSniper)

DEFINE_FIELD(m_flDelayedFire, FIELD_TIME),
DEFINE_FIELD(m_bShotDelayed, FIELD_BOOLEAN),
DEFINE_FIELD(m_pBeam, FIELD_CLASSPTR),
DEFINE_FIELD(m_pBeam2, FIELD_CLASSPTR),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeaponSniper, DT_WeaponSniper)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_sniper, CWeaponSniper);
PRECACHE_WEAPON_REGISTER(weapon_sniper);

acttable_t	CWeaponSniper::m_acttable[] =
{
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },	// FIXME: hook to shotgun unique

	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_AR2,		true },
	{ ACT_RELOAD,					ACT_RELOAD_SHOTGUN,					false },
	{ ACT_WALK,						ACT_WALK_RIFLE,						true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SHOTGUN,				true },

	// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SHOTGUN_RELAXED,		false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SHOTGUN_STIMULATED,	false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_SHOTGUN_AGITATED,		false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

																			// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
																			//End readiness activities

	{ ACT_WALK_AIM,					ACT_WALK_AIM_SHOTGUN,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,				true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,			true },
	{ ACT_RUN,						ACT_RUN_RIFLE,						true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_SHOTGUN,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,				true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,			true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SHOTGUN,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SHOTGUN_LOW,		true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SHOTGUN_LOW,				false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SHOTGUN,			false },
};

IMPLEMENT_ACTTABLE(CWeaponSniper);

short sSniperBeamHaloSprite;

CWeaponSniper::CWeaponSniper()
{
	m_fMinRange1 = 65;
	m_fMaxRange1 = 2048;
}

void CWeaponSniper::Precache(void)
{
	sSniperBeamHaloSprite = PrecacheModel("sprites/light_glow03.vmt");
	PrecacheModel("sprites/purplelaser1.vmt");
	BaseClass::Precache();
}

void CWeaponSniper::ItemPostFrame(void)
{
	BaseClass::ItemPostFrame();
}

Activity CWeaponSniper::GetPrimaryAttackActivity(void)
{
	return ACT_VM_PRIMARYATTACK;
}

void CWeaponSniper::DoImpactEffect(trace_t &tr, int nDamageType)
{
	CEffectData data;

	data.m_vOrigin = tr.endpos + (tr.plane.normal * 1.0f);
	data.m_vNormal = tr.plane.normal;

	DispatchEffect("AR2Impact", data);

	BaseClass::DoImpactEffect(tr, nDamageType);
}

void CWeaponSniper::DelayedAttack(void)
{
	CNPC_Combine *pSoldier;
	pSoldier = dynamic_cast<CNPC_Combine*>(GetOwner());
	if (!pSoldier)
		return;

	m_bShotDelayed = false;

	LaserOff();

	WeaponSound(SINGLE);

	Vector vecShootOrigin, vecShootDir;
	vecShootOrigin = pSoldier->Weapon_ShootPosition();

	// Fire!
	vecShootDir = pSoldier->GetActualShootTrajectory(vecShootOrigin);
	pSoldier->FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0);

	m_iClip1--;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextPrimaryAttack = CURTIME + 1.0f;
}

void CWeaponSniper::PrimaryAttack(void)
{
	// Only the player fires this way so we can cast
	CBasePlayer *player = ToBasePlayer(GetOwner());

	if (!player)
	{
		return;
	}

	if (m_iClip1 <= 0)
	{
		if (!m_bFireOnEmpty)
		{
			Reload();
		}
		else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(player, true, GetClassname());

	WeaponSound(SINGLE);
	player->DoMuzzleFlash();

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);
	player->SetAnimation(PLAYER_ATTACK1);

	m_flNextPrimaryAttack = CURTIME + 0.75;
	m_flNextSecondaryAttack = CURTIME + 0.75;

	if (!(GetOwner()->GetFlags() & FL_GODMODE))
	{
		m_iClip1--;
	}

	Vector vecSrc = player->Weapon_ShootPosition();
	Vector vecAiming = player->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);

	player->FireBullets(1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0);

	player->SetMuzzleFlashTime(CURTIME + 0.5);

	//Disorient the player
	player->ViewPunch(QAngle(-4, random->RandomFloat(-2, 2), 0));

	CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 600, 0.2, GetOwner());

	if (!m_iClip1 && player->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		player->SetSuitUpdate("HEVVOXNEW_AMMODEPLETED", true, 0);
	}
}

bool CWeaponSniper::CanHolster(void)
{
	if (m_bShotDelayed)
		return false;

	return BaseClass::CanHolster();
}

bool CWeaponSniper::Reload(void)
{
	if (m_bShotDelayed)
		return false;

	if (IsLaserOn()) LaserOff();

	return BaseClass::Reload();
}

void CWeaponSniper::Detach()
{
	LaserOff(); // detach is called, among other things, when the owner is killed
}

void CWeaponSniper::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_SHOTGUN_FIRE:
	{
		CNPC_Combine *pSoldier;
		pSoldier = dynamic_cast<CNPC_Combine*>(GetOwner());
		if (!pSoldier)
			return;

		m_bShotDelayed = true;

		// scope glint
		CEffectData data;
		
		WeaponSound(WPN_DOUBLE);

		Vector vecShootOrigin, vecShootDir;
		vecShootOrigin = pSoldier->Weapon_ShootPosition();
		vecShootDir = pSoldier->GetActualShootTrajectory(vecShootOrigin);
		VectorNormalizeFast(vecShootDir);

		LaserOn(vecShootOrigin + vecShootDir * 2048, Vector(1, 1, 1));

		VectorNormalizeFast(vecShootDir);

		m_flDelayedFire = CURTIME + 2.0f;
		m_flNextPrimaryAttack = CURTIME + 3.0f;
	}
	break;

	default:
		CBaseCombatWeapon::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

void CWeaponSniper::Operator_FrameUpdate(CBaseCombatCharacter *pOperator)
{
	BaseClass::Operator_FrameUpdate(pOperator);

	if (!pOperator || !pOperator->IsAlive()) LaserOff();

	CNPC_Combine *pSoldier;
	pSoldier = dynamic_cast<CNPC_Combine*>(pOperator);

	if (!pSoldier) return;

	if (pSoldier->GetCurrentActivity() != ACT_RANGE_ATTACK1)
	{
		LaserOff(); // failsafe if he starts moving or doing something else
		m_bShotDelayed = false;
		m_flNextPrimaryAttack = CURTIME + 1.0f;
		return;
	}

	// See if we need to fire off our round
	if (m_bShotDelayed)
	{
		Vector vecShootOrigin, vecShootDir;
		vecShootOrigin = pSoldier->Weapon_ShootPosition();
		vecShootDir = pSoldier->GetActualShootTrajectory(vecShootOrigin);

		trace_t tr;
		UTIL_TraceLine(vecShootOrigin, vecShootOrigin + vecShootDir * 8192, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		Vector gunPosition = GetOwner()->Weapon_ShootPosition();
		GetAttachment(LookupAttachment("laser"), gunPosition, NULL);

		m_pBeam->SetStartPos(tr.endpos);
		m_pBeam->SetEndPos(gunPosition);
		m_pBeam->RelinkBeam();

		m_pBeam2->SetStartPos(tr.endpos);
		m_pBeam2->SetEndPos(gunPosition);
		m_pBeam2->RelinkBeam();

		if (CURTIME > m_flDelayedFire)
			DelayedAttack();
	}
}

#define LASER_LEAD_DIST	64
void CWeaponSniper::LaserOn(const Vector &vecTarget, const Vector &vecDeviance)
{
	Assert(GetOwner() != NULL);

	if (!m_pBeam && !m_pBeam2 && !m_pScopeGlow)
	{
		m_pBeam = CBeam::BeamCreate("sprites/purplelaser1.vmt", 1.0f);
		m_pBeam->SetColor(255, 75, 75);

		m_pBeam2 = CBeam::BeamCreate("sprites/purplelaser1.vmt", 1.0f);
		m_pBeam2->SetColor(255, 75, 75);

		m_pScopeGlow = CSprite::SpriteCreate("sprites/blueflare1.vmt", GetAbsOrigin(), FALSE);
		m_pScopeGlow->SetTransparency(kRenderGlow, 255, 75, 75, 0, kRenderFxFlickerFast);
		m_pScopeGlow->SetAttachment(this, LookupAttachment("laser"));
		m_pScopeGlow->SetBrightness(100);
		m_pScopeGlow->SetScale(0.5);
	}
	else
	{
		// Beam seems to be on.
		//return;
	}

	// Don't aim right at the guy right now.
	Vector vecInitialAim;
	vecInitialAim = vecTarget;

	vecInitialAim.x += random->RandomFloat(-vecDeviance.x, vecDeviance.x);
	vecInitialAim.y += random->RandomFloat(-vecDeviance.y, vecDeviance.y);
	vecInitialAim.z += random->RandomFloat(-vecDeviance.z, vecDeviance.z);
	
	// The beam is backwards, sortof. The endpoint is the sniper. This is
	// so that the beam can be tapered to very thin where it emits from the sniper.

	Vector gunPosition = GetOwner()->Weapon_ShootPosition();
	GetAttachment(LookupAttachment("laser"), gunPosition, NULL);

	m_pBeam->SetBrightness(255);
	m_pBeam->SetNoise(0);
	m_pBeam->SetWidth(2.0f);
	m_pBeam->SetEndWidth(0.0f);
	m_pBeam->SetScrollRate(0);
	m_pBeam->SetFadeLength(0.1f);
	m_pBeam->SetBeamFlag(FBEAM_SHADEIN);
	m_pBeam->PointsInit(vecInitialAim, gunPosition);

	// The second beam is not backwards. It starts at the sniper to draw the halo around the rifle.
	m_pBeam2->SetBrightness(25);
	m_pBeam2->SetNoise(0);
	m_pBeam2->SetWidth(0.0f);
	m_pBeam2->SetEndWidth(0.0f);
	m_pBeam2->SetScrollRate(0);
	m_pBeam2->SetFadeLength(0);
	m_pBeam2->SetHaloTexture(sSniperBeamHaloSprite);
	m_pBeam2->SetHaloScale(0.3f);
	m_pBeam2->SetBeamFlag(FBEAM_SHADEOUT);
	m_pBeam2->PointsInit(gunPosition, vecInitialAim);
}

void CWeaponSniper::LaserOff(void)
{
	if (m_pBeam)
	{
		UTIL_Remove(m_pBeam);
		m_pBeam = NULL;
	}

	if (m_pBeam2)
	{
		UTIL_Remove(m_pBeam2);
		m_pBeam2 = NULL;
	}

	if (m_pScopeGlow)
	{
		UTIL_Remove(m_pScopeGlow);
		m_pScopeGlow = NULL;
	}
}

void CWeaponSniper::AddViewKick(void)
{
	CBasePlayer *player = ToBasePlayer(GetOwner());
	if (!player)
		return;

	QAngle	viewPunch;
	viewPunch.x = random->RandomFloat(-7.0, 7.0);
	viewPunch.y = random->RandomFloat(-2.0, 2.0);
	Vector forward;
	AngleVectors(EyeAngles(), &forward, NULL, NULL);
	player->ApplyAbsVelocityImpulse(forward * -30); // phys. punch the player

	viewPunch.z = 0.0f;
	player->ViewPunch(viewPunch); // do the punch
}

const WeaponProficiencyInfo_t *CWeaponSniper::GetProficiencyValues()
{
	static WeaponProficiencyInfo_t proficiencyTable[] =
	{
		{ 7.0,		0.75 },
		{ 5.00,		0.75 },
		{ 3.0,		0.85 },
		{ 5.0 / 3.0,	0.75 },
		{ 1.00,		1.0 },
	};

	COMPILE_TIME_ASSERT(ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return proficiencyTable;
}
#endif

//====================================
// Plasma Rifle (Dark Interval)
// Similar to HL2's AR2, but shoots
// miniature combine balls as
// primary fire (automatic).
//====================================
#ifdef PLASMA_RIFLE
class CWeaponPlasma : public CHLMachineGun
{
public:
	DECLARE_CLASS(CWeaponPlasma, CHLMachineGun);

	CWeaponPlasma();

	DECLARE_SERVERCLASS();

	void	PrimaryAttack(void);

	void	ItemPostFrame(void);
	void	Precache(void);

	void	SecondaryAttack(void);
	void	DelayedAttack(void);

	const char *GetTracerType(void) { return "AR2Tracer"; }

	void	AddViewKick(void);

	void	FireNPCPrimaryAttack(CBaseCombatCharacter *pOperator, bool bUseWeaponAngles);
	void	FireNPCSecondaryAttack(CBaseCombatCharacter *pOperator, bool bUseWeaponAngles);
	void	Operator_ForceNPCFire(CBaseCombatCharacter  *pOperator, bool bSecondary);
	void	Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);

	int		GetMinBurst(void) { return 2; }
	int		GetMaxBurst(void) { return 5; }
	float	GetFireRate(void) { return 0.1f; }

	bool	CanHolster(void);
	bool	Reload(void);

	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	Activity	GetPrimaryAttackActivity(void);

	void	DoImpactEffect(trace_t &tr, int nDamageType);

	virtual const Vector& GetBulletSpread(void)
	{
		static Vector cone;

		cone = VECTOR_CONE_3DEGREES;

		return cone;
	}

	const WeaponProficiencyInfo_t *GetProficiencyValues();

protected:

	float					m_flDelayedFire;
	bool					m_bShotDelayed;
	int						m_nVentPose;

	DECLARE_ACTTABLE();
	DECLARE_DATADESC();
};

ConVar sk_weapon_plasma_alt_fire_radius("sk_weapon_plasma_alt_fire_radius", "10");
ConVar sk_weapon_plasma_alt_fire_duration("sk_weapon_plasma_alt_fire_duration", "5");
ConVar sk_weapon_plasma_alt_fire_mass("sk_weapon_plasma_alt_fire_mass", "150");

//=========================================================
//=========================================================

BEGIN_DATADESC(CWeaponPlasma)

DEFINE_FIELD(m_flDelayedFire, FIELD_TIME),
DEFINE_FIELD(m_bShotDelayed, FIELD_BOOLEAN),
//DEFINE_FIELD( m_nVentPose, FIELD_INTEGER ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeaponPlasma, DT_WeaponPlasma)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_plasma, CWeaponPlasma);
PRECACHE_WEAPON_REGISTER(weapon_plasma);

acttable_t	CWeaponPlasma::m_acttable[] =
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_AR2,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,				true },		// FIXME: hook to AR2 unique
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },		// FIXME: hook to AR2 unique
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG1,			true },		// FIXME: hook to AR2 unique

	{ ACT_WALK,						ACT_WALK_RIFLE,					true },

	// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG1_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG1_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

																			// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
																			//End readiness activities

	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN,						ACT_RUN_RIFLE,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_AR2,	false },
	{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },		// FIXME: hook to AR2 unique
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_AR2_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG1_LOW,		true },		// FIXME: hook to AR2 unique
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },
	//	{ ACT_RANGE_ATTACK2, ACT_RANGE_ATTACK_AR2_GRENADE, true },
};

IMPLEMENT_ACTTABLE(CWeaponPlasma);

CWeaponPlasma::CWeaponPlasma()
{
	m_fMinRange1 = 65;
	m_fMaxRange1 = 2048;

	m_fMinRange2 = 256;
	m_fMaxRange2 = 1024;

	m_shots_fired_int = 0;
	m_nVentPose = -1;

	m_can_altfire_underwater_boolean = false;
}

void CWeaponPlasma::Precache(void)
{
	BaseClass::Precache();

	UTIL_PrecacheOther("prop_combine_ball");
	UTIL_PrecacheOther("env_entity_dissolver");
}

//-----------------------------------------------------------------------------
// Purpose: Replaced function of the base class to have AR2 with
// projectile energy-ball bullets.
//-----------------------------------------------------------------------------
void CWeaponPlasma::PrimaryAttack(void)
{
	// Only the player fires this way so we can cast
	CBasePlayer *player = ToBasePlayer(GetOwner());
	if (!player)
		return;

	// Abort here to handle burst and auto fire modes
	if ((UsesClipsForAmmo1() && m_iClip1 == 0) || (!UsesClipsForAmmo1() && !player->GetAmmoCount(m_iPrimaryAmmoType)))
		return;

	m_shots_fired_int++;

	player->DoMuzzleFlash();

	// To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems, 
	// especially if the weapon we're firing has a really fast rate of fire.
	int iBulletsToFire = 0;
	float fireRate = GetFireRate();

	// MUST call sound before removing a round from the clip of a CHLMachineGun
	while (m_flNextPrimaryAttack <= CURTIME)
	{
		WeaponSound(SINGLE, m_flNextPrimaryAttack);
		m_flNextPrimaryAttack = m_flNextPrimaryAttack + fireRate;
		iBulletsToFire++;
	}

	// Make sure we don't fire more than the amount in the clip, if this weapon uses clips
	if (UsesClipsForAmmo1())
	{
		if (iBulletsToFire > m_iClip1)
			iBulletsToFire = m_iClip1;
		if (!(GetOwner()->GetFlags() & FL_GODMODE))
			m_iClip1 -= iBulletsToFire;
	}

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(player, true, GetClassname());

	// Fire the bullets
	Vector vecSrc = player->Weapon_TrueShootPosition_Front();
	vecSrc.z -= 2; // hacky offset. Better tie it to attachment in the future
	Vector vecAiming = player->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);
	Vector impactPoint = vecSrc + (vecAiming * MAX_TRACE_LENGTH);

	// Fire the bullets
	Vector vecVelocity = vecAiming * 1000.0f;

	// Fire the combine ball
	CreateCombineBall(vecSrc,
		vecVelocity,
		sk_weapon_plasma_alt_fire_radius.GetFloat() / 3,
		sk_weapon_plasma_alt_fire_mass.GetFloat() / 20,
		sk_weapon_plasma_alt_fire_duration.GetFloat() / 2,
		player, true);

	//Factor in the view kick
	AddViewKick();

	CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, player);

	if (!m_iClip1 && player->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		player->SetSuitUpdate("HEVVOXNEW_AMMODEPLETED", true, 0);
	}

	SendWeaponAnim(GetPrimaryAttackActivity());
	player->SetAnimation(PLAYER_ATTACK1);

	// Register a muzzleflash for the AI
	player->SetMuzzleFlashTime(CURTIME + 0.5);
}

//-----------------------------------------------------------------------------
// Purpose: Handle grenade detonate in-air (even when no ammo is left)
//-----------------------------------------------------------------------------
void CWeaponPlasma::ItemPostFrame(void)
{
	// See if we need to fire off our secondary round
	if (m_bShotDelayed && CURTIME > m_flDelayedFire)
	{
		DelayedAttack();
	}

	// Update our pose parameter for the vents
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if (pOwner)
	{
		CBaseViewModel *pVM = pOwner->GetViewModel();

		if (pVM)
		{
			if (m_nVentPose == -1)
			{
				m_nVentPose = pVM->LookupPoseParameter("VentPoses");
			}

			float flVentPose = RemapValClamped(m_shots_fired_int, 0, 5, 0.0f, 1.0f);
			pVM->SetPoseParameter(m_nVentPose, flVentPose);
		}
	}

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CWeaponPlasma::GetPrimaryAttackActivity(void)
{
	if (m_shots_fired_int < 2)
		return ACT_VM_PRIMARYATTACK;

	if (m_shots_fired_int < 3)
		return ACT_VM_RECOIL1;

	if (m_shots_fired_int < 4)
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &tr - 
//			nDamageType - 
//-----------------------------------------------------------------------------
void CWeaponPlasma::DoImpactEffect(trace_t &tr, int nDamageType)
{
	CEffectData data;

	data.m_vOrigin = tr.endpos + (tr.plane.normal * 1.0f);
	data.m_vNormal = tr.plane.normal;

	DispatchEffect("AR2Impact", data);

	BaseClass::DoImpactEffect(tr, nDamageType);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPlasma::DelayedAttack(void)
{
	m_bShotDelayed = false;

	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	// Deplete the clip completely
	SendWeaponAnim(ACT_VM_SECONDARYATTACK);
	m_flNextSecondaryAttack = pOwner->m_flNextAttack = CURTIME + SequenceDuration();

	// Register a muzzleflash for the AI
	pOwner->DoMuzzleFlash();
	pOwner->SetMuzzleFlashTime(CURTIME + 0.5);

	WeaponSound(WPN_DOUBLE);

	pOwner->RumbleEffect(RUMBLE_SHOTGUN_DOUBLE, 0, RUMBLE_FLAG_RESTART);

	// Fire the bullets
	Vector vecSrc = pOwner->Weapon_ShootPosition();
	Vector vecAiming = pOwner->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);
	Vector impactPoint = vecSrc + (vecAiming * MAX_TRACE_LENGTH);

	// Fire the bullets
	Vector vecVelocity = vecAiming * 1000.0f;

	CCombineBallAttractor *pGrenade = (CCombineBallAttractor *)CBaseEntity::Create("prop_combine_ball_attractor", vecSrc, vec3_angle, pOwner);
	pGrenade->SetVelocity(vecVelocity, vec3_origin);

	/*
	// Fire the combine ball
	CreateCombineBall(	vecSrc,
	vecVelocity,
	sk_weapon_plasma_alt_fire_radius.GetFloat(),
	sk_weapon_plasma_alt_fire_mass.GetFloat(),
	sk_weapon_plasma_alt_fire_duration.GetFloat(),
	pOwner, false );

	// View effects
	color32 white = {255, 255, 255, 64};
	UTIL_ScreenFade( pOwner, white, 0.1, 0, FFADE_IN  );
	*/
	//Disorient the player
	QAngle angles = pOwner->GetLocalAngles();

	angles.x += random->RandomInt(-4, 4);
	angles.y += random->RandomInt(-4, 4);
	angles.z = 0;

	pOwner->SnapEyeAngles(angles);

	pOwner->ViewPunch(QAngle(random->RandomInt(-8, -12), random->RandomInt(1, 2), 0));

	// Decrease ammo
	pOwner->RemoveAmmo(1, m_iSecondaryAmmoType);

	// Can shoot again immediately
	m_flNextPrimaryAttack = CURTIME + 0.5f;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextSecondaryAttack = CURTIME + 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPlasma::SecondaryAttack(void)
{
	if (m_bShotDelayed)
		return;

	// Cannot fire underwater
	if (GetOwner() && GetOwner()->GetWaterLevel() == 3)
	{
		SendWeaponAnim(ACT_VM_DRYFIRE);
		BaseClass::WeaponSound(EMPTY);
		m_flNextSecondaryAttack = CURTIME + 0.5f;
		return;
	}

	m_bShotDelayed = true;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flDelayedFire = CURTIME + 0.5f;

	CBasePlayer *player = ToBasePlayer(GetOwner());
	if (player)
	{
		player->RumbleEffect(RUMBLE_AR2_ALT_FIRE, 0, RUMBLE_FLAG_RESTART);
	}

	SendWeaponAnim(ACT_VM_FIDGET);
	WeaponSound(SPECIAL1);

	m_iSecondaryAttacks++;
	gamestats->Event_WeaponFired(player, false, GetClassname());
}

//-----------------------------------------------------------------------------
// Purpose: Override if we're waiting to release a shot
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPlasma::CanHolster(void)
{
	if (m_bShotDelayed)
		return false;

	return BaseClass::CanHolster();
}

//-----------------------------------------------------------------------------
// Purpose: Override if we're waiting to release a shot
//-----------------------------------------------------------------------------
bool CWeaponPlasma::Reload(void)
{
	if (m_bShotDelayed)
		return false;

	return BaseClass::Reload();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOperator - 
//-----------------------------------------------------------------------------
void CWeaponPlasma::FireNPCPrimaryAttack(CBaseCombatCharacter *pOperator, bool bUseWeaponAngles)
{
	Vector vecShootOrigin, vecShootDir;

	CAI_BaseNPC *npc = pOperator->MyNPCPointer();
	ASSERT(npc != NULL);

	if (bUseWeaponAngles)
	{
		QAngle	angShootDir;
		GetAttachment(LookupAttachment("muzzle"), vecShootOrigin, angShootDir);
		AngleVectors(angShootDir, &vecShootDir);
	}
	else
	{
		vecShootOrigin = pOperator->Weapon_ShootPosition();
		vecShootDir = npc->GetActualShootTrajectory(vecShootOrigin);
	}

	WeaponSoundRealtime(SINGLE_NPC);

	CSoundEnt::InsertSound(SOUND_COMBAT | SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy());

	pOperator->FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2);

	// NOTENOTE: This is overriden on the client-side
	// pOperator->DoMuzzleFlash();

	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPlasma::FireNPCSecondaryAttack(CBaseCombatCharacter *pOperator, bool bUseWeaponAngles)
{
	WeaponSound(WPN_DOUBLE);

	if (!GetOwner())
		return;

	CAI_BaseNPC *pNPC = GetOwner()->MyNPCPointer();
	if (!pNPC)
		return;

	// Fire!
	Vector vecSrc;
	Vector vecAiming;

	if (bUseWeaponAngles)
	{
		QAngle	angShootDir;
		GetAttachment(LookupAttachment("muzzle"), vecSrc, angShootDir);
		AngleVectors(angShootDir, &vecAiming);
	}
	else
	{
		vecSrc = pNPC->Weapon_ShootPosition();

		Vector vecTarget;

		CNPC_Combine *pSoldier = dynamic_cast<CNPC_Combine *>(pNPC);
		if (pSoldier)
		{
			// In the distant misty past, elite soldiers tried to use bank shots.
			// Therefore, we must ask them specifically what direction they are shooting.
			vecTarget = pSoldier->GetAltFireTarget();
		}
		else
		{
			// All other users of the AR2 alt-fire shoot directly at their enemy.
			if (!pNPC->GetEnemy())
				return;

			vecTarget = pNPC->GetEnemy()->BodyTarget(vecSrc);
		}

		vecAiming = vecTarget - vecSrc;
		VectorNormalize(vecAiming);
	}

	Vector impactPoint = vecSrc + (vecAiming * MAX_TRACE_LENGTH);

	float flAmmoRatio = 1.0f;
	float flDuration = RemapValClamped(flAmmoRatio, 0.0f, 1.0f, 0.5f, sk_weapon_plasma_alt_fire_duration.GetFloat());
	float flRadius = RemapValClamped(flAmmoRatio, 0.0f, 1.0f, 4.0f, sk_weapon_plasma_alt_fire_radius.GetFloat());

	// Fire the bullets
	Vector vecVelocity = vecAiming * 1000.0f;

	// Fire the combine ball
	CreateCombineBall(vecSrc,
		vecVelocity,
		flRadius,
		sk_weapon_plasma_alt_fire_mass.GetFloat(),
		flDuration,
		pNPC, false);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPlasma::Operator_ForceNPCFire(CBaseCombatCharacter *pOperator, bool bSecondary)
{
	if (bSecondary)
	{
		FireNPCSecondaryAttack(pOperator, true);
	}
	else
	{
		// Ensure we have enough rounds in the clip
		m_iClip1++;

		FireNPCPrimaryAttack(pOperator, true);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponPlasma::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_AR2:
	{
		FireNPCPrimaryAttack(pOperator, false);
	}
	break;

	case EVENT_WEAPON_AR2_ALTFIRE:
	{
		FireNPCSecondaryAttack(pOperator, false);
	}
	break;

	default:
		CBaseCombatWeapon::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPlasma::AddViewKick(void)
{
	//Get the view kick
	CBasePlayer *player = ToBasePlayer(GetOwner());

	if (!player)
		return;

	float flDuration = m_fFireDuration;

	if (g_pGameRules->GetAutoAimMode() == AUTOAIM_ON_CONSOLE)
	{
		// On the 360 (or in any configuration using the 360 aiming scheme), don't let the
		// AR2 progressive into the late, highly inaccurate stages of its kick. Just
		// spoof the time to make it look (to the kicking code) like we haven't been
		// firing for very long.
		flDuration = MIN(flDuration, 0.75f);
	}

	DoMachineGunKick(player, 0.5f, 8.0f, flDuration, 5.0f);
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CWeaponPlasma::GetProficiencyValues()
{
static WeaponProficiencyInfo_t proficiencyTable[] =
{
{ 7.0,		0.75	},
{ 5.00,		0.75	},
{ 3.0,		0.85	},
{ 5.0/3.0,	0.75	},
{ 1.00,		1.0		},
};

COMPILE_TIME_ASSERT( ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

return proficiencyTable;
}
#endif

//====================================
// Proxmiti Mine (Dark Interval)
//====================================
#if defined(SLAM_NEW)

extern ConVar sk_plr_dmg_slam;
extern ConVar sk_npc_dmg_slam;
ConVar sk_slam_radius("sk_slam_radius", "260");
ConVar sk_slam_throw_force("sk_slam_throw_force", "700");
ConVar sk_slam_drop_force("sk_slam_drop_force", "200");
#define	SLAM_SPRITE	"sprites/redglow1.vmt"

CSlamGrenade::CSlamGrenade()
{
	m_vLastPosition.Init();
	m_NPCOwnerClass = CLASS_NONE; // we use this class_t to remember whose side planted the slam, if it was NPC planted. Default to none.
}

CSlamGrenade::~CSlamGrenade()
{
	if (m_hGlowSprite != NULL)
	{
		UTIL_Remove(m_hGlowSprite);
		m_hGlowSprite = NULL;
	}
}

void CSlamGrenade::Precache(void)
{
	PrecacheModel("models/Weapons/w_slam.mdl");
	PrecacheScriptSound("DI_SLAM.DetonateNoise");
	PrecacheScriptSound("DI_SLAM.Activate");
	PrecacheModel(SLAM_SPRITE);
}
void CSlamGrenade::Spawn(void)
{
	Precache();
	SetModel("models/Weapons/w_slam.mdl");

	VPhysicsInitNormal(SOLID_BBOX, GetSolidFlags() | FSOLID_TRIGGER, false);
	SetMoveType(MOVETYPE_VPHYSICS);
	SetCollisionGroup(COLLISION_GROUP_PROJECTILE);

	//	SetTouch(&CSlamGrenade::SlamFlyTouch); // bugged.
	SetThink(&CSlamGrenade::SlamFlyThink);
	SetNextThink(CURTIME + 0.1f);

	//	m_flDamage = sk_plr_dmg_slam.GetFloat(); // we receive the damage value from the thrower
	m_DmgRadius = sk_slam_radius.GetFloat();
	m_takedamage = DAMAGE_YES;
	m_iHealth = 1;

	SetGravity(UTIL_ScaleForGravity(400));	// slightly lower gravity
	SetFriction(1.0);
	SetSequence(1);
	SetDamage(150);

	m_bIsAttached = false;
	m_flNextBounceSoundTime = 0;

	m_vLastPosition = vec3_origin;

	m_hGlowSprite = NULL;
	CreateEffects();
}

void CSlamGrenade::CreateEffects(void)
{
	// Only do this once
	if (m_hGlowSprite != NULL)
		return;

	// Create a blinking light to show we're an active SLAM
	m_hGlowSprite = CSprite::SpriteCreate(SLAM_SPRITE, GetAbsOrigin(), false);
	m_hGlowSprite->SetAttachment(this, LookupAttachment("beam_attach"));
	if( this->m_bPlayerOwned)
		m_hGlowSprite->SetTransparency(kRenderWorldGlow, 25, 255, 25, 255, kRenderFxStrobeFast);
	else
		m_hGlowSprite->SetTransparency(kRenderWorldGlow, 255, 255, 255, 255, kRenderFxStrobeFast);
	m_hGlowSprite->SetBrightness(0, 1.0f);
	m_hGlowSprite->SetScale(0.1f, 0.5f);
	m_hGlowSprite->TurnOn();
}

void CSlamGrenade::BounceSound(void)
{
	if (CURTIME > m_flNextBounceSoundTime)
	{
		m_flNextBounceSoundTime = CURTIME + 0.1;
	}
}

// start thinking once we've settled down, either attached to something or on the ground.
void CSlamGrenade::SlamSensorThink()
{
	CBaseEntity *pOther = NULL;

	while ((pOther = gEntList.FindEntityInSphere(pOther, GetAbsOrigin(), m_DmgRadius * 0.5)) != NULL)
	{
		if (pOther->IsNPC())
		{
			if (m_bPlayerOwned && (pOther->MyNPCPointer()->IsPlayerAlly()) == false)
			{
				if (!GetOwnerEntity() || (GetOwnerEntity() != pOther))
				{
					SetTouch(NULL);
					m_flDetonateTime = CURTIME + 0.2f;
					CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin(), 400, 1.5, this);
					EmitSound("DI_SLAM.DetonateNoise");
					SetThink(&CSlamGrenade::SlamDetonateThink);
					SetNextThink(CURTIME);
					return;
				}
			}
			else if (!m_bPlayerOwned && (pOther->MyNPCPointer()->Classify()) != m_NPCOwnerClass)
			{ // We're checking for remembered class and not owner's class because owner might have died, removed, or never really existed.
				SetTouch(NULL);
				m_flDetonateTime = CURTIME + 0.2f;
				CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin(), 400, 1.5, this);
				EmitSound("DI_SLAM.DetonateNoise");
				SetThink(&CSlamGrenade::SlamDetonateThink);
				SetNextThink(CURTIME);
				return;
			}
		}
		else if (pOther->IsPlayer())
		{
			if (!m_bPlayerOwned && (m_NPCOwnerClass != CLASS_PLAYER_ALLY
				&& m_NPCOwnerClass != CLASS_PLAYER_ALLY_VITAL
				&& m_NPCOwnerClass != CLASS_PLAYER))
			{
				SetTouch(NULL);
				m_flDetonateTime = CURTIME + 0.2f;
				CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin(), 400, 1.5, this);
				EmitSound("DI_SLAM.DetonateNoise");
				SetThink(&CSlamGrenade::SlamDetonateThink);
				SetNextThink(CURTIME);
				return;
			}
		}
	}

	SetNextThink(CURTIME + 0.1f);
}

// touch detection while flying
void CSlamGrenade::SlamFlyTouch(CBaseEntity *pOther)
{
	if (pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER))
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ((pOther->m_takedamage == DAMAGE_NO) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY))
			return;
	}

	if (pOther->IsPlayer())
		return;

	if (FClassnameIs(pOther, "npc_grenade_slam"))
		return;

	trace_t	tr;
	tr = BaseClass::GetTouchTrace();

	if (pOther->m_takedamage != DAMAGE_NO)
	{
		Vector	vecNormalizedVel = GetAbsVelocity();

		ClearMultiDamage();
		VectorNormalize(vecNormalizedVel);
		
		// Keep going through breakable glass.
		if (pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS)
			return;

		SetAbsVelocity(Vector(0, 0, 0));

		EmitSound("DI_SLAM.AttachToThing");
		
		Vector vForward;
		AngleVectors(GetAbsAngles(), &vForward);
		VectorNormalize(vForward);

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
		else if (pOther->GetMoveType() == MOVETYPE_VPHYSICS && (FClassnameIs(pOther, "prop_physics*") || FClassnameIs(pOther, "func_physbox")))
		{
			// We hit a non-breakable prop_physics, stick to it.
			StickTo(pOther, tr);
		}
	}
}

void CSlamGrenade::StickTo(CBaseEntity *pOther, trace_t &tr)
{
	EmitSound("DI_Slam.AttachToWorld");

	SetMoveType(MOVETYPE_NONE);

	if (!pOther->IsWorld())
	{
		SetParent(pOther); // Note that this disables collision on the bolt - it's a general thing in engine; entities with a parent become generally non solid.
		SetSolid(SOLID_NONE);
		SetSolidFlags(FSOLID_NOT_SOLID);
	}

	if (FClassnameIs(pOther, "prop_physics"))
	{
		SetSolid(SOLID_VPHYSICS);
		RemoveSolidFlags(FSOLID_NOT_SOLID);
		SetCollisionGroup(COLLISION_GROUP_NONE);
		PhysDisableEntityCollisions(pOther, this);
		PhysDisableEntityCollisions(UTIL_GetLocalPlayer(), this);
		PhysEnableEntityCollisions(VPhysicsGetObject(), g_PhysWorldObject);
	}

	Vector vNormal = tr.plane.normal;
	QAngle normalAngles;
	VectorAngles(vNormal, normalAngles);
	SetAbsAngles(normalAngles);

	Vector vecVelocity = GetAbsVelocity();

	SetTouch(NULL);
	// Clear think function
	m_bIsAttached = true;
	m_hGlowSprite->SetBrightness(255, 1.0f);
	EmitSound("DI_SLAM.Activate");
	SetThink(&CSlamGrenade::SlamSensorThink);
	SetNextThink(CURTIME + 0.1f);
}

void CSlamGrenade::SlamDetonateThink(void)
{
	if (CURTIME > m_flDetonateTime)
	{
		Detonate();
		return;
	}

	SetNextThink(CURTIME + 0.1f);
}

void CSlamGrenade::SlamFlyThink(void)
{
	// If attached resize so player can pick up off wall
	if (m_bIsAttached)
	{
		UTIL_SetSize(this, Vector(-2, -2, -6), Vector(2, 2, 6));
	}

	// See if I can lose my owner (has dropper moved out of way?)
	// Want do this so owner can shoot the satchel charge
	if (GetOwnerEntity())
	{
		trace_t tr;
		Vector	vUpABit = GetAbsOrigin();
		vUpABit.z += 5.0;

		CBaseEntity* saveOwner = GetOwnerEntity();
		SetOwnerEntity(NULL);
		UTIL_TraceEntity(this, GetAbsOrigin(), vUpABit, MASK_SOLID, &tr);
		if (tr.startsolid || tr.fraction != 1.0)
		{
			SetOwnerEntity(saveOwner);
		}
	}

	// Bounce movement code gets this think stuck occasionally so check if I've 
	// succeeded in moving, otherwise kill my motions.
	if ((GetAbsOrigin() - m_vLastPosition).LengthSqr()<1)
	{
		SetAbsVelocity(vec3_origin);

		QAngle angVel = GetLocalAngularVelocity();
		angVel.y = 0;
		SetLocalAngularVelocity(angVel);

		// Clear think function
		m_bIsAttached = true;
		// activation feedback
		m_hGlowSprite->SetBrightness(255, 1.0f);
		EmitSound("DI_SLAM.Activate");
		SetBodygroup(0, 1); // flip the PIPS cover
							//
		SetThink(&CSlamGrenade::SlamSensorThink);
		SetNextThink(CURTIME + 0.1f);
		return;
	}
	m_vLastPosition = GetAbsOrigin();

	StudioFrameAdvance();
	SetNextThink(CURTIME + 0.1f);

	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	// Is it attached to a wall?
	if (m_bIsAttached)
	{
		return;
	}
}

void CSlamGrenade::Deactivate(void)
{
	AddSolidFlags(FSOLID_NOT_SOLID);
	UTIL_Remove(this);

	if (m_hGlowSprite != NULL)
	{
		UTIL_Remove(m_hGlowSprite);
		m_hGlowSprite = NULL;
	}
}

// Input handlers
void CSlamGrenade::InputExplode(inputdata_t &inputdata)
{
	ExplosionCreate(GetAbsOrigin() + Vector(0, 0, 16), GetAbsAngles(), GetThrower(), GetDamage(), 200,
		SF_ENVEXPLOSION_NOSMOKE, 0.0f, this);

	UTIL_Remove(this);
}

BEGIN_DATADESC(CSlamGrenade)

DEFINE_FIELD(m_flNextBounceSoundTime, FIELD_TIME),
DEFINE_FIELD(m_bPlayerOwned, FIELD_BOOLEAN),
DEFINE_FIELD(m_vLastPosition, FIELD_POSITION_VECTOR),
//DEFINE_FIELD(m_pMyWeaponSLAM, FIELD_CLASSPTR),
DEFINE_FIELD(m_bIsAttached, FIELD_BOOLEAN),

// Function Pointers
DEFINE_THINKFUNC(SlamFlyThink),
DEFINE_THINKFUNC(SlamDetonateThink),
DEFINE_THINKFUNC(SlamSensorThink),
DEFINE_ENTITYFUNC(SlamFlyTouch),

// Inputs
DEFINE_INPUTFUNC(FIELD_VOID, "Explode", InputExplode),

END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_grenade_slam, CSlamGrenade);

class CWeaponSlam : public CBaseHLCombatWeapon
{
	DECLARE_CLASS(CWeaponSlam, CBaseHLCombatWeapon);
private:
	bool	m_bRedraw;	//Draw the weapon again after throwing a grenade
	int		m_attackMode;
	bool	m_hasThrownSlam;

	DECLARE_ACTTABLE();

	DECLARE_DATADESC();
public:
	DECLARE_SERVERCLASS();

	CWeaponSlam() :
		CBaseHLCombatWeapon(),
		m_bRedraw(false),
		m_attackMode(0),
		m_hasThrownSlam(false)
	{
		NULL;
	}

	~CWeaponSlam()
	{
	}

	int CapabilitiesGet(void)
	{
		return bits_CAP_WEAPON_RANGE_ATTACK1 | bits_CAP_WEAPON_RANGE_ATTACK2;
	}
	
	void Spawn(void)
	{
		Precache();
		BaseClass::Spawn();
	}

	void Precache(void)
	{
		BaseClass::Precache();
	//	UTIL_PrecacheOther("npc_grenade_slam");
		UTIL_PrecacheOther("shared_projectile");
		PrecacheScriptSound("WeaponSLAM.Throw");
	}

	bool Deploy(void)
	{
		m_bRedraw = m_hasThrownSlam = false;
		m_attackMode = 0;
		SendWeaponAnim(ACT_VM_DRAW);
		return BaseClass::Deploy();
	}

	bool Holster(CBaseCombatWeapon *pSwitchingTo)
	{
		m_bRedraw = m_hasThrownSlam = false;
		m_attackMode = 0;
		return BaseClass::Holster(pSwitchingTo);
	}

	bool Reload(void)
	{
		if (!HasPrimaryAmmo())
			return false;

		if ((m_bRedraw) && (m_flNextPrimaryAttack <= CURTIME) && (m_flNextSecondaryAttack <= CURTIME))
		{
			//Redraw the weapon
			SendWeaponAnim(ACT_VM_DRAW);

			//Update our times
			m_flNextPrimaryAttack = CURTIME + SequenceDuration();
			m_flNextSecondaryAttack = CURTIME + SequenceDuration();
			m_flTimeWeaponIdle = CURTIME + SequenceDuration();

			m_hasThrownSlam = false;

			//Mark this as done
			m_bRedraw = false;
		}

		return true;
	}

	void WeaponIdle(void)
	{
		// Ready to switch animations?
		if (HasWeaponIdleTimeElapsed())
		{			
			CBaseCombatCharacter *pOwner = GetOwner();
			if (!pOwner)
			{
				return;
			}

			int iAnim = ACT_VM_IDLE;

			SendWeaponAnim(iAnim);
		}
	}

	void PrimaryAttack(void)
	{
		if (m_bRedraw)
			return;

		CBaseCombatCharacter *pOwner = GetOwner();

		if (pOwner == NULL)
		{
			return;
		}

		CBasePlayer *player = ToBasePlayer(GetOwner());;

		if (!player)
			return;

		m_attackMode = 1;
		SendWeaponAnim(ACT_VM_PULLBACK);
		WeaponSound(SPECIAL2);

		// Put both of these off indefinitely. We do not know how long
		// the player will hold the grenade.
		m_flTimeWeaponIdle = FLT_MAX;
		m_flNextPrimaryAttack = FLT_MAX;

		// If I'm now out of ammo, switch away
		if (!HasPrimaryAmmo())
		{
			player->SwitchToNextBestWeapon(this);
		}
	}
	
	void SecondaryAttack(void)
	{
		if (m_bRedraw)
			return;

		if (!HasPrimaryAmmo())
			return;

		CBaseCombatCharacter *pOwner = GetOwner();

		if (pOwner == NULL)
			return;

		CBasePlayer *player = ToBasePlayer(pOwner);

		if (player == NULL)
			return;

		m_attackMode = 2;
		SendWeaponAnim(ACT_VM_PULLBACK_LOW);
		WeaponSound(SPECIAL2); // blip in hand

		// Don't let weapon idle interfere in the middle of a throw!
		m_flTimeWeaponIdle = FLT_MAX;
		m_flNextSecondaryAttack = FLT_MAX;

		// If I'm now out of ammo, switch away
		if (!HasPrimaryAmmo())
		{
			player->SwitchToNextBestWeapon(this);
		}
	}
	
	void Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
	{
		switch (pEvent->event)
		{
		case AE_WPN_PRIMARYATTACK:
			SendWeaponAnim(ACT_VM_THROW);
			if (!m_hasThrownSlam)
				ThrowSlam(ToBasePlayer(pOperator), true);
			break;

		case AE_WPN_PLAYWPNSOUND: // hack
			SendWeaponAnim(ACT_VM_RELEASE);
			if (!m_hasThrownSlam)
				ThrowSlam(ToBasePlayer(pOperator), false);
			break;

		default:
			BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
			break;
		}
	}
	
	void ItemPostFrame()
	{
		/*
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());

		if (pOwner == NULL)
			return;

		if (pOwner)
		{
			switch (m_attackMode)
			{
			case 1:
				if (IsViewModelSequenceFinished())
				{
					SendWeaponAnim(ACT_VM_THROW);
					if( !m_hasThrownSlam)
						ThrowSlam(pOwner, true);
				}
				break;

			case 2:
				if (IsViewModelSequenceFinished())
				{
					SendWeaponAnim(ACT_VM_RELEASE);
					if (!m_hasThrownSlam)
						ThrowSlam(pOwner, false);
				}
				break;

			default:
				break;
			}
		}
		*/

		BaseClass::ItemPostFrame();

		if (m_bRedraw)
		{
			if (IsViewModelSequenceFinished())
			{
				Reload();
			}
		}
	}

	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
	void CheckThrowPosition(CBasePlayer *player, const Vector &vecEye, Vector &vecSrc)
	{
		trace_t tr;

		UTIL_TraceHull(vecEye, vecSrc, -Vector(4 + 2, 4 + 2, 4 + 2), Vector(4 + 2, 4 + 2, 4 + 2),
			player->PhysicsSolidMaskForEntity(), player, player->GetCollisionGroup(), &tr);

		if (tr.DidHit())
		{
			vecSrc = tr.endpos;
		}
	}

	void ThrowSlam(CBasePlayer *player, bool throwFar)
	{
		Vector	vecEye = player->EyePosition();
		Vector	vForward, vRight;

		player->EyeVectors(&vForward, &vRight, NULL);
		Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f + (throwFar? Vector(0, 0, -8) : Vector(0,0,-16));
		CheckThrowPosition(player, vecEye, vecSrc);
		//	vForward[0] += 0.1f;
		vForward[2] += 0.1f;

		Vector vecThrow;
		player->GetVelocity(&vecThrow, NULL);
		vecThrow += vForward * (throwFar ? (sk_slam_throw_force.GetFloat()) : (sk_slam_drop_force.GetFloat())) + (throwFar ? Vector(0,0,0) : Vector(0, 0, 50));

		////
		CSlamGrenade *pGrenade = (CSlamGrenade*)Create("npc_grenade_slam", vecSrc, vec3_angle, player);
		pGrenade->SetThrower(player);
		pGrenade->ApplyAbsVelocityImpulse(vecThrow);
		pGrenade->SetLocalAngularVelocity(QAngle(0, 400, 0));
		pGrenade->m_bIsLive = true;
		pGrenade->SetDamage(sk_plr_dmg_slam.GetFloat());
		if (GetOwner()->IsPlayer())
			pGrenade->m_bPlayerOwned = true;

		DecrementAmmo(player);

		WeaponSound(SINGLE);

		m_iPrimaryAttacks++;
		gamestats->Event_WeaponFired(player, true, GetClassname());

		m_flNextPrimaryAttack = CURTIME + 0.5f;
		m_flNextSecondaryAttack = CURTIME + 0.5f;
		m_flTimeWeaponIdle = FLT_MAX; //NOTE: This is set once the animation has finished up!

		m_hasThrownSlam = true;
		m_attackMode = 0;
		m_bRedraw = true;
	}

	void DecrementAmmo(CBaseCombatCharacter *pOwner)
	{
		pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
	}
};

acttable_t	CWeaponSlam::m_acttable[] =
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SLAM, true },
};

IMPLEMENT_ACTTABLE(CWeaponSlam);

BEGIN_DATADESC(CWeaponSlam)
DEFINE_FIELD(m_bRedraw, FIELD_BOOLEAN),
DEFINE_FIELD(m_attackMode, FIELD_INTEGER),
DEFINE_FIELD(m_hasThrownSlam, FIELD_BOOLEAN),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CWeaponSlam, DT_WeaponSlam)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_slam, CWeaponSlam);
PRECACHE_WEAPON_REGISTER(weapon_slam);
#endif

//====================================
// Crossbow (Dark Interval)
// Behaves similarly to HL2, 
// but can be heated with altfire
//====================================
#ifdef CROSSBOW
#ifdef DARKINTERVAL
static const char *BOLT_MODEL = "models/_Weapons/crossbow_bolt_physical.mdl";

#define BOLT_AIR_VELOCITY	2000
#define BOLT_WATER_VELOCITY	1300
#else
#define BOLT_MODEL	"models/weapons/w_missile_closed.mdl"

#define BOLT_AIR_VELOCITY	2500
#define BOLT_WATER_VELOCITY	1500
#endif

extern ConVar sk_plr_dmg_crossbow;
extern ConVar sk_npc_dmg_crossbow;

ConVar sk_crossbow_heatup_mult("sk_crossbow_heatup_mult", "0.78", 0, "Tweak the speed at which the crossbow bolt heats up on alt attack");
ConVar sk_crossbow_bolt_speed("sk_crossbow_bolt_speed", "2000");
//ConVar di_crossbow_proxy_value("di_crossbow_proxy_value", "0.0", FCVAR_HIDDEN | FCVAR_REPLICATED);

void TE_StickyBolt(IRecipientFilter& filter, float delay, Vector vecDirection, const Vector *origin);

#define	BOLT_SKIN_NORMAL	0
#define BOLT_SKIN_GLOW		1

//-----------------------------------------------------------------------------
// Crossbow Bolt
//-----------------------------------------------------------------------------
class CCrossbowBolt : public CBaseCombatCharacter
{
	DECLARE_CLASS(CCrossbowBolt, CBaseCombatCharacter);

public:
	CCrossbowBolt()
	{
#ifdef DARKINTERVAL
		m_bolt_fully_heated_boolean = false;
#endif
	};
	~CCrossbowBolt();

	Class_T Classify(void) { return CLASS_NONE; }

public:
	void Spawn(void);
	void Precache(void);
	void BubbleThink(void);
	void BoltTouch(CBaseEntity *pOther);
	bool CreateVPhysics(void);
	unsigned int PhysicsSolidMaskForEntity() const;
#ifdef DARKINTERVAL
	static CCrossbowBolt *BoltCreate(const Vector &vecOrigin, const QAngle &angAngles, CBasePlayer *pentOwner = NULL, bool FullyHeated = false);
	bool	m_bolt_fully_heated_boolean;
#else
	static CCrossbowBolt *BoltCreate(const Vector &vecOrigin, const QAngle &angAngles, CBasePlayer *pentOwner = NULL);
#endif
protected:
#ifdef DARKINTERVAL
	bool	CreateSprites(bool bHot = true); // if not hot, create a faint non glowing trail, and no glow sprite
#else
	bool	CreateSprites(void);
#endif
	CHandle<CSprite>		m_glow_sprite_handle; // renamed in DI
	CHandle<CSpriteTrail>	m_glow_trail_handle;
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
};
LINK_ENTITY_TO_CLASS(crossbow_bolt, CCrossbowBolt);

BEGIN_DATADESC(CCrossbowBolt)
// Function Pointers
DEFINE_FUNCTION(BubbleThink),
DEFINE_FUNCTION(BoltTouch),
DEFINE_FIELD(m_glow_sprite_handle, FIELD_EHANDLE),
#ifdef DARKINTERVAL
DEFINE_FIELD(m_bolt_fully_heated_boolean, FIELD_BOOLEAN),
DEFINE_FIELD(m_glow_trail_handle, FIELD_EHANDLE),
#endif
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CCrossbowBolt, DT_CrossbowBolt)
END_SEND_TABLE()

#ifdef DARKINTERVAL
CCrossbowBolt *CCrossbowBolt::BoltCreate(const Vector &vecOrigin, const QAngle &angAngles, CBasePlayer *pentOwner, bool FullyHeated)
#else
CCrossbowBolt *CCrossbowBolt::BoltCreate(const Vector &vecOrigin, const QAngle &angAngles, CBasePlayer *pentOwner)
#endif
{
	// Create a new entity with CCrossbowBolt private data
	CCrossbowBolt *pBolt = ( CCrossbowBolt * )CreateEntityByName("crossbow_bolt");
	UTIL_SetOrigin(pBolt, vecOrigin);
	pBolt->SetAbsAngles(angAngles);
#ifdef DARKINTERVAL
	pBolt->m_bolt_fully_heated_boolean = FullyHeated;
#endif
	pBolt->Spawn();
	pBolt->SetOwnerEntity(pentOwner);

	return pBolt;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCrossbowBolt::~CCrossbowBolt(void)
{
	if ( m_glow_sprite_handle )
	{
		UTIL_Remove(m_glow_sprite_handle);
	}
#ifdef DARKINTERVAL
	if ( m_glow_trail_handle )
	{
		UTIL_Remove(m_glow_trail_handle);
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CCrossbowBolt::CreateVPhysics(void)
{
	// Create the object in the physics system
	VPhysicsInitNormal(SOLID_BBOX, FSOLID_NOT_STANDABLE, false);

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
unsigned int CCrossbowBolt::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
#ifdef DARKINTERVAL
bool CCrossbowBolt::CreateSprites(bool bHot)
#else
bool CCrossbowBolt::CreateSprites(void)
#endif
{
#ifdef DARKINTERVAL
	if ( bHot )
#endif
	{
		// Start up the bolt glow
		m_glow_sprite_handle = CSprite::SpriteCreate("sprites/light_glow02_noz.vmt", GetLocalOrigin(), false);

		if ( m_glow_sprite_handle != NULL )
		{
			m_glow_sprite_handle->FollowEntity(this);
			m_glow_sprite_handle->SetTransparency(kRenderGlow, 255, 255, 255, 128, kRenderFxNoDissipation);
			m_glow_sprite_handle->SetScale(0.2f);
#ifndef DARKINTERVAL
			m_glow_sprite_handle->TurnOff();
#endif
		}

#ifdef DARKINTERVAL
		// Start up the bolt trail: orange if hot, grey smoke if cold
		m_glow_trail_handle = CSpriteTrail::SpriteTrailCreate("sprites/bluelaser1.vmt", GetLocalOrigin(), false);
	} else
	{
		m_glow_trail_handle = CSpriteTrail::SpriteTrailCreate("sprites/zbeam3.vmt", GetLocalOrigin(), false);
#endif
	}

#ifdef DARKINTERVAL
	if ( m_glow_trail_handle != NULL )
	{
		m_glow_trail_handle->FollowEntity(this);
		if ( bHot )
		{
			m_glow_trail_handle->SetTransparency(kRenderTransAdd, 255, 25, 0, 255, kRenderFxNone);
			m_glow_trail_handle->SetStartWidth(8.0f);
			m_glow_trail_handle->SetEndWidth(1.0f);
		} else
		{
			m_glow_trail_handle->SetTransparency(kRenderTransAdd, 100, 100, 105, 150, kRenderFxNone);
			m_glow_trail_handle->SetStartWidth(2.0f);
			m_glow_trail_handle->SetEndWidth(0.25f);
		};
		m_glow_trail_handle->SetLifeTime(0.5f);
		//	m_glow_trail_handle->TurnOff();
	}
#endif
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCrossbowBolt::Spawn(void)
{
	Precache();

	SetModel("models/crossbow_bolt.mdl");
	SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM);
	UTIL_SetSize(this, -Vector(0.3f, 0.3f, 0.3f), Vector(0.3f, 0.3f, 0.3f));
	SetSolid(SOLID_BBOX);
	SetGravity(0.05f);

	// Make sure we're updated if we're underwater
	UpdateWaterState();

	SetTouch(&CCrossbowBolt::BoltTouch);

	SetThink(&CCrossbowBolt::BubbleThink);
	SetNextThink(CURTIME + 0.1f);
#ifdef DARKINTERVAL
	if ( m_bolt_fully_heated_boolean )
	{
		CreateSprites(true);
		m_nSkin = BOLT_SKIN_GLOW;
	} else
		CreateSprites(false);
#else
	CreateSprites();

	// Make us glow until we've hit the wall
	m_nSkin = BOLT_SKIN_GLOW;
#endif
}


void CCrossbowBolt::Precache(void)
{
	PrecacheModel(BOLT_MODEL);

	// This is used by C_TEStickyBolt, despte being different from above!!!
	PrecacheModel("models/crossbow_bolt.mdl");

	PrecacheModel("sprites/light_glow02_noz.vmt");
#ifdef DARKINTERVAL
	PrecacheModel("sprites/bluelaser1.vmt"); // trails
	PrecacheModel("sprites/zbeam3.vmt");
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCrossbowBolt::BoltTouch(CBaseEntity *pOther)
{
	if ( pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER) )
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ( ( pOther->m_takedamage == DAMAGE_NO ) || ( pOther->m_takedamage == DAMAGE_EVENTS_ONLY ) )
			return;
	}

	if ( pOther->m_takedamage != DAMAGE_NO )
	{
		trace_t	tr, tr2;
		tr = BaseClass::GetTouchTrace();
		Vector	vecNormalizedVel = GetAbsVelocity();

		ClearMultiDamage();
		VectorNormalize(vecNormalizedVel);

		if ( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() && pOther->IsNPC() )
		{
			CTakeDamageInfo	dmgInfo(this, GetOwnerEntity(), sk_plr_dmg_crossbow.GetFloat(), DMG_NEVERGIB);
			dmgInfo.AdjustPlayerDamageInflictedForSkillLevel();
			CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
			dmgInfo.SetDamagePosition(tr.endpos);
			pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);
#ifdef DARKINTERVAL
			if ( m_bolt_fully_heated_boolean )
			{
				CBaseCombatCharacter *basecc = ToBaseCombatCharacter(pOther);
				if ( basecc )
				{
					basecc->Ignite(5.0, true, 8.0f, false);
				}
			}
#endif
			CBasePlayer *pPlayer = ToBasePlayer(GetOwnerEntity());
			if ( pPlayer )
			{
				gamestats->Event_WeaponHit(pPlayer, true, "weapon_crossbow", dmgInfo);
			}
		} else
		{
			CTakeDamageInfo	dmgInfo(this, GetOwnerEntity(), sk_plr_dmg_crossbow.GetFloat(), DMG_BULLET | DMG_NEVERGIB);
			CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
			dmgInfo.SetDamagePosition(tr.endpos);
			pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);
		}

		ApplyMultiDamage();

		// keep going through the glass.
		if ( pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS )
			return;

		if ( !pOther->IsAlive() )
		{
			// We killed it! 
			const surfacedata_t *pdata = physprops->GetSurfaceData(tr.surface.surfaceProps);
			if ( pdata->game.material == CHAR_TEX_GLASS )
			{
				return;
			}
		}

		SetAbsVelocity(Vector(0, 0, 0));

		// play body "thwack" sound
		EmitSound("Weapon_Crossbow.BoltHitBody");

		Vector vForward;

		AngleVectors(GetAbsAngles(), &vForward);
		VectorNormalize (vForward);

		UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + vForward * 128, MASK_BLOCKLOS, pOther, COLLISION_GROUP_NONE, &tr2);

		if ( tr2.fraction != 1.0f )
		{
			//			NDebugOverlay::Box( tr2.endpos, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 255, 0, 0, 10 );
			//			NDebugOverlay::Box( GetAbsOrigin(), Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 0, 255, 0, 10 );

			if ( tr2.m_pEnt == NULL || ( tr2.m_pEnt && tr2.m_pEnt->GetMoveType() == MOVETYPE_NONE ) )
			{
				CEffectData	data;

				data.m_vOrigin = tr2.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = tr2.fraction != 1.0f;

				DispatchEffect("BoltImpact", data);
			}
		}
#ifdef DARKINTERVAL
		if ( m_glow_trail_handle != NULL )
		{
			m_glow_trail_handle->TurnOn();
			m_glow_trail_handle->FadeAndDie(1.0f);
		}
#endif
		SetTouch(NULL);
		SetThink(NULL);
		UTIL_Remove(this);
	} else
	{
		trace_t	tr;
		tr = BaseClass::GetTouchTrace();

		// See if we struck the world
		if ( pOther->GetMoveType() == MOVETYPE_NONE && !( tr.surface.flags & SURF_SKY ) )
		{
			EmitSound("Weapon_Crossbow.BoltHitWorld");

			// if what we hit is static architecture, can stay around for a while.
			Vector vecDir = GetAbsVelocity();
			float speed = VectorNormalize(vecDir);

			// See if we should reflect off this surface
			float hitDot = DotProduct(tr.plane.normal, -vecDir);

			if ( ( hitDot < 0.5f ) && ( speed > 100 ) )
			{
				Vector vReflection = 2.0f * tr.plane.normal * hitDot + vecDir;

				QAngle reflectAngles;

				VectorAngles(vReflection, reflectAngles);

				SetLocalAngles(reflectAngles);

				SetAbsVelocity(vReflection * speed * 0.75f);

				// Start to sink faster
				SetGravity(1.0f);
			} else
			{
				SetThink(&CCrossbowBolt::SUB_Remove);
				SetNextThink(CURTIME + 2.0f);

				//FIXME: We actually want to stick (with hierarchy) to what we've hit
				SetMoveType(MOVETYPE_NONE);

				Vector vForward;

				AngleVectors(GetAbsAngles(), &vForward);
				VectorNormalize (vForward);

				CEffectData	data;

				data.m_vOrigin = tr.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = 0;

				DispatchEffect("BoltImpact", data);

				UTIL_ImpactTrace(&tr, DMG_BULLET);

				AddEffects(EF_NODRAW);
				SetTouch(NULL);
				SetThink(&CCrossbowBolt::SUB_Remove);
				SetNextThink(CURTIME + 2.0f);

				if ( m_glow_sprite_handle != NULL )
				{
					m_glow_sprite_handle->TurnOn();
					m_glow_sprite_handle->FadeAndDie(3.0f);
				}
			}

			// Shoot some sparks
			if ( UTIL_PointContents(GetAbsOrigin()) != CONTENTS_WATER )
			{
				g_pEffects->Sparks(GetAbsOrigin());
			}
		} else
		{
			// Put a mark unless we've hit the sky
			if ( ( tr.surface.flags & SURF_SKY ) == false )
			{
				UTIL_ImpactTrace(&tr, DMG_BULLET);
			}

			UTIL_Remove(this);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCrossbowBolt::BubbleThink(void)
{
	QAngle angNewAngles;

	VectorAngles(GetAbsVelocity(), angNewAngles);
	SetAbsAngles(angNewAngles);

	SetNextThink(CURTIME + 0.02f);

	// Make danger sounds out in front of me, to scare snipers back into their hole
	CSoundEnt::InsertSound(SOUND_DANGER_SNIPERONLY, GetAbsOrigin() + GetAbsVelocity() * 0.2, 120.0f, 0.5f, this, SOUNDENT_CHANNEL_REPEATED_DANGER);

	if ( GetWaterLevel() == 0 )
		return;
#ifdef DARKINTERVAL
	int bubblecount = m_bolt_fully_heated_boolean ? 25 : 2;
	UTIL_BubbleTrail(GetAbsOrigin() - GetAbsVelocity() * 0.1f, GetAbsOrigin(), bubblecount);
#else
	UTIL_BubbleTrail(GetAbsOrigin() - GetAbsVelocity() * 0.1f, GetAbsOrigin(), 5);
#endif
}

#ifdef DARKINTERVAL
//=================================================
// Physically simulated crossbow bolt
//=================================================
static const char *s_szPhysicalBoltBubbles = "PhysicalBoltBubbles";

class CPhysicalBolt : public CPhysicsProp, public IParentPropInteraction
{
	DECLARE_CLASS(CPhysicalBolt, CPhysicsProp);

public:

	CPhysicalBolt()
	{
		UseClientSideAnimation();
		m_bolt_fully_heated_boolean = false;
	}
	~CPhysicalBolt()
	{
		if ( m_glow_sprite_handle )
		{
			UTIL_Remove(m_glow_sprite_handle);
		}

		if ( m_glow_trail_handle )
		{
			UTIL_Remove(m_glow_trail_handle);
		}
	}

	Class_T Classify() { return CLASS_NONE; }

public:

	bool m_bolt_fully_heated_boolean;

	void Precache()
	{
		PrecacheModel(BOLT_MODEL);
		PrecacheModel("sprites/light_glow02_noz.vmt");
	}

	void Spawn()
	{
		Precache();
		SetModel(BOLT_MODEL);
		SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM);
		UTIL_SetSize(this, -Vector(1.0f, 1.0f, 1.0f), Vector(1.0f, 1.0f, 1.0f));
		SetSolid(SOLID_BBOX);
		SetGravity(0.05f);
		SetCollisionGroup(COLLISION_GROUP_PROJECTILE);

		// Make sure we're updated if we're underwater
		UpdateWaterState();

		SetTouch(&CPhysicalBolt::BoltTouch);
		SetThink(&CPhysicalBolt::BubbleThink);
		SetNextThink(CURTIME);

		// if we're firing with full charge, make the bolt glow
		if ( m_bolt_fully_heated_boolean )
		{
			CreateSprites(true);
			// Make us glow until we've hit the wall
			m_nSkin = BOLT_SKIN_GLOW;
		} else
			CreateSprites(false);

		//	m_parent_constraint = NULL;
	}

	void Activate()
	{
		BaseClass::Activate();
		SetupGlobalModelData();
	}

	bool CreateVPhysics()
	{
		// Create the object in the physics system
		VPhysicsInitNormal(SOLID_BBOX, FSOLID_NOT_STANDABLE, false);
		return true;
	}
	unsigned int PhysicsSolidMaskForEntity() const
	{
		return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
	}

	// IParentPropInteraction
	void OnParentCollisionInteraction(parentCollisionInteraction_t eType, int index, gamevcollisionevent_t *pEvent)
	{
		if ( eType == COLLISIONINTER_PARENT_FIRST_IMPACT )
		{
			//	m_bThrownBack = true;
			//	Explode();
		}
	}
	void OnParentPhysGunDrop(CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason)
	{
		//	m_bThrownBack = true;
	}

	static CPhysicalBolt *CreatePhysicalBolt(const Vector &vecOrigin, const QAngle &angAngles, CBaseEntity *pentOwner = NULL)
	{
		// Create a new entity with CPhysicalBolt private data
		CPhysicalBolt *pBolt = ( CPhysicalBolt * )CreateEntityByName("crossbow_bolt_physical");
		UTIL_SetOrigin(pBolt, vecOrigin);
		pBolt->SetAbsAngles(angAngles);
		//	pBolt->m_bolt_fully_heated_boolean = FullyHeated;
		//	pBolt->Spawn();
		//	pBolt->Activate();
		//	pBolt->SetOwnerEntity(pentOwner);
		return pBolt;
	}

	void Shoot(Vector &vecVelocity)
	{
		m_vecShootPosition = GetAbsOrigin();

		SetAbsVelocity(vecVelocity);

		//	SetThink(&CHunterFlechette::DopplerThink);
		//	SetNextThink(CURTIME);

		SetContextThink(&CPhysicalBolt::BubbleThink, CURTIME + 0.1, s_szPhysicalBoltBubbles);
	}

	//	void SetSeekTarget(CBaseEntity *pTargetEntity);
	//	void Explode();

protected:
	void SetupGlobalModelData() {};

	//	IPhysicsConstraint *m_parent_constraint;

	void StickTo(CBaseEntity *pOther, trace_t &tr)
	{
		EmitSound("Weapon_Crossbow.BoltHitWorld");

		SetMoveType(MOVETYPE_NONE);

		if ( !pOther->IsWorld() )
		{
#if 1
			SetParent(pOther);
			SetSolid(SOLID_NONE);
			SetSolidFlags(FSOLID_NOT_SOLID);
#else
			CPhysFixed *m_parent_constraint = ( CPhysFixed * )CreateNoSpawn("phys_constraint", GetAbsOrigin(), vec3_angle);

			hl_constraint_info_t info;
			info.pObjects[ 0 ] = VPhysicsGetObject();
			info.pObjects[ 1 ] = pOther->VPhysicsGetObject();
			info.pGroup = NULL;

			m_parent_constraint->CreateConstraint(NULL, info);
			m_parent_constraint->Spawn();
			m_parent_constraint->Activate();
#endif

			if ( m_glow_trail_handle != NULL )
			{
				m_glow_trail_handle->TurnOn();
				m_glow_trail_handle->FadeAndDie(1.0f);
			}
		}

		if ( FClassnameIs(pOther, "prop_physics") )
		{
			SetSolid(SOLID_VPHYSICS);
			RemoveSolidFlags(FSOLID_NOT_SOLID);
			SetCollisionGroup(COLLISION_GROUP_INTERACTIVE_DEBRIS);
			PhysDisableEntityCollisions(pOther, this);
			PhysDisableEntityCollisions(UTIL_GetLocalPlayer(), this);
			PhysEnableEntityCollisions(VPhysicsGetObject(), g_PhysWorldObject);
		}

		Vector vecVelocity = GetAbsVelocity();

		SetTouch(NULL);
		SetThink(NULL);
		// We're no longer flying. Stop checking for water volumes.
		SetContextThink(NULL, 0, s_szPhysicalBoltBubbles);

		static int s_nImpactCount = 0;
		s_nImpactCount++;
		if ( s_nImpactCount & 0x01 )
		{
			UTIL_ImpactTrace(&tr, DMG_BULLET);

			// Shoot some sparks
			if ( UTIL_PointContents(GetAbsOrigin()) != CONTENTS_WATER )
			{
				g_pEffects->Sparks(GetAbsOrigin());
			}
		}
	}

	void BoltTouch(CBaseEntity *pOther)
	{
		if ( pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER) )
		{
			// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
			if ( ( pOther->m_takedamage == DAMAGE_NO ) || ( pOther->m_takedamage == DAMAGE_EVENTS_ONLY ) )
				return;
		}

		if ( pOther->IsPlayer() )
			return;

		if ( FClassnameIs(pOther, "crossbow_bolt_physical") )
			return;

		trace_t	tr;
		tr = BaseClass::GetTouchTrace();

		if ( pOther->m_takedamage != DAMAGE_NO )
		{
			Vector	vecNormalizedVel = GetAbsVelocity();

			ClearMultiDamage();
			VectorNormalize(vecNormalizedVel);

			float flDamage = sk_plr_dmg_crossbow.GetFloat();
			CBreakable *pBreak = dynamic_cast <CBreakable *>( pOther );
			if ( pBreak && ( pBreak->GetMaterialType() == matGlass ) )
			{
				flDamage = max(pOther->GetHealth(), flDamage);
			}

			CTakeDamageInfo	dmgInfo(this, GetOwnerEntity(), flDamage, DMG_SLASH | DMG_NEVERGIB);
			CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
			dmgInfo.SetDamagePosition(tr.endpos);
#ifdef DARKINTERVAL
			if ( flDamage >= pOther->GetHealth() && pOther->IsNPC() )
			{
				//	pOther->RemoveSpawnFlags(SF_ALWAYS_SERVER_RAGDOLL); // rob them of the flag, so the sticky bolt can pin their ragdoll
				// todo: figure out a real solution for pinning serverside ragdolls.
			}
#endif
			pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);

			if ( m_bolt_fully_heated_boolean )
			{
				CBaseCombatCharacter *basecc = ToBaseCombatCharacter(pOther);
				if ( basecc )
				{
					basecc->Ignite(5.0, true, 8.0f, false);
				}

				// raise the temperature so firesensors can feel the bolt
				// todo: do for partial heats too; scale heat with %
				// (need the crossbow weapon to relay that to the bolt)
				CFire *pFires[ 128 ];
				int fireCount = FireSystem_GetFiresInSphere(pFires, ARRAYSIZE(pFires), false, GetAbsOrigin(), 64);
				for ( int i = 0; i < fireCount; i++ )
				{
					pFires[ i ]->AddHeat(4);
				}
			}

			CBasePlayer *pPlayer = ToBasePlayer(GetOwnerEntity());
			if ( pPlayer )
			{
				gamestats->Event_WeaponHit(pPlayer, true, "weapon_crossbow", dmgInfo);
			}

			ApplyMultiDamage();

			// Keep going through breakable glass.
			if ( pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS )
				return;

			SetAbsVelocity(Vector(0, 0, 0));

			// play body "thwack" sound
			EmitSound("Weapon_Crossbow.BoltHitBody");

			//	StopParticleEffects(this);

			Vector vForward;
			AngleVectors(GetAbsAngles(), &vForward);
			VectorNormalize(vForward);

			trace_t	tr2;
			UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + vForward * 128, MASK_BLOCKLOS, pOther, COLLISION_GROUP_NONE, &tr2);

			if ( tr2.fraction != 1.0f )
			{
				if ( tr2.m_pEnt == NULL || ( tr2.m_pEnt && tr2.m_pEnt->GetMoveType() == MOVETYPE_NONE ) )
				{
					CEffectData	data;

					data.m_vOrigin = tr2.endpos;
					data.m_vNormal = vForward;
					data.m_nEntIndex = tr2.fraction != 1.0f;
					DispatchEffect("BoltImpact", data);

					// Create a ballsocket constraint. This allows pinning serverside ragdolls, as the clinetside BoltImpact code only works with clientside ragdoll edicts.
					//partition->EnumerateElementsInSphere(PARTITION_ENGINE_NON_STATIC_EDICTS, GetAbsOrigin(), 128, false, &ragdollEnum);
					//	CBaseEntity *findRagdoll = NULL;
					//	NDebugOverlay::Sphere(GetAbsOrigin(), 128, 255, 100, 100, true, 5);
					//	while((findRagdoll = gEntList.FindEntityByClassnameWithin(findRagdoll, "prop_ragdoll", GetAbsOrigin(), 128)) != NULL)
					//	{
					//		if (findRagdoll->VPhysicsGetObject())
					//		{
					//			Msg("found nearby ragdoll %s\n", findRagdoll->GetModelName());
					//		}
					//	}
				}
			}

			if ( ( ( pOther->GetMoveType() == MOVETYPE_VPHYSICS ) || ( pOther->GetMoveType() == MOVETYPE_PUSH ) ) && ( ( pOther->GetHealth() > 0 ) || ( pOther->m_takedamage == DAMAGE_EVENTS_ONLY ) ) )
			{
				CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>( pOther );
				if ( pProp )
				{
					pProp->SetInteraction(PROPINTER_PHYSGUN_NOTIFY_CHILDREN);
				}

				// We hit a physics object that survived the impact. Stick to it.
				StickTo(pOther, tr);
			} else
			{
				SetTouch(NULL);
				SetThink(NULL);
				SetContextThink(NULL, 0, s_szPhysicalBoltBubbles);
				UTIL_Remove(this);
			}
		} else
		{
			// See if we struck the world
			if ( pOther->GetMoveType() == MOVETYPE_NONE && !( tr.surface.flags & SURF_SKY ) )
			{
				// if what we hit is static architecture, can stay around for a while.
				Vector vecDir = GetAbsVelocity();
				float speed = VectorNormalize(vecDir);

				// See if we should reflect off this surface
				float hitDot = DotProduct(tr.plane.normal, -vecDir);

				if ( ( hitDot < 0.5f ) && ( speed > 100 ) )
				{
					Vector vReflection = 2.0f * tr.plane.normal * hitDot + vecDir;

					QAngle reflectAngles;

					VectorAngles(vReflection, reflectAngles);

					SetLocalAngles(reflectAngles);

					SetAbsVelocity(vReflection * speed * 0.75f);

					// Start to sink faster
					SetGravity(1.0f);
				} else
				{
					// We hit a physics object that survived the impact. Stick to it.
					StickTo(pOther, tr);
				}

				// Shoot some sparks
				if ( UTIL_PointContents(GetAbsOrigin()) != CONTENTS_WATER )
				{
					g_pEffects->Sparks(GetAbsOrigin());
				}
			} else if ( pOther->GetMoveType() == MOVETYPE_PUSH && FClassnameIs(pOther, "func_breakable") )
			{
				// We hit a func_breakable, stick to it.
				// The MOVETYPE_PUSH is a micro-optimization to cut down on the classname checks.
				StickTo(pOther, tr);
			} else if ( pOther->GetMoveType() == MOVETYPE_VPHYSICS && ( FClassnameIs(pOther, "prop_physics*") || FClassnameIs(pOther, "func_physbox") ) )
			{
				// We hit a non-breakable prop_physics, stick to it.
				StickTo(pOther, tr);
			} else
			{
				// Put a mark unless we've hit the sky
				if ( ( tr.surface.flags & SURF_SKY ) == false )
				{
					UTIL_ImpactTrace(&tr, DMG_BULLET);
				}

				UTIL_Remove(this);
			}
		}
	}

	void BubbleThink()
	{
		SetNextThink(CURTIME + 0.01f, s_szPhysicalBoltBubbles);

		if ( GetWaterLevel() == 0 )
			return;

		UTIL_BubbleTrail(GetAbsOrigin() - GetAbsVelocity() * 0.1f, GetAbsOrigin(), 5);
	}

	CHandle<CSprite>		m_glow_sprite_handle;
	CHandle<CSpriteTrail>	m_glow_trail_handle;
	bool CreateSprites(bool bHot = true)
	{
		if ( bHot )
		{
			// Start up the eye glow
			m_glow_sprite_handle = CSprite::SpriteCreate("sprites/light_glow02_noz.vmt", GetLocalOrigin(), false);

			if ( m_glow_sprite_handle != NULL )
			{
				m_glow_sprite_handle->FollowEntity(this);
				m_glow_sprite_handle->SetTransparency(kRenderGlow, 255, 255, 255, 128, kRenderFxNoDissipation);
				m_glow_sprite_handle->SetScale(0.2f);
				//	m_glow_sprite_handle->TurnOff();
			}

			// Start up the eye trail
			m_glow_trail_handle = CSpriteTrail::SpriteTrailCreate("sprites/bluelaser1.vmt", GetLocalOrigin(), false);
		} else
		{
			m_glow_trail_handle = CSpriteTrail::SpriteTrailCreate("sprites/zbeam3.vmt", GetLocalOrigin(), false);
		}
		if ( m_glow_trail_handle != NULL )
		{
			m_glow_trail_handle->FollowEntity(this);
			if ( bHot )
			{
				m_glow_trail_handle->SetTransparency(kRenderTransAdd, 255, 25, 0, 255, kRenderFxNone);
				m_glow_trail_handle->SetStartWidth(8.0f);
				m_glow_trail_handle->SetEndWidth(1.0f);
			} else
			{
				m_glow_trail_handle->SetTransparency(kRenderTransAdd, 100, 100, 105, 150, kRenderFxNone);
				m_glow_trail_handle->SetStartWidth(2.0f);
				m_glow_trail_handle->SetEndWidth(0.25f);
			}
			m_glow_trail_handle->SetLifeTime(0.5f);
			//	m_glow_trail_handle->TurnOff();
		}

		return true;
	}

	Vector m_vecShootPosition;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(crossbow_bolt_physical, CPhysicalBolt);

BEGIN_DATADESC(CPhysicalBolt)

DEFINE_THINKFUNC(BubbleThink),
DEFINE_FIELD(m_bolt_fully_heated_boolean, FIELD_BOOLEAN),
DEFINE_FIELD(m_glow_sprite_handle, FIELD_EHANDLE),
DEFINE_FIELD(m_glow_trail_handle, FIELD_EHANDLE),
DEFINE_ENTITYFUNC(BoltTouch),
DEFINE_FIELD(m_vecShootPosition, FIELD_POSITION_VECTOR),

END_DATADESC()
#endif // DI's physical crossbow bolt
//-----------------------------------------------------------------------------
// CWeaponCrossbow
//-----------------------------------------------------------------------------

class CWeaponCrossbow : public CBaseHLCombatWeapon
{
	DECLARE_CLASS(CWeaponCrossbow, CBaseHLCombatWeapon);
public:
	CWeaponCrossbow(void);
#ifdef DARKINTERVAL
	~CWeaponCrossbow(void);
#endif
	virtual void	Precache(void);
	virtual void	PrimaryAttack(void);
	virtual void	SecondaryAttack(void);
	virtual bool	Deploy(void);
	virtual void	Drop(const Vector &vecVelocity);
	virtual bool	Holster(CBaseCombatWeapon *pSwitchingTo = NULL);
	virtual bool	Reload(void);
	virtual void	ItemPostFrame(void);
#ifndef DARKINTERVAL
	virtual void	ItemBusyFrame(void);
#endif
	virtual void	Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);
	virtual bool	SendWeaponAnim(int iActivity);
	virtual bool	IsWeaponZoomed() { return m_is_zoomed_boolean; }

#ifndef DARKINTERVAL
	bool	ShouldDisplayHUDHint() { return true; }
#endif

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:

	void	StopEffects(void);
	void	SetSkin(int skinNum);
	void	CheckZoomToggle(void);
	void	FireBolt(void);
	void	ToggleZoom(void);

	// Various states for the crossbow's charger
	enum ChargerState_t
	{
		CHARGER_STATE_START_LOAD,
		CHARGER_STATE_START_CHARGE,
		CHARGER_STATE_READY,
		CHARGER_STATE_DISCHARGE,
		CHARGER_STATE_OFF,
	};

	void	CreateChargerEffects(void);
	void	SetChargerState(ChargerState_t state);
	void	DoLoadEffect(void);

private:

	// Charger effects
	ChargerState_t		m_charger_state_int;
	CHandle<CSprite>	m_charger_sprite_handle;
#ifdef DARKINTERVAL
	CSoundPatch			*m_heating_sound_patch;
#endif
	bool				m_is_zoomed_boolean;
	bool				m_must_reload_boolean;
#ifdef DARKINTERVAL
public:
	CNetworkVar(float, m_heating_amount_float);
#endif
};

LINK_ENTITY_TO_CLASS(weapon_crossbow, CWeaponCrossbow);

PRECACHE_WEAPON_REGISTER(weapon_crossbow);

IMPLEMENT_SERVERCLASS_ST(CWeaponCrossbow, DT_WeaponCrossbow)
#ifdef DARKINTERVAL
SendPropFloat(SENDINFO(m_heating_amount_float)),
#endif
END_SEND_TABLE()

BEGIN_DATADESC(CWeaponCrossbow)

DEFINE_FIELD(m_is_zoomed_boolean, FIELD_BOOLEAN),
DEFINE_FIELD(m_must_reload_boolean, FIELD_BOOLEAN),
DEFINE_FIELD(m_charger_state_int, FIELD_INTEGER),
DEFINE_FIELD(m_charger_sprite_handle, FIELD_EHANDLE),
#ifdef DARKINTERVAL
DEFINE_FIELD(m_heating_amount_float, FIELD_FLOAT),
#endif
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponCrossbow::CWeaponCrossbow(void)
{
	m_reloads_singly_boolean = true;
	m_can_fire_underwater_boolean = true;
	m_can_altfire_underwater_boolean = true;
	m_is_zoomed_boolean = false;
	m_must_reload_boolean = false;
#ifdef DARKINTERVAL
	m_heating_amount_float = 0.0f;
#endif
}
#ifdef DARKINTERVAL
CWeaponCrossbow::~CWeaponCrossbow(void)
{
	m_heating_amount_float = 0.0f;

	if ( m_heating_sound_patch != NULL )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundDestroy(m_heating_sound_patch);
		m_heating_sound_patch = NULL;
	}
}
#endif
#define	CROSSBOW_GLOW_SPRITE	"sprites/light_glow02_noz.vmt"
#define	CROSSBOW_GLOW_SPRITE2	"sprites/blueflare1.vmt"

#define DI_USE_PHYSICAL_BOLTS 1
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::Precache(void)
{
#ifdef DARKINTERVAL
#if DI_USE_PHYSICAL_BOLTS
	UTIL_PrecacheOther("crossbow_bolt_physical");
#else
	UTIL_PrecacheOther("crossbow_bolt");
#endif
#else
	UTIL_PrecacheOther("crossbow_bolt");
#endif
	PrecacheScriptSound("Weapon_Crossbow.BoltHitBody");
	PrecacheScriptSound("Weapon_Crossbow.BoltHitWorld");
	PrecacheScriptSound("Weapon_Crossbow.BoltSkewer");
	PrecacheScriptSound("DI_Crossbow.HeatingLoop");

	PrecacheModel(CROSSBOW_GLOW_SPRITE);
	PrecacheModel(CROSSBOW_GLOW_SPRITE2);

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCrossbow::PrimaryAttack(void)
{
#ifdef DARKINTERVAL
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if ( pPlayer->m_nButtons & IN_ATTACK2 ) return; // Don't allow firing while RMB is pressed.
#endif
	FireBolt();

	// Signal a reload
	m_must_reload_boolean = true;

	SetWeaponIdleTime(CURTIME + SequenceDuration(ACT_VM_PRIMARYATTACK));

#ifndef DARKINTERVAL
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
#endif
	if ( pPlayer )
	{
		m_iPrimaryAttacks++;
		gamestats->Event_WeaponFired(pPlayer, true, GetClassname());
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SecondaryAttack(void)
{
#ifdef DARKINTERVAL
	if ( m_heating_amount_float == 0.0f && m_charger_state_int == CHARGER_STATE_START_LOAD )
	{
		//Start charging sound
		CPASAttenuationFilter filter(this);
		m_heating_sound_patch = ( CSoundEnvelopeController::GetController() ).SoundCreate(filter, entindex(),
			CHAN_STATIC, "DI_Crossbow.HeatingLoop", ATTN_NORM);
		assert(m_heating_sound_patch != NULL);
		if ( m_heating_sound_patch != NULL )
		{
			CSoundEnvelopeController::GetController().Play(m_heating_sound_patch, 1.0f, 50);
			CSoundEnvelopeController::GetController().SoundChangePitch(m_heating_sound_patch, 150, 3.0f);
		}
		return;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Reload(void)
{
	if ( BaseClass::Reload() )
	{
		m_must_reload_boolean = false;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::CheckZoomToggle(void)
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

	if ( pPlayer->m_afButtonPressed & IN_ATTACK2 )
	{
		ToggleZoom();
	}
}
#ifndef DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ItemBusyFrame(void)
{
	// Allow zoom toggling even when we're reloading
	CheckZoomToggle();
}
#endif
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ItemPostFrame(void)
{
#ifndef DARKINTERVAL
	// Allow zoom toggling
	CheckZoomToggle();
#endif
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if ( !pPlayer )
	{
		return;
	}
#ifdef DARKINTERVAL
	if ( pPlayer->m_nButtons & IN_ATTACK2 )
	{
		SecondaryAttack();

		m_heating_amount_float += ( 0.01f * sk_crossbow_heatup_mult.GetFloat() );

		if ( m_heating_amount_float > 0.01f && m_charger_state_int < CHARGER_STATE_START_CHARGE )
		{
			SetChargerState(CHARGER_STATE_START_CHARGE);
		}

		if ( m_heating_amount_float > 1.00f )
		{
			if ( m_charger_state_int != CHARGER_STATE_READY )
			{
				SetChargerState(CHARGER_STATE_READY);
			}

			m_heating_amount_float = 1.00f;
		}

	} else if ( pPlayer->m_afButtonReleased & IN_ATTACK2 )
	{
		if ( m_heating_sound_patch != NULL )
			( CSoundEnvelopeController::GetController() ).SoundFadeOut(m_heating_sound_patch, 0.1f);
		FireBolt();
		m_heating_amount_float = 0.0f;
		m_must_reload_boolean = true;
	} else
	{
		if ( m_heating_sound_patch != NULL )
			( CSoundEnvelopeController::GetController() ).SoundFadeOut(m_heating_sound_patch, 0.1f);
	}
	//	Msg("Bolt heated up to %.2f\n", m_heating_amount_float);
#endif
	if ( m_must_reload_boolean && HasWeaponIdleTimeElapsed() )
	{
		Reload();
	}
#ifdef DARKINTERVAL
	pPlayer->m_Local.m_WeaponProxyValueSlot01_float = m_heating_amount_float;
#endif
	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::FireBolt(void)
{
	if ( m_iClip1 <= 0 )
	{
		if ( !m_bFireOnEmpty )
		{
			Reload();
		} else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if ( pOwner == NULL )
		return;

	pOwner->RumbleEffect(RUMBLE_357, 0, RUMBLE_FLAG_RESTART);

	Vector vecAiming = pOwner->GetAutoaimVector(0);
#ifdef DARKINTERVAL
	Vector vecSrc = pOwner->Weapon_TrueShootPosition_Front();
	vecSrc.x -= 4;
	vecSrc.z -= 1;
	vecSrc.y += 1.5;
	//	if (pOwner->GetViewModel())
	//	{
	//		pOwner->GetViewModel()->GetAttachment(pOwner->GetViewModel()->LookupAttachment("bolt_start"), vecSrc);
	//	}
#else
	Vector vecSrc = pOwner->Weapon_ShootPosition();
#endif

	QAngle angAiming;
	VectorAngles(vecAiming, angAiming);
#ifdef DARKINTERVAL
	vecAiming *= sk_crossbow_bolt_speed.GetFloat();
#if DI_USE_PHYSICAL_BOLTS
	CPhysicalBolt *pBolt = CPhysicalBolt::CreatePhysicalBolt(vecSrc, angAiming);
#else
	CCrossbowBolt *pBolt = CCrossbowBolt::BoltCreate(vecSrc, angAiming, pOwner, ( m_charger_state_int == CHARGER_STATE_READY ));
#endif
	if ( pOwner->GetWaterLevel() == 3 )
	{
		pBolt->SetAbsVelocity(vecAiming * BOLT_WATER_VELOCITY);
	} else
	{
		pBolt->SetAbsVelocity(vecAiming * BOLT_AIR_VELOCITY);
	}

	pBolt->m_bolt_fully_heated_boolean = ( m_charger_state_int == CHARGER_STATE_READY );
	pBolt->Spawn();
	pBolt->Activate();
	pBolt->AddEffects(EF_NOSHADOW);
	pBolt->Shoot(vecAiming);
#else // not DI
	CCrossbowBolt *pBolt = CCrossbowBolt::BoltCreate(vecSrc, angAiming, pOwner);

	if ( pOwner->GetWaterLevel() == 3 )
	{
		pBolt->SetAbsVelocity(vecAiming * BOLT_WATER_VELOCITY);
	} else
	{
		pBolt->SetAbsVelocity(vecAiming * BOLT_AIR_VELOCITY);
	}
#endif

	if ( !( GetOwner()->GetFlags() & FL_GODMODE ) )
	{
		m_iClip1--;
	}

	pOwner->ViewPunch(QAngle(-2, 0, 0));

	WeaponSound(SINGLE);
	WeaponSound(SPECIAL2);

	CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 200, 0.2);

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);

	if ( !m_iClip1 && pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pOwner->SetSuitUpdate("!HEVVOXNEW_AMMODEPLETED", true, 0);
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = CURTIME + 0.75;

	DoLoadEffect();
	SetChargerState(CHARGER_STATE_DISCHARGE);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Deploy(void)
{
	if ( m_iClip1 <= 0 )
	{
		return DefaultDeploy(( char* )GetViewModel(), ( char* )GetWorldModel(), ACT_CROSSBOW_DRAW_UNLOADED, ( char* )GetAnimPrefix());
	}

	SetSkin(BOLT_SKIN_GLOW);

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSwitchingTo - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Holster(CBaseCombatWeapon *pSwitchingTo)
{
#ifdef DARKINTERVAL
	if ( m_heating_amount_float > 0.00f ) m_heating_amount_float = 0;
	SetChargerState(CHARGER_STATE_OFF);
	if ( m_heating_sound_patch != NULL )
		( CSoundEnvelopeController::GetController() ).SoundFadeOut(m_heating_sound_patch, 0.1f);
	SetSkin(BOLT_SKIN_NORMAL);
#endif
	StopEffects();
	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ToggleZoom(void)
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

	if ( pPlayer == NULL )
		return;

	if ( m_is_zoomed_boolean )
	{
		if ( pPlayer->SetFOV(this, 0, 0.2f) )
		{
			m_is_zoomed_boolean = false;
		}
	} else
	{
		if ( pPlayer->SetFOV(this, 20, 0.1f) )
		{
			m_is_zoomed_boolean = true;
		}
	}
}

#define	BOLT_TIP_ATTACHMENT	2

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::CreateChargerEffects(void)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if ( m_charger_sprite_handle != NULL )
		return;

	m_charger_sprite_handle = CSprite::SpriteCreate(CROSSBOW_GLOW_SPRITE, GetAbsOrigin(), false);

	if ( m_charger_sprite_handle )
	{
		m_charger_sprite_handle->SetAttachment(pOwner->GetViewModel(), BOLT_TIP_ATTACHMENT);
		m_charger_sprite_handle->SetTransparency(kRenderTransAdd, 255, 128, 0, 255, kRenderFxNoDissipation);
		m_charger_sprite_handle->SetBrightness(0);
		m_charger_sprite_handle->SetScale(0.1f);
		m_charger_sprite_handle->TurnOff();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : skinNum - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SetSkin(int skinNum)
{
#ifndef DARKINTERVAL
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if ( pOwner == NULL )
		return;

	CBaseViewModel *pViewModel = pOwner->GetViewModel();

	if ( pViewModel == NULL )
		return;

	pViewModel->m_nSkin = skinNum;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::DoLoadEffect(void)
{
	SetSkin(BOLT_SKIN_GLOW);

	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if ( pOwner == NULL )
		return;

	CBaseViewModel *pViewModel = pOwner->GetViewModel();

	if ( pViewModel == NULL )
		return;

	CEffectData	data;

	data.m_nEntIndex = pViewModel->entindex();
	data.m_nAttachmentIndex = 1;

	DispatchEffect("CrossbowLoad", data);

	CSprite *pBlast = CSprite::SpriteCreate(CROSSBOW_GLOW_SPRITE2, GetAbsOrigin(), false);

	if ( pBlast )
	{
		pBlast->SetAttachment(pOwner->GetViewModel(), 1);
		pBlast->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone);
		pBlast->SetBrightness(128);
		pBlast->SetScale(0.2f);
		pBlast->FadeOutFromSpawn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SetChargerState(ChargerState_t state)
{
	// Make sure we're setup
	CreateChargerEffects();

	// Don't do this twice
	if ( state == m_charger_state_int )
		return;

	m_charger_state_int = state;

	switch ( m_charger_state_int )
	{
		case CHARGER_STATE_START_LOAD:

			WeaponSound(SPECIAL1);

			// Shoot some sparks and draw a beam between the two outer points
			DoLoadEffect();

			break;

		case CHARGER_STATE_START_CHARGE:
		{
			if ( m_charger_sprite_handle == NULL )
				break;

			m_charger_sprite_handle->SetBrightness(32, 0.5f);
			m_charger_sprite_handle->SetScale(0.025f, 0.5f);
#ifdef DARKINTERVAL
			m_charger_sprite_handle->TurnOff();
#else
			m_charger_sprite_handle->TurnOn();
#endif
		}

		break;

		case CHARGER_STATE_READY:
		{
			// Get fully charged
			if ( m_charger_sprite_handle == NULL )
				break;

			m_charger_sprite_handle->SetBrightness(80, 1.0f);
			m_charger_sprite_handle->SetScale(0.1f, 0.5f);
			m_charger_sprite_handle->TurnOn();
#ifdef DARKINTERVAL
			DoLoadEffect();
			WeaponSound(SPECIAL1);
#endif
		}

		break;

		case CHARGER_STATE_DISCHARGE:
		{
			SetSkin(BOLT_SKIN_NORMAL);

			if ( m_charger_sprite_handle == NULL )
				break;

			m_charger_sprite_handle->SetBrightness(0);
			m_charger_sprite_handle->TurnOff();
		}

		break;

		case CHARGER_STATE_OFF:
		{
			SetSkin(BOLT_SKIN_NORMAL);

			if ( m_charger_sprite_handle == NULL )
				break;

			m_charger_sprite_handle->SetBrightness(0);
			m_charger_sprite_handle->TurnOff();
		}
		break;

		default:
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
	switch ( pEvent->event )
	{
		case EVENT_WEAPON_THROW:
			SetChargerState(CHARGER_STATE_START_LOAD);
			break;
#ifndef DARKINTERVAL
		case EVENT_WEAPON_THROW2:
			SetChargerState(CHARGER_STATE_START_CHARGE);
			break;

		case EVENT_WEAPON_THROW3:
			SetChargerState(CHARGER_STATE_READY);
			break;
#endif
		default:
			BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the desired activity for the weapon and its viewmodel counterpart
// Input  : iActivity - activity to play
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::SendWeaponAnim(int iActivity)
{
	int newActivity = iActivity;

	// The last shot needs a non-loaded activity
	if ( ( newActivity == ACT_VM_IDLE ) && ( m_iClip1 <= 0 ) )
	{
		newActivity = ACT_VM_FIDGET;
	}

	//For now, just set the ideal activity and be done with it
	return BaseClass::SendWeaponAnim(newActivity);
}

//-----------------------------------------------------------------------------
// Purpose: Stop all zooming and special effects on the viewmodel
//-----------------------------------------------------------------------------
void CWeaponCrossbow::StopEffects(void)
{
	// Stop zooming
	if ( m_is_zoomed_boolean )
	{
		ToggleZoom();
	}

	// Turn off our sprites
	SetChargerState(CHARGER_STATE_OFF);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::Drop(const Vector &vecVelocity)
{
#ifdef DARKINTERVAL
	if ( m_heating_sound_patch != NULL )
		( CSoundEnvelopeController::GetController() ).SoundFadeOut(m_heating_sound_patch, 0.1f);
	m_heating_amount_float = 0.0f;
#endif
	StopEffects();
	BaseClass::Drop(vecVelocity);
}
#endif

//====================================
// Frag grenade (HL2).
// DI adds cooking to the grenade.
//====================================
#ifdef FRAG_GRENADE
#define GRENADE_TIMER	3.0f //Seconds

#define GRENADE_PAUSED_NO			0
#define GRENADE_PAUSED_PRIMARY		1
#define GRENADE_PAUSED_SECONDARY	2

#define GRENADE_RADIUS	4.0f // inches

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CWeaponFrag : public CBaseHLCombatWeapon
{
	DECLARE_CLASS(CWeaponFrag, CBaseHLCombatWeapon);
public:
	DECLARE_SERVERCLASS();

public:
	CWeaponFrag();

	void	Precache(void);
	void	Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);
	void	PrimaryAttack(void);
	void	SecondaryAttack(void);
	void	DecrementAmmo(CBaseCombatCharacter *pOwner);
	void	ItemPostFrame(void);

	bool	Deploy(void);
	bool	Holster(CBaseCombatWeapon *pSwitchingTo = NULL);

	int		CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	bool	Reload(void);

	bool	ShouldDisplayHUDHint() { return true; }
#ifdef DARKINTERVAL
	//	bool	ShouldDisplayAltFireHUDHint() { return true; }
#endif
private:
#ifdef DARKINTERVAL
	void	ThrowGrenade(CBasePlayer *pPlayer, bool lob);
#else
	void	ThrowGrenade(CBasePlayer *pPlayer);
	void	LobGrenade(CBasePlayer *pPlayer);
#endif
	void	RollGrenade(CBasePlayer *pPlayer);
	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
	void	CheckThrowPosition(CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc);

	bool	m_bRedraw;	//Draw the weapon again after throwing a grenade

	int		m_AttackPaused;
	bool	m_bDrawbackFinished;
#ifdef DARKINTERVAL // for cooking
	float	m_flStartThrow;
	float	m_flNextBlipTime;
#endif
	DECLARE_ACTTABLE();

	DECLARE_DATADESC();
};

CWeaponFrag::CWeaponFrag() :
	CBaseHLCombatWeapon(),
	m_bRedraw(false)
{
	NULL;
#ifdef DARKINTERVAL
	m_can_fire_underwater_boolean = true; // DI allows throwing and lobbing grenades under water
	m_can_altfire_underwater_boolean = true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::Precache(void)
{
	BaseClass::Precache();

	UTIL_PrecacheOther("npc_grenade_frag");

	PrecacheScriptSound("WeaponFrag.Throw");
	PrecacheScriptSound("WeaponFrag.Roll");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponFrag::Deploy(void)
{
	m_bRedraw = false;
	m_bDrawbackFinished = false;

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponFrag::Holster(CBaseCombatWeapon *pSwitchingTo)
{
	m_bRedraw = false;
	m_bDrawbackFinished = false;

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponFrag::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());
	bool fThrewGrenade = false;

	switch ( pEvent->event )
	{
		case EVENT_WEAPON_SEQUENCE_FINISHED:
			m_bDrawbackFinished = true;
			break;

		case EVENT_WEAPON_THROW:
#ifdef DARKINTERVAL
			ThrowGrenade(pOwner, false);
#else
			ThrowGrenade(pOwner);
#endif
			DecrementAmmo(pOwner);
			fThrewGrenade = true;
			break;

		case EVENT_WEAPON_THROW2:
			RollGrenade(pOwner);
			DecrementAmmo(pOwner);
			fThrewGrenade = true;
			break;

		case EVENT_WEAPON_THROW3:
#ifdef DARKINTERVAL
			ThrowGrenade(pOwner, true);
#else
			LobGrenade(pOwner);
#endif
			DecrementAmmo(pOwner);
			fThrewGrenade = true;
			break;

		default:
			BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
			break;
	}

#define RETHROW_DELAY	0.5
	if ( fThrewGrenade )
	{
		m_flNextPrimaryAttack = CURTIME + RETHROW_DELAY;
		m_flNextSecondaryAttack = CURTIME + RETHROW_DELAY;
		m_flTimeWeaponIdle = FLT_MAX; //NOTE: This is set once the animation has finished up!

									  // Make a sound designed to scare snipers back into their holes!
		CBaseCombatCharacter *pOwner = GetOwner();

		if ( pOwner )
		{
			Vector vecSrc = pOwner->Weapon_ShootPosition();
			Vector	vecDir;

			AngleVectors(pOwner->EyeAngles(), &vecDir);

			trace_t tr;

			UTIL_TraceLine(vecSrc, vecSrc + vecDir * 1024, MASK_SOLID_BRUSHONLY, pOwner, COLLISION_GROUP_NONE, &tr);

			CSoundEnt::InsertSound(SOUND_DANGER_SNIPERONLY, tr.endpos, 384, 0.2, pOwner);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponFrag::Reload(void)
{
	if ( !HasPrimaryAmmo() )
		return false;

	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= CURTIME ) && ( m_flNextSecondaryAttack <= CURTIME ) )
	{
		//Redraw the weapon
		SendWeaponAnim(ACT_VM_DRAW);

		//Update our times
		m_flNextPrimaryAttack = CURTIME + SequenceDuration();
		m_flNextSecondaryAttack = CURTIME + SequenceDuration();
		m_flTimeWeaponIdle = CURTIME + SequenceDuration();

		//Mark this as done
		m_bRedraw = false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::SecondaryAttack(void)
{
	if ( m_bRedraw )
		return;

	if ( !HasPrimaryAmmo() )
		return;

	CBaseCombatCharacter *pOwner = GetOwner();

	if ( pOwner == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer(pOwner);

	if ( pPlayer == NULL )
		return;

	// Note that this is a secondary attack and prepare the grenade attack to pause.
	m_AttackPaused = GRENADE_PAUSED_SECONDARY;
	SendWeaponAnim(ACT_VM_PULLBACK_LOW);
#ifdef DARKINTERVAL
	m_flStartThrow = CURTIME + GRENADE_TIMER;
	WeaponSound(SPECIAL2); // blip in hand
	m_flNextBlipTime = CURTIME + 1.0f;
#endif
	// Don't let weapon idle interfere in the middle of a throw!
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextSecondaryAttack = FLT_MAX;

	// If I'm now out of ammo, switch away
	if ( !HasPrimaryAmmo() )
	{
		pPlayer->SwitchToNextBestWeapon(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::PrimaryAttack(void)
{
	if ( m_bRedraw )
		return;

	CBaseCombatCharacter *pOwner = GetOwner();

	if ( pOwner == NULL )
	{
		return;
	}

	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());;

	if ( !pPlayer )
		return;

	// Note that this is a primary attack and prepare the grenade attack to pause.
	m_AttackPaused = GRENADE_PAUSED_PRIMARY;
	SendWeaponAnim(ACT_VM_PULLBACK_HIGH);
#ifdef DARKINTERVAL
	m_flStartThrow = CURTIME + GRENADE_TIMER;
	WeaponSound(SPECIAL2);
	m_flNextBlipTime = CURTIME + 1.0f;
#endif
	// Put both of these off indefinitely. We do not know how long
	// the player will hold the grenade.
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextPrimaryAttack = FLT_MAX;

	// If I'm now out of ammo, switch away
	if ( !HasPrimaryAmmo() )
	{
		pPlayer->SwitchToNextBestWeapon(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponFrag::DecrementAmmo(CBaseCombatCharacter *pOwner)
{
	pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::ItemPostFrame(void)
{
	if ( m_bDrawbackFinished )
	{
		CBasePlayer *pOwner = ToBasePlayer(GetOwner());

		if ( pOwner )
		{
			switch ( m_AttackPaused )
			{
				case GRENADE_PAUSED_PRIMARY:
#ifdef DARKINTERVAL
					if ( !( pOwner->m_nButtons & IN_ATTACK ) || CURTIME >= m_flStartThrow )
#else
					if ( !( pOwner->m_nButtons & IN_ATTACK ) )
#endif
					{
						SendWeaponAnim(ACT_VM_THROW);
						m_bDrawbackFinished = false;
					}
					break;

				case GRENADE_PAUSED_SECONDARY:
#ifdef DARKINTERVAL
					if ( !( pOwner->m_nButtons & IN_ATTACK2 ) || CURTIME >= m_flStartThrow )
#else
					if ( !( pOwner->m_nButtons & IN_ATTACK2 ) )
#endif
					{
						//See if we're ducking
						if ( pOwner->m_nButtons & IN_DUCK )
						{
							//Send the weapon animation
							SendWeaponAnim(ACT_VM_SECONDARYATTACK);
						} else
						{
							//Send the weapon animation
							SendWeaponAnim(ACT_VM_HAULBACK);
						}

						m_bDrawbackFinished = false;
					}
					break;

				default:
					break;
			}
#ifdef DARKINTERVAL
			if ( CURTIME > m_flNextBlipTime )
			{
				WeaponSound(SPECIAL2);
				m_flNextBlipTime = CURTIME + 1.0f;

				if ( CURTIME >= m_flStartThrow - 1.5f )
				{
					m_flNextBlipTime = CURTIME + 0.3f;
				}
			}
#endif
		}
	}

	BaseClass::ItemPostFrame();

	if ( m_bRedraw )
	{
		if ( IsViewModelSequenceFinished() )
		{
			Reload();
		}
	}
}

// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
void CWeaponFrag::CheckThrowPosition(CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc)
{
	trace_t tr;

	UTIL_TraceHull(vecEye, vecSrc, -Vector(GRENADE_RADIUS + 2, GRENADE_RADIUS + 2, GRENADE_RADIUS + 2), Vector(GRENADE_RADIUS + 2, GRENADE_RADIUS + 2, GRENADE_RADIUS + 2),
		pPlayer->PhysicsSolidMaskForEntity(), pPlayer, pPlayer->GetCollisionGroup(), &tr);

	if ( tr.DidHit() )
	{
		vecSrc = tr.endpos;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
#ifdef DARKINTERVAL
void CWeaponFrag::ThrowGrenade(CBasePlayer *pPlayer, bool lob)
{
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors(&vForward, &vRight, NULL);
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f;
	if ( lob ) vecSrc += Vector(0, 0, -8);

	CheckThrowPosition(pPlayer, vecEye, vecSrc);
	//	vForward[0] += 0.1f;
	vForward[ 2 ] += 0.1f;

	Vector vecThrow;
	pPlayer->GetVelocity(&vecThrow, NULL);

	if ( lob )
		vecThrow += vForward * 350 + Vector(0, 0, 50);
	else
		vecThrow += vForward * 1200;

	float timeToExplode = m_flStartThrow - CURTIME; // DI - deduct the time spent cooking
	if ( timeToExplode < 0 ) timeToExplode = 0;

	Fraggrenade_Create(vecSrc, vec3_angle, vecThrow, AngularImpulse(lob ? 200 : 600, lob ? RandomInt(-600, 600) : RandomInt(-1200, 1200), 0), pPlayer, timeToExplode, false, 0, m_flNextBlipTime);
	m_flNextBlipTime = CURTIME + FLT_MAX; // until it gets reset by the next attack
	m_bRedraw = true;

	WeaponSound(lob ? WPN_DOUBLE : SINGLE);

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(pPlayer, true, GetClassname());
}
#else
void CWeaponFrag::ThrowGrenade(CBasePlayer *pPlayer)
{
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors(&vForward, &vRight, NULL);
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f;
	CheckThrowPosition(pPlayer, vecEye, vecSrc);
	//	vForward[0] += 0.1f;
	vForward[ 2 ] += 0.1f;

	Vector vecThrow;
	pPlayer->GetVelocity(&vecThrow, NULL);
	vecThrow += vForward * 1200;
	Fraggrenade_Create(vecSrc, vec3_angle, vecThrow, AngularImpulse(600, random->RandomInt(-1200, 1200), 0), pPlayer, GRENADE_TIMER, false);

	m_bRedraw = true;

	WeaponSound(SINGLE);

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(pPlayer, true, GetClassname());
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponFrag::LobGrenade(CBasePlayer *pPlayer)
{
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors(&vForward, &vRight, NULL);
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f + Vector(0, 0, -8);
	CheckThrowPosition(pPlayer, vecEye, vecSrc);

	Vector vecThrow;
	pPlayer->GetVelocity(&vecThrow, NULL);
	vecThrow += vForward * 350 + Vector(0, 0, 50);
	Fraggrenade_Create(vecSrc, vec3_angle, vecThrow, AngularImpulse(200, random->RandomInt(-600, 600), 0), pPlayer, GRENADE_TIMER, false);

	WeaponSound(WPN_DOUBLE);

	m_bRedraw = true;

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(pPlayer, true, GetClassname());
}
#endif // DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponFrag::RollGrenade(CBasePlayer *pPlayer)
{
	// BUGBUG: Hardcoded grenade width of 4 - better not change the model :)
	Vector vecSrc;
	pPlayer->CollisionProp()->NormalizedToWorldSpace(Vector(0.5f, 0.5f, 0.0f), &vecSrc);
	vecSrc.z += GRENADE_RADIUS;

	Vector vecFacing = pPlayer->BodyDirection2D();
	// no up/down direction
	vecFacing.z = 0;
	VectorNormalize(vecFacing);
	trace_t tr;
	UTIL_TraceLine(vecSrc, vecSrc - Vector(0, 0, 16), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_NONE, &tr);
	if ( tr.fraction != 1.0 )
	{
		// compute forward vec parallel to floor plane and roll grenade along that
		Vector tangent;
		CrossProduct(vecFacing, tr.plane.normal, tangent);
		CrossProduct(tr.plane.normal, tangent, vecFacing);
	}
	vecSrc += ( vecFacing * 18.0 );
	CheckThrowPosition(pPlayer, pPlayer->WorldSpaceCenter(), vecSrc);

	Vector vecThrow;
	pPlayer->GetVelocity(&vecThrow, NULL);
	vecThrow += vecFacing * 700;
	// put it on its side
	QAngle orientation(0, pPlayer->GetLocalAngles().y, -90);
	// roll it
	AngularImpulse rotSpeed(0, 0, 720);
#ifdef DARKINTERVAL // cooking enabled
	float timeToExplode = m_flStartThrow - CURTIME;
	if ( timeToExplode < 0 ) timeToExplode = 0;
	Fraggrenade_Create(vecSrc, orientation, vecThrow, rotSpeed, pPlayer, timeToExplode, false, 0, m_flNextBlipTime);
#else
	Fraggrenade_Create(vecSrc, orientation, vecThrow, rotSpeed, pPlayer, GRENADE_TIMER, false);
#endif
	WeaponSound(SPECIAL1);

	m_bRedraw = true;

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(pPlayer, true, GetClassname());
}

BEGIN_DATADESC(CWeaponFrag)
DEFINE_FIELD(m_bRedraw, FIELD_BOOLEAN),
DEFINE_FIELD(m_AttackPaused, FIELD_INTEGER),
DEFINE_FIELD(m_bDrawbackFinished, FIELD_BOOLEAN),
#ifdef DARKINTERVAL
DEFINE_FIELD(m_flStartThrow, FIELD_TIME),
DEFINE_FIELD(m_flNextBlipTime, FIELD_TIME),
#endif
END_DATADESC()

acttable_t	CWeaponFrag::m_acttable[] =
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SLAM, true },
};

IMPLEMENT_ACTTABLE(CWeaponFrag);

IMPLEMENT_SERVERCLASS_ST(CWeaponFrag, DT_WeaponFrag)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_frag, CWeaponFrag);
PRECACHE_WEAPON_REGISTER(weapon_frag);
#endif
