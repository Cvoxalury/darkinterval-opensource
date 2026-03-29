#include "cbase.h"
#include "c_env_fog_controller.h"
#include "inewmenubase.h"
#include "inewmenuroot.h"
#include "iweathersettings.h"
#include "ioptionsmaindialogue.h"
#include "ienginevgui.h"
#include "vgui/IInput.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//==========================================================
// Singleton
static CWeatherSettingsDialog *g_pWSUI;
vgui::Panel *GetSDKRootPanel();

CWeatherSettingsDialog *GetWSUI()
{
	Assert(g_pWSUI);
	return g_pWSUI;
}

void WSUI_Create(vgui::VPANEL parent)
{
	Assert(!g_pWSUI);
	g_pWSUI = new CWeatherSettingsDialog(NULL);
}

void WSUI_Destroy()
{
	if (g_pWSUI)
	{
		g_pWSUI->MarkForDeletion();
		g_pWSUI = NULL;
	}
}
//--------------------------------------------------------------------------------------------
// Fog page
//--------------------------------------------------------------------------------------------
CWeatherSettings_Fog::CWeatherSettings_Fog(vgui::Panel *parent) : PropertyPage(parent, NULL)
{
	DrawSettingsItems();
	LoadControlSettingsAndUserConfig("resource/ui/di_dev_fog.res");
	SetBuildModeEditable(true);
}
void CWeatherSettings_Fog::DrawSettingsItems(void)
{
	m_pOverrideLevelFog = new CheckButton(this, "OverrideLevelFogButton", "#DI_WSettings_OverrideFog");
	m_pEnableMainFog = new CheckButton(this, "EnableMainFogButton", "#DI_WSettings_EnableMainFog");
	m_pEnableSkyboxFog = new CheckButton(this, "EnableSkyboxFogButton", "#DI_WSettings_EnableSkyboxFog");
	m_pFogMainDensitySlider = new IGenericCvarSlider(this, "MainFogDensitySlider", "Density", 0.0f, 1.0f, "fog_density");
	m_pFogMainStartSlider = new IGenericCvarSlider(this, "MainFogStartSlider", "Start", -1000, 5000, "fog_start");
	m_pFogMainStartSlider->AddActionSignalTarget(this);
	m_pFogMainStartSlider->SetDragOnRepositionNob(true);

	m_pFogMainEndSlider = new IGenericCvarSlider(this, "MainFogEndSlider", "End", 0, 50000, "fog_end");
	m_pFogMainEndSlider->AddActionSignalTarget(this);
	m_pFogMainEndSlider->SetDragOnRepositionNob(true);

	m_pFogSkyboxDensitySlider = new IGenericCvarSlider(this, "SkyFogDensitySlider", "Density (skybox)", 0.0f, 1.0f, "fog_skybox_density");
	m_pFogSkyboxStartSlider = new IGenericCvarSlider(this, "SkyFogStartSlider", "Start (skybox)", -10000, 50000, "fog_skybox_start");
	m_pFogSkyboxEndSlider = new IGenericCvarSlider(this, "SkyFogEndSlider", "End (skybox)", 0, 100000, "fog_skybox_end");

	m_pMainFogRedSlider = new Slider(this, "MainFogRedSlider");
	m_pMainFogRedSlider->SetRange(0, 255);
	m_pMainFogRedSlider->SetValue(17);

	m_pMainFogGreenSlider = new Slider(this, "MainFogGreenSlider");
	m_pMainFogGreenSlider->SetRange(0, 255);
	m_pMainFogGreenSlider->SetValue(17);

	m_pMainFogBlueSlider = new Slider(this, "MainFogBlueSlider");
	m_pMainFogBlueSlider->SetRange(0, 255);
	m_pMainFogBlueSlider->SetValue(17);

	m_pSkyFogRedSlider = new Slider(this, "SkyFogRedSlider");
	m_pSkyFogRedSlider->SetRange(0, 255);
	m_pSkyFogRedSlider->SetValue(17);

	m_pSkyFogGreenSlider = new Slider(this, "SkyFogGreenSlider");
	m_pSkyFogGreenSlider->SetRange(0, 255);
	m_pSkyFogGreenSlider->SetValue(17);

	m_pSkyFogBlueSlider = new Slider(this, "SkyFogBlueSlider");
	m_pSkyFogBlueSlider->SetRange(0, 255);
	m_pSkyFogBlueSlider->SetValue(17);

	// Text entry for Main Fog Start 
	char buf[16];

	m_pReadout_MainFogStart = new TextEntry(this, "MainFogStartReadout");
	m_pReadout_MainFogStart->SetEditable(false);
	m_pReadout_MainFogStart->SendNewLine(true);
	m_pReadout_MainFogStart->SetCatchEnterKey(true);
	m_pReadout_MainFogStart->AddActionSignalTarget(this);

	//char mfs[6];
	sprintf(buf, "%i", (int)m_pFogMainStartSlider->GetSliderValue());
	m_pReadout_MainFogStart->SetText(buf);

	// Text entry for Main Fog End

	m_pReadout_MainFogEnd = new TextEntry(this, "MainFogEndReadout");
	m_pReadout_MainFogEnd->SetEditable(false);
	m_pReadout_MainFogEnd->SendNewLine(true);
	m_pReadout_MainFogEnd->SetCatchEnterKey(true);
	m_pReadout_MainFogEnd->AddActionSignalTarget(this);

	//char mfe[6];
	sprintf(buf, "%i", (int)m_pFogMainEndSlider->GetSliderValue());
	m_pReadout_MainFogEnd->SetText(buf);

	// Text entry for Main Fog Density

	m_pReadout_MainFogDensity = new TextEntry(this, "MainFogDensityReadout");
	m_pReadout_MainFogDensity->SetEditable(false);
	m_pReadout_MainFogDensity->SendNewLine(true);
	m_pReadout_MainFogDensity->SetCatchEnterKey(true);
	m_pReadout_MainFogDensity->AddActionSignalTarget(this);

	//char mfd[6];
	sprintf(buf, "%.2f", m_pFogMainDensitySlider->GetSliderValue());
	m_pReadout_MainFogDensity->SetText(buf);

	// Text entry for Skybox Fog Start

	m_pReadout_SkyFogStart = new TextEntry(this, "SkyFogStartReadout");
	m_pReadout_SkyFogStart->SetEditable(false);
	m_pReadout_SkyFogStart->SendNewLine(true);
	m_pReadout_SkyFogStart->SetCatchEnterKey(true);
	m_pReadout_SkyFogStart->AddActionSignalTarget(this);

	//char sfs[6];
	sprintf(buf, "%i", (int)m_pFogSkyboxStartSlider->GetSliderValue());
	m_pReadout_SkyFogStart->SetText(buf);

	// Text entry for Skybox Fog End

	m_pReadout_SkyFogEnd = new TextEntry(this, "SkyFogEndReadout");
	m_pReadout_SkyFogEnd->SetEditable(false);
	m_pReadout_SkyFogEnd->SendNewLine(true);
	m_pReadout_SkyFogEnd->SetCatchEnterKey(true);
	m_pReadout_SkyFogEnd->AddActionSignalTarget(this);

	//char sfe[6];
	sprintf(buf, "%i", (int)m_pFogSkyboxEndSlider->GetSliderValue());
	m_pReadout_SkyFogEnd->SetText(buf);

	// Text entry for Skybox Fog Density

	m_pReadout_SkyFogDensity = new TextEntry(this, "SkyFogDensityReadout");
	m_pReadout_SkyFogDensity->SetEditable(false);
	m_pReadout_SkyFogDensity->SendNewLine(true);
	m_pReadout_SkyFogDensity->SetCatchEnterKey(true);
	m_pReadout_SkyFogDensity->AddActionSignalTarget(this);

	//char sfd[6];
	sprintf(buf, "%.2f", m_pFogSkyboxDensitySlider->GetSliderValue());
	m_pReadout_SkyFogDensity->SetText(buf);

	// Text entry for Colour Readout, main area

	m_pReadout_MainFogColor = new TextEntry(this, "MainColorReadout");
	m_pReadout_MainFogColor->SetEditable(false);
	m_pReadout_MainFogColor->SendNewLine(true);
	m_pReadout_MainFogColor->SetCatchEnterKey(true);
	m_pReadout_MainFogColor->AddActionSignalTarget(this);

	//char mfc[11];
	sprintf(buf, "%i %i %i",
		m_pMainFogRedSlider->GetValue(),
		m_pMainFogGreenSlider->GetValue(),
		m_pMainFogBlueSlider->GetValue());
	m_pReadout_MainFogColor->SetText(buf);

	// Text entry for Colour Readout, skybox

	m_pReadout_SkyFogColor = new TextEntry(this, "SkyboxColorReadout");
	m_pReadout_SkyFogColor->SetEditable(true);
	m_pReadout_SkyFogColor->SendNewLine(true);
	m_pReadout_SkyFogColor->SetCatchEnterKey(true);
	m_pReadout_SkyFogColor->AddActionSignalTarget(this);

	//char mfc[11];
	sprintf(buf, "%i %i %i",
		m_pSkyFogRedSlider->GetValue(),
		m_pSkyFogGreenSlider->GetValue(),
		m_pSkyFogBlueSlider->GetValue());
	m_pReadout_SkyFogColor->SetText(buf);

	ConVarRef fogoverride("fog_override");
	ConVarRef enable("fog_enable");
	ConVarRef enableskybox("fog_skybox_enable");

	m_pOverrideLevelFog->SetSelected(fogoverride.GetBool());
	m_pEnableMainFog->SetSelected(enable.GetBool());
	m_pEnableSkyboxFog->SetSelected(enableskybox.GetBool());
}

