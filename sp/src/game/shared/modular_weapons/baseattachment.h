#ifndef BASE_WEAPON_ATTACHMENT_H
#define BASE_WEAPON_ATTACHMENT_H
#include "cbase.h"

#ifdef CLIENT_DLL
#define CBaseWeaponAttachment C_BaseWeaponAttachment
#endif // CLIENT_DLL

class CBaseWeaponAttachment : public CBaseEntity
{
#ifndef CLIENT_DLL
    DECLARE_DATADESC();
#endif // !CLIENT_DLL

    DECLARE_CLASS( CBaseWeaponAttachment, CBaseEntity )

public:
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

private:

};
#endif // !BASE_WEAPON_ATTACHMENT_H