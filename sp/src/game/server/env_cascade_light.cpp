//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sunlight + cascaded shadow map control entity (server side).
//
//=============================================================================//

#include "cbase.h"
#include "env_cascade_light.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CCascadeLight *g_pCascadeLight;

LINK_ENTITY_TO_CLASS( env_cascade_light, CCascadeLight );

BEGIN_DATADESC( CCascadeLight )

	DEFINE_KEYFIELD( m_bEnabled,		FIELD_BOOLEAN,	"enabled" ),
	DEFINE_KEYFIELD( m_bStartDisabled,	FIELD_BOOLEAN,	"StartDisabled" ),
	DEFINE_KEYFIELD( m_LightColorScale,	FIELD_INTEGER,	"lightcolorscale" ),
	DEFINE_KEYFIELD( m_flMaxShadowDist,	FIELD_FLOAT,	"maxshadowdistance" ),
	DEFINE_FIELD( m_shadowDirection,	FIELD_VECTOR ),
	DEFINE_FIELD( m_LightColor,			FIELD_COLOR32 ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_COLOR32,	"LightColor",			InputSetLightColor ),
	DEFINE_INPUTFUNC( FIELD_INTEGER,	"LightColorScale",		InputSetLightColorScale ),
	DEFINE_INPUTFUNC( FIELD_FLOAT,		"SetMaxShadowDistance",	InputSetMaxShadowDistance ),
	DEFINE_INPUTFUNC( FIELD_STRING,		"SetAngles",			InputSetAngles ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"Enable",				InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"Disable",				InputDisable ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST_NOBASE( CCascadeLight, DT_CascadeLight )
	SendPropVector( SENDINFO( m_shadowDirection ), -1, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bEnabled ) ),
	SendPropInt( SENDINFO( m_LightColor ), 32, SPROP_UNSIGNED, SendProxy_Color32ToInt ),
	SendPropInt( SENDINFO( m_LightColorScale ), 32, 0 ),
	SendPropFloat( SENDINFO( m_flMaxShadowDist ), 0, SPROP_NOSCALE ),
END_SEND_TABLE()

CCascadeLight::CCascadeLight()
{
	m_bEnabled = true;
	m_bStartDisabled = false;

	m_LightColor.Init( 255, 255, 255, 255 );
	m_LightColorScale = 255;

	// Default sun angle until overridden by the
	// "angles" keyvalue or the SetAngles input.
	QAngle angles( 50, 43, 0 );
	Vector vForward;
	AngleVectors( angles, &vForward );
	m_shadowDirection = vForward;

	m_flMaxShadowDist = 400.0f;

	g_pCascadeLight = this;
}

CCascadeLight::~CCascadeLight()
{
	if ( g_pCascadeLight == this )
		g_pCascadeLight = NULL;
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model.
//------------------------------------------------------------------------------
int CCascadeLight::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

bool CCascadeLight::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "angles" ) )
	{
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );
		if ( angles == vec3_angle )
		{
			angles.Init( 50, 43, 0 );
		}
		Vector vForward;
		AngleVectors( angles, &vForward );
		m_shadowDirection = vForward;
		return true;
	}
	else if ( FStrEq( szKeyName, "lightcolor" ) || FStrEq( szKeyName, "color" ) )
	{
		float tmp[4];
		UTIL_StringToFloatArray( tmp, 4, szValue );
		m_LightColor.SetR( (byte)tmp[0] );
		m_LightColor.SetG( (byte)tmp[1] );
		m_LightColor.SetB( (byte)tmp[2] );
		m_LightColor.SetA( 255 );
		// The 4th component of a light color is the brightness scale, not alpha.
		if ( tmp[3] > 0.0f )
			m_LightColorScale = (int)tmp[3];
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

bool CCascadeLight::GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		Q_snprintf( szValue, iMaxLen, "%d %d %d %d", m_LightColor.GetR(), m_LightColor.GetG(), m_LightColor.GetB(), m_LightColorScale.Get() );
		return true;
	}
	return BaseClass::GetKeyValue( szKeyName, szValue, iMaxLen );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CCascadeLight::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );

	m_bEnabled = !m_bStartDisabled;

	BaseClass::Spawn();
}

void CCascadeLight::SetEnabled( bool bEnable )
{
	m_bEnabled = bEnable;
}

//------------------------------------------------------------------------------
// Input handlers
//------------------------------------------------------------------------------
void CCascadeLight::InputSetAngles( inputdata_t &inputdata )
{
	QAngle angles;
	UTIL_StringToVector( angles.Base(), inputdata.value.String() );

	Vector vTemp;
	AngleVectors( angles, &vTemp );
	m_shadowDirection = vTemp;
}

void CCascadeLight::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

void CCascadeLight::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}

void CCascadeLight::InputSetLightColor( inputdata_t &inputdata )
{
	m_LightColor = inputdata.value.Color32();
}

void CCascadeLight::InputSetLightColorScale( inputdata_t &inputdata )
{
	m_LightColorScale = inputdata.value.Int();
}

void CCascadeLight::InputSetMaxShadowDistance( inputdata_t &inputdata )
{
	m_flMaxShadowDist = inputdata.value.Float();
}