void CWeatherSettings_Fog::OnSliderMoved()
{
	char buf[16];
	sprintf(buf, "%i", (int)m_pFogMainStartSlider->GetSliderValue());
	m_pReadout_MainFogStart->SetText(buf);

	sprintf(buf, "%i", (int)m_pFogMainEndSlider->GetSliderValue());
	m_pReadout_MainFogEnd->SetText(buf);

	sprintf(buf, "%.2f", m_pFogMainDensitySlider->GetSliderValue());
	m_pReadout_MainFogDensity->SetText(buf);

	sprintf(buf, "%i", (int)m_pFogSkyboxStartSlider->GetSliderValue());
	m_pReadout_SkyFogStart->SetText(buf);

	sprintf(buf, "%i", (int)m_pFogSkyboxEndSlider->GetSliderValue());
	m_pReadout_SkyFogEnd->SetText(buf);

	sprintf(buf, "%.2f", m_pFogSkyboxDensitySlider->GetSliderValue());
	m_pReadout_SkyFogDensity->SetText(buf);

	sprintf(buf, "%i %i %i",
		m_pMainFogRedSlider->GetValue(),
		m_pMainFogGreenSlider->GetValue(),
		m_pMainFogBlueSlider->GetValue());
	m_pReadout_MainFogColor->SetText(buf);

	sprintf(buf, "%i %i %i",
		m_pSkyFogRedSlider->GetValue(),
		m_pSkyFogGreenSlider->GetValue(),
		m_pSkyFogBlueSlider->GetValue());
	m_pReadout_SkyFogColor->SetText(buf);

	OnDataChanged();
}

