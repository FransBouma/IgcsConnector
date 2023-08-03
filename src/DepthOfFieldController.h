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
#include <functional>
#include <mutex>

#include "CameraToolsConnector.h"
#include "ConstantsEnums.h"
#include <reshade.hpp>

#include "ReshadeStateSnapshot.h"

class DepthOfFieldController
{
public:
	DepthOfFieldController(CameraToolsConnector& connector);
	~DepthOfFieldController() = default;

	DepthOfFieldControllerState getState() { return _state; }

	/// <summary>
	/// Sets the max bokeh size. This controls the value 'B' in the design which is the maximum diameter of the bokeh circles and 2x the max step the camera will traverse. In world units of the engine.
	/// </summary>
	/// <param name="newValue"></param>
	void setMaxBokehSize(reshade::api::effect_runtime* runtime, float newValue);
	/// <summary>
	/// Sets the focus delta. This controls value 'A' in the design which is the percentage the pixels which have to be in focus have moved over MaxBokehSize.
	///	This value is used to realign the image when the camera steps a factor of MaxBokehSize.
	/// </summary>
	/// <param name="newValue"></param>
	void setFocusDelta(reshade::api::effect_runtime* runtime, float newValue);
	/// <summary>
	/// Starts a new session. 
	/// </summary>
	void startSession(reshade::api::effect_runtime* runtime);
	/// <summary>
	/// Ends the current session in progress
	/// </summary>
	void endSession(reshade::api::effect_runtime* runtime);
	/// <summary>
	/// Called when the present method is called but before reshade effects are rendered. 
	/// </summary>
	/// <param name="runtime"></param>
	void reshadeBeginEffectsCalled(reshade::api::effect_runtime* runtime);
	void startRender(reshade::api::effect_runtime* runtime);
	void migrateReshadeState(reshade::api::effect_runtime* runtime);

	float getMaxBokehSize() { return _maxBokehSize; }
	float getFocusDelta() { return _focusDelta; }

private:
	void setUniformIntVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, int valueToWrite);
	void setUniformFloatVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, float valueToWrite);
	void displayScreenshotSessionStartError(const ScreenshotSessionStartReturnCode sessionStartResult);
	void writeVariableStateToShader(reshade::api::effect_runtime* runtime);

	CameraToolsConnector& _cameraToolsConnector;
	DepthOfFieldControllerState _state;

	std::function<void(reshade::api::effect_runtime*)>  _onPresentWorkFunc = nullptr;			// if set, this function is called when the onPresentWork counter reaches 0.

	float _maxBokehSize = 0.25;		// value 'B', so the max diameter of a circle we're going to walk. In world units of the engine
	float _focusDelta = 0.0f;		// value 'A', the relationship between stepping over maxBokehSize and the movement of the pixels that have to be in focus.
	int _onPresentWorkCounter = 0;					// if 0, reshadeBeginEffectsCalled will call onPresentWorkFunc (if set), otherwise this counter is decreased.

	ReshadeStateSnapshot _reshadeStateAtStart;
	std::mutex _reshadeStateMutex;

};

