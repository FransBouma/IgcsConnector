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
#include "ConstantsEnums.h"

/// <summary>
/// Starts a screenshot session of the specified type
/// </summary>
///	<param name="type">specifies the screenshot session type:</param>
///	0: 360 degrees panorama (using Cube projection, so 6 shots with 90 degree FoV: front, up, down, left, right, back)
///	1: Normal panorama
/// 2: Lightfield (horizontal steps for 3D lightfield)
/// <returns>true if session could be started, false otherwise (e.g. camera isn't enabled, already playing a path etc.)</returns>
typedef bool (__stdcall* IGCS_StartScreenshotSession)(uint8_t type);
/// <summary>
/// Moves the camera in the current session over the specified stepsize. It depends on the session active what stepSize is
/// </summary>
/// <param name="stepSize">
/// if the session is a panorama, stepSize is the angle to rotate over to the right. Negative means the camera will move to the left.
///	if the session is a 360 degrees panorama, stepsize is <=0 and the session controller steps the camera
///	if the session is a lightfield, stepSize is the amount to move the camera to the right. Negative means the camera will move to the left
/// </param>
/// <param name="side">the side to pick for the next shot. 0 for sessions other than 360 degree panos</param>
typedef void (__stdcall* IGCS_MoveCamera)(float stepSize, int side);
/// <summary>
/// Ends the active screenshot session, restoring camera data if required.
/// </summary>
typedef void (__stdcall* IGCS_EndScreenshotSession)();


// Simple controller class which controls the screenshot session.
class ScreenshotController
{
public:
	ScreenshotController();
	~ScreenshotController() = default;

	void configure(std::string rootFolder, int numberOfFramesToWaitBetweenSteps);
	void startHorizontalPanoramaShot(float totalFoVInDegrees, float overlapPercentagePerPanoShot, float currentFoVInDegrees, bool isTestRun);
	void startLightfieldShot(float distancePerStep, int numberOfShots, bool isTestRun);
	void startCubemapProjectionPanorama(bool isTestRun);
	ScreenshotControllerState getState() { return _state; }
	void reset();
	bool shouldTakeShot();		// returns true if a shot should be taken, false otherwise. 
	void presentCalled();
	void reshadeEffectsRendered(reshade::api::effect_runtime* runtime);
	void cancelSession();

	/// <summary>
	/// Connects to the camera tools, after the camera tools have called connectFromCameraTools(). it's done this way
	///	so other addins can also communicate with the camera tools without a required call from the camera tools. 
	/// </summary>
	void connectToCameraTools();
	void completeShotSession();

	bool cameraToolsConnected() { return (nullptr != _igcs_MoveCameraFunc && nullptr != _igcs_EndScreenshotSessionFunc && nullptr != _igcs_StartScreenshotSessionFunc); }

private:
	void waitForShots();
	void saveGrabbedShots();
	void storeGrabbedShot(std::vector<uint8_t>);
	void saveShotToFile(std::string destinationFolder, std::vector<uint8_t> data, int frameNumber);
	std::string createScreenshotFolder();
	void moveCameraForLightfield(int direction, bool end);
	void moveCameraForPanorama(int direction, bool end);
	void moveCameraForCubemapProjection(bool start);
	void modifyCamera();

	IGCS_StartScreenshotSession _igcs_StartScreenshotSessionFunc = nullptr;
	IGCS_MoveCamera _igcs_MoveCameraFunc = nullptr;
	IGCS_EndScreenshotSession _igcs_EndScreenshotSessionFunc = nullptr;
	
	float _pano_totalFoV = 0.0f;
	float _pano_currentFoV = 0.0f;
	float _pano_anglePerStep = 0.0f;
	float _lightField_distancePerStep = 0.0f;
	float _overlapPercentagePerPanoShot = 30.0f;
	int _numberOfShotsToTake = 0;
	int _convolutionFrameCounter = 0;		// counts down to 0 from _amountOfFramesToWaitBetweenSteps
	int _shotCounter = 0;
	int _numberOfFramesToWaitBetweenSteps = 1;
	uint32_t _framebufferWidth = 0;
	uint32_t _framebufferHeight = 0;
	CubemapProjectionState _cubeMapState = CubemapProjectionState::Unknown;
	ScreenshotType _typeOfShot = ScreenshotType::HorizontalPanorama;
	ScreenshotControllerState _state = ScreenshotControllerState::Off;
	ScreenshotFiletype _filetype = ScreenshotFiletype::Jpeg;
	bool _isTestRun = false;

	std::string _rootFolder;
	std::vector<std::vector<uint8_t>> _grabbedFrames;

	// Used together to make sure the main thread in System doesn't busy-wait and waits till the grabbing process has been completed.
	std::mutex _waitCompletionMutex;
	std::condition_variable _waitCompletionHandle;

};