/*
void CWeatherSettings_Fog::OnTextChanged()
{
char buf[6];
int iValue;
float fValue;

m_pReadout_MainFogStart->GetText(buf, 6);
sscanf(buf, "%i", &iValue);
m_pFogMainStartSlider->SetSliderValue(iValue);

m_pReadout_MainFogEnd->GetText(buf, 6);
sscanf(buf, "%i", &iValue);
m_pFogMainEndSlider->SetSliderValue(iValue);

m_pReadout_MainFogDensity->GetText(buf, 6);
sscanf(buf, "%.2f", &fValue);
m_pFogMainDensitySlider->SetSliderValue(fValue);

m_pReadout_SkyFogStart->GetText(buf, 6);
sscanf(buf, "%i", &iValue);
m_pFogSkyboxStartSlider->SetSliderValue(iValue);

m_pReadout_SkyFogEnd->GetText(buf, 6);
sscanf(buf, "%i", &iValue);
m_pFogSkyboxEndSlider->SetSliderValue(iValue);

m_pReadout_SkyFogDensity->GetText(buf, 6);
sscanf(buf, "%.2f", &fValue);
m_pFogSkyboxDensitySlider->SetSliderValue(fValue);
}
*/
void CWeatherSettings_Fog::OnTextNewLine(KeyValues *data)
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>(data->GetPtr("panel"));
	if (!pPanel)
	{
		return;
	}

	vgui::TextEntry *pTextEntry = dynamic_cast<vgui::TextEntry *>(pPanel);
	if (!pTextEntry)
	{
		return;
	}

	//char buf[6];
	//int iValue;
	/*
	// Main area
	if (pTextEntry == m_pReadout_MainFogStart)
	{
	m_pReadout_MainFogStart->GetText(buf, 6);
	sscanf(buf, "%i", &iValue);
	m_pFogMainStartSlider->SetValue(iValue);
	OnDataChanged();
	return;
	}
	if (pTextEntry == m_pReadout_MainFogEnd)
	{
	m_pReadout_MainFogEnd->GetText(buf, 6);
	sscanf(buf, "%i", &iValue);
	ApplyChangesToConVar("fog_end", iValue);
	//m_pFogMainEndSlider->SetValue(m_pReadout_MainFogEnd->GetValueAsInt());
	OnDataChanged();
	return;
	}

	// Skybox
	if (pTextEntry == m_pReadout_SkyFogStart)
	{
	m_pReadout_SkyFogStart->GetText(buf, 6);
	sscanf(buf, "%i", &iValue);
	ApplyChangesToConVar("fog_skybox_start", iValue);
	//m_pFogSkyboxStartSlider->SetValue(m_pReadout_SkyFogStart->GetValueAsInt());
	OnDataChanged();
	return;
	}
	if (pTextEntry == m_pReadout_SkyFogEnd)
	{
	m_pReadout_SkyFogEnd->GetText(buf, 6);
	sscanf(buf, "%i", &iValue);
	ApplyChangesToConVar("fog_skybox_end", iValue);
	//m_pFogSkyboxEndSlider->SetValue(m_pReadout_SkyFogEnd->GetValueAsInt());
	OnDataChanged();
	return;
	}
	*/
}

