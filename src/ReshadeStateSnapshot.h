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

#include <reshade.hpp>
#include <string>
#include <unordered_map>
#include "EffectState.h"

/// <summary>
/// Defines a reshade state snapshot, which contains all enabled techniques and all uniform variables and their values. 
/// </summary>
class ReshadeStateSnapshot
{
public:
	void applyState(reshade::api::effect_runtime* runtime);

	/// <summary>
	/// Will migrate the state contained in this snapshot to the new id's used for variables. Doesn't mgirate variables to new values, only
	///	id's so we can set the state again using the current id's. Will also remove effects that are no longer enabled, and add new ones that are now enabled. 
	/// </summary>
	void migrateState(const ReshadeStateSnapshot& currentState);
	void obtainReshadeState(reshade::api::effect_runtime* runtime);
	void applyStateFromTo(const ReshadeStateSnapshot& snapShotDestination, float interpolationFactor, reshade::api::effect_runtime* runtime);
	ReshadeStateSnapshot getNewlyEnabledEffects(const ReshadeStateSnapshot& originalSnapshot) const;
	void addNewlyEnabledEffects(const ReshadeStateSnapshot& snapShotWithNewlyEnabledEffectsToCopy);

	bool isEmpty() const { return _effectStatePerEffectName.size() <= 0; }
	int numberOfContainedEffects() { return _effectStatePerEffectName.size(); }
	void logContents();

private:
	void addEffectState(EffectState toAdd);
	void addTechnique(reshade::api::effect_technique id, std::string name, bool isEnabled);

	std::unordered_map<std::string, EffectState> _effectStatePerEffectName;

	std::unordered_map<std::string, uint64_t> _techniqueIdPerName;
	std::unordered_map<std::string, bool> _techniqueEnabledPerName;
};

