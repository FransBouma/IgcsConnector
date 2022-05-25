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

#include "ConstantsEnums.h"

/// <summary>
/// Starts a screenshot session of the specified type
/// </summary>
///	<param name="type">specifies the screenshot session type:</param>
///	0: Normal panorama
/// 1: Lightfield (horizontal steps for 3D lightfield)
/// <returns>0 if the session was successfully started or a value > 0 if not:
/// 1 if the camera wasn't enabled
/// 2 if a camera path is playing
/// 3 if there was already a session active
/// 4 if camera feature isn't available
/// 5 if another error occurred</returns>
typedef ScreenshotSessionStartReturnCode (__stdcall* IGCS_StartScreenshotSession)(uint8_t type);
/// <summary>
/// Rotates the camera in the current session over the specified stepAngle.
/// </summary>
/// <param name="stepAngle">
/// stepAngle is the angle (in radians) to rotate over to the right. Negative means the camera will rotate to the left.
/// </param>
///	<remarks>stepAngle isn't divided by movementspeed/rotation speed yet, so it has to be done locally in the camerafeaturebase.</remarks>
typedef void (__stdcall* IGCS_MoveCameraPanorama)(float stepAngle);
/// <summary>
/// Moves the camera up/down/left/right based on the values specified and whether that's relative to the start location or to the current camera location.
/// </summary>
/// <param name="stepLeftRight">The amount to step left/right. Negative values will make the camera move to the left, positive values make the camera move to the right</param>
/// <param name="stepUpDown">The amount to step up/down. Negative values with make the camera move down, positive values make the camera move up</param>
/// <param name="fovDegrees">The fov in degrees to use for the step. If &lt= 0, the value is ignored</param>
/// <param name="fromStartPosition">If true the values specified will be relative to the start location of the session, otherwise to the current location of the camera</param>
typedef void(__stdcall* IGCS_MoveCameraMultishot)(float stepLeftRight, float stepUpDown, float fovDegrees, bool fromStartPosition);
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

	void configure(std::string rootFolder, int numberOfFramesToWaitBetweenSteps, ScreenshotFiletype filetype);
	void startHorizontalPanoramaShot(float totalFoVInDegrees, float overlapPercentagePerPanoShot, float currentFoVInDegrees, bool isTestRun);
	void startLightfieldShot(float distancePerStep, int numberOfShots, bool isTestRun);
	void startDebugGridShot();
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
	void displayScreenshotSessionStartError(ScreenshotSessionStartReturnCode sessionStartResult);

	bool cameraToolsConnected() { return (nullptr != _igcs_MoveCameraPanoramaFunc && nullptr != _igcs_EndScreenshotSessionFunc && nullptr != _igcs_StartScreenshotSessionFunc && nullptr!= _igcs_MoveCameraMultishotFunc); }

private:
	/// <summary>
	/// Starts a screenshot session. Displays the error if one occurs.
	/// </summary>
	/// <returns>true if session could successfully be started, false otherwise</returns>
	bool startSession();
	void waitForShots();
	void saveGrabbedShots();
	void storeGrabbedShot(std::vector<uint8_t>);
	void saveShotToFile(std::string destinationFolder, const std::vector<uint8_t>& data, int frameNumber);
	std::string createScreenshotFolder();
	void moveCameraForLightfield(int direction, bool end);
	void moveCameraForPanorama(int direction, bool end);
	void moveCameraForDebugGrid(int shotCounter, bool end);
	void modifyCamera();
	std::string typeOfShotAsString();

	IGCS_StartScreenshotSession _igcs_StartScreenshotSessionFunc = nullptr;
	IGCS_MoveCameraPanorama _igcs_MoveCameraPanoramaFunc = nullptr;
	IGCS_MoveCameraMultishot _igcs_MoveCameraMultishotFunc = nullptr;
	IGCS_EndScreenshotSession _igcs_EndScreenshotSessionFunc = nullptr;
	
	float _pano_totalFoVRadians = 0.0f;
	float _pano_currentFoVRadians = 0.0f;
	float _pano_anglePerStep = 0.0f;
	float _lightField_distancePerStep = 0.0f;
	float _overlapPercentagePerPanoShot = 30.0f;
	int _numberOfShotsToTake = 0;
	int _convolutionFrameCounter = 0;		// counts down to 0 from _amountOfFramesToWaitBetweenSteps
	int _shotCounter = 0;
	int _numberOfFramesToWaitBetweenSteps = 1;
	uint32_t _framebufferWidth = 0;
	uint32_t _framebufferHeight = 0;
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