void CWeatherSettings_Fog::OnTextKillFocus(KeyValues *data)
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>(data->GetPtr("panel"));
	if (!pPanel)
	{
		return;
	}

	vgui::TextEntry *pTextEntry = dynamic_cast<vgui::TextEntry *>(pPanel);
	if (!pTextEntry)
	{
		return;
	}

	/*
	// Main area
	if (pTextEntry == m_pReadout_MainFogStart)
	{
	m_pFogMainStartSlider->SetValue(m_pReadout_MainFogStart->GetValueAsInt());
	return;
	}
	if (pTextEntry == m_pReadout_MainFogEnd)
	{
	m_pFogMainEndSlider->SetValue(m_pReadout_MainFogEnd->GetValueAsInt());
	return;
	}

	// Skybox
	if (pTextEntry == m_pReadout_SkyFogStart)
	{
	m_pFogSkyboxStartSlider->SetValue(m_pReadout_SkyFogStart->GetValueAsInt());
	return;
	}
	if (pTextEntry == m_pReadout_SkyFogEnd)
	{
	m_pFogSkyboxEndSlider->SetValue(m_pReadout_SkyFogEnd->GetValueAsInt());
	return;
	}
	*/
}

void CWeatherSettings_Fog::OnDataChanged()
{
	ConVarRef fogoverride("fog_override");
	ConVarRef enable("fog_enable");
	ConVarRef enableskybox("fog_skybox_enable");
	ConVarRef start("fog_start");
	ConVarRef startskybox("fog_skybox_start");
	ConVarRef end("fog_end");
	ConVarRef endskybox("fog_skybox_end");
	ConVarRef density("fog_density");
	ConVarRef densityskybox("fog_skybox_density");

	fogoverride.SetValue(m_pOverrideLevelFog->IsSelected());
	enable.SetValue(m_pEnableMainFog->IsSelected());
	enableskybox.SetValue(m_pEnableSkyboxFog->IsSelected());
	start.SetValue(m_pFogMainStartSlider->GetSliderValue());
	startskybox.SetValue(m_pFogSkyboxStartSlider->GetSliderValue());
	end.SetValue(m_pFogMainEndSlider->GetSliderValue());
	endskybox.SetValue(m_pFogSkyboxEndSlider->GetSliderValue());
	density.SetValue(m_pFogMainDensitySlider->GetSliderValue());
	densityskybox.SetValue(m_pFogSkyboxDensitySlider->GetSliderValue());

	char szCmd[32];
	Q_snprintf(szCmd, sizeof(szCmd), "fog_color %i %i %i\n",
		m_pMainFogRedSlider->GetValue(),
		m_pMainFogGreenSlider->GetValue(),
		m_pMainFogBlueSlider->GetValue());

	engine->ClientCmd_Unrestricted(szCmd);

	Q_snprintf(szCmd, sizeof(szCmd), "fog_skybox_color %i %i %i\n",
		m_pSkyFogRedSlider->GetValue(),
		m_pSkyFogGreenSlider->GetValue(),
		m_pSkyFogBlueSlider->GetValue());

	engine->ClientCmd_Unrestricted(szCmd);
}

void CWeatherSettings_Fog::ApplyChangesToConVar(const char *pConVarName, int value)
{
	Assert(cvar->FindVar(pConVarName));
	char szCmd[32];
	Q_snprintf(szCmd, sizeof(szCmd), "%s %g\n", pConVarName, value);
	engine->ClientCmd_Unrestricted(szCmd);
}

