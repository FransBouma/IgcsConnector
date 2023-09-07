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
#include <imgui.h>
#include <mutex>

#include "CameraToolsConnector.h"
#include "ConstantsEnums.h"
#include <reshade.hpp>

#include "CDataFile.h"
#include "Utils.h"

#include "ReshadeStateSnapshot.h"

class DepthOfFieldController
{
	/// <summary>
	/// A location definition for the camera to step to. It contains the step info as well as the alignment information for the shader.
	/// </summary>
	struct CameraLocation
	{
		float xDelta = 0.0f;
		float yDelta = 0.0f;
		float xAlignmentDelta = 0.0f;
		float yAlignmentDelta = 0.0f;

		float sampleWeightRGB[3] = {1.0f, 1.0f, 1.0f};
	};

	struct MagnifierSettings
	{
		bool ShowMagnifier = false;
		float MagnificationFactor = 2.0f;
		float XMagnifierLocation = 0.5f;
		float YMagnifierLocation = 0.5f;
		float WidthMagnifierArea = 0.19f;
		float HeightMagnifierArea = 0.1f;
	};

	struct ApertureShapeSettings
	{
		int NumberOfVertices = 4;
		float RotationAngle = 0.0f;
		float RoundFactor = 0.25f;
	};

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
	void setXFocusDelta(reshade::api::effect_runtime* runtime, float newValueX);
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
	/// <summary>
	/// Called when all the reshade effects have been handled and have run.
	/// </summary>
	/// <param name="runtime"></param>
	void reshadeFinishEffectsCalled(reshade::api::effect_runtime* runtime);
	/// <summary>
	/// Starts the render of the final image
	/// </summary>
	/// <param name="runtime"></param>
	void startRender(reshade::api::effect_runtime* runtime);
	/// <summary>
	/// Migrates the grabbed reshade state to the new one passed in. Occurs when the user reloads the reshade preset or the viewport got resized
	/// </summary>
	/// <param name="runtime">Can be empty, in which case it's ignored</param>
	void migrateReshadeState(reshade::api::effect_runtime* runtime);
	/// <summary>
	/// Calculates the set of points in the shape to use
	/// </summary>
	void calculateShapePoints();
	/// <summary>
	/// Renders the overlay which contains the progress bar
	/// </summary>
	void renderOverlay();
	/// <summary>
	/// Draws the created camera steps as points on the drawList specified
	/// </summary>
	/// <param name="drawList"></param>
	/// <param name="topLeftScreenCoord"></param>
	/// <param name="canvasWidthHeight"></param>
	void drawShape(ImDrawList* drawList, ImVec2 topLeftScreenCoord, float canvasWidthHeight);
	/// <summary>
	/// Renders a progress bar at the current ImGui location, which can be in the settings or in an overlay.
	/// </summary>
	void renderProgressBar();
	/// <summary>
	/// Writes a set of member variables to the shader's uniforms using the reshade runtime specified, so the shader can use the values.
	/// </summary>
	/// <param name="runtime"></param>
	void writeVariableStateToShader(reshade::api::effect_runtime* runtime);
	void loadIniFileData(CDataFile& iniFile);
	void saveIniFileData(CDataFile& iniFile);
	void invalidateShapePoints() { calculateShapePoints(); }

