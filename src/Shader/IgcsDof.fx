////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper shader for IGCS Depth of Field, a multi-sampling adaptive depth of field system built into
// the IGCS Connector for IGCS powered camera tools.
//
// By Frans Bouma, aka Otis / Infuse Project (Otis_Inf) https://opm.fransbouma.com 
// and
// By Pascal Gilcher, aka Marty McFly  https://www.martysmods.com
// 
// Additional contributions: https://github.com/FransBouma/IgcsConnector/graphs/contributors
//
// This shader has been released under the following license:
//
// Copyright (c) 2024 Frans Bouma
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer. 
// 
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ReShade.fxh"

namespace IgcsDOF
{
	#define IGCS_DOF_SHADER_VERSION "v2.5.0"
	
// #define IGCS_DOF_DEBUG	
	
	// ------------------------------
	// Visible values
	// ------------------------------

	// ------------------------------
	// Hidden values, set by the connector
	// ------------------------------
	
	uniform float HighlightBoost <
		ui_category = "Highlight tweaking";
		ui_label="Highlight boost factor";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 1.00;
		ui_tooltip = "Will boost/dim the highlights a small amount";
		ui_step = 0.001;
		hidden=true;
	> = 0.50;
	uniform float SampleWeightR <
		ui_category = "Sample Weight Red";
		ui_label="Sample Weight for Red Channel for current sample";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 10.00;
		ui_step = 0.001;
		hidden=true;
	> = 1.0;
	uniform float SampleWeightG <
		ui_category = "Sample Weight Green";
		ui_label="Sample Weight for Green Channel for current sample";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 10.00;
		ui_step = 0.001;
		hidden=true;
	> = 1.0;
	uniform float SampleWeightB <
		ui_category = "Sample Weight Blue";
		ui_label="Sample Weight for Blue Channel for current sample";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 10.00;
		ui_step = 0.001;
		hidden=true;
	> = 1.0;
	uniform float HighlightGammaFactor <
		ui_category = "Highlight tweaking";
		ui_label="Highlight gamma factor";
		ui_type = "drag";
		ui_min = 0.001; ui_max = 5.00;
		ui_tooltip = "Controls the gamma factor to boost/dim highlights\n2.2, the default, gives natural colors and brightness";
		ui_step = 0.01;
		hidden=true;
	> = 2.2;
	
	uniform float FocusDelta <
		ui_label = "Focus delta";
		ui_type = "drag";
		ui_min = -1.0; ui_max = 1.0;
		ui_step = 0.001;
		hidden=true;
	> = 0.0f;

	uniform int SessionState < 
		ui_type = "combo";
		ui_min= 0; ui_max=1;
		ui_items="Off\0SessionStart\0Setup\0Render\0Done\0";		// 0: done, 1: start, 2: setup, 3: render, 4: done
		ui_label = "Session state";
		hidden=true;
	> = 0;

	uniform bool BlendFrame <
		ui_label = "Blend frame";				// if true and state is render, the current framebuffer is blended with the temporary result.
		hidden=true;
	> = false;
	
	uniform float BlendFactor < 				// Which is used as alpha for the current framebuffer to blend with the temporary result. 
		ui_label = "Blend factor";
		ui_type = "drag";
		ui_min = 0.0f; ui_max = 1.0f;
		ui_step = 0.01f;
		hidden=true;
	> = 0.0f;
	
	uniform float2 AlignmentDelta <				// Which is used as alignment delta for the current framebuffer to determine with which pixel to blend with.
		ui_type = "drag";
		ui_step = 0.001;
		ui_min = 0.000; ui_max = 1.000;
		hidden=true;
	> = float2(0.0f, 0.0f);

	uniform bool ShowMagnifier<
		ui_label = "Show magnifier";
		hidden=true;
	> = false;
	
	uniform float MagnificationFactor <
		ui_label = "MagnificationFactor";
		ui_type = "drag";
		ui_min = 1.0; ui_max = 10.0;
		ui_step = 1.0;
		hidden=true;
	> = 2.0;
	
	uniform float2 MagnificationArea <				// Which is used as alignment delta for the current framebuffer to determine with which pixel to blend with.
		ui_type = "drag";
		ui_step = 0.001;
		ui_min = 0.01; ui_max = 1.000;
		hidden=true;
	> = float2(0.1f, 0.1f);

