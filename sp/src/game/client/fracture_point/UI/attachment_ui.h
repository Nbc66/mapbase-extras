#ifndef ATTATCHMENT_UI
#define ATTATCHMENT_UI

#include <vgui/IVGui.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/HTML.h>

#include <game/client/iviewport.h>
#include "shareddefs.h"

#define PANEL_ATTACHMENT_UI "attachmentui"

class CAttachmentUI : public vgui::Frame, public IViewPortPanel
{
    DECLARE_CLASS_SIMPLE(CAttachmentUI, vgui::Frame);

public:
    CAttachmentUI(IViewPort* pViewPort);
	~CAttachmentUI() {}

	virtual const char* GetName(void) { return PANEL_ATTACHMENT_UI; }
	virtual void SetData(KeyValues* data);
	virtual void Reset();
	virtual void Update();
	virtual bool NeedsUpdate(void) { return false; }
	virtual bool HasInputElements(void) { return true; }
	virtual void ShowPanel(bool bShow);
	vgui::VPANEL GetVPanel(void) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent(vgui::VPANEL parent) { BaseClass::SetParent(parent); }
private:
	virtual void OnCommand(const char* command);
	virtual void OnThink();

	vgui::Button* Test;

protected:
    IViewPort* m_pViewPort;
};

#endif