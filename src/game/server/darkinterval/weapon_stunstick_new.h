#include "cbase.h"
#include "npcevent.h"
//#include "weapon_hl2mpbasebasebludgeon.h"
#include "basebludgeonweapon.h"
#include "IEffects.h"
#include "debugoverlay_shared.h"

#ifndef CLIENT_DLL
#include "npc_metropolice.h"
#include "te_effect_dispatch.h"
#endif

#ifdef CLIENT_DLL

#include "iviewrender_beams.h"
#include "beam_shared.h"
#include "materialsystem/IMaterial.h"
#include "model_types.h"
#include "c_te_effect_dispatch.h"
#include "fx_quad.h"
#include "fx.h"

extern void DrawHalo(IMaterial* pMaterial, const Vector &source, float scale, float const *color, float flHDRColorScale);
extern void FormatViewModelAttachment(Vector &vOrigin, bool bInverse);

#endif

extern ConVar metropolice_move_and_melee;

#define	STUNSTICK_RANGE				75.0f
#define	STUNSTICK_REFIRE			0.8f
#define	STUNSTICK_BEAM_MATERIAL		"sprites/lgtning.vmt"
#define STUNSTICK_GLOW_MATERIAL		"sprites/light_glow02_add"
#define STUNSTICK_GLOW_MATERIAL2	"effects/blueflare1"
#define STUNSTICK_GLOW_MATERIAL_NOZ	"sprites/light_glow02_add_noz"

#ifdef CLIENT_DLL
#define CWeaponStunStick C_WeaponStunStick
#endif

class CWeaponStunStick : public CBaseHLBludgeonWeapon
{
	DECLARE_CLASS(CWeaponStunStick, CBaseHLBludgeonWeapon);

public:

	CWeaponStunStick();

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
/*
#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif
*/
#ifdef CLIENT_DLL
	virtual int				DrawModel(int flags);
	virtual void			ClientThink(void);
	virtual void			OnDataChanged(DataUpdateType_t updateType);
	virtual RenderGroup_t	GetRenderGroup(void);
	virtual void			ViewModelDrawn(C_BaseViewModel *pBaseViewModel);

#endif

	virtual void Precache();

	void		Spawn();

	float		GetRange(void) { return STUNSTICK_RANGE; }
	float		GetFireRate(void) { return STUNSTICK_REFIRE; }


	bool		Deploy(void);
	bool		Holster(CBaseCombatWeapon *pSwitchingTo = NULL);

	void		Drop(const Vector &vecVelocity);
	void		ImpactEffect(trace_t &traceHit);
	void		SecondaryAttack(void) {}
	void		SetStunState(bool state);
	bool		GetStunState(void);

#ifndef CLIENT_DLL
	void		Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);
	int			WeaponMeleeAttack1Condition(float flDot, float flDist);
#endif

	float		GetDamageForActivity(Activity hitActivity);

	CWeaponStunStick(const CWeaponStunStick &);

private:

#ifdef CLIENT_DLL

#define	NUM_BEAM_ATTACHMENTS	9

	struct stunstickBeamInfo_t
	{
		int IDs[2];		// 0 - top, 1 - bottom
	};

	stunstickBeamInfo_t		m_BeamAttachments[NUM_BEAM_ATTACHMENTS];	// Lookup for arc attachment points on the head of the stick
	int						m_BeamCenterAttachment;						// "Core" of the effect (center of the head)

	void	SetupAttachmentPoints(void);
	void	DrawFirstPersonEffects(void);
	void	DrawThirdPersonEffects(void);
	void	DrawEffects(void);
	bool	InSwing(void);

	bool	m_bSwungLastFrame;

#define	FADE_DURATION	0.25f

	float	m_flFadeTime;

#endif

	CNetworkVar(bool, m_bActive);
};