	uniform float2 MagnificationLocationCenter <				// Which is used as alignment delta for the current framebuffer to determine with which pixel to blend with.
		ui_type = "drag";
		ui_step = 0.001;
		ui_min = 0.01; ui_max = 1.000;
		hidden=true;
	> = float2(0.5f, 0.5f);
				
	uniform float SetupAlpha <
		ui_label = "Setup alpha";
		ui_type = "drag";
		ui_min = 0.2; ui_max = 0.8;
		ui_step = 0.001;
		hidden=true;
	> = 0.5;

	uniform float CateyeRadiusStart <
		ui_label = "Cateye Bokeh Radius start";
		ui_type = "drag";
		ui_min = 0.0; ui_max = 1.0;
		ui_step = 0.001;
		hidden=true;
	> = 0.2;
	uniform float CateyeRadiusEnd <
		ui_label = "Cateye Bokeh Radius end";
		ui_type = "drag";
		ui_min = 0.0; ui_max = 1.0;
		ui_step = 0.001;
		hidden=true;
	> = 0.7;
	uniform float CateyeIntensity <
		ui_label = "Cateye Bokeh Intensity";
		ui_type = "drag";
		ui_min = -1.0; ui_max = 1.0;
		ui_step = 0.001;
		hidden=true;
	> = 0.0;
	uniform bool CateyeVignette <		
		hidden=true;
	> = false;
	
#ifdef IGCS_DOF_DEBUG
	uniform bool DBBool1<
		ui_label = "DBG Bool1";
	> =false;
	uniform bool DBBool2<
		ui_label = "DBG Bool2";
	> =false;
	uniform bool DBBool3<
		ui_label = "DBG Bool3";
	> =false;
#endif

#ifndef BUFFER_PIXEL_SIZE
	#define BUFFER_PIXEL_SIZE	ReShade::PixelSize
#endif
#ifndef BUFFER_SCREEN_SIZE
	#define BUFFER_SCREEN_SIZE	ReShade::ScreenSize
#endif

	#define CEIL_DIV(num, denom) ((((num) - 1) / (denom)) + 1)

