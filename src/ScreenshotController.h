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
#include <mutex>
#include <reshade_api.hpp>
#include <string>

#include "ScreenshotSettings.h"

enum class ScreenshotControllerState : short
{
	Off,
	InSession,
};


enum class ScreenshotType : int
{
	CubemapProjectionPanorama=0,
	HorizontalPanorama=1,
	Lightfield=2,
};


enum class CubemapProjectionState: int
{
	Unknown = 0,
	Front = 1,
	Up = 2,
	Down = 3,
	Left = 4,
	Right = 5,
	Back = 6
};

/// <summary>
/// Starts a screenshot session of the specified type
/// </summary>
///	<param name="type">specifies the screenshot session type:</param>
///	0: 360 degrees panorama (using Cube projection, so 6 shots with 90 degree FoV: front, up, down, left, right, back)
///	1: Normal panorama
/// 2: Lightfield (horizontal steps for 3D lightfield)
/// <returns></returns>
typedef bool (__stdcall* IGCS_StartScreenshotSession)(uint8_t type);
/// <summary>
/// Moves the camera in the current session over the specified stepsize. It depends on the session active what stepSize is
/// </summary>
/// <param name="stepSize">
/// if the session is a panorama, stepSize is the angle to rotate over to the right. Negative means the camera will move to the left.
///	if the session is a 360 degrees panorama, stepsize is <=0 and the session controller steps the camera
///	if the session is a lightfield, stepSize is the amount to move the camera to the right. Negative means the camera will move to the left
/// </param>
typedef void (__stdcall* IGCS_MoveCamera)(float stepSize);
/// <summary>
/// Ends the active screenshot session, restoring camera data if required.
/// </summary>
typedef void (__stdcall* IGCS_EndScreenshotSession)();


// Simple controller class which controls the screenshot session.
class ScreenshotController
{
public:
	ScreenshotController();
	~ScreenshotController();

	void configure(ScreenshotSettings settings);
	void startHorizontalPanoramaShot(float currentFoVInDegrees, bool isTestRun);
	void startLightfieldShot(bool isTestRun);
	void startCubemapProjectionPanorama(bool isTestRun);
	ScreenshotControllerState getState() { return _state; }
	void reset();
	bool shouldTakeShot();		// returns true if a shot should be taken, false otherwise. 
	void presentCalled(reshade::api::effect_runtime* runtime);
	/// <summary>
	/// Connects to the camera tools, after the camera tools have called connectFromCameraTools(). it's done this way
	///	so other addins can also communicate with the camera tools without a required call from the camera tools. 
	/// </summary>
	void connectToCameraTools();

	bool cameraToolsConnected() { return nullptr!=_igcs_StartScreenshotSessionFunc;}

private:
	void moveCameraForLightfield(int direction, bool end);
	void moveCameraForPanorama(int direction, bool end);
	void moveCameraForCubemapProjection(bool start);
	void modifyCamera();

	IGCS_StartScreenshotSession _igcs_StartScreenshotSessionFunc = nullptr;
	IGCS_MoveCamera _igcs_MoveCameraFunc = nullptr;
	IGCS_EndScreenshotSession _igcs_EndScreenshotSessionFunc = nullptr;
	
	float _totalFoV = 0.0f;
	float _currentFoV = 0.0f;
	float _distancePerStep = 0.0f;
	float _anglePerStep = 0.0f;
	float _movementSpeed = 0.0f;
	float _rotationSpeed = 0.0f;
	float _overlapPercentagePerPanoShot = 30.0f;
	int _amountOfShotsToTake = 0;
	int _amountOfColumns = 0;
	int _amountOfRows = 0;
	int _convolutionFrameCounter = 0;		// counts down to 0 from _amountOfFramesToWaitBetweenSteps
	int _shotCounter = 0;
	int _numberOfFramesToWaitBetweenSteps = 1;
	int _framebufferWidth = 0;
	int _framebufferHeight = 0;
	CubemapProjectionState _cubeMapState = CubemapProjectionState::Unknown;
	ScreenshotSettings _settingsToUse;

	ScreenshotType _typeOfShot = ScreenshotType::HorizontalPanorama;
	ScreenshotControllerState _state = ScreenshotControllerState::Off;
	bool _isTestRun = false;

	std::string _rootFolder;
	std::vector<std::vector<uint8_t>> _grabbedFrames;

	// Used together to make sure the main thread in System doesn't busy-wait and waits till the grabbing process has been completed.
	std::mutex _waitCompletionMutex;
	std::condition_variable _waitCompletionHandle;

};

