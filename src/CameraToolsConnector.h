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

#pragma once
#include <cstdint>
#include "ConstantsEnums.h"

/// <summary>
/// Starts a screenshot session of the specified type
/// </summary>
///	<param name="type">specifies the screenshot session type:</param>
///	0: Normal panorama
/// 1: Multishot (moving the camera over a line or grid or other trajectory)
/// <returns>0 if the session was successfully started or a value > 0 if not:
/// 1 if the camera wasn't enabled
/// 2 if a camera path is playing
/// 3 if there was already a session active
/// 4 if camera feature isn't available
/// 5 if another error occurred</returns>
typedef ScreenshotSessionStartReturnCode(*IGCS_StartScreenshotSession)(uint8_t type);
/// <summary>
/// Rotates the camera in the current session over the specified stepAngle.
/// </summary>
/// <param name="stepAngle">
/// stepAngle is the angle (in radians) to rotate over to the right. Negative means the camera will rotate to the left.
/// </param>
///	<remarks>stepAngle isn't divided by movementspeed/rotation speed yet, so it has to be done locally in the camerafeaturebase.</remarks>
typedef void(*IGCS_MoveCameraPanorama)(float stepAngle);
/// <summary>
/// Moves the camera up/down/left/right based on the values specified and whether that's relative to the start location or to the current camera location.
/// </summary>
/// <param name="stepLeftRight">The amount to step left/right. Negative values will make the camera move to the left, positive values make the camera move to the right</param>
/// <param name="stepUpDown">The amount to step up/down. Negative values with make the camera move down, positive values make the camera move up</param>
/// <param name="fovDegrees">The fov in degrees to use for the step. If &lt= 0, the value is ignored</param>
/// <param name="fromStartPosition">If true the values specified will be relative to the start location of the session, otherwise to the current location of the camera</param>
typedef void(*IGCS_MoveCameraMultishot)(float stepLeftRight, float stepUpDown, float fovDegrees, bool fromStartPosition);
/// <summary>
/// Ends the active screenshot session, restoring camera data if required.
/// </summary>
typedef void(*IGCS_EndScreenshotSession)();


/// <summary>
/// Class which controls a connection with the camera tools. It's created and started by main and passed onto controllers which need to connect to the camera tools. 
/// </summary>
class CameraToolsConnector
{
public:
	/// <summary>
	/// Connects to the camera tools, after the camera tools have called connectFromCameraTools(). it's done this way
	///	so other addins can also communicate with the camera tools without a required call from the camera tools. 
	/// </summary>
	void connectToCameraTools();
	/// <summary>
	/// Starts a screenshot session of the specified type
	/// </summary>
	///	<param name="type">specifies the screenshot session type:</param>
	///	0: Normal panorama
	/// 1: Multishot (moving the camera over a line or grid or other trajectory)
	/// <returns>0 if the session was successfully started or a value > 0 if not:
	/// 1 if the camera wasn't enabled
	/// 2 if a camera path is playing
	/// 3 if there was already a session active
	/// 4 if camera feature isn't available
	/// 5 if another error occurred</returns>
	ScreenshotSessionStartReturnCode startScreenshotSession(uint8_t type);
	/// <summary>
	/// Rotates the camera in the current session over the specified stepAngle.
	/// </summary>
	/// <param name="stepAngle">
	/// stepAngle is the angle (in radians) to rotate over to the right. Negative means the camera will rotate to the left.
	/// </param>
	///	<remarks>stepAngle isn't divided by movementspeed/rotation speed yet, so it has to be done locally in the camerafeaturebase.</remarks>
	void moveCameraPanorama(float stepAngle);
	/// <summary>
	/// Moves the camera up/down/left/right based on the values specified and whether that's relative to the start location or to the current camera location.
	/// </summary>
	/// <param name="stepLeftRight">The amount to step left/right. Negative values will make the camera move to the left, positive values make the camera move to the right</param>
	/// <param name="stepUpDown">The amount to step up/down. Negative values with make the camera move down, positive values make the camera move up</param>
	/// <param name="fovDegrees">The fov in degrees to use for the step. If &lt= 0, the value is ignored</param>
	/// <param name="fromStartPosition">If true the values specified will be relative to the start location of the session, otherwise to the current location of the camera</param>
	void moveCameraMultishot(float stepLeftRight, float stepUpDown, float fovDegrees, bool fromStartPosition);
	/// <summary>
	/// Ends the active screenshot session, restoring camera data if required.
	/// </summary>
	void endScreenshotSession();
	/// <summary>
	/// Returns true if this object is connected to camera tools, false otherwise
	/// </summary>
	/// <returns></returns>
	bool cameraToolsConnected()
	{
		return (nullptr != _igcs_MoveCameraPanoramaFunc && nullptr != _igcs_EndScreenshotSessionFunc && nullptr != _igcs_StartScreenshotSessionFunc && nullptr != _igcs_MoveCameraMultishotFunc);
	}

private:
	IGCS_StartScreenshotSession _igcs_StartScreenshotSessionFunc = nullptr;
	IGCS_MoveCameraPanorama _igcs_MoveCameraPanoramaFunc = nullptr;
	IGCS_MoveCameraMultishot _igcs_MoveCameraMultishotFunc = nullptr;
	IGCS_EndScreenshotSession _igcs_EndScreenshotSessionFunc = nullptr;
};