	sampler BackBufferPoint			{ Texture = ReShade::BackBufferTex; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; AddressW = CLAMP; };
	texture texBlendAccumulate 		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA32F; };
	sampler SamplerBlendAccumulate	{ Texture = texBlendAccumulate; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT; };
	storage StorageBlendAccumulate  { Texture = texBlendAccumulate;  };

	texture texDisplay 		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGB10A2; };
	sampler SamplerDisplay	{ Texture = texDisplay;  };
	storage StorageDisplay  { Texture = texDisplay;  };
	
	float3 ConeOverlap(float3 fragment)
	{
		float k = 0.4 * 0.33;
		float2 f = float2(1 - 2 * k, k);
		float3x3 m = float3x3(f.xyy, f.yxy, f.yyx);
		return mul(fragment, m);
	}
	
	float3 ConeOverlapInverse(float3 fragment)
	{		
		float k = 0.4 * 0.33;
		float2 f = float2(k - 1, k) * rcp(3 * k - 1);
		float3x3 m = float3x3(f.xyy, f.yxy, f.yyx);
		return mul(fragment, m);
	}

	float3 AccentuateWhites(float3 fragment)
	{
		// apply small tow to the incoming fragment, so the whitepoint gets slightly lower than max.
		// De-tonemap color (reinhard). Thanks Marty :) 
		fragment = pow(abs(ConeOverlap(fragment)), HighlightGammaFactor);
		return fragment / max((1.001 - (HighlightBoost * fragment)), 0.001);
	}
	
	float3 CorrectForWhiteAccentuation(float3 fragment)
	{
		// Re-tonemap color (reinhard). Thanks Marty :)
		float3 toReturn = fragment / (1.001 + (HighlightBoost * fragment));
		return ConeOverlapInverse(pow(max(0, toReturn), 1.0 / HighlightGammaFactor)); 
	}

	float3 ReadHDRInput(float2 uv)
	{
		uv = uv * BUFFER_SCREEN_SIZE - 0.5;
		int2 gridStart = int2(uv);
		float2 gridPos = frac(uv);

		// manual bilinear interpolation to ramp colors into HDR first, then interpolating.
		// Unfeasible if this is called many times, but for a single call, it's perfect
		float4 weights = float4(gridPos, 1 - gridPos);
		weights = weights.zxzx * weights.wwyy;

		float3 tl = AccentuateWhites(tex2Dfetch(BackBufferPoint, gridStart + float2(0, 0)).rgb);
		float3 tr = AccentuateWhites(tex2Dfetch(BackBufferPoint, gridStart + float2(1, 0)).rgb);
		float3 bl = AccentuateWhites(tex2Dfetch(BackBufferPoint, gridStart + float2(0, 1)).rgb);
		float3 br = AccentuateWhites(tex2Dfetch(BackBufferPoint, gridStart + float2(1, 1)).rgb);

		return tl * weights.x + tr * weights.y + bl * weights.z + br * weights.w;		
	}	

	float3 goldenDither(uint2 p)
	{ 
		uint2 umagic = uint2(3242174889u, 2447445413u); //1/phi, 1/phi² * 2^32, replaces frac() with a static precision method     
		uint3 ret = p.x * umagic.x + p.y * umagic.y;
		ret.y += (umagic.x + umagic.y) * 3u;
		ret.z += (umagic.x + umagic.y) * 7u;
		return float3(ret) * exp2(-32.0) - 0.5;
	}

	float linearstep(float lo, float hi, float x)
	{
		return saturate((x - lo) / (hi - lo));
	}

	struct CSIN 
	{
		uint3 groupthreadid     : SV_GroupThreadID;         
		uint3 groupid           : SV_GroupID;            
		uint3 dispatchthreadid  : SV_DispatchThreadID;     
		uint threadid           : SV_GroupIndex;
	};


	void IGCSCS(in CSIN i)
	{
		float2 uv = (i.dispatchthreadid.xy + 0.5) * BUFFER_PIXEL_SIZE;
		
		if(SessionState == 0 //idle, skip
		|| SessionState == 4 //done accumulating, skip
		|| i.dispatchthreadid.x >= BUFFER_WIDTH || i.dispatchthreadid.y >= BUFFER_HEIGHT) 
		{
			return;
		}
		
		//Start - backup framebuffer at default camera location
		else if(SessionState == 1)
		{
			float3 color = tex2Dfetch(BackBufferPoint, i.dispatchthreadid.xy).rgb;
			tex2Dstore(StorageBlendAccumulate, i.dispatchthreadid.xy, float4(color, 0));
			return;
		}
		//Setup - blend the two camera locations and optionally show the magnifier
		else if(SessionState == 2)
		{
			float2 shifted_uv = uv - float2(FocusDelta, 0);
			float3 currentFragment = tex2Dlod(BackBufferPoint, float4(shifted_uv, 0, 0)).rgb;
			float3 cachedFragment  = tex2Dfetch(StorageBlendAccumulate, i.dispatchthreadid.xy).rgb;
			float3 fragment = lerp(cachedFragment, currentFragment, SetupAlpha);
			tex2Dstore(StorageDisplay, i.dispatchthreadid.xy, float4(fragment, 1));
			return;
		}		

		[branch]
		if(BlendFrame)
		{
			//Stack frames
			const float2 aspectRatio = float2(1, float(BUFFER_PIXEL_SIZE.y) / float(BUFFER_PIXEL_SIZE.x));
			float2 uvToReadFrom = uv + AlignmentDelta.xy * aspectRatio;

			bool isInside = all(saturate(uvToReadFrom - uvToReadFrom*uvToReadFrom));

			float4 result;
			result.rgb = ReadHDRInput(uvToReadFrom);
			result.rgb *= float3(SampleWeightR, SampleWeightG, SampleWeightB);
			result.a = 1.0;

			result.rgba = isInside ? result : 0.0;

			//cateye bokeh
			float2 normalizedOffset = AlignmentDelta.xy / FocusDelta * 2.0;
			float2 cateyeOffset = uv * 2 - 1;
			cateyeOffset.y /= BUFFER_WIDTH * BUFFER_RCP_HEIGHT;
			cateyeOffset /= length(float2(rcp(BUFFER_WIDTH * BUFFER_RCP_HEIGHT), 1)); //adjust length such that length of 1 == vector to screen corner

			float distFromCenter = length(cateyeOffset);
			cateyeOffset /= max(1e-6, distFromCenter);
	
			distFromCenter = linearstep(CateyeRadiusStart, CateyeRadiusEnd, distFromCenter) * sqrt(2.0) * CateyeIntensity; //negative value occludes discs from the other side
			cateyeOffset *= distFromCenter;
			normalizedOffset += cateyeOffset; 

			float cateyeMask = smoothstep(1.0, 0.98, length(normalizedOffset)); //tiny feather to avoid too harsh edges as cutting the neat bokeh sample pattern in half causes a jagged edge

			result.rgb *= cateyeMask;
			result.a *= CateyeVignette ? 1 : cateyeMask; //if we want to produce vignetting, don't multiply alpha such that the resulting image is divided by the full framecount
			
			if(BlendFactor < 0.75)
			{
				float4 prevAccumulator = tex2Dfetch(StorageBlendAccumulate, i.dispatchthreadid.xy);
				result += prevAccumulator;
			}

			tex2Dstore(StorageBlendAccumulate, i.dispatchthreadid.xy, result);

			//We only need to update our output buffer when we changed the accumulator. This saves significant runtime overhead.			
			result.rgb /= result.w; 
			result.rgb = CorrectForWhiteAccentuation(result.rgb);
			float3 dither = goldenDither(i.dispatchthreadid.xy);
			dither *= 0.499; //slightly limit so we don't dither fully flat areas (which can happen with exactly +-0.5)
			dither *= exp2(-BUFFER_COLOR_BIT_DEPTH); //1.0/256.0 or 1.0/1024.0 depending on 8 or 10 bit backbuffer
			result.rgb = saturate(result.rgb + dither);
			tex2Dstore(StorageDisplay, i.dispatchthreadid.xy, float4(result.rgb, 1));			
		}
	}


	void VS_Output(in uint id : SV_VertexID, out float4 vpos : SV_Position, out float2 texcoord : TEXCOORD)
	{
		PostProcessVS(id, vpos, texcoord);		
		if(SessionState == 0)
		{
			vpos = asfloat(0x7F800000);
		}
	}


	void PS_Output(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float3 fragment : SV_Target0)
	{
		fragment = tex2Dlod(SamplerDisplay, float4(texcoord, 0, 0)).rgb;		
	}


	void PS_HandleMagnification(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target0)
	{
		fragment = tex2Dlod(ReShade::BackBuffer, float4(texcoord, 0, 0));
		if(SessionState==2 && ShowMagnifier)
		{
			float2 areaTopLeft = MagnificationLocationCenter - (MagnificationArea / 2.0f);
			float2 areaBottomRight = MagnificationLocationCenter + (MagnificationArea / 2.0f);
			if(texcoord.x >= areaTopLeft.x && texcoord.y >= areaTopLeft.y && texcoord.x <= areaBottomRight.x && texcoord.y <= areaBottomRight.y)
			{
				// inside magnify area
				float2 sourceCoord = ((texcoord - MagnificationLocationCenter) / MagnificationFactor) + MagnificationLocationCenter;
				fragment = tex2Dlod(BackBufferPoint, float4(sourceCoord, 0, 0));
			}
		}
	}


	technique IgcsDOF
