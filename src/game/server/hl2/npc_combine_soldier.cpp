//========================================================================//
//
// Purpose: This is the soldier version of the combine, analogous to the HL1 grunt.
// DI todo - put Combine Soldiers on the NPC script base
// DI gets rid of HL2_EPISODIC conditionals for march anims because they're 
// routinely used here and there in DI.
// DI also gets rid of several hl2_episodic convar checks as again, those
// behaviours are used by default for soldiers.
//=============================================================================//

#include "cbase.h"
#include "npc_combine_soldier.h"

#include "ai_hull.h"
#include "ai_memory.h"
#include "ai_motor.h"
#include "ammodef.h"
#include "bitstring.h"
#include "engine/ienginesound.h"
#ifndef DARKINTERVAL // unused
#include "env_explosion.h"
#endif
#include "game.h"
#include "gameweaponmanager.h"
#include "hl2/hl2_player.h"
#include "hl2_gamerules.h"
#include "ndebugoverlay.h"
#include "npcevent.h"
#ifndef DARKINTERVAL // unused
#include "sprite.h"
#endif
#include "soundent.h"
#include "soundenvelope.h"
#include "vehicle_base.h"
#include "weapon_physcannon.h"

#ifdef DARKINTERVAL // for random nameplates
#include "filesystem.h"
#include "fmtstr.h"
#include "stringpool.h"
#include "utlbuffer.h"
#endif
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_combine_s_health( "sk_combine_s_health","0");
ConVar	sk_combine_s_kick( "sk_combine_s_kick","0");

ConVar sk_combine_guard_health( "sk_combine_guard_health", "0");
ConVar sk_combine_guard_kick( "sk_combine_guard_kick", "0");
#ifndef DARKINTERVAL // reducing amount of convars
// Whether or not the combine guard should spawn health on death
ConVar combine_guard_spawn_health( "combine_guard_spawn_health", "1" );
//Whether or not the combine should spawn health on death
ConVar	combine_spawn_health( "combine_spawn_health", "1" );
#endif
extern ConVar sk_plr_dmg_buckshot;	
extern ConVar sk_plr_num_shotgun_pellets;

#define AE_SOLDIER_BLOCK_PHYSICS		20 // trying to block an incoming physics object

