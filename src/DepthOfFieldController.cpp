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

DepthOfFieldController::DepthOfFieldController(CameraToolsConnector& connector) : _cameraToolsConnector(connector), _state(DepthOfFieldControllerState::Off), _quality(1), _numberOfPointsInnermostRing(5)
{
}


void DepthOfFieldController::setMaxBokehSize(reshade::api::effect_runtime* runtime, float newValue)
{
	if(DepthOfFieldControllerState::Setup != _state || newValue <=0.0f)
	{
		// not in setup or value is out of range
		return;
	}

	_maxBokehSize = newValue;
	// we have to move the camera over the new distance. We move relative to the start position.
	_cameraToolsConnector.moveCameraMultishot(_maxBokehSize, _maxBokehSize, 0.0f, true);
	// the value is passed to the shader next present call
}


void DepthOfFieldController::setXYFocusDelta(reshade::api::effect_runtime* runtime, float newValueX, float newValueY)
{
	if(DepthOfFieldControllerState::Setup!=_state)
	{
		// not in setup
		return;
	}
	_xFocusDelta = newValueX;
	_yFocusDelta = newValueY;
	// set the uniform in the shader for blending the new framebuffer so the user has visual feedback
	setUniformFloat2Variable(runtime, "FocusDelta", _xFocusDelta, _yFocusDelta);
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
	if(_reshadeStateAtStart.isEmpty())
	{
		return;
	}

	switch(_state)
	{
		case DepthOfFieldControllerState::Start: 
		case DepthOfFieldControllerState::Setup: 
		case DepthOfFieldControllerState::Rendering: 
		case DepthOfFieldControllerState::Done:
			setUniformIntVariable(runtime, "SessionState", (int)_state);
			setUniformFloat2Variable(runtime, "FocusDelta", _xFocusDelta, _yFocusDelta);
			setUniformBoolVariable(runtime, "BlendFrame", _blendFrame);
			setUniformFloatVariable(runtime, "BlendFactor", _blendFactor);
			setUniformFloat2Variable(runtime, "AlignmentDelta", _xAlignmentDelta, _yAlignmentDelta);
			break;
	}

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

	{
		std::scoped_lock lock(_reshadeStateMutex);
		_reshadeStateAtStart.obtainReshadeState(runtime);
	}

	// set uniform variable 'SessionState' to 1 (start)
	_state = DepthOfFieldControllerState::Start;
	setUniformIntVariable(runtime, "SessionState", (int)_state);

	// set framecounter to 3 so we wait 3 frames before moving on to 'Setup'
	_onPresentWorkCounter = 3;	// wait 3 frames
	_onPresentWorkFunc = [&](reshade::api::effect_runtime* r)
	{
		this->_state = DepthOfFieldControllerState::Setup;
		// we have to move the camera over the new distance. We move relative to the start position.
		_cameraToolsConnector.moveCameraMultishot(_maxBokehSize, _maxBokehSize, 0.0f, true);
	};
}


void DepthOfFieldController::endSession(reshade::api::effect_runtime* runtime)
{
	_state = DepthOfFieldControllerState::Off;
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
		handleRenderStateFrame();
	}

	// Then make sure the shader knows our changed data...

	// always write the variables, as otherwise they'll lose their value when the user e.g. hotsamples.
	writeVariableStateToShader(runtime);
}


