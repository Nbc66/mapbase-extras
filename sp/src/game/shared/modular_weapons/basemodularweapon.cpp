//------------------------------------------------------------------------------
//  Base Modular Weapon
//  Modular weapons that can change attachments at runtime.
//  Attachments are data-driven (loaded from scripts) and rendered client-side
//  with no server edicts consumed.
//------------------------------------------------------------------------------

#include "cbase.h"
#include "basemodularweapon.h"
#include "ammodef.h"
#include "in_buttons.h"

#ifdef FP
#include "baseviewmodel_shared.h"      // gesture passthrough -> viewmodel front door
#include "gestures/gesture_def.h"      // CGestureRegistry::PrecacheAll
#endif // FP

#ifdef FP_SERVER
#include "hl2_player.h"
#endif // FP_SERVER

#ifdef CLIENT_DLL
#include "prediction.h"
#include "c_baseplayer.h"
#include "c_baseviewmodel.h"
#endif

#include "tier0/memdbgon.h"

// ---------------------------------------------------------------------------
// Viewmodel adjustment convars (unchanged from original)
// ---------------------------------------------------------------------------

void vm_adjust_enable_callback(IConVar* pConVar, char const* pOldString, float flOldValue);
void vm_adjust_fov_callback(IConVar* pConVar, const char* pOldString, float flOldValue);

