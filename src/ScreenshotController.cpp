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
#include "stdafx.h"
#include "ScreenshotController.h"
#include <direct.h>
#include "OverlayControl.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "std_image_write.h"
#include "Utils.h"
#include <thread>

ScreenshotController::ScreenshotController()
{
}


void ScreenshotController::configure(std::string rootFolder, int numberOfFramesToWaitBetweenSteps)
{
	if (_state != ScreenshotControllerState::Off)
	{
		// Configure can't be called when a screenhot is in progress. ignore
		return;
	}
	reset();

	_rootFolder = rootFolder;
	_numberOfFramesToWaitBetweenSteps = numberOfFramesToWaitBetweenSteps;
}


bool ScreenshotController::shouldTakeShot()
{
	if (_convolutionFrameCounter > 0)
	{
		// always false as we're still waiting
		return false;
	}
	return _state == ScreenshotControllerState::InSession;
}


void ScreenshotController::presentCalled()
{
	if (_convolutionFrameCounter > 0)
	{
		_convolutionFrameCounter--;
	}
}


void ScreenshotController::reshadeEffectsRendered(reshade::api::effect_runtime* runtime)
{
	if(_state!=ScreenshotControllerState::InSession)
	{
		return;
	}
	if(shouldTakeShot())
	{
		// take a screenshot
		runtime->get_screenshot_width_and_height(&_framebufferWidth, &_framebufferHeight);
		std::vector<uint8_t> shotData(_framebufferWidth * _framebufferHeight * 4);
		runtime->capture_screenshot(shotData.data());
		storeGrabbedShot(shotData);
	}
}


void ScreenshotController::cancelSession()
{
	switch(_state)
	{
	case ScreenshotControllerState::Off: 
		return;
	case ScreenshotControllerState::InSession:
		if(nullptr!=_igcs_EndScreenshotSessionFunc)
		{
			_igcs_EndScreenshotSessionFunc();
		}
		_state = ScreenshotControllerState::Canceling;
		// kill the wait thread
		_waitCompletionHandle.notify_all();
		reset();
		break;
	case ScreenshotControllerState::SavingShots:
		_state = ScreenshotControllerState::Canceling;
		break;
	}
}


void ScreenshotController::connectToCameraTools()
{
	// Enumerate all modules in the process and check if they export a defined function (IGCS_StartScreenshotSession). If so, we map to the known functions. 
	const HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
	if(nullptr==processHandle)
	{
		return;
	}
	HMODULE modules[512];
	DWORD cbNeeded;
	if(EnumProcessModules(processHandle, modules, sizeof(modules), &cbNeeded))
	{
		for(int i=0;i< cbNeeded/sizeof(HMODULE); i++)
		{
			// check if this module exports the IGCS_StartScreenshotSession function.
			const HMODULE moduleHandle = modules[i];
			_igcs_StartScreenshotSessionFunc = (IGCS_StartScreenshotSession)GetProcAddress(moduleHandle, "IGCS_StartScreenshotSession");
			if(nullptr==_igcs_StartScreenshotSessionFunc)
			{
				// doesn't export it, not an IGCS camera tools dll
				continue;
			}
			// map the other functions
			_igcs_EndScreenshotSessionFunc = (IGCS_EndScreenshotSession)GetProcAddress(moduleHandle, "IGCS_EndScreenshotSession");
			_igcs_MoveCameraFunc = (IGCS_MoveCamera)GetProcAddress(moduleHandle, "IGCS_MoveCamera");
			break;
		}
	}
	OverlayControl::addNotification(cameraToolsConnected() ? "Camera tools connected" : "No camera tools found");
}


void ScreenshotController::completeShotSession()
{
	std::string shotTypeDescription = "";
	switch(_typeOfShot)
	{
		case ScreenshotType::CubemapProjectionPanorama:
			shotTypeDescription = "360 degree panorama";
			break;
		case ScreenshotType::HorizontalPanorama:
			shotTypeDescription = "Horizontal panorama";
			break;
		case ScreenshotType::Lightfield:
			shotTypeDescription = "Lightfield";
			break;
	}
	// we'll wait now till all the shots are taken. 
	waitForShots();
	if(_state != ScreenshotControllerState::Canceling)
	{
		OverlayControl::addNotification("All " + shotTypeDescription + " shots have been taken. Writing shots to disk...");
		saveGrabbedShots();
		OverlayControl::addNotification(shotTypeDescription + " done.");
	}
	// done
	_grabbedFrames.clear();
	_state = ScreenshotControllerState::Off;
}


