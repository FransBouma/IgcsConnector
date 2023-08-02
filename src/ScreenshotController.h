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

#include "CameraToolsConnector.h"
#include "ConstantsEnums.h"


// Simple controller class which controls the screenshot session.
class ScreenshotController
{
public:
	ScreenshotController(CameraToolsConnector& connector);
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
	void completeShotSession();
	void displayScreenshotSessionStartError(ScreenshotSessionStartReturnCode sessionStartResult);

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
	CameraToolsConnector& _cameraToolsConnector;

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