void CWeatherSettings_Fog::OnCommand(const char *command)
{
	if (!Q_strcmp(command, "btn_fog_override"))
	{
		ApplyChangesToConVar("fog_override", m_pOverrideLevelFog->IsSelected());
	}
	else if (!Q_strcmp(command, "btn_fog_enable"))
	{
		ApplyChangesToConVar("fog_enable", m_pEnableMainFog->IsSelected());
	}
	else if (!Q_strcmp(command, "btn_fog_enableskybox"))
	{
		ApplyChangesToConVar("fog_skybox_enable", m_pEnableSkyboxFog->IsSelected());
	}

	BaseClass::OnCommand(command);
}

//--------------------------------------------------------------------------------------------
// HDR page
//--------------------------------------------------------------------------------------------
CWeatherSettings_HDR::CWeatherSettings_HDR(vgui::Panel *parent) : PropertyPage(parent, NULL)
{
	DrawSettingsItems();
	LoadControlSettingsAndUserConfig("resource/ui/di_dev_hdr.res");
	SetBuildModeEditable(true);
}
void CWeatherSettings_HDR::DrawSettingsItems(void)
{
	m_pOverrideHDR = new CheckButton(this, "OverrideLevelFogButton", "");
	m_pHDRMinSlider = new IGenericCvarSlider(this, "HDRMinSlider", "Min Autoexposure", 0.0f, 10.0f, "mat_autoexposure_min");
	m_pHDRMaxSlider = new IGenericCvarSlider(this, "HDRMaxSlider", "Max Autoexposure", 0.0f, 20.0f, "mat_autoexposure_max");
	m_pBloomSlider = new IGenericCvarSlider(this, "BloomSlider", "Bloom Strength", 0.0f, 10.0f, "mat_bloomscale");

	// Text entries
	char buf[16];

	m_pReadout_HDRMin = new TextEntry(this, "HDRMinReadout");
	m_pReadout_HDRMin->SetEditable(false);
	m_pReadout_HDRMin->SendNewLine(true);
	m_pReadout_HDRMin->SetCatchEnterKey(true);
	m_pReadout_HDRMin->AddActionSignalTarget(this);

	sprintf(buf, "%.1f", m_pHDRMinSlider->GetSliderValue());
	m_pReadout_HDRMin->SetText(buf);

	m_pReadout_HDRMax = new TextEntry(this, "HDRMaxReadout");
	m_pReadout_HDRMax->SetEditable(false);
	m_pReadout_HDRMax->SendNewLine(true);
	m_pReadout_HDRMax->SetCatchEnterKey(true);
	m_pReadout_HDRMax->AddActionSignalTarget(this);

	sprintf(buf, "%.1f", m_pHDRMaxSlider->GetSliderValue());
	m_pReadout_HDRMax->SetText(buf);

	m_pReadout_Bloom = new TextEntry(this, "BloomReadout");
	m_pReadout_Bloom->SetEditable(false);
	m_pReadout_Bloom->SendNewLine(true);
	m_pReadout_Bloom->SetCatchEnterKey(true);
	m_pReadout_Bloom->AddActionSignalTarget(this);

	sprintf(buf, "%.1f", m_pBloomSlider->GetSliderValue());
	m_pReadout_Bloom->SetText(buf);

	ConVarRef hdroverride("mat_hdr_override");

	m_pOverrideHDR->SetSelected(false);
}

void CWeatherSettings_HDR::OnSliderMoved()
{
	char buf[16];
	sprintf(buf, "%g", m_pHDRMinSlider->GetSliderValue());
	m_pReadout_HDRMin->SetText(buf);

	sprintf(buf, "%g", m_pHDRMaxSlider->GetSliderValue());
	m_pReadout_HDRMax->SetText(buf);

	sprintf(buf, "%g", m_pBloomSlider->GetSliderValue());
	m_pReadout_Bloom->SetText(buf);

	OnDataChanged();
}

void CWeatherSettings_HDR::OnTextChanged()
{
	char buf[16];
	float value;

	m_pReadout_HDRMin->GetText(buf, 16);
	sscanf(buf, "%f", &value);
	m_pHDRMinSlider->SetSliderValue(value);

	m_pReadout_HDRMax->GetText(buf, 16);
	sscanf(buf, "%f", &value);
	m_pHDRMaxSlider->SetSliderValue(value);

	m_pReadout_Bloom->GetText(buf, 16);
	sscanf(buf, "%f", &value);
	m_pBloomSlider->SetSliderValue(value);
}

