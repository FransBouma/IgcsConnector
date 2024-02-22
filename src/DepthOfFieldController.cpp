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
#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#include "stdafx.h"
#include "DepthOfFieldController.h"

#include "OverlayControl.h"
#include "Utils.h"
#include <random>
#include <algorithm>
#include "CDataFile.h"

DepthOfFieldController::DepthOfFieldController(CameraToolsConnector& connector) : _cameraToolsConnector(connector), _state(DepthOfFieldControllerState::Off), _quality(4), _numberOfPointsInnermostRing(3)
{
}


void DepthOfFieldController::setMaxBokehSize(reshade::api::effect_runtime* runtime, float newValue)
{
	if(DepthOfFieldControllerState::Setup != _state || newValue <=0.0f)
	{
		// not in setup or value is out of range
		return;
	}

	const float oldValue = _maxBokehSize;
	_maxBokehSize = newValue;
	// recalculate focus x/y
	if(oldValue > 0.0f)
	{
		const float ratio = _maxBokehSize / oldValue;
		_focusDelta *= ratio;
	}
	calculateShapePoints();

	// we have to move the camera over the new distance. We move relative to the start position.
	_cameraToolsConnector.moveCameraMultishot(_maxBokehSize, 0.0f, 0.0f, true);
	// the value is passed to the shader next present call
}


void DepthOfFieldController::setXFocusDelta(reshade::api::effect_runtime* runtime, float newValueX)
{
	if(DepthOfFieldControllerState::Setup!=_state)
	{
		// not in setup
		return;
	}
	_focusDelta = newValueX;

	calculateShapePoints();

	// set the uniform in the shader for blending the new framebuffer so the user has visual feedback
	setUniformFloatVariable(runtime, "FocusDelta", _focusDelta);
	// the value is passed to the shader next present call
}


void DepthOfFieldController::displayScreenshotSessionStartError(const ScreenshotSessionStartReturnCode sessionStartResult)
{
	std::string reason = "Unknown error.";
	switch(sessionStartResult)
	{
		case ScreenshotSessionStartReturnCode::Error_CameraNotEnabled:
			reason = "you haven't enabled the camera.";
			break;
		case ScreenshotSessionStartReturnCode::Error_CameraPathPlaying:
			reason = "there's a camera path playing.";
			break;
		case ScreenshotSessionStartReturnCode::Error_AlreadySessionActive:
			reason = "there's already a session active.";
			break;
		case ScreenshotSessionStartReturnCode::Error_CameraFeatureNotAvailable:
			reason = "the camera feature isn't available in the tools.";
			break;
	}
	OverlayControl::addNotification("Depth-of-field session couldn't be started: " + reason);
}


void DepthOfFieldController::writeVariableStateToShader(reshade::api::effect_runtime* runtime)
{
	if(isReshadeStateEmpty())
	{
		std::scoped_lock lock(_reshadeStateMutex);
		_reshadeStateAtStart.obtainReshadeState(runtime);
	}

	setUniformIntVariable(runtime, "SessionState", (int)_state);
	setUniformFloatVariable(runtime, "FocusDelta", _focusDelta);
	setUniformBoolVariable(runtime, "BlendFrame", _blendFrame);
	setUniformFloatVariable(runtime, "BlendFactor", _blendFactor);
	setUniformFloat2Variable(runtime, "AlignmentDelta", _xAlignmentDelta, _yAlignmentDelta);
	setUniformFloatVariable(runtime, "HighlightBoost", _highlightBoostFactor);

	setUniformFloatVariable(runtime, "SampleWeightR", _sampleWeightRGB[0]);
	setUniformFloatVariable(runtime, "SampleWeightG", _sampleWeightRGB[1]);
	setUniformFloatVariable(runtime, "SampleWeightB", _sampleWeightRGB[2]);

	setUniformFloatVariable(runtime, "HighlightGammaFactor", _highlightGammaFactor);
	setUniformBoolVariable(runtime, "ShowMagnifier", _magnificationSettings.ShowMagnifier);
	setUniformFloatVariable(runtime, "MagnificationFactor", _magnificationSettings.MagnificationFactor);
	setUniformFloat2Variable(runtime, "MagnificationArea", _magnificationSettings.WidthMagnifierArea, _magnificationSettings.HeightMagnifierArea);
	setUniformFloat2Variable(runtime, "MagnificationLocationCenter", _magnificationSettings.XMagnifierLocation, _magnificationSettings.YMagnifierLocation);

	setUniformBoolVariable(runtime, "CateyeVignette", _addCatEyeVignette);
	setUniformFloatVariable(runtime, "CateyeRadiusStart", _catEyeRadiusStart);
	setUniformFloatVariable(runtime, "CateyeRadiusEnd", _catEyeRadiusEnd);
	setUniformFloatVariable(runtime, "CateyeIntensity", _catEyeBokehIntensity);
}


