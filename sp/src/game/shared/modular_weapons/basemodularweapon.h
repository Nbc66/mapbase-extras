#ifndef MODULAR_WEAPON_BASE_H
#define MODULAR_WEAPON_BASE_H
#ifdef _WIN32
#pragma once
#endif
#include "basehlcombatweapon_shared.h"
#include "baseattachment.h"

enum AttachmentType_t
{
    ATTACHMENT_NONE = 0,     // No attachment
    ATTACHMENT_SILENCER,     // Reduces noise, changes firing sound
    ATTACHMENT_SCOPE,        // Changes ADS behavior, modifies zoom
    ATTACHMENT_MAG, // Increases ammo capacity
    ATTACHMENT_GRIP,         // Reduces recoil
    ATTACHMENT_LASER_SIGHT,  // Adds laser dot for hip-fire accuracy
    ATTACHMENT_FLASHLIGHT,   // Enables flashlight
    ATTACHMENT_BARREL,       // Modifies bullet spread
};

#if defined( CLIENT_DLL )
#define CBaseModularWeapon C_BaseModularWeapon
#endif // CLIENT_DLL

class CBaseModularWeapon : public CBaseHLCombatWeapon
{

#ifndef CLIENT_DLL
    DECLARE_DATADESC();
#endif // !CLIENT_DLL

	DECLARE_CLASS( CBaseModularWeapon, CBaseHLCombatWeapon )

public:
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

	CBaseModularWeapon();
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

#endif // !MODULAR_WEAPON_BASE_H
