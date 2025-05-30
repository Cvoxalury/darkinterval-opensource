#include <vgui/Ivgui.h>
#include <vgui_controls/Label.h>

#pragma once

class IGenericMenuItem : public vgui::Label
{
	DECLARE_CLASS_SIMPLE(IGenericMenuItem, vgui::Label);

	public:
		IGenericMenuItem(Panel *parent, const char *label, const char *command, Panel *target);

		virtual bool IsItemActive();
		virtual void SetItemActive(bool);
		virtual void PaintBackground(void);
		virtual void OnCursorEntered(void);
		virtual void OnCursorExited(void);
		virtual void PerformLayout();
		virtual void ApplySchemeSettings(vgui::IScheme*);

	private:
		Color				lightRedTrans;
		Color				lightRed;
		bool				isItemActive;
		vgui::Panel			*hoverBorder;
		vgui::ImagePanel	*normalImage;
		vgui::Label			*buttonLabel;
};