void DepthOfFieldController::loadIniFileData(CDataFile& iniFile)
{
	loadFloatFromIni(iniFile, "MaxBokehSize", &_maxBokehSize);
	loadFloatFromIni(iniFile, "HighlightBoostFactor", &_highlightBoostFactor);
	loadFloatFromIni(iniFile, "HighlightGammaFactor", &_highlightGammaFactor);
	loadFloatFromIni(iniFile, "MagnificationAreaWidth", &_magnificationSettings.WidthMagnifierArea);
	loadFloatFromIni(iniFile, "MagnificationAreaHeight", &_magnificationSettings.HeightMagnifierArea);
	loadFloatFromIni(iniFile, "AnamorphicFactor", &_anamorphicFactor);
	loadFloatFromIni(iniFile, "RingAngleOffset", &_ringAngleOffset);
	loadFloatFromIni(iniFile, "RotationAngle", &_apertureShapeSettings.RotationAngle);
	loadFloatFromIni(iniFile, "RoundFactor", &_apertureShapeSettings.RoundFactor);
	loadFloatFromIni(iniFile, "SphericalAberrationDimFactor", &_sphericalAberrationDimFactor);
	loadFloatFromIni(iniFile, "FringeIntensity", &_fringeIntensity);
	loadFloatFromIni(iniFile, "FringeWidth", &_fringeWidth);
	loadFloatFromIni(iniFile, "CAStrength", &_caStrength);
	loadFloatFromIni(iniFile, "CAWidth", &_caWidth);
	loadIntFromIni(iniFile, "NumberOfVertices", &_apertureShapeSettings.NumberOfVertices);
	loadIntFromIni(iniFile, "Quality", &_quality);
	loadIntFromIni(iniFile, "NumberOfPointsInnermostRing", &_numberOfPointsInnermostRing);
	loadIntFromIni(iniFile, "NumberOfFramesToWaitPerFrame", &_numberOfFramesToWait);
	loadBoolFromIni(iniFile, "ShowProgressBarAsOverlay", &_showProgressBarAsOverlay, true);
	loadBoolFromIni(iniFile, "AddCatEyeVignette", &_addCatEyeVignette, false);
	loadFloatFromIni(iniFile, "CatEyeRadiusStart", &_catEyeRadiusStart);
	loadFloatFromIni(iniFile, "CatEyeRadiusEnd", &_catEyeRadiusEnd);
	loadFloatFromIni(iniFile, "CatEyeBokehIntensity", &_catEyeBokehIntensity);

	int intValueFromIni = 0;
	loadIntFromIni(iniFile, "BlurType", &intValueFromIni);
	_blurType = (DepthOfFieldBlurType)intValueFromIni;
	loadIntFromIni(iniFile, "CAType", &intValueFromIni);
	_caType = (DepthOfFieldCAType)intValueFromIni;
}


void DepthOfFieldController::saveIniFileData(CDataFile& iniFile)
{
	iniFile.SetFloat("MaxBokehSize", _maxBokehSize, "", "DepthOfField");
	iniFile.SetFloat("HighlightBoostFactor", _highlightBoostFactor, "", "DepthOfField");
	iniFile.SetFloat("HighlightGammaFactor", _highlightGammaFactor, "", "DepthOfField");
	iniFile.SetFloat("MagnificationAreaWidth", _magnificationSettings.WidthMagnifierArea, "", "DepthOfField");
	iniFile.SetFloat("MagnificationAreaHeight", _magnificationSettings.HeightMagnifierArea, "", "DepthOfField");
	iniFile.SetFloat("AnamorphicFactor", _anamorphicFactor, "", "DepthOfField");
	iniFile.SetFloat("RingAngleOffset", _ringAngleOffset, "", "DepthOfField");
	iniFile.SetFloat("RotationAngle", _apertureShapeSettings.RotationAngle, "", "DepthOfField");
	iniFile.SetFloat("RoundFactor", _apertureShapeSettings.RoundFactor, "", "DepthOfField");
	iniFile.SetFloat("SphericalAberrationDimFactor", _sphericalAberrationDimFactor, "", "DepthOfField");
	iniFile.SetFloat("FringeIntensity", _fringeIntensity, "", "DepthOfField");
	iniFile.SetFloat("FringeWidth", _fringeWidth, "", "DepthOfField");
	iniFile.SetFloat("CAStrength", _caStrength, "", "DepthOfField");
	iniFile.SetFloat("CAWidth", _caWidth, "", "DepthOfField");
	iniFile.SetInt("NumberOfVertices", _apertureShapeSettings.NumberOfVertices, "", "DepthOfField");
	iniFile.SetInt("Quality", _quality, "", "DepthOfField");
	iniFile.SetInt("NumberOfPointsInnermostRing", _numberOfPointsInnermostRing, "", "DepthOfField");
	iniFile.SetInt("NumberOfFramesToWaitPerFrame", _numberOfFramesToWait, "", "DepthOfField");
	iniFile.SetBool("ShowProgressBarAsOverlay", _showProgressBarAsOverlay, "", "DepthOfField");
	iniFile.SetInt("BlurType", (int)_blurType, "", "DepthOfField");
	iniFile.SetInt("CAType", (int)_caType, "", "DepthOfField");
	iniFile.SetBool("AddCatEyeVignette", _addCatEyeVignette, "", "DepthOfField");
	iniFile.SetFloat("CatEyeRadiusStart", _catEyeRadiusStart, "", "DepthOfField");
	iniFile.SetFloat("CatEyeRadiusEnd", _catEyeRadiusEnd, "", "DepthOfField");
	iniFile.SetFloat("CatEyeBokehIntensity", _catEyeBokehIntensity, "", "DepthOfField");
}


