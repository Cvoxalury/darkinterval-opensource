//========================================================================//
//
// Purpose: Hud locator element, helps direct the player to objects in the world
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "hud_numericdisplay.h"
#include <vgui_controls/Panel.h>
#include "hud.h"
#include "hud_suitpower.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include <vgui_controls/animationcontroller.h>
#include <vgui/ISurface.h>
#include "c_basehlplayer.h"
#ifdef DARKINTERVAL
#include "in_buttons.h"
//TE120--
#include "hl2_vehicle_radar.h"
#include "hud_locator.h"
//TE120--
#endif
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#ifdef DARKINTERVAL
//TE120--
//#define LOCATOR_MATERIAL_BIG_TICK		"vgui/icons/tick_long"
//#define LOCATOR_MATERIAL_SMALL_TICK		"vgui/icons/tick_short"
//#define LOCATOR_MATERIAL_BIG_TICK_N		"vgui/icons/tick_long"

#define LOCATOR_MATERIAL_POINTOFINTEREST	"vgui/icons/icon_poi"
#define LOCATOR_MATERIAL_OUTPOST			"vgui/icons/icon_combine"
#define LOCATOR_MATERIAL_MAINOBJECTIVE		"vgui/icons/icon_objective"

//#define LOCATOR_MATERIAL_LARGE_ENEMY	"vgui/icons/icon_strider"
//#define LOCATOR_MATERIAL_RADIATION		"vgui/icons/icon_radiation"
#endif

ConVar hud_locator_alpha("hud_locator_alpha", "230");
#ifdef DARKINTERVAL
ConVar hud_locator_fov("hud_locator_fov", "150");
#else
ConVar hud_locator_fov("hud_locator_fov", "350");
#endif
//ConVar hud_locator_lifetime("di_locator_lifetime", "5.0");
// 0 = disabled, 1 = hold hotkey to display, 2 = always enable
//ConVar hud_locator_displaymode("hud_locator_displaymode", "2"); // , FCVAR_ARCHIVE | FCVAR_REPLICATED);

using namespace vgui;

//#ifdef HL2_EPISODIC
#ifdef DARKINTERVAL
static CHudLocator *s_Locator = NULL;

CHudLocator *GetHudLocator()
{
	return s_Locator;
}
#endif // DARKINTERVAL
//#endif 
DECLARE_HUDELEMENT(CHudLocator);
#ifdef DARKINTERVAL
CLocatorContact::CLocatorContact()
{
	m_iType = 0;
	m_pEnt = NULL;
}
#endif
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudLocator::CHudLocator(const char *pElementName) : CHudElement(pElementName), BaseClass(NULL, "HudLocator")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent(pParent);
#ifdef DARKINTERVAL // todo - shouldn't this just be same as vanilla?
	SetHiddenBits(HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
#else
	SetHiddenBits(HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT);
#endif

#ifdef DARKINTERVAL
	m_textureID_IconPointOfInterest = -1;
	m_textureID_IconOutpost = -1;
	m_textureID_IconMainObjective = -1;
//	m_textureID_IconBigEnemy = -1;
//	m_textureID_IconRadiation = -1;

	m_iNumlocatorContacts = 0;
	s_Locator = this;
#else
	m_textureID_IconJalopy = -1;
	m_textureID_IconSmallTick = -1;
	m_textureID_IconBigTick = -1;
#endif
}
#ifndef DARKINTERVAL
CHudLocator::~CHudLocator(void)
{
	if (vgui::surface())
	{
		if (m_textureID_IconJalopy != -1)
		{
			vgui::surface()->DestroyTextureID(m_textureID_IconJalopy);
			m_textureID_IconJalopy = -1;
		}

		if (m_textureID_IconSmallTick != -1)
		{
			vgui::surface()->DestroyTextureID(m_textureID_IconSmallTick);
			m_textureID_IconSmallTick = -1;
		}

		if (m_textureID_IconBigTick != -1)
		{
			vgui::surface()->DestroyTextureID(m_textureID_IconBigTick);
			m_textureID_IconBigTick = -1;
		}
	}
}
#endif
//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pScheme -
//-----------------------------------------------------------------------------
void CHudLocator::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHudLocator::VidInit(void)
{
#ifndef DARKINTERVAL
	Reset();
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CHudLocator::ShouldDraw(void)
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return false;
#ifdef DARKINTERVAL
	// For manhack-styled red locator
	if (pPlayer && pPlayer->IsPlayingArcadeAsManhack())
		return true;
	// Need the HEV suit ( HL2 )
	if (!pPlayer->IsSuitEquipped())
		return false;
	if (engine->IsLevelMainMenuBackground()) // in a normal world, background maps wouldn't give you the HEV, but some EP2 ones still do.
		return false;
	if (!pPlayer->IsLocatorVisible())
		return false;
//	int checkConvar = hud_locator_displaymode.GetInt();
//	if (checkConvar == 0) // disabled via settings
//	{
//		return false;
//	}
//	else if (checkConvar == 1 && GetAlpha() == 0) // set to flash on hotkey but time's up
//	{
//		return false;
//	}
	return true;
#else
	if (pPlayer->GetVehicle())
		return false;

	if (pPlayer->m_HL2Local.m_vecLocatorOrigin == vec3_invalid)
		return false;

	return true;
#endif
}

