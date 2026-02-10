//========================================================================//
//
// Purpose: 
// DI changes that are not ifdef'd: 
// - removed all CS/TF/MP stuff
// $NoKeywords: $
//
//=============================================================================//
/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include "cbase.h"
#include "player.h"
#include "client.h"
#include "soundent.h"
#include "gamerules.h"
#include "game.h"
#include "physics.h"
#include "entitylist.h"
#include "env_shake.h"
#include "globalstate.h"
#include "event_tempentity_tester.h"
#include "ndebugoverlay.h"
#include "engine/ienginesound.h"
#include <ctype.h>
#include "tier1/strtools.h"
#include "te_effect_dispatch.h"
#include "globals.h"
#include "nav_mesh.h"
#ifndef DARKINTERVAL // DI is singleplayer, so it ditched teams
#include "team.h"
#endif
#include "datacache/imdlcache.h"
#include "voice_gamemgr.h"

#ifdef HL2_DLL
#include "weapon_physcannon.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern int giPrecacheGrunt;

// For not just using one big ai net
extern CBaseEntity*	FindPickerEntity( CBasePlayer* pPlayer );

extern bool IsInCommentaryMode( void );
#ifndef DARKINTERVAL // DI doesn't care about sv cheats
ConVar  *sv_cheats = NULL;
#endif
void ClientKill( edict_t *pEdict, const Vector &vecForce, bool bExplode = false )
{
	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( GetContainingEntity( pEdict ) );
	pPlayer->CommitSuicide( vecForce, bExplode );
}

char * CheckChatText( CBasePlayer *pPlayer, char *text )
{
	char *p = text;

	// invalid if NULL or empty
	if ( !text || !text[0] )
		return NULL;

	int length = Q_strlen( text );

	// remove quotes (leading & trailing) if present
	if (*p == '"')
	{
		p++;
		length -=2;
		p[length] = 0;
	}

	// cut off after 127 chars
	if ( length > 127 )
		text[127] = 0;

	GameRules()->CheckChatText( pPlayer, p );

	return p;
}

void ClientPrecache( void )
{
	// Precache cable textures.
	CBaseEntity::PrecacheModel( "cable/cable.vmt" );	
	CBaseEntity::PrecacheModel( "cable/cable_lit.vmt" );	
	CBaseEntity::PrecacheModel( "cable/chain.vmt" );	
	CBaseEntity::PrecacheModel( "cable/rope.vmt" );	
	CBaseEntity::PrecacheModel( "sprites/blueglow1.vmt" );	
	CBaseEntity::PrecacheModel( "sprites/purpleglow1.vmt" );	
	CBaseEntity::PrecacheModel( "sprites/purplelaser1.vmt" );	
#ifdef DARKINTERVAL // for ropes in ch05 hydra
	CBaseEntity::PrecacheModel( "cable/hydra_gut.vmt" );
#endif
	CBaseEntity::PrecacheScriptSound( "Hud.Hint" );
	CBaseEntity::PrecacheScriptSound( "Player.FallDamage" );
	CBaseEntity::PrecacheScriptSound( "Player.Swim" );
#ifdef DARKINTERVAL // unused, maybe one day
	CBaseEntity::PrecacheScriptSound( "Player.ColloidMovement");
#endif
	// General HUD sounds
	CBaseEntity::PrecacheScriptSound( "Player.PickupWeapon" );
	CBaseEntity::PrecacheScriptSound( "Player.DenyWeaponSelection" );
	CBaseEntity::PrecacheScriptSound( "Player.WeaponSelected" );
	CBaseEntity::PrecacheScriptSound( "Player.WeaponSelectionClose" );
	CBaseEntity::PrecacheScriptSound( "Player.WeaponSelectionMoveSlot" );

	// General legacy temp ents sounds
	CBaseEntity::PrecacheScriptSound( "Bounce.Glass" );
	CBaseEntity::PrecacheScriptSound( "Bounce.Metal" );
	CBaseEntity::PrecacheScriptSound( "Bounce.Flesh" );
	CBaseEntity::PrecacheScriptSound( "Bounce.Wood" );
	CBaseEntity::PrecacheScriptSound( "Bounce.Shrapnel" );
	CBaseEntity::PrecacheScriptSound( "Bounce.ShotgunShell" );
	CBaseEntity::PrecacheScriptSound( "Bounce.Shell" );
	CBaseEntity::PrecacheScriptSound( "Bounce.Concrete" );

	ClientGamePrecache();
}
#ifndef DARKINTERVAL // unused
CON_COMMAND_F( cast_ray, "Tests collision detection", FCVAR_NONE )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	
	Vector forward;
	trace_t tr;

	pPlayer->EyeVectors( &forward );
	Vector start = pPlayer->EyePosition();
	UTIL_TraceLine(start, start + forward * MAX_COORD_RANGE, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

	if ( tr.DidHit() )
	{
		DevMsg(1, "Hit %s\nposition %.2f, %.2f, %.2f\nangles %.2f, %.2f, %.2f\n", tr.m_pEnt->GetClassname(),
			tr.m_pEnt->GetAbsOrigin().x, tr.m_pEnt->GetAbsOrigin().y, tr.m_pEnt->GetAbsOrigin().z,
			tr.m_pEnt->GetAbsAngles().x, tr.m_pEnt->GetAbsAngles().y, tr.m_pEnt->GetAbsAngles().z );
		DevMsg(1, "Hit: hitbox %d, hitgroup %d, physics bone %d, solid %d, surface %s, surfaceprop %s, contents %08x\n", tr.hitbox, tr.hitgroup, tr.physicsbone, tr.m_pEnt->GetSolid(), tr.surface.name, physprops->GetPropName( tr.surface.surfaceProps ), tr.contents );
		NDebugOverlay::Line( start, tr.endpos, 0, 255, 0, false, 10 );
		NDebugOverlay::Line( tr.endpos, tr.endpos + tr.plane.normal * 12, 255, 255, 0, false, 10 );
	}
}