	// setters
	void setNumberOfFramesToWaitPerFrame(int newValue) { _numberOfFramesToWaitPerFrame = newValue; }
	void setQuality(int newValue)
	{
		_quality = IGCS::Utils::clampEx(newValue, 1, 100);
		calculateShapePoints();
	}
	void setNumberOfPointsInnermostRing(int newValue)
	{
		_numberOfPointsInnermostRing = IGCS::Utils::clampEx(newValue, 1, 100);
		calculateShapePoints();
	}
	void setBlurType(DepthOfFieldBlurType newValue)
	{
		_blurType = newValue;
		calculateShapePoints();
	}
	void setAnamorphicFactor(float newValue)
	{
		_anamorphicFactor = IGCS::Utils::clampEx(newValue, 0.01f, 1.0f);
		calculateShapePoints();
	}
	void setRingAngleOffset(float newValue)
	{
		_ringAngleOffset = IGCS::Utils::clampEx(newValue, -2.0f, 2.0f);
		calculateShapePoints();
	}
	void setSphericalAberrationDimFactor(float newValue)
	{
		_sphericalAberrationDimFactor = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f);
		calculateShapePoints();
	}
	void setRenderOrder(DepthOfFieldRenderOrder newValue)
	{
		_renderOrder = newValue;
		calculateShapePoints();
	}
	void setHighlightBoostFactor(float newValue) { _highlightBoostFactor = IGCS::Utils::clampEx(newValue, 0.0f, 1.0f); }
	void setHighlightGammaFactor(float newValue) { _highlightGammaFactor = IGCS::Utils::clampEx(newValue, 0.1f, 5.0f); }
	void setRenderPaused(bool newValue) { _renderPaused = newValue; }
	void setShowProgressBarAsOverlay(bool newValue) { _showProgressBarAsOverlay = newValue; }

	// getters
	DepthOfFieldRenderOrder getRenderOrder() { return _renderOrder; }
	float getMaxBokehSize() { return _maxBokehSize; }
	float getXFocusDelta() { return _focusDelta; }
	int getQuality() { return _quality; }
	float getHighlightBoostFactor() { return _highlightBoostFactor; }
	float getHighlightGammaFactor() { return _highlightGammaFactor; }
	DepthOfFieldBlurType getBlurType() { return _blurType; }
	int getNumberOfPointsInnermostRing() { return _numberOfPointsInnermostRing; }
	int getNumberOfFramesToWaitPerFrame() { return _numberOfFramesToWaitPerFrame; }
	bool getRenderPaused() { return _renderPaused; }
	int getTotalNumberOfStepsToTake() { return _cameraSteps.size(); }
	bool getShowProgressBarAsOverlay() { return _showProgressBarAsOverlay; }
	float getAnamorphicFactor() { return _anamorphicFactor; }
	float getRingAngleOffset() { return _ringAngleOffset; }
	float getSphericalAberrationDimFactor() { return _sphericalAberrationDimFactor; }

	MagnifierSettings& getMagnifierSettings() { return _magnificationSettings; }		// this is a bit dirty...
	ApertureShapeSettings& getApertureShapeSettings() { return _apertureShapeSettings; }						// same

	void setDebugBool1(bool newVal) { _debugBool1 = newVal; }
	void setDebugBool2(bool newVal) { _debugBool2 = newVal; }
	void setDebugVal1(float newVal) { _debugVal1 = newVal; }
	void setDebugVal2(float newVal) { _debugVal2 = newVal; }
	bool getDebugBool1() { return _debugBool1; }
	bool getDebugBool2() { return _debugBool2; }
	float getDebugVal1() { return _debugVal1; }
	float getDebugVal2() { return _debugVal2; }

