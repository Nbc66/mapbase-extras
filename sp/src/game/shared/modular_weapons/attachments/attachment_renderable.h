#ifndef ATTACHMENT_RENDERABLE_H
#define ATTACHMENT_RENDERABLE_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL

#include "c_baseanimating.h"

struct ClientModelRenderInfo_t;

//-----------------------------------------------------------------------------
// Client-only renderable for weapon attachment models. Bonemerges to a parent
// entity (viewmodel, worldmodel, or weapon entity in the world). Does not
// consume server edicts — created via InitializeAsClientEntity().
//
// Implementation lives in attachment_renderable.cpp.
//-----------------------------------------------------------------------------
class C_AttachmentRenderable : public C_BaseAnimating
{
public:
    DECLARE_CLASS(C_AttachmentRenderable, C_BaseAnimating)

    // Required for bonemerge to actually access bones on the parent skeleton.
    // Without these, bonemerge silently fails and the model floats.
    void SetupBonemerge();

    // Render group depends on the parent: viewmodel attachments must draw in
    // the viewmodel pass (after viewmodel bone setup), worldmodel attachments
    // draw in the normal entity passes alongside the parent worldmodel.
    virtual RenderGroup_t GetRenderGroup() OVERRIDE;

    // Sample lighting at the parent's illumination point instead of our own
    // bonemerged origin. Without this, the attachment can look unlit in lit
    // areas (or vice versa) because our bone-derived position is somewhere
    // weird relative to the parent's actual mesh.
    virtual bool OnInternalDrawModel(ClientModelRenderInfo_t* pInfo) OVERRIDE;

#ifdef FP
    // As a gesture source: route the gesture anim event to the owning weapon.
    virtual void FireEvent(const Vector& origin, const QAngle& angles, int event, const char* options) OVERRIDE;

    // Deferred self-removal: CBaseViewModel::RetireGesture schedules this so it doesn't
    // delete us mid view-model-render-walk (which would crash). See the .cpp.
    virtual void ClientThink() OVERRIDE;
#endif
};

#endif // CLIENT_DLL

#endif // ATTACHMENT_RENDERABLE_H