#if __RESHADE__ >= 40000
	< ui_tooltip = "IGCS Depth of Field worker shader "
			IGCS_DOF_SHADER_VERSION
			"\n===========================================\n\n"
			"IGCS DoF is an addon-powered advanced depth of field system which\n"
			"uses Otis_Inf's camera tools as well as the IgcsConnector Reshade Addon\n"
			"to produce realistic depth of field effects. This shader works only\n"
			"with the addon and camera tools present.\n\n"
			"IGCS DoF was written by:\nFrans 'Otis_Inf' Bouma, Pascal 'Marty McFly' Gilcher and contributors.\n"
			"https://opm.fransbouma.com | https://github.com/FransBouma/IgcsConnector"; >
#endif
	{
		pass { ComputeShader = IGCSCS<32, 32>;DispatchSizeX = CEIL_DIV(BUFFER_WIDTH, 32);DispatchSizeY = CEIL_DIV(BUFFER_HEIGHT, 32); }
		pass { VertexShader = VS_Output; PixelShader = PS_Output; }		// to show the buffer written by the compute shader. 
		pass HandleMagnifierPass 
		{
			// simple pass to show the magnifier. Done here so the blended setup buffer is visible in the magnified area, as it's constructed in the compute shader. 
			VertexShader = PostProcessVS; PixelShader = PS_HandleMagnification; 
		} 
	}
}