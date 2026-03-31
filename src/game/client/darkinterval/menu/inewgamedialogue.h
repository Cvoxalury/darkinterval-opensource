#include <vgui_controls/Frame.h>
#include "GameUI/IGameUI.h"
#include "itgaiconloader.h"

#define BLANK Color(0, 0, 0, 0)
#define PREVIOUS_CHAPTER			"SwitchToPreviousChapter"
#define NEXT_CHAPTER				"SwitchToNextChapter"
#define	LAUNCH_GAME					"LaunchNewGame"

class CNewGameDialogue : public vgui::Frame // It's the base of the menu which holds other elements on it.
{
	DECLARE_CLASS_SIMPLE(CNewGameDialogue, vgui::Frame);

	CNewGameDialogue(vgui::Panel *parent);
	~CNewGameDialogue() {};

protected:
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnTick();
	virtual void OnThink();
	virtual void Activate();
	virtual void OnCommand(const char*);
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme*);
	void SetChapterTGA(void);

private:
	CRAWTGAImagePanel	*tgaChapterImg;
	vgui::Label			*m_pChapterDescription;
	vgui::Label			*m_pChapterTitle;
	vgui::Button		*m_pButtonPrevious;
	vgui::Button		*m_pButtonNext;
	IGameUI				*m_pGameUI;
	int					m_nSelectedChapter;
};