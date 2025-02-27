#include "cbase.h"
#include "attachment_ui.h"

using namespace vgui; // >:(

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

CON_COMMAND(showattachment, "ditto")
{
    if (!gViewPortInterface)
        return;

    IViewPortPanel* panel = gViewPortInterface->FindPanelByName(PANEL_ATTACHMENT_UI);

    if (panel)
    {
        gViewPortInterface->ShowPanel(panel, true);
    }
    else
    {
        Msg("Couldn't find PANEL_ATTACHMENT_UI panel.\n");
    }
}

CAttachmentUI::CAttachmentUI(IViewPort* pViewPort) : Frame(NULL, PANEL_ATTACHMENT_UI)
{
    m_pViewPort = pViewPort;

    SetSize(320, 320);
    SetVisible(true);
    Test = new Button(this, "Test", "Test");

    m_pViewPort->ShowPanel(this, true);

    DevMsg("Attachment UI is here\n");
}

void CAttachmentUI::Update(void)
{
    // Resolution
}

void CAttachmentUI::SetData(KeyValues* data)
{
    // TODO
}

void CAttachmentUI::Reset(void)
{
    // TODO
}

void CAttachmentUI::ShowPanel(bool bShow)
{
    if (BaseClass::IsVisible() == bShow)
        return;

    m_pViewPort->ShowBackGround(bShow);

    if (bShow)
    {
        Activate();
        SetMouseInputEnabled(true);
    }
    else
    {
        SetVisible(false);
        SetMouseInputEnabled(false);
    }
}