void DepthOfFieldController::startSession(reshade::api::effect_runtime* runtime)
{
	if(!_cameraToolsConnector.cameraToolsConnected())
	{
		return;
	}
	const auto sessionStartResult = _cameraToolsConnector.startScreenshotSession((uint8_t)ScreenshotType::MultiShot);
	if(sessionStartResult!=ScreenshotSessionStartReturnCode::AllOk)
	{
		displayScreenshotSessionStartError(sessionStartResult);
		return;
	}

	calculateShapePoints();

	{
		std::scoped_lock lock(_reshadeStateMutex);
		_reshadeStateAtStart.obtainReshadeState(runtime);
	}

	// set uniform variable 'SessionState' to 1 (start)
	_state = DepthOfFieldControllerState::Start;
	_renderPaused = false;
	setUniformIntVariable(runtime, "SessionState", (int)_state);
	// set framecounter to 3 so we wait 3 frames before moving on to 'Setup'
	_onPresentWorkCounter = 3;	// wait 3 frames
	_onPresentWorkFunc = [&](reshade::api::effect_runtime* r)
	{
		this->_state = DepthOfFieldControllerState::Setup;
		// we have to move the camera over the new distance. We move relative to the start position.
		_cameraToolsConnector.moveCameraMultishot(_maxBokehSize, 0.0f, 0.0f, true);
	};
}


void DepthOfFieldController::endSession(reshade::api::effect_runtime* runtime)
{
	_state = DepthOfFieldControllerState::Off;
	_renderPaused = false;
	setUniformIntVariable(runtime, "SessionState", (int)_state);

	if(_cameraToolsConnector.cameraToolsConnected())
	{
		_cameraToolsConnector.endScreenshotSession();
	}
}


void DepthOfFieldController::reshadeBeginEffectsCalled(reshade::api::effect_runtime* runtime)
{
	if(nullptr==runtime || !_cameraToolsConnector.cameraToolsConnected())
	{
		return;
	}
	// First handle data changing work
	if(_onPresentWorkCounter<=0)
	{
		_onPresentWorkCounter = 0;

		if(nullptr!=_onPresentWorkFunc)
		{
			const std::function<void(reshade::api::effect_runtime*)> funcToCall = _onPresentWorkFunc;
			// reset the current func to nullptr so next time we won't call it again.
			_onPresentWorkFunc = nullptr;

			funcToCall(runtime);
		}
	}
	else
	{
		_onPresentWorkCounter--;
	}

	if(DepthOfFieldControllerState::Rendering== _state)
	{
		handlePresentBeforeReshadeEffects();
	}

	// Then make sure the shader knows our changed data...

	// always write the variables, as otherwise they'll lose their value when the user e.g. hotsamples.
	writeVariableStateToShader(runtime);
}


void DepthOfFieldController::reshadeFinishEffectsCalled(reshade::api::effect_runtime* runtime)
{
	if(nullptr == runtime || !_cameraToolsConnector.cameraToolsConnected())
	{
		return;
	}

	if(DepthOfFieldControllerState::Rendering == _state)
	{
		handlePresentAfterReshadeEffects();
	}
}


void DepthOfFieldController::performRenderFrameSetupWork()
{
	// move camera and set counter and move to next state
	if(_currentStepFrame < _cameraSteps.size())
	{
		const auto& currentStepFrameData = _cameraSteps[_currentStepFrame];
		_cameraToolsConnector.moveCameraMultishot(currentStepFrameData.xDelta, currentStepFrameData.yDelta, 0.0f, true);
	}
	if(_currentBlendFrame >= 0)
	{
		const auto& currentBlendFrameData = _cameraSteps[_currentBlendFrame];
		_xAlignmentDelta = currentBlendFrameData.xAlignmentDelta;
		_yAlignmentDelta = currentBlendFrameData.yAlignmentDelta;
		switch(_frameWaitType)
		{
			case DepthOfFieldFrameWaitType::Fast:
				_frameWaitCounter = 0;
				break;
			case DepthOfFieldFrameWaitType::Classic:
				_frameWaitCounter = _numberOfFramesToWait;
				break;
		}
		_blendFactor = 1.0f / (static_cast<float>(_currentBlendFrame) + 1.0f);		// frame start at 0 so +1, to get 1/1=100% blend factor for first frame

		//since the lerp blending implicitly already divides the sum by N, we must not do it again, so compensate
		const float numSamples = _cameraSteps.size();
		_sampleWeightRGB[0] = currentBlendFrameData.sampleWeightRGB[0] * numSamples;
		_sampleWeightRGB[1] = currentBlendFrameData.sampleWeightRGB[1] * numSamples;
		_sampleWeightRGB[2] = currentBlendFrameData.sampleWeightRGB[2] * numSamples;
	}
	// Set the framestate to wait so the counter will take effect.
	_renderFrameState = DepthOfFieldRenderFrameState::FrameWait;
}


