//========================================================================//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_PLAYERRESOURCE_H
#define C_PLAYERRESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"
#include "const.h"
#include "c_baseentity.h"
#include <igameresources.h>

#define PLAYER_UNCONNECTED_NAME	"unconnected"
#define PLAYER_ERROR_NAME		"ERRORNAME"

class C_PlayerResource : public C_BaseEntity, public IGameResources
{
	DECLARE_CLASS( C_PlayerResource, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_PlayerResource();
	virtual			~C_PlayerResource();

public : // IGameResources intreface
#ifndef DARKINTERVAL // DI is singleplayer, so it ditched teams
	// Team data access 
	virtual int		GetTeamScore(int index);
	virtual const char *GetTeamName(int index);
	virtual const Color&GetTeamColor(int index);
#endif
	// Player data access
	virtual bool	IsConnected( int index );
	virtual bool	IsAlive( int index );
	virtual bool	IsFakePlayer( int index );
	virtual bool	IsLocalPlayer( int index  );
	virtual bool	IsHLTV(int index);
	virtual bool	IsReplay(int index);

	virtual const char *GetPlayerName( int index );
	virtual int		GetPing( int index );
//	virtual int		GetPacketloss( int index );
	virtual int		GetPlayerScore( int index );
	virtual int		GetDeaths( int index );
#ifndef DARKINTERVAL // DI is singleplayer, so it ditched teams
	virtual int		GetTeam(int index);
#endif
	virtual int		GetFrags( int index );
	virtual int		GetHealth( int index );

	virtual void ClientThink();
	virtual	void	OnDataChanged(DataUpdateType_t updateType);

protected:
	void	UpdatePlayerName( int slot );

	// Data for each player that's propagated to all clients
	// Stored in individual arrays so they can be sent down via datatables
	string_t	m_szName[MAX_PLAYERS+1];
	int		m_iPing[MAX_PLAYERS+1];
	int		m_iScore[MAX_PLAYERS+1];
	int		m_iDeaths[MAX_PLAYERS+1];
	bool	m_bConnected[MAX_PLAYERS+1];
#ifndef DARKINTERVAL // DI is singleplayer, so it ditched teams
	int		m_iTeam[MAX_PLAYERS + 1];
#endif
	bool	m_bAlive[MAX_PLAYERS+1];
	int		m_iHealth[MAX_PLAYERS+1];
	Color	m_Colors[MAX_TEAMS];
	string_t m_szUnconnectedName;

};

extern C_PlayerResource *g_PR;

#endif // C_PLAYERRESOURCE_H
