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

#include <ranges>
#include <unordered_set>
#include <unordered_map>

#include "EffectState.h"
#include "Utils.h"


void ReshadeStateSnapshot::logContents()
{
	reshade::log_message(reshade::log_level::info, "\tTechniques: ");
	for(auto& kvp : _techniqueEnabledPerName)
	{
		reshade::log_message(reshade::log_level::info, IGCS::Utils::formatString("\t\t%s. Enabled: %s", kvp.first.c_str(), kvp.second ? "true" : "false").c_str());
	}

	reshade::log_message(reshade::log_level::info, "\tEffects: ");
	for(const auto& effectName : _effectStatePerEffectName | std::views::keys)
	{
		reshade::log_message(reshade::log_level::info, IGCS::Utils::formatString("\t\t%s", effectName.c_str()).c_str());
	}
}


void ReshadeStateSnapshot::addEffectState(EffectState toAdd)
{
	_effectStatePerEffectName.emplace(toAdd.name(), toAdd);
}


void ReshadeStateSnapshot::applyState(reshade::api::effect_runtime* runtime)
{
	// Apply uniform value state
	for(auto& it : _effectStatePerEffectName)
	{
		it.second.applyState(runtime);
	}

	// Apply technique state
	for(auto& nameIdPair : _techniqueIdPerName)
	{
		runtime->set_technique_state(reshade::api::effect_technique(nameIdPair.second), _techniqueEnabledPerName[nameIdPair.first]);
	}
}


void ReshadeStateSnapshot::migrateState(const ReshadeStateSnapshot& currentState)
{
	// we have to collect new id's for our collected variables. Do this by using the name and check if they're still there. If so, migrate
	// the id they had to their new id.
	// This call can be made in various scenarios, but they have either one of 2 characteristics: 1) there are 0 effects or 2) there are effects but they're changing.
	// We can safely ignore the first one, as that's the one originating from the call to destroy_effects. All the other scenarios are from update_effects which is
	// called in on_present and will end up raising the event in multiple scenarios.

	if(currentState.isEmpty())
	{
		// safely ignore this.
		return;
	}

	// now migrate to this new state. 
	for(auto& nameEffectPair : currentState._effectStatePerEffectName)
	{
		if(!_effectStatePerEffectName.contains(nameEffectPair.first))
		{
			// not known. We can ignore it as we don't store uniforms for unknown effects (it's the same as having an effect / technique being disabled).
			continue;
		}
		// it's there, migrate id's
		EffectState& toMigrate = _effectStatePerEffectName[nameEffectPair.first];
		toMigrate.migrateIds(nameEffectPair.second);
	}

	_techniqueIdPerName.clear();
	// migrate technique ids
	for(auto& nameIsEnabledPair : currentState._techniqueEnabledPerName)
	{
		// It's a bit nonsense to alter shader files while setting up a path and reloading the preset, but let's cover all the basis... 
		auto sourceIdsPerName = currentState._techniqueIdPerName;
		const auto id = sourceIdsPerName[nameIsEnabledPair.first];
		if(!_techniqueEnabledPerName.contains(nameIsEnabledPair.first))
		{
			// add it, it was not yet known
			_techniqueEnabledPerName[nameIsEnabledPair.first] = nameIsEnabledPair.second;
		}
		_techniqueIdPerName[nameIsEnabledPair.first] = id;
	}

	// it might be the new state has less techniques than we had in the previous state. Check if there are left over techniques.
	std::vector<std::string> toRemove;
	for(auto& nameValuePair : _techniqueEnabledPerName)
	{
		if(!_techniqueIdPerName.contains(nameValuePair.first))
		{
			// no longer there
			toRemove.push_back(nameValuePair.first);
		}
	}

	for(auto& it : toRemove)
	{
		_techniqueEnabledPerName.erase(it);
	}
}


void ReshadeStateSnapshot::addTechnique(reshade::api::effect_technique id, std::string name, bool isEnabled)
{
	_techniqueEnabledPerName[name] = isEnabled;
	_techniqueIdPerName.emplace(name, id.handle);
}