extern Activity ACT_WALK_EASY;
extern Activity ACT_WALK_MARCH;

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_CombineS::Precache()
{
	const char *pModelName = STRING(GetModelName());

	if (!Q_stricmp(pModelName, "models/combine_super_soldier.mdl"))
	{
		m_fIsElite = true;
	}
	else
	{
		m_fIsElite = false;
	}
#ifdef DARKINTERVAL // replace HL2 Combine Soldier model with DI's model
	if (!GetModelName() || !Q_strcmp(STRING(GetModelName()), "models/combine_soldier.mdl"))
	{
		SetModelName(MAKE_STRING("models/_Monsters/Combine/combine_soldier.mdl"));
	}
#else
	if ( !GetModelName() )
	{
		SetModelName(MAKE_STRING("models/combine_soldier.mdl"));
	}
#endif
	PrecacheModel(STRING(GetModelName()));

	UTIL_PrecacheOther("item_healthvial");
	UTIL_PrecacheOther("weapon_frag");
	UTIL_PrecacheOther("item_ammo_ar2_altfire");
#ifdef DARKINTERVAL // OICW-wielding soldiers can drop OICW grenades as reward
	UTIL_PrecacheOther("item_oicw_grenades");
#endif
	BaseClass::Precache();

#ifdef DARKINTERVAL // nameplate system. The system is globalised but soldiers specifically get random names from a file.
	if (m_nameplate_string.Get() == NULL_STRING)
	{
		CUtlBuffer buf;
		bool bFound = filesystem->ReadFile("scripts/npc_gamedata/nameplates_combine.txt", "GAME", buf); // copy the file contents into the buffer
		if (!bFound)
		{
			Warning("Couldn't load nameplates file for  %s!\n", GetDebugName());
			return; // todo: maybe, if there's no file, we can use a hardcoded string array with some default random names?
		}

		char allNames[2048];
		if (buf.Size() > sizeof(allNames)) // watch out for our arbitrary size limit
		{
			Warning("The list of names for %s is too long, should be no more than %i characters total\n", GetDebugName(), sizeof(allNames));
			return;
		}
		sprintf_s(allNames, sizeof(allNames), "%s", static_cast<const char*>(buf.Base())); // copy the buffer into the char;	

		char *nameToken; // currently tokenised line from the file
		char *nameLines[512]; // token that will be taken into the nameplate
		const char *nameDelim = "\n";
		int numLines = 1;
		char *bufScan = allNames;

		while ((nameToken = strtok(bufScan, nameDelim)) != NULL &&
			    numLines < static_cast<int>(ARRAYSIZE(nameLines)))
		{
			bufScan = NULL;
			//	Msg("%i - %s\n", numLines, nameToken);
			nameLines[numLines] = nameToken;
			numLines++;
		}

		int randomRoll = RandomInt(1, numLines - 1); // choose a random line
												 //	Msg("randomly chose line #%i - '%s'\n", randomRoll, nameLines[randomRoll]);

		SetNameplateText(AllocPooledString(nameLines[randomRoll])); // and use the token under that index as the nameplate choice
	}
#endif // DARKINTERVAL
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CombineS::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

	if( IsElite() )
	{
		// Stronger, tougher.
		SetHealth( sk_combine_guard_health.GetFloat() );
		SetMaxHealth( sk_combine_guard_health.GetFloat() );
		SetKickDamage( sk_combine_guard_kick.GetFloat() );
	}
	else
	{
		SetHealth( sk_combine_s_health.GetFloat() );
		SetMaxHealth( sk_combine_s_health.GetFloat() );
		SetKickDamage( sk_combine_s_kick.GetFloat() );
	}

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
//	CapabilitiesAdd( bits_CAP_TURN_HEAD );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );
#ifdef DARKINTERVAL
	// Make Combine soldiers obey weapon rules (burst, rest period)
	CapabilitiesAdd(bits_CAP_USE_SHOT_REGULATOR);
#endif
	BaseClass::Spawn();

	if (m_iUseMarch && !HasSpawnFlags(SF_NPC_START_EFFICIENT))
	{
		DevMsg( 2, "Soldier %s is set to use march anim, but is not an efficient AI. The blended march anim can only be used for dead-ahead walks!\n", GetDebugName() );
	}
}

void CNPC_CombineS::DeathSound( const CTakeDamageInfo &info )
{
	// NOTE: The response system deals with this at the moment
	if ( GetFlags() & FL_DISSOLVING )
		return;

	GetSentences()->Speak( "COMBINE_DIE", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS ); 
}