void DepthOfFieldController::handlePresentBeforeReshadeEffects()
{
	if(_state!=DepthOfFieldControllerState::Rendering)
	{
		return;
	}

	switch(_renderFrameState)
	{
		case DepthOfFieldRenderFrameState::Off:
		case DepthOfFieldRenderFrameState::FrameBlending:
		case DepthOfFieldRenderFrameState::Start:
			// no-op
			break;
		case DepthOfFieldRenderFrameState::FrameWait:
			{
				// check if counter is 0. If so, switch to next state, if not, decrease and do nothing
				if(_frameWaitCounter <= 0)
				{
					_frameWaitCounter = 0;
					if(_currentBlendFrame >= 0)
					{
						// Ready to blend. As we're currently before the reshade effects are handled but after the frame has been drawn by the engine
						// we can set blendFrame to true here and the shader will blend the current framebuffer this frame.
						// This works because after this method, the uniforms are written to the shader, so the shader will pick the new value up
						// when it's being drawn 
						_blendFrame = true;
					}
					// Setting the state to blending as we're blending after this method. The handling of this event is done in handlePresentAfterReshadeEffects
					_renderFrameState = DepthOfFieldRenderFrameState::FrameBlending;
				}
				else
				{
					_frameWaitCounter--;
				}
			}
			break;
	}
}


void DepthOfFieldController::handlePresentAfterReshadeEffects()
{
	if(_state != DepthOfFieldControllerState::Rendering)
	{
		return;
	}

	switch(_renderFrameState)
	{
		case DepthOfFieldRenderFrameState::Off:
		case DepthOfFieldRenderFrameState::FrameWait:
			// no-op
			break;
		case DepthOfFieldRenderFrameState::Start:
			// start state of the whole process. Only arriving here once per render session.
			performRenderFrameSetupWork();
			break;
		case DepthOfFieldRenderFrameState::FrameBlending:
			{
				// Blending work has taken place, we're now done with that as the shader has run. We switch it off by resetting the variable.
				// This variable is written to the shader at the end of the handler called before the reshade effects will be rendered (reshadeBeginEffectsCalled), so
				// it will take effect then. (the shader isn't run before that point so it's ok).
				_blendFrame = false;
				if(!_renderPaused)
				{
					_currentStepFrame++;
					_currentBlendFrame++;
					if(_currentBlendFrame >= _numberOfFramesToRender)
					{
						// we're done rendering
						_renderFrameState = DepthOfFieldRenderFrameState::Off;
						_state = DepthOfFieldControllerState::Done;
						reshade::log_message(reshade::log_level::info, "Dof render session completed");
					}
					else
					{
						// back to setup for the next frame
						performRenderFrameSetupWork();
					}
				}
			}
			break;
	}
}


void DepthOfFieldController::applySphericalAberration(float radiusNormalized, CameraLocation& sample)
{
	//radius^4 yields plausible results, see for analysis https://jtra.cz/stuff/essays/bokeh/index.html
	//this is theoretically incorrect, as aberration should be caused by light taking different paths, i.e. it could be
	//emulated by modifying the camera angles and correctly deliver inverted bokeh in foreground, 
	//however this would yield blurry focal areas which we don't want. So approximate it with sample masking

	float aberrationCurve = radiusNormalized * radiusNormalized;
	aberrationCurve *= aberrationCurve; 

	//lerp between flat profile and curve with intensity 0 in center
	//*0.99 -> ensure samples in center are never _exactly_ zero, this avoids issues with renormalized sample weights
	const float aberrationFactor = (1.0f - _sphericalAberrationDimFactor * 0.99f) + _sphericalAberrationDimFactor * aberrationCurve * 0.99f;

	sample.sampleWeightRGB[0] *= aberrationFactor;
	sample.sampleWeightRGB[1] *= aberrationFactor;
	sample.sampleWeightRGB[2] *= aberrationFactor;
}


float DepthOfFieldController::calculateChannelDimFactor(float angleSegment, float segmentAngleMin, int numberOfSegments)
{
	// using Iq's parabola using k==0.5, see: https://www.desmos.com/calculator/aszway25gw and https://iquilezles.org/articles/functions/
	const float segmentSize = 1.0f / (float)(numberOfSegments ==0 ? 1 : numberOfSegments);
	const float angleToSegmentNormalized = IGCS::Utils::clampEx(angleSegment - segmentAngleMin, 0.0f, segmentSize) / segmentSize;
	return std::pow(4.0f * angleToSegmentNormalized * (1.0 - angleToSegmentNormalized), 0.5f);
}


