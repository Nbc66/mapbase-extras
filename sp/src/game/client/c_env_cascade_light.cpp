//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sunlight + cascaded shadow map control entity (client side).
//
//=============================================================================//

#include "cbase.h"
#include "c_env_cascade_light.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "igamesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "renderparm.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

C_CascadeLight *C_CascadeLight::m_pCascadeLight = NULL;

IMPLEMENT_CLIENTCLASS_DT( C_CascadeLight, DT_CascadeLight, CCascadeLight )
	RecvPropVector( RECVINFO( m_shadowDirection ) ),
	RecvPropBool( RECVINFO( m_bEnabled ) ),
	RecvPropInt( RECVINFO( m_LightColor ), 0, RecvProxy_IntToColor32 ),
	RecvPropInt( RECVINFO( m_LightColorScale ) ),
	RecvPropFloat( RECVINFO( m_flMaxShadowDist ) ),
END_RECV_TABLE()

C_CascadeLight::C_CascadeLight()
{
	m_pCascadeLight = this;

	m_shadowDirection.Init( 0.0f, 0.0f, -1.0f );
	m_bEnabled = false;
	m_LightColor.r = m_LightColor.g = m_LightColor.b = 255;
	m_LightColor.a = 255;
	m_LightColorScale = 255;
	m_flMaxShadowDist = 400.0f;
}

C_CascadeLight::~C_CascadeLight()
{
	if ( m_pCascadeLight == this )
		m_pCascadeLight = NULL;
}

//=============================================================================
//
// Cascaded shadow map manager (client).
//
// Each frame (driven from viewrender.cpp at the flashlight-shadow-depth point),
// splits the view frustum into CSM_MAX_CASCADES slices, fits a texel-snapped
// orthographic sun projection to each, renders scene depth into a per-cascade
// depth texture, and publishes the depth textures + world->shadow-texture
// matrices to the lit shaders through the rendering-parameter channels.
//
// The ortho matrix + depth render exactly mirror the engine's single-ortho
// flashlight path (see clientshadowmgr BuildOrthoWorldToFlashlightMatrix /
// ComputeShadowDepthTextures), which env_global_light already uses successfully
// — so the sampling matrix is guaranteed to match what the engine renders.
//
//=============================================================================

#define CSM_MAX_CASCADES 3

ConVar r_csm_enabled( "r_csm_enabled", "1", FCVAR_ARCHIVE, "Enable cascaded shadow maps (real sun shadows)." );
ConVar r_csm_depthres( "r_csm_depthres", "1024", FCVAR_CHEAT, "Per-cascade shadow depth texture resolution." );
ConVar r_csm_max_shadow_dist( "r_csm_max_shadow_dist", "3000", FCVAR_CHEAT, "Maximum distance from the camera that receives sun shadows." );
ConVar r_csm_caster_distance( "r_csm_caster_distance", "500", FCVAR_CHEAT, "How far toward the sun to extend each cascade to catch off-screen shadow casters. Higher = catches taller off-screen casters but wastes shadow-map depth precision (causes banding). Lower = crisper depth, may miss very tall casters." );
ConVar r_csm_split_lambda( "r_csm_split_lambda", "0.94", FCVAR_CHEAT, "PSSM split blend: 0 = uniform, 1 = logarithmic." );
ConVar r_csm_split_lerp( "r_csm_split_lerp", "0.2", FCVAR_CHEAT, "Cross-fade band width between adjacent cascades, as a fraction." );
ConVar r_csm_dist_fade( "r_csm_dist_fade", "0.85", FCVAR_CHEAT, "Fraction of the shadow range at which shadows begin fading to unshadowed." );
ConVar r_csm_slopescaledepthbias( "r_csm_slopescaledepthbias", "2", FCVAR_CHEAT );
ConVar r_csm_depthbias( "r_csm_depthbias", "0.00005", FCVAR_CHEAT );
ConVar r_csm_strength( "r_csm_strength", "0.7", FCVAR_ARCHIVE, "How dark sun shadows get (0 = off, 1 = black)." );
ConVar r_csm_debug( "r_csm_debug", "0", 0, "Print per-cascade CSM computation info each frame." );
ConVar r_csm_debug_tint( "r_csm_debug_tint", "0", FCVAR_CHEAT, "Tint each cascade a brightness band (0.25/0.5/0.75) to visualize cascade selection in-material." );

