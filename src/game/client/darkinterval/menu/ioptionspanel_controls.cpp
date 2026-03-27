#include "cbase.h"
#include "ioptionspanel_controls.h"
#include <vgui/Ivgui.h>
#include <vgui_controls/QueryBox.h>
#include <vgui_controls/button.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Divider.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Tooltip.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/URLLabel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Slider.h>
#include <vgui/IInput.h>
#include "vgui/ISurface.h"
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"
#include "igenericcvarslider.h"
#include "igameuifuncs.h"
#include "materialsystem\materialsystem_config.h"
#include "filesystem.h"
#include "estranged_system_caps.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
using namespace vgui;

COptionsPanel_Controls::COptionsPanel_Controls(vgui::Panel *parent) : PropertyPage(parent, NULL) //(VPANEL parent) : BaseClass(NULL, "DI_Options_Video")
{
	memset(m_Bindings, 0, sizeof(m_Bindings));
	
	DrawOptionsItems();
	m_pSetBindingButton = new Button(this, "ChangeKeyButton", "");
	m_pClearBindingButton = new Button(this, "ClearKeyButton", "");

	// Store all current key bindings
	SaveCurrentBindings();
	// Parse default descriptions
	ParseActionDescriptions();

	m_pSetBindingButton->SetEnabled(false);
	m_pClearBindingButton->SetEnabled(false);

	LoadControlSettingsAndUserConfig("resource/ui/di_options_controls.res");

	if (ScreenHeight() < 600)
		LoadControlSettingsAndUserConfig("resource/ui/di_options_controls_640.res");

	SetBuildModeEditable(true);
}

void COptionsPanel_Controls::DrawOptionsItems()
{
	m_pMouseSensitivity = new IGenericCvarSlider(this, "MouseSensitivity", "", 1.0, 20.0, "sensitivity" );
	
	m_pSensitivityText = new Label(this, "SensitivityText", "");

	m_pMouseSmoothingButton = new CheckButton(this, "MouseSmoothingButton", "#DI_Options_MouseSmoothing");

	m_pMouseInverseXButton = new CheckButton(this, "MouseInverseButtonX", "#DI_Options_MouseInvertedX");

	m_pMouseInverseYButton = new CheckButton(this, "MouseInverseButtonY", "#DI_Options_MouseInvertedY");

	m_pMouseRawButton = new CheckButton(this, "MouseRawButton", "#DI_Options_MouseRawInput");

	m_pConsoleEnabledButton = new CheckButton(this, "ConsoleEnabledButton", "#DI_Options_EnableConsole");

	m_pWeaponFastSwitchButton = new CheckButton(this, "WeaponFastSwitchButton", "#DI_Options_WeaponFastSwitch");

	m_pKeybordBingingsList = new COptionsPanel_Controls_BindingsList(this, "listpanel_keybindlist");
}

void COptionsPanel_Controls::PerformLayout()
{
	BaseClass::PerformLayout();
}