void ScreenshotController::displayScreenshotSessionStartError(const ScreenshotSessionStartReturnCode sessionStartResult)
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
	OverlayControl::addNotification("Screenshot session couldn't be started: " + reason);
}


bool ScreenshotController::startSession()
{
	const auto sessionStartResult = _igcs_StartScreenshotSessionFunc((uint8_t)_typeOfShot);
	if(sessionStartResult != ScreenshotSessionStartReturnCode::AllOk)
	{
		displayScreenshotSessionStartError(sessionStartResult);
		return false;
	}
	return true;
}


void ScreenshotController::startHorizontalPanoramaShot(float totalFoVInDegrees, float overlapPercentagePerPanoShot, float currentFoVInDegrees, bool isTestRun)
{
	if(!cameraToolsConnected())
	{
		return;
	}

	reset();

	// the fov passed in is in degrees as well as the total fov, we change that to radians as the tools camera works with radians.
	float currentFoVInRadians = IGCS::Utils::degreesToRadians(currentFoVInDegrees);
	_pano_totalFoVRadians = IGCS::Utils::degreesToRadians(totalFoVInDegrees);
	_overlapPercentagePerPanoShot = overlapPercentagePerPanoShot;
	_pano_currentFoVRadians = currentFoVInRadians;
	_typeOfShot = ScreenshotType::HorizontalPanorama;
	_isTestRun = isTestRun;
	// panos are rotated from the far left to the far right of the total fov, where at the start, the center of the screen is rotated to the far left of the total fov, 
	// till the center of the screen hits the far right of the total fov. This is done because panorama stitching can often lead to corners not being used, so an overlap
	// on either side is preferable.

	// calculate the angle to step
	_pano_anglePerStep = currentFoVInRadians * ((100.0f-overlapPercentagePerPanoShot) / 100.0f);
	// calculate the # of shots to take
	_numberOfShotsToTake = ((_pano_totalFoVRadians / _pano_anglePerStep) + 1);

	// tell the camera tools we're starting a session.
	if(!startSession())
	{
		return;
	}
	
	// move to start
	moveCameraForPanorama(-1, true);

	// set convolution counter to its initial value
	_convolutionFrameCounter = _numberOfFramesToWaitBetweenSteps;
	_state = ScreenshotControllerState::InSession;

	// Create a thread which will handle the end of the shot session as the shot taking is done by event handlers
	std::thread t(&ScreenshotController::completeShotSession, this);
	t.detach();
}


void ScreenshotController::startLightfieldShot(float distancePerStep, int numberOfShots, bool isTestRun)
{
	if(!cameraToolsConnected())
	{
		return;
	}

	reset();
	_isTestRun = isTestRun;
	_lightField_distancePerStep = distancePerStep;
	_numberOfShotsToTake = numberOfShots;
	_typeOfShot = ScreenshotType::Lightfield;

	// tell the camera tools we're starting a session.
	if(!startSession())
	{
		return;
	}

	// move to start
	moveCameraForLightfield(-1, true);
	// set convolution counter to its initial value
	_convolutionFrameCounter = _numberOfFramesToWaitBetweenSteps;
	_state = ScreenshotControllerState::InSession;

	// Create a thread which will handle the end of the shot session as the shot taking is done by event handlers
	std::thread t(&ScreenshotController::completeShotSession, this);
	t.detach();
}


void ScreenshotController::startCubemapProjectionPanorama(bool isTestRun)
{
}


std::string ScreenshotController::createScreenshotFolder()
{
	time_t t = time(nullptr);
	tm tm;
	localtime_s(&tm, &t);
	const std::string optionalBackslash = (_rootFolder.ends_with('\\')) ? "" : "\\";
	std::string folderName = IGCS::Utils::formatString("%s%s%.4d-%.2d-%.2d-%.2d-%.2d-%.2d", _rootFolder.c_str(), optionalBackslash.c_str(), (tm.tm_year + 1900), (tm.tm_mon + 1), tm.tm_mday,
	                                                   tm.tm_hour, tm.tm_min, tm.tm_sec);
	_mkdir(folderName.c_str());
	return folderName;
}


void ScreenshotController::modifyCamera()
{
	// based on the type of the shot, we'll either rotate or move.
	switch (_typeOfShot)
	{
	case ScreenshotType::HorizontalPanorama:
		moveCameraForPanorama(1, false);
		break;
	case ScreenshotType::Lightfield:
		moveCameraForLightfield(1, false);
		break;
	case ScreenshotType::CubemapProjectionPanorama:
		moveCameraForCubemapProjection(false);
		break;
	}
}


