//========================================================================//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "iviewrender.h"
#include "view.h"
#include "studio.h"
#include "bone_setup.h"
#include "model_types.h"
#include "beamdraw.h"
#include "engine/ivdebugoverlay.h"
#include "iviewrender_beams.h"
#include "fx.h"
#ifdef DARKINTERVAL
#include "particle_simple3d.h"
#endif
#include "ieffects.h"
#include "c_entitydissolve.h"
#include "movevars_shared.h"
#include "clienteffectprecachesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#ifdef DARKINTERVAL
#define sparkmat "custom_uni/dissolve_fire_spark"
#define muzzlemat "custom_uni/dissolve_fire_glow"
#define ashmat "custom_uni/dissolve_fire_spark"
#endif
CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectBuild )
CLIENTEFFECT_MATERIAL( "effects/tesla_glow_noz" )
CLIENTEFFECT_MATERIAL( "effects/spark" )
CLIENTEFFECT_MATERIAL( "effects/combinemuzzle2" )
#ifdef DARKINTERVAL
CLIENTEFFECT_MATERIAL( sparkmat )
CLIENTEFFECT_MATERIAL( muzzlemat )
CLIENTEFFECT_MATERIAL( ashmat )
#endif
CLIENTEFFECT_REGISTER_END()

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT( C_EntityDissolve, DT_EntityDissolve, CEntityDissolve )
	RecvPropTime(RECVINFO(m_flStartTime)),
	RecvPropFloat(RECVINFO(m_flFadeOutStart)),
	RecvPropFloat(RECVINFO(m_flFadeOutLength)),
	RecvPropFloat(RECVINFO(m_flFadeOutModelStart)),
	RecvPropFloat(RECVINFO(m_flFadeOutModelLength)),
	RecvPropFloat(RECVINFO(m_flFadeInStart)),
	RecvPropFloat(RECVINFO(m_flFadeInLength)),
	RecvPropInt(RECVINFO(m_nDissolveType)),
	RecvPropVector( RECVINFO( m_vDissolverOrigin) ),
	RecvPropInt( RECVINFO( m_nMagnitude ) ),
#ifdef DARKINTERVAL
	RecvPropBool( RECVINFO(m_dissolve_spawn_ashes_bool )),
#endif
END_RECV_TABLE()

extern PMaterialHandle g_Material_Spark;
PMaterialHandle g_Material_AR2Glow = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_EntityDissolve::C_EntityDissolve( void )
{
	m_bLinkedToServerEnt = true;
	m_pController = NULL;
	m_bCoreExplode = false;
	m_vEffectColor = Vector( 255, 255, 255 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EntityDissolve::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if ( GetMoveParent() )
	{
		GetMoveParent()->GetRenderBounds( theMins, theMaxs );
	}
	else
	{
		theMins = GetAbsOrigin();
		theMaxs = theMaxs;
	}
}

//-----------------------------------------------------------------------------
// On data changed
//-----------------------------------------------------------------------------
void C_EntityDissolve::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_flNextSparkTime = m_flStartTime;
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Cleanup
//-----------------------------------------------------------------------------
void C_EntityDissolve::UpdateOnRemove( void )
{
	if ( m_pController )
	{
		physenv->DestroyMotionController( m_pController );
		m_pController = NULL;
	}

	BaseClass::UpdateOnRemove();
}

//------------------------------------------------------------------------------
// Apply the forces to the entity
//------------------------------------------------------------------------------
IMotionEvent::simresult_e C_EntityDissolve::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	linear.Init();
	angular.Init();

	// Make it zero g
	linear.z -= -1.02 * GetCurrentGravity();

	Vector vel;
	AngularImpulse angVel;
	pObject->GetVelocity( &vel, &angVel );
	vel += linear * deltaTime; // account for gravity scale

	Vector unitVel = vel;
	Vector unitAngVel = angVel;

	float speed = VectorNormalize( unitVel );
//	float angSpeed = VectorNormalize( unitAngVel );

//	float speedScale = 0.0;
//	float angSpeedScale = 0.0;

	float flLinearLimit = 50;
	float flLinearLimitDelta = 40;
	if ( speed > flLinearLimit )
	{
		float flDeltaVel = (flLinearLimit - speed) / deltaTime;
		if ( flLinearLimitDelta != 0.0f )
		{
			float flMaxDeltaVel = -flLinearLimitDelta / deltaTime;
			if ( flDeltaVel < flMaxDeltaVel )
			{
				flDeltaVel = flMaxDeltaVel;
			}
		}
		VectorMA( linear, flDeltaVel, unitVel, linear );
	}

	return SIM_GLOBAL_ACCELERATION;
}


