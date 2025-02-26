#include "cbase.h"
#include "attachment_ui.h"

using namespace vgui; // >:(

#include <vgui/IVGui.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/Button.h>

Vector2D WorldToScreen(const Vector& worldPos)
{
    const VMatrix& w2s = engine->WorldToScreenMatrix();

    // Transform the world position to clip space
    float w = w2s[3][0] * worldPos.x + w2s[3][1] * worldPos.y + w2s[3][2] * worldPos.z + w2s[3][3];

    if (w < 0.001f)  // If behind the camera, return an invalid position
        return Vector2D(-1, -1);

    float x = w2s[0][0] * worldPos.x + w2s[0][1] * worldPos.y + w2s[0][2] * worldPos.z + w2s[0][3];
    float y = w2s[1][0] * worldPos.x + w2s[1][1] * worldPos.y + w2s[1][2] * worldPos.z + w2s[1][3];

    // Normalize coordinates
    x /= w;
    y /= w;

    // Convert to screen space
    int screenWidth, screenHeight;
    engine->GetScreenSize(screenWidth, screenHeight);

    float screenX = (screenWidth / 2.0f) * (1.0f + x);
    float screenY = (screenHeight / 2.0f) * (1.0f - y); // Invert Y-axis

    return Vector2D(screenX, screenY);
}

class CAttachmentUI : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CAttachmentUI, vgui::Frame);
    CAttachmentUI(vgui::VPANEL parent);

private:
    Button* Test;
};

CAttachmentUI::CAttachmentUI(vgui::VPANEL parent) : BaseClass(NULL, "AttachmentUI")
{
    SetParent(parent);
    SetSize(320, 320);
    SetVisible(true);
    Test = new Button(this, "Test", "Test");
}

class CAttachmentUIInterface : public IAttachmentUI
{
private:
    CAttachmentUI* AttachmentUI;
public:
    CAttachmentUIInterface() { AttachmentUI = NULL; }
    void Create(vgui::VPANEL parent)
    {
        AttachmentUI = new CAttachmentUI(parent);
    }
    void Destroy()
    {
        if (AttachmentUI)
        {
            AttachmentUI->SetParent((vgui::Panel*)NULL);
            delete AttachmentUI;
        }
    }
};

static CAttachmentUIInterface g_AttachmentUI;
IAttachmentUI* AttachmentUI = (IAttachmentUI*)&g_AttachmentUI;