void CWeatherSettings_HDR::OnDataChanged()
{
	char szCmd[64];
	Q_snprintf(szCmd, sizeof(szCmd),
		"ent_fire env_tonemap_controller SetAutoExposureMin %.2g\n", m_pHDRMinSlider->GetSliderValue());
	engine->ClientCmd_Unrestricted(szCmd);

	Q_snprintf(szCmd, sizeof(szCmd),
		"ent_fire env_tonemap_controller SetAutoExposureMax %.2g\n", m_pHDRMaxSlider->GetSliderValue());
	engine->ClientCmd_Unrestricted(szCmd);

	Q_snprintf(szCmd, sizeof(szCmd),
		"ent_fire env_tonemap_controller SetBloomScale %.2f\n", m_pBloomSlider->GetSliderValue());
	engine->ClientCmd_Unrestricted(szCmd);
}

void CWeatherSettings_HDR::OnCommand(const char *command)
{
	BaseClass::OnCommand(command);
}


//--------------------------------------------------------------------------------------------
// DoF page
//--------------------------------------------------------------------------------------------
CWeatherSettings_DoF::CWeatherSettings_DoF(vgui::Panel *parent) : PropertyPage(parent, NULL)
{
	DrawSettingsItems();
	LoadControlSettingsAndUserConfig("resource/ui/di_dev_dof.res");
	SetBuildModeEditable(true);
}
void CWeatherSettings_DoF::DrawSettingsItems(void)
{
	m_pDoFFarFocusDepthSlider = new IGenericCvarSlider(this, "FarFocusDepthSlider", "Far Focus Depth", 0.0f, 30000.0f, "mat_dof_far_focus_depth");
	m_pDoFFarBlurDepthSlider = new IGenericCvarSlider(this, "FarBlurDepthSlider", "Far Blur Depth", 0.0f, 60000.0f, "mat_dof_far_blur_depth");
	m_pDoFFarBlurRadiusSlider = new IGenericCvarSlider(this, "FarBlurRadiusSlider", "Far Blur Radius", 0.0f, 20.0f, "mat_dof_far_blur_radius");

	m_pDoFNearFocusDepthSlider = new IGenericCvarSlider(this, "NearFocusDepthSlider", "Near Focus Depth", 0.0f, 50000.0f, "mat_dof_near_focus_depth");
	m_pDoFNearBlurDepthSlider = new IGenericCvarSlider(this, "NearBlurDepthSlider", "Near Blur Depth", 0.0f, 60000.0f, "mat_dof_near_blur_depth");
	m_pDoFNearBlurRadiusSlider = new IGenericCvarSlider(this, "NearBlurRadiusSlider", "Near Blur Radius", 0.0f, 50.0f, "mat_dof_near_blur_radius");

	// Text entries
	char buf[16];
	//
	m_pReadout_DoFFarFocusDepth = new TextEntry(this, "DoFFarFocusDepthReadout");
	m_pReadout_DoFFarFocusDepth->SetEditable(false);
	m_pReadout_DoFFarFocusDepth->SendNewLine(true);
	m_pReadout_DoFFarFocusDepth->SetCatchEnterKey(true);
	m_pReadout_DoFFarFocusDepth->AddActionSignalTarget(this);

	//char ffd[16];
	sprintf(buf, "%f", m_pDoFFarFocusDepthSlider->GetSliderValue());
	m_pReadout_DoFFarFocusDepth->SetText(buf);
	//
	m_pReadout_DoFFarBlurDepth = new TextEntry(this, "DoFFarBlurDepthReadout");
	m_pReadout_DoFFarBlurDepth->SetEditable(false);
	m_pReadout_DoFFarBlurDepth->SendNewLine(true);
	m_pReadout_DoFFarBlurDepth->SetCatchEnterKey(true);
	m_pReadout_DoFFarBlurDepth->AddActionSignalTarget(this);

	//char fbd[16];
	sprintf(buf, "%f", m_pDoFFarBlurDepthSlider->GetSliderValue());
	m_pReadout_DoFFarBlurDepth->SetText(buf);
	//
	m_pReadout_DoFFarBlurRadius = new TextEntry(this, "DoFFarBlurRadiusReadout");
	m_pReadout_DoFFarBlurRadius->SetEditable(false);
	m_pReadout_DoFFarBlurRadius->SendNewLine(true);
	m_pReadout_DoFFarBlurRadius->SetCatchEnterKey(true);
	m_pReadout_DoFFarBlurRadius->AddActionSignalTarget(this);

	//char fbr[16];
	sprintf(buf, "%f", m_pDoFFarBlurRadiusSlider->GetSliderValue());
	m_pReadout_DoFFarBlurDepth->SetText(buf);
	//
	m_pReadout_DoFNearFocusDepth = new TextEntry(this, "DoFNearFocusDepthReadout");
	m_pReadout_DoFNearFocusDepth->SetEditable(false);
	m_pReadout_DoFNearFocusDepth->SendNewLine(true);
	m_pReadout_DoFNearFocusDepth->SetCatchEnterKey(true);
	m_pReadout_DoFNearFocusDepth->AddActionSignalTarget(this);

	//char nfd[16];
	sprintf(buf, "%f", m_pDoFNearFocusDepthSlider->GetSliderValue());
	m_pReadout_DoFNearFocusDepth->SetText(buf);
	//
	m_pReadout_DoFNearBlurDepth = new TextEntry(this, "DoFNearBlurDepthReadout");
	m_pReadout_DoFNearBlurDepth->SetEditable(false);
	m_pReadout_DoFNearBlurDepth->SendNewLine(true);
	m_pReadout_DoFNearBlurDepth->SetCatchEnterKey(true);
	m_pReadout_DoFNearBlurDepth->AddActionSignalTarget(this);

	//char nbd[16];
	sprintf(buf, "%f", m_pDoFNearBlurDepthSlider->GetSliderValue());
	m_pReadout_DoFNearBlurDepth->SetText(buf);
	//
	m_pReadout_DoFNearBlurRadius = new TextEntry(this, "DoFNearBlurRadiusReadout");
	m_pReadout_DoFNearBlurRadius->SetEditable(false);
	m_pReadout_DoFNearBlurRadius->SendNewLine(true);
	m_pReadout_DoFNearBlurRadius->SetCatchEnterKey(true);
	m_pReadout_DoFNearBlurRadius->AddActionSignalTarget(this);

	//char nbr[16];
	sprintf(buf, "%f", m_pDoFNearBlurRadiusSlider->GetSliderValue());
	m_pReadout_DoFNearBlurDepth->SetText(buf);

}