//-----------------------------------------------------------------------------
// world->shadow-view matrix, identical to CClientShadowMgr::BuildWorldToShadowMatrix
//-----------------------------------------------------------------------------
static void CSM_BuildWorldToShadowMatrix( VMatrix &matWorldToShadow, const Vector &origin, const Quaternion &quatOrientation )
{
	matrix3x4_t matOrientation;
	QuaternionMatrix( quatOrientation, matOrientation );		// quat -> matrix3x4
	PositionMatrix( vec3_origin, matOrientation );				// zero translation

	VMatrix matBasis( matOrientation );

	Vector vForward, vLeft, vUp;
	matBasis.GetBasisVectors( vForward, vLeft, vUp );
	matBasis.SetForward( vLeft );								// same "bizarre" basis flip the engine uses
	matBasis.SetLeft( vUp );
	matBasis.SetUp( vForward );
	matWorldToShadow = matBasis.Transpose();

	Vector translation;
	Vector3DMultiply( matWorldToShadow, origin, translation );
	translation *= -1.0f;
	matWorldToShadow.SetTranslation( translation );

	matWorldToShadow[3][0] = matWorldToShadow[3][1] = matWorldToShadow[3][2] = 0.0f;
	matWorldToShadow[3][3] = 1.0f;
}

//-----------------------------------------------------------------------------
// world -> [0,1] shadow-texture matrix for an ortho sun projection, identical to
// CClientShadowMgr::BuildOrthoWorldToFlashlightMatrix.
//-----------------------------------------------------------------------------
static void CSM_BuildOrthoWorldToTextureMatrix( VMatrix &matWorldToTexture,
	const Vector &origin, const Quaternion &quatOrientation,
	float flLeft, float flTop, float flRight, float flBottom, float flNearZ, float flFarZ )
{
	VMatrix matWorldToShadowView, matProj;
	CSM_BuildWorldToShadowMatrix( matWorldToShadowView, origin, quatOrientation );

	MatrixBuildOrtho( matProj, flLeft, flTop, flRight, flBottom, flNearZ, flFarZ );

	// Shift x/y to 0..-2 space
	VMatrix addW;
	addW.Identity();
	addW[0][3] = -1.0f;
	addW[1][3] = -1.0f;
	addW[2][3] = 0.0f;
	MatrixMultiply( addW, matProj, matProj );

	// Flip x/y to positive 0..1, flip z to negative
	VMatrix scaleHalf;
	scaleHalf.Identity();
	scaleHalf[0][0] = -0.5f;
	scaleHalf[1][1] = -0.5f;
	scaleHalf[2][2] = -1.0f;
	MatrixMultiply( scaleHalf, matProj, matProj );

	MatrixMultiply( matProj, matWorldToShadowView, matWorldToTexture );
}

//-----------------------------------------------------------------------------
// PSSM split plane distance (log/linear blend).
//-----------------------------------------------------------------------------
static float CSM_SplitDistance( int iSplit, int nSplits, float flNear, float flFar, float flLambda )
{
	// PSSM split plane distance (log/linear blend + bias).
	float fIM = (float)iSplit / (float)nSplits;
	float flLog = flNear * powf( flFar / flNear, fIM );
	float flLinear = flNear + ( flFar - flNear ) * fIM;
	const float fBias = 0.1f;
	return flLambda * flLog + ( 1.0f - flLambda ) * flLinear + fBias;
}

//-----------------------------------------------------------------------------
class CCascadeLightManager : public CAutoGameSystem
{
public:
	CCascadeLightManager() : CAutoGameSystem( "CCascadeLightManager" )
	{
		m_bRenderTargetsAllocated = false;
		m_nDepthResolution = 0;
	}

	virtual void LevelInitPostEntity() { InitRenderTargets(); }
	virtual void Shutdown() { ShutdownRenderTargets(); }

	void ComputeShadowDepthTextures( const CViewSetup &viewSetup );

private:
	void InitRenderTargets();
	void ShutdownRenderTargets();