void DepthOfFieldController::applyFringe(float ringRadiusNormalized, float sampleAngle, CameraLocation& sample)
{
	const float transitionWidth = 0.5f / (float)_quality;
	// perform a linear step with the spacing of a ring radius
	
	//(x-a)/(b-a)
	const float fringeRampStart = 1.0f - _fringeWidth - transitionWidth;
	const float fringeRampEnd   = 1.0f - _fringeWidth + transitionWidth;
	const float fringeMask = IGCS::Utils::clampEx((ringRadiusNormalized - fringeRampStart) / (fringeRampEnd - fringeRampStart), 0.0f, 1.0f);
	const float fringeFactor = (1.0f - _fringeIntensity) * (1.0f - fringeMask) + fringeMask;

	// factors for the dimming. 1.0 means visible, 0.0 means dimmed 100%
	float blueFactor = 1.0f;
	float greenFactor = 1.0f;
	float redFactor = 1.0f;
	const float angleSegment = sampleAngle / 6.28318530717958f;
	// for 3 segments: 
	// 0-0.33333: blue, 0.33333-0.6666: green, 0.6666-1: red
	// for 2 segments, the two colors in the type both have 0.5

	DepthOfFieldColorChannel segmentOneProminentColor = DepthOfFieldColorChannel::Red;
	DepthOfFieldColorChannel segmentTwoProminentColor = DepthOfFieldColorChannel::Green;
	DepthOfFieldColorChannel segmentThreeProminentColor = DepthOfFieldColorChannel::Blue;

	int numberOfSegments = 3;
	switch(_caType)
	{
		case DepthOfFieldCAType::RGB:
			// The defaults are ok for this setup
			break;
		case DepthOfFieldCAType::RG:
			numberOfSegments = 2;
			// prominent color defaults are ok for this setup
			break;
		case DepthOfFieldCAType::RB:
			numberOfSegments = 2;
			segmentTwoProminentColor = DepthOfFieldColorChannel::Blue;
			break;
		case DepthOfFieldCAType::BG:
			numberOfSegments = 2;
			segmentOneProminentColor = DepthOfFieldColorChannel::Blue;
			break;
	}
	const float segmentOneMaxAngle = 1.0f / (float)numberOfSegments;
	const float segmentTwoMaxAngle = 2.0f / (float)numberOfSegments;

	bool redChannelDimmable = true;
	bool greenChannelDimmable = true;
	bool blueChannelDimmable = true;
	float dimFactor = 0.0f;
	// cheap filter out segments and apply operands. Per segment a channel is prominent and the others are dimmed graciously
	// execution flow will always arrive in 1 if handler below so we can use that to our advantage with setting flags for the final calculations
	if(angleSegment <= segmentOneMaxAngle)
	{
		dimFactor = 1.0f - calculateChannelDimFactor(angleSegment, 0.0f, numberOfSegments);
		redChannelDimmable = segmentOneProminentColor != DepthOfFieldColorChannel::Red;
		greenChannelDimmable = segmentOneProminentColor != DepthOfFieldColorChannel::Green;
		blueChannelDimmable = segmentOneProminentColor != DepthOfFieldColorChannel::Blue;
	}
	else
	{
		if(angleSegment <= segmentTwoMaxAngle)
		{
			// last segment for 2 colors, middle segment for 3 colors
			dimFactor = 1.0f - calculateChannelDimFactor(angleSegment, segmentOneMaxAngle, numberOfSegments);
			redChannelDimmable = segmentTwoProminentColor != DepthOfFieldColorChannel::Red;
			greenChannelDimmable = segmentTwoProminentColor != DepthOfFieldColorChannel::Green;
			blueChannelDimmable = segmentTwoProminentColor != DepthOfFieldColorChannel::Blue;
		}
		else
		{
			// last segment for 3 colors, for 2 color ca we'll never end up here. 
			dimFactor = 1.0f - calculateChannelDimFactor(angleSegment, segmentTwoMaxAngle, numberOfSegments);
			redChannelDimmable = segmentThreeProminentColor != DepthOfFieldColorChannel::Red;
			greenChannelDimmable = segmentThreeProminentColor != DepthOfFieldColorChannel::Green;
			blueChannelDimmable = segmentThreeProminentColor != DepthOfFieldColorChannel::Blue;
		}
	}

	const float caRampStart = 1.0f - _caWidth - transitionWidth;
	const float caRampEnd = 1.0f - _caWidth + transitionWidth;
	const float caMask = IGCS::Utils::clampEx((ringRadiusNormalized - caRampStart) / (caRampEnd - caRampStart), 0.0f, 1.0f);
	const float caFactor = _caStrength * caMask;

	redFactor = redChannelDimmable ? IGCS::Utils::lerp(dimFactor, 1.0f, (1.0f - caFactor)) : redFactor;
	greenFactor = greenChannelDimmable ? IGCS::Utils::lerp(dimFactor, 1.0f, (1.0f - caFactor)) : greenFactor;
	blueFactor = blueChannelDimmable ? IGCS::Utils::lerp(dimFactor, 1.0f, (1.0f - caFactor)) : blueFactor;

	sample.sampleWeightRGB[0] *= fringeFactor * redFactor;
	sample.sampleWeightRGB[1] *= fringeFactor * greenFactor;
	sample.sampleWeightRGB[2] *= fringeFactor * blueFactor;
}


