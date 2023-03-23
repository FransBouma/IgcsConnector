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
#include <reshade.hpp>
#include "CameraPathData.h"
#include "ReshadeStateSnapshot.h"


/// <summary>
/// Class which maintains reshade state snapshots for various paths.
/// </summary>
class ReshadeStateController
{
public:
	void removeCameraPath(int pathIndex);
	void addCameraPath();
	void appendStateSnapshotToPath(int pathIndex, reshade::api::effect_runtime* runtime);
	void insertStateSnapshotBeforeSnapshotOnPath(int pathIndex, int indexToInsertBefore, reshade::api::effect_runtime* runtime);
	void appendStateSnapshotAfterSnapshotOnPath(int pathIndex, int indexToAppendAfter, reshade::api::effect_runtime* runtime);
	void removeStateSnapshotFromPath(int pathIndex, int stateIndex);
	void updateStateSnapshotOnPath(int pathIndex, int stateIndex, reshade::api::effect_runtime* runtime);
	void migrateContainedHandles(reshade::api::effect_runtime* runtime);
	void setReshadeState(int pathIndex, int fromStateIndex, int toStateIndex, float interpolationFactor, reshade::api::effect_runtime* runtime);
	void setReshadeState(int pathIndex, int stateIndex, reshade::api::effect_runtime* runtime);
	void clearPaths();
	int numberOfSnapshotsOnPath(int pathIndex);

	int numberOfPaths() { return _cameraPathsData.size(); }

private:
	std::vector<CameraPathData> _cameraPathsData;
	mutable std::mutex mutex_;

	CameraPathData& getCameraPath(int pathIndex);
	ReshadeStateSnapshot getCurrentReshadeStateSnapshot(reshade::api::effect_runtime* runtime);
};

