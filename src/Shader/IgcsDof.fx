////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper shader for IGCS Depth of Field, a multi-sampling adaptive depth of field system built into
// the IGCS Connector for IGCS powered camera tools.
//
// By Frans Bouma, aka Otis / Infuse Project (Otis_Inf)
// https://fransbouma.com 
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
	// ------------------------------
	// Visible values
	// ------------------------------

	// ------------------------------
	// Hidden values, set by the connector
	// ------------------------------
	uniform float FocusDelta <
		ui_label = "Focus delta";
		ui_type = "drag";
		ui_min = -1.0; ui_max = 1.0;
		ui_step = 0.001;
	> = 0.0;

	uniform float MaxBokehSize <
		ui_label = "Max bokeh size";
		ui_type = "drag";
		ui_min = 0.001; ui_max = 10.0;
		ui_step = 0.01;
	> = 0.001;

	uniform int SessionState < 
		ui_type = "combo";
		ui_min= 0; ui_max=1;
		ui_items="Off\0SessionStart\0Setup\0Render\0Done\0";		// 0: done, 1: start, 2: setup, 3: render, 4: done
		ui_label = "Session state";
		ui_tooltip = "The blur type. Focus Point Targeting Strokes means the blur directions\nper pixel are towards the Focus Point.";
	> = 0;

	uniform bool DBBool1<
		ui_label = "DBG Bool1";
	> =false;
	uniform bool DBBool2<
		ui_label = "DBG Bool2";
	> =false;
	uniform bool DBBool3<
		ui_label = "DBG Bool3";
	> =false;
	
#ifndef BUFFER_PIXEL_SIZE
	#define BUFFER_PIXEL_SIZE	ReShade::PixelSize
#endif
#ifndef BUFFER_SCREEN_SIZE
	#define BUFFER_SCREEN_SIZE	ReShade::ScreenSize
#endif

	texture texOriginalFBCache 		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
	texture texBlendTempResult1		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
	texture texBlendTempResult2		{ Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA16F; };
	
	sampler SamplerOriginalFBCache	{ Texture = texOriginalFBCache; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT; };
	sampler SamplerBlendTempResult1	{ Texture = texBlendTempResult1; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT; };
	sampler SamplerBlendTempResult2	{ Texture = texBlendTempResult2; MagFilter = POINT; MinFilter = POINT; MipFilter = POINT; };
	
	
	void PS_HandleStateStart(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment0 : SV_Target0, out float4 fragment1 : SV_Target1)
	{
		if(SessionState==1)
		{
			float4 originalFragment = float4(tex2Dlod(ReShade::BackBuffer, float4(texcoord, 0, 0)).rgb, 1.0f);
			fragment0 = originalFragment;
			fragment1 = originalFragment;
		}
		else
		{
			discard;
		}
	}


	void PS_HandleStateSetup(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target0)
	{
		if(SessionState==2)
		{
			float4 originalFragment = tex2Dlod(ReShade::BackBuffer, float4(texcoord.x-FocusDelta, texcoord.y, 0, 0));
			float4 cachedFragment = tex2Dlod(SamplerOriginalFBCache, float4(texcoord, 0, 0));
			fragment = lerp(cachedFragment, originalFragment, 0.5);
		}
		else
		{
			fragment = tex2Dlod(ReShade::BackBuffer, float4(texcoord, 0, 0));
		}
	}
	
	
	technique IgcsDOF
	{
		pass HandleStateStartPass { 
			// If state is 'Start': writes original framebuffer to both render targets
			// If state isn't 'Start': it'll discard the pixel.
			VertexShader = PostProcessVS; PixelShader = PS_HandleStateStart; 
			RenderTarget0=texOriginalFBCache; RenderTarget1=texBlendTempResult1;ClearRenderTargets= false;
		}
		pass HandleStateSetupPass { 
			// If state is 'Setup': writes original framebuffer blended with the texOriginalFBCache to the output
			// If state isn't 'Setup': it'll copy the current framebuffer to the output without any changes
			VertexShader = PostProcessVS; PixelShader = PS_HandleStateSetup; 
		}
		/*
		pass HandleStateRenderPass { 
			// If state is 'Render': writes original framebuffer blended with the temp result stored in texBlendTempResult1 to the 
			// output (LDR corrected) and to texBlendTempResult2 (not LDR corrected so keeps the HDR values). 
			// If state isn't 'Render' it discards the pixel
			VertexShader = PostProcessVS; PixelShader = PS_HandleStateRender; RenderTarget=texBlendTempResult2;
		}
		pass CopyTempResult2ToTempResult1 {
			// If state is 'Render': performs a simple copy from temp result 2 to temp result 1 as we can't read / write to the same texture in a single pass
			// so next frame we can read from texBlendTempResult1 again in HandleStateRenderPass
			// If state isn't 'Render': discards the pixel
			VertexShader = PostProcessVS; PixelShader = PS_CopyTempResult2ToTempResult1; RenderTarget = texBlendTempResult2;
		}
		pass HandleStateDonePass {
			// If state is 'Done': copies textBlendTempResult1 to output
			// If state isn't 'Done: it'll copy the original framebuffer to the output
			VertexShader = PostProcessVS; PixelShader = PS_HandleStateDone; 
		}
		*/
	}
}