void DepthOfFieldController::createCircleDoFPoints()
{
	_cameraSteps.clear();

	CameraLocation center = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
	applySphericalAberration(0.0f, center);	
	applyFringe(0.0f, 0.0f, center);	
	_cameraSteps.push_back(center);

	const float pointsFirstRing = (float)_numberOfPointsInnermostRing;
	float pointsOnRing = pointsFirstRing;
	const float maxBokehRadius = _maxBokehSize / 2.0f;
	const float focusDeltaHalf = _focusDelta / 2.0f;
	for(int ringNo = 1; ringNo <= _quality; ringNo++)
	{
		const float anglePerPoint = 6.28318530717958f / pointsOnRing;
		float angle = ((float)ringNo * _ringAngleOffset);
		const float ringDistance = (float)ringNo / (float)_quality;
		for(int pointNumber = 0;pointNumber<pointsOnRing;pointNumber++)
		{
			const float sinAngle = sin(angle);
			const float cosAngle = cos(angle);
			const float x = ringDistance * cosAngle * _anamorphicFactor;
			const float y = ringDistance * sinAngle;
			const float xDelta = maxBokehRadius * x;
			const float yDelta = maxBokehRadius * y;

			CameraLocation sample = {xDelta, yDelta, x * -focusDeltaHalf, y * focusDeltaHalf, 1.0f, 1.0f, 1.0f};	
			applySphericalAberration(ringDistance, sample);
			// angle 0 is on the right of the circle but we want it to be up top, so we subtract 1/2pi from it so the angle for the color is transposed 90 degrees.
			applyFringe(ringDistance, fmod((angle-(6.28318530717958f / 4.0f)) + 6.28318530717958f, 6.28318530717958f), sample);
			_cameraSteps.push_back(sample);

			angle += anglePerPoint;
			angle = fmod(angle, 6.28318530717958f);
		}

		pointsOnRing += pointsFirstRing;
	}

	renormalizeBokehWeights();
	applyRenderOrder();
}


void DepthOfFieldController::createApertureShapedDoFPoints()
{
	_cameraSteps.clear();

	CameraLocation center = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
	applySphericalAberration(0.0f, center);
	applyFringe(0.0f, 0.0f, center);
	_cameraSteps.push_back(center);

	// sanitize input for 4 vertex elements
	if(4 == _apertureShapeSettings.NumberOfVertices)
	{
		if(_ringAngleOffset<-0.015f || _ringAngleOffset > 0.015f)
		{
			_ringAngleOffset = 0.0f;
		}
	}

	const float maxBokehRadius = _maxBokehSize / 2.0f;
	const float focusDeltaHalf = _focusDelta / 2.0f;
	const float anglePerVertex = 6.28318530717958f / (float)_apertureShapeSettings.NumberOfVertices;
	for(int ringNo = 1; ringNo <= _quality; ringNo++)
	{
		float vertexAngleForFringe = 0.0f;
		// ring angle offset is applied stronger on inner rings than on outer rings, to keep the outer ring from staying in the same place. 
		float vertexAngle = fmod((_apertureShapeSettings.RotationAngle * 6.28318530717958f) + ((float)(_quality-ringNo) * _ringAngleOffset), 6.28318530717958f);
		const float ringDistance = (float)ringNo / (float)_quality;		
		for(int vertexNo = 0; vertexNo < _apertureShapeSettings.NumberOfVertices; vertexNo++)
		{
			const float sinAngleCurrentVertex = sin(vertexAngle);
			const float cosAngleCurrentVertex = cos(vertexAngle);
			const float nextVertexAngle = fmod(vertexAngle + anglePerVertex, 6.28318530717958f);
			const float sinAngleNextVertex = sin(nextVertexAngle);
			const float cosAngleNextVertex = cos(nextVertexAngle);
			const float xCurrentVertex = ringDistance * cosAngleCurrentVertex;
			const float yCurrentVertex = ringDistance * sinAngleCurrentVertex;
			const float xNextVertex = ringDistance * cosAngleNextVertex;
			const float yNextVertex = ringDistance * sinAngleNextVertex;
			const float pointStepSize = 1.0f / (float)ringNo;
			float pointStep = pointStepSize;
			for(int pointNumber = 0; pointNumber < ringNo; pointNumber++)
			{
				const float pointAngle = IGCS::Utils::lerp(vertexAngle, vertexAngle + anglePerVertex, pointStep);
				const float pointAngleForFringe = IGCS::Utils::lerp(vertexAngleForFringe, vertexAngleForFringe + anglePerVertex, pointStep);
				const float sinPointAngle = sin(pointAngle);
				const float cosPointAngle = cos(pointAngle);
				const float xRoundPoint = ringDistance * cosPointAngle;
				const float yRoundPoint = ringDistance * sinPointAngle;
				const float xLinePoint = IGCS::Utils::lerp(xCurrentVertex, xNextVertex, pointStep);
				const float yLinePoint = IGCS::Utils::lerp(yCurrentVertex, yNextVertex, pointStep);
				float x = IGCS::Utils::lerp(xLinePoint, xRoundPoint, _apertureShapeSettings.RoundFactor);
				float y = IGCS::Utils::lerp(yLinePoint, yRoundPoint, _apertureShapeSettings.RoundFactor);
				//cannot use ringDistance in polygonal mode, as spherical aberration is purely a factor of radius and ringDistance follows aperture shape
				//hence use euclidean distance from center instead. However, spherical aberration happens before anamorphic film squeeze
				//as the anamorphic lens is the last lens in front of the sensor/film
				const float radiusNormalized = sqrtf(x * x + y * y); 
				x *= _anamorphicFactor; //apply scaling here after calculating spherical aberration
				const float xDelta = maxBokehRadius * x;
				const float yDelta = maxBokehRadius * y;
				CameraLocation sample = {xDelta, yDelta, x * -focusDeltaHalf, y * focusDeltaHalf, 1.0f, 1.0f, 1.0f};	
				applySphericalAberration(radiusNormalized, sample);	
				applyFringe(ringDistance, pointAngleForFringe, sample);
				_cameraSteps.push_back(sample);
				pointStep += pointStepSize;
			}
			vertexAngle += anglePerVertex;
			vertexAngle = fmod(vertexAngle, 6.28318530717958f);
			vertexAngleForFringe += anglePerVertex;
			vertexAngleForFringe = fmod(vertexAngleForFringe, 6.28318530717958f);
		}
	}

	renormalizeBokehWeights();
	applyRenderOrder();
}


