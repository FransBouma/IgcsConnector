///////////////////////////////////////////////////////////////////////
//
// Part of IGCS Connector, an add on for Reshade 5+ which allows you
// to connect IGCS built camera tools with reshade to exchange data and control
// from Reshade.
// 
// (c) Frans 'Otis_Inf' Bouma.
//
// All rights reserved.
// https://github.com/FransBouma/IgcsConnector
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/////////////////////////////////////////////////////////////////////////
#pragma once

enum class DepthOfFieldRenderOrder : int
{
	InnerRingToOuterRing,
	OuterRingToInnerRing,
	Randomized,
};

enum class DepthOfFieldBlurType : int
{
	ApertureShape,
	Circular,
};


enum class ScreenshotControllerState : int
{
	Off,
	InSession,
	SavingShots,
	Canceling,
};


enum class DepthOfFieldControllerState : int
{
	Off,			// off state, user clicks the button 'Begin session'
	Start,			// start state for the shader, it'll copy the backbuffer, the original setup shot, to a cache texture
	Setup,			// user defines value 'A' (relationship between step size and pixels) and 'B' (step size)
	Rendering,		// user is done setting things up and the shot is rendered
	Done,			// Rendering has been completed, user can click 'Done' to end the session (shader will NOT be switched off so result will stay)
	Cancelling,		// user cancels the session and things go back to off
};


enum class DepthOfFieldRenderFrameState : int
{
	Off,			// no work
	Start,			// start state of the whole process. 
	FrameWait,		// Waiting for the camera to have moved after setup. This waiting is done with a counter
	FrameBlending,	// Currently in the blending operation. 
};


enum class ScreenshotType : int
{
	HorizontalPanorama = 0,
	MultiShot = 1,
	DebugGrid = 2,
};


enum class ScreenshotFiletype : int
{
	Bmp,
	Jpeg,
	Png
};


enum class ScreenshotSessionStartReturnCode : int
{
	AllOk = 0,
	Error_CameraNotEnabled = 1,
	Error_CameraPathPlaying = 2,
	Error_AlreadySessionActive = 3,
	Error_CameraFeatureNotAvailable = 4,
	Error_UnknownError = 5
};

