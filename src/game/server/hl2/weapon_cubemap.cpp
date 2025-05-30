//========================================================================//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CWeaponCubemap : public CBaseCombatWeapon
{
public:

	DECLARE_CLASS( CWeaponCubemap, CBaseCombatWeapon );

	void	Precache( void );

	bool	HasAnyAmmo( void )	{ return true; }

	void	Spawn( void );

	DECLARE_SERVERCLASS();
};

LINK_ENTITY_TO_CLASS( weapon_cubemap, CWeaponCubemap );

IMPLEMENT_SERVERCLASS_ST( CWeaponCubemap, DT_WeaponCubemap )
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCubemap::Precache( void )
{
	BaseClass::Precache();
}

void CWeaponCubemap::Spawn( void )
{
	BaseClass::Spawn();

	UTIL_SetSize( this, Vector( -16, -16, -16 ),  Vector( 16, 16, 16 ) );
}