void DepthOfFieldController::renormalizeBokehWeights()
{
	//renormalize bokeh weights so they do not scale the exposure or add a tint	
	float weightSumRGB[3] = { 0.0f, 0.0f, 0.0f };
	for(const auto& step : _cameraSteps)
	{
		weightSumRGB[0] += step.sampleWeightRGB[0];
		weightSumRGB[1] += step.sampleWeightRGB[1];
		weightSumRGB[2] += step.sampleWeightRGB[2];
	}
	for(auto& step : _cameraSteps)
	{
		step.sampleWeightRGB[0] /= weightSumRGB[0];
		step.sampleWeightRGB[1] /= weightSumRGB[1];
		step.sampleWeightRGB[2] /= weightSumRGB[2];
	}
}


void DepthOfFieldController::applyRenderOrder()
{
	switch(_renderOrder)
	{
		case DepthOfFieldRenderOrder::InnerRingToOuterRing:
			// nothing, we're already having the points in the right order
			break;
		case DepthOfFieldRenderOrder::OuterRingToInnerRing:
			// reverse the container.
			std::ranges::reverse(_cameraSteps);
			break;
		case DepthOfFieldRenderOrder::Randomized:
			std::ranges::shuffle(_cameraSteps, std::random_device());
			break;
		default:;
	}
}


void DepthOfFieldController::calculateShapePoints()
{
	switch(_blurType)
	{
		case DepthOfFieldBlurType::ApertureShape:
			createApertureShapedDoFPoints();
			break;
		case DepthOfFieldBlurType::Circular: 
			createCircleDoFPoints();
			break;
	}
}


void DepthOfFieldController::startRender(reshade::api::effect_runtime* runtime)
{
	if(nullptr == runtime || !_cameraToolsConnector.cameraToolsConnected())
	{
		return;
	}

	if(_state!=DepthOfFieldControllerState::Setup)
	{
		// not in the right previous state
		return;
	}

	reshade::log_message(reshade::log_level::info, "Dof render session started");

	// set initial shader start state
	_blendFrame = false;
	_blendFactor = 0.0f;
	_currentStepFrame = 0;
	switch(_frameWaitType)
	{
		case DepthOfFieldFrameWaitType::Classic:
			_currentBlendFrame = 0;
			break;
		case DepthOfFieldFrameWaitType::Fast:
			_currentBlendFrame = 0-_numberOfFramesToWait;
			break;
	}
	_numberOfFramesToRender = _cameraSteps.size();
	_renderFrameState = DepthOfFieldRenderFrameState::Start;
	_state = DepthOfFieldControllerState::Rendering;
}


void DepthOfFieldController::migrateReshadeState(reshade::api::effect_runtime* runtime)
{
	if(!_cameraToolsConnector.cameraToolsConnected())
	{
		return;
	}
	switch(_state)
	{
		case DepthOfFieldControllerState::Cancelling:
			return;
	}
	if(isReshadeStateEmpty())
	{
		return;
	}

	{
		std::scoped_lock lock(_reshadeStateMutex);
		ReshadeStateSnapshot newState;
		newState.obtainReshadeState(runtime);
		// we don't care about the variable values, only about id's and variable names. So we can replace what we have with the new state.
		// If the new state is empty, that's fine, setting variables takes care of that. 
		_reshadeStateAtStart = newState;
	}
		
	// if the newstate is empty we do nothing. If the new state isn't empty we had a migration and the variables are valid.
	if(!isReshadeStateEmpty() && _state == DepthOfFieldControllerState::Setup)
	{
		// we now restart the session. This is necessary because we lose the cached start texture.
		endSession(runtime);
		startSession(runtime);
	}
}