void DepthOfFieldController::handleRenderStateFrame()
{
	if(_state!=DepthOfFieldControllerState::Rendering)
	{
		return;
	}

	switch(_renderFrameState)
	{
		case DepthOfFieldRenderFrameState::Off:
			// no-op
			break;
		case DepthOfFieldRenderFrameState::FrameSetup:
			{
				// move camera and set counter and move to next state
				_blendFactor = 1.0f / (static_cast<float>(_currentFrame) + 2.0);
				auto& currentFrameData = _cameraSteps[_currentFrame];
				_cameraToolsConnector.moveCameraMultishot(currentFrameData.xDelta, currentFrameData.yDelta, 0.0f, true);
				_xAlignmentDelta = currentFrameData.xAlignmentDelta;
				_yAlignmentDelta = currentFrameData.yAlignmentDelta;
				_frameWaitCounter = _numberOfFramesToWaitPerFrame;
				_renderFrameState = DepthOfFieldRenderFrameState::FrameWait;
				reshade::log_message(reshade::log_level::info, IGCS::Utils::formatString("\t\tSetup frame for step: %d. xDelta: %.5f, yDelta: %.5f, xAlignmentDelta: %.5f, yAlignmentDelta: %.5f", _currentFrame, currentFrameData.xDelta, currentFrameData.yDelta, currentFrameData.xAlignmentDelta, currentFrameData.yAlignmentDelta).c_str());
			}
			break;
		case DepthOfFieldRenderFrameState::FrameWait:
			{
				// check if counter is 0. If so, switch to next state, if not, decrease and do nothing
				if(_frameWaitCounter <= 0)
				{
					_frameWaitCounter = 0;
					// move to next state
					_renderFrameState = DepthOfFieldRenderFrameState::FrameBlend;
				}
				else
				{
					_frameWaitCounter--;
				}
			}
			break;
		case DepthOfFieldRenderFrameState::FrameBlend:
			{
				// Set blend flag to true and move to next state
				_blendFrame = true;
				_renderFrameState = DepthOfFieldRenderFrameState::FrameDone;
			}
			break;
		case DepthOfFieldRenderFrameState::FrameDone:
			{
				reshade::log_message(reshade::log_level::info, IGCS::Utils::formatString("\t\tDone frame: %d.", _currentFrame).c_str());
				_blendFrame = false;
				_currentFrame++;
				if(_currentFrame >= _numberOfFramesToRender)
				{
					// we're done rendering
					_renderFrameState = DepthOfFieldRenderFrameState::Off;
					_state = DepthOfFieldControllerState::Done;
					reshade::log_message(reshade::log_level::info, "Done with dof session");
				}
				else
				{
					// back to setup for the next frame
					_renderFrameState = DepthOfFieldRenderFrameState::FrameSetup;
				}
			}
			break;
	}
}


void DepthOfFieldController::createLinearDoFPoints()
{
	_cameraSteps.clear();
	bool vertical = _debugBool1;
	for(int ringNo = 1;ringNo<=_quality;ringNo++)
	{
		// for testing, first a linear move like a lightfield.
		const float stepDelta = (float)ringNo / (float)_quality;
		float xDelta = vertical ? 0.0f :_maxBokehSize * stepDelta;
		float yDelta = vertical ? _maxBokehSize * stepDelta : 0.0f;
		float xAlignmentDelta = stepDelta * -_xFocusDelta;
		float yAlignmentDelta = stepDelta * _yFocusDelta;
		_cameraSteps.push_back({ xDelta, yDelta, vertical ? 0.0f : xAlignmentDelta, vertical? yAlignmentDelta : 0.0f });
	}
}


