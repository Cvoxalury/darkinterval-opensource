#include "cbase.h"
#include "inewgamedialogue.h"
#include "igenericmenupanel.h"
#include <vgui/Ivgui.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>
#include "vgui_controls/button.h"
#include "ienginevgui.h"
#include "igenericmenuutil.h"
#include <vgui/ISurface.h>

#define CLOSED_BETA_0

#ifdef CLOSED_BETA_1
#define DARKINTERVAL_NUM_CHAPTERS 8
#else
#define DARKINTERVAL_NUM_CHAPTERS 8 // set as real number of chapters minus one, because we start from chapter 0.
#endif
// See interface.h/.cpp for specifics:  basically this ensures that we actually Sys_UnloadModule the dll and that we don't call Sys_LoadModule 
//  over and over again.
static CDllDemandLoader g_GameUI("GameUI");

CNewGameDialogue::CNewGameDialogue(vgui::Panel parent) : BaseClass(NULL, "NewGameDialogue_Darkinterval")
{
	SetTitle("#GameUI_NewGame", true);

	SetDeleteSelfOnClose(true);

//	SetParent(&parent);
	SetBounds(0, 0, 500, 600);
	SetCloseButtonVisible(false);
	SetMoveable(true);
	SetDragEnabled(false);
	SetShowDragHelper(false);
	SetVisible(false);
	SetTitleBarVisible(true);
	SetSizeable(false);
	SetRoundedCorners(0);
	SetPaintBorderEnabled(true);

#ifdef CLOSED_BETA_1
	m_nSelectedChapter = 4;
#else
	m_nSelectedChapter = 0;
#endif
	tgaChapterImg = new CRAWTGAImagePanel(this, "ChapterImage");
	m_pChapterDescription = new Label(this, "ChapterDescription", "");
	m_pChapterTitle = new Label(this, "ChapterTitle", "");
	m_pButtonNext = new vgui::Button(this, "ButtonNext", ">");
	m_pButtonPrevious = new vgui::Button(this, "ButtonPrev", "<");
	
	CreateInterfaceFn gameUIFactory = g_GameUI.GetFactory();
	if (gameUIFactory)
	{
		m_pGameUI = (IGameUI*)gameUIFactory(GAMEUI_INTERFACE_VERSION, NULL);
	}

	vgui::ivgui()->AddTickSignal(GetVPanel(), 100);

	LoadControlSettings("resource/ui/di_newgamedialogue.res", NULL);
}

void CNewGameDialogue::Activate()
{
	BaseClass::Activate();
}

void CNewGameDialogue::OnKeyCodePressed(vgui::KeyCode code)
{
	BaseClass::OnKeyCodePressed(code);
}

void CNewGameDialogue::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNewGameDialogue::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBorder(pScheme->GetBorder("RaisedBorder"));
	m_pChapterTitle->SetBorder(pScheme->GetBorder("ToolTipBorder"));
	tgaChapterImg->SetBorder(pScheme->GetBorder("ToolTipBorder"));
	m_pChapterDescription->SetBorder(pScheme->GetBorder("ToolTipBorder"));
	m_pChapterDescription->SetBgColor(pScheme->GetColor("NewMenuNewGameUnSelectedBorder", BLANK));
}

void CNewGameDialogue::SetChapterTGA(void)
{
	tgaChapterImg->DrawNewGameTGAImage(m_nSelectedChapter);
}

void CNewGameDialogue::OnCommand(const char *command)
{
	if (!Q_strcmp(command, PREVIOUS_CHAPTER))
	{
		m_nSelectedChapter--;
		SetChapterTGA();
	}

	if (!Q_strcmp(command, NEXT_CHAPTER))
	{
		m_nSelectedChapter++;
		SetChapterTGA();
	}

	if (!Q_strcmp(command, "StartNewGame"))
	{
		Close();

		char command[64];
		Q_snprintf(command, sizeof(command), "progress_enable\nexec chapter%i", m_nSelectedChapter);
		engine->ClientCmd_Unrestricted(command);
		
	}

	if (!Q_strcmp(command, "close")) Close();
	IGenericMenuUtil::CloseGameFrames(GetVPanel());
}

void CNewGameDialogue::OnThink()
{
	BaseClass::OnThink();
}

void CNewGameDialogue::OnTick()
{
	BaseClass::OnTick();

	if (engine->IsInGame() && !(engine->IsPaused() || engine->IsLevelMainMenuBackground()))
	{
		Close();
	}

	if (!engine->IsDrawingLoadingImage() && !IsVisible())
	{
		Activate();
		SetVisible(true);
	}
#ifdef CLOSED_BETA_1
//	m_nSelectedChapter = 4;
//	SetChapterTGA();
	if (m_nSelectedChapter < 4) m_nSelectedChapter = 4;
	if (m_nSelectedChapter > DARKINTERVAL_NUM_CHAPTERS) m_nSelectedChapter = DARKINTERVAL_NUM_CHAPTERS;

	if (m_nSelectedChapter < 5) m_pButtonPrevious->SetEnabled(false);
	else m_pButtonPrevious->SetEnabled(true);

	if (m_nSelectedChapter > DARKINTERVAL_NUM_CHAPTERS - 1) m_pButtonNext->SetEnabled(false);
	else m_pButtonNext->SetEnabled(true);
#else
	if (m_nSelectedChapter < 0) m_nSelectedChapter = 0;
	if (m_nSelectedChapter > DARKINTERVAL_NUM_CHAPTERS) m_nSelectedChapter = DARKINTERVAL_NUM_CHAPTERS;

	if (m_nSelectedChapter < 1) m_pButtonPrevious->SetEnabled(false);
	else m_pButtonPrevious->SetEnabled(true);

	if (m_nSelectedChapter > DARKINTERVAL_NUM_CHAPTERS - 1) m_pButtonNext->SetEnabled(false);
	else m_pButtonNext->SetEnabled(true);
#endif

	char desc[30];
	Q_snprintf(desc, sizeof(desc), "#darkinterval_Chapter%i_Desc", m_nSelectedChapter);
	char title[40];
	Q_snprintf(title, sizeof(title), "#darkinterval_Chapter%i_Num_Title", m_nSelectedChapter);

	m_pChapterDescription->SetText(desc);
	m_pChapterTitle->SetText(title);	
}