//-----------------------------------------------------------------------------
// Purpose: Soldiers use CAN_RANGE_ATTACK2 to indicate whether they can throw
//			a grenade. Because they check only every half-second or so, this
//			condition must persist until it is updated again by the code
//			that determines whether a grenade can be thrown, so prevent the 
//			base class from clearing it out. (sjb)
//-----------------------------------------------------------------------------
void CNPC_CombineS::ClearAttackConditions( )
{
	bool fCanRangeAttack2 = HasCondition( COND_CAN_RANGE_ATTACK2 );

	// Call the base class.
	BaseClass::ClearAttackConditions();

	if( fCanRangeAttack2 )
	{
		// We don't allow the base class to clear this condition because we
		// don't sense for it every frame.
		SetCondition( COND_CAN_RANGE_ATTACK2 );
	}
}
#ifdef DARKINTERVAL // moved from base to soldier class
WeaponProficiency_t CNPC_CombineS::CalcWeaponProficiency(CBaseCombatWeapon *pWeapon)
{
	if (FClassnameIs(pWeapon, "weapon_shotgun"))
	{
		return WEAPON_PROFICIENCY_AVERAGE;
	}

	if (FClassnameIs(pWeapon, "weapon_smg1"))
	{
		return WEAPON_PROFICIENCY_GOOD;
	}

	if (FClassnameIs(pWeapon, "weapon_oicw"))
	{
		return WEAPON_PROFICIENCY_AVERAGE;
	}

	if (FClassnameIs(pWeapon, "weapon_sniper"))
	{
		return WEAPON_PROFICIENCY_AVERAGE;
	}
	
	return BaseClass::CalcWeaponProficiency(pWeapon);
}
#endif // DARKINTERVAL
void CNPC_CombineS::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_CombineS::BuildScheduleTestBits( void )
{
	//Interrupt any schedule with physics danger (as long as I'm not moving or already trying to block)
	if ( m_flGroundSpeed == 0.0 && !IsCurSchedule( SCHED_FLINCH_PHYSICS ) )
	{
		SetCustomInterruptCondition( COND_HEAR_PHYSICS_DANGER );
	}

	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_CombineS::SelectSchedule ( void )
{
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_CombineS::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
#ifndef DARKINTERVAL // moved from soldier class to base (and changed there to 1.4 bonus)
	switch ( iHitGroup )
	{
		case HITGROUP_HEAD:
		{
			// Soldiers take double headshot damage
			return 2.0f;
		}
	}
#endif
	return BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );
}
#ifdef DARKINTERVAL
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void CNPC_CombineS::TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	// flinch code, taken from metrocops
	if (m_takedamage == DAMAGE_YES)
	{
		Vector vecLastHitDirection;
		VectorIRotate(vecDir, EntityToWorldTransform(), vecLastHitDirection);

		// Point *at* the shooter
		vecLastHitDirection *= -1.0f;

		QAngle lastHitAngles;
		VectorAngles(vecLastHitDirection, lastHitAngles);
		m_flLastHitYaw = lastHitAngles.y;
	}


	CTakeDamageInfo I = info;
	/*
	bool bE = IsElite();

	if (I.GetAmmoType() == GetAmmoDef()->Index("Pistol") || I.GetAmmoType() == GetAmmoDef()->Index("SMG1"))	
	{
		g_pEffects->Ricochet(ptr->endpos, (vecDir*-0.5f));
		if (bE)
		{
			I.ScaleDamage(0.0f); // these boys just don't care
		}
		else
		{
			if (ptr->hitgroup == HITGROUP_CHEST || ptr->hitgroup == HITGROUP_STOMACH)
				I.ScaleDamage(0.4f); // almost no at all damage to vest-protected areas.
			else if (ptr->hitgroup == HITGROUP_HEAD)
				I.ScaleDamage(0.5f); // reduced dmg to head
			else if (ptr->hitgroup == HITGROUP_GEAR)
				I.ScaleDamage(0.0f);
			else
				I.ScaleDamage(0.5f); // reduced but still passing dmg to limbs.
		}
	}
	if (I.GetAmmoType() == GetAmmoDef()->Index("Buckshot"))
	{
		if (bE)
		{
			if (RandomFloat(0.0f, 1.0f) < 0.3f)
			{
				g_pEffects->Ricochet(ptr->endpos, (vecDir*-0.5f));
				I.ScaleDamage(0.0f); // ~30% chance it'll bounce off of Elite's armour
			}
			else
			{
				if (ptr->hitgroup == HITGROUP_CHEST || ptr->hitgroup == HITGROUP_STOMACH)
					I.ScaleDamage(0.3f); // They still take reduced dmg from pellets, because their armour.
				else if (ptr->hitgroup == HITGROUP_GEAR)
					I.ScaleDamage(0.0f);
				else
					I.ScaleDamage(0.6f);
			}
		}
		else
		{
			if (RandomFloat(0.0f, 1.0f) < 0.1f)
			{
				g_pEffects->Ricochet(ptr->endpos, (vecDir*-0.5f));
				I.ScaleDamage(0.0f); // ~10% chance it'll bounce off of Regular's armour
			}
			else
			{
				if (ptr->hitgroup == HITGROUP_CHEST || ptr->hitgroup == HITGROUP_STOMACH)
					I.ScaleDamage(0.5f); // They still take reduced dmg from pellets, because their armour.
				else if (ptr->hitgroup == HITGROUP_GEAR)
					I.ScaleDamage(0.0f);
				else
					I.ScaleDamage(0.75f);
			}
		}
	}
	*/
	BaseClass::TraceAttack(I, vecDir, ptr);
}

//-----------------------------------------------------------------------------
// Determines the best type of flinch anim to play.
//-----------------------------------------------------------------------------
ConVar combine_soldier_flinch_debug("combine_soldier_flinch_debug", "0", FCVAR_NONE,
	"Shows a coloured marker above the soldier when selecting flinch gesture.\n0 = don't show\ngreen = ACT_FLINCH_PHYSICS\norange = ACT_GESTURE_SMALL_FLINCH\ncyan = ACT_FLINCH_LEFTARM\npurple = ACT_FLINCH_RIGHTARM\nred = ACT_GESTURE_FLINCH_HEAD\nwhite = ACT_GESTURE_FLINCH_STOMACH\nyellow = ACT_GESTURE_SMALL_FLINCH\nblue = something else");