	bool m_bRenderTargetsAllocated;
	int m_nDepthResolution;
	CTextureReference m_DummyColorTexture;
	CTextureReference m_CascadeDepthTexture[CSM_MAX_CASCADES];
	VMatrix m_CascadeWorldToTex[CSM_MAX_CASCADES];
};

static CCascadeLightManager s_CascadeLightManager;

void CSM_ComputeShadowDepthTextures( const CViewSetup &viewSetup )
{
	s_CascadeLightManager.ComputeShadowDepthTextures( viewSetup );
}

//-----------------------------------------------------------------------------
void CCascadeLightManager::InitRenderTargets()
{
	if ( m_bRenderTargetsAllocated )
		return;

	if ( !materials )
		return;

	ImageFormat dstFormat = materials->GetShadowDepthTextureFormat();
	if ( dstFormat == IMAGE_FORMAT_UNKNOWN )
		return;	// hardware has no shadow-depth support

	ImageFormat nullFormat = materials->GetNullTextureFormat();

	m_nDepthResolution = r_csm_depthres.GetInt();

	materials->BeginRenderTargetAllocation();

	m_DummyColorTexture.InitRenderTarget( m_nDepthResolution, m_nDepthResolution, RT_SIZE_NO_CHANGE,
		nullFormat, MATERIAL_RT_DEPTH_NONE, false, "_rt_CSMDummy" );

	for ( int i = 0; i < CSM_MAX_CASCADES; ++i )
	{
		char szName[64];
		Q_snprintf( szName, sizeof( szName ), "_rt_CSMDepth_%d", i );
		m_CascadeDepthTexture[i].InitRenderTarget( m_nDepthResolution, m_nDepthResolution, RT_SIZE_NO_CHANGE,
			dstFormat, MATERIAL_RT_DEPTH_NONE, false, szName );
	}

	materials->EndRenderTargetAllocation();

	m_bRenderTargetsAllocated = true;
}

//-----------------------------------------------------------------------------
void CCascadeLightManager::ShutdownRenderTargets()
{
	if ( !m_bRenderTargetsAllocated )
		return;

	m_DummyColorTexture.Shutdown();
	for ( int i = 0; i < CSM_MAX_CASCADES; ++i )
		m_CascadeDepthTexture[i].Shutdown();

	m_bRenderTargetsAllocated = false;
}

