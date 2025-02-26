#ifndef MODULAR_WEAPON_BASE_H
#define MODULAR_WEAPON_BASE_H
#ifdef _WIN32
#pragma once
#endif
#include "basehlcombatweapon_shared.h"
#include "baseattachment.h"

#if defined( CLIENT_DLL )
#define CBaseModularWeapon C_BaseModularWeapon
#endif // CLIENT_DLL

class CBaseModularWeapon : public CBaseHLCombatWeapon
{
public:
    DECLARE_CLASS(CBaseModularWeapon, CBaseHLCombatWeapon)
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

    CBaseModularWeapon();

#ifndef CLIENT_DLL
    DECLARE_DATADESC();
#endif // !CLIENT_DLL
	
    virtual ~CBaseModularWeapon();
    virtual void EquipAttachment(CBaseWeaponAttachment* pAttachment);
    virtual void RemoveAttachment(AttachmentType_t type);

    virtual float GetFireRate();
    virtual float GetDamage();
    virtual float GetSpread();

private:
    float m_flBaseDamage;
    float m_flBaseFireRate;
    float m_flBaseSpread;

    float m_flModifiedDamage;
    float m_flModifiedFireRate;
    float m_flModifiedSpread;

    CUtlMap<AttachmentType_t, CBaseWeaponAttachment*> m_Attachments;
};

inline CBaseModularWeapon* ToModularWeapon(CBaseEntity* pEntity)
{
    if (!pEntity || !pEntity->IsBaseCombatWeapon())
        return NULL;
    return dynamic_cast<CBaseModularWeapon*>(pEntity);
}

#endif // !MODULAR_WEAPON_BASE_H
