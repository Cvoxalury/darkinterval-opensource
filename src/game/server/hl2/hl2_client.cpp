//========================================================================//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== hl2_client.cpp ========================================================

  HL2 client/server game specific stuff

*/

#include "cbase.h"
#include "engine/ienginesound.h"
#include "entitylist.h"
#include "game.h"
#include "gamerules.h"
#include "hl2_gamerules.h"
#include "hl2_player.h"
#include "physics.h"
#include "player_resource.h"
#ifndef DARKINTERVAL // DI is singleplayer, so it ditched teams
#include "teamplay_gamerules.h"
#endif
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void Host_Say( edict_t *pEdict, bool teamonly );

extern CBaseEntity*	FindPickerEntityClass( CBasePlayer *pPlayer, char *classname );
extern bool			g_fGameOver;

/*
===========
ClientPutInServer

called each time a player is spawned into the game
============
*/
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBasePlayer for pev, and call spawn
	CHL2_Player *pPlayer = CHL2_Player::CreatePlayer( "player", pEdict );
	pPlayer->SetPlayerName( playername );
}

void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	CHL2_Player *pPlayer = dynamic_cast< CHL2_Player* >( CBaseEntity::Instance( pEdict ) );
	Assert( pPlayer );

	if ( !pPlayer )
	{
		return;
	}

	pPlayer->InitialSpawn();

	if ( !bLoadGame )
	{
		pPlayer->Spawn();
	}
}

/*
===============
const char *GetGameDescription()

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Half-Life 2";
}

//-----------------------------------------------------------------------------
// Purpose: Given a player and optional name returns the entity of that 
//			classname that the player is nearest facing
//			
// Input  :
// Output :
//-----------------------------------------------------------------------------
CBaseEntity* FindEntity( edict_t *pEdict, char *classname)
{
	// If no name was given set bits based on the picked
	if (FStrEq(classname,"")) 
	{
		return (FindPickerEntityClass( static_cast<CBasePlayer*>(GetContainingEntity(pEdict)), classname ));
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Precache game-specific models & sounds
//-----------------------------------------------------------------------------
void ClientGamePrecache( void )
{
	CBaseEntity::PrecacheModel("models/player.mdl");
	CBaseEntity::PrecacheModel( "models/gibs/agibs.mdl" );
	CBaseEntity::PrecacheModel ("models/weapons/v_hands.mdl");

	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowAmmo" );
	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowHealth" );

	CBaseEntity::PrecacheScriptSound( "FX_AntlionImpact.ShellImpact" );
	CBaseEntity::PrecacheScriptSound( "Missile.ShotDown" );
	CBaseEntity::PrecacheScriptSound( "Bullets.DefaultNearmiss" );
	CBaseEntity::PrecacheScriptSound( "Bullets.GunshipNearmiss" );
	CBaseEntity::PrecacheScriptSound( "Bullets.StriderNearmiss" );

	CBaseEntity::PrecacheScriptSound( "Geiger.BeepHigh" );
	CBaseEntity::PrecacheScriptSound( "Geiger.BeepLow" );

#ifdef DARKINTERVAL
	CBaseEntity::PrecacheModel("models/player/gordon.mdl");
	CBaseEntity::PrecacheModel("models/portals/portal1.mdl");
	CBaseEntity::PrecacheModel("models/portals/portal2.mdl");
	CBaseEntity::PrecacheScriptSound("DI_BulletHit.Feedback");
#endif
}

// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	// restart the entire server
	engine->ServerCommand("reload\n");
}

void GameStartFrame( void )
{
	VPROF("GameStartFrame()");
	if ( g_fGameOver )
		return;
#ifndef DARKINTERVAL // DI is singleplayer, so it ditched teams
	gpGlobals->teamplay = ( teamplay.GetInt() != 0 );
#endif
}

#ifdef HL2_EPISODIC
extern ConVar gamerules_survival;
#endif

//=========================================================
// instantiate the proper game rules object
//=========================================================
void InstallGameRules()
{
#ifdef HL2_EPISODIC
	if ( gamerules_survival.GetBool() )
	{
		// Survival mode
		CreateGameRulesObject("CHalfLife2Survival");
	} else
#endif
	{
		// generic half-life
		CreateGameRulesObject("CHalfLife2");
	}
}

