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
#include "Utils.h"

EffectState::EffectState(std::string name) : _name(name)
{}

void EffectState::addUniform(reshade::api::effect_uniform_variable id, std::string name, float* values)
{
	_uniformFloatValuePerName.emplace(name, DirectX::XMFLOAT4(values));
	_uniformFloatVariableIdPerName.emplace(name, id.handle);
}


void EffectState::applyState(reshade::api::effect_runtime* runtime)
{
	for(auto& nameValuePair : _uniformFloatValuePerName)
	{
		// grab the id so we can use it to set the variable.
		const auto id = _uniformFloatVariableIdPerName[nameValuePair.first];
		if(id==0)
		{
			continue;
		}
		runtime->set_uniform_value_float(reshade::api::effect_uniform_variable(id), nameValuePair.second.x, nameValuePair.second.y, nameValuePair.second.z, nameValuePair.second.w);
	}
}


void EffectState::obtainEffectState(reshade::api::effect_runtime* runtime)
{
	runtime->enumerate_uniform_variables(_name.c_str(), [&](reshade::api::effect_runtime* sourceRuntime, reshade::api::effect_uniform_variable variable)
	{
		// get name
		char nameBuffer[1024];
		size_t nameBufferLength = 1024;
		sourceRuntime->get_uniform_variable_name(variable, nameBuffer, &nameBufferLength);
		const std::string uniformName(nameBuffer);

		// get type
		reshade::api::format typeFormat;
		uint32_t numberOfRows;
		uint32_t numberOfColumns;
		uint32_t arrayLength;
		sourceRuntime->get_uniform_variable_type(variable, &typeFormat, &numberOfRows, &numberOfColumns, &arrayLength);

		// we only store r32_floats with # of rows 1-4. (float4 has 4 rows, cols is 1). We don't support other types as we can't interpolate between them anyway.
		if(typeFormat == reshade::api::format::r32_float && numberOfColumns == 1 && (numberOfRows >= 1 && numberOfRows < 5))
		{
			// get value
			float values[4] = {};
			sourceRuntime->get_uniform_value_float(variable, values, 4);
			addUniform(variable, uniformName, values);
		}
		// Always store the id as well in the general map, so we can use it to set a value different from a float if we need to using another context than the paths/interpolation
		_uniformVariableIdPerName.emplace(uniformName, variable.handle);
	});
}


void EffectState::migrateIds(const EffectState& idSource)
{
	_uniformFloatVariableIdPerName.clear();
	_uniformVariableIdPerName.clear();

	for(auto& nameValuePair : idSource._uniformFloatValuePerName)
	{
		auto sourceIdsPerName = idSource._uniformFloatVariableIdPerName;
		const auto id = sourceIdsPerName[nameValuePair.first];
		if(!_uniformFloatValuePerName.contains(nameValuePair.first))
		{
			// add it, it was not yet known
			_uniformFloatValuePerName[nameValuePair.first] = nameValuePair.second;
		}
		_uniformFloatVariableIdPerName[nameValuePair.first] = id;
	}
	// it might be the new state has less variables than we had in the previous state. Check if there are left over variables.
	std::vector<std::string> toRemove;
	for(auto& nameValuePair : _uniformFloatValuePerName)
	{
		if(!_uniformFloatVariableIdPerName.contains(nameValuePair.first))
		{
			// no longer there
			toRemove.push_back(nameValuePair.first);
		}
	}

	for(auto& nameValuePair : idSource._uniformVariableIdPerName)
	{
		// simply copy over
		_uniformVariableIdPerName[nameValuePair.first] = nameValuePair.second;
	}

	for(auto& it : toRemove)
	{
		_uniformFloatValuePerName.erase(it);
	}
}


void EffectState::applyStateFromTo(reshade::api::effect_runtime* runtime, EffectState destinationEffect, float interpolationFactor)
{
	auto& destinationValuesPerName = destinationEffect._uniformFloatValuePerName;

	for(auto& nameValuePair : _uniformFloatValuePerName)
	{
		if(!destinationValuesPerName.contains(nameValuePair.first))
		{
			continue;
		}
		const auto& destinationValues = destinationValuesPerName[nameValuePair.first];
		const auto& id = _uniformFloatVariableIdPerName[nameValuePair.first];

		const float x = IGCS::Utils::lerp(nameValuePair.second.x, destinationValues.x, interpolationFactor);
		const float y = IGCS::Utils::lerp(nameValuePair.second.y, destinationValues.y, interpolationFactor);
		const float z = IGCS::Utils::lerp(nameValuePair.second.z, destinationValues.z, interpolationFactor);
		const float w = IGCS::Utils::lerp(nameValuePair.second.w, destinationValues.w, interpolationFactor);
		runtime->set_uniform_value_float(reshade::api::effect_uniform_variable(id), x, y, z, w);
	}
}


void EffectState::setUniformIntVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, int valueToWrite)
{
	if(!_uniformVariableIdPerName.contains(uniformName))
	{
		return;
	}
	const uint64_t uniformId = _uniformVariableIdPerName[uniformName];
	runtime->set_uniform_value_int(reshade::api::effect_uniform_variable(uniformId), valueToWrite);
}


void EffectState::setUniformFloatVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, float valueToWrite)
{
	if(!_uniformVariableIdPerName.contains(uniformName))
	{
		return;
	}
	const uint64_t uniformId = _uniformVariableIdPerName[uniformName];
	runtime->set_uniform_value_float(reshade::api::effect_uniform_variable(uniformId), valueToWrite);
}
