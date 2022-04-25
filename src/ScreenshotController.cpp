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
///
#include "stdafx.h"
#include "ScreenshotController.h"

ScreenshotController::ScreenshotController()
{
}

ScreenshotController::~ScreenshotController()
{}


void ScreenshotController::configure(int numberOfFramesToWaitBetweenSteps, float movementSpeed, float rotationSpeed)
{
	if (_state != ScreenshotControllerState::Off)
	{
		// Initialize can't be called when a screenhot is in progress. ignore
		return;
	}
	_numberOfFramesToWaitBetweenSteps = numberOfFramesToWaitBetweenSteps;
	_movementSpeed = movementSpeed;
	_rotationSpeed = rotationSpeed;
}


bool ScreenshotController::shouldTakeShot()
{
	if (_convolutionFrameCounter > 0)
	{
		// always false
		return false;
	}
	return _state == ScreenshotControllerState::InSession;
}


void ScreenshotController::presentCalled(reshade::api::effect_runtime* runtime)
{
	if (_convolutionFrameCounter > 0)
	{
		_convolutionFrameCounter--;
	}
	/*
	if(shouldTakeShot())
	{
		// take a screenshot
		uint32_t width = 0;
		uint32_t height = 0;
		runtime->get_screenshot_width_and_height(&width, &height);
		uint8_t* frameCaptured = (uint8_t*)malloc(width * height * 4);
		runtime->capture_screenshot(frameCaptured);
		// save screenshot.
	}
	*/

	// OnReshadePresent is called BEFORE the effects are rendered. So we can't use that to grab the screenshot. 

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
}


void ScreenshotController::setBufferSize(int width, int height)
{
	if (_state != ScreenshotControllerState::Off)
	{
		// ignore
		return;
	}
	_framebufferHeight = height;
	_framebufferWidth = width;
}


void ScreenshotController::startHorizontalPanoramaShot(float totalFoV, float overlapPercentagePerPanoShot, float currentFoV, bool isTestRun)
{
	reset();

	_totalFoV = totalFoV;
	_overlapPercentagePerPanoShot = overlapPercentagePerPanoShot;
	_currentFoV = currentFoV;
	_typeOfShot = ScreenshotType::HorizontalPanorama;
	_isTestRun = isTestRun;
	// panos are rotated from the far left to the far right of the total fov, where at the start, the center of the screen is rotated to the far left of the total fov, 
	// till the center of the screen hits the far right of the total fov. This is done because panorama stitching can often lead to corners not being used, so an overlap
	// on either side is preferable.

	// calculate the angle to step
	_anglePerStep = currentFoV * ((100.0f-overlapPercentagePerPanoShot) / 100.0f);
	// calculate the # of shots to take
	_amountOfShotsToTake = ((_totalFoV / _anglePerStep) + 1);

	// move to start
	moveCameraForPanorama(-1, true);

	// set convolution counter to its initial value
	_convolutionFrameCounter = _numberOfFramesToWaitBetweenSteps;
	_state = ScreenshotControllerState::InSession;
	/*
	// we'll wait now till all the shots are taken. 
	waitForShots();
	OverlayControl::addNotification("All Panorama shots have been taken. Writing shots to disk...");
	saveGrabbedShots();
	OverlayControl::addNotification("Panorama done.");
	*/
	// done

}


void ScreenshotController::startLightfieldShot(float distancePerStep, int amountOfShots, bool isTestRun)
{
	reset();
	_isTestRun = isTestRun;
	_distancePerStep = distancePerStep;
	_amountOfShotsToTake = amountOfShots;
	_typeOfShot = ScreenshotType::Lightfield;
	// move to start
	moveCameraForLightfield(-1, true);
	// set convolution counter to its initial value
	_convolutionFrameCounter = _numberOfFramesToWaitBetweenSteps;
	_state = ScreenshotControllerState::InSession;

	/*
	// we'll wait now till all the shots are taken. 
	waitForShots();
	OverlayControl::addNotification("All Lightfield have been shots taken. Writing shots to disk...");
	saveGrabbedShots();
	OverlayControl::addNotification("Lightfield done.");
	*/
	// done
}


void ScreenshotController::startCubemapProjectionPanorama(bool isTestRun)
{
}


/*
string ScreenshotController::createScreenshotFolder()
{
	time_t t = time(nullptr);
	tm tm;
	localtime_s(&tm, &t);
	string optionalBackslash = (_rootFolder.ends_with('\\')) ? "" : "\\";
	string folderName = Utils::formatString("%s%s%.4d-%.2d-%.2d-%.2d-%.2d-%.2d", _rootFolder.c_str(), optionalBackslash.c_str(), (tm.tm_year + 1900), (tm.tm_mon + 1), tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	mkdir(folderName.c_str());
	return folderName;
}
*/


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
		// nothing
		break;
	}
}


void ScreenshotController::moveCameraForLightfield(int direction, bool end)
{
	//_camera.resetMovement();
	float dist = direction * _distancePerStep;
	if (end)
	{
		dist *= 0.5f * _amountOfShotsToTake;
	}
	float stepSize = dist / _movementSpeed; // scale to be independent of camera movement speed
	//_camera.moveRight(stepSize);
}


void ScreenshotController::moveCameraForPanorama(int direction, bool end)
{
	//_camera.resetMovement();
	float dist = direction * _anglePerStep;
	if (end)
	{
		dist *= 0.5f * _amountOfShotsToTake;
	}
	float stepSize = dist / _rotationSpeed; // scale to be independent of camera rotation speed
	//_camera.yaw(stepSize);
}


void ScreenshotController::moveCameraForCubemapProjection(bool start)
{
}


void ScreenshotController::reset()
{
	// don't reset framebuffer width/height, numberOfFramesToWaitBetweenSteps, movementSpeed, 
	// rotationSpeed, rootFolder as those are set through configure!
	_typeOfShot = ScreenshotType::Lightfield;
	_state = ScreenshotControllerState::Off;
	_totalFoV = 0.0f;
	_currentFoV = 0.0f;
	_distancePerStep = 0.0f;
	_anglePerStep = 0.0f;
	_amountOfShotsToTake = 0;
	_amountOfColumns = 0;
	_amountOfRows = 0;
	_convolutionFrameCounter = 0;
	_shotCounter = 0;
	_overlapPercentagePerPanoShot = 30.0f;
	_isTestRun = false;
}
