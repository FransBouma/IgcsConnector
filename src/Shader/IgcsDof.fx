////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper shader for IGCS Depth of Field, a multi-sampling adaptive depth of field system built into
// the IGCS Connector for IGCS powered camera tools.
//
// By Frans Bouma, aka Otis / Infuse Project (Otis_Inf)
// https://opm.fransbouma.com 
//
// This shader has been released under the following license:
//
// Copyright (c) 2023 Frans Bouma
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
	#define IGCS_DOF_SHADER_VERSION "v1.2.1"
	
// #define IGCS_DOF_DEBUG	
	
	// ------------------------------
	// Visible values
	// ------------------------------
		
	uniform float SetupAlpha <
		ui_label = "Setup alpha";
		ui_type = "drag";
		ui_min = 0.2; ui_max = 0.8;
		ui_step = 0.001;
	> = 0.5;

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
	uniform float SphericalAberrationDimFactorForFrame <
		ui_category = "Spherical Aberration";
		ui_label="Spherical Aberration Dim Factor for Frame";
		ui_type = "drag";
		ui_min = 0.00; ui_max = 1.00;
		ui_tooltip = "Will weight each frame to produce spherical aberration";
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

	texture texCDNoise				< source = "monochrome_gaussnoise.png"; > { Width = 512; Height = 512; Format = RGBA8; };
	sampler SamplerCDNoise			{ Texture = texCDNoise; MipFilter = POINT; MinFilter = POINT; MagFilter = POINT; AddressU = WRAP; AddressV = WRAP; AddressW = WRAP;};
	
	sampler BackBufferPoint			{ Texture = ReShade::BackBufferTex; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; AddressW = CLAMP; };
	
	texture texBlendAccumulate 		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA32F; };
	sampler SamplerBlendAccumulate	{ Texture = texBlendAccumulate; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT; };


	float3 ConeOverlap(float3 fragment)
	{
		float k = 0.4 * 0.33;
		float2 f = float2(1-2 * k, k);
		float3x3 m = float3x3(f.xyy, f.yxy, f.yyx);
		return mul(fragment, m);
	}
	
	float3 ConeOverlapInverse(float3 fragment)
	{
		float k = 0.4 * 0.33;
		float2 f = float2(k-1, k) * rcp(3 * k-1);
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
		return ConeOverlapInverse(pow(abs(toReturn), 1.0/ HighlightGammaFactor));
	}


	void PS_HandleStateStart(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float3 fragment : SV_Target0)
	{
		if(SessionState==1)
		{
			fragment = tex2Dlod(ReShade::BackBuffer, float4(texcoord, 0, 0)).rgb;
		}
		else
		{
			discard;
		}
	}


	void PS_HandleStateSetup(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float3 fragment : SV_Target0)
	{
		if(SessionState==2)
		{
			float3 currentFragment = tex2Dlod(ReShade::BackBuffer, float4(texcoord.x - FocusDelta, texcoord.y, 0, 0)).rgb;
			float3 cachedFragment = tex2Dlod(SamplerBlendAccumulate, float4(texcoord, 0, 0)).rgb;
			fragment = lerp(cachedFragment, currentFragment, SetupAlpha);
		}
		else
		{
			discard;
		}
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

	
	void PS_HandleStateRender(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target0)
	{
		if(SessionState==3)
		{
			fragment = 0;
			if(BlendFrame)
			{
				const float2 aspectRatio = float2(1, float(BUFFER_PIXEL_SIZE.y) / float(BUFFER_PIXEL_SIZE.x));
				float2 texCoordToReadFrom = texcoord + AlignmentDelta.xy * aspectRatio;
				bool isInside = all(saturate(texCoordToReadFrom - texCoordToReadFrom*texCoordToReadFrom));
				if(isInside)
				{
					float3 currentFragment = tex2Dlod(ReShade::BackBuffer, float4(texCoordToReadFrom, 0.0f, 0.0f)).rgb;
					currentFragment = AccentuateWhites(currentFragment);
					currentFragment *= SphericalAberrationDimFactorForFrame; //adjust exposure of current sample here
					fragment = float4(currentFragment, BlendFactor);
				}				
			}					
		}
		else
		{
			discard;
		}
	}


	void PS_OutputBlendedResultToFrameBuffer(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float3 fragment : SV_Target0)
	{
		if(SessionState==3 || SessionState==4)
		{
			fragment = tex2Dlod(SamplerBlendAccumulate, float4(texcoord, 0, 0.0f)).rgb;
			// doing a tent filter here will also slightly blur the in-focus areas so we can't do that without calculating CoC's and asking 
			// the user for focus points...
			fragment = CorrectForWhiteAccentuation(fragment);
			
			// Dither so no mach bands will appear. From CinematicDOF, contributed by Prod80 to CinematicDOF
			float2 uv = float2(BUFFER_WIDTH, BUFFER_HEIGHT) / float2( 512.0f, 512.0f ); // create multiplier on texcoord so that we can use 1px size reads on gaussian noise texture (since much smaller than screen)
			uv.xy = uv.xy * texcoord.xy;
			float noise = tex2D(SamplerCDNoise, uv).x; // read, uv is scaled, sampler is set to tile noise texture (WRAP)
			fragment.rgb = saturate(fragment.rgb + lerp( -0.5/255.0, 0.5/255.0, noise)); // apply dither
		}
		else
		{
			discard;
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
			"IGCS DoF was written by Frans 'Otis_Inf' Bouma\n"
			"https://opm.fransbouma.com | https://github.com/FransBouma/IgcsConnector"; >
#endif
	{
		pass HandleStateStartPass { 
			// If state is 'Start': backups original framebuffer to texBlendAccumulate
			// If state isn't 'Start': it'll discard the pixel.
			VertexShader = PostProcessVS; PixelShader = PS_HandleStateStart; 
			RenderTarget0=texBlendAccumulate;
			ClearRenderTargets= false;
		}
		pass HandleStateSetupPass { 
			// If state is 'Setup': writes original framebuffer blended with the texBlendAccumulate to the output
			// If state isn't 'Setup': it'll copy the current framebuffer to the output without any changes
			VertexShader = PostProcessVS; PixelShader = PS_HandleStateSetup; 
		}
		pass HandleMagnifierPass {
			// If state is 'Setup': if ShowMagnifier is true, it'll show the pixels around the mousepointer magnified
			// If state isn't 'Setup', it'll copy the framebuffer.
			VertexShader = PostProcessVS; PixelShader = PS_HandleMagnification;
		}		
		pass HandleStateRenderPass { 
			// If state is 'Render': 
			//		if 'BlendFrame' is true, blends current framebuffer into texBlendAccumulate in HDR space	
			//      First frame with blend factor 100% will wipe data from the backup phase during Setup		
			//		if 'BlendFrame' is false or state isn't 'render', discards and leaves texBlendAccumulate in its previous state
			VertexShader = PostProcessVS; PixelShader = PS_HandleStateRender; RenderTarget=texBlendAccumulate;
			BlendEnable = true;
			BlendOp = ADD;
			SrcBlend = SRCALPHA; DestBlend = INVSRCALPHA;
		}
		pass OutputBlendedResultToFrameBuffer {
			// If state is 'Render' or 'Done': copies texBlendAccumulate to framebuffer
			// If state isn't 'Render': copies the current framebuffer to the framebuffer
			VertexShader = PostProcessVS; PixelShader = PS_OutputBlendedResultToFrameBuffer; 
		}
	}
}