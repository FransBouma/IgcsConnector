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
#include <cstdint>

struct Vec3
{
	float values[3];

	//----------------------------------
	// Methods below this line
	explicit Vec3(float xyz[3])
	{
		values[0] = xyz[0];
		values[1] = xyz[1];
		values[2] = xyz[2];
	}

	void setValues(DirectX::XMFLOAT3 v)
	{
		setValues(v.x, v.y, v.z);
	}

	void setValues(float x, float y, float z)
	{
		values[0] = x;
		values[1] = y;
		values[2] = z;
	}

	float x() { return values[0]; }
	float y() { return values[1]; }
	float z() { return values[2]; }
};


struct Vec4
{
	float values[4];

	//----------------------------------
	// Methods below this line
	explicit Vec4(float xyzw[4])
	{
		values[0] = xyzw[0];
		values[1] = xyzw[1];
		values[2] = xyzw[2];
		values[3] = xyzw[3];
	}

	void setValues(DirectX::XMFLOAT4 v)
	{
		setValues(v.x, v.y, v.z, v.w);
	}

	void setValues(float x, float y, float z, float w)
	{
		values[0] = x;
		values[1] = y;
		values[2] = z;
		values[3] = w;
	}

	float x() { return values[0]; }
	float y() { return values[1]; }
	float z() { return values[2]; }
	float w() { return values[3]; }
};


struct CameraToolsData
{
	uint8_t cameraEnabled;					// 1 is enabled 0 is not enabled
	uint8_t cameraMovementLocked;			// 1 is camera movement is locked, 0 is camera movement isn't locked.
	uint8_t reserved1;
	uint8_t reserved2;
	float fov;								// in degrees
	Vec3 coordinates;						// camera coordinates (x, y, z)
	Vec4 lookQuaternion;					// camera look quaternion qx, qy, qz, qw
	Vec3 rotationMatrixUpVector;			// up vector from the rotation matrix calculated from the look quaternion. 
	Vec3 rotationMatrixRightVector;			// right vector from the rotation matrix calculated from the look quaternion. 
	Vec3 rotationMatrixForwardVector;		// forward vector from the rotation matrix calculated from the look quaternion. 
	float pitch;							// in radians
	float yaw;
	float roll;
};