void DepthOfFieldController::createCircleDoFPoints()
{
	_cameraSteps.clear();
	float pointsFirstRing = (float)_numberOfPointsInnermostRing;
	float pointsOnRing = pointsFirstRing;
	float maxBokehRadius = _maxBokehSize / 2.0f;
	for(int ringNo = 1; ringNo <= _quality; ringNo++)
	{
		float anglePerPoint = 6.28318530717958f / pointsOnRing;
		float angle = anglePerPoint;
		const float ringDistance = (float)ringNo / (float)_quality;
		for(int pointNumber = 0;pointNumber<pointsOnRing;pointNumber++)
		{
			float sinAngle = sin(angle);
			float cosAngle = cos(angle);
			float xDelta = (maxBokehRadius * ringDistance) * cosAngle;
			float yDelta = (maxBokehRadius * ringDistance)* sinAngle;
			_cameraSteps.push_back({ xDelta, yDelta, ringDistance * cosAngle * -(_xFocusDelta / 2.0f), ringDistance * sinAngle * (_yFocusDelta / 2.0f)});
			angle += anglePerPoint;
		}

		pointsOnRing += pointsFirstRing;
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

	// calculate the positions of the camera
	// For testing, we're first going to render only horizontally, to the left. Moving camera to the left means our focus points *in the image* moves to the right so we align the image
	// by moving it to the left!
		// TODO ring coords
		// Use code from: https://www.shadertoy.com/view/ctjcRz
		/*

#define NUM_RINGS 10

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 uv = fragCoord/iResolution.xy;


	uv = uv * 2.0 - 1.0;
	uv.x *= iResolution.x / iResolution.y;
	//uv *= 2.5;

	vec2 mousemod = iMouse.xy / iResolution.xy;


	vec3 col = vec3(0.0);

	float BLUR_KERNEL_ROUNDNESS = mousemod.y;
	int NUM_VTX = int(mix(3.,8.,mousemod.x));

	int mult = max(1, 6 - NUM_VTX);

	float phi = radians(360.0 / float(NUM_VTX));
	mat2 vertexmat = mat2(cos(phi), sin(phi), -sin(phi), cos(phi));
	vec2 thisvertex = vec2(0.0, 1.0);				// Rotate this coordinate to rotate the shape.

	for(int i = 0; i < NUM_VTX; i++)
	{
		vec2 nextvertex = thisvertex * vertexmat;

		for(int j = 1; j <= NUM_RINGS; j++)
		for(int k = 0; k < j * mult; k++)
		{
			float curr_radius = float(j) / float(NUM_RINGS);

			float numsamples = float(j * mult);
			float pos_on_secant = float(k) / numsamples;

			//fix distribution - lerping offsets to normalized causes fucked up distribution
			float b = tan(phi * (pos_on_secant - 0.5));
			float x = b * (1.0 + vertexmat[0][0]) / vertexmat[0][1] * 0.5 + 0.5;

			pos_on_secant = mix(pos_on_secant, x, BLUR_KERNEL_ROUNDNESS);

			vec2 current_offset = mix(thisvertex, nextvertex, pos_on_secant);
			current_offset -= current_offset * (BLUR_KERNEL_ROUNDNESS - BLUR_KERNEL_ROUNDNESS * inversesqrt(dot(current_offset, current_offset)));

			current_offset *= curr_radius;

			vec2 uv_delta = uv - current_offset;
			col += smoothstep(3.0,2.0, length(uv_delta)/length(fwidth(uv_delta)));



		}
		thisvertex = nextvertex;

	}

	col += smoothstep(3.0,2.0, length(uv)/length(fwidth(uv)));

	fragColor = vec4(col,1.0);
}
		 */


	int pointsFirstRing = 7;			// same as with cinedof. We'll add this number to each circle
	//createLinearDoFPoints();
	createCircleDoFPoints();
	// Log the camera steps created
	reshade::log_message(reshade::log_level::info, "\tCamera steps calculated: ");
	int stepCounter = 0;
	for(auto& stepData : _cameraSteps)
	{
		reshade::log_message(reshade::log_level::info, IGCS::Utils::formatString("\t\tStep: %d. xDelta: %.3f, yDelta: %.5f, xAlignmentDelta: %.5f, yAlignmentDelta: %.5f", stepCounter, stepData.xDelta, stepData.yDelta, stepData.xAlignmentDelta, stepData.yAlignmentDelta).c_str());
		stepCounter++;
	}

	reshade::log_message(reshade::log_level::info, "Start of dof session");
	// set initial shader start state
	_blendFactor = 0.0f;
	_currentFrame = 0;
	_numberOfFramesToRender = _cameraSteps.size();
	_renderFrameState = DepthOfFieldRenderFrameState::FrameSetup;
	_state = DepthOfFieldControllerState::Rendering;
}


// TODO: DOESN'T WORK
void DepthOfFieldController::migrateReshadeState(reshade::api::effect_runtime* runtime)
{
	if(!_cameraToolsConnector.cameraToolsConnected())
	{
		return;
	}
	switch(_state)
	{
		case DepthOfFieldControllerState::Off: 
		case DepthOfFieldControllerState::Cancelling:
			return;
	}
	if(_reshadeStateAtStart.isEmpty())
	{
		return;
	}

	std::scoped_lock lock(_reshadeStateMutex);
	ReshadeStateSnapshot newState;
	newState.obtainReshadeState(runtime);
	// we don't care about the variable values, only about id's and variable names. So we can replace what we have with the new state.
	// If the new state is empty, that's fine, setting variables takes care of that. 
	_reshadeStateAtStart = newState;

	// if the newstate is empty we do nothing. If the new state isn't empty we had a migration and the variables are valid.
	if(!newState.isEmpty() && _state == DepthOfFieldControllerState::Setup)
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

	float x = canvasWidthHeight / 2.0f + topLeftScreenCoord.x;
	float y = canvasWidthHeight / 2.0f + topLeftScreenCoord.y;
	float maxRadius = (canvasWidthHeight / 2.0f)-5;	// to have some edge
	float maxBokehRadius = _maxBokehSize / 2.0f;
	maxBokehRadius = maxBokehRadius < 0.00000001 ? 1.0f : maxBokehRadius;
	ImColor dotColor = ImColor(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
	drawList->AddCircleFilled(ImVec2(x, y), 1.5f, dotColor);
	for(auto& step : _cameraSteps)
	{
		drawList->AddCircleFilled(ImVec2(x + (((step.xDelta / 2.0) / maxBokehRadius) * maxRadius), y + (((step.yDelta / 2.0) / maxBokehRadius) * maxRadius)), 1.5f, dotColor);
	}
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
