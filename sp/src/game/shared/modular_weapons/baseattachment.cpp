#include "cbase.h"
#include "baseattachment.h"


#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED(BaseWeaponAttachment, DT_BaseWeaponAttachment)

BEGIN_NETWORK_TABLE(CBaseWeaponAttachment, DT_BaseWeaponAttachment)
#ifdef GAME_DLL
#endif
#ifdef CLIENT_DLL
#endif // CLIENT_DLL
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CBaseWeaponAttachment)
//DEFINE_PRED_FIELD(m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK),
END_PREDICTION_DATA()
#endif

//LINK_ENTITY_TO_CLASS(weapon_base, CBaseModularWeapon);

#ifdef GAME_DLL
BEGIN_DATADESC(CBaseWeaponAttachment)
END_DATADESC()
#endif


float CBaseWeaponAttachment::GetDamageModifier()
{
	return 0.0f;
}

AttachmentType_t CBaseWeaponAttachment::GetAttachmentType()
{
	return ATTACHMENT_NONE;
}
