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

#include "ReshadeStateSnapshot.h"

#include <unordered_set>

#include "EffectState.h"


void ReshadeStateSnapshot::addEffectState(EffectState toAdd)
{
	_effectStatePerEffectName.emplace(toAdd.name(), toAdd);
}


void ReshadeStateSnapshot::applyState(reshade::api::effect_runtime* runtime)
{
	for(auto& it : _effectStatePerEffectName)
	{
		it.second.applyState(runtime);
	}
}


void ReshadeStateSnapshot::migrateState(const ReshadeStateSnapshot& currentState)
{
	// we have to collect new id's for our collected variables. Do this by using the name and check if they're still there. If so, migrate
	// the id they had to their new id.

	// now migrate to this new state
	for(auto& nameEffectPair : currentState._effectStatePerEffectName)
	{
		if(!_effectStatePerEffectName.contains(nameEffectPair.first))
		{
			// not yet known, add it
			addEffectState(nameEffectPair.second);
			continue;
		}
		// it's there, migrate id's
		EffectState& toMigrate = _effectStatePerEffectName[nameEffectPair.first];
		toMigrate.migrateIds(nameEffectPair.second);
	}

	std::vector<std::string> toRemove;	// old ones now gone
	for(auto& nameEffectPair : _effectStatePerEffectName)
	{
		if(!currentState._effectStatePerEffectName.contains(nameEffectPair.first))
		{
			// no longer there, remove it
			toRemove.push_back(nameEffectPair.second.name());
		}
	}
	for(auto& it : toRemove)
	{
		_effectStatePerEffectName.erase(it);
	}
}


void ReshadeStateSnapshot::obtainReshadeState(reshade::api::effect_runtime* runtime)
{
	// enumerate techniques. if a technique is enabled, grab the effect name and its uniforms and per uniform its value.
	std::unordered_set<std::string> effectNamesWithEnabledTechniques;
	runtime->enumerate_techniques(nullptr, [&effectNamesWithEnabledTechniques](reshade::api::effect_runtime* sourceRuntime, reshade::api::effect_technique technique)
	{
		if(sourceRuntime->get_technique_state(technique))
		{
			// grab the effect name
			char nameBuffer[1024];
			size_t nameBufferLength = 1024;
			sourceRuntime->get_technique_effect_name(technique, nameBuffer, &nameBufferLength);
			std::string effectName(nameBuffer);
			effectNamesWithEnabledTechniques.emplace(effectName);
		}
	});

	// per effect name, obtain all uniforms.
	for(const auto it : effectNamesWithEnabledTechniques)
	{
		const auto effectName = it;
		EffectState state(effectName);
		state.obtainEffectState(runtime);
		addEffectState(state);
	}

}
