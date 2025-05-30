#ifndef	BEAMCHASER_H
#define	BEAMCHASER_H

#define MAX_SHIELDBEAM_NODES 10

class CBeam;

//#################################################################################
//	>> CShieldBeamNode
//#################################################################################
class CShieldBeamNode : public CBaseEntity
{
public:
	DECLARE_CLASS( CShieldBeamNode, CBaseEntity );

	void				Spawn( void );
	CShieldBeamNode*	m_pNextNode;

	DECLARE_DATADESC();
};

//#################################################################################
//	>> CShieldBeam
//#################################################################################
class CShieldBeam : public CLogicalEntity
{
public:
	DECLARE_CLASS( CShieldBeam, CLogicalEntity );

	static CShieldBeam* ShieldBeamCreate(CBaseEntity* pHeadEntity, int nAttachment);

	void				ShieldBeamStart(CBaseEntity* pHeadEntity, CBaseEntity*	pTailEntity, const Vector &vInitialVelocity,  float speed );
	void				ShieldBeamStop( void );
	void				SetNoise( float fNoise );
	bool				IsShieldBeamOn( void );
	bool				ReachedTail( void );
	void				ShieldBeamThink( void );

	EHANDLE				m_hHead;
	int					m_nHeadAttachment;
	EHANDLE				m_hTail;
	Vector				m_vTailOffset;

	float				m_fSpeed;

	DECLARE_DATADESC();

private:
	CBeam*				m_pBeam;				// The spline beam
	CShieldBeamNode*	m_pBeamNode;			// Nodes that form the spline beam
	float				m_fNextNodeTime;
	bool				m_bKillBeam;
	bool				m_bReachedTail;
	float				m_fBrightness;
	float				m_fNoise;
	Vector				m_vInitialVelocity;
	
	int					CountNodes(void);
	void				UpdateBeam( void );		// Update the spline beam
	float				ThinkInterval( void );
	CShieldBeamNode*	GetNewNode( void );
	bool				UpdateShieldNode( CShieldBeamNode* pNode, Vector &vTargetPos, float fInterval );
};

#endif	//BEAMCHASER_H