void DepthOfFieldController::drawShape(ImDrawList* drawList, ImVec2 topLeftScreenCoord, float canvasWidthHeight)
{
	if(_cameraSteps.size()<=0)
	{
		return;
	}

	const float x = canvasWidthHeight / 2.0f + topLeftScreenCoord.x;
	const float y = canvasWidthHeight / 2.0f + topLeftScreenCoord.y;
	const float maxRadius = (canvasWidthHeight / 2.0f)-5.0f;	// to have some space around the edge
	float maxBokehRadius = _maxBokehSize / 2.0f;
	maxBokehRadius = maxBokehRadius < FLT_EPSILON ? 1.0f : maxBokehRadius;

	//aberration weights are normalized to sum up to 1, meaning if inner samples are weighted < 1, outer samples must be weighted > 1
	//but since we can't display values > 1, we need to figure out the maximum value. As we might have shuffled them for random order rendering
	//we can't just take the busy bokeh factor of innermost or outermost ring.
	float maxChannel = 0.0f; //scale all channels by the same amount
	for(const auto& step : _cameraSteps)
	{
		maxChannel = std::max(maxChannel, step.sampleWeightRGB[0]);
		maxChannel = std::max(maxChannel, step.sampleWeightRGB[1]);
		maxChannel = std::max(maxChannel, step.sampleWeightRGB[2]);
	}

	for(const auto& step : _cameraSteps)
	{
		ImColor dotColor = ImColor(step.sampleWeightRGB[0] / maxChannel, step.sampleWeightRGB[1] / maxChannel, step.sampleWeightRGB[2] / maxChannel);
		// our (0,0) for rendering is top left, however the (0, 0) for the canvas is bottom left.
		drawList->AddCircleFilled(ImVec2(x + ((step.xDelta / maxBokehRadius) * maxRadius), y - ((step.yDelta / maxBokehRadius) * maxRadius)), 1.5f, dotColor);
	}
}


void DepthOfFieldController::renderProgressBar()
{
	const int totalAmountOfSteps = _cameraSteps.size();
	const float progress = (float)_currentBlendFrame / (float)totalAmountOfSteps;
	const float progress_saturated = IGCS::Utils::clampEx(progress, 0.0f, 1.0f);
	char buf[128];
	sprintf(buf, "%d/%d", (int)(progress_saturated * totalAmountOfSteps), totalAmountOfSteps);
	ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), buf);
}


void DepthOfFieldController::renderOverlay()
{
	if(_state!=DepthOfFieldControllerState::Rendering || _cameraSteps.size()<=0 || !_showProgressBarAsOverlay)
	{
		return;
	}

	ImGui::SetNextWindowBgAlpha(0.9f);
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	if(ImGui::Begin("IgcsConnector_DoFProgress", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
	{
		renderProgressBar();
	}
	ImGui::End();
}


void DepthOfFieldController::setUniformIntVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, int valueToWrite)
{
	std::scoped_lock lock(_reshadeStateMutex);
	_reshadeStateAtStart.setUniformIntVariable(runtime, "IgcsDof.fx", uniformName, valueToWrite);
}


void DepthOfFieldController::setUniformFloatVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, float valueToWrite)
{
	std::scoped_lock lock(_reshadeStateMutex);
	_reshadeStateAtStart.setUniformFloatVariable(runtime, "IgcsDof.fx", uniformName, valueToWrite);
}


void DepthOfFieldController::setUniformBoolVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, bool valueToWrite)
{
	std::scoped_lock lock(_reshadeStateMutex);
	_reshadeStateAtStart.setUniformBoolVariable(runtime, "IgcsDof.fx", uniformName, valueToWrite);
}


void DepthOfFieldController::setUniformFloat2Variable(reshade::api::effect_runtime* runtime, const std::string& uniformName, float value1ToWrite, float value2ToWrite)
{
	std::scoped_lock lock(_reshadeStateMutex);
	_reshadeStateAtStart.setUniformFloat2Variable(runtime, "IgcsDof.fx", uniformName, value1ToWrite, value2ToWrite);
}


void DepthOfFieldController::loadFloatFromIni(CDataFile& iniFile, const std::string& key, float* toWriteTo)
{
	if(nullptr == toWriteTo)
	{
		return;
	}
	const float value = iniFile.GetFloat(key, "DepthOfField");
	if(value != FLT_MIN)
	{
		*toWriteTo = value;
	}
}


void DepthOfFieldController::loadIntFromIni(CDataFile& iniFile, const std::string& key, int* toWriteTo)
{
	if(nullptr == toWriteTo)
	{
		return;
	}
	const float value = iniFile.GetInt(key, "DepthOfField");
	if(value != INT_MIN)
	{
		*toWriteTo = value;
	}
}

void DepthOfFieldController::loadBoolFromIni(CDataFile& iniFile, const std::string& key, bool* toWriteTo, bool defaultValue)
{
	if(nullptr==toWriteTo)
	{
		return;
	}
	// a little inefficient, but getkey is protected
	const auto boolAsString = iniFile.GetValue(key, "DepthOfField");
	bool valueToUse = defaultValue;
	if(boolAsString.length()>0)
	{
		valueToUse = iniFile.GetBool(key, "DepthOfField");
	}
	*toWriteTo = valueToUse;
}

