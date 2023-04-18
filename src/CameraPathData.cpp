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
#include "Utils.h"
#include "ReshadeStateSnapshot.h"

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


void CameraPathData::appendStateSnapshot(const ReshadeStateSnapshot& toAppend)
{
	_snapshots.push_back(toAppend);
}


void CameraPathData::insertStateSnapshotBeforeSnapshot(int indexToInsertBefore, const ReshadeStateSnapshot& reshadeStateSnapshot)
{
	if(indexToInsertBefore<0 || indexToInsertBefore >= _snapshots.size())
	{
		return;
	}
	const auto& nextSnapshot = _snapshots[indexToInsertBefore];
	const auto onlyNewlyEnabledEffectsSnapshot = reshadeStateSnapshot.getNewlyEnabledEffects(nextSnapshot);
	_snapshots.insert(_snapshots.begin() + indexToInsertBefore, reshadeStateSnapshot);
	// propagate the effects which are enabled in the new node to the following nodes.
	propagateNewlyEnabledEffects(indexToInsertBefore, onlyNewlyEnabledEffectsSnapshot);
}


void CameraPathData::appendStateSnapshotAfterSnapshot(int indexToAppendAfter, const ReshadeStateSnapshot& reshadeStateSnapshot)
{
	if(indexToAppendAfter<0 || indexToAppendAfter>=_snapshots.size())
	{
		return;
	}

	if(indexToAppendAfter==_snapshots.size()-1)
	{
		// last node, append
		_snapshots.push_back(reshadeStateSnapshot);
	}
	else
	{
		const auto& nextSnapshot = _snapshots[indexToAppendAfter + 1];
		const auto onlyNewlyEnabledEffectsSnapshot = reshadeStateSnapshot.getNewlyEnabledEffects(nextSnapshot);
		_snapshots.insert(_snapshots.begin() + indexToAppendAfter + 1, reshadeStateSnapshot);
		// propagate the effects which are enabled in the new node to the following nodes.
		// pass + 2 as we've inserted a new entry!
		propagateNewlyEnabledEffects(indexToAppendAfter + 2, onlyNewlyEnabledEffectsSnapshot);
	}
}


void CameraPathData::removeStateSnapshot(int stateIndex)
{
	if(stateIndex<0 || stateIndex>=_snapshots.size())
	{
		return;
	}
	_snapshots.erase(_snapshots.begin() + stateIndex);
}


void CameraPathData::updateStateSnapshot(const ReshadeStateSnapshot& snapshot, int stateIndex)
{
	if(stateIndex<0 || stateIndex>=_snapshots.size())
	{
		return;
	}

	// we first have to check which shaders are now enabled. These have to be copied to the nodes after this one so it's easy for the user to define effect states on an existing path.
	const auto& currentSnapshot = _snapshots[stateIndex];
	const auto onlyNewlyEnabledEffectsSnapshot = snapshot.getNewlyEnabledEffects(currentSnapshot);
	_snapshots[stateIndex] = snapshot;
	propagateNewlyEnabledEffects(stateIndex+1, onlyNewlyEnabledEffectsSnapshot);
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


void CameraPathData::propagateNewlyEnabledEffects(int startIndex, const ReshadeStateSnapshot& snapShotWithNewlyEnabledEffectsToCopy)
{
	if(snapShotWithNewlyEnabledEffectsToCopy.isEmpty())
	{
		return;
	}
	for(int i = startIndex;i<_snapshots.size();i++)
	{
		auto& snapshot = _snapshots[i];
		snapshot.addNewlyEnabledEffects(snapShotWithNewlyEnabledEffectsToCopy);
	}
}
