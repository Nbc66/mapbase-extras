//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "baseviewmodel_shared.h"
#include "datacache/imdlcache.h"

#ifdef FP
#include "basemodularweapon.h"
#include "gestures/gesture_def.h"   // GESTURES: registry + GestureDef_t for the shared front door
#endif // FP


#if defined( CLIENT_DLL )
#include "iprediction.h"
#include "prediction.h"
#include "client_virtualreality.h"
#include "sourcevr/isourcevirtualreality.h"
#ifdef FP
#include "bone_setup.h"   // GESTURES: full IBoneSetup / AccumulatePose definition
#endif // FP
#else
#include "vguiscreen.h"
#endif

#if defined( CLIENT_DLL ) && defined( SIXENSE )
#include "sixense/in_sixense.h"
#include "sixense/sixense_convars_extern.h"
#endif

#ifdef SIXENSE
extern ConVar in_forceuser;
#include "iclientmode.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VIEWMODEL_ANIMATION_PARITY_BITS 3
#define SCREEN_OVERLAY_MATERIAL "vgui/screens/vgui_overlay"

#ifdef FP
#define VIEWMODEL_GESTURE_PARITY_BITS 3   // GESTURES: bump-to-refire trigger parity
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseViewModel::CBaseViewModel()
{
#if defined( CLIENT_DLL )
	// NOTE: We do this here because the color is never transmitted for the view model.
	m_nOldAnimationParity = 0;
	m_EntClientFlags |= ENTCLIENTFLAG_ALWAYS_INTERPOLATE;
#ifdef FP
	V_memset(m_Gestures, 0, sizeof(m_Gestures));   // GESTURES
	m_nOldGesturePlayParity = 0;
	m_nOldGestureStopParity = 0;
#endif // FP
#endif
	SetRenderColor( 255, 255, 255, 255 );

	// View model of this weapon
	m_sVMName			= NULL_STRING;		
	// Prefix of the animations that should be used by the player carrying this weapon
	m_sAnimationPrefix	= NULL_STRING;

	m_nViewModelIndex	= 0;

	m_nAnimationParity	= 0;

#ifdef FP
	// GESTURES: server->client trigger state (parity-driven; see header).
	m_iGesturePlayDef    = INVALID_GESTURE_DEF_INDEX;
	m_iGesturePlaySlot   = 0;
	m_nGesturePlayParity = 0;
	m_iGestureStopSlot   = -1;
	m_nGestureStopParity = 0;
#endif // FP
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseViewModel::~CBaseViewModel()
{
}

void CBaseViewModel::UpdateOnRemove( void )
{
#if defined( CLIENT_DLL ) && defined( FP )
	StopAllGestures();   // tear down any live pose-source models
#endif
	BaseClass::UpdateOnRemove();

	DestroyControlPanels();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::Spawn( void )
{
	Precache( );
	SetSize( Vector( -8, -4, -2), Vector(8, 4, 2) );
	SetSolid( SOLID_NONE );
}


#if defined ( CSTRIKE_DLL ) && !defined ( CLIENT_DLL )
#define VGUI_CONTROL_PANELS
#endif

#if defined ( TF_DLL )
#define VGUI_CONTROL_PANELS
#endif

#ifdef INVASION_DLL
#define VGUI_CONTROL_PANELS
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetControlPanelsActive( bool bState )
{
#if defined( VGUI_CONTROL_PANELS )
	// Activate control panel screens
	for ( int i = m_hScreens.Count(); --i >= 0; )
	{
		if (m_hScreens[i].Get())
		{
			m_hScreens[i]->SetActive( bState );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CBaseViewModel::SpawnControlPanels()
{
#if defined( VGUI_CONTROL_PANELS )
	char buf[64];

	// Destroy existing panels
	DestroyControlPanels();

	CBaseCombatWeapon *weapon = m_hWeapon.Get();

	if ( weapon == NULL )
	{
		return;
	}

	MDLCACHE_CRITICAL_SECTION();

	// FIXME: Deal with dynamically resizing control panels?

	// If we're attached to an entity, spawn control panels on it instead of use
	CBaseAnimating *pEntityToSpawnOn = this;
	char *pOrgLL = "controlpanel%d_ll";
	char *pOrgUR = "controlpanel%d_ur";
	char *pAttachmentNameLL = pOrgLL;
	char *pAttachmentNameUR = pOrgUR;
	/*
	if ( IsBuiltOnAttachment() )
	{
		pEntityToSpawnOn = dynamic_cast<CBaseAnimating*>((CBaseEntity*)m_hBuiltOnEntity.Get());
		if ( pEntityToSpawnOn )
		{
			char sBuildPointLL[64];
			char sBuildPointUR[64];
			Q_snprintf( sBuildPointLL, sizeof( sBuildPointLL ), "bp%d_controlpanel%%d_ll", m_iBuiltOnPoint );
			Q_snprintf( sBuildPointUR, sizeof( sBuildPointUR ), "bp%d_controlpanel%%d_ur", m_iBuiltOnPoint );
			pAttachmentNameLL = sBuildPointLL;
			pAttachmentNameUR = sBuildPointUR;
		}
		else
		{
			pEntityToSpawnOn = this;
		}
	}
	*/

	Assert( pEntityToSpawnOn );

	// Lookup the attachment point...
	int nPanel;
	for ( nPanel = 0; true; ++nPanel )
	{
		Q_snprintf( buf, sizeof( buf ), pAttachmentNameLL, nPanel );
		int nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
		if (nLLAttachmentIndex <= 0)
		{
			// Try and use my panels then
			pEntityToSpawnOn = this;
			Q_snprintf( buf, sizeof( buf ), pOrgLL, nPanel );
			nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nLLAttachmentIndex <= 0)
				return;
		}

		Q_snprintf( buf, sizeof( buf ), pAttachmentNameUR, nPanel );
		int nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
		if (nURAttachmentIndex <= 0)
		{
			// Try and use my panels then
			Q_snprintf( buf, sizeof( buf ), pOrgUR, nPanel );
			nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nURAttachmentIndex <= 0)
				return;
		}

		const char *pScreenName;
		weapon->GetControlPanelInfo( nPanel, pScreenName );
		if (!pScreenName)
			continue;

		const char *pScreenClassname;
		weapon->GetControlPanelClassName( nPanel, pScreenClassname );
		if ( !pScreenClassname )
			continue;

		// Compute the screen size from the attachment points...
		matrix3x4_t	panelToWorld;
		pEntityToSpawnOn->GetAttachment( nLLAttachmentIndex, panelToWorld );

		matrix3x4_t	worldToPanel;
		MatrixInvert( panelToWorld, worldToPanel );

		// Now get the lower right position + transform into panel space
		Vector lr, lrlocal;
		pEntityToSpawnOn->GetAttachment( nURAttachmentIndex, panelToWorld );
		MatrixGetColumn( panelToWorld, 3, lr );
		VectorTransform( lr, worldToPanel, lrlocal );

		float flWidth = lrlocal.x;
		float flHeight = lrlocal.y;

		CVGuiScreen *pScreen = CreateVGuiScreen( pScreenClassname, pScreenName, pEntityToSpawnOn, this, nLLAttachmentIndex );
		pScreen->ChangeTeam( GetTeamNumber() );
		pScreen->SetActualSize( flWidth, flHeight );
		pScreen->SetActive( false );
		pScreen->MakeVisibleOnlyToTeammates( false );
	
#ifdef INVASION_DLL
		pScreen->SetOverlayMaterial( SCREEN_OVERLAY_MATERIAL );
#endif
		pScreen->SetAttachedToViewModel( true );
		int nScreen = m_hScreens.AddToTail( );
		m_hScreens[nScreen].Set( pScreen );
	}
#endif
}

void CBaseViewModel::DestroyControlPanels()
{
#if defined( VGUI_CONTROL_PANELS )
	// Kill the control panels
	int i;
	for ( i = m_hScreens.Count(); --i >= 0; )
	{
		DestroyVGuiScreen( m_hScreens[i].Get() );
	}
	m_hScreens.RemoveAll();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetOwner( CBaseEntity *pEntity )
{
	m_hOwner = pEntity;
#if !defined( CLIENT_DLL )
	// Make sure we're linked into hierarchy
	//SetParent( pEntity );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetIndex( int nIndex )
{
	m_nViewModelIndex = nIndex;
	Assert( m_nViewModelIndex < (1 << VIEWMODEL_INDEX_BITS) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseViewModel::ViewModelIndex( ) const
{
	return m_nViewModelIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Pass our visibility on to our child screens
//-----------------------------------------------------------------------------
void CBaseViewModel::AddEffects( int nEffects )
{
	if ( nEffects & EF_NODRAW )
	{
		SetControlPanelsActive( false );
	}

#ifdef MAPBASE
	if (GetOwningWeapon() && GetOwningWeapon()->UsesHands())
	{
		// If using hands, apply effect changes to any viewmodel children as well
		// (fixes hand models)
		for (CBaseEntity *pChild = FirstMoveChild(); pChild != NULL; pChild = pChild->NextMovePeer())
		{
			if (pChild->GetClassname()[0] == 'h')
				pChild->AddEffects( nEffects );
		}
	}
#endif

	BaseClass::AddEffects( nEffects );
}

//-----------------------------------------------------------------------------
// Purpose: Pass our visibility on to our child screens
//-----------------------------------------------------------------------------
void CBaseViewModel::RemoveEffects( int nEffects )
{
	if ( nEffects & EF_NODRAW )
	{
		SetControlPanelsActive( true );
	}

#ifdef MAPBASE
	if (GetOwningWeapon() && GetOwningWeapon()->UsesHands())
	{
		// If using hands, apply effect changes to any viewmodel children as well
		// (fixes hand models)
		for (CBaseEntity *pChild = FirstMoveChild(); pChild != NULL; pChild = pChild->NextMovePeer())
		{
			if (pChild->GetClassname()[0] == 'h')
				pChild->RemoveEffects( nEffects );
		}
	}
#endif

	BaseClass::RemoveEffects( nEffects );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *modelname - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetWeaponModel( const char *modelname, CBaseCombatWeapon *weapon )
{
	m_hWeapon = weapon;

#if defined( CLIENT_DLL )
	SetModel( modelname );
#else
	string_t str;
	if ( modelname != NULL )
	{
		str = MAKE_STRING( modelname );
	}
	else
	{
		str = NULL_STRING;
	}

	if ( str != m_sVMName )
	{
		// Msg( "SetWeaponModel %s at %f\n", modelname, gpGlobals->curtime );
		m_sVMName = str;
		SetModel( STRING( m_sVMName ) );

		// Create any vgui control panels associated with the weapon
		SpawnControlPanels();

		bool showControlPanels = weapon && weapon->ShouldShowControlPanels();
		SetControlPanelsActive( showControlPanels );
	}
#endif

#ifdef MAPBASE
	// If our owning weapon doesn't support hands, disable the hands viewmodel(s)
	bool bSupportsHands = weapon != NULL ? weapon->UsesHands() : false;
	for (CBaseEntity *pChild = FirstMoveChild(); pChild != NULL; pChild = pChild->NextMovePeer())
	{
		if (pChild->GetClassname()[0] == 'h')
		{
			bSupportsHands ? pChild->RemoveEffects( EF_NODRAW ) : pChild->AddEffects( EF_NODRAW );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseCombatWeapon
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseViewModel::GetOwningWeapon( void )
{
	return m_hWeapon.Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sequence - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SendViewModelMatchingSequence( int sequence )
{
	// since all we do is send a sequence number down to the client, 
	// set this here so other weapons code knows which sequence is playing.
	SetSequence( sequence );

	m_nAnimationParity = ( m_nAnimationParity + 1 ) & ( (1<<VIEWMODEL_ANIMATION_PARITY_BITS) - 1 );

#if defined( CLIENT_DLL )
	m_nOldAnimationParity = m_nAnimationParity;

	// Force frame interpolation to start at exactly frame zero
	m_flAnimTime			= gpGlobals->curtime;
#else
	CBaseCombatWeapon *weapon = m_hWeapon.Get();
	bool showControlPanels = weapon && weapon->ShouldShowControlPanels();
	SetControlPanelsActive( showControlPanels );
#endif

	// Restart animation at frame 0
	SetCycle( 0 );
	ResetSequenceInfo();
}

#if defined( CLIENT_DLL )
#include "ivieweffects.h"
#endif

void CBaseViewModel::CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles )
{
	// UNDONE: Calc this on the server?  Disabled for now as it seems unnecessary to have this info on the server
#if defined( CLIENT_DLL )
	QAngle vmangoriginal = eyeAngles;
	QAngle vmangles = eyeAngles;
	Vector vmorigin = eyePosition;

	CBaseCombatWeapon *pWeapon = m_hWeapon.Get();
#ifdef FP
	CBaseModularWeapon* pModWeapon = ToModularWeapon(m_hWeapon.Get());
#endif // FP
	//Allow weapon lagging
	if ( pWeapon != NULL
#ifdef FP
		|| pModWeapon && !pModWeapon->IsIronsighted()
#endif // FP
		)
	{
#if defined( CLIENT_DLL )
		if ( !prediction->InPrediction() )
#endif
		{
			// add weapon-specific bob 
			pWeapon->AddViewmodelBob( this, vmorigin, vmangles );
#if defined ( CSTRIKE_DLL )
			CalcViewModelLag( vmorigin, vmangles, vmangoriginal );
#endif
		}
	}

	// Add model-specific bob even if no weapon associated (for head bob for off hand models)
	AddViewModelBob(owner, vmorigin, vmangles);


#if defined( CLIENT_DLL )
	if ( !prediction->InPrediction() 
#ifdef FP
		&& pModWeapon && !pModWeapon->IsIronsighted()
#endif // FP
		)
	{
		// Add lag
		CalcViewModelLag( vmorigin, vmangles, vmangoriginal );

		// Let the viewmodel shake at about 10% of the amplitude of the player's view
		vieweffects->ApplyShake( vmorigin, vmangles, 0.1 );	
	}
#endif

	if( UseVR() )
	{
		g_ClientVirtualReality.OverrideViewModelTransform( vmorigin, vmangles, pWeapon && pWeapon->ShouldUseLargeViewModelVROverride() );
	}

#ifdef MAPBASE
	// Flip the view if we should be flipping
	if (ShouldFlipViewModel())
	{
		Vector vecOriginDiff = (eyePosition - vmorigin);
		QAngle angAnglesDiff = (eyeAngles - vmangles);

		vmorigin.x = (eyePosition.x + vecOriginDiff.x);
		vmorigin.y = (eyePosition.y + vecOriginDiff.y);
		
		vmangles.y = (eyeAngles.y + angAnglesDiff.y);
		vmangles.z = (eyeAngles.z + angAnglesDiff.z);
	}
#endif

	if (pModWeapon)
	{
		CalcIronsights(vmorigin, vmangles);
	}

	SetLocalOrigin( vmorigin );
	SetLocalAngles( vmangles );

#ifdef SIXENSE
	if( g_pSixenseInput->IsEnabled() && (owner->GetObserverMode()==OBS_MODE_NONE) && !UseVR() )
	{
		const float max_gun_pitch = 20.0f;

		float viewmodel_fov_ratio = g_pClientMode->GetViewModelFOV()/owner->GetFOV();
		QAngle gun_angles = g_pSixenseInput->GetViewAngleOffset() * -viewmodel_fov_ratio;

		// Clamp pitch a bit to minimize seeing back of viewmodel
		if( gun_angles[PITCH] < -max_gun_pitch )
		{ 
			gun_angles[PITCH] = -max_gun_pitch; 
		}

#ifdef WIN32 // ShouldFlipViewModel comes up unresolved on osx? Mabye because it's defined inline? fixme
		if( ShouldFlipViewModel() ) 
		{
			gun_angles[YAW] *= -1.0f;
		}
#endif

		vmangles = EyeAngles() +  gun_angles;

		SetLocalAngles( vmangles );
	}
#endif
#endif

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float g_fMaxViewModelLag = 1.5f;

void CBaseViewModel::CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& original_angles )
{
	Vector vOriginalOrigin = origin;
	QAngle vOriginalAngles = angles;

	// Calculate our drift
	Vector	forward;
	AngleVectors( angles, &forward, NULL, NULL );

	if ( gpGlobals->frametime != 0.0f )
	{
		Vector vDifference;
		VectorSubtract( forward, m_vecLastFacing, vDifference );

		float flSpeed = 5.0f;

#ifdef MAPBASE
		CBaseCombatWeapon *pWeapon = m_hWeapon.Get();
		if (pWeapon)
		{
			const FileWeaponInfo_t *pInfo = &pWeapon->GetWpnData();
			if (pInfo->m_flSwayScale != 1.0f)
			{
				vDifference *= pInfo->m_flSwayScale;
				pInfo->m_flSwayScale != 0.0f ? flSpeed /= pInfo->m_flSwayScale : flSpeed = 0.0f;
			}
			if (pInfo->m_flSwaySpeedScale != 1.0f)
			{
				flSpeed *= pInfo->m_flSwaySpeedScale;
			}
		}
#endif

		// If we start to lag too far behind, we'll increase the "catch up" speed.  Solves the problem with fast cl_yawspeed, m_yaw or joysticks
		//  rotating quickly.  The old code would slam lastfacing with origin causing the viewmodel to pop to a new position
		float flDiff = vDifference.Length();
		if ( (flDiff > g_fMaxViewModelLag) && (g_fMaxViewModelLag > 0.0f) )
		{
			float flScale = flDiff / g_fMaxViewModelLag;
			flSpeed *= flScale;
		}

		// FIXME:  Needs to be predictable?
		VectorMA( m_vecLastFacing, flSpeed * gpGlobals->frametime, vDifference, m_vecLastFacing );
		// Make sure it doesn't grow out of control!!!
		VectorNormalize( m_vecLastFacing );
		VectorMA( origin, 5.0f, vDifference * -1.0f, origin );

		Assert( m_vecLastFacing.IsValid() );
	}

	Vector right, up;
	AngleVectors( original_angles, &forward, &right, &up );

	float pitch = original_angles[ PITCH ];
	if ( pitch > 180.0f )
		pitch -= 360.0f;
	else if ( pitch < -180.0f )
		pitch += 360.0f;

	if ( g_fMaxViewModelLag == 0.0f )
	{
		origin = vOriginalOrigin;
		angles = vOriginalAngles;
	}

	//FIXME: These are the old settings that caused too many exposed polys on some models
	VectorMA( origin, -pitch * 0.035f,	forward,	origin );
	VectorMA( origin, -pitch * 0.03f,		right,	origin );
	VectorMA( origin, -pitch * 0.02f,		up,		origin);
}

//-----------------------------------------------------------------------------
// Stub to keep networking consistent for DEM files
//-----------------------------------------------------------------------------
#if defined( CLIENT_DLL )
  extern void RecvProxy_EffectFlags( const CRecvProxyData *pData, void *pStruct, void *pOut );
  void RecvProxy_SequenceNum( const CRecvProxyData *pData, void *pStruct, void *pOut );
#endif

//-----------------------------------------------------------------------------
// Purpose: Resets anim cycle when the server changes the weapon on us
//-----------------------------------------------------------------------------
#if defined( CLIENT_DLL )
static void RecvProxy_Weapon( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseViewModel *pViewModel = ((CBaseViewModel*)pStruct);
	CBaseCombatWeapon *pOldWeapon = pViewModel->GetOwningWeapon();

	// Chain through to the default recieve proxy ...
	RecvProxy_IntToEHandle( pData, pStruct, pOut );

	// ... and reset our cycle index if the server is switching weapons on us
	CBaseCombatWeapon *pNewWeapon = pViewModel->GetOwningWeapon();
	if ( pNewWeapon != pOldWeapon )
	{
		// Restart animation at frame 0
		pViewModel->SetCycle( 0 );
		pViewModel->m_flAnimTime = gpGlobals->curtime;
	}
}
#endif

// GESTURES: the play/stop triggers fire on a PARITY change (re-sending the same
// def/slot still re-plays). We do NOT act on them in a recv proxy -- proxies run
// during the net-update phase with abs queries disabled, and playing a gesture there
// spawns/parents the source entity, which asserts s_bAbsQueriesValid. Instead we latch
// the parity and compare it in OnDataChanged (runs after abs is re-enabled).


LINK_ENTITY_TO_CLASS( viewmodel, CBaseViewModel );

IMPLEMENT_NETWORKCLASS_ALIASED( BaseViewModel, DT_BaseViewModel )

BEGIN_NETWORK_TABLE_NOBASE(CBaseViewModel, DT_BaseViewModel)
#if !defined( CLIENT_DLL )
	SendPropModelIndex(SENDINFO(m_nModelIndex)),
	SendPropInt		(SENDINFO(m_nBody), 8),
	SendPropInt		(SENDINFO(m_nSkin), 10),
	SendPropInt		(SENDINFO(m_nSequence),	8, SPROP_UNSIGNED),
	SendPropInt		(SENDINFO(m_nViewModelIndex), VIEWMODEL_INDEX_BITS, SPROP_UNSIGNED),
	SendPropFloat	(SENDINFO(m_flPlaybackRate),	8,	SPROP_ROUNDUP,	-4.0,	12.0f),
	SendPropInt		(SENDINFO(m_fEffects),		10, SPROP_UNSIGNED),
	SendPropInt		(SENDINFO(m_nAnimationParity), 3, SPROP_UNSIGNED ),
	SendPropEHandle (SENDINFO(m_hWeapon)),
	SendPropEHandle (SENDINFO(m_hOwner)),

#ifdef FP
	// GESTURES: server->client triggers. Payload BEFORE its parity (the parity proxy
	// reads the payload). Stop slot is signed (-1 = stop all).
	SendPropInt( SENDINFO( m_iGesturePlayDef ),    16, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iGesturePlaySlot ),    4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nGesturePlayParity ),  VIEWMODEL_GESTURE_PARITY_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iGestureStopSlot ),    5 ),
	SendPropInt( SENDINFO( m_nGestureStopParity ),  VIEWMODEL_GESTURE_PARITY_BITS, SPROP_UNSIGNED ),
#endif // FP

	SendPropInt( SENDINFO( m_nNewSequenceParity ), EF_PARITY_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nResetEventsParity ), EF_PARITY_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nMuzzleFlashParity ), EF_MUZZLEFLASH_BITS, SPROP_UNSIGNED ),

#if !defined( INVASION_DLL ) && !defined( INVASION_CLIENT_DLL )
	SendPropArray	(SendPropFloat(SENDINFO_ARRAY(m_flPoseParameter),	8, 0, 0.0f, 1.0f), m_flPoseParameter),
#endif
#else
	RecvPropInt		(RECVINFO(m_nModelIndex)),
	RecvPropInt		(RECVINFO(m_nSkin)),
	RecvPropInt		(RECVINFO(m_nBody)),
	RecvPropInt		(RECVINFO(m_nSequence), 0, RecvProxy_SequenceNum ),
	RecvPropInt		(RECVINFO(m_nViewModelIndex)),
	RecvPropFloat	(RECVINFO(m_flPlaybackRate)),
	RecvPropInt		(RECVINFO(m_fEffects), 0, RecvProxy_EffectFlags ),
	RecvPropInt		(RECVINFO(m_nAnimationParity)),
	RecvPropEHandle (RECVINFO(m_hWeapon), RecvProxy_Weapon ),
	RecvPropEHandle (RECVINFO(m_hOwner)),

#ifdef FP
	// GESTURES: just latched here; OnDataChanged detects the parity change and plays
	// (deferred out of the net-update phase where abs queries are disabled).
	RecvPropInt( RECVINFO( m_iGesturePlayDef ) ),
	RecvPropInt( RECVINFO( m_iGesturePlaySlot ) ),
	RecvPropInt( RECVINFO( m_nGesturePlayParity ) ),
	RecvPropInt( RECVINFO( m_iGestureStopSlot ) ),
	RecvPropInt( RECVINFO( m_nGestureStopParity ) ),
#endif // FP

	RecvPropInt( RECVINFO( m_nNewSequenceParity )),
	RecvPropInt( RECVINFO( m_nResetEventsParity )),
	RecvPropInt( RECVINFO( m_nMuzzleFlashParity )),

#if !defined( INVASION_DLL ) && !defined( INVASION_CLIENT_DLL )
	RecvPropArray(RecvPropFloat(RECVINFO(m_flPoseParameter[0]) ), m_flPoseParameter ),
#endif
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL

BEGIN_PREDICTION_DATA( CBaseViewModel )

	// Networked
	DEFINE_PRED_FIELD( m_nModelIndex, FIELD_SHORT, FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX ),
	DEFINE_PRED_FIELD( m_nSkin, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nBody, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nViewModelIndex, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flPlaybackRate, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.125f ),
	DEFINE_PRED_FIELD( m_fEffects, FIELD_INTEGER, FTYPEDESC_INSENDTABLE | FTYPEDESC_OVERRIDE ),
	DEFINE_PRED_FIELD( m_nAnimationParity, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hWeapon, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flAnimTime, FIELD_FLOAT, 0 ),

	DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flTimeWeaponIdle, FIELD_FLOAT ),
	DEFINE_FIELD( m_Activity, FIELD_INTEGER ),
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),

END_PREDICTION_DATA()

void RecvProxy_SequenceNum( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseViewModel *model = (CBaseViewModel *)pStruct;
	if (pData->m_Value.m_Int != model->GetSequence())
	{
		MDLCACHE_CRITICAL_SECTION();

		model->SetSequence(pData->m_Value.m_Int);
		model->m_flAnimTime = gpGlobals->curtime;
		model->SetCycle(0);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseViewModel::LookupAttachment( const char *pAttachmentName )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->LookupAttachment( pAttachmentName );

	return BaseClass::LookupAttachment( pAttachmentName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachment( int number, matrix3x4_t &matrix )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachment( number, matrix );

	return BaseClass::GetAttachment( number, matrix );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachment( int number, Vector &origin )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachment( number, origin );

	return BaseClass::GetAttachment( number, origin );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachment( number, origin, angles );

	return BaseClass::GetAttachment( number, origin, angles );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachmentVelocity( number, originVel, angleVel );

	return BaseClass::GetAttachmentVelocity( number, originVel, angleVel );
}

#ifdef FP
// ====================== GESTURES ======================
int CBaseViewModel::PlayGesture(const char* seqName, float speed, float peak,
	float speedIn, float speedOut, float curve,
	float startCycle, bool loop, float fadeOut, int forceSlot)
{
	int seq = LookupSequence(seqName);
	if (seq < 0)
	{
		DevWarning("CBaseViewModel: no sequence '%s' on current VM model\n", seqName);
		return -1;
	}

	// forceSlot >= 0 pins that exact channel (retiring what's there); < 0 = first free slot.
	const bool bForced = (forceSlot >= 0 && forceSlot < MAX_VM_GESTURES);
	if (bForced)
		RetireGesture(forceSlot);

	for (int i = 0; i < MAX_VM_GESTURES; ++i)
	{
		if (bForced && i != forceSlot)
			continue;
		if (m_Gestures[i].active)
			continue;
		vmgesture_t& g = m_Gestures[i];
		g.sequence = seq;
		g.pSource = NULL;
		g.modelIndex = GetModelIndex();
		g.startTime = gpGlobals->curtime;
		g.cycleStartTime = gpGlobals->curtime;
		g.startCycle = startCycle;
		g.speed = speed;
		g.peakOffset = peak;
		g.speedIn = speedIn;
		g.speedOut = speedOut;
		g.curve = curve;
		g.fadeOutDur = fadeOut;
		g.fadeOutStart = -1.0f;
		g.loop = loop;
		g.active = true;
		g.nextSeq[0] = '\0';   // fresh slot: no follow-up queued
		g.nextLoop = false;
		return i;
	}
	return -1;
}

int CBaseViewModel::PlayGestureFromModel(const char* modelName, const char* seqName,
	float speed, float peak, float speedIn,
	float speedOut, float curve, float startCycle, bool loop, float fadeOut, int forceSlot)
{
	C_AttachmentRenderable* pSrc = new C_AttachmentRenderable;
	if (!pSrc->InitializeAsClientEntity(modelName, RENDER_GROUP_VIEW_MODEL_OPAQUE))
	{
		pSrc->Release();
		return -1;
	}
	pSrc->SetParent(this);                 // view-space, glued to the VM (bob/sway inherited)
	pSrc->SetLocalOrigin(vec3_origin);
	pSrc->SetLocalAngles(vec3_angle);
	pSrc->SetPlaybackRate(0.0f);           // we drive its cycle

	int seq = pSrc->LookupSequence(seqName);
	if (seq < 0)
	{
		DevWarning("PlayGestureFromModel: no sequence '%s' in %s\n", seqName, modelName);
		pSrc->Remove();
		return -1;
	}

	// forceSlot >= 0 pins that exact channel (retiring what's there); < 0 = first free slot.
	const bool bForced = (forceSlot >= 0 && forceSlot < MAX_VM_GESTURES);
	if (bForced)
		RetireGesture(forceSlot);

	for (int i = 0; i < MAX_VM_GESTURES; ++i)
	{
		if (bForced && i != forceSlot) continue;
		if (m_Gestures[i].active) continue;
		vmgesture_t& g = m_Gestures[i];
		g.pSource = pSrc;  g.sequence = seq;  g.modelIndex = -1;
		g.startTime = gpGlobals->curtime;  g.cycleStartTime = gpGlobals->curtime;
		g.startCycle = startCycle;
		g.speed = speed;  g.peakOffset = peak;  g.speedIn = speedIn;
		g.speedOut = speedOut;  g.curve = curve;  g.loop = loop;  g.active = true;
		g.fadeOutDur = fadeOut;  g.fadeOutStart = -1.0f;
		g.nextSeq[0] = '\0';   // fresh slot: no follow-up queued
		g.nextLoop = false;
		pSrc->ResetSequence(seq);
		pSrc->SetCycle(startCycle);
		return i;
	}
	pSrc->Remove();   // no free slot
	return -1;
}

void CBaseViewModel::ApplyGestureFromModel(C_BaseAnimating* pSource, int seq, float cycle,
	float weight, float currentTime,
	Vector pos[], Quaternion q[])
{
	CStudioHdr* srcHdr = pSource->GetModelPtr();
	if (!srcHdr)
		return;

	Vector     srcPos[MAXSTUDIOBONES];
	Quaternion srcQ[MAXSTUDIOBONES];
	float      srcPP[MAXSTUDIOPOSEPARAM];
	pSource->GetPoseParameters(srcHdr, srcPP);

	IBoneSetup srcSetup(srcHdr, BONE_USED_BY_ANYTHING, srcPP);
	srcSetup.InitPose(srcPos, srcQ);
	srcSetup.AccumulatePose(srcPos, srcQ, seq, cycle, 1.0f, currentTime, NULL);

	// Per-bone weightlist for this sequence; weightlistindex 0 = none -> drive all bones.
	mstudioseqdesc_t& seqdesc = srcHdr->pSeqdesc(seq);
	const bool bHasWeightlist = (seqdesc.weightlistindex != 0);

	for (int sb = 0; sb < srcHdr->numbones(); ++sb)
	{
		int vb = LookupBone(srcHdr->pBone(sb)->pszName());
		if (vb < 0)
			continue;   // bone not on the viewmodel

		float wl = bHasWeightlist ? seqdesc.weight(sb) : 1.0f;
		if (wl <= 0.0f)
			continue;   // weight 0 -> keep the VM's own pose for this bone

		float boneW = weight * wl;
		if (boneW <= 0.0f)
			continue;

		QuaternionSlerp(q[vb], srcQ[sb], boneW, q[vb]);
		pos[vb] = pos[vb] + (srcPos[sb] - pos[vb]) * boneW;
	}
}

bool CBaseViewModel::IsGestureActive(int slot) const
{
	return (slot >= 0 && slot < MAX_VM_GESTURES) ? m_Gestures[slot].active : false;
}

C_BaseAnimating* CBaseViewModel::GetGestureSourceModel(int slot) const
{
	if (slot < 0 || slot >= MAX_VM_GESTURES || !m_Gestures[slot].active)
		return NULL;
	return m_Gestures[slot].pSource;   // NULL for the layer path (no separate model)
}

// Play a registry def into 'slot' (-1 = first free) and auto-queue its szNext follow-up.
// Client executor behind both the networked play trigger and PlayGestureByName.
int CBaseViewModel::PlayGestureDefIndex(unsigned short defIndex, int slot)
{
	const GestureDef_t* pDef = CGestureRegistry::Instance().FindByIndex(defIndex);
	if (!pDef)
	{
		DevWarning("PlayGestureDefIndex: bad def index %d\n", defIndex);
		return -1;
	}

	int played = -1;

	// If the slot already runs this def's model, swap the sequence in place (RepointGesture)
	// instead of respawning -- keeps the held pose continuous (idle->pulldown, on->off->on).
	if (pDef->UsesModel() && slot >= 0 && slot < MAX_VM_GESTURES
		&& m_Gestures[slot].active && m_Gestures[slot].pSource)
	{
		// Match by model INDEX: a client-created source has an empty GetModelName().
		int slotModelIdx = m_Gestures[slot].pSource->GetModelIndex();
		if (slotModelIdx > 0 && slotModelIdx == modelinfo->GetModelIndex(pDef->szModel))
		{
			vmgesture_t& g = m_Gestures[slot];
			g.speed = pDef->speed;     g.peakOffset = pDef->peak;
			g.speedIn = pDef->speedIn; g.speedOut = pDef->speedOut; g.curve = pDef->curve;
			g.fadeOutDur = pDef->fadeOut;
			if (RepointGesture(slot, pDef->szSequence, pDef->bLoop, gpGlobals->curtime))
				played = slot;
		}
	}

	if (played < 0)
	{
		played = pDef->UsesModel()
			? PlayGestureFromModel(pDef->szModel, pDef->szSequence,
				pDef->speed, pDef->peak, pDef->speedIn, pDef->speedOut,
				pDef->curve, pDef->startCycle, pDef->bLoop, pDef->fadeOut, slot)
			: PlayGesture(pDef->szSequence,
				pDef->speed, pDef->peak, pDef->speedIn, pDef->speedOut,
				pDef->curve, pDef->startCycle, pDef->bLoop, pDef->fadeOut, slot);
	}

	if (played < 0)
		return -1;

	// A play replaces the slot's follow-up with this def's own: clear any inherited queue
	// (the reuse path keeps it), then re-queue below only if this def has a next. Otherwise
	// an interrupting pulldown would still advance to the previously-queued idle.
	m_Gestures[played].nextSeq[0] = '\0';
	m_Gestures[played].nextLoop  = false;

	// Auto-queue the follow-up def (pullout -> idle); its loop flag is the next def's bLoop.
	if (pDef->szNext[0])
	{
		const GestureDef_t* pNext = CGestureRegistry::Instance().FindByName(pDef->szNext);
		if (pNext)
			QueueGestureNext(played, pNext->szSequence, pNext->bLoop);
		else
			DevWarning("PlayGestureDefIndex: '%s' names a missing next def '%s'\n",
				pDef->szName, pDef->szNext);
	}

	return played;
}

int CBaseViewModel::PlayGestureLocal(const char* pszGestureName, int slot)
{
	CGestureRegistry::Instance().EnsureLoaded();
	unsigned short idx = CGestureRegistry::Instance().FindIndexByName(pszGestureName);
	if (idx == INVALID_GESTURE_DEF_INDEX)
	{
		DevWarning("PlayGestureLocal: no gesture def '%s'\n", pszGestureName ? pszGestureName : "(null)");
		return -1;
	}
	return PlayGestureDefIndex(idx, slot);
}

// Client side of the server play/stop triggers (called from OnDataChanged).
void CBaseViewModel::OnGesturePlayParityChanged(void)
{
	PlayGestureDefIndex((unsigned short)m_iGesturePlayDef, m_iGesturePlaySlot);
}

void CBaseViewModel::OnGestureStopParityChanged(void)
{
	if (m_iGestureStopSlot < 0)
		StopAllGestures();
	else
		StopGesture(m_iGestureStopSlot);
}

float CBaseViewModel::ComputeGestureWeight(const vmgesture_t& g, float now)
{
	// Ramp in over the first fraction of a second, then hold at full weight.
	float t = (now - g.startTime) * 7.0f * g.speedIn;
	float m = clamp(1.0f - t, 0.0f, 1.0f);
	return 1.0f - powf(m, g.curve);
}

float CBaseViewModel::ComputeGestureCycle(const vmgesture_t& g, float now)
{
	C_BaseAnimating* src = g.pSource ? g.pSource : this; 
	CStudioHdr* hdr = src->GetModelPtr();
	float rate = hdr ? src->GetSequenceCycleRate(hdr, g.sequence) : 1.0f;

	float c = g.startCycle + (now - g.cycleStartTime) * rate * g.speed;
	if (g.loop)  
		c -= floorf(c);
	else
		c = clamp(c, 0.0f, 1.0f);
	return c;
}

void CBaseViewModel::RetireGesture(int slot)
{
	vmgesture_t& g = m_Gestures[slot];
	g.active = false;
	g.nextSeq[0] = '\0';   // clear queue so a recycled slot doesn't inherit it
	g.nextLoop = false;
	g.fadeOutStart = -1.0f;
	if (g.pSource)
	{
		// Don't delete synchronously: RetireGesture can run from StandardBlendingRules while
		// the engine walks the view-model render list, and the source is in that list -- an
		// immediate Remove() would dangle a list entry and crash. Hide it and let it remove
		// itself on its next client think (sim phase, after the walk); see
		// C_AttachmentRenderable::ClientThink.
		g.pSource->AddEffects( EF_NODRAW );
		g.pSource->SetNextClientThink( gpGlobals->curtime );
		g.pSource = NULL;
	}
}

// Queue one follow-up sequence (by name) on a live slot. No-op if the slot isn't active.
void CBaseViewModel::QueueGestureNext(int slot, const char* seqName, bool loop)
{
	if (slot < 0 || slot >= MAX_VM_GESTURES)
		return;
	vmgesture_t& g = m_Gestures[slot];
	if (!g.active || !seqName || !*seqName)
		return;

	// A loop never reaches cycle>=1 to consume a queue, so interrupt it now (repoint in
	// place). One-shots defer: the queue is consumed when the animation finishes.
	if (g.loop)
	{
		if (!RepointGesture(slot, seqName, loop, gpGlobals->curtime))
			RetireGesture(slot);   // bad sequence name -> end the gesture
		return;
	}

	V_strncpy(g.nextSeq, seqName, sizeof(g.nextSeq));
	g.nextLoop = loop;
}

// In-place sequence swap on the slot's SAME model: restarts the cycle clock but preserves
// the weight clock (no dip). Reuses pSource (ResetSequence, no Remove). Doesn't touch the queue.
bool CBaseViewModel::RepointGesture(int slot, const char* seqName, bool loop, float now)
{
	vmgesture_t& g = m_Gestures[slot];

	C_BaseAnimating* src = g.pSource ? g.pSource : this;   // foreign reuses pSource, layer uses the VM
	int seq = src->LookupSequence(seqName);
	if (seq < 0)
	{
		DevWarning("RepointGesture: no sequence '%s' on %s\n", seqName, src->GetModelName());
		return false;
	}

	g.sequence = seq;
	g.loop = loop;
	g.startCycle = 0.0f;
	g.cycleStartTime = now;     // restart cycle; startTime (weight clock) untouched
	g.fadeOutStart = -1.0f;

	if (g.pSource)
	{
		g.pSource->ResetSequence(seq);
		g.pSource->SetCycle(0.0f);
	}
	return true;
}

// Consume the one-deep queue when a one-shot finishes: repoint to nextSeq and clear it.
// Returns false if nothing valid is queued (caller then retires the slot).
bool CBaseViewModel::AdvanceGestureToNext(int slot, float now)
{
	vmgesture_t& g = m_Gestures[slot];
	if (!g.nextSeq[0])
		return false;

	bool ok = RepointGesture(slot, g.nextSeq, g.nextLoop, now);
	g.nextSeq[0] = '\0';
	g.nextLoop = false;
	return ok;
}

void CBaseViewModel::StandardBlendingRules(CStudioHdr* hdr, Vector pos[],
	Quaternion q[], float currentTime,
	int boneMask)
{
	// Normal weapon viewmodel pose first.
	BaseClass::StandardBlendingRules(hdr, pos, q, currentTime, boneMask);

	if (!hdr || !hdr->SequencesAvailable())
		return;

	float poseParams[MAXSTUDIOPOSEPARAM];
	GetPoseParameters(hdr, poseParams);
	IBoneSetup boneSetup(hdr, boneMask, poseParams);

	for (int i = 0; i < MAX_VM_GESTURES; ++i)
	{
		vmgesture_t& g = m_Gestures[i];
		if (!g.active) continue;

		// validity: local checks the VM model; foreign checks the source model
		if (g.pSource == NULL)
		{
			if (g.modelIndex != GetModelIndex() || g.sequence < 0 || g.sequence >= hdr->GetNumSeq())
			{
				RetireGesture(i); 
				continue;
			}
		}
		else
		{
			CStudioHdr* srcHdr = g.pSource->GetModelPtr();
			if (!srcHdr || !srcHdr->SequencesAvailable() || g.sequence < 0 || g.sequence >= srcHdr->GetNumSeq())
			{
				RetireGesture(i); 
				continue;
			}
		}

		float weight = ComputeGestureWeight(g, currentTime);
		float cycle = ComputeGestureCycle(g, currentTime);

		const bool pastPeak = (currentTime >= g.startTime + g.peakOffset);

		if (g.loop)
		{
			// loop has no natural end -> only a deliberate fade-out retires it
			if (pastPeak && weight <= 0.0f) { RetireGesture(i); continue; }
		}
		else if (cycle >= 1.0f)
		{
			// One-shot hit its last frame: 1) advance to a queued follow-up (no gap), else
			// 2) fade weight 1->0 so the arm lerps back to the live weapon pose, else 3) retire.
			if (AdvanceGestureToNext(i, currentTime))
			{
				cycle = ComputeGestureCycle(g, currentTime);
				weight = ComputeGestureWeight(g, currentTime);
			}
			else if (g.fadeOutDur > 0.0f)
			{
				if (g.fadeOutStart < 0.0f)
					g.fadeOutStart = currentTime;

				const float fadeT = (currentTime - g.fadeOutStart) / g.fadeOutDur;
				if (fadeT >= 1.0f) { RetireGesture(i); continue; }

				weight *= (1.0f - fadeT);   // cycle is clamped to 1 -> last frame held
			}
			else
			{
				RetireGesture(i);
				continue;
			}
		}
		if (weight <= 0.0f) continue;   // skip a zero-weight frame without retiring

		// ---- apply the gesture pose ----
		if (g.pSource == NULL)
		{
			// local (layer) gesture sequence lives in the weapon's own model
			boneSetup.AccumulatePose(pos, q, g.sequence, cycle, weight, currentTime, NULL);
		}
		else
		{
			// separate-model gesture evaluate source, transfer bones (weightlist-masked)
			g.pSource->SetCycle(cycle);   // keep its rendered mesh's frame in sync with the pose
			ApplyGestureFromModel(g.pSource, g.sequence, cycle, weight, currentTime, pos, q);
		}
	}
}
#endif // FP
#endif

#ifdef MAPBASE
#if defined( CLIENT_DLL )
#define CHandViewModel C_HandViewModel
#endif

// ---------------------------------------
// OzxyBox's hand viewmodel code.
// All credit goes to him.
// ---------------------------------------
class CHandViewModel : public CBaseViewModel
{
	DECLARE_CLASS( CHandViewModel, CBaseViewModel );
public:
	DECLARE_NETWORKCLASS();

	CBaseViewModel	*GetVMOwner();

	CBaseCombatWeapon *GetOwningWeapon( void );

private:
	CHandle<CBaseViewModel> m_hVMOwner;
};

LINK_ENTITY_TO_CLASS(hand_viewmodel, CHandViewModel);
IMPLEMENT_NETWORKCLASS_ALIASED(HandViewModel, DT_HandViewModel)

// for whatever reason the parent doesn't get sent 
// I don't really want to mess with the baseviewmodel
// so now it does
BEGIN_NETWORK_TABLE(CHandViewModel, DT_HandViewModel)
#ifndef CLIENT_DLL
	SendPropEHandle(SENDINFO_NAME(m_hMoveParent, moveparent)),
#else
	RecvPropInt(RECVINFO_NAME(m_hNetworkMoveParent, moveparent), 0, RecvProxy_IntToMoveParent),
#endif
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseViewModel *CHandViewModel::GetVMOwner()
{
	if (!m_hVMOwner)
		m_hVMOwner = assert_cast<CBaseViewModel*>(GetMoveParent());
	return m_hVMOwner;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CHandViewModel::GetOwningWeapon()
{
	CBaseViewModel *pVM = GetVMOwner();
	if (pVM)
		return pVM->GetOwningWeapon();
	else
		return NULL;
}
#endif

#ifdef FP
void CBaseViewModel::CalcIronsights(Vector& pos, QAngle& ang)
{
	CBaseModularWeapon* pWeapon = ToModularWeapon(GetOwningWeapon());

	if (!pWeapon)
		return;

	//get delta time for interpolation
	float delta = (gpGlobals->curtime - pWeapon->m_flIronsightedTime) * 2.5f; //modify this value to adjust how fast the interpolation is
	float exp = (pWeapon->IsIronsighted()) ?
		(delta > 1.0f) ? 1.0f : delta : //normal blending
		(delta > 1.0f) ? 0.0f : 1.0f - delta; //reverse interpolation

	if (exp <= 0.001f) //fully not ironsighted; save performance
		return;

	Vector newPos = pos;
	QAngle newAng = ang;

	Vector vForward, vRight, vUp, vOffset;
	AngleVectors(newAng, &vForward, &vRight, &vUp);
	vOffset = pWeapon->GetIronsightPositionOffset();

	newPos += vForward * vOffset.x;
	newPos += vRight * vOffset.y;
	newPos += vUp * vOffset.z;
	newAng += pWeapon->GetIronsightAngleOffset();
	//fov is handled by CBaseCombatWeapon

	pos += (newPos - pos) * exp;
	ang += (newAng - ang) * exp;
}

// ====================== GESTURES: server-callable front door ======================
// Gestures run on the client. The server just nets a parity-tagged trigger (handled in
// OnDataChanged); the client runs the call directly. Cosmetic only -- don't call from
// unguarded predicted code (it would net AND run locally).
void CBaseViewModel::PlayGestureByName(const char* pszGestureName, int slot)
{
	CGestureRegistry::Instance().EnsureLoaded();
	unsigned short idx = CGestureRegistry::Instance().FindIndexByName(pszGestureName);
	if (idx == INVALID_GESTURE_DEF_INDEX)
	{
		DevWarning("PlayGestureByName: no gesture def '%s'\n", pszGestureName ? pszGestureName : "(null)");
		return;
	}

#ifdef CLIENT_DLL
	PlayGestureDefIndex(idx, slot);                 // client: run it now
#else
	m_iGesturePlayDef  = idx;                       // server: net it (payload, then parity)
	m_iGesturePlaySlot = slot;
	m_nGesturePlayParity = (m_nGesturePlayParity + 1) & ((1 << VIEWMODEL_GESTURE_PARITY_BITS) - 1);
#endif
}

void CBaseViewModel::StopGesture(int slot)
{
#ifdef CLIENT_DLL
	if (slot >= 0 && slot < MAX_VM_GESTURES)
		RetireGesture(slot);                        // client: retire it now
#else
	m_iGestureStopSlot = slot;                      // server: net it
	m_nGestureStopParity = (m_nGestureStopParity + 1) & ((1 << VIEWMODEL_GESTURE_PARITY_BITS) - 1);
#endif
}

void CBaseViewModel::StopAllGestures(void)
{
#ifdef CLIENT_DLL
	for (int i = 0; i < MAX_VM_GESTURES; ++i)
		RetireGesture(i);                           // client: retire everything now
#else
	m_iGestureStopSlot = -1;                        // server: -1 = stop all
	m_nGestureStopParity = (m_nGestureStopParity + 1) & ((1 << VIEWMODEL_GESTURE_PARITY_BITS) - 1);
#endif
}

#ifdef CLIENT_DLL
CON_COMMAND_F(vm_testgesturemodel, "vm_testgesturemodel <model> <seq> [loop] [speedIn] [peak] [speedOut] [curve] [startCycle] [fadeOut]", FCVAR_CHEAT)
{
	if (args.ArgC() < 3) { Msg("usage: <model> <seq> [loop] [speedIn] [peak] [speedOut] [curve] [startCycle] [fadeOut]\n"); return; }
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer) return;
	C_BaseViewModel* pVM = pPlayer->GetViewModel();
	if (!pVM) return;

	bool  loop = (args.ArgC() >= 4) ? atoi(args.Arg(3)) != 0 : false;
	float speedIn = (args.ArgC() >= 5) ? atof(args.Arg(4)) : 1.0f;
	float peak = (args.ArgC() >= 6) ? atof(args.Arg(5)) : 0.4f;
	float speedOut = (args.ArgC() >= 7) ? atof(args.Arg(6)) : 1.0f;
	float curve = (args.ArgC() >= 8) ? atof(args.Arg(7)) : 1.0f;
	float startCyc = (args.ArgC() >= 9) ? atof(args.Arg(8)) : 0.0f;
	float fadeOut = (args.ArgC() >= 10) ? atof(args.Arg(9)) : 0.0f;

	int slot = pVM->PlayGestureFromModel(args.Arg(1), args.Arg(2),
		1.0f /*speed*/, peak, speedIn, speedOut, curve, startCyc, loop, fadeOut);
	Msg("slot %d  speedIn=%.2f peak=%.2f speedOut=%.2f curve=%.2f startCycle=%.2f loop=%d fadeOut=%.2f\n",
		slot, speedIn, peak, speedOut, curve, startCyc, loop, fadeOut);
}

CON_COMMAND_F(vm_testgesturechain, "vm_testgesturechain <model> <seq1_oneshot> <seq2_loop> -- play seq1 then auto-advance to looping seq2 on the SAME source model (pullout->idle test)", FCVAR_CHEAT)
{
	if (args.ArgC() < 4) { Msg("usage: <model> <seq1_oneshot> <seq2_loop>\n"); return; }
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer) return;
	C_BaseViewModel* pVM = pPlayer->GetViewModel();
	if (!pVM) { Msg("no viewmodel\n"); return; }

	int slot = pVM->PlayGestureFromModel(args.Arg(1), args.Arg(2),
		1.0f /*speed*/, 0.4f /*peak*/, 1.0f /*speedIn*/, 1.0f /*speedOut*/,
		1.0f /*curve*/, 0.0f /*startCycle*/, false /*one-shot*/);
	if (slot < 0) { Msg("play failed\n"); return; }

	pVM->QueueGestureNext(slot, args.Arg(3), true /*loop the idle*/);
	Msg("slot %d: '%s' (one-shot) -> '%s' (loop)\n", slot, args.Arg(2), args.Arg(3));
}

CON_COMMAND_F(vm_testgestureinterrupt, "vm_testgestureinterrupt <slot> <oneshot_seq> [return_loop_seq] -- insta-interrupt a looping gesture on <slot> with a one-shot, optionally returning to a loop", FCVAR_CHEAT)
{
	if (args.ArgC() < 3) { Msg("usage: <slot> <oneshot_seq> [return_loop_seq]\n"); return; }
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer) return;
	C_BaseViewModel* pVM = pPlayer->GetViewModel();
	if (!pVM) { Msg("no viewmodel\n"); return; }

	int slot = atoi(args.Arg(1));
	if (!pVM->IsGestureActive(slot)) { Msg("slot %d not active\n", slot); return; }

	pVM->QueueGestureNext(slot, args.Arg(2), false);    // loop -> insta-stop, play one-shot NOW
	if (args.ArgC() >= 4)
		pVM->QueueGestureNext(slot, args.Arg(3), true); // one-shot ends -> back to the loop

	Msg("slot %d: interrupt -> '%s'%s\n", slot, args.Arg(2),
		args.ArgC() >= 4 ? " -> (loop) again" : "");
}

CON_COMMAND_F( vm_testgesture, "Play a viewmodel gesture by sequence name", FCVAR_CHEAT )
{
	if (args.ArgC() < 2)
	{
		Msg("usage: vm_testgesture <sequenceName> [holdSeconds]\n");
		return;
	}

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer) return;

	C_BaseViewModel* pVM = pPlayer->GetViewModel();
	if (!pVM) { Msg("no viewmodel\n"); return; }

	float hold = (args.ArgC() >= 3) ? atof(args.Arg(2)) : 0.4f;

	int slot = pVM->PlayGesture(args.Arg(1), 1.0f, hold);
	Msg("PlayGesture('%s', peak=%.2f) -> slot %d\n", args.Arg(1), hold, slot);
}

CON_COMMAND_F( vm_listseq, "List sequences on the current viewmodel", FCVAR_CHEAT )
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer) return;

	C_BaseViewModel* pVM = pPlayer->GetViewModel();
	if (!pVM) { Msg("no viewmodel\n"); return; }

	CStudioHdr* hdr = pVM->GetModelPtr();
	if (!hdr) { Msg("no studiohdr\n"); return; }

	Msg("%d sequences on %s:\n", hdr->GetNumSeq(), pVM->GetModelName());
	for (int i = 0; i < hdr->GetNumSeq(); ++i)
		Msg("  %2d  %s\n", i, hdr->pSeqdesc(i).pszLabel());
}
#endif // CLIENT_DLL

#endif // FP

