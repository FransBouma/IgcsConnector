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
#include "CameraToolsConnector.h"
#include "OverlayControl.h"


void CameraToolsConnector::connectToCameraTools()
{
	// Enumerate all modules in the process and check if they export a defined function (IGCS_StartScreenshotSession). If so, we map to the known functions. 
	const HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
	if(nullptr == processHandle)
	{
		return;
	}
	HMODULE modules[512];
	DWORD cbNeeded;
	if(EnumProcessModules(processHandle, modules, sizeof(modules), &cbNeeded))
	{
		for(int i = 0; i < cbNeeded / sizeof(HMODULE); i++)
		{
			// check if this module exports the IGCS_StartScreenshotSession function.
			const HMODULE moduleHandle = modules[i];
			_igcs_StartScreenshotSessionFunc = (IGCS_StartScreenshotSession)GetProcAddress(moduleHandle, "IGCS_StartScreenshotSession");
			if(nullptr == _igcs_StartScreenshotSessionFunc)
			{
				// doesn't export it, not an IGCS camera tools dll
				continue;
			}
			// map the other functions
			_igcs_EndScreenshotSessionFunc = (IGCS_EndScreenshotSession)GetProcAddress(moduleHandle, "IGCS_EndScreenshotSession");
			_igcs_MoveCameraPanoramaFunc = (IGCS_MoveCameraPanorama)GetProcAddress(moduleHandle, "IGCS_MoveCameraPanorama");
			_igcs_MoveCameraMultishotFunc = (IGCS_MoveCameraMultishot)GetProcAddress(moduleHandle, "IGCS_MoveCameraMultishot");
			break;
		}
	}
	CloseHandle(processHandle);
	OverlayControl::addNotification(cameraToolsConnected() ? "Camera tools connected" : "No camera tools found");
}


ScreenshotSessionStartReturnCode CameraToolsConnector::startScreenshotSession(uint8_t type)
{
	if(!cameraToolsConnected())
	{
		return ScreenshotSessionStartReturnCode::Error_CameraFeatureNotAvailable;
	}
	return _igcs_StartScreenshotSessionFunc(type);
}


void CameraToolsConnector::moveCameraPanorama(float stepAngle)
{
	if(!cameraToolsConnected())
	{
		return;
	}
	_igcs_MoveCameraPanoramaFunc(stepAngle);
}


void CameraToolsConnector::moveCameraMultishot(float stepLeftRight, float stepUpDown, float fovDegrees, bool fromStartPosition)
{
	if(!cameraToolsConnected())
	{
		return;
	}
	_igcs_MoveCameraMultishotFunc(stepLeftRight, stepUpDown, fovDegrees, fromStartPosition);
}


void CameraToolsConnector::endScreenshotSession()
{
	if(!cameraToolsConnected())
	{
		return;
	}
	_igcs_EndScreenshotSessionFunc();
}