ConVar viewmodel_adjust_forward("viewmodel_adjust_forward", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_right("viewmodel_adjust_right", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_up("viewmodel_adjust_up", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_pitch("viewmodel_adjust_pitch", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_yaw("viewmodel_adjust_yaw", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_roll("viewmodel_adjust_roll", "0", FCVAR_REPLICATED);
ConVar viewmodel_adjust_fov("viewmodel_adjust_fov", "0", FCVAR_REPLICATED, "Note: not available during zoom", vm_adjust_fov_callback);
ConVar viewmodel_adjust_enabled("viewmodel_adjust_enabled", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Enable viewmodel adjusting", vm_adjust_enable_callback);

#ifdef CLIENT_DLL
void CC_ToggleIronSights(void)
{
    CBasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
    if (!pPlayer) return;

    CBaseModularWeapon* pWeapon = ToModularWeapon(pPlayer->GetActiveWeapon());
    if (!pWeapon) return;

    pWeapon->ToggleIronsights();
    engine->ServerCmd("toggle_ironsight");
}
static ConCommand toggle_ironsight("toggle_ironsight", CC_ToggleIronSights);
#endif

#ifdef CLIENT_DLL
void RecvProxy_ToggleSights(const CRecvProxyData* pData, void* pStruct, void* pOut)
{
    CBaseModularWeapon* pWeapon = ToModularWeapon((CBaseEntity*)pStruct);
    if (pWeapon)
    {
        if (pData->m_Value.m_Int)
            pWeapon->EnableIronsights();
        else
            pWeapon->DisableIronsights();
    }
}
#endif

// ---------------------------------------------------------------------------
// Network table
// ---------------------------------------------------------------------------

IMPLEMENT_NETWORKCLASS_ALIASED(BaseModularWeapon, DT_BaseModularWeapon)

BEGIN_NETWORK_TABLE(CBaseModularWeapon, DT_BaseModularWeapon)
#ifdef GAME_DLL
SendPropExclude("DT_AnimTimeMustBeFirst", "m_flAnimTime"),
SendPropExclude("DT_BaseAnimating", "m_nSequence"),
SendPropArray3(SENDINFO_ARRAY3(m_AttachmentDefIndices),
    SendPropInt(SENDINFO_ARRAY(m_AttachmentDefIndices), 16, SPROP_UNSIGNED)),
    SendPropBool(SENDINFO(m_bIsIronsighted)),
    SendPropFloat(SENDINFO(m_flIronsightedTime)),
#else
RecvPropArray3(RECVINFO_ARRAY(m_AttachmentDefIndices),
    RecvPropInt(RECVINFO(m_AttachmentDefIndices[0]))),
    RecvPropInt(RECVINFO(m_bIsIronsighted), 0, RecvProxy_ToggleSights),
    RecvPropFloat(RECVINFO(m_flIronsightedTime)),
#endif
    END_NETWORK_TABLE()

#ifdef CLIENT_DLL
    BEGIN_PREDICTION_DATA(CBaseModularWeapon)
    DEFINE_PRED_FIELD(m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK),
    DEFINE_PRED_FIELD(m_bIsIronsighted, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
    DEFINE_PRED_FIELD(m_flIronsightedTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
    END_PREDICTION_DATA()
#endif

#ifdef GAME_DLL
    BEGIN_DATADESC(CBaseModularWeapon)
    // m_AttachmentDefIndices is NOT saved — it is the networked runtime
    // index cache, rebuilt from m_szAttachmentDefNames in OnRestore().
    DEFINE_AUTO_ARRAY2D(m_szAttachmentDefNames, FIELD_CHARACTER),
    DEFINE_AUTO_ARRAY(m_AttachmentInstanceIDs, FIELD_INTEGER),
    DEFINE_FIELD(m_bIsIronsighted, FIELD_BOOLEAN),
    DEFINE_FIELD(m_flIronsightedTime, FIELD_FLOAT),
    END_DATADESC()
#endif

    // ---------------------------------------------------------------------------
    // Construction / destruction
    // ---------------------------------------------------------------------------

    CBaseModularWeapon::CBaseModularWeapon()
{
    m_bIsIronsighted = false;
    m_flIronsightedTime = 0.0f;

    for (int i = 0; i < ATTACHMENT_COUNT; i++)
    {
        m_AttachmentDefIndices.Set(i, INVALID_ATTACHMENT_DEF_INDEX);
#ifndef CLIENT_DLL
        m_AttachmentInstanceIDs[i] = INVALID_ATTACHMENT_INSTANCE_ID;
        m_szAttachmentDefNames[i][0] = '\0';
#else
        m_hClientAttachments[i] = NULL;
        m_LastAttachmentDefIndices[i] = INVALID_ATTACHMENT_DEF_INDEX;
#endif
    }
}

CBaseModularWeapon::~CBaseModularWeapon()
{
#ifdef CLIENT_DLL
    for (int i = 0; i < ATTACHMENT_COUNT; i++)
        ReleaseClientAttachment(i);
#endif
}

void CBaseModularWeapon::Precache(void)
{
    BaseClass::Precache();

    // Precache every attachment def's assets. Runs once per modular weapon
    // spawn, which means it runs at least once per level (the first time a
    // modular weapon spawns) and again for any later spawn. PrecacheModel /
    // PrecacheScriptSound no-op when the asset is already cached, so the
    // duplicate calls across multiple weapon spawns are effectively free.
    CAttachmentDefRegistry::Instance().PrecacheAll();

#ifdef FP
    // Gesture source models must be precached up front, same as attachments.
    CGestureRegistry::Instance().PrecacheAll();
#endif // FP
}

#ifdef FP
// Gesture passthrough: forward to the owner's viewmodel front door.
void CBaseModularWeapon::PlayGesture(const char* pszGestureName, int slot)
{
    CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
    if (!pPlayer)
        return;
    CBaseViewModel* pVM = pPlayer->GetViewModel();
    if (pVM)
        pVM->PlayGestureByName(pszGestureName, slot);
}

void CBaseModularWeapon::StopGesture(int slot)
{
    CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
    if (!pPlayer)
        return;
    CBaseViewModel* pVM = pPlayer->GetViewModel();
    if (pVM)
        pVM->StopGesture(slot);
}

void CBaseModularWeapon::StopAllGestures(void)
{
    CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
    if (!pPlayer)
        return;
    CBaseViewModel* pVM = pPlayer->GetViewModel();
    if (pVM)
        pVM->StopAllGestures();
}

// A gesture anim event. The gesture is client-only, so the client forwards the event to
// the server, where it can drive gameplay (the handheld flashlight defers EF_DIMLIGHT to
// here, so the beam clicks on at the animation frame instead of the keypress).
void CBaseModularWeapon::OnGestureEvent(const char* options)
{
    if (!options || !*options)
        return;

#ifdef CLIENT_DLL
    if ( engine )
        engine->ServerCmd( VarArgs( "vm_gesture_event \"%s\"\n", options ) );
#else
    CBaseEntity *pOwner = GetOwner();
    if ( !pOwner )
        return;

    if ( !Q_stricmp( options, "flashlight_on" ) )
        pOwner->AddEffects( EF_DIMLIGHT );
    else if ( !Q_stricmp( options, "flashlight_off" ) )
        pOwner->RemoveEffects( EF_DIMLIGHT );
#endif
}

#ifdef GAME_DLL
// Runs a client-forwarded gesture anim event through the issuing player's active weapon.
CON_COMMAND( vm_gesture_event, "Internal: client-forwarded viewmodel gesture anim event." )
{
    CBasePlayer* pPlayer = ToBasePlayer( UTIL_GetCommandClient() );
    if ( !pPlayer || args.ArgC() < 2 )
        return;

    CBaseModularWeapon* pWeapon = ToModularWeapon( pPlayer->GetActiveWeapon() );
    if ( pWeapon )
        pWeapon->OnGestureEvent( args.Arg( 1 ) );
}
#endif // GAME_DLL

#ifdef CLIENT_DLL
bool CBaseModularWeapon::GetFlashlightLightTransform(Vector& origin, QAngle& angles)
{
    // 1) Gun-mounted flashlight prop -- highest priority.
    C_AttachmentRenderable* pProp = m_hClientAttachments[ATTACHMENT_FLASHLIGHT].Get();
    if (pProp)
    {
        int a = pProp->LookupAttachment("light");
        if (a > 0 && pProp->GetAttachment(a, origin, angles))
            return true;
    }

    // 2) Any active gesture source carrying a "light" attachment (the handheld flashlight).
    C_BasePlayer* pOwner = ToBasePlayer(GetOwner());
    C_BaseViewModel* pVM = pOwner ? pOwner->GetViewModel() : NULL;
    if (pVM)
    {
        for (int i = 0; i < MAX_VM_GESTURES; ++i)
        {
            C_BaseAnimating* pSrc = pVM->GetGestureSourceModel(i);
            if (!pSrc)
                continue;
            int a = pSrc->LookupAttachment("light");
            if (a > 0 && pSrc->GetAttachment(a, origin, angles))
                return true;
        }
    }

    return false;   // caller falls back to the eye (plain suit flashlight)
}
#endif // CLIENT_DLL
#endif // FP

// ---------------------------------------------------------------------------
// Slot accessors
// ---------------------------------------------------------------------------

bool CBaseModularWeapon::HasAttachmentInSlot(AttachmentType_t type) const
{
    if (type <= ATTACHMENT_NONE || type >= ATTACHMENT_COUNT)
        return false;
    return m_AttachmentDefIndices[type] != INVALID_ATTACHMENT_DEF_INDEX;
}

unsigned short CBaseModularWeapon::GetAttachmentDefIndex(AttachmentType_t type) const
{
    if (type <= ATTACHMENT_NONE || type >= ATTACHMENT_COUNT)
        return INVALID_ATTACHMENT_DEF_INDEX;
    return m_AttachmentDefIndices[type];
}

const AttachmentDef_t* CBaseModularWeapon::GetAttachmentDefForSlot(AttachmentType_t type) const
{
    return GetAttachmentDef(GetAttachmentDefIndex(type));
}

#ifndef CLIENT_DLL
void CBaseModularWeapon::SetAttachmentInSlot(AttachmentType_t type, AttachmentInstanceID_t instanceID, unsigned short defIndex)
{
    if (type <= ATTACHMENT_NONE || type >= ATTACHMENT_COUNT)
        return;
    m_AttachmentDefIndices.Set(type, defIndex);
    m_AttachmentInstanceIDs[type] = instanceID;

    // Keep the persistent name in lockstep with the runtime index, so the
    // next save writes the correct frozen key for this slot.
    const AttachmentDef_t* pDef = GetAttachmentDef(defIndex);
    V_strncpy(m_szAttachmentDefNames[type],
              pDef ? pDef->szName : "",
              sizeof(m_szAttachmentDefNames[type]));
}

void CBaseModularWeapon::ClearAttachmentInSlot(AttachmentType_t type)
{
    if (type <= ATTACHMENT_NONE || type >= ATTACHMENT_COUNT)
        return;
    m_AttachmentDefIndices.Set(type, INVALID_ATTACHMENT_DEF_INDEX);
    m_AttachmentInstanceIDs[type] = INVALID_ATTACHMENT_INSTANCE_ID;
    m_szAttachmentDefNames[type][0] = '\0';
}

AttachmentInstanceID_t CBaseModularWeapon::GetAttachmentInstanceID(AttachmentType_t type) const
{
    if (type <= ATTACHMENT_NONE || type >= ATTACHMENT_COUNT)
        return INVALID_ATTACHMENT_INSTANCE_ID;
    return m_AttachmentInstanceIDs[type];
}

void CBaseModularWeapon::UpdateOnRemove(void)
{
    // Return any equipped attachments to the player's inventory.
    CHL2_Player* pOwner = dynamic_cast<CHL2_Player*>(GetOwner());
    if (pOwner)
    {
        CAttachmentInventory* pInv = pOwner->GetAttachmentInventory();
        if (pInv)
            pInv->OnWeaponDestroyed(this);
    }

    BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: The datadesc restored m_szAttachmentDefNames (the persistent
//          frozen keys) but NOT m_AttachmentDefIndices (the networked runtime
//          cache). Rebuild the index array from the names against the current
//          registry. Slots whose attachment no longer exists are cleared.
//
//          Registry is loaded lazily by Precache; if a load happens before
//          any precache (shouldn't, but defensively) FindIndexByName simply
//          returns INVALID and the slot clears — graceful, not a crash.
//-----------------------------------------------------------------------------
void CBaseModularWeapon::OnRestore(void)
{
    BaseClass::OnRestore();

    CAttachmentDefRegistry& registry = CAttachmentDefRegistry::Instance();
    registry.EnsureLoaded();   // restore may run before any Precache

    for (int slot = ATTACHMENT_NONE + 1; slot < ATTACHMENT_COUNT; slot++)
    {
        const char* pszName = m_szAttachmentDefNames[slot];

        if (!pszName[0])
        {
            // Empty slot.
            m_AttachmentDefIndices.Set(slot, INVALID_ATTACHMENT_DEF_INDEX);
            continue;
        }

        unsigned short idx = registry.FindIndexByName(pszName);

        if (idx != INVALID_ATTACHMENT_DEF_INDEX)
        {
            // Defensive: make sure the resolved def actually belongs in this
            // slot. If an attachment's "type" was changed in a patch, the
            // saved name now resolves to a def of a different type — treat
            // that as a missing attachment rather than a type-mismatched slot.
            const AttachmentDef_t* pDef = registry.FindByIndex(idx);
            if (pDef && pDef->type != (AttachmentType_t)slot)
            {
                Warning("CBaseModularWeapon::OnRestore: saved attachment '%s' "
                        "changed type; clearing slot %d.\n", pszName, slot);
                idx = INVALID_ATTACHMENT_DEF_INDEX;
            }
        }
        else
        {
            Warning("CBaseModularWeapon::OnRestore: saved attachment '%s' "
                    "no longer exists; clearing slot %d.\n", pszName, slot);
        }

        m_AttachmentDefIndices.Set(slot, idx);

        if (idx == INVALID_ATTACHMENT_DEF_INDEX)
        {
            // Keep the slot fully consistent — drop the stale name and ID too.
            m_szAttachmentDefNames[slot][0] = '\0';
            m_AttachmentInstanceIDs[slot] = INVALID_ATTACHMENT_INSTANCE_ID;
        }
    }
}
#endif // !CLIENT_DLL

// ---------------------------------------------------------------------------
// Damage and shoot sound resolution from attachments
// ---------------------------------------------------------------------------

float CBaseModularWeapon::GetDamage(void)
{
    float total = GetAmmoDef()->GetAmmoOfIndex(GetPrimaryAmmoType())->pPlrDmgCVar->GetFloat();

    for (int i = ATTACHMENT_NONE + 1; i < ATTACHMENT_COUNT; i++)
    {
        const AttachmentDef_t* pDef = GetAttachmentDefForSlot((AttachmentType_t)i);
        if (pDef)
            total *= pDef->flDamageMod;
    }
    return total;
}

char const* CBaseModularWeapon::GetShootSound(int iIndex) const
{
    if (iIndex == SINGLE)
    {
        const AttachmentDef_t* pDef = GetAttachmentDefForSlot(ATTACHMENT_SILENCER);
        if (pDef && pDef->szFireSound[0])
            return pDef->szFireSound;
    }
    return BaseClass::GetShootSound(iIndex);
}

Vector CBaseModularWeapon::GetIronsightPositionOffset(void) const
{
    if (viewmodel_adjust_enabled.GetBool())
        return Vector(viewmodel_adjust_forward.GetFloat(),
            viewmodel_adjust_right.GetFloat(),
            viewmodel_adjust_up.GetFloat());

    const AttachmentDef_t* pDef = GetAttachmentDefForSlot(ATTACHMENT_SCOPE);
    if (pDef)
        return pDef->vecVMOffset;

    return GetWpnData().vecIronsightPosOffset;
}

QAngle CBaseModularWeapon::GetIronsightAngleOffset(void) const
{
    if (viewmodel_adjust_enabled.GetBool())
        return QAngle(viewmodel_adjust_pitch.GetFloat(),
            viewmodel_adjust_yaw.GetFloat(),
            viewmodel_adjust_roll.GetFloat());
    return GetWpnData().angIronsightAngOffset;
}

float CBaseModularWeapon::GetIronsightFOVOffset(void) const
{
    if (viewmodel_adjust_enabled.GetBool())
        return viewmodel_adjust_fov.GetFloat();
    return GetWpnData().flIronsightFOVOffset;
}

// ---------------------------------------------------------------------------
// Visibility / holster / reload � keep your existing behavior, hook client side
// ---------------------------------------------------------------------------

void CBaseModularWeapon::SetWeaponVisible(bool visible)
{
    DevMsg("SetWeaponVisible(%d) on weapon %s\n", visible, GetClassname());
    BaseClass::SetWeaponVisible(visible);
#ifdef CLIENT_DLL
    UpdateClientAttachments();
#endif
}

bool CBaseModularWeapon::Holster(CBaseCombatWeapon* pSwitchingTo)
{
    SetWeaponVisible(false);
    DisableIronsights();
    return BaseClass::Holster(pSwitchingTo);
}

bool CBaseModularWeapon::DefaultReload(int iClipSize1, int iClipSize2, int iActivity)
{
    DisableIronsights();
    return BaseClass::DefaultReload(iClipSize1, iClipSize2, iActivity);
}

// ---------------------------------------------------------------------------
// Ironsights (unchanged from original)
// ---------------------------------------------------------------------------

void vm_adjust_enable_callback(IConVar* pConVar, char const* pOldString, float flOldValue)
{
    ConVarRef sv_cheats("sv_cheats");
    if (!sv_cheats.IsValid() || sv_cheats.GetBool())
        return;

    ConVarRef var(pConVar);
    if (var.GetBool())
        var.SetValue("0");
}

void vm_adjust_fov_callback(IConVar* pConVar, char const* pOldString, float flOldValue)
{
    if (!viewmodel_adjust_enabled.GetBool())
        return;

    ConVarRef var(pConVar);

    CBasePlayer* pPlayer =
#ifdef GAME_DLL
        UTIL_GetCommandClient();
#else
        C_BasePlayer::GetLocalPlayer();
#endif
    if (!pPlayer)
        return;

    if (!pPlayer->SetFOV(pPlayer, pPlayer->GetDefaultFOV() + var.GetFloat(), 0.1f))
    {
        Warning("Could not set FOV\n");
        var.SetValue("0");
    }
}

bool CBaseModularWeapon::IsIronsighted(void)
{
    return (m_bIsIronsighted || viewmodel_adjust_enabled.GetBool());
}

void CBaseModularWeapon::ToggleIronsights(void)
{
    if (m_bIsIronsighted)
        DisableIronsights();
    else
        EnableIronsights();
}

void CBaseModularWeapon::EnableIronsights(void)
{
#ifdef CLIENT_DLL
    if (!prediction->IsFirstTimePredicted())
        return;
#endif
    if (!HasIronsights() || m_bIsIronsighted)
        return;

    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (!pOwner) return;

    if (pOwner->SetFOV(this, pOwner->GetDefaultFOV() + GetIronsightFOVOffset(), 1.0f))
    {
        m_bIsIronsighted = true;
        SetIronsightTime();
    }
}

void CBaseModularWeapon::DisableIronsights(void)
{
#ifdef CLIENT_DLL
    if (!prediction->IsFirstTimePredicted())
        return;
#endif
    if (!HasIronsights() || !m_bIsIronsighted)
        return;

    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (!pOwner) return;

    if (pOwner->SetFOV(this, 0, 0.4f))
    {
        m_bIsIronsighted = false;
        SetIronsightTime();
    }
}

void CBaseModularWeapon::SetIronsightTime(void)
{
    m_flIronsightedTime = gpGlobals->curtime;
}

void CBaseModularWeapon::AddViewmodelBob(CBaseViewModel* viewmodel, Vector& origin, QAngle& angles)
{
    if (!IsIronsighted())
        BaseClass::AddViewmodelBob(viewmodel, origin, angles);
}

float CBaseModularWeapon::CalcViewmodelBob(void)
{
    if (!IsIronsighted())
        return BaseClass::CalcViewmodelBob();
    return 0.0f;
}

// ---------------------------------------------------------------------------
// Item frame / fire mode (unchanged from original)
// ---------------------------------------------------------------------------

void CBaseModularWeapon::ItemPreFrame(void)
{
    BaseClass::ItemPreFrame();

    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (!pOwner) return;

    if (m_flNextSecondaryAttack < gpGlobals->curtime && (pOwner->m_nButtons & IN_FIREMODE))
    {
        ToggleFireMode();
        m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
    }
}

void CBaseModularWeapon::ItemPostFrame(void)
{
    BaseClass::ItemPostFrame();

    if (m_bInReload)
        return;

    HandleBurstFire();
}

void CBaseModularWeapon::HandleBurstFire(void)
{
    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (!pOwner) return;

    if (m_nFireMode == FM_BURST && burstFire > 0)
    {
        if (gpGlobals->curtime > m_flNextPrimaryAttack && (pOwner->m_nButtons & IN_ATTACK) == false)
        {
            if (burstFire < 3 && m_iClip1 > 0)
                PrimaryAttack();
            else
                burstFire = 0;
        }
    }
}

void CBaseModularWeapon::ToggleFireMode(void)
{
    if (m_nFireMode < FM_MAX_FIREMODE - 1)
        m_nFireMode++;
    else
        m_nFireMode = 0;
    EmitSound("Weapon.FireModeSwitch");
    burstFire = 0;
}

void CBaseModularWeapon::PrimaryAttack(void)
{
    if (UsesClipsForAmmo1() && !m_iClip1)
    {
        Reload();
        return;
    }

    CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
    if (!pPlayer) return;

    pPlayer->DoMuzzleFlash();
    SendWeaponAnim(GetPrimaryAttackActivity());
    pPlayer->SetAnimation(PLAYER_ATTACK1);

    FireBulletsInfo_t info;
    info.m_vecSrc = pPlayer->Weapon_ShootPosition();
    info.m_vecDirShooting = pPlayer->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);
    info.m_iShots = 0;

    float fireRate = GetFireRate();
    while (m_flNextPrimaryAttack <= gpGlobals->curtime)
    {
        WeaponSound(SINGLE, m_flNextPrimaryAttack);
        m_flNextPrimaryAttack = m_flNextPrimaryAttack + fireRate;
        info.m_iShots++;
        if (!fireRate)
            break;
    }

    if (UsesClipsForAmmo1())
    {
        info.m_iShots = MIN(info.m_iShots, m_iClip1);
        m_iClip1 -= info.m_iShots;
    }
    else
    {
        info.m_iShots = MIN(info.m_iShots, pPlayer->GetAmmoCount(m_iPrimaryAmmoType));
        pPlayer->RemoveAmmo(info.m_iShots, m_iPrimaryAmmoType);
    }

    info.m_flDistance = MAX_TRACE_LENGTH;
    info.m_iAmmoType = m_iPrimaryAmmoType;
    info.m_iTracerFreq = 2;

#if !defined( CLIENT_DLL )
    info.m_vecSpread = pPlayer->GetAttackSpread(this);
    float flDmg = GetDamage();
    info.SetPlayerDamage(flDmg);
    info.SetDamage(flDmg);
#else
    info.m_vecSpread = GetActiveWeapon()->GetBulletSpread();
#endif

    pPlayer->FireBullets(info);

    if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
        pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

    AddViewKick();
}

// ---------------------------------------------------------------------------
// Client-side attachment rendering
// ---------------------------------------------------------------------------

#ifdef CLIENT_DLL

void CBaseModularWeapon::OnDataChanged(DataUpdateType_t updateType)
{
    BaseClass::OnDataChanged(updateType);

    if (updateType == DATA_UPDATE_CREATED)
        SetNextClientThink(CLIENT_THINK_ALWAYS);

    // We need to react to state changes, def changes, owner changes � any
    // of these can change what GetAttachmentRenderParent returns.
    // Cheap to just always update; the function early-outs if nothing changed.
    UpdateClientAttachments();
}

C_BaseEntity* CBaseModularWeapon::GetAttachmentRenderParent(void)
{
    C_BasePlayer* pOwner = ToBasePlayer(GetOwner());
    C_BasePlayer* pLocal = C_BasePlayer::GetLocalPlayer();

    if (pOwner && pOwner == pLocal)
    {
        // Use m_iState rather than ActiveWeapon comparison � state networks
        // atomically with the weapon, so it's always self-consistent on the client.
        if (m_iState == WEAPON_IS_ACTIVE)
            return pOwner->GetViewModel();
        return NULL;
    }

    if (pOwner)
        return pOwner;

    return this;
}

void CBaseModularWeapon::UpdateClientAttachments(void)
{
    C_BaseEntity* pParent = GetAttachmentRenderParent();

    for (int i = 0; i < ATTACHMENT_COUNT; i++)
    {
        unsigned short defIdx = m_AttachmentDefIndices[i];
        const AttachmentDef_t* pDef = ::GetAttachmentDef(defIdx);

        // Slot empty -> tear down the prop entirely.
        if (!pDef)
        {
            ReleaseClientAttachment(i);
            m_LastAttachmentDefIndices[i] = defIdx;
            continue;
        }

        // Slot has data but no parent right now (we're holstered, etc.).
        // Keep the prop alive but hidden so we don't recreate it later.
        if (!pParent)
        {
            if (m_hClientAttachments[i].Get())
                m_hClientAttachments[i]->AddEffects(EF_NODRAW);
            m_LastAttachmentDefIndices[i] = defIdx;
            continue;
        }

        // Def changed -> tear down old prop, fall through to recreate.
        if (m_LastAttachmentDefIndices[i] != defIdx)
            ReleaseClientAttachment(i);

        if (!m_hClientAttachments[i].Get())
        {
            CreateClientAttachment(i, pDef, pParent);
        }
        else
        {
            // Existing prop. Make sure it's visible and on the right parent.
            m_hClientAttachments[i]->RemoveEffects(EF_NODRAW);

            if (m_hClientAttachments[i]->GetMoveParent() != pParent)
            {
                m_hClientAttachments[i]->SetParent(pParent);
                m_hClientAttachments[i]->SetLocalOrigin(vec3_origin);
                m_hClientAttachments[i]->SetLocalAngles(vec3_angle);
            }
        }

        m_LastAttachmentDefIndices[i] = defIdx;
    }
}

bool CBaseModularWeapon::CreateClientAttachment(int slot, const AttachmentDef_t* pDef, C_BaseEntity* pParent)
{
    if (!pDef || !pDef->szModel[0] || !pParent)
        return false;

    ReleaseClientAttachment(slot);

    C_AttachmentRenderable* pProp = new C_AttachmentRenderable;
    if (!pProp->InitializeAsClientEntity(pDef->szModel, RENDER_GROUP_VIEW_MODEL_TRANSLUCENT))
    {
        pProp->Release();
        return false;
    }

    pProp->SetupBonemerge();

    // FollowEntity sets up the proper parent relationship for bonemerge,
    // including bone-update ordering. This is what the old entity-based
    // system was doing via FollowEntity(pVM) in EquipAttachment.
    pProp->FollowEntity(pParent, true);

    pProp->SetLocalOrigin(vec3_origin);
    pProp->SetLocalAngles(vec3_angle);
    pProp->AddEffects(EF_PARENT_ANIMATES | EF_NOSHADOW);
    //pProp->AddSolidFlags(FSOLID_NOT_SOLID);

    m_hClientAttachments[slot] = pProp;
    return true;
}

void CBaseModularWeapon::ReleaseClientAttachment(int slot)
{
    if (m_hClientAttachments[slot].Get())
    {
        m_hClientAttachments[slot]->Release();
        m_hClientAttachments[slot] = NULL;
    }
}

#endif // CLIENT_DLL
