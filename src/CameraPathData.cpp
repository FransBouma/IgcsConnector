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

#include "CameraPathData.h"

CameraPathData::CameraPathData() : CameraPathData(false)
{
}

CameraPathData::CameraPathData(bool isNonExisting) : _isNonExisting(isNonExisting)
{
}


CameraPathData& CameraPathData::getNonExisting()
{
	static CameraPathData nonExistingInstance(true);

	return nonExistingInstance;
}


void CameraPathData::appendState(const ReshadeStateSnapshot& toAppend)
{
	_snapshots.push_back(toAppend);
}


void CameraPathData::removeState(int stateIndex)
{
	if(stateIndex<0 || stateIndex>=_snapshots.size())
	{
		return;
	}
	_snapshots.erase(_snapshots.begin() + stateIndex);
}


void CameraPathData::updateState(const ReshadeStateSnapshot& snapshot, int stateIndex)
{
	if(stateIndex<0 || stateIndex>=_snapshots.size())
	{
		return;
	}
	_snapshots[stateIndex] = snapshot;
}


void CameraPathData::migratedContainedHandles(const ReshadeStateSnapshot& currentState)
{
	for(auto& snapshot : _snapshots)
	{
		snapshot.migrateState(currentState);
	}
}


void CameraPathData::setReshadeState(int fromStateIndex, int toStateIndex, float interpolationFactor, reshade::api::effect_runtime* runtime)
{
	if(fromStateIndex<0 || toStateIndex < 0 || fromStateIndex>=_snapshots.size() || toStateIndex >= _snapshots.size())
	{
		return;
	}
	ReshadeStateSnapshot& fromState = _snapshots[fromStateIndex];
	const ReshadeStateSnapshot& toState = _snapshots[toStateIndex];
	fromState.applyStateFromTo(toState, interpolationFactor, runtime);
}


void CameraPathData::setReshadeState(int stateIndex, reshade::api::effect_runtime* runtime)
{
	if(stateIndex < 0 || stateIndex >= _snapshots.size())
	{
		return;
	}
	ReshadeStateSnapshot& state = _snapshots[stateIndex];
	state.applyState(runtime);
}