//-----------------------------------------------------------------------------
// Tesla effect
//-----------------------------------------------------------------------------
static void FX_BuildTesla( C_BaseEntity *pEntity, Vector &vecOrigin, Vector &vecEnd )
{
	BeamInfo_t beamInfo;
	beamInfo.m_pStartEnt = pEntity;
	beamInfo.m_nStartAttachment = 0;
	beamInfo.m_pEndEnt = NULL;
	beamInfo.m_nEndAttachment = 0;
	beamInfo.m_nType = TE_BEAMTESLA;
	beamInfo.m_vecStart = vecOrigin;
	beamInfo.m_vecEnd = vecEnd;
	beamInfo.m_pszModelName = "sprites/lgtning.vmt";
	beamInfo.m_flHaloScale = 0.0;
	beamInfo.m_flLife = random->RandomFloat( 0.25f, 1.0f );
	beamInfo.m_flWidth = random->RandomFloat( 8.0f, 14.0f );
	beamInfo.m_flEndWidth = 1.0f;
	beamInfo.m_flFadeLength = 0.5f;
	beamInfo.m_flAmplitude = 24;
	beamInfo.m_flBrightness = 255.0;
	beamInfo.m_flSpeed = 150.0f;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 30.0;
	beamInfo.m_flRed = 255.0;
	beamInfo.m_flGreen = 255.0;
	beamInfo.m_flBlue = 255.0;
	beamInfo.m_nSegments = 18;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = 0; //FBEAM_ONLYNOISEONCE;
	
	beams->CreateBeamEntPoint( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Tesla effect
//-----------------------------------------------------------------------------
void C_EntityDissolve::BuildTeslaEffect( mstudiobbox_t *pHitBox, const matrix3x4_t &hitboxToWorld, bool bRandom, float flYawOffset )
{
	Vector vecOrigin;
	QAngle vecAngles;
	MatrixGetColumn( hitboxToWorld, 3, vecOrigin );
	MatrixAngles( hitboxToWorld, vecAngles.Base() );
	C_BaseEntity *pEntity = GetMoveParent();

	// Make a couple of tries at it
	int iTries = -1;
	Vector vecForward;
	trace_t tr;
	do
	{
		iTries++;

		// Some beams are deliberatly aimed around the point, the rest are random.
		if ( !bRandom )
		{
			QAngle vecTemp = vecAngles;
			vecTemp[YAW] += flYawOffset;
			AngleVectors( vecTemp, &vecForward );

			// Randomly angle it up or down
			vecForward.z = RandomFloat( -1, 1 );
		}
		else
		{
			vecForward = RandomVector( -1, 1 );
		}

		UTIL_TraceLine( vecOrigin, vecOrigin + (vecForward * 192), MASK_SHOT, pEntity, COLLISION_GROUP_NONE, &tr );
	} while ( tr.fraction >= 1.0 && iTries < 3 );

	Vector vecEnd = tr.endpos - (vecForward * 8);

	// Only spark & glow if we hit something
	if ( tr.fraction < 1.0 )
	{
		if ( !EffectOccluded( tr.endpos ) )
		{
			// Move it towards the camera
			Vector vecFlash = tr.endpos;
			Vector vecForward;
			AngleVectors( MainViewAngles(), &vecForward );
			vecFlash -= (vecForward * 8);

			g_pEffects->EnergySplash( vecFlash, -vecForward, false );

			// End glow
			CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "dust" );
			pSimple->SetSortOrigin( vecFlash );
			SimpleParticle *pParticle;
			pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/tesla_glow_noz" ), vecFlash );
			if ( pParticle != NULL )
			{
				pParticle->m_flLifetime = 0.0f;
				pParticle->m_flDieTime	= RandomFloat( 0.5, 1 );
				pParticle->m_vecVelocity = vec3_origin;
				Vector color( 1,1,1 );
				float  colorRamp = RandomFloat( 0.75f, 1.25f );
				pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
				pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
				pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
				pParticle->m_uchStartSize	= RandomFloat( 6,13 );
				pParticle->m_uchEndSize		= pParticle->m_uchStartSize - 2;
				pParticle->m_uchStartAlpha	= 255;
				pParticle->m_uchEndAlpha	= 10;
				pParticle->m_flRoll			= RandomFloat( 0,360 );
				pParticle->m_flRollDelta	= 0;
			}
		}
	}

	// Build the tesla
	FX_BuildTesla( pEntity, vecOrigin, tr.endpos );
}

//-----------------------------------------------------------------------------
// Sorts the components of a vector
//-----------------------------------------------------------------------------
static inline void SortAbsVectorComponents( const Vector& src, int* pVecIdx )
{
	Vector absVec( fabs(src[0]), fabs(src[1]), fabs(src[2]) );

	int maxIdx = (absVec[0] > absVec[1]) ? 0 : 1;
	if (absVec[2] > absVec[maxIdx])
	{
		maxIdx = 2;
	}

	// always choose something right-handed....
	switch(	maxIdx )
	{
	case 0:
		pVecIdx[0] = 1;
		pVecIdx[1] = 2;
		pVecIdx[2] = 0;
		break;
	case 1:
		pVecIdx[0] = 2;
		pVecIdx[1] = 0;
		pVecIdx[2] = 1;
		break;
	case 2:
		pVecIdx[0] = 0;
		pVecIdx[1] = 1;
		pVecIdx[2] = 2;
		break;
	}
}

//-----------------------------------------------------------------------------
// Compute the bounding box's center, size, and basis
//-----------------------------------------------------------------------------
void C_EntityDissolve::ComputeRenderInfo( mstudiobbox_t *pHitBox, const matrix3x4_t &hitboxToWorld, 
										 Vector *pVecAbsOrigin, Vector *pXVec, Vector *pYVec )
{
	// Compute the center of the hitbox in worldspace
	Vector vecHitboxCenter;
	VectorAdd( pHitBox->bbmin, pHitBox->bbmax, vecHitboxCenter );
	vecHitboxCenter *= 0.5f;
	VectorTransform( vecHitboxCenter, hitboxToWorld, *pVecAbsOrigin );

	// Get the object's basis
	Vector vec[3];
	MatrixGetColumn( hitboxToWorld, 0, vec[0] );
	MatrixGetColumn( hitboxToWorld, 1, vec[1] );
	MatrixGetColumn( hitboxToWorld, 2, vec[2] );
//	vec[1] *= -1.0f;

	Vector vecViewDir;
	VectorSubtract( CurrentViewOrigin(), *pVecAbsOrigin, vecViewDir );
	VectorNormalize( vecViewDir );

	// Project the shadow casting direction into the space of the hitbox
	Vector localViewDir;
	localViewDir[0] = DotProduct( vec[0], vecViewDir );
	localViewDir[1] = DotProduct( vec[1], vecViewDir );
	localViewDir[2] = DotProduct( vec[2], vecViewDir );

	// Figure out which vector has the largest component perpendicular
	// to the view direction...
	// Sort by how perpendicular it is
	int vecIdx[3];
	SortAbsVectorComponents( localViewDir, vecIdx );

	// Here's our hitbox basis vectors; namely the ones that are
	// most perpendicular to the view direction
	*pXVec = vec[vecIdx[0]];
	*pYVec = vec[vecIdx[1]];

	// Project them into a plane perpendicular to the view direction
	*pXVec -= vecViewDir * DotProduct( vecViewDir, *pXVec );
	*pYVec -= vecViewDir * DotProduct( vecViewDir, *pYVec );
	VectorNormalize( *pXVec );
	VectorNormalize( *pYVec );

	// Compute the hitbox size
	Vector boxSize;
	VectorSubtract( pHitBox->bbmax, pHitBox->bbmin, boxSize );

	// We project the two longest sides into the vectors perpendicular
	// to the projection direction, then add in the projection of the perp direction
	Vector2D size( boxSize[vecIdx[0]], boxSize[vecIdx[1]] );
	size.x *= fabs( DotProduct( vec[vecIdx[0]], *pXVec ) );
	size.y *= fabs( DotProduct( vec[vecIdx[1]], *pYVec ) );

	// Add the third component into x and y
	size.x += boxSize[vecIdx[2]] * fabs( DotProduct( vec[vecIdx[2]], *pXVec ) );
	size.y += boxSize[vecIdx[2]] * fabs( DotProduct( vec[vecIdx[2]], *pYVec ) );

	// Bloat a bit, since the shadow wants to extend outside the model a bit
	size *= 2.0f;

	// Clamp the minimum size
	Vector2DMax( size, Vector2D(10.0f, 10.0f), size );

	// Factor the size into the xvec + yvec
	(*pXVec) *= size.x * 0.5f;
	(*pYVec) *= size.y * 0.5f;
}


//-----------------------------------------------------------------------------
// Sparks!
//-----------------------------------------------------------------------------
void C_EntityDissolve::DoSparks( mstudiohitboxset_t *set, matrix3x4_t *hitboxbones[MAXSTUDIOBONES] )
{
	if ( m_flNextSparkTime > CURTIME )
		return;

	float dt = m_flStartTime + m_flFadeOutStart - CURTIME;
	dt = clamp( dt, 0.0f, m_flFadeOutStart );
	
	float flNextTime;
	if (m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL)
	{
		flNextTime = SimpleSplineRemapVal( dt, 0.0f, m_flFadeOutStart, 2.0f * TICK_INTERVAL, 0.4f );
	}
	else
	{
		// m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL_LIGHT);
		flNextTime = SimpleSplineRemapVal( dt, 0.0f, m_flFadeOutStart, 0.3f, 1.0f );
	}

	m_flNextSparkTime = CURTIME + flNextTime;

	// Send out beams around us
	int iNumBeamsAround = 2;
	int iNumRandomBeams = 1;
	int iTotalBeams = iNumBeamsAround + iNumRandomBeams;
	float flYawOffset = RandomFloat(0,360);
	for ( int i = 0; i < iTotalBeams; i++ )
	{
		int nHitbox = random->RandomInt( 0, set->numhitboxes - 1 );
		mstudiobbox_t *pBox = set->pHitbox(nHitbox);

		float flActualYawOffset = 0;
		bool bRandom = ( i >= iNumBeamsAround );
		if ( !bRandom )
		{
			flActualYawOffset = anglemod( flYawOffset + ((360 / iTotalBeams) * i) );
		}

		BuildTeslaEffect( pBox, *hitboxbones[pBox->bone], bRandom, flActualYawOffset );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EntityDissolve::SetupEmitter( void )
{
	if ( !m_pEmitter )
	{
		m_pEmitter = CSimpleEmitter::Create( "C_EntityDissolve" );
		m_pEmitter->SetSortOrigin( GetAbsOrigin() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float C_EntityDissolve::GetFadeInPercentage( void )
{
	float dt = CURTIME - m_flStartTime;
	
	if ( dt > m_flFadeOutStart )
		return 1.0f;

	if ( dt < m_flFadeInStart )
		return 0.0f;

	if ( (dt > m_flFadeInStart) && (dt < m_flFadeInStart + m_flFadeInLength) )
	{
		dt -= m_flFadeInStart;
		
		return ( dt / m_flFadeInLength );
	}

	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float C_EntityDissolve::GetFadeOutPercentage( void )
{
	float dt = CURTIME - m_flStartTime;
	
	if ( dt < m_flFadeInStart )
		return 1.0f;

	if ( dt > m_flFadeOutStart )
	{
		dt -= m_flFadeOutStart;
		
		if ( dt > m_flFadeOutLength )
			return 0.0f;
		
		return 1.0f - ( dt / m_flFadeOutLength );
	}
	
	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float C_EntityDissolve::GetModelFadeOutPercentage( void )
{
	float dt = CURTIME - m_flStartTime;
	
	if ( dt < m_flFadeOutModelStart )
		return 1.0f;

	if ( dt > m_flFadeOutModelStart )
	{
		dt -= m_flFadeOutModelStart;
		
		if ( dt > m_flFadeOutModelLength )
			return 0.0f;
		
		return 1.0f - ( dt / m_flFadeOutModelLength );
	}
	
	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EntityDissolve::ClientThink( void )
{
	C_BaseEntity *pEnt = GetMoveParent();
	if ( !pEnt )
		return;

	bool bIsRagdoll;

	C_BaseAnimating *pAnimating = GetMoveParent() ? GetMoveParent()->GetBaseAnimating() : NULL;
	if (!pAnimating)
		return;

	bIsRagdoll = pAnimating->IsRagdoll();
	
	// NOTE: IsRagdoll means *client-side* ragdoll. We shouldn't be trying to fight
	// the server ragdoll (or any server physics) on the client
	if (( !m_pController ) && ( m_nDissolveType == ENTITY_DISSOLVE_NORMAL ) && bIsRagdoll )
	{
		IPhysicsObject *ppList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
		int nCount = pEnt->VPhysicsGetObjectList( ppList, ARRAYSIZE(ppList) );
		if ( nCount > 0 )
		{
			m_pController = physenv->CreateMotionController( this );
			for ( int i = 0; i < nCount; ++i )
			{
				m_pController->AttachObject( ppList[i], true );
			}
		}
	}
#ifdef DARKINTERVAL
	if (m_nDissolveType == ENTITY_DISSOLVE_BURN && GetModelFadeOutPercentage() > 0.2f)
	{
		//Play a sound
	//	CPASAttenuationFilter filter(pAnimating);
	//	pAnimating->EmitSound(filter, pAnimating->GetSoundSourceIndex(), "General.BurningObject");
	}
#endif
	color32 color;

	color.r = ( 1.0f - GetFadeInPercentage() ) * m_vEffectColor.x;
	color.g = ( 1.0f - GetFadeInPercentage() ) * m_vEffectColor.y;
	color.b = ( 1.0f - GetFadeInPercentage() ) * m_vEffectColor.z;
	color.a = GetModelFadeOutPercentage() * 255.0f;

	// Setup the entity fade
	pEnt->SetRenderMode( kRenderTransColor );
	pEnt->SetRenderColor( color.r, color.g, color.b, color.a );

	if ( GetModelFadeOutPercentage() <= 0.2f )
	{
		m_bCoreExplode = true;
	}

	// If we're dead, fade out
	if ( GetFadeOutPercentage() <= 0.0f )
	{
#ifdef DARKINTERVAL
		if (m_nDissolveType == ENTITY_DISSOLVE_BURN)
		{
			//Stop the sound
		//	CPASAttenuationFilter filter(pAnimating);
		//	pAnimating->EmitSound(filter, pAnimating->GetSoundSourceIndex(), "General.StopBurning");
		}
#endif
		// Do NOT remove from the client entity list. It'll confuse the local network backdoor, and the entity will never get destroyed
		// because when the server says to destroy it, the client won't be able to find it.
		// ClientEntityList().RemoveEntity( GetClientHandle() );

		partition->Remove( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS, CollisionProp()->GetPartitionHandle() );

		RemoveFromLeafSystem();

		//FIXME: Ick!
		// I'll assume we don't need the ragdoll either so I'll remove that too.
		if ( m_bLinkedToServerEnt == false )
		{
			Release();

			C_ClientRagdoll *pRagdoll = dynamic_cast <C_ClientRagdoll *> ( pEnt );

			if ( pRagdoll )
			{
				pRagdoll->ReleaseRagdoll();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flags - 
// Output : int
//-----------------------------------------------------------------------------
#ifdef DARKINTERVAL
ConVar entity_dissolve_burn_numparticles("entity_dissolve_burn_numparticles", "1");
ConVar entity_dissolve_burn_numashes("entity_dissolve_burn_numashes", "2");
ConVar entity_dissolve_burn_numparts("entity_dissolve_burn_numparts", "1");
ConVar entity_dissolve_burn_maxemits("entity_dissolve_burn_maxemits", "3");
ConVar entity_dissolve_burn_dietime("entity_dissolve_burn_dietime", "1.0f");
ConVar entity_dissolve_burn_lifetime("entity_dissolve_burn_lifetime", "0.0f");
ConVar entity_dissolve_burn_scale("entity_dissolve_burn_scale", "1.0f");
#endif
int C_EntityDissolve::DrawModel( int flags )
{
	// See if we should draw
	if ( gpGlobals->frametime == 0 || m_bReadyToDraw == false )
		return 0;

	C_BaseAnimating *pAnimating = GetMoveParent() ? GetMoveParent()->GetBaseAnimating() : NULL;
	if ( pAnimating == NULL )
		return 0;

	matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
	if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) == false )
		return 0;

	studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
	if ( pStudioHdr == NULL )
		return false;

	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( pAnimating->GetHitboxSet() );
	if ( set == NULL )
		return false;
	
	// Make sure the emitter is setup properly
	SetupEmitter();
	
	// Get fade percentages for the effect
	float fadeInPerc = GetFadeInPercentage();
	float fadeOutPerc = GetFadeOutPercentage();

	float fadePerc = ( fadeInPerc >= 1.0f ) ? fadeOutPerc : fadeInPerc;

	Vector vecSkew = vec3_origin;

	// Do extra effects under certain circumstances
	if ( ( fadePerc < 0.99f ) && ( (m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL) || (m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL_LIGHT) ) )
	{
		DoSparks( set, hitboxbones );
	}

	// Skew the particles in front or in back of their targets
	vecSkew = CurrentViewForward() * ( 8.0f - ( ( 1.0f - fadePerc ) * 32.0f ) );

	float spriteScale = ( ( CURTIME - m_flStartTime ) / m_flFadeOutLength );
	spriteScale = clamp( spriteScale, 0.75f, 1.0f );

	// Cache off this material reference
	if ( g_Material_Spark == NULL )
	{
		g_Material_Spark = ParticleMgr()->GetPMaterial( "effects/spark" );
	}

	if ( g_Material_AR2Glow == NULL )
	{
#ifdef DARKINTERVAL
		if (m_nDissolveType == ENTITY_DISSOLVE_BURN)
			g_Material_AR2Glow = ParticleMgr()->GetPMaterial(muzzlemat);
		else
#endif
			g_Material_AR2Glow = ParticleMgr()->GetPMaterial( "effects/combinemuzzle2" );
	}

	SimpleParticle *sParticle;
	
	for ( int i = 0; i < set->numhitboxes; ++i )
	{
#ifdef DARKINTERVAL
		if (m_nDissolveType == ENTITY_DISSOLVE_BURN)
		{
			if (i % 2 || i % 3) // burn effects emit way too many particles on characters. 
				continue;
		}
#endif
		Vector vecAbsOrigin, xvec, yvec;
		mstudiobbox_t *pBox = set->pHitbox(i);
		ComputeRenderInfo( pBox, *hitboxbones[pBox->bone], &vecAbsOrigin, &xvec, &yvec );

		Vector offset;
		Vector	xDir, yDir;

		xDir = xvec;
		float xScale = VectorNormalize( xDir ) * 0.75f;

		yDir = yvec;
		float yScale = VectorNormalize( yDir ) * 0.75f;

		int numParticles = clamp( 3.0f * fadePerc, 0.f, 3.f );

		int iTempParts = 2;

		if ( m_nDissolveType == ENTITY_DISSOLVE_CORE )
		{
			if ( m_bCoreExplode == true )
			{
				numParticles = 15;
				iTempParts = 20;
			}
		}
#ifdef DARKINTERVAL
		else if (m_nDissolveType == ENTITY_DISSOLVE_BURN)
		{
			numParticles = entity_dissolve_burn_numparticles.GetInt();
			iTempParts = entity_dissolve_burn_numparts.GetInt();
			spriteScale = RandomFloat(entity_dissolve_burn_scale.GetFloat() * 0.5, entity_dissolve_burn_scale.GetFloat() * 2);
		//	spriteScale /= m_flFadeOutLength;
		}
#endif
		for ( int j = 0; j < iTempParts; j++ )
		{
			// Skew the origin
			offset = xDir * Helper_RandomFloat( -xScale*0.5f, xScale*0.5f ) + yDir * Helper_RandomFloat( -yScale*0.5f, yScale*0.5f );
			offset += vecSkew;

			if ( random->RandomInt( 0, 2 ) != 0 )
				continue;
#ifdef DARKINTERVAL
			if( m_nDissolveType == ENTITY_DISSOLVE_BURN )				
				sParticle = (SimpleParticle *)m_pEmitter->AddParticle(sizeof(SimpleParticle), ParticleMgr()->GetPMaterial(sparkmat), vecAbsOrigin + offset);
			else
#endif
				sParticle = (SimpleParticle *) m_pEmitter->AddParticle( sizeof(SimpleParticle), g_Material_Spark, vecAbsOrigin + offset );
			
			if ( sParticle == NULL )
				return 1;

			sParticle->m_vecVelocity	= Vector( Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( 16.0f, 64.0f ) );
			
			if ( m_nDissolveType == ENTITY_DISSOLVE_CORE )
			{
				if ( m_bCoreExplode == true )
				{
					Vector vDirection = (vecAbsOrigin + offset) - m_vDissolverOrigin;
					VectorNormalize( vDirection );
					sParticle->m_vecVelocity = vDirection * m_nMagnitude;
				}
			}

			if ( sParticle->m_vecVelocity.z > 0 )
			{
				sParticle->m_uchStartSize	= random->RandomFloat( 4, 6 ) * spriteScale;
			}
			else
			{
				sParticle->m_uchStartSize	= 2 * spriteScale;
			}

			sParticle->m_flDieTime = random->RandomFloat( 0.4f, 0.5f );
			
			// If we're the last particles, last longer
			if ( numParticles == 0 )
			{
				sParticle->m_flDieTime *= 2.0f;
				sParticle->m_uchStartSize = 2 * spriteScale;
#ifdef DARKINTERVAL
				if (m_nDissolveType != ENTITY_DISSOLVE_BURN)
				{
					sParticle->m_flRollDelta = Helper_RandomFloat(-4.0f, 4.0f);
				}
#endif
				if ( m_nDissolveType == ENTITY_DISSOLVE_CORE )
				{
					if ( m_bCoreExplode == true )
					{
						sParticle->m_flDieTime *= 2.0f;
						sParticle->m_flRollDelta	= Helper_RandomFloat( -1.0f, 1.0f );
					}
				}
			}
			else
			{
#ifdef DARKINTERVAL
				if (m_nDissolveType != ENTITY_DISSOLVE_BURN)
				{
					sParticle->m_flRollDelta = Helper_RandomFloat(-8.0f, 8.0f);
				}
#endif
			}
			
			sParticle->m_flLifetime		= 0.0f;

			sParticle->m_flRoll			= Helper_RandomInt( 0, 360 );

			float alpha = 255;

			sParticle->m_uchColor[0]	= m_vEffectColor.x;
			sParticle->m_uchColor[1]	= m_vEffectColor.y;
			sParticle->m_uchColor[2]	= m_vEffectColor.z;
			sParticle->m_uchStartAlpha	= alpha;
			sParticle->m_uchEndAlpha	= 0;
			sParticle->m_uchEndSize		= 0;
		}
			
		for ( int j = 0; j < numParticles; j++ )
		{
			offset = xDir * Helper_RandomFloat( -xScale*0.5f, xScale*0.5f ) + yDir * Helper_RandomFloat( -yScale*0.5f, yScale*0.5f );
			offset += vecSkew;
#ifdef DARKINTERVAL
			if (m_nDissolveType == ENTITY_DISSOLVE_BURN)
				sParticle = (SimpleParticle *)m_pEmitter->AddParticle(sizeof(SimpleParticle), ParticleMgr()->GetPMaterial(muzzlemat), vecAbsOrigin + offset);
			else
#endif
				sParticle = (SimpleParticle *) m_pEmitter->AddParticle( sizeof(SimpleParticle), g_Material_AR2Glow, vecAbsOrigin + offset );

			if ( sParticle == NULL )
				return 1;
#ifdef DARKINTERVAL
		//	if (m_nDissolveType == ENTITY_DISSOLVE_BURN)
		//	{
		//		if (random->RandomInt(0, 2) != 0)
		//			continue;
		//	}
#endif
			sParticle->m_vecVelocity	= Vector( Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( -4.0f, 4.0f ), Helper_RandomFloat( -64.0f, 128.0f ) );
			sParticle->m_uchStartSize	= random->RandomFloat( 8, 12 ) * spriteScale;
			sParticle->m_flDieTime		= 0.1f;
			sParticle->m_flLifetime		= 0.0f;
#ifdef DARKINTERVAL
			if (m_nDissolveType != ENTITY_DISSOLVE_BURN)
			{
				sParticle->m_flRoll = Helper_RandomInt(0, 360);
				sParticle->m_flRollDelta = Helper_RandomFloat(-2.0f, 2.0f);
			}
			else
			{
			}
#endif
			float alpha = 255;

			sParticle->m_uchColor[0]	= m_vEffectColor.x;
			sParticle->m_uchColor[1]	= m_vEffectColor.y;
			sParticle->m_uchColor[2]	= m_vEffectColor.z;
			sParticle->m_uchStartAlpha	= alpha;
			sParticle->m_uchEndAlpha	= 0;
			sParticle->m_uchEndSize		= 0;

			if ( m_nDissolveType == ENTITY_DISSOLVE_CORE )
			{
				if ( m_bCoreExplode == true )
				{
					Vector vDirection = (vecAbsOrigin + offset) - m_vDissolverOrigin;

					VectorNormalize( vDirection );

					sParticle->m_vecVelocity = vDirection * m_nMagnitude;

					sParticle->m_flDieTime		= 0.5f;
				}
			}
#ifdef DARKINTERVAL
			else if (m_nDissolveType == ENTITY_DISSOLVE_BURN)
			{
				Vector vDirection = (vecAbsOrigin + offset) - Vector(0,0,400);

				VectorNormalize(vDirection);

				sParticle->m_vecVelocity = vDirection * RandomFloat(0.9f,1.1f);

				sParticle->m_flDieTime = 1.0f;
				sParticle->m_flLifetime = 0.0f;
			}
		}

		// don't do the follow ash emittance if not burn or electrical burn
		if (m_nDissolveType != ENTITY_DISSOLVE_BURN && m_nDissolveType != ENTITY_DISSOLVE_ELECTRICAL) return 1;

		// don't bother if the entity is set to not have ashes
		if (!m_dissolve_spawn_ashes_bool) return 1;

		CSmartPtr<CSimple3DEmitter> pAshEmitter = CSimple3DEmitter::Create("DissolveBurnAshes");
		if (pAshEmitter == NULL)
			return 1;
		
		pAshEmitter->SetSortOrigin(vecAbsOrigin);

		// Handle increased scale
		const float flMaxSpeed = 400.0f;
		const float flMinSpeed = 50.0f;
		float flAngularSpray = 1.0f;

		// Setup our collision information
		pAshEmitter->m_ParticleCollision.Setup(vecAbsOrigin, &yvec, flAngularSpray, flMinSpeed, flMaxSpeed, 400.0f, 0.2f);

		Particle3D *pAshParticle;
		Vector spawnOffset;
		
		// ashes for the burn/electrical types
		for (int k = 0; k < entity_dissolve_burn_numashes.GetInt(); k++)
		{
			// Skew the origin
			offset = xDir * Helper_RandomFloat(-xScale*0.5f, xScale*0.5f) + yDir * Helper_RandomFloat(-yScale*0.5f, yScale*0.5f);
			offset += vecSkew;

			spawnOffset = vecAbsOrigin + offset;
			pAshParticle = (Particle3D *)pAshEmitter->AddParticle(sizeof(Particle3D), g_Mat_Fleck_Cement[random->RandomInt(0, 1)], spawnOffset);

			if (pAshParticle == NULL)
				break;

			pAshParticle->m_flLifeRemaining = random->RandomFloat(2.0f, 3.0f);

			if (abs(sParticle->m_vecVelocity.z) > 0)
			{
				pAshParticle->m_uchSize = random->RandomFloat(4, 8) * spriteScale;
			}
			else
			{
				pAshParticle->m_uchSize = 4.0 * spriteScale;
			}

			pAshParticle->m_vecVelocity = Vector(Helper_RandomFloat(-16.0f, 16.0f), Helper_RandomFloat(-16.0f, 16.0f), Helper_RandomFloat(-128.0f, -256.0f));

			pAshParticle->m_vAngles = RandomAngle(0, 360);
			pAshParticle->m_flAngSpeed = random->RandomFloat(-800, 800);

			pAshParticle->m_flLifeRemaining = RandomFloat(10.0f, 15.0f);

			pAshParticle->m_uchFrontColor[0] = 50;
			pAshParticle->m_uchFrontColor[1] = 50;
			pAshParticle->m_uchFrontColor[2] = 50;
			pAshParticle->m_uchBackColor[0] = 15;
			pAshParticle->m_uchBackColor[1] = 15;
			pAshParticle->m_uchBackColor[2] = 15;	
#endif // DARKINTERVAL
		}
	}

	return 1;
}
