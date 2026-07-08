//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sunlight + cascaded shadow map control entity (client side).
//          Receives the networked sun direction / color / enable state; the
//          client CSM manager reads it through C_CascadeLight::Get().
//
//=============================================================================//
#ifndef C_ENV_CASCADE_LIGHT_H
#define C_ENV_CASCADE_LIGHT_H

#ifdef _WIN32
#pragma once
#endif

#include "c_baseentity.h"

class CViewSetup;

//------------------------------------------------------------------------------
// Renders the sun-shadow cascade depth textures for the given main view.
// Called from viewrender.cpp at the same point flashlight shadow depths render.
//------------------------------------------------------------------------------
void CSM_ComputeShadowDepthTextures( const CViewSetup &viewSetup );

//------------------------------------------------------------------------------
// Purpose: Directional sunlight + cascaded shadow map control entity.
//------------------------------------------------------------------------------
class C_CascadeLight : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_CascadeLight, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_CascadeLight();
	virtual ~C_CascadeLight();

	// The single active cascade light, or NULL if none is present in the map.
	static C_CascadeLight *Get() { return m_pCascadeLight; }

	virtual bool ShouldDraw() { return false; }

	const Vector &GetShadowDirection() const { return m_shadowDirection; }
	bool IsEnabled() const { return m_bEnabled; }
	color32 GetColor() const { return m_LightColor; }
	int GetColorScale() const { return m_LightColorScale; }
	float GetMaxShadowDist() const { return m_flMaxShadowDist; }

private:
	static C_CascadeLight *m_pCascadeLight;

	Vector	m_shadowDirection;
	bool	m_bEnabled;
	color32	m_LightColor;
	int		m_LightColorScale;
	float	m_flMaxShadowDist;
};

#endif // C_ENV_CASCADE_LIGHT_H
