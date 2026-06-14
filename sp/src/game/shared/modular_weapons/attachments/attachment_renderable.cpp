#include "cbase.h"

#ifdef CLIENT_DLL

#include "attachment_renderable.h"
#include "studio.h"

#ifdef FP
#include "baseviewmodel_shared.h"
#include "modular_weapons/basemodularweapon.h"
#endif

#include "eventlist.h"

#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Enable bone access so bonemerge can read the parent skeleton and
//          write our own. Source's bone accessor defaults to no access; a
//          bonemerged model with no readable/writable bones silently fails
//          to follow its parent and renders at the origin.
//-----------------------------------------------------------------------------
void C_AttachmentRenderable::SetupBonemerge()
{
    m_BoneAccessor.SetReadableBones( BONE_USED_BY_ANYTHING );
    m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );
}

//-----------------------------------------------------------------------------
// Purpose: Pick the render pass based on what we're parented to.
//
// Viewmodel parent  -> RENDER_GROUP_VIEW_MODEL_*. The viewmodel render pass
//   runs AFTER viewmodel bone setup; rendering here is the only way bonemerge
//   resolves against valid parent bone matrices. Drawing a viewmodel
//   attachment in a world pass merges against stale/uninitialized bones and
//   the attachment floats (the original first-person bug).
//
// Worldmodel parent (NPC-held weapon, or a weapon lying in the world) ->
//   normal entity passes, same as the parent worldmodel itself, so the
//   parent's bones are computed before/alongside ours.
//-----------------------------------------------------------------------------
RenderGroup_t C_AttachmentRenderable::GetRenderGroup()
{
    C_BaseEntity *pMoveParent = GetMoveParent();

    if ( pMoveParent && pMoveParent->GetBaseAnimating() && pMoveParent->GetBaseAnimating()->IsViewModel())
        return RENDER_GROUP_VIEW_MODEL_TRANSLUCENT;

    // Worldmodel attachment. OPAQUE is correct for solid attachments
    // (silencers, most scopes) and is cheaper than translucent sorting.
    // If a specific attachment has alpha-blended materials (glass lens),
    // it needs RENDER_GROUP_TRANSLUCENT_ENTITY instead — drive that from
    // a per-def flag if/when such an attachment exists.
    return RENDER_GROUP_OPAQUE_ENTITY;
}

//-----------------------------------------------------------------------------
// Purpose: Borrow the parent's lighting origin so the attachment is lit as if
//          it were part of the parent model. Our own bonemerged origin can sit
//          somewhere unrepresentative of where the mesh actually is, producing
//          wrong lighting (dark attachment in a lit room, etc.).
//-----------------------------------------------------------------------------
bool C_AttachmentRenderable::OnInternalDrawModel( ClientModelRenderInfo_t *pInfo )
{
    if ( !BaseClass::OnInternalDrawModel( pInfo ) )
        return false;

    C_BaseEntity *pMoveParent = GetMoveParent();
    if ( !pMoveParent )
        return true;

    C_BaseAnimating *pParent = pMoveParent->GetBaseAnimating();
    if ( !pParent )
        return true;

    CStudioHdr *pParentHdr = pParent->GetModelPtr();
    if ( !pParentHdr )
        return true;

    // Static is fine here — only one model draws at a time per thread,
    // and pInfo->pLightingOrigin is consumed before the next call.
    static Vector vecLightingOrigin = vec3_origin;

    int iIllumAttachIdx = pParentHdr->IllumPositionAttachmentIndex();
    if ( iIllumAttachIdx <= 0 )
    {
        // No dedicated illum-position attachment; use the parent's
        // local illum position transformed by its world matrix.
        VectorTransform( pParentHdr->illumposition(),
                         pParent->RenderableToWorldTransform(),
                         vecLightingOrigin );
    }
    else
    {
        matrix3x4_t matAttachment;
        pParent->GetAttachment( iIllumAttachIdx, matAttachment );
        VectorTransform( pParentHdr->illumposition(),
                         matAttachment,
                         vecLightingOrigin );
    }

    pInfo->pLightingOrigin = &vecLightingOrigin;
    return true;
}

#ifdef FP
//-----------------------------------------------------------------------------
// Purpose: As a gesture source, route the gesture anim event to the owning weapon.
//          We're parented to the viewmodel, which knows its weapon.
//-----------------------------------------------------------------------------
void C_AttachmentRenderable::FireEvent( const Vector& origin, const QAngle& angles, int event, const char* options )
{
    if ( event == AE_VM_GESTURE_EVENT )
    {
        C_BaseEntity* pParent = GetMoveParent();
        if ( pParent && pParent->GetBaseAnimating() && pParent->GetBaseAnimating()->IsViewModel() )
        {
            C_BaseViewModel* pVM = assert_cast<C_BaseViewModel*>( pParent );
            C_BaseModularWeapon* pWeapon = ToModularWeapon( pVM->GetOwningWeapon() );
            if ( pWeapon )
                pWeapon->OnGestureEvent( options );
        }
        return;
    }

    BaseClass::FireEvent( origin, angles, event, options );
}

// Deferred self-removal scheduled by CBaseViewModel::RetireGesture: it can't delete us
// synchronously (it retires from inside the view-model render-list walk), so it hides us
// and schedules this think, which runs in the safe sim phase. Nothing else thinks us.
void C_AttachmentRenderable::ClientThink()
{
    Remove();
}
#endif // FP

#endif // CLIENT_DLL
