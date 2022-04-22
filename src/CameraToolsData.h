///////////////////////////////////////////////////////////////////////
//
// Part of IGCS Connector, an add on for Reshade 5+ which allows you
// to connect IGCS built camera tools with reshade to exchange data and control
// from Reshade.
// 
// (c) Frans 'Otis_Inf' Bouma.
//
// All rights reserved.
// https://github.com/FransBouma/ShaderToggler
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
	Vec3(float x, float y, float z): values{x, y, z}
	{
	}

	float values[3];
	float x() { return values[0];}
	float y() { return values[1];}
	float z() { return values[2];}
};

struct CameraToolsData
{
	uint64_t reserved;
	float fov;					// in degrees
	float coordinates[3];		// camera coordinates (x, y, z)
	float lookQuaternion[4];	// camera look quaternion qx, qy, qz, qw
	float rotationMatrix[9];	// rotation matrix calculated from the look quaternion. row major order, the first 3 values are the first row of the matrix
	float up[3];				// up vector (e.g. 0, 1, 0 for y being up)
	float right[3];				// right vector (e.g. 1, 0, 0 for x being right)
	float forward[3];			// forward vector (e.g. 0, 0, -1 for negative z being forward)

	Vec3 getRotationMatrixRow1()
	{
		return Vec3(rotationMatrix[0], rotationMatrix[1], rotationMatrix[2]);
	}

	Vec3 getRotationMatrixRow2()
	{
		return Vec3(rotationMatrix[3], rotationMatrix[4], rotationMatrix[5]);
	}

	Vec3 getRotationMatrixRow3()
	{
		return Vec3(rotationMatrix[6], rotationMatrix[7], rotationMatrix[8]);
	}
};
