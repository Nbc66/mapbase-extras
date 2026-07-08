//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sunlight + cascaded shadow map control entity (server side).
//          Networks the sun direction / color / enable state to the client,
//          where CCascadeLightManager consumes it to build the CSM cascades.
//
//=============================================================================//
#ifndef ENV_CASCADE_LIGHT_H
#define ENV_CASCADE_LIGHT_H

#ifdef _WIN32
#pragma once
#endif

//------------------------------------------------------------------------------
// Purpose : Directional sunlight + cascaded shadow map control entity.
//------------------------------------------------------------------------------
class CCascadeLight : public CBaseEntity
{
public:
	DECLARE_CLASS( CCascadeLight, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CCascadeLight();
	virtual ~CCascadeLight();

	void Spawn( void );
	bool KeyValue( const char *szKeyName, const char *szValue );
	virtual bool GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen );
	int  UpdateTransmitState();

	inline const Vector &GetShadowDirection() const { return m_shadowDirection.Get(); }

	// Inputs
	void	InputSetAngles( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );
	void	InputSetLightColor( inputdata_t &inputdata );
	void	InputSetLightColorScale( inputdata_t &inputdata );
	void	InputSetMaxShadowDistance( inputdata_t &inputdata );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	void SetEnabled( bool bEnable );

private:
	CNetworkVector( m_shadowDirection );
	CNetworkVar( bool, m_bEnabled );
	bool m_bStartDisabled;

	CNetworkColor32( m_LightColor );
	CNetworkVar( int, m_LightColorScale );
	CNetworkVar( float, m_flMaxShadowDist );
};

extern CCascadeLight *g_pCascadeLight;

#endif // ENV_CASCADE_LIGHT_H