//-----------------------------------------------------------------------------
void CCascadeLightManager::ComputeShadowDepthTextures( const CViewSetup &viewSetup )
{
	if ( !r_csm_enabled.GetBool() )
		return;

	C_CascadeLight *pLight = C_CascadeLight::Get();
	if ( !pLight || !pLight->IsEnabled() )
		return;

	if ( !m_bRenderTargetsAllocated )
	{
		InitRenderTargets();
		if ( !m_bRenderTargetsAllocated )
			return;
	}

	Vector sunDir = pLight->GetShadowDirection();
	if ( sunDir.LengthSqr() < 1e-6f )
		return;
	VectorNormalize( sunDir );

	// Sun orientation basis. Deriving it from angles the same way the engine builds the
	// shadow view keeps our ortho box, sampling matrix, and the depth render consistent.
	QAngle sunAngles;
	VectorAngles( sunDir, sunAngles );
	Vector vForward, vRight, vUp;
	AngleVectors( sunAngles, &vForward, &vRight, &vUp );
	Quaternion quatOrientation;
	BasisToQuaternion( vForward, vRight, vUp, quatOrientation );

	// Camera basis + frustum tangents for computing sub-frustum corners.
	Vector vCamFwd, vCamRight, vCamUp;
	AngleVectors( viewSetup.angles, &vCamFwd, &vCamRight, &vCamUp );
	float flTanHalfH = tanf( DEG2RAD( viewSetup.fov ) * 0.5f );
	float flAspect = ( viewSetup.m_flAspectRatio != 0.0f ) ? viewSetup.m_flAspectRatio
		: ( (float)viewSetup.width / (float)MAX( 1, viewSetup.height ) );
	float flTanHalfV = flTanHalfH / flAspect;

	float flNear = MAX( 1.0f, viewSetup.zNear );
	float flShadowFar = MIN( viewSetup.zFar, r_csm_max_shadow_dist.GetFloat() );
	if ( flShadowFar <= flNear )
		return;

	float flLambda = clamp( r_csm_split_lambda.GetFloat(), 0.0f, 1.0f );
	float flCasterDist = r_csm_caster_distance.GetFloat();

	CMatRenderContextPtr pRenderContext( materials );

	for ( int i = 0; i < CSM_MAX_CASCADES; ++i )
	{
		float flSplitNear = ( i == 0 ) ? flNear : CSM_SplitDistance( i, CSM_MAX_CASCADES, flNear, flShadowFar, flLambda );
		float flSplitFar  = CSM_SplitDistance( i + 1, CSM_MAX_CASCADES, flNear, flShadowFar, flLambda );

		// Sub-frustum world corners (near slice + far slice).
		Vector corners[8];
		int c = 0;
		for ( int s = 0; s < 2; ++s )
		{
			float d = ( s == 0 ) ? flSplitNear : flSplitFar;
			float hw = d * flTanHalfH;
			float hh = d * flTanHalfV;
			Vector center = viewSetup.origin + vCamFwd * d;
			corners[c++] = center - vCamRight * hw - vCamUp * hh;
			corners[c++] = center + vCamRight * hw - vCamUp * hh;
			corners[c++] = center - vCamRight * hw + vCamUp * hh;
			corners[c++] = center + vCamRight * hw + vCamUp * hh;
		}

		// Bounding sphere of the slice (rotation-invariant -> no shimmer from box resizing).
		Vector vCenter( 0, 0, 0 );
		for ( int k = 0; k < 8; ++k )
			vCenter += corners[k];
		vCenter *= ( 1.0f / 8.0f );
		float flRadius = 0.0f;
		for ( int k = 0; k < 8; ++k )
			flRadius = MAX( flRadius, ( corners[k] - vCenter ).Length() );
		flRadius = ceilf( flRadius );
		if ( flRadius < 1.0f )
			flRadius = 1.0f;

		// Texel-snap the sphere center in the light's XY plane to kill shadow crawl.
		float flTexelSize = ( 2.0f * flRadius ) / (float)m_nDepthResolution;
		float cx = DotProduct( vCenter, vRight );
		float cy = DotProduct( vCenter, vUp );
		float snapX = floorf( cx / flTexelSize ) * flTexelSize;
		float snapY = floorf( cy / flTexelSize ) * flTexelSize;
		vCenter += vRight * ( snapX - cx ) + vUp * ( snapY - cy );

		// Pull the sun "camera" back along the light to catch off-screen casters.
		float flPullBack = flRadius + flCasterDist;
		Vector vEye = vCenter - sunDir * flPullBack;
		float flNearZ = 1.0f;
		float flFarZ = 2.0f * flRadius + flCasterDist + 1.0f;

		float flLeft = -flRadius, flRight = flRadius;
		float flTop = -flRadius, flBottom = flRadius;

		// Sampling matrix (must exactly match the ortho render below).
		CSM_BuildOrthoWorldToTextureMatrix( m_CascadeWorldToTex[i], vEye, quatOrientation,
			flLeft, flTop, flRight, flBottom, flNearZ, flFarZ );

		// Render scene depth from the sun for this cascade.
		CViewSetup shadowView;
		shadowView.x = shadowView.y = 0;
		shadowView.width = m_CascadeDepthTexture[i]->GetActualWidth();
		shadowView.height = m_CascadeDepthTexture[i]->GetActualHeight();
		shadowView.m_bOrtho = true;
		shadowView.m_OrthoLeft = flLeft;
		shadowView.m_OrthoTop = flTop;
		shadowView.m_OrthoRight = flRight;
		shadowView.m_OrthoBottom = flBottom;
		shadowView.m_bDoBloomAndToneMapping = false;
		shadowView.m_flAspectRatio = 1.0f;
		shadowView.fov = shadowView.fovViewmodel = 90.0f;	// unused in ortho
		shadowView.origin = vEye;
		QuaternionAngles( quatOrientation, shadowView.angles );
		shadowView.zNear = shadowView.zNearViewmodel = flNearZ;
		shadowView.zFar = shadowView.zFarViewmodel = flFarZ;

		// Set the depth bias AND cull mode per-cascade, immediately before the render. These are
		// global device states: if we don't set them ourselves each render they leak in from
		// whatever ran before us — and the flashlight's depth pass runs right before ours, so its
		// bias/cull state changed our shadows depending on whether the flashlight was on.
		// (SP has no MATERIAL_CULLMODE_NONE enum — CCW, the default, at
		// least makes our render deterministic regardless of the flashlight.)
		pRenderContext->SetShadowDepthBiasFactors( r_csm_slopescaledepthbias.GetFloat(), r_csm_depthbias.GetFloat() );
		pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );

		view->UpdateShadowDepthTexture( m_DummyColorTexture, m_CascadeDepthTexture[i], shadowView );

		// Publish to the lit shaders.
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_CSM_DEPTHTEXTURE_BASE + i,
			int( (ITexture *)m_CascadeDepthTexture[i] ) );
		for ( int r = 0; r < 3; ++r )
		{
			Vector xyz( m_CascadeWorldToTex[i][r][0], m_CascadeWorldToTex[i][r][1], m_CascadeWorldToTex[i][r][2] );
			pRenderContext->SetVectorRenderingParameter( VECTOR_RENDERPARM_CSM_MATRIX_ROW_BASE + i * 3 + r, xyz );
			pRenderContext->SetFloatRenderingParameter( FLOAT_RENDERPARM_CSM_MATRIX_ROW_W_BASE + i * 3 + r, m_CascadeWorldToTex[i][r][3] );
		}

		if ( r_csm_debug.GetBool() )
		{
			DevMsg( "CSM cascade %d: split %.0f..%.0f  radius %.0f  eye (%.0f %.0f %.0f)\n",
				i, flSplitNear, flSplitFar, flRadius, vEye.x, vEye.y, vEye.z );
		}
	}

	// Shader params:
	//   c27 = strength, splitLerpBase, splitLerpInvRange, 1/depthRes
	//   c28 = zLerpBase, zLerpRange, debugTint, unused
	// Split cross-fade band.
	float flLerpRange = MAX( 0.001f, r_csm_split_lerp.GetFloat() );
	float flSplitLerpBase = 0.5f - flLerpRange;
	float flSplitLerpInvRange = 1.0f / flLerpRange;

	// Quadratic radial distance fade: fade to unshadowed between
	// (r_csm_dist_fade * maxDist) and maxDist, based on squared world distance from camera.
	float flZLerpStart = flShadowFar * r_csm_dist_fade.GetFloat();
	float flZLerpEnd = flShadowFar;
	float flQ = 1.0f / MAX( 1.0f, flZLerpEnd * flZLerpEnd - flZLerpStart * flZLerpStart );
	float flZLerpBase = -( flZLerpStart * flZLerpStart * flQ );
	float flZLerpRange = flQ;

	pRenderContext->SetFloatRenderingParameter( FLOAT_RENDERPARM_CSM_PARAMS_BASE + 0, r_csm_strength.GetFloat() );
	pRenderContext->SetFloatRenderingParameter( FLOAT_RENDERPARM_CSM_PARAMS_BASE + 1, flSplitLerpBase );
	pRenderContext->SetFloatRenderingParameter( FLOAT_RENDERPARM_CSM_PARAMS_BASE + 2, flSplitLerpInvRange );
	pRenderContext->SetFloatRenderingParameter( FLOAT_RENDERPARM_CSM_PARAMS_BASE + 3, 1.0f / (float)m_nDepthResolution );
	pRenderContext->SetFloatRenderingParameter( FLOAT_RENDERPARM_CSM_PARAMS_BASE + 4, r_csm_debug_tint.GetFloat() );
	pRenderContext->SetFloatRenderingParameter( FLOAT_RENDERPARM_CSM_PARAMS_BASE + 5, flZLerpBase );
	pRenderContext->SetFloatRenderingParameter( FLOAT_RENDERPARM_CSM_PARAMS_BASE + 6, flZLerpRange );

	// Restore the default cull mode.
	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
}