void COptionsPanel_Controls::OnControlModified()
{
	char szCmd[256];
	Q_snprintf(szCmd, sizeof(szCmd), "sensitivity %.2f\n", m_pMouseSensitivity->GetSliderValue());
	engine->ClientCmd_Unrestricted(szCmd);

	char buf[64];
	Q_snprintf(buf, sizeof(buf), " %.1f", m_pMouseSensitivity->GetSliderValue());
	m_pSensitivityText->SetText(buf);

	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

void COptionsPanel_Controls::OnApplyChanges()
{
	SaveControlsSettings();
}

void COptionsPanel_Controls::ApplyChangesToConVar(const char *pConVarName, int value)
{
	Assert(cvar->FindVar(pConVarName));
	char szCmd[256];
	Q_snprintf(szCmd, sizeof(szCmd), "%s %d\n", pConVarName, value);
	engine->ClientCmd_Unrestricted(szCmd);
}

void COptionsPanel_Controls::LoadControlsSettings()
{
	ConVarRef m_pitch("m_pitch");
	m_pMouseInverseXButton->SetSelected(m_pitch.GetFloat() < 0);
	ConVarRef m_yaw("m_yaw");
	m_pMouseInverseYButton->SetSelected(m_yaw.GetFloat() < 0);
	ConVarRef m_filter("m_filter");
	m_pMouseSmoothingButton->SetSelected(m_filter.GetInt());
	ConVarRef m_rawinput("m_rawinput");
	m_pMouseRawButton->SetSelected(m_rawinput.GetInt());
	ConVarRef con_enable("con_enable");
	m_pConsoleEnabledButton->SetSelected(con_enable.GetInt());
	ConVarRef hud_fastswitch("hud_fastswitch");
	if (hud_fastswitch.GetInt() == 0) m_pWeaponFastSwitchButton->SetSelected(false);
	else m_pWeaponFastSwitchButton->SetSelected(true);
	m_pMouseSensitivity->Reset();
	char buf[64];
	Q_snprintf(buf, sizeof(buf), " %.1f", m_pMouseSensitivity->GetSliderValue());
	m_pSensitivityText->SetText(buf);

	FillInCurrentBindings();
	if (IsVisible())
	{
		m_pKeybordBingingsList->SetSelectedItem(0);
	}
}

void COptionsPanel_Controls::SaveControlsSettings()
{
	ConVarRef m_pitch("m_pitch");
	if (m_pMouseInverseXButton->IsSelected() )
	{
		if (m_pitch.GetFloat() >= 0.0f)
			m_pitch.SetValue(-(m_pitch.GetFloat()));
	}
	else
	{
		m_pitch.SetValue(fabsf(m_pitch.GetFloat()));
	}

	ConVarRef m_yaw("m_yaw");
	if (m_pMouseInverseYButton->IsSelected())
	{
		if (m_yaw.GetFloat() >= 0.0f)
			m_yaw.SetValue(-(m_yaw.GetFloat()));
	}
	else
	{
		m_yaw.SetValue(fabsf(m_yaw.GetFloat()));
	}

	ApplyChangesToConVar("m_filter", m_pMouseSmoothingButton->IsSelected());
	ApplyChangesToConVar("m_rawinput", m_pMouseRawButton->IsSelected());
	ApplyChangesToConVar("con_enable", m_pConsoleEnabledButton->IsSelected());
	ApplyChangesToConVar("hud_fastswitch", m_pWeaponFastSwitchButton->IsSelected());
	ApplyAllBindings();
}

void COptionsPanel_Controls::OnCommand(const char *command)
{
	if (!stricmp(command, "Defaults"))
	{
		// open a box asking if we want to restore defaults
		QueryBox *box = new QueryBox("#GameUI_KeyboardSettings", "#GameUI_KeyboardSettingsText");
		box->AddActionSignalTarget(this);
		box->SetOKCommand(new KeyValues("Command", "command", "DefaultsOK"));
		box->DoModal();
	}
	else if (!stricmp(command, "DefaultsOK"))
	{
		// Restore defaults from default keybindings file
		FillInDefaultBindings();
		m_pKeybordBingingsList->RequestFocus();
	}
	else if (!m_pKeybordBingingsList->IsCapturing() && !stricmp(command, "ChangeKey"))
	{
		m_pKeybordBingingsList->StartCaptureMode(dc_blank);
	}
	else if (!m_pKeybordBingingsList->IsCapturing() && !stricmp(command, "ClearKey"))
	{
		OnKeyCodePressed(KEY_DELETE);
		m_pKeybordBingingsList->RequestFocus();
	}
	else
	{
		BaseClass::OnCommand(command);
	}

	/*
	if (!Q_strcmp(command, "cancel") || !Q_strcmp(command, "Close"))
	{
		SetVisible(false);
		//	di_vgui_advancedvideooptions.SetValue("0");
	}
	else if (!Q_strcmp(command, "save"))
	{
		SaveControlsSettings();
		SetVisible(false);
		//	di_vgui_advancedvideooptions.SetValue("0");
	}
	else if (!Q_strcmp(command, "apply"))
	{
		SaveControlsSettings();
	}

	BaseClass::OnCommand(command);
	*/
}

void COptionsPanel_Controls::OnResetData()
{
	LoadControlsSettings();
}

const char *UTIL_Parse(const char *data, char *token, int sizeofToken)
{
	data = engine->ParseFile(data, token, sizeofToken);
	return data;
}

static char *UTIL_CopyString(const char *in)
{
	int len = strlen(in) + 1;
	char *out = new char[len];
	Q_strncpy(out, in, len);
	return out;
}

char *UTIL_va(char *format, ...)
{
	va_list		argptr;
	static char	string[4][1024];
	static int	curstring = 0;

	curstring = (curstring + 1) % 4;

	va_start(argptr, format);
	Q_vsnprintf(string[curstring], 1024, format, argptr);
	va_end(argptr);

	return string[curstring];
}

void COptionsPanel_Controls::OnKeyCodeTyped(vgui::KeyCode code)
{
	if (code == KEY_ENTER)
	{
		OnCommand("ChangeKey");
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::ParseActionDescriptions(void)
{
	char szBinding[256];
	char szDescription[256];

	KeyValues *item;

	// Load the default keys list
	CUtlBuffer buf(0, 0, CUtlBuffer::TEXT_BUFFER);
	if (!g_pFullFileSystem->ReadFile("scripts/kb_act.lst", NULL, buf))
		return;

	const char *data = (const char*)buf.Base();

	int sectionIndex = 0;
	char token[512];
	while (1)
	{
		data = UTIL_Parse(data, token, sizeof(token));
		// Done.
		if (strlen(token) <= 0)
			break;

		Q_strncpy(szBinding, token, sizeof(szBinding));

		data = UTIL_Parse(data, token, sizeof(token));
		if (strlen(token) <= 0)
		{
			break;
		}

		Q_strncpy(szDescription, token, sizeof(szDescription));

		// Skip '======' rows
		if (szDescription[0] != '=')
		{
			// Flag as special header row if binding is "blank"
			if (!stricmp(szBinding, "blank"))
			{
				// add header item
				m_pKeybordBingingsList->AddSection(++sectionIndex, szDescription);
				m_pKeybordBingingsList->AddColumnToSection(sectionIndex, "Action", szDescription, SectionedListPanel::COLUMN_BRIGHT, 286);
				m_pKeybordBingingsList->AddColumnToSection(sectionIndex, "Key", "#GameUI_KeyButton", SectionedListPanel::COLUMN_BRIGHT, 128);
			}
			else
			{
				// Create a new: blank item
				item = new KeyValues("Item");

				// fill in data
				item->SetString("Action", szDescription);
				item->SetString("Binding", szBinding);
				item->SetString("Key", "");

				// Add to list
				m_pKeybordBingingsList->AddItem(sectionIndex, item);
				item->deleteThis();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Search current data set for item which has the specified binding string
// Input  : *binding - string to find
// Output : KeyValues or NULL on failure
//-----------------------------------------------------------------------------
KeyValues *COptionsPanel_Controls::GetItemForBinding(const char *binding)
{
	static int bindingSymbol = KeyValuesSystem()->GetSymbolForString("Binding");

	// Loop through all items
	for (int i = 0; i < m_pKeybordBingingsList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeybordBingingsList->GetItemData(m_pKeybordBingingsList->GetItemIDFromRow(i));
		if (!item)
			continue;

		KeyValues *bindingItem = item->FindKey(bindingSymbol);
		const char *bindString = bindingItem->GetString();

		// Check the "Binding" key
		if (!stricmp(bindString, binding))
			return item;
	}
	// Didn't find it
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Bind the specified keyname to the specified item
// Input  : *item - Item to which to add the key
//			*keyname - The key to be added
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::AddBinding(KeyValues *item, const char *keyname)
{
	// See if it's already there as a binding
	if (!stricmp(item->GetString("Key", ""), keyname))
		return;

	// Make sure it doesn't live anywhere
	RemoveKeyFromBindItems(item, keyname);

	const char *binding = item->GetString("Binding", "");

	// Loop through all the key bindings and set all entries that have
	// the same binding. This allows us to have multiple entries pointing 
	// to the same binding.
	for (int i = 0; i < m_pKeybordBingingsList->GetItemCount(); i++)
	{
		KeyValues *curitem = m_pKeybordBingingsList->GetItemData(m_pKeybordBingingsList->GetItemIDFromRow(i));
		if (!curitem)
			continue;

		const char *curbinding = curitem->GetString("Binding", "");

		if (!stricmp(curbinding, binding))
		{
			curitem->SetString("Key", keyname);
			m_pKeybordBingingsList->InvalidateItem(i);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Remove all keys from list
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::ClearBindItems(void)
{
	for (int i = 0; i < m_pKeybordBingingsList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeybordBingingsList->GetItemData(m_pKeybordBingingsList->GetItemIDFromRow(i));
		if (!item)
			continue;

		// Reset keys
		item->SetString("Key", "");

		m_pKeybordBingingsList->InvalidateItem(i);
	}

	m_pKeybordBingingsList->InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Remove all instances of the specified key from bindings
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::RemoveKeyFromBindItems(KeyValues *org_item, const char *key)
{
	Assert(key && key[0]);
	if (!key || !key[0])
		return;

	int len = Q_strlen(key);
	char *pszKey = new char[len + 1];

	if (!pszKey)
		return;

	Q_memcpy(pszKey, key, len + 1);

	for (int i = 0; i < m_pKeybordBingingsList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeybordBingingsList->GetItemData(m_pKeybordBingingsList->GetItemIDFromRow(i));
		if (!item)
			continue;

		// If it's bound to the primary: then remove it
		if (!stricmp(pszKey, item->GetString("Key", "")))
		{
			bool bClearEntry = true;

			if (org_item)
			{
				// Only clear it out if the actual binding isn't the same. This allows
				// us to have the same key bound to multiple entries in the keybinding list
				// if they point to the same command.
				const char *org_binding = org_item->GetString("Binding", "");
				const char *binding = item->GetString("Binding", "");
				if (!stricmp(org_binding, binding))
				{
					bClearEntry = false;
				}
			}

			if (bClearEntry)
			{
				item->SetString("Key", "");
				m_pKeybordBingingsList->InvalidateItem(i);
			}
		}
	}

	delete[] pszKey;

	// Make sure the display is up to date
	m_pKeybordBingingsList->InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Ask the engine for all bindings and set up the list
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::FillInCurrentBindings(void)
{
	// reset the unbind list
	// we only unbind keys used by the normal config items (not custom binds)
	m_KeysToUnbind.RemoveAll();

	// Clear any current settings
	ClearBindItems();

	for (int i = 0; i < BUTTON_CODE_LAST; i++)
	{
		// Look up binding
		const char *binding = gameuifuncs->GetBindingForButtonCode((ButtonCode_t)i);
		if (!binding)
			continue;

		// See if there is an item for this one?
		KeyValues *item = GetItemForBinding(binding);
		if (item)
		{
			// Bind it by name
			const char *keyName = g_pInputSystem->ButtonCodeToString((ButtonCode_t)i);

			// Already in list, means user had two keys bound to this item.  We'll only note the first one we encounter
			char const *currentKey = item->GetString("Key", "");
			if (currentKey && currentKey[0])
			{
				ButtonCode_t currentBC = (ButtonCode_t)gameuifuncs->GetButtonCodeForBind(currentKey);

				// Remove the key we're about to override from the unbinding list
				m_KeysToUnbind.FindAndRemove(currentKey);
			}

			AddBinding(item, keyName);

			// remember to apply unbinding of this key when we apply
			m_KeysToUnbind.AddToTail(keyName);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clean up memory used by saved bindings
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::DeleteSavedBindings(void)
{
	for (int i = 0; i < BUTTON_CODE_LAST; i++)
	{
		if (m_Bindings[i].binding)
		{
			delete[] m_Bindings[i].binding;
			m_Bindings[i].binding = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copy all bindings into save array
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::SaveCurrentBindings(void)
{
	DeleteSavedBindings();
	for (int i = 0; i < BUTTON_CODE_LAST; i++)
	{
		const char *binding = gameuifuncs->GetBindingForButtonCode((ButtonCode_t)i);
		if (!binding || !binding[0])
			continue;

		// Copy the binding string
		m_Bindings[i].binding = UTIL_CopyString(binding);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tells the engine to bind a key
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::BindKey(const char *key, const char *binding)
{
#ifndef _XBOX
	engine->ClientCmd_Unrestricted(UTIL_va("bind \"%s\" \"%s\"\n", key, binding));
#else
	char buff[256];
	Q_snprintf(buff, sizeof(buff), "bind \"%s\" \"%s\"\n", key, binding);
	engine->ClientCmd_Unrestricted(buff);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Tells the engine to unbind a key
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::UnbindKey(const char *key)
{
#ifndef _XBOX
	engine->ClientCmd_Unrestricted(UTIL_va("unbind \"%s\"\n", key));
#else
	char buff[256];
	Q_snprintf(buff, sizeof(buff), "unbind \"%s\"\n", key);
	engine->ClientCmd_Unrestricted(buff);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Go through list and bind specified keys to actions
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::ApplyAllBindings(void)
{
	// unbind everything that the user unbound
	{for (int i = 0; i < m_KeysToUnbind.Count(); i++)
	{
		UnbindKey(m_KeysToUnbind[i].String());
	}}
	m_KeysToUnbind.RemoveAll();

	// free binding memory
	DeleteSavedBindings();

	for (int i = 0; i < m_pKeybordBingingsList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeybordBingingsList->GetItemData(m_pKeybordBingingsList->GetItemIDFromRow(i));
		if (!item)
			continue;

		// See if it has a binding
		const char *binding = item->GetString("Binding", "");
		if (!binding || !binding[0])
			continue;

		const char *keyname;

		// Check main binding
		keyname = item->GetString("Key", "");
		if (keyname && keyname[0])
		{
			// Tell the engine
			BindKey(keyname, binding);
			ButtonCode_t code = g_pInputSystem->StringToButtonCode(keyname);
			if (code != BUTTON_CODE_INVALID)
			{
				m_Bindings[code].binding = UTIL_CopyString(binding);
			}
		}
	}

	// Now exec their custom bindings
	engine->ClientCmd_Unrestricted("exec userconfig.cfg\nhost_writeconfig\n");
}

//-----------------------------------------------------------------------------
// Purpose: Read in defaults from game's default config file and populate list 
//			using those defaults
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::FillInDefaultBindings(void)
{
	FileHandle_t fh = g_pFullFileSystem->Open("cfg/config_default.cfg", "rb");
	if (fh == FILESYSTEM_INVALID_HANDLE)
		return;

	int size = g_pFullFileSystem->Size(fh);
	CUtlBuffer buf(0, size, CUtlBuffer::TEXT_BUFFER);
	g_pFullFileSystem->Read(buf.Base(), size, fh);
	g_pFullFileSystem->Close(fh);

	// Clear out all current bindings
	ClearBindItems();

	const char *data = (const char*)buf.Base();

	// loop through all the binding
	while (data != NULL)
	{
		char cmd[64];
		data = UTIL_Parse(data, cmd, sizeof(cmd));
		if (strlen(cmd) <= 0)
			break;

		if (!stricmp(cmd, "bind"))
		{
			// Key name
			char szKeyName[256];
			data = UTIL_Parse(data, szKeyName, sizeof(szKeyName));
			if (strlen(szKeyName) <= 0)
				break; // Error

			char szBinding[256];
			data = UTIL_Parse(data, szBinding, sizeof(szBinding));
			if (strlen(szKeyName) <= 0)
				break; // Error

					   // Find item
			KeyValues *item = GetItemForBinding(szBinding);
			if (item)
			{
				// Bind it
				AddBinding(item, szKeyName);
			}
		}
	}

	PostActionSignal(new KeyValues("ApplyButtonEnable"));

	// Make sure console and escape key are always valid
	KeyValues *item = GetItemForBinding("toggleconsole");
	if (item)
	{
		// Bind it
		AddBinding(item, "`");
	}
	item = GetItemForBinding("cancelselect");
	if (item)
	{
		// Bind it
		AddBinding(item, "ESCAPE");
	}
}

//-----------------------------------------------------------------------------
// Purpose: User clicked on item: remember where last active row/column was
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::ItemSelected(int itemID)
{
	m_pKeybordBingingsList->SetItemOfInterest(itemID);

	if (m_pKeybordBingingsList->IsItemIDValid(itemID))
	{
		// find the details, see if we should be enabled/clear/whatever
		m_pSetBindingButton->SetEnabled(true);

		KeyValues *kv = m_pKeybordBingingsList->GetItemData(itemID);
		if (kv)
		{
			const char *key = kv->GetString("Key", NULL);
			if (key && *key)
			{
				m_pClearBindingButton->SetEnabled(true);
			}
			else
			{
				m_pClearBindingButton->SetEnabled(false);
			}

			if (kv->GetInt("Header"))
			{
				m_pSetBindingButton->SetEnabled(false);
			}
		}
	}
	else
	{
		m_pSetBindingButton->SetEnabled(false);
		m_pClearBindingButton->SetEnabled(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the capture has finished
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::Finish(ButtonCode_t code)
{
	int r = m_pKeybordBingingsList->GetItemOfInterest();

	// Retrieve clicked row and column
	m_pKeybordBingingsList->EndCaptureMode(dc_arrow);

	// Find item for this row
	KeyValues *item = m_pKeybordBingingsList->GetItemData(r);
	if (item)
	{
		// Handle keys: but never rebind the escape key
		// Esc just exits bind mode silently
		if (code != BUTTON_CODE_NONE && code != KEY_ESCAPE && code != BUTTON_CODE_INVALID)
		{
			// Bind the named key
			AddBinding(item, g_pInputSystem->ButtonCodeToString(code));
			PostActionSignal(new KeyValues("ApplyButtonEnable"));
		}

		m_pKeybordBingingsList->InvalidateItem(r);
	}

	m_pSetBindingButton->SetEnabled(true);
	m_pClearBindingButton->SetEnabled(true);
}

//-----------------------------------------------------------------------------
// Purpose: Scans for captured key presses
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::OnThink()
{
	BaseClass::OnThink();

	if (m_pKeybordBingingsList->IsCapturing())
	{
		ButtonCode_t code = BUTTON_CODE_INVALID;
		if (engine->CheckDoneKeyTrapping(code))
		{
			Finish(code);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check for enter key and go into keybinding mode if it was pressed
//-----------------------------------------------------------------------------
void COptionsPanel_Controls::OnKeyCodePressed(vgui::KeyCode code)
{
	// Enter key pressed and not already trapping next key/button press
	if (!m_pKeybordBingingsList->IsCapturing())
	{
		// Grab which item was set as interesting
		int r = m_pKeybordBingingsList->GetItemOfInterest();

		// Check that it's visible
		int x, y, w, h;
		bool visible = m_pKeybordBingingsList->GetCellBounds(r, 1, x, y, w, h);
		if (visible)
		{
			if (KEY_DELETE == code)
			{
				// find the current binding and remove it
				KeyValues *kv = m_pKeybordBingingsList->GetItemData(r);

				const char *key = kv->GetString("Key", NULL);
				if (key && *key)
				{
					RemoveKeyFromBindItems(NULL, key);
				}

				m_pClearBindingButton->SetEnabled(false);
				m_pKeybordBingingsList->InvalidateItem(r);
				PostActionSignal(new KeyValues("ApplyButtonEnable"));

				// message handled, don't pass on
				return;
			}
		}
	}

	// Allow base class to process message instead
	BaseClass::OnKeyCodePressed(code);
}