void ScreenshotController::moveCameraForLightfield(int direction, bool end)
{
	if(nullptr==_igcs_MoveCameraFunc)
	{
		return;
	}

	float distance = direction * _lightField_distancePerStep;
	if (end)
	{
		distance *= 0.5f * _numberOfShotsToTake;
	}
	// we don't know the movement speed, so we pass the distance to the camera, and the camere has to divide by movement speed so it's independent of movement speed.
	_igcs_MoveCameraFunc(distance, 0);
}


void ScreenshotController::moveCameraForPanorama(int direction, bool end)
{
	if(nullptr == _igcs_MoveCameraFunc)
	{
		return;
	}

	float distance = direction * _pano_anglePerStep;
	if (end)
	{
		distance *= 0.5f * _numberOfShotsToTake;
	}
	// we don't know the movement speed, so we pass the distance to the camera, and the camere has to divide by movement speed so it's independent of movement speed.
	_igcs_MoveCameraFunc(distance, 0);
}


void ScreenshotController::moveCameraForCubemapProjection(bool start)
{
}


void ScreenshotController::storeGrabbedShot(std::vector<uint8_t> grabbedShot)
{
	if(grabbedShot.size() <= 0)
	{
		// failed
		return;
	}
	_grabbedFrames.push_back(grabbedShot);
	_shotCounter++;
	if(_shotCounter > _numberOfShotsToTake)
	{
		// we're done. Move to the next state, which is saving shots. 
		_state = ScreenshotControllerState::SavingShots;
		// tell the waiting thread to wake up so the system can proceed as normal.
		_waitCompletionHandle.notify_all();
	}
	else
	{
		modifyCamera();
		_convolutionFrameCounter = _numberOfFramesToWaitBetweenSteps;
	}
}


void ScreenshotController::saveGrabbedShots()
{
	if(_grabbedFrames.size() <= 0)
	{
		return;
	}
	if(!_isTestRun)
	{
		_state = ScreenshotControllerState::SavingShots;
		const std::string destinationFolder = createScreenshotFolder();
		int frameNumber = 0;
		for(std::vector<uint8_t> frame : _grabbedFrames)
		{
			saveShotToFile(destinationFolder, frame, frameNumber);
			frameNumber++;
		}
	}
}


void ScreenshotController::saveShotToFile(std::string destinationFolder, std::vector<uint8_t> data, int frameNumber)
{
	bool saveSuccessful = false;
	std::string filename = "";

	switch(_filetype)
	{
	case ScreenshotFiletype::Bmp:
		filename = IGCS::Utils::formatString("%s\\%d.bmp", destinationFolder.c_str(), frameNumber);
		saveSuccessful = stbi_write_bmp(filename.c_str(), _framebufferWidth, _framebufferHeight, 4, data.data()) != 0;
		break;
	case ScreenshotFiletype::Jpeg:
		filename = IGCS::Utils::formatString("%s\\%d.jpg", destinationFolder.c_str(), frameNumber);
		saveSuccessful = stbi_write_jpg(filename.c_str(), _framebufferWidth, _framebufferHeight, 4, data.data(), 98) != 0;
		break;
	case ScreenshotFiletype::Png:
		filename = IGCS::Utils::formatString("%s\\%d.png", destinationFolder.c_str(), frameNumber);
		saveSuccessful = stbi_write_png(filename.c_str(), _framebufferWidth, _framebufferHeight, 8, data.data(), 4 * _framebufferWidth) != 0;
		break;
	}
}


void ScreenshotController::waitForShots()
{
	std::unique_lock lock(_waitCompletionMutex);
	while(_state != ScreenshotControllerState::SavingShots)
	{
		_waitCompletionHandle.wait(lock);
	}
	// state has changed, we're notified so we're all goed to save the shots.
	// signal the tools the session ended.
	_igcs_EndScreenshotSessionFunc();
}


void ScreenshotController::reset()
{
	// don't reset framebuffer width/height, numberOfFramesToWaitBetweenSteps, movementSpeed, 
	// rotationSpeed, rootFolder as those are set through configure!
	_typeOfShot = ScreenshotType::HorizontalPanorama;
	_state = ScreenshotControllerState::Off;
	_pano_totalFoVRadians = 0.0f;
	_pano_currentFoVRadians = 0.0f;
	_lightField_distancePerStep = 0.0f;
	_pano_anglePerStep = 0.0f;
	_numberOfShotsToTake = 0;
	_convolutionFrameCounter = 0;
	_shotCounter = 0;
	_overlapPercentagePerPanoShot = 30.0f;
	_isTestRun = false;
}