void ReshadeStateSnapshot::obtainReshadeState(reshade::api::effect_runtime* runtime)
{
	// enumerate techniques. if a technique is enabled, grab the effect name and its uniforms and per uniform its value. We don't store uniforms for effects which have no
	// enabled technique, as we won't lerp to these values anyway. 
	std::unordered_set<std::string> effectNamesWithEnabledTechniques;
	runtime->enumerate_techniques(nullptr, [this, &effectNamesWithEnabledTechniques](reshade::api::effect_runtime* sourceRuntime, reshade::api::effect_technique technique)
	{
		const bool isEnabled = sourceRuntime->get_technique_state(technique);
		char nameBuffer[1024] = { 0 };
		size_t nameBufferLength = 1024;

		if(isEnabled)
		{
			// grab the effect name
			sourceRuntime->get_technique_effect_name(technique, nameBuffer, &nameBufferLength);
			std::string effectName(nameBuffer);
			effectNamesWithEnabledTechniques.emplace(effectName);
		}

		memset(nameBuffer, 0, sizeof(char));
		nameBufferLength = 1024;
		sourceRuntime->get_technique_name(technique, nameBuffer, &nameBufferLength);
		const std::string techniqueName(nameBuffer);

		addTechnique(technique, techniqueName, isEnabled);
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


void ReshadeStateSnapshot::applyStateFromTo(const ReshadeStateSnapshot& snapShotDestination, float interpolationFactor, reshade::api::effect_runtime* runtime)
{
	// traverse our effects and interpolate their values to the values in snapShotDestination using the interpolation factor
	std::unordered_map<std::string, EffectState> destinationEffectsPerName = snapShotDestination._effectStatePerEffectName;
	for(auto& nameEffectPair : _effectStatePerEffectName)
	{
		if(!destinationEffectsPerName.contains(nameEffectPair.first))
		{
			continue;
		}
		const auto& destinationEffect = destinationEffectsPerName[nameEffectPair.first];
		nameEffectPair.second.applyStateFromTo(runtime, destinationEffect, interpolationFactor);
	}

	// if techniques are enabled in both this snapshot and the destination snapshot, we're going to enable the technique. Otherwise the technique is disabled.
	std::unordered_map<std::string, bool> destinationTechniqueEnabledPerName = snapShotDestination._techniqueEnabledPerName;
	for(auto& nameIsEnabledPair : _techniqueEnabledPerName)
	{
		bool newTechniqueState = false;

		if(destinationTechniqueEnabledPerName.contains(nameIsEnabledPair.first))
		{
			const auto& destinationIsEnabled = destinationTechniqueEnabledPerName[nameIsEnabledPair.first];
			newTechniqueState = nameIsEnabledPair.second && destinationIsEnabled;
		}

		const auto id = _techniqueIdPerName[nameIsEnabledPair.first];
		runtime->set_technique_state(reshade::api::effect_technique(id), newTechniqueState);
	}
}


ReshadeStateSnapshot ReshadeStateSnapshot::getNewlyEnabledEffects(const ReshadeStateSnapshot& originalSnapshot) const
{
	// only return the newly enabled effects. effects that have been disabled now aren't reported.
	ReshadeStateSnapshot toReturn;

	// techniques
	for (const auto& [techniqueName, isEnabled] : _techniqueEnabledPerName)
	{
		auto originalKvp = originalSnapshot._techniqueEnabledPerName.find(techniqueName);
		if(originalKvp == originalSnapshot._techniqueEnabledPerName.end() || (isEnabled && !originalKvp->second))
		{
			// this technique is now enabled or wasn't present in the original and is now present, so copy it over.
			toReturn._techniqueEnabledPerName[techniqueName] = isEnabled;
			toReturn._techniqueIdPerName[techniqueName] = _techniqueIdPerName.at(techniqueName);		// id entry is there as we add them together.
		}
	}

	// effects
	for(const auto& [effectName, effectState] : _effectStatePerEffectName)
	{
		if(!originalSnapshot._effectStatePerEffectName.contains(effectName))
		{
			// not found, so it's new in this snapshot, so we have to copy it over.
			toReturn._effectStatePerEffectName[effectName] = effectState;
		}
	}

	return toReturn;
}


void ReshadeStateSnapshot::addNewlyEnabledEffects(const ReshadeStateSnapshot& snapShotWithNewlyEnabledEffectsToCopy)
{
	// snapShotWithNewlyEnabledEffectsToCopy contains effects that should be copied to this snapshot. If we already have the effects
	// we'll skip it, otherwise we'll copy the effects. We'll also set the enabled flags on the techniques if they're set in the snapShotWithNewlyEnabledEffectsToCopy.

	// techniques
	for(const auto& [techniqueName, isEnabled] : snapShotWithNewlyEnabledEffectsToCopy._techniqueEnabledPerName)
	{
		auto currentKvp = _techniqueEnabledPerName.find(techniqueName);
		if(currentKvp == _techniqueEnabledPerName.end() || (isEnabled && !currentKvp->second))
		{
			// this technique is now enabled or wasn't present yet, so copy it over.
			_techniqueEnabledPerName[techniqueName] = isEnabled;
			_techniqueIdPerName[techniqueName] = _techniqueIdPerName.at(techniqueName);		// id entry is there as we add them together.
		}
	}

	// effects
	for(const auto& [effectName, effectState] : snapShotWithNewlyEnabledEffectsToCopy._effectStatePerEffectName)
	{
		if(!_effectStatePerEffectName.contains(effectName))
		{
			// not found, so it's new in this snapshot, so we have to copy it over.
			_effectStatePerEffectName[effectName] = effectState;
		}
	}
}
