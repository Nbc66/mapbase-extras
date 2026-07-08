//========= Fracture Point - Cascaded Shadow Maps (in-material sampling) ========//
//
// Cascaded shadow map in-material sampling (PSSM cascades, PCF, blend, distance fade).
// Uses separate per-cascade depth textures (not an atlas) and tex2Dproj for the
// hardware shadow-compare.
//
// Included by the lit pixel shaders. The including shader declares, at the PC-free
// flashlight registers:
//   c: g_CSMCascadeNRow{X,Y,Z}  (N=0..2)   world->shadow-texture matrix rows
//   c: g_CSMParams   ( x=strength  y=splitLerpBase  z=splitLerpInvRange  w=1/depthRes )
//   c: g_CSMParams2  ( x=zLerpBase y=zLerpRange     z=debugTint          w=unused )
//   s: g_CSMDepthSampler0/1/2               cascade depth textures (s13/s14/s15)
//
//===============================================================================

#ifndef FRACTURE_CSM_FXC_H
#define FRACTURE_CSM_FXC_H

static const float g_flCSMShadowBias = 0.0005f;

// True if uv is inside [0,1]^2.
bool CSM_UVInRange( float2 uv )
{
	float2 t = saturate( uv ) - uv;
	return dot( t, t ) == 0.0f;
}

// Split lerp factor: 1.0 at the cascade center, ramping to 0.0 near
// the cascade edge, so we can cross-fade into the next (coarser) cascade.
float CSM_SplitLerpFactor( float2 uv, float base, float invRange )
{
	float2 t = float2( 1.0f, 1.0f ) - saturate( ( abs( uv - float2( 0.5f, 0.5f ) ) - base ) * invRange );
	return t.x * t.y;
}

// 9-tap hardware-PCF (weighting: 4/16 center,
// 2/16 edges, 1/16 corners). tex2Dproj does the hardware depth compare (ref in .z).
// Sampled at the top level (never in a dynamic branch) so gradients stay valid.
float CSM_SampleCascade( sampler depthSampler, float2 uv, float z, float e )
{
	float4 corner;
	corner.x = tex2Dproj( depthSampler, float4( uv + float2(  e,  e ), z, 1 ) ).x;
	corner.y = tex2Dproj( depthSampler, float4( uv + float2( -e,  e ), z, 1 ) ).x;
	corner.z = tex2Dproj( depthSampler, float4( uv + float2(  e, -e ), z, 1 ) ).x;
	corner.w = tex2Dproj( depthSampler, float4( uv + float2( -e, -e ), z, 1 ) ).x;

	float4 edge;
	edge.x = tex2Dproj( depthSampler, float4( uv + float2(  e,  0 ), z, 1 ) ).x;
	edge.y = tex2Dproj( depthSampler, float4( uv + float2( -e,  0 ), z, 1 ) ).x;
	edge.z = tex2Dproj( depthSampler, float4( uv + float2(  0,  e ), z, 1 ) ).x;
	edge.w = tex2Dproj( depthSampler, float4( uv + float2(  0, -e ), z, 1 ) ).x;

	float center = tex2Dproj( depthSampler, float4( uv, z, 1 ) ).x;

	return dot( corner, float4( 1.0f/16.0f, 1.0f/16.0f, 1.0f/16.0f, 1.0f/16.0f ) )
	     + dot( edge,   float4( 2.0f/16.0f, 2.0f/16.0f, 2.0f/16.0f, 2.0f/16.0f ) )
	     + center * ( 4.0f/16.0f );
}

// Returns a [0,1] shadow scalar to MULTIPLY the lit color by (1 = fully lit).
float FractureCSM_ComputeShadow(
	float3 worldPos, float3 eyePos,
	float4 c0x, float4 c0y, float4 c0z,
	float4 c1x, float4 c1y, float4 c1z,
	float4 c2x, float4 c2y, float4 c2z,
	sampler d0, sampler d1, sampler d2,
	float4 params, float4 params2 )
{
	float strength = params.x;
	if ( strength <= 0.0f )
		return 1.0f;					// CSM disabled -> fully lit

	float4 wp = float4( worldPos, 1.0f );
	float e = params.w;

	float2 uv0 = float2( dot( wp, c0x ), dot( wp, c0y ) );
	float2 uv1 = float2( dot( wp, c1x ), dot( wp, c1y ) );
	float2 uv2 = float2( dot( wp, c2x ), dot( wp, c2y ) );

	// Sample every cascade at its own uv/depth, unconditionally (top-level -> tex2Dproj
	// gradients valid). Then pick the tightest cascade and its neighbour to blend with.
	float s0 = CSM_SampleCascade( d0, uv0, dot( wp, c0z ) - g_flCSMShadowBias, e );
	float s1 = CSM_SampleCascade( d1, uv1, dot( wp, c1z ) - g_flCSMShadowBias, e );
	float s2 = CSM_SampleCascade( d2, uv2, dot( wp, c2z ) - g_flCSMShadowBias, e );

	float2 uvSel = uv0;
	float shadowCur = 1.0f, shadowNext = 1.0f;
	int nCascade = 0;
	if ( CSM_UVInRange( uv0 ) )      { nCascade = 0; uvSel = uv0; shadowCur = s0; shadowNext = s1; }
	else if ( CSM_UVInRange( uv1 ) ) { nCascade = 1; uvSel = uv1; shadowCur = s1; shadowNext = s2; }
	else if ( CSM_UVInRange( uv2 ) ) { nCascade = 2; uvSel = uv2; shadowCur = s2; shadowNext = 1.0f; }
	else                             { return 1.0f; }	// beyond all cascades

	// r_csm_debug_tint: brightness band per cascade (0.25/0.5/0.75), no depth compare.
	if ( params2.z > 0.0f )
		return (float)( nCascade + 1 ) * 0.25f;

	// Cross-fade into the next cascade near the current cascade's edge.
	float splitLerp = CSM_SplitLerpFactor( uvSel, params.y, params.z );
	float shadow = lerp( shadowNext, shadowCur, saturate( splitLerp ) );

	// Quadratic distance fade to fully lit at the shadow range limit.
	float3 camDelta = worldPos - eyePos;
	float zLerp = saturate( dot( camDelta, camDelta ) * params2.y + params2.x );
	shadow = lerp( shadow, 1.0f, zLerp );

	// strength controls how dark a full shadow becomes.
	return lerp( 1.0f, shadow, strength );
}

#endif // FRACTURE_CSM_FXC_H