void CHudLocator::OnThink(void)
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;
	
	if (false) //(hud_locator_displaymode.GetInt() == 1) // FIXME: finish this code
	{
		if (!(pPlayer->m_Local.m_bLocatorVisible) && GetAlpha() > 0 )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("LocatorFadeOut");

		}
		else
		{
			SetAlpha(hud_locator_alpha.GetInt());
		}
	}
}

void CHudLocator::Reset(void)
{
	m_vecLocation = Vector(0, 0, 0);
#ifdef DARKINTERVAL
	m_iNumlocatorContacts = 0;
//	SetBgColor(Color(255, 255, 0, 75));
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Make it a bit more convenient to do a filled rect.
//-----------------------------------------------------------------------------
void CHudLocator::FillRect(int x, int y, int w, int h)
{
	int panel_x, panel_y, panel_w, panel_h;
	GetBounds(panel_x, panel_y, panel_w, panel_h);
	vgui::surface()->DrawFilledRect(x, y, x + w, y + h);
}

float CHudLocator::LocatorXPositionForYawDiff(float yawDiff)
{
	float fov = hud_locator_fov.GetFloat() / 2;
	float remappedAngle = RemapVal(yawDiff, -fov, fov, -90, 90);
	float cosine = sin(DEG2RAD(remappedAngle));
	int element_wide = GetWide();

	float position = (element_wide >> 1) + ((element_wide >> 1) * cosine);

	return position;
}

//-----------------------------------------------------------------------------
// Draw the tickmarks on the locator
//-----------------------------------------------------------------------------
#ifdef DARKINTERVAL
#define NUM_GRADUATIONS	256.0f
#else
#define NUM_GRADUATIONS	16.0f
#endif
void CHudLocator::DrawGraduations(float flYawPlayerFacing)
{
	int xPos, yPos;
	float fov = hud_locator_fov.GetFloat() / 2;
#ifdef DARKINTERVAL
	int				r, g, b, a;
	Color clrGrad;
	(gHUD.m_clrNormal).GetColor(r, g, b, a);
	clrGrad.SetColor(r, g, b, 100);
	
	int element_tall = GetTall();		// Height of the VGUI element

	surface()->DrawSetColor(255, 255, 255, 255);

	// Tick Icons

	float angleStep = 360.0f / NUM_GRADUATIONS;
	bool tallLine = true;

	for (float angle = -180; angle <= 180; angle += angleStep)
	{
		yPos = (element_tall >> 1);
	//	yPos += (element_tall / 2);

		if (tallLine)
		{
			tallLine = false;
		}
		else
		{
			tallLine = true;
		}

		float flDiff = UTIL_AngleDiff(flYawPlayerFacing, angle);

		if (fabs(flDiff) > fov)
			continue;

		float xPosition = LocatorXPositionForYawDiff(flDiff);

		xPos = (int)xPosition;
		xPos -= (1);
		
		wchar_t text[1];
#ifdef DARKINTERVAL  // _snwprintf for char* should use %S, not %s format specifier.
		_snwprintf(text, sizeof(text), L"%S", tallLine ? "|" : ".");
#else
		_snwprintf(text, sizeof(text), L"%s", tallLine ? "|" : ".");		
#endif
		vgui::surface()->DrawSetTextFont(m_hTextFont);
		vgui::surface()->DrawSetTextColor(clrGrad);
		vgui::surface()->DrawSetTextPos(xPos, yPos);

		if( tallLine ) // skip small marks - design choice
			vgui::surface()->DrawUnicodeString(text);

	//	vgui::surface()->DrawTexturedRect(xPos, yPos, xPos + icon_wide, yPos + icon_tall);
	}
#else
	if (m_textureID_IconBigTick == -1)
	{
		m_textureID_IconBigTick = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(m_textureID_IconBigTick, LOCATOR_MATERIAL_BIG_TICK, true, false);
	}

	if (m_textureID_IconSmallTick == -1)
	{
		m_textureID_IconSmallTick = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(m_textureID_IconSmallTick, LOCATOR_MATERIAL_SMALL_TICK, true, false);
	}

	int element_tall = GetTall();		// Height of the VGUI element

	surface()->DrawSetColor(255, 255, 255, 255);

	// Tick Icons

	float angleStep = 360.0f / NUM_GRADUATIONS;
	bool tallLine = true;

	for (float angle = -180; angle <= 180; angle += angleStep)
	{
		yPos = (element_tall >> 1);

		if (tallLine)
		{
			vgui::surface()->DrawSetTexture(m_textureID_IconBigTick);
			vgui::surface()->DrawGetTextureSize(m_textureID_IconBigTick, icon_wide, icon_tall);
			tallLine = false;
		}
		else
		{
			vgui::surface()->DrawSetTexture(m_textureID_IconSmallTick);
			vgui::surface()->DrawGetTextureSize(m_textureID_IconSmallTick, icon_wide, icon_tall);
			tallLine = true;
		}

		float flDiff = UTIL_AngleDiff(flYawPlayerFacing, angle);

		if (fabs(flDiff) > fov)
			continue;

		float xPosition = LocatorXPositionForYawDiff(flDiff);

		xPos = (int)xPosition;
		xPos -= (icon_wide >> 1);

		vgui::surface()->DrawTexturedRect(xPos, yPos, xPos + icon_wide, yPos + icon_tall);
#endif
}
#ifdef DARKINTERVAL
ConVar di_locator_range_start("di_locator_range_start", "64"); // 60 feet
ConVar di_locator_range_end("di_locator_range_end", "16000"); // 90 feet
#endif
void CHudLocator::Paint()
{
#ifdef DARKINTERVAL
	if (m_textureID_IconPointOfInterest == -1)
	{
		m_textureID_IconPointOfInterest = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(m_textureID_IconPointOfInterest, LOCATOR_MATERIAL_POINTOFINTEREST, true, false);

		m_textureID_IconOutpost = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(m_textureID_IconOutpost, LOCATOR_MATERIAL_OUTPOST, true, false);

		m_textureID_IconMainObjective = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(m_textureID_IconMainObjective, LOCATOR_MATERIAL_MAINOBJECTIVE, true, false);

	//	m_textureID_IconBigEnemy = vgui::surface()->CreateNewTextureID();
	//	vgui::surface()->DrawSetTextureFile(m_textureID_IconBigEnemy, LOCATOR_MATERIAL_LARGE_ENEMY, true, false);

	//	m_textureID_IconRadiation = vgui::surface()->CreateNewTextureID();
	//	vgui::surface()->DrawSetTextureFile(m_textureID_IconRadiation, LOCATOR_MATERIAL_RADIATION, true, false);
	}
#else
	if (m_textureID_IconJalopy == -1)
	{
		m_textureID_IconJalopy = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(m_textureID_IconJalopy, LOCATOR_MATERIAL_JALOPY, true, false);
	}

	int alpha = hud_locator_alpha.GetInt();

	SetAlpha(alpha);
#endif
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;
#ifndef DARKINTERVAL
	if (pPlayer->m_HL2Local.m_vecLocatorOrigin == vec3_origin)
		return;
#endif
	float fov = hud_locator_fov.GetFloat() / 2.0f;
#ifdef DARKINTERVAL
/*
	if ( pPlayer->IsPlayingArcadeAsManhack())
	{
		SetPos(ScreenWidth() - (GetWide() / 2), ScreenHeight() / 6);
		SetBgColor(Color(255, 75, 25, 255));
		fov = (hud_locator_fov.GetFloat()) / 6.0f;
	}
	else
	{
		SetPos(ScreenWidth() - (ScreenWidth() / 2), (ScreenHeight() / 6) * 5);
		SetBgColor(Color(0,0,0,0));
		fov = (hud_locator_fov.GetFloat()) / 2.0f;
	}
*/
#endif
	int element_tall = GetTall();		// Height of the VGUI element

	// Compute the relative position of objects we're tracking
	// We'll need the player's yaw for comparison.
	float flYawPlayerForward = pPlayer->EyeAngles().y;
#ifdef DARKINTERVAL
	// Draw compass
	DrawGraduations(flYawPlayerForward);
	surface()->DrawSetColor(255, 255, 255, 255);

	// Draw icons, if any
	for (int i = 0; i < pPlayer->m_HL2Local.m_iNumLocatorContacts; i++)
	{
		// Copy this value out of the member variable in case we decide to expand this
		// feature later and want to iterate some kind of list.
		EHANDLE ent = pPlayer->m_HL2Local.m_locatorEnt[i];

		if (ent)
		{
			Vector vecLocation = ent->GetAbsOrigin();

			Vector vecToLocation = vecLocation - pPlayer->GetAbsOrigin();
			QAngle locationAngles;

			VectorAngles(vecToLocation, locationAngles);
			float yawDiff = UTIL_AngleDiff(flYawPlayerForward, locationAngles.y);
			bool bObjectInFOV = (yawDiff > -fov && yawDiff < fov);

			// Draw the icons!
			int icon_wide, icon_tall;
			int xPos, yPos;

			if (bObjectInFOV)
			{
				Vector vecPos = ent->GetAbsOrigin();
				float x_diff = vecPos.x - pPlayer->GetAbsOrigin().x;
				float y_diff = vecPos.y - pPlayer->GetAbsOrigin().y;
				float flDist = sqrt(((x_diff)*(x_diff)+(y_diff)*(y_diff)));

				if (flDist <= di_locator_range_end.GetFloat() 
					|| pPlayer->m_HL2Local.m_iLocatorContactType[i] == LOCATOR_CONTACT_MAIN_OBJECTIVE)
				{
					// The object's location maps to a valid position along the tape, so draw an icon.
					float tapePosition = LocatorXPositionForYawDiff(yawDiff);
					// Msg("tapePosition: %f\n", tapePosition);
					pPlayer->m_HL2Local.m_flTapePos[i] = tapePosition;

					// derive a scale for the locator icon
					yawDiff = fabs(yawDiff);
					float scale = 0.55f;
					float xscale = RemapValClamped(yawDiff, (fov / 4), fov, 1.0f, 0.25f);

					switch (pPlayer->m_HL2Local.m_iLocatorContactType[i])
					{
					case LOCATOR_CONTACT_POINT_OF_INTEREST:
						vgui::surface()->DrawSetTexture(m_textureID_IconPointOfInterest);
						vgui::surface()->DrawGetTextureSize(m_textureID_IconPointOfInterest, icon_wide, icon_tall);
						break;
					case LOCATOR_CONTACT_OUTPOST:
						vgui::surface()->DrawSetTexture(m_textureID_IconOutpost);
						vgui::surface()->DrawGetTextureSize(m_textureID_IconOutpost, icon_wide, icon_tall);
						break;
					case LOCATOR_CONTACT_MAIN_OBJECTIVE:
						vgui::surface()->DrawSetTexture(m_textureID_IconMainObjective);
						vgui::surface()->DrawGetTextureSize(m_textureID_IconMainObjective, icon_wide, icon_tall);
						break;
						/*
					case LOCATOR_CONTACT_LARGE_ENEMY:
						vgui::surface()->DrawSetTexture(m_textureID_IconBigEnemy);
						vgui::surface()->DrawGetTextureSize(m_textureID_IconBigEnemy, icon_wide, icon_tall);
						break;
					case LOCATOR_CONTACT_RADIATION:
						vgui::surface()->DrawSetTexture(m_textureID_IconRadiation);
						vgui::surface()->DrawGetTextureSize(m_textureID_IconRadiation, icon_wide, icon_tall);
						break;
						*/
					default:
						break;
					}

					float flIconWide = ((float)element_tall * 1.25f);
					float flIconTall = ((float)element_tall * 1.25f);

					// Scale the icon based on distance
					float flDistScale = 1.0f;
					if (flDist > di_locator_range_start.GetFloat())
					{
						flDistScale = RemapValClamped(flDist, di_locator_range_start.GetFloat(), di_locator_range_end.GetFloat(), 1.0f, 0.5f);
					}

					// Put back into ints
					icon_wide = (int)flIconWide * 1.25;
					icon_tall = (int)flIconTall * 1.25;

					// Positon Scale
					icon_wide *= xscale;

					// Distance Scale Icons
					icon_wide *= flDistScale;
					icon_tall *= flDistScale;

					// Global Scale Icons
					icon_wide *= scale;
					icon_tall *= scale;
					//Msg("yawDiff:%f  xPos:%d  scale:%f\n", yawDiff, xPos, scale );

					// Center the icon around its position.
					xPos = (int)tapePosition;
					xPos -= (icon_wide >> 1);
					yPos = (element_tall >> 1) - (icon_tall >> 1);

					// If this overlaps the last drawn items reduce opacity
					float fMostOverlapDist = 14.0f;
					if (pPlayer->m_HL2Local.m_iLocatorContactType[i] != LOCATOR_CONTACT_MAIN_OBJECTIVE)
					{
						for (int j = i - 1; j >= 0; j--)
						{
							EHANDLE lastEnt = pPlayer->m_HL2Local.m_locatorEnt[j];
							if (lastEnt.IsValid())
							{
								if (pPlayer->m_HL2Local.m_flTapePos[j] > 0)
								{
									float fDiff = abs(pPlayer->m_HL2Local.m_flTapePos[j] - tapePosition);
									if (fMostOverlapDist > fDiff)
										fMostOverlapDist = fDiff;
								}
							}
						}
					}

					// Msg("fMostOverlapDist: %f\n", fMostOverlapDist );
					if (fMostOverlapDist < 14.0f)
					{
						int numOpacity = (int)(32.0f + fMostOverlapDist * 9.6f);
						// Msg("numOpacity: %d\n", numOpacity );

						surface()->DrawSetColor(255, 255, 255, numOpacity);
					}
					else
					{
						surface()->DrawSetColor(255, 255, 255, 255);
					}

					//Msg("Drawing at %f %f\n", x, y );
					vgui::surface()->DrawTexturedRect(xPos, yPos - 7, xPos + icon_wide, yPos + icon_tall - 7);
				}
			}
			else
			{
				pPlayer->m_HL2Local.m_flTapePos[i] = -1.0f;
			}
		}
	}
	MaintainLocatorContacts();
#else
	// Copy this value out of the member variable in case we decide to expand this
	// feature later and want to iterate some kind of list. 
	Vector vecLocation = pPlayer->m_HL2Local.m_vecLocatorOrigin;

	Vector vecToLocation = vecLocation - pPlayer->GetAbsOrigin();
	QAngle locationAngles;

	VectorAngles(vecToLocation, locationAngles);
	float yawDiff = UTIL_AngleDiff(flYawPlayerForward, locationAngles.y);
	bool bObjectInFOV = (yawDiff > -fov && yawDiff < fov);

	// Draw the icons!
	int icon_wide, icon_tall;
	int xPos, yPos;
	surface()->DrawSetColor(255, 255, 255, 255);

	DrawGraduations(flYawPlayerForward);

	if (bObjectInFOV)
	{
		// The object's location maps to a valid position along the tape, so draw an icon.
		float tapePosition = LocatorXPositionForYawDiff(yawDiff);

		// derive a scale for the locator icon
		yawDiff = fabs(yawDiff);
		float scale = 1.0f;
		scale = RemapValClamped(yawDiff, (fov / 4), fov, 1.0f, 0.25f);

		vgui::surface()->DrawSetTexture(m_textureID_IconJalopy);
		vgui::surface()->DrawGetTextureSize(m_textureID_IconJalopy, icon_wide, icon_tall);

		float flIconWide = ((float)element_tall * 1.25f);
		float flIconTall = ((float)element_tall * 1.25f);

		// Scale the icon as desired...

		// Put back into ints
		icon_wide = (int)flIconWide;
		icon_tall = (int)flIconTall;

		icon_wide *= scale;

		//Msg("yawDiff:%f  xPos:%d  scale:%f\n", yawDiff, xPos, scale );

		// Center the icon around its position.
		xPos = (int)tapePosition;
		xPos -= (icon_wide >> 1);
		yPos = (element_tall >> 1) - (icon_tall >> 1);

		//Msg("Drawing at %f %f\n", x, y );
		vgui::surface()->DrawTexturedRect(xPos, yPos, xPos + icon_wide, yPos + icon_tall);
	}
	#endif
}
#ifdef DARKINTERVAL
//TE120--
//---------------------------------------------------------
// Purpose: Register a radar contact in the list of contacts
//---------------------------------------------------------
void CHudLocator::AddLocatorContact(EHANDLE hEnt, int iType)
{
	if (m_iNumlocatorContacts == LOCATOR_MAX_CONTACTS)
	{
		return;
	}

	if (!hEnt)
	{
		return;
	}

	int iExistingContact = FindLocatorContact(hEnt);
	if (iExistingContact == -1)
	{
		m_locatorContacts[m_iNumlocatorContacts].m_pEnt = hEnt;
		m_locatorContacts[m_iNumlocatorContacts].m_iType = iType;
		m_iNumlocatorContacts++;
	}
}

//---------------------------------------------------------
// Purpose: Search the contact list for a specific contact
//---------------------------------------------------------
int CHudLocator::FindLocatorContact(EHANDLE hEnt)
{
	for (int i = 0; i < m_iNumlocatorContacts; i++)
	{
		if (m_locatorContacts[i].m_pEnt == hEnt)
			return i;
	}

	return -1;
}

//---------------------------------------------------------
// Purpose: Go through all radar targets and see if any
//			have expired. If yes, remove them from the
//			list.
//---------------------------------------------------------
void CHudLocator::MaintainLocatorContacts()
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	for (int i = 0; i < m_iNumlocatorContacts; i++)
	{
		// If I don't exist... remove me
		if (m_locatorContacts[i].m_pEnt)
		{
			// If I'm too far, remove me
			Vector vecPos = m_locatorContacts[i].m_pEnt->GetAbsOrigin();

			float x_diff = vecPos.x - pPlayer->GetAbsOrigin().x;
			float y_diff = vecPos.y - pPlayer->GetAbsOrigin().y;
			float flDist = sqrt(((x_diff)*(x_diff)+(y_diff)*(y_diff)));

			if (flDist > di_locator_range_end.GetFloat()) // keep MO seen always
			{
				// Time for this guy to go. Easiest thing is just to copy the last element
				// into this element's spot and then decrement the count of entities.
				m_locatorContacts[i] = m_locatorContacts[m_iNumlocatorContacts - 1];
				m_iNumlocatorContacts--;
				break;
			}
		}
		else
		{
			// Time for this guy to go. Easiest thing is just to copy the last element
			// into this element's spot and then decrement the count of entities.
			m_locatorContacts[i] = m_locatorContacts[m_iNumlocatorContacts - 1];
			m_iNumlocatorContacts--;
			break;
		}
	}
}
#endif // DARKINTERVAL