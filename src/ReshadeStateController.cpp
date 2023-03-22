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

#include "ReshadeStateController.h"
#include "CameraPathData.h"


void ReshadeStateController::removeCameraPath(int pathIndex)
{
	std::scoped_lock lock(mutex_);
	if(pathIndex<0 || pathIndex>=_cameraPathsData.size())
	{
		return;
	}
	_cameraPathsData.erase(_cameraPathsData.begin() + pathIndex);
}


void ReshadeStateController::addCameraPath()
{
	std::scoped_lock lock(mutex_);
	_cameraPathsData.push_back(CameraPathData());
}


void ReshadeStateController::appendStateToPath(int pathIndex, reshade::api::effect_runtime* runtime)
{
	std::scoped_lock lock(mutex_);
	CameraPathData& path = getCameraPath(pathIndex);
	if(path.isNonExisting())
	{
		return;
	}
	path.appendState(getCurrentReshadeState(runtime));
}


void ReshadeStateController::removeStateFromPath(int pathIndex, int stateIndex)
{
	std::scoped_lock lock(mutex_);
	CameraPathData& path = getCameraPath(pathIndex);
	if(path.isNonExisting())
	{
		return;
	}
	path.removeState(stateIndex);
}


void ReshadeStateController::updateStateOnPath(int pathIndex, int stateIndex, reshade::api::effect_runtime* runtime)
{
	std::scoped_lock lock(mutex_);
	CameraPathData& path = getCameraPath(pathIndex);
	if(path.isNonExisting())
	{
		return;
	}
	path.updateState(getCurrentReshadeState(runtime), stateIndex);
}


void ReshadeStateController::migrateContainedHandles(reshade::api::effect_runtime* runtime)
{
	std::scoped_lock lock(mutex_);
	const ReshadeStateSnapshot currentState = getCurrentReshadeState(runtime);

	for(auto& path : _cameraPathsData)
	{
		path.migratedContainedHandles(currentState);
	}
}


void ReshadeStateController::setReshadeState(int pathIndex, int fromStateIndex, int toStateIndex, float interpolationFactor, reshade::api::effect_runtime* runtime)
{
	std::scoped_lock lock(mutex_);
	CameraPathData& path = getCameraPath(pathIndex);
	if(path.isNonExisting())
	{
		return;
	}
	path.setReshadeState(fromStateIndex, toStateIndex, interpolationFactor, runtime);
}


void ReshadeStateController::setReshadeState(int pathIndex, int stateIndex, reshade::api::effect_runtime* runtime)
{
	std::scoped_lock lock(mutex_);
	CameraPathData& path = getCameraPath(pathIndex);

	if(path.isNonExisting())
	{
		return;
	}
	path.setReshadeState(stateIndex, runtime);
}


CameraPathData& ReshadeStateController::getCameraPath(int index)
{
	if(index<0 || index >= _cameraPathsData.size())
	{
		return CameraPathData::getNonExisting();
	}

	return _cameraPathsData[index];
}


ReshadeStateSnapshot ReshadeStateController::getCurrentReshadeState(reshade::api::effect_runtime* runtime)
{
	ReshadeStateSnapshot currentState;

	currentState.obtainReshadeState(runtime);
	return currentState;
}
