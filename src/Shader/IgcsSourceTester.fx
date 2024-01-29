////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IGCS -> IGCS Connector -> Reshade uniforms tester.
// By Frans Bouma, aka Otis / Infuse Project (Otis_Inf)
// https://fransbouma.com 
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
#include "DrawText.fxh"

namespace IGCSSourceTester
{
	//////////////////////////////////////////////////
	//
	// User interface controls
	//
	//////////////////////////////////////////////////

	uniform bool IGCS_cameraDataAvailable < source = "IGCS_cameraDataAvailable"; >;		// only true if camera tools are connected. Data is invalid if false
	uniform bool IGCS_cameraEnabled < source = "IGCS_cameraEnabled"; >;					// true if the user has enabled the camera
	uniform bool IGCS_cameraMovementLocked < source = "IGCS_cameraMovementLocked"; >;	// true if the user has locked camera movement
	uniform float IGCS_cameraFoV < source = "IGCS_cameraFoV"; >;						// camera fov in degrees
	uniform float3 IGCS_cameraWorldPosition < source = "IGCS_cameraWorldPosition"; >;	// camera position in world coordinates
	uniform float4 IGCS_cameraOrientation < source = "IGCS_cameraOrientation"; >;		// camera orientation (quaternion)
	uniform float IGCS_cameraRotationPitch < source = "IGCS_cameraRotationPitch"; >;	// camera rotation angle (Pitch) in radians
	uniform float IGCS_cameraRotationYaw < source = "IGCS_cameraRotationYaw"; >;		// camera rotation angle (Yaw) in radians
	uniform float IGCS_cameraRotationRoll < source = "IGCS_cameraRotationRoll"; >;		// camera rotation angle (Roll) in radians
	uniform float4x4 IGCS_cameraViewMatrix4x4 < source = "IGCS_cameraViewMatrix4x4"; >;	// camera view matrix calculated from camera orientation 
	uniform float4x4 IGCS_cameraProjectionMatrix4x4LH < source = "IGCS_cameraProjectionMatrix4x4LH"; >;	// camera projection matrix calculated from fov, aspect ratio, near (0.1) and far (10000.0). LH format, row major order
	uniform float3 IGCS_cameraUp < source = "IGCS_cameraUp";>;							// camera up vector part of camera view matrix. In world space
	uniform float3 IGCS_cameraRight < source = "IGCS_cameraRight";>;					// camera right vector part of camera view matrix. In world space
	uniform float3 IGCS_cameraForward < source = "IGCS_cameraForward";>;				// camera forward vector part of camera view matrix. In world space
	

	//////////////////////////////////////////////////
	//
	// Shaders
	//
	//////////////////////////////////////////////////

	void PS_TestValues(float4 vpos : SV_Position, float2 texcoord : TEXCOORD, out float4 fragment : SV_Target0)
	{
		static int cameraDataAvailable[21]  = { __C, __a, __m, __e, __r, __a, __Space, __d, __a, __t, __a, __Space, __a, __v, __a, __i, __l, __a, __b, __l, __e }; 
		static int cameraEnabled[14] = {__C, __a, __m, __e, __r, __a, __Space, __e, __n, __a, __b, __l, __e, __d};
		static int cameraMovementLocked[22] = {__C, __a, __m, __e, __r, __a, __Space, __m, __o, __v, __e, __m, __e, __n, __t, __Space, __l, __o, __c, __k, __e, __d};
		static int fov[4] = { __F, __o, __V, __Colon};
		static int position[9] = { __P, __o, __s, __i, __t, __i, __o, __n, __Colon };
		static int orientation[12] = { __O, __r, __i, __e, __n, __t, __a, __t, __i, __o, __n, __Colon};
		static int anglesPYR[15] = {__A, __n, __g, __l, __e, __s, __Space, __rBrac_O, __P, __Comma, __Y, __Comma, __R, __rBrac_C, __Colon};
		static int cameraUp[10] = {__C, __a, __m, __e, __r, __a, __Space, __u, __p, __Colon};
		static int cameraRight[13] = {__C, __a, __m, __e, __r, __a, __Space, __r, __i, __g, __h, __t, __Colon};
		static int cameraForward[15] = {__C, __a, __m, __e, __r, __a, __Space, __f, __o, __r, __w, __a, __r, __d, __Colon};
		static int viewMatrix[11] = { __V, __i, __e, __w, __m, __a, __t, __r, __i, __x, __Colon};
		static int projectionMatrix[17] = { __P, __r, __o, __j, __e, __c, __t, __i, __o, __n, __m, __a, __t, __r, __i, __x, __Colon };
		
		fragment = tex2D(ReShade::BackBuffer, texcoord);
		
		float charPixel = 0.0f;
		
		// data available
		if(IGCS_cameraDataAvailable)
		{
			DrawText_String(float2(10.0 , 10.0), 16, 1, texcoord,  cameraDataAvailable, 21, charPixel);
		}
		
		// camera enabled
		if(IGCS_cameraEnabled)
		{
			DrawText_String(DrawText_Shift(float2(10.0 , 10.0), int2(0, 1), 16, 1), 16, 1, texcoord,  cameraEnabled, 14, charPixel);
		}
		
		// movement locked
		if(IGCS_cameraMovementLocked)
		{
			DrawText_String(DrawText_Shift(float2(10.0 , 10.0), int2(0, 2), 16, 1), 16, 1, texcoord,  cameraMovementLocked, 22, charPixel);
		}
		
		// fov
		DrawText_String(DrawText_Shift(float2(10.0 , 10.0), int2(0, 3), 16, 1), 16, 1, texcoord,  fov, 4, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 3), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 2, IGCS_cameraFoV, charPixel);
		