void CWeatherSettings_DoF::OnSliderMoved()
{
	char buf[16];
	sprintf(buf, "%g", m_pDoFFarFocusDepthSlider->GetSliderValue());
	m_pReadout_DoFFarFocusDepth->SetText(buf);

	sprintf(buf, "%g", m_pDoFFarBlurDepthSlider->GetSliderValue());
	m_pReadout_DoFFarBlurDepth->SetText(buf);

	sprintf(buf, "%g", m_pDoFFarBlurRadiusSlider->GetSliderValue());
	m_pReadout_DoFFarBlurRadius->SetText(buf);

	sprintf(buf, "%g", m_pDoFNearFocusDepthSlider->GetSliderValue());
	m_pReadout_DoFNearFocusDepth->SetText(buf);

	sprintf(buf, "%g", m_pDoFNearBlurDepthSlider->GetSliderValue());
	m_pReadout_DoFNearBlurDepth->SetText(buf);

	sprintf(buf, "%g", m_pDoFNearBlurRadiusSlider->GetSliderValue());
	m_pReadout_DoFNearBlurRadius->SetText(buf);

	OnDataChanged();
}

void CWeatherSettings_DoF::OnTextChanged()
{
	char buf[16];
	float value;

	m_pReadout_DoFFarFocusDepth->GetText(buf, 16);
	sscanf(buf, "%f", &value);
	m_pDoFFarFocusDepthSlider->SetSliderValue(value);

	m_pReadout_DoFFarBlurDepth->GetText(buf, 16);
	sscanf(buf, "%f", &value);
	m_pDoFFarBlurDepthSlider->SetSliderValue(value);

	m_pReadout_DoFFarBlurRadius->GetText(buf, 16);
	sscanf(buf, "%f", &value);
	m_pDoFFarBlurRadiusSlider->SetSliderValue(value);

	m_pReadout_DoFNearFocusDepth->GetText(buf, 16);
	sscanf(buf, "%f", &value);
	m_pDoFNearFocusDepthSlider->SetSliderValue(value);

	m_pReadout_DoFNearBlurDepth->GetText(buf, 16);
	sscanf(buf, "%f", &value);
	m_pDoFNearBlurDepthSlider->SetSliderValue(value);

	m_pReadout_DoFNearBlurRadius->GetText(buf, 16);
	sscanf(buf, "%f", &value);
	m_pDoFNearBlurRadiusSlider->SetSliderValue(value);

}