CON_COMMAND_F( cast_hull, "Tests hull collision detection", FCVAR_NONE )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	
	Vector forward;
	trace_t tr;

	Vector extents;
	extents.Init(16,16,16);
	pPlayer->EyeVectors( &forward );
	Vector start = pPlayer->EyePosition();
	UTIL_TraceHull(start, start + forward * MAX_COORD_RANGE, -extents, extents, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.DidHit() )
	{
		DevMsg(1, "Hit %s\nposition %.2f, %.2f, %.2f\nangles %.2f, %.2f, %.2f\n", tr.m_pEnt->GetClassname(),
			tr.m_pEnt->GetAbsOrigin().x, tr.m_pEnt->GetAbsOrigin().y, tr.m_pEnt->GetAbsOrigin().z,
			tr.m_pEnt->GetAbsAngles().x, tr.m_pEnt->GetAbsAngles().y, tr.m_pEnt->GetAbsAngles().z );
		DevMsg(1, "Hit: hitbox %d, hitgroup %d, physics bone %d, solid %d, surface %s, surfaceprop %s\n", tr.hitbox, tr.hitgroup, tr.physicsbone, tr.m_pEnt->GetSolid(), tr.surface.name, physprops->GetPropName( tr.surface.surfaceProps ) );
		NDebugOverlay::SweptBox( start, tr.endpos, -extents, extents, vec3_angle, 0, 0, 255, 0, 10 );
		Vector end = tr.endpos;// - tr.plane.normal * DotProductAbs( tr.plane.normal, extents );
		NDebugOverlay::Line( end, end + tr.plane.normal * 24, 255, 255, 64, false, 10 );
	}
}
#endif // DARKINTERVAL
//-----------------------------------------------------------------------------
// Purpose: Used to find targets for ent_* commands
//			Without a name, returns the entity under the player's crosshair.
//			With a name it finds entities via name/classname/index
//-----------------------------------------------------------------------------
CBaseEntity *GetNextCommandEntity( CBasePlayer *pPlayer, const char *name, CBaseEntity *ent )
{
	if ( !pPlayer )
		return NULL;

	// If no name was given set bits based on the picked
	if (FStrEq(name,"")) 
	{
		// If we've already found an entity, return NULL. 
		// Makes it easier to write code using this func.
		if ( ent )
			return NULL;

		return FindPickerEntity( pPlayer );
	}

	int index = atoi( name );
	if ( index )
	{
		// If we've already found an entity, return NULL. 
		// Makes it easier to write code using this func.
		if ( ent )
			return NULL;

		return CBaseEntity::Instance( index );
	}
		
	// Loop through all entities matching, starting from the specified previous
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		if (  (ent->GetEntityName() != NULL_STRING	&& ent->NameMatches(name))	|| 
			  (ent->m_iClassname != NULL_STRING && ent->ClassMatches(name)) )
		{
			return ent;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: called each time a player uses a "cmd" command
// Input  : pPlayer - the player who issued the command
//-----------------------------------------------------------------------------
void SetDebugBits( CBasePlayer* pPlayer, const char *name, int bit )
{
	if ( !pPlayer )
		return;

	CBaseEntity *pEntity = NULL;
	while ( (pEntity = GetNextCommandEntity( pPlayer, name, pEntity )) != NULL )
	{
		if (pEntity->m_debugOverlays & bit)
		{
			pEntity->m_debugOverlays &= ~bit;
		}
		else
		{
			pEntity->m_debugOverlays |= bit;

#ifdef AI_MONITOR_FOR_OSCILLATION
			if( pEntity->IsNPC() )
			{
				pEntity->MyNPCPointer()->m_ScheduleHistory.RemoveAll();
			}
#endif//AI_MONITOR_FOR_OSCILLATION
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pKillTargetName - 
//-----------------------------------------------------------------------------
void KillTargets( const char *pKillTargetName )
{
	CBaseEntity *pentKillTarget = NULL;

	DevMsg( 2, "KillTarget: %s\n", pKillTargetName );
	pentKillTarget = gEntList.FindEntityByName( NULL, pKillTargetName );
	while ( pentKillTarget )
	{
		UTIL_Remove( pentKillTarget );

		DevMsg( 2, "killing %s\n", STRING( pentKillTarget->m_iClassname ) );
		pentKillTarget = gEntList.FindEntityByName( pentKillTarget, pKillTargetName );
	}
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void ConsoleKillTarget( CBasePlayer *pPlayer, const char *name )
{
	// If no name was given use the picker
	if (FStrEq(name,"")) 
	{
		CBaseEntity *pEntity = FindPickerEntity( pPlayer );
		if ( pEntity )
		{
			UTIL_Remove( pEntity );
			Msg( "killing %s\n", pEntity->GetDebugName() );
			return;
		}
	}
	// Otherwise use name or classname
	KillTargets( name );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointClientCommand : public CPointEntity
{
public:
	DECLARE_CLASS( CPointClientCommand, CPointEntity );
	DECLARE_DATADESC();

	void InputCommand( inputdata_t& inputdata );
};

void CPointClientCommand::InputCommand( inputdata_t& inputdata )
{
	if ( !inputdata.value.String()[0] )
		return;

	edict_t *pClient = NULL;
	if ( gpGlobals->maxClients == 1 )
	{
		pClient = engine->PEntityOfEntIndex( 1 );
	}
	else
	{
		// In multiplayer, send it back to the activator
		CBasePlayer *player = dynamic_cast< CBasePlayer * >( inputdata.pActivator );
		if ( player )
		{
			pClient = player->edict();
		}

		if ( IsInCommentaryMode() && !pClient )
		{
			// Commentary is stuffing a command in. We'll pretend it came from the first player.
			pClient = engine->PEntityOfEntIndex( 1 );
		}
	}

	if ( !pClient || !pClient->GetUnknown() )
		return;

	engine->ClientCommand( pClient, "%s\n", inputdata.value.String() );
}

BEGIN_DATADESC( CPointClientCommand )
	DEFINE_INPUTFUNC( FIELD_STRING, "Command", InputCommand ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( point_clientcommand, CPointClientCommand );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPointServerCommand : public CPointEntity
{
public:
	DECLARE_CLASS( CPointServerCommand, CPointEntity );
	DECLARE_DATADESC();
	void InputCommand( inputdata_t& inputdata );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : inputdata - 
//-----------------------------------------------------------------------------
void CPointServerCommand::InputCommand( inputdata_t& inputdata )
{
	if ( !inputdata.value.String()[0] )
		return;

	engine->ServerCommand( UTIL_VarArgs( "%s\n", inputdata.value.String() ) );
}

BEGIN_DATADESC( CPointServerCommand )
	DEFINE_INPUTFUNC( FIELD_STRING, "Command", InputCommand ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( point_servercommand, CPointServerCommand );

//------------------------------------------------------------------------------
// Purpose : Draw a line betwen two points.  White if no world collisions, red if collisions
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CC_DrawLine( const CCommand &args )
{
	Vector startPos;
	Vector endPos;

	startPos.x = atof(args[1]);
	startPos.y = atof(args[2]);
	startPos.z = atof(args[3]);
	endPos.x = atof(args[4]);
	endPos.y = atof(args[5]);
	endPos.z = atof(args[6]);

	UTIL_AddDebugLine(startPos,endPos,true,true);
}
static ConCommand drawline("drawline", CC_DrawLine, "Draws line between two 3D Points.\n\tGreen if no collision\n\tRed is collides with something\n\tArguments: x1 y1 z1 x2 y2 z2", FCVAR_NONE);

//------------------------------------------------------------------------------
// Purpose : Draw a cross at a points.  
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CC_DrawCross( const CCommand &args )
{
	Vector vPosition;

	vPosition.x = atof(args[1]);
	vPosition.y = atof(args[2]);
	vPosition.z = atof(args[3]);

	// Offset since min and max z in not about center
	Vector mins = Vector(-5,-5,-5);
	Vector maxs = Vector(5,5,5);

	Vector start = mins + vPosition;
	Vector end   = maxs + vPosition;
	UTIL_AddDebugLine(start,end,true,true);

	start.x += (maxs.x - mins.x);
	end.x	-= (maxs.x - mins.x);
	UTIL_AddDebugLine(start,end,true,true);

	start.y += (maxs.y - mins.y);
	end.y	-= (maxs.y - mins.y);
	UTIL_AddDebugLine(start,end,true,true);

	start.x -= (maxs.x - mins.x);
	end.x	+= (maxs.x - mins.x);
	UTIL_AddDebugLine(start,end,true,true);
}
static ConCommand drawcross("drawcross", CC_DrawCross, "Draws a cross at the given location\n\tArguments: x y z", FCVAR_NONE);


//------------------------------------------------------------------------------
// helper function for kill and explode
//------------------------------------------------------------------------------
void kill_helper( const CCommand &args, bool bExplode )
{
	if ( args.ArgC() > 1 /*&& sv_cheats->GetBool()*/ )
	{
		// Find the matching netname
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = ToBasePlayer( UTIL_PlayerByIndex(i) );
			if ( pPlayer )
			{
				if ( Q_strstr( pPlayer->GetPlayerName(), args[1] ) )
				{
					pPlayer->CommitSuicide( bExplode );
				}
			}
		}
	}
	else
	{
		CBasePlayer *pPlayer = UTIL_GetCommandClient();
		if ( pPlayer )
		{
			pPlayer->CommitSuicide( bExplode );
		}
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CON_COMMAND( kill, "Kills the player with generic damage" )
{
	kill_helper( args, false );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CON_COMMAND( explode, "Kills the player with explosive damage" )
{
	kill_helper( args, true );
}

//------------------------------------------------------------------------------
// helper function for killvector and explodevector
//------------------------------------------------------------------------------
void killvector_helper( const CCommand &args, bool bExplode )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( pPlayer && args.ArgC() == 5 )
	{
		// Find the matching netname.
		for ( int iClient = 1; iClient <= gpGlobals->maxClients; iClient++ )
		{
			CBasePlayer *pPlayer = ToBasePlayer( UTIL_PlayerByIndex( iClient ) );
			if ( pPlayer )
			{
				if ( Q_strstr( pPlayer->GetPlayerName(), args[1] ) )
				{
					// Build world-space force vector.
					Vector vecForce;
					vecForce.x = atof( args[2] );
					vecForce.y = atof( args[3] );
					vecForce.z = atof( args[4] );

					ClientKill( pPlayer->edict(), vecForce, bExplode );
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CON_COMMAND_F( killvector, "Kills a player applying force. Usage: killvector <player> <x value> <y value> <z value>", FCVAR_NONE )
{
	killvector_helper( args, false );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CON_COMMAND_F( explodevector, "Kills a player applying an explosive force. Usage: explodevector <player> <x value> <y value> <z value>", FCVAR_NONE )
{
	killvector_helper( args, false );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CON_COMMAND_F( buddha, "Toggle.  Player takes damage but won't die. (Shows red cross when health is zero)", FCVAR_NOTIFY)
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( pPlayer )
	{
		if (pPlayer->m_debugOverlays & OVERLAY_BUDDHA_MODE)
		{
			pPlayer->m_debugOverlays &= ~OVERLAY_BUDDHA_MODE;
			ConColorMsg(Color(255, 100, 255, 255), "Buddha OFF\n");
		//	Msg("Buddha Mode off...\n");
		}
		else
		{
			pPlayer->m_debugOverlays |= OVERLAY_BUDDHA_MODE;
			ConColorMsg(Color(255, 100, 255, 255), "Buddha ON\n");
		//	Msg("Buddha Mode on...\n");
		}
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CON_COMMAND( give, "Give item to player.\n\tArguments: <item_name>" )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( pPlayer 
#ifndef DARKINTERVAL
		&& (gpGlobals->maxClients == 1 || sv_cheats->GetBool()) 
#endif
		&& args.ArgC() >= 2 )
	{
		char item_to_give[ 256 ];
		Q_strncpy( item_to_give, args[1], sizeof( item_to_give ) );
		Q_strlower( item_to_give );

		// Dirty hack to avoid suit playing it's pickup sound
		if ( !Q_stricmp( item_to_give, "item_suit" ) )
		{
			pPlayer->EquipSuit( false );
			return;
		}

#ifdef DARKINTERVAL
		// Bring back the classic "give all"
		if (!Q_stricmp(item_to_give, "all"))
		{
			pPlayer->EquipSuit();

			// Give the player everything!
			pPlayer->GiveAmmo(255, "Pistol");
			pPlayer->GiveAmmo(255, "AR2");
			pPlayer->GiveAmmo(5, "AR2AltFire");
			pPlayer->GiveAmmo(255, "SMG1");
			pPlayer->GiveAmmo(255, "OICW");
			pPlayer->GiveAmmo(255, "Buckshot");
			pPlayer->GiveAmmo(3, "smg1_grenade");
			pPlayer->GiveAmmo(3, "rpg_round");
			pPlayer->GiveAmmo(5, "grenade");
			pPlayer->GiveAmmo(32, "357");
			pPlayer->GiveAmmo(16, "XBowBolt");
			pPlayer->GiveAmmo(15, "flaregun");
			pPlayer->GiveAmmo(150, "TauRound");
			pPlayer->GiveNamedItem("weapon_smg1");
			pPlayer->GiveNamedItem("weapon_frag");
			pPlayer->GiveNamedItem("weapon_crowbar");
			pPlayer->GiveNamedItem("weapon_pistol");
			pPlayer->GiveNamedItem("weapon_ar2");
			pPlayer->GiveNamedItem("weapon_shotgun");
			pPlayer->GiveNamedItem("weapon_physcannon");
			pPlayer->GiveNamedItem("weapon_bugbait");
			pPlayer->GiveNamedItem("weapon_rpg");
			pPlayer->GiveNamedItem("weapon_357");
			pPlayer->GiveNamedItem("weapon_crossbow");
			pPlayer->GiveNamedItem("weapon_flaregun");
			pPlayer->GiveNamedItem("weapon_immolator");
			pPlayer->GiveNamedItem("weapon_tau");
			pPlayer->GiveNamedItem("weapon_oicw");
			if (pPlayer->GetHealth() < 100)
			{
				pPlayer->TakeHealth(25, DMG_GENERIC);
			}
			if (pPlayer->ArmorValue() < 100)
			{
				pPlayer->GiveNamedItem("item_battery");
			}
			return;
		}
#endif // DARKINTERVAL
		string_t iszItem = AllocPooledString( item_to_give );	// Make a copy of the classname
		pPlayer->GiveNamedItem( STRING(iszItem) );
	}
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CON_COMMAND( fov, "Change players FOV" )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	if ( pPlayer 
#ifndef DARKINTERVAL
		&& sv_cheats->GetBool()
#endif
		)
	{
		if ( args.ArgC() > 1 )
		{
			int nFOV = atoi( args[1] );
			pPlayer->SetDefaultFOV( nFOV );
		}
		else
		{
			ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "\"fov\" is \"%d\"\n", pPlayer->GetFOV() ) );
		}
	}
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#ifndef DARKINTERVAL // unstable, but maybe todo - revisit in the future
void CC_Player_SetModel( const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	if ( pPlayer && args.ArgC() == 2)
	{
		static char szName[256];
		Q_snprintf( szName, sizeof( szName ), "models/%s.mdl", args[1] );
		pPlayer->SetModel( szName );
		UTIL_SetSize(pPlayer, VEC_HULL_MIN, VEC_HULL_MAX);
	}
}
static ConCommand setmodel("setmodel", CC_Player_SetModel, "Changes's player's model", FCVAR_NONE );
#endif
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CC_Player_TestDispatchEffect( const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	if ( !pPlayer)
		return;
	
	if ( args.ArgC() < 2 )
	{
		Msg(" Usage: test_dispatcheffect <effect name> <distance away> <flags> <magnitude> <scale>\n " );
		Msg("		 defaults are: <distance 1024> <flags 0> <magnitude 0> <scale 0>\n" );
		return;
	}

	// Optional distance
	float flDistance = 1024;
	if ( args.ArgC() >= 3 )
	{
		flDistance = atoi( args[ 2 ] );
	}

	// Optional flags
	float flags = 0;
	if ( args.ArgC() >= 4 )
	{
		flags = atoi( args[ 3 ] );
	}

	// Optional magnitude
	float magnitude = 0;
	if ( args.ArgC() >= 5 )
	{
		magnitude = atof( args[ 4 ] );
	}

	// Optional scale
	float scale = 0;
	if ( args.ArgC() >= 6 )
	{
		scale = atof( args[ 5 ] );
	}

	Vector vecForward;
	QAngle vecAngles = pPlayer->EyeAngles();
	AngleVectors( vecAngles, &vecForward );

	// Trace forward
	trace_t tr;
	Vector vecSrc = pPlayer->EyePosition();
	Vector vecEnd = vecSrc + (vecForward * flDistance);
	UTIL_TraceLine( vecSrc, vecEnd, MASK_ALL, pPlayer, COLLISION_GROUP_NONE, &tr );

	// Fill out the generic data
	CEffectData data;
	// If we hit something, use that data
	if ( tr.fraction < 1.0 )
	{
		data.m_vOrigin = tr.endpos;
		VectorAngles( tr.plane.normal, data.m_vAngles );
		data.m_vNormal = tr.plane.normal;
	}
	else
	{
		data.m_vOrigin = vecEnd;
		data.m_vAngles = vecAngles;
		AngleVectors( vecAngles, &data.m_vNormal );
	}
	data.m_nEntIndex = pPlayer->entindex();
	data.m_fFlags = flags;
	data.m_flMagnitude = magnitude;
	data.m_flScale = scale;
	DispatchEffect( (char *)args[1], data );
}

static ConCommand test_dispatcheffect("test_dispatcheffect", CC_Player_TestDispatchEffect, "Test a clientside dispatch effect.\n\tUsage: test_dispatcheffect <effect name> <distance away> <flags> <magnitude> <scale>\n\tDefaults are: <distance 1024> <flags 0> <magnitude 0> <scale 0>\n", FCVAR_NONE);

#ifdef HL2_DLL
//-----------------------------------------------------------------------------
// Purpose: Quickly switch to the physics cannon, or back to previous item
//-----------------------------------------------------------------------------
void CC_Player_PhysSwap( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	
	if ( pPlayer )
	{
		CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();

		if ( pWeapon )
		{
			// Tell the client to stop selecting weapons
			engine->ClientCommand( UTIL_GetCommandClient()->edict(), "cancelselect" );

			const char *strWeaponName = pWeapon->GetName();

			if ( !Q_stricmp( strWeaponName, "weapon_physcannon" ) )
			{
				PhysCannonForceDrop( pWeapon, NULL );
				pPlayer->SelectLastItem();
			}
			else
			{
				pPlayer->SelectItem( "weapon_physcannon" );
			}
		}
	}
}
static ConCommand physswap("phys_swap", CC_Player_PhysSwap, "Automatically swaps the current weapon for the physcannon and back again." );
#endif

//-----------------------------------------------------------------------------
// Purpose: Quickly switch to the bug bait, or back to previous item
//-----------------------------------------------------------------------------
#ifndef DARKINTERVAL // DI doesn't have bugbait and so the command is removed to reduce amount of concommands
void CC_Player_BugBaitSwap( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
	
	if ( pPlayer )
	{
		CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();

		if ( pWeapon )
		{
			// Tell the client to stop selecting weapons
			engine->ClientCommand( UTIL_GetCommandClient()->edict(), "cancelselect" );

			const char *strWeaponName = pWeapon->GetName();

			if ( !Q_stricmp( strWeaponName, "weapon_bugbait" ) )
			{
				pPlayer->SelectLastItem();
			}
			else
			{
				pPlayer->SelectItem( "weapon_bugbait" );
			}
		}
	}
}
static ConCommand bugswap("bug_swap", CC_Player_BugBaitSwap, "Automatically swaps the current weapon for the bug bait and back again.", FCVAR_NONE );
#endif // DARKINTERVAL
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CC_Player_Use( const CCommand &args )
{
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( pPlayer)
	{
		pPlayer->SelectItem((char *)args[1]);
	}
}
static ConCommand use("use", CC_Player_Use, "Use a particular weapon\t\nArguments: <weapon_name>");


//------------------------------------------------------------------------------
// A small wrapper around SV_Move that never clips against the supplied entity.
//------------------------------------------------------------------------------
static bool TestEntityPosition ( CBasePlayer *pPlayer )
{	
	trace_t	trace;
	UTIL_TraceEntity( pPlayer, pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin(), MASK_PLAYERSOLID, &trace );
	return (trace.startsolid == 0);
}


//------------------------------------------------------------------------------
// Searches along the direction ray in steps of "step" to see if 
// the entity position is passible.
// Used for putting the player in valid space when toggling off noclip mode.
//------------------------------------------------------------------------------
static int FindPassableSpace( CBasePlayer *pPlayer, const Vector& direction, float step, Vector& oldorigin )
{
	int i;
	for ( i = 0; i < 100; i++ )
	{
		Vector origin = pPlayer->GetAbsOrigin();
		VectorMA( origin, step, direction, origin );
		pPlayer->SetAbsOrigin( origin );
		if ( TestEntityPosition( pPlayer ) )
		{
			VectorCopy( pPlayer->GetAbsOrigin(), oldorigin );
			return 1;
		}
	}
	return 0;
}


//------------------------------------------------------------------------------
// Noclip
//------------------------------------------------------------------------------
void EnableNoClip( CBasePlayer *pPlayer )
{
	// Disengage from hierarchy
	pPlayer->SetParent( NULL );
	pPlayer->SetMoveType( MOVETYPE_NOCLIP );
//	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "noclip ON\n");
	ConColorMsg(Color(255, 100, 255, 255), "Noclip ON\n");
	pPlayer->AddEFlags( EFL_NOCLIP_ACTIVE );
}

void CC_Player_NoClip( void )
{
#ifndef DARKINTERVAL
	if ( !sv_cheats->GetBool() )
		return;
#endif
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	CPlayerState *pl = pPlayer->PlayerData();
	Assert( pl );

	if (pPlayer->GetMoveType() != MOVETYPE_NOCLIP)
	{
		EnableNoClip( pPlayer );
		return;
	}

	pPlayer->RemoveEFlags( EFL_NOCLIP_ACTIVE );
	pPlayer->SetMoveType( MOVETYPE_WALK );

	Vector oldorigin = pPlayer->GetAbsOrigin();
	ConColorMsg(Color(255, 100, 255, 255), "Noclip OFF\n");
//	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "noclip OFF\n");
	if ( !TestEntityPosition( pPlayer ) )
	{
		Vector forward, right, up;

		AngleVectors ( pl->v_angle, &forward, &right, &up);
		
		// Try to move into the world
		if ( !FindPassableSpace( pPlayer, forward, 1, oldorigin ) )
		{
			if ( !FindPassableSpace( pPlayer, right, 1, oldorigin ) )
			{
				if ( !FindPassableSpace( pPlayer, right, -1, oldorigin ) )		// left
				{
					if ( !FindPassableSpace( pPlayer, up, 1, oldorigin ) )	// up
					{
						if ( !FindPassableSpace( pPlayer, up, -1, oldorigin ) )	// down
						{
							if ( !FindPassableSpace( pPlayer, forward, -1, oldorigin ) )	// back
							{
								Msg( "Can't find the world\n" );
							}
						}
					}
				}
			}
		}

		pPlayer->SetAbsOrigin( oldorigin );
	}
}

static ConCommand noclip("noclip", CC_Player_NoClip, "Toggle. Player becomes non-solid and flies.", FCVAR_NOTIFY);


//------------------------------------------------------------------------------
// Sets client to godmode
//------------------------------------------------------------------------------
void CC_God_f (void)
{
#ifndef DARKINTERVAL
	if ( !sv_cheats->GetBool() )
		return;
#endif
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;
	
	pPlayer->ToggleFlag( FL_GODMODE );
	if (!(pPlayer->GetFlags() & FL_GODMODE ) )
		ConColorMsg(Color(255, 100, 255, 255), "Godmode OFF\n");
	//	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "godmode OFF\n");
	else
		ConColorMsg(Color(255, 100, 255, 255), "Godmode ON\n");
	//	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "godmode ON\n");
}

static ConCommand god("god", CC_God_f, "Toggle. Player becomes invulnerable.", FCVAR_NOTIFY);


//------------------------------------------------------------------------------
// Sets client to godmode
//------------------------------------------------------------------------------
CON_COMMAND_F( setpos, "Move player to specified origin.", FCVAR_NONE)
{
#ifndef DARKINTERVAL
	if ( !sv_cheats->GetBool() )
		return;
#endif
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( args.ArgC() < 3 )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Usage:  setpos x y <z optional>\n");
		return;
	}

	Vector oldorigin = pPlayer->GetAbsOrigin();

	Vector newpos;
	newpos.x = atof( args[1] );
	newpos.y = atof( args[2] );
	newpos.z = args.ArgC() == 4 ? atof( args[3] ) : oldorigin.z;

	pPlayer->SetAbsOrigin( newpos );

	if ( !TestEntityPosition( pPlayer ) )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "setpos into world, use noclip to unstick yourself!\n");
	}
}


//------------------------------------------------------------------------------
// Sets client to godmode
//------------------------------------------------------------------------------
void CC_setang_f (const CCommand &args)
{
#ifndef DARKINTERVAL
	if ( !sv_cheats->GetBool() )
		return;
#endif
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( args.ArgC() < 3 )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Usage:  setang pitch yaw <roll optional>\n");
		return;
	}

	QAngle oldang = pPlayer->GetAbsAngles();

	QAngle newang;
	newang.x = atof( args[1] );
	newang.y = atof( args[2] );
	newang.z = args.ArgC() == 4 ? atof( args[3] ) : oldang.z;

	pPlayer->SnapEyeAngles( newang );
}

static ConCommand setang("setang", CC_setang_f, "Snap player eyes to specified pitch yaw <roll:optional>.", FCVAR_NONE);

static float GetHexFloat( const char *pStr )
{
	if ( ( pStr[0] == '0' ) && ( pStr[1] == 'x' ) )
	{
		uint32 f = (uint32)V_atoi64( pStr );
		return *reinterpret_cast< const float * >( &f );
	}
	
	return atof( pStr );
}

//------------------------------------------------------------------------------
// Move position
//------------------------------------------------------------------------------
CON_COMMAND_F( setpos_exact, "Move player to an exact specified origin.", FCVAR_NONE)
{
#ifndef DARKINTERVAL
	if ( !sv_cheats->GetBool() )
		return;
#endif
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( args.ArgC() < 3 )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Usage:  setpos_exact x y <z optional>\n");
		return;
	}

	Vector oldorigin = pPlayer->GetAbsOrigin();

	Vector newpos;
	newpos.x = GetHexFloat( args[1] );
	newpos.y = GetHexFloat( args[2] );
	newpos.z = args.ArgC() == 4 ? GetHexFloat( args[3] ) : oldorigin.z;

	pPlayer->Teleport( &newpos, NULL, NULL );

	if ( !TestEntityPosition( pPlayer ) )
	{
		if ( pPlayer->GetMoveType() != MOVETYPE_NOCLIP )
		{
			EnableNoClip( pPlayer );
			return;
		}
	}
}

CON_COMMAND_F( setang_exact, "Snap player eyes and orientation to specified pitch yaw <roll:optional>.", FCVAR_NONE)
{
#ifndef DARKINTERVAL
	if ( !sv_cheats->GetBool() )
		return;
#endif
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	if ( args.ArgC() < 3 )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Usage:  setang_exact pitch yaw <roll optional>\n");
		return;
	}

	QAngle oldang = pPlayer->GetAbsAngles();

	QAngle newang;
	newang.x = GetHexFloat( args[1] );
	newang.y = GetHexFloat( args[2] );
	newang.z = args.ArgC() == 4 ? GetHexFloat( args[3] ) : oldang.z;

	pPlayer->Teleport( NULL, &newang, NULL );
	pPlayer->SnapEyeAngles( newang );
}


//------------------------------------------------------------------------------
// Sets client to notarget mode.
//------------------------------------------------------------------------------
void CC_Notarget_f (void)
{
#ifndef DARKINTERVAL
	if ( !sv_cheats->GetBool() )
		return;
#endif
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;
	
	pPlayer->ToggleFlag( FL_NOTARGET );
	if ( !(pPlayer->GetFlags() & FL_NOTARGET ) )
		ConColorMsg(Color(255, 100, 255, 255), "Notarget OFF\n");
	//	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "notarget OFF\n");
	else

		ConColorMsg(Color(255, 100, 255, 255), "Notarget ON\n");
	//	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "notarget ON\n");
}

ConCommand notarget("notarget", CC_Notarget_f, "Toggle. Player becomes hidden to NPCs.", FCVAR_NOTIFY);

//------------------------------------------------------------------------------
// Damage the client the specified amount
//------------------------------------------------------------------------------
void CC_HurtMe_f(const CCommand &args)
{
#ifndef DARKINTERVAL
	if ( !sv_cheats->GetBool() )
		return;
#endif
	CBasePlayer *pPlayer = ToBasePlayer( UTIL_GetCommandClient() ); 
	if ( !pPlayer )
		return;

	int iDamage = 10;
	if ( args.ArgC() >= 2 )
	{
		iDamage = atoi( args[ 1 ] );
	}

	pPlayer->TakeDamage( CTakeDamageInfo( pPlayer, pPlayer, (float)iDamage, DMG_PREVENT_PHYSICS_FORCE ) );
}

static ConCommand hurtme("hurtme", CC_HurtMe_f, "Hurts the player.\n\tArguments: <health to lose>", FCVAR_NONE);

static bool IsInGroundList( CBaseEntity *ent, CBaseEntity *ground )
{
	if ( !ground || !ent )
		return false;

	groundlink_t *root = ( groundlink_t * )ground->GetDataObject( GROUNDLINK );
	if ( root )
	{
		groundlink_t *link = root->nextLink;
		while ( link != root )
		{
			CBaseEntity *other = link->entity;
			if ( other == ent )
				return true;
			link = link->nextLink;
		}
	}

	return false;

}

static int DescribeGroundList( CBaseEntity *ent )
{
	if ( !ent )
		return 0;

	int c = 1;

	Msg( "%i : %s (ground %i %s)\n", ent->entindex(), ent->GetClassname(), 
		ent->GetGroundEntity() ? ent->GetGroundEntity()->entindex() : -1,
		ent->GetGroundEntity() ? ent->GetGroundEntity()->GetClassname() : "NULL" );
	groundlink_t *root = ( groundlink_t * )ent->GetDataObject( GROUNDLINK );
	if ( root )
	{
		groundlink_t *link = root->nextLink;
		while ( link != root )
		{
			CBaseEntity *other = link->entity;
			if ( other )
			{
				Msg( "  %02i:  %i %s\n", c++, other->entindex(), other->GetClassname() );

				if ( other->GetGroundEntity() != ent )
				{
					Assert( 0 );
					Msg( "   mismatched!!!\n" );
				}
			}
			else
			{
				Assert( 0 );
				Msg( "  %02i:  NULL link\n", c++ );
			}
			link = link->nextLink;
		}
	}

	if ( ent->GetGroundEntity() != NULL )
	{
		Assert( IsInGroundList( ent, ent->GetGroundEntity() ) );
	}

	return c - 1;
}

void CC_GroundList_f(const CCommand &args)
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() == 2 )
	{
		int idx = atoi( args[1] );

		CBaseEntity *ground = CBaseEntity::Instance( idx );
		if ( ground )
		{
			DescribeGroundList( ground );
		}
	}
	else
	{
		CBaseEntity *ent = NULL;
		int linkCount = 0;
		while ( (ent = gEntList.NextEnt(ent)) != NULL )
		{
			linkCount += DescribeGroundList( ent );
		}

		extern int groundlinksallocated;
		Assert( linkCount == groundlinksallocated );

		Msg( "--- %i links\n", groundlinksallocated );
	}
}

static ConCommand groundlist("groundlist", CC_GroundList_f, "Display ground entity list <index>" );

//-----------------------------------------------------------------------------
// Purpose: called each time a player uses a "cmd" command
// Input  : *pEdict - the player who issued the command
//-----------------------------------------------------------------------------
void ClientCommand( CBasePlayer *pPlayer, const CCommand &args )
{
	const char *pCmd = args[0];

	// Is the client spawned yet?
	if ( !pPlayer )
		return;

	MDLCACHE_CRITICAL_SECTION();

	/*
	const char *pstr;

	if (((pstr = strstr(pcmd, "weapon_")) != NULL)  && (pstr == pcmd))
	{
		// Subtype may be specified
		if ( args.ArgC() == 2 )
		{
			pPlayer->SelectItem( pcmd, atoi( args[1] ) );
		}
		else
		{
			pPlayer->SelectItem(pcmd);
		}
	}
	*/
	
	if ( FStrEq( pCmd, "killtarget" ) )
	{
#ifndef DARKINTERVAL
		if ( g_pDeveloper->GetBool() && sv_cheats->GetBool() && UTIL_IsCommandIssuedByServerAdmin() )
#endif
		{
			ConsoleKillTarget( pPlayer, args[1] );
		}
	}
	else if ( FStrEq( pCmd, "demorestart" ) ) 
	{
		pPlayer->ForceClientDllUpdate(); 
	}
	else if ( FStrEq( pCmd, "fade" ) )
	{
		color32 black = {32,63,100,200};
		UTIL_ScreenFade( pPlayer, black, 3, 3, FFADE_OUT  );
	} 
	else if ( FStrEq( pCmd, "te" ) )
	{
#ifndef DARKINTERVAL
		if ( sv_cheats->GetBool() && UTIL_IsCommandIssuedByServerAdmin() )
#endif
		{
			if ( FStrEq( args[1], "stop" ) )
			{
				// Destroy it
				//
				CBaseEntity *ent = gEntList.FindEntityByClassname( NULL, "te_tester" );
				while ( ent )
				{
					CBaseEntity *next = gEntList.FindEntityByClassname( ent, "te_tester" );
					UTIL_Remove( ent );
					ent = next;
				}
			}
			else
			{
				CTempEntTester::Create( pPlayer->WorldSpaceCenter(), pPlayer->EyeAngles(), args[1], args[2] );
			}
		}
	}
	else 
	{
		if ( !g_pGameRules->ClientCommand( pPlayer, args ) )
		{
			if ( Q_strlen( pCmd ) > 128 )
			{
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Console command too long.\n" );
			}
			else
			{
				// tell the user they entered an unknown command
				ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "Unknown command: %s\n", pCmd ) );
			}
		}
	}
}

#ifdef DARKINTERVAL
char *build_date = __DATE__ ;
char *build_time = __TIME__;
char *mon[12] = 
{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
char mond[12] = 
{ 31,    28,    31,    30,    31,    30,    31,    31,    30,    31,    30,    31 };

class CDIBuildNumber
{
public:
	CDIBuildNumber( void )
	{
		ComputeBuildNumber();
	}
	
	// returns days since Dec 13 2024
	int	GetBuildNumber( void ) 
	{
		return m_nBuildNumber;
	}

private:
	void ComputeBuildNumber( void )
	{
		int m = 0; 
		int d = 0;
		int y = 0;

		for (m = 0; m < 11; m++)
		{
			if (Q_strncasecmp( &build_date[0], mon[m], 3 ) == 0)
				break;
			d += mond[m];
		}

		d += atoi( &build_date[4] ) - 1;

		y = atoi( &build_date[7] ) - 1900;

		m_nBuildNumber = d + (int)((y - 1) * 365.25);

		if (((y % 4) == 0) && m > 1)
		{
			m_nBuildNumber += 1;
		}

		m_nBuildNumber -= 45272;  // Dec 13 2024 (Part 2 release)
	}

	int m_nBuildNumber;
};

// Singleton
static CDIBuildNumber g_DI_BuildNumber;

int di_build_number( void )
{
	return g_DI_BuildNumber.GetBuildNumber();
}
//------------------------------------------------------------------------------
// Purpose : A command that reports this Dark Interval build's version.
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CC_DarkIntervalVersion( const CCommand &args )
{
	float version = 2.1; // keep this updated between releases

	ConMsg("Playing Dark Interval version %.1f\n", version);
	ConMsg("Build timestamp: %s %s.\n", build_time, build_date);
	ConMsg("Build number: %i\n", di_build_number());
}
static ConCommand darkinterval_version("darkinterval_version", CC_DarkIntervalVersion, "Reports this Dark Interval build's version\n", FCVAR_NONE);

#endif
