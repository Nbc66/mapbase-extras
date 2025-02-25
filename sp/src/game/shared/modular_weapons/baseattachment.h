#ifndef BASE_WEAPON_ATTACHMENT_H
#define BASE_WEAPON_ATTACHMENT_H
#include "cbase.h"
//#include "basemodularweapon.h"

#ifdef CLIENT_DLL
#define CBaseWeaponAttachment C_BaseWeaponAttachment
#endif // CLIENT_DLL

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

class CBaseWeaponAttachment : public CBaseEntity
{
public:
    DECLARE_CLASS(CBaseWeaponAttachment, CBaseEntity)
#ifndef CLIENT_DLL
    DECLARE_DATADESC();
#endif // !CLIENT_DLL
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

    AttachmentType_t GetAttachmentType();

    float GetDamageModifier();
    float GetFireRateModifier();
    float GetSpreadModifier();

private:
    AttachmentType_t m_AttachmentType;
    float m_flDamageModifier;
    float m_flFireRateModifier;
    float m_flSpreadModifier;

};
#endif // !BASE_WEAPON_ATTACHMENT_H