void CWeatherSettings_DoF::OnDataChanged()
{
	char szCmd1[64];
	Q_snprintf(szCmd1, sizeof(szCmd1),
		"ent_fire env_dof_controller SetFarFocusDepth %.2g\n", m_pDoFFarFocusDepthSlider->GetSliderValue());
	engine->ClientCmd_Unrestricted(szCmd1);

	char szCmd2[64];
	Q_snprintf(szCmd2, sizeof(szCmd2),
		"ent_fire env_dof_controller SetFarBlurDepth %.2g\n", m_pDoFFarBlurDepthSlider->GetSliderValue());
	engine->ClientCmd_Unrestricted(szCmd2);

	char szCmd3[64];
	Q_snprintf(szCmd3, sizeof(szCmd3),
		"ent_fire env_dof_controller SetFarBlurRadius %.2f\n", m_pDoFFarBlurRadiusSlider->GetSliderValue());
	engine->ClientCmd_Unrestricted(szCmd3);

	char szCmd4[64];
	Q_snprintf(szCmd4, sizeof(szCmd4),
		"ent_fire env_dof_controller SetNearFocusDepth %.2g\n", m_pDoFNearFocusDepthSlider->GetSliderValue());
	engine->ClientCmd_Unrestricted(szCmd4);

	char szCmd5[64];
	Q_snprintf(szCmd5, sizeof(szCmd5),
		"ent_fire env_dof_controller SetNearBlurDepth %.2g\n", m_pDoFNearBlurDepthSlider->GetSliderValue());
	engine->ClientCmd_Unrestricted(szCmd5);

	char szCmd6[64];
	Q_snprintf(szCmd6, sizeof(szCmd6),
		"ent_fire env_dof_controller SetNearBlurRadius %.2f\n", m_pDoFNearBlurRadiusSlider->GetSliderValue());
	engine->ClientCmd_Unrestricted(szCmd6);
}

void CWeatherSettings_DoF::OnCommand(const char *command)
{
	BaseClass::OnCommand(command);
}

//--------------------------------------------------------------------------------------------
// Main Interface
//--------------------------------------------------------------------------------------------

CWeatherSettingsDialog::CWeatherSettingsDialog(vgui::Panel *parent) : PropertyDialog(parent, "WSUIDialogue")
{
	SetDeleteSelfOnClose(false);
	int horCenter = ScreenWidth() / 2; // -300;
	int verCenter = ScreenHeight() / 4 - 300;
	SetBounds(horCenter + 250, verCenter + 200, 500, 700);
	SetSizeable(true);
	SetRoundedCorners(0);

	SetTitle("#DI_WSettings_Title", true);

	m_pFogSettingsPanel = new CWeatherSettings_Fog(this);
	AddPage(m_pFogSettingsPanel, "#DI_WSettings_Fog");

	m_pHDRSettingsPanel = new CWeatherSettings_HDR(this);
	AddPage(m_pHDRSettingsPanel, "#DI_WSettings_HDR");

	m_pDoFSettingsPanel = new CWeatherSettings_DoF(this);
	AddPage(m_pDoFSettingsPanel, "#DI_WSettings_DoF");

	SetOKButtonVisible(false);
	SetCancelButtonVisible(false);
	SetApplyButtonVisible(false);
	GetPropertySheet()->SetTabWidth(84);

	//	Activate();
	ivgui()->AddTickSignal(GetVPanel(), 200);
	SetVisible(false);
	SetAlpha(50);

}

CWeatherSettingsDialog::~CWeatherSettingsDialog() {};

void CWeatherSettingsDialog::Run()
{
	SetTitle("Weather UI", true);
	Activate();
}

void CWeatherSettingsDialog::Activate()
{
	BaseClass::Activate();
}

void CWeatherSettingsDialog::OnTick(void)
{
	//	if (input()->IsKeyDown(KEY_LCONTROL) && input()->IsKeyDown(KEY_LSHIFT) && input()->IsKeyDown(KEY_L))
	//		SetVisible(!IsVisible());
	BaseClass::OnTick();
}

void CWeatherSettingsDialog::OnGameUIHidden()
{
	// tell our children about it
	for (int i = 0; i < GetChildCount(); i++)
	{
		Panel *pChild = GetChild(i);
		if (pChild)
		{
			PostMessage(pChild, new KeyValues("GameUIHidden"));
		}
	}
}

void WSUI_f()
{
	CWeatherSettingsDialog *wsui = GetWSUI();

	if (wsui)
	{
		if (wsui->IsVisible())
		{
			wsui->Close();
		}
		else
		{
			wsui->Activate();
		}
	}
	else
	{
		Msg("wsui not found\n");
		return;
	}
}

static ConCommand fogui_new("fogui_new", WSUI_f, "Show/hide weather control UI.", FCVAR_DONTRECORD);