		// camera world position
		DrawText_String(DrawText_Shift(float2(10.0 , 10.0), int2(0, 4), 16, 1), 16, 1, texcoord,  position, 9, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 4), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraWorldPosition.x, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 4), 16, 1), int2(45, 0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraWorldPosition.y, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 4), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraWorldPosition.z, charPixel);
		
		// camera orientation
		DrawText_String(DrawText_Shift(float2(10.0 , 10.0), int2(0, 5), 16, 1), 16, 1, texcoord,  orientation, 12, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 5), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraOrientation.x, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 5), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraOrientation.y, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 5), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraOrientation.z, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 5), 16, 1), int2(85,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraOrientation.w, charPixel);
		
		// angles
		DrawText_String(DrawText_Shift(float2(10.0 , 10.0), int2(0, 6), 16, 1), 16, 1, texcoord,  anglesPYR, 15, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 6), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraRotationPitch, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 6), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraRotationYaw, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 6), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraRotationRoll, charPixel);

		// camera up
		DrawText_String(DrawText_Shift(float2(10.0 , 10.0), int2(0, 7), 16, 1), 16, 1, texcoord,  cameraUp, 10, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 7), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraUp.x, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 7), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraUp.y, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 7), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraUp.z, charPixel);
		
		// camera right
		DrawText_String(DrawText_Shift(float2(10.0 , 10.0), int2(0, 8), 16, 1), 16, 1, texcoord,  cameraRight, 13, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 8), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraRight.x, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 8), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraRight.y, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 8), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraRight.z, charPixel);

		// camera forward
		DrawText_String(DrawText_Shift(float2(10.0 , 10.0), int2(0, 9), 16, 1), 16, 1, texcoord,  cameraForward, 15, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 9), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraForward.x, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 9), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraForward.y, charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 10.0), int2(0, 9), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraForward.z, charPixel);
		
		// view matrix
		DrawText_String(DrawText_Shift(float2(10.0 , 10.0), int2(0, 12), 16, 1), 16, 1, texcoord,  viewMatrix, 11, charPixel);
		// row 0
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 12.0), int2(0, 12), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[0][0], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 12.0), int2(0, 12), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[0][1], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 12.0), int2(0, 12), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[0][2], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 12.0), int2(0, 12), 16, 1), int2(85,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[0][3], charPixel);
		// row 1
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 13.0), int2(0, 13), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[1][0], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 13.0), int2(0, 13), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[1][1], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 13.0), int2(0, 13), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[1][2], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 13.0), int2(0, 13), 16, 1), int2(85,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[1][3], charPixel);
		// row 2
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 14.0), int2(0, 14), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[2][0], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 14.0), int2(0, 14), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[2][1], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 14.0), int2(0, 14), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[2][2], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 14.0), int2(0, 14), 16, 1), int2(85,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[2][3], charPixel);
		// row 3
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 15.0), int2(0, 15), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[3][0], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 15.0), int2(0, 15), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[3][1], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 15.0), int2(0, 15), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[3][2], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 15.0), int2(0, 15), 16, 1), int2(85,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraViewMatrix4x4[3][3], charPixel);
		
		// projection matrix
		DrawText_String(DrawText_Shift(float2(10.0 , 10.0), int2(0, 17), 16, 1), 16, 1, texcoord,  projectionMatrix, 17, charPixel);
		// row 0
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 17.0), int2(0, 17), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[0][0], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 17.0), int2(0, 17), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[0][1], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 17.0), int2(0, 17), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[0][2], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 17.0), int2(0, 17), 16, 1), int2(85,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[0][3], charPixel);
		// row 1
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 18.0), int2(0, 18), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[1][0], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 18.0), int2(0, 18), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[1][1], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 18.0), int2(0, 18), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[1][2], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 18.0), int2(0, 18), 16, 1), int2(85,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[1][3], charPixel);
		// row 2
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 19.0), int2(0, 19), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[2][0], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 19.0), int2(0, 19), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[2][1], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 19.0), int2(0, 19), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[2][2], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 19.0), int2(0, 19), 16, 1), int2(85,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[2][3], charPixel);
		// row 3
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 20.0), int2(0, 20), 16, 1), int2(25,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[3][0], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 20.0), int2(0, 20), 16, 1), int2(45,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[3][1], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 20.0), int2(0, 20), 16, 1), int2(65,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[3][2], charPixel);
		DrawText_Digit(DrawText_Shift(DrawText_Shift(float2(10.0 , 20.0), int2(0, 20), 16, 1), int2(85,0), 16, 1), 16, 1, texcoord, 3, IGCS_cameraProjectionMatrix4x4LH[3][3], charPixel);
		
		fragment += float4(charPixel, charPixel, charPixel, 1.0);
		fragment = saturate(fragment);
	}




	//////////////////////////////////////////////////
	//
	// Techniques
	//
	//////////////////////////////////////////////////

	technique IgcsSourceTester
	{
		pass DetermineCurrentFocus { VertexShader = PostProcessVS; PixelShader = PS_TestValues; }
	}
}