private:
	void setUniformIntVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, int valueToWrite);
	void setUniformFloatVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, float valueToWrite);
	void setUniformBoolVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, bool valueToWrite);
	void setUniformFloat2Variable(reshade::api::effect_runtime* runtime, const std::string& uniformName, float value1ToWrite, float value2ToWrite);
	void loadFloatFromIni(CDataFile& iniFile, const std::string& key, float* toWriteTo);
	void loadIntFromIni(CDataFile& iniFile, const std::string& key, int* toWriteTo);
	void loadBoolFromIni(CDataFile& iniFile, const std::string& key, bool* toWriteTo, bool defaultValue);
	/// <summary>
	/// Create a set of circular points using nested circles, which are used to build the camera steps array
	/// </summary>
	void createCircleDoFPoints();
	void createApertureShapedDoFPoints();

	void displayScreenshotSessionStartError(const ScreenshotSessionStartReturnCode sessionStartResult);
	/// <summary>
	/// Method called after the game has rendered a frame but before reshade will render the reshade effects (and thus our shader)
	/// </summary>
	void handlePresentBeforeReshadeEffects();
	/// <summary>
	/// Method called after the game has rendered a frame and after reshade has rendered the reshade effects (and thus our shader)
	/// </summary>
	void handlePresentAfterReshadeEffects();
	/// <summary>
	/// Calculates the spherical aberration factor to use for camera steps using the actual current ring in the shape
	/// </summary>
	/// <param name="ringNo"></param>
	/// <param name="weightsRGB"></param>
	/// <returns></returns>
	void applySphericalAberration(float radiusNormalized, CameraLocation& sample);
	/// <summary>
	/// Method which will setup the frame for blending, moving the camera, configuring the shader.
	/// </summary>
	void performRenderFrameSetupWork();
	bool isReshadeStateEmpty()
	{
		std::scoped_lock lock(_reshadeStateMutex);
		return _reshadeStateAtStart.isEmpty();
	}

	CameraToolsConnector& _cameraToolsConnector;
	DepthOfFieldControllerState _state;
	std::vector<CameraLocation> _cameraSteps;

	std::function<void(reshade::api::effect_runtime*)>  _onPresentWorkFunc = nullptr;			// if set, this function is called when the onPresentWork counter reaches 0.

	float _maxBokehSize = 0.25;			// value 'B', so the max diameter of a circle we're going to walk. In world units of the engine
	float _focusDelta = 0.0f;			// value 'A', the relationship between stepping over maxBokehSize and the movement of the pixels that have to be in focus. X specific
	bool _blendFrame = false;			// if true, the shader will blend the curreent frame if state is Render
	float _blendFactor = 0.0f;			// for the shader, the blend factor to use when blending a frame
	float _xAlignmentDelta = 0.0f;		// for the shader, the alignment x delta to use
	float _yAlignmentDelta = 0.0f;		// for the shader, the alignment y delta to use
	float _highlightBoostFactor = 0.9f;
	float _highlightGammaFactor = 2.2f;
	float _sphericalAberrationDimFactor = 0.5f; //dim factor as intensity, 0% to 100% for center sample
	//float _sphericalAberrationDimFactorForFrame = 1.0f; //per-sample weight for spherical aberration
	float _sampleWeightRGB[3] = {1.0f, 1.0f, 1.0f};

	MagnifierSettings _magnificationSettings;

	int _onPresentWorkCounter = 0;		// if 0, reshadeBeginEffectsCalled will call onPresentWorkFunc (if set), otherwise this counter is decreased.

	DepthOfFieldBlurType _blurType = DepthOfFieldBlurType::Circular;
	DepthOfFieldRenderFrameState _renderFrameState = DepthOfFieldRenderFrameState::Off;
	int _frameWaitCounter = 0;		// When 0 move the render state to the next state, otherwise decrease
	int _currentFrame = -1;		// < 0: no frame, >= 0 the current frame, 0 based.
	bool _renderPaused = false;

	int _numberOfFramesToRender = 0;
	int _numberOfFramesToWaitPerFrame = 1;
	int _quality;		// # of circles
	int _numberOfPointsInnermostRing;
	float _ringAngleOffset = 0.0f;
	float _anamorphicFactor = 1.0f;
	DepthOfFieldRenderOrder _renderOrder = DepthOfFieldRenderOrder::InnerRingToOuterRing;
	bool _showProgressBarAsOverlay = true;
	ApertureShapeSettings _apertureShapeSettings;

	ReshadeStateSnapshot _reshadeStateAtStart;
	std::mutex _reshadeStateMutex;

	float _debugVal1 = 0.0f;
	float _debugVal2 = 0.0f;
	bool _debugBool1 = false;
	bool _debugBool2 = false;
};