Activity CNPC_CombineS::GetFlinchActivity(bool bHeavyDamage, bool bGesture)
{
	// Version for getting shot from behind
	if ((m_flLastHitYaw > 90) && (m_flLastHitYaw < 270))
	{
		Activity flinchActivity;
		if(combine_soldier_flinch_debug.GetBool())
			NDebugOverlay::Box(EyePosition() + Vector(0, 0, 8), Vector(-3, -3, -3), Vector(3, 3, 3), 0, 255, 0, 200, 0.5f);
		flinchActivity = (Activity)ACT_GESTURE_FLINCH_HEAD;
		if (SelectWeightedSequence(flinchActivity) != ACTIVITY_NOT_AVAILABLE)
			return flinchActivity;
	}

	if (false)
	{
		switch (LastHitGroup())
		{
		case HITGROUP_CHEST:
		{
			Activity flinchActivity = ACT_FLINCH_CHEST;
			if (SelectWeightedSequence(ACT_FLINCH_CHEST) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		case HITGROUP_LEFTARM:
		{
			Activity flinchActivity = ACT_FLINCH_LEFTARM;
			if (SelectWeightedSequence(ACT_FLINCH_LEFTARM) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		case HITGROUP_RIGHTARM:
		{
			Activity flinchActivity = ACT_FLINCH_RIGHTARM;
			if (SelectWeightedSequence(ACT_FLINCH_RIGHTARM) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		case HITGROUP_HEAD:
		{
			Activity flinchActivity = ACT_FLINCH_HEAD;
			if (SelectWeightedSequence(ACT_FLINCH_HEAD) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		case HITGROUP_STOMACH:
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
		{
			Activity flinchActivity = ACT_FLINCH_STOMACH;
			if (SelectWeightedSequence(ACT_FLINCH_STOMACH) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		default:
		{
			Activity flinchActivity = ACT_FLINCH_CHEST;
			if (SelectWeightedSequence(ACT_FLINCH_CHEST) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		}
	}

	else
	{
		switch (LastHitGroup())
		{
		case HITGROUP_CHEST:
		{
			if (combine_soldier_flinch_debug.GetBool())
				NDebugOverlay::Box(EyePosition() + Vector(0, 0, 8), Vector(-3, -3, -3), Vector(3, 3, 3), 255, 255, 0, 200, 0.5f);
			Activity flinchActivity = ACT_GESTURE_SMALL_FLINCH;
			if (SelectWeightedSequence(ACT_GESTURE_SMALL_FLINCH) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		case HITGROUP_LEFTARM:
		{
			if (combine_soldier_flinch_debug.GetBool())
				NDebugOverlay::Box(EyePosition() + Vector(0, 0, 8), Vector(-3, -3, -3), Vector(3, 3, 3), 0, 255, 255, 200, 0.5f);
			Activity flinchActivity = ACT_GESTURE_FLINCH_LEFTARM;
			if (SelectWeightedSequence(ACT_GESTURE_FLINCH_LEFTARM) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		case HITGROUP_RIGHTARM:
		{
			if (combine_soldier_flinch_debug.GetBool())
				NDebugOverlay::Box(EyePosition() + Vector(0, 0, 8), Vector(-3, -3, -3), Vector(3, 3, 3), 255, 0, 255, 200, 0.5f);
			Activity flinchActivity = ACT_GESTURE_FLINCH_RIGHTARM;
			if (SelectWeightedSequence(ACT_GESTURE_FLINCH_RIGHTARM) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		case HITGROUP_HEAD:
		{
			if (combine_soldier_flinch_debug.GetBool())
				NDebugOverlay::Box(EyePosition() + Vector(0, 0, 8), Vector(-3, -3, -3), Vector(3, 3, 3), 255, 0, 0, 200, 0.5f);
			Activity flinchActivity = ACT_GESTURE_FLINCH_HEAD;
			if (SelectWeightedSequence(ACT_GESTURE_FLINCH_HEAD) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		case HITGROUP_STOMACH:
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
		{
			if (combine_soldier_flinch_debug.GetBool())
				NDebugOverlay::Box(EyePosition() + Vector(0, 0, 8), Vector(-3, -3, -3), Vector(3, 3, 3), 255, 255, 255, 200, 0.5f);
			Activity flinchActivity = ACT_GESTURE_FLINCH_STOMACH;
			if (SelectWeightedSequence(ACT_GESTURE_FLINCH_STOMACH) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		default:
		{
			if (combine_soldier_flinch_debug.GetBool())
				NDebugOverlay::Box(EyePosition() + Vector(0, 0, 8), Vector(-3, -3, -3), Vector(3, 3, 3), 255, 255, 0, 200, 0.5f);
			Activity flinchActivity = ACT_GESTURE_SMALL_FLINCH;
			if (SelectWeightedSequence(ACT_GESTURE_SMALL_FLINCH) != ACTIVITY_NOT_AVAILABLE)
				return flinchActivity;
		}
		break;
		}
	}

	return BaseClass::GetFlinchActivity(bHeavyDamage, bGesture);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CombineS::PlayFlinchGesture(void)
{
	BaseClass::PlayFlinchGesture();

	// To ensure old playtested difficulty stays the same, stop cops shooting for a bit after gesture flinches
//	GetShotRegulator()->FireNoEarlierThan(CURTIME + 0.2);
}
#endif // DARKINTERVAL
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CombineS::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	case AE_SOLDIER_BLOCK_PHYSICS:
	{
		DevMsg("BLOCKING!\n");
		m_fIsBlocking = true;
	}
	break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

void CNPC_CombineS::OnChangeActivity( Activity eNewActivity )
{
	// Any new sequence stops us blocking.
	m_fIsBlocking = false;

	BaseClass::OnChangeActivity( eNewActivity );
	
	if (m_iUseMarch)
	{
		SetPoseParameter("casual", 1.0f);
	}
}

void CNPC_CombineS::OnListened()
{
	BaseClass::OnListened();

	if ( HasCondition( COND_HEAR_DANGER ) && HasCondition( COND_HEAR_PHYSICS_DANGER ) )
	{
		if ( HasInterruptCondition( COND_HEAR_DANGER ) )
		{
			ClearCondition( COND_HEAR_PHYSICS_DANGER );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CNPC_CombineS::Event_Killed( const CTakeDamageInfo &info )
{
	// Don't bother if we've been told not to, or the player has a megaphyscannon
	if ( 
#ifndef DARKINTERVAL
		combine_spawn_health.GetBool() == false || 
#endif
		PlayerHasMegaPhysCannon() )
	{
		BaseClass::Event_Killed( info );
		return;
	}

	CBasePlayer *pPlayer = ToBasePlayer( info.GetAttacker() );

	if ( !pPlayer )
	{
		CPropVehicleDriveable *pVehicle = dynamic_cast<CPropVehicleDriveable *>( info.GetAttacker() ) ;
		if ( pVehicle && pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer() )
		{
			pPlayer = assert_cast<CBasePlayer *>( pVehicle->GetDriver() );
		}
	}

	if ( pPlayer != NULL )
	{
		// Elites drop alt-fire ammo, so long as they weren't killed by dissolving.
		if( IsElite() )
		{
			if ( HasSpawnFlags( SF_COMBINE_NO_AR2DROP ) == false )
			{
				CBaseEntity *pItem = DropItem( "item_ammo_ar2_altfire", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );

				if ( pItem )
				{
					IPhysicsObject *pObj = pItem->VPhysicsGetObject();

					if ( pObj )
					{
						Vector			vel		= RandomVector( -64.0f, 64.0f );
						AngularImpulse	angImp	= RandomAngularImpulse( -300.0f, 300.0f );

						vel[2] = 0.0f;
						pObj->AddVelocity( &vel, &angImp );
					}

					if( info.GetDamageType() & DMG_DISSOLVE )
					{
						CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating*>(pItem);

						if( pAnimating )
						{
							pAnimating->Dissolve( NULL, CURTIME, false, ENTITY_DISSOLVE_NORMAL );
						}
					}
					else
					{
						WeaponManager_AddManaged( pItem );
					}
				}
			}
		}
#ifdef DARKINTERVAL // OICW-wielding soldiers can drop OICW grenades as reward
		if (GetActiveWeapon() && GetActiveWeapon()->ClassMatches("weapon_oicw"))
		{
			if (RandomFloat(0, 1) <= 0.3f && HasSpawnFlags(SF_COMBINE_NO_AR2DROP) == false)
			{
				CBaseEntity *pItem = DropItem("item_oicw_grenade", WorldSpaceCenter() + RandomVector(-4, 4), RandomAngle(0, 360));

				if (pItem)
				{
					IPhysicsObject *pObj = pItem->VPhysicsGetObject();

					if (pObj)
					{
						Vector			vel = RandomVector(-64.0f, 64.0f);
						AngularImpulse	angImp = RandomAngularImpulse(-300.0f, 300.0f);

						vel[2] = 0.0f;
						pObj->AddVelocity(&vel, &angImp);
					}

					if (info.GetDamageType() & DMG_DISSOLVE)
					{
						CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating*>(pItem);

						if (pAnimating)
						{
							pAnimating->Dissolve(NULL, CURTIME, false, ENTITY_DISSOLVE_NORMAL);
						}
					}
					else
					{
						WeaponManager_AddManaged(pItem);
					}
				}
			}
		}
#endif // DARKINTERVAL
		CHalfLife2 *pHL2GameRules = static_cast<CHalfLife2 *>(g_pGameRules);

		// Attempt to drop health
		if ( pHL2GameRules->NPC_ShouldDropHealth( pPlayer ) )
		{
			DropItem( "item_healthvial", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			pHL2GameRules->NPC_DroppedHealth();
		}
		
		if ( HasSpawnFlags( SF_COMBINE_NO_GRENADEDROP ) == false )
		{
			// Attempt to drop a grenade
			if ( pHL2GameRules->NPC_ShouldDropGrenade( pPlayer ) )
			{
				DropItem( "weapon_frag", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
				pHL2GameRules->NPC_DroppedGrenade();
			}
		}
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CombineS::IsLightDamage( const CTakeDamageInfo &info )
{
#ifdef DARKINTERVAL
//	if (info.GetAmmoType() == GetAmmoDef()->Index("SMG1"))
//		return false;

	if (info.GetAmmoType() == GetAmmoDef()->Index("Pistol"))
		return false;
#endif
	return BaseClass::IsLightDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CombineS::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// Combine considers AR2 fire to be heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("AR2") )
		return true;
#ifdef DARKINTERVAL
	// Combine considers OICW fire to be heavy damage
	if (info.GetAmmoType() == GetAmmoDef()->Index("OICW"))
		return true;
#endif
	// 357 rounds are heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("357") )
		return true;

	// Shotgun blasts where at least half the pellets hit me are heavy damage
	if ( info.GetDamageType() & DMG_BUCKSHOT )
	{
		int iHalfMax = sk_plr_dmg_buckshot.GetFloat() * sk_plr_num_shotgun_pellets.GetInt() * 0.5;
		if ( info.GetDamage() >= iHalfMax )
			return true;
	}

	// Rollermine shocks
	if( (info.GetDamageType() & DMG_SHOCK) )
	{
		return true;
	}

	return BaseClass::IsHeavyDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: Translate base class activities into combot activites
//-----------------------------------------------------------------------------
Activity CNPC_CombineS::NPC_TranslateActivity( Activity eNewActivity )
{
	// If the special ep2_outland_05 "use march" flag is set, use the more casual marching anim.
	if ( m_iUseMarch && eNewActivity == ACT_WALK )
	{
		eNewActivity = ACT_WALK_MARCH;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
// Save/restore
//-----------------------------------------------------------------------------
BEGIN_DATADESC(CNPC_CombineS)
	DEFINE_KEYFIELD(m_iUseMarch, FIELD_INTEGER, "usemarch"),
#ifdef DARKINTERVAL // flinch code, taken from metrocops
	DEFINE_FIELD(m_flLastPhysicsFlinchTime, FIELD_TIME),
	DEFINE_FIELD(m_flLastDamageFlinchTime, FIELD_TIME),
	DEFINE_FIELD(m_flLastHitYaw, FIELD_FLOAT),
#endif
END_DATADESC()

LINK_ENTITY_TO_CLASS(npc_combine_s, CNPC_CombineS);
#ifdef DARKINTERVAL // reduntant classname for convenience
LINK_ENTITY_TO_CLASS(npc_combine_soldier, CNPC_CombineS);
#endif