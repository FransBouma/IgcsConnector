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

#include <DirectXMath.h>
#include <reshade.hpp>
#include <string>
#include <unordered_map>


class EffectState
{
public:
	EffectState() = default;
	EffectState(std::string name);

	/// <summary>
	/// Applies the state contained by this effect state to the runtime specified
	/// </summary>
	/// <param name="runtime"></param>
	void applyState(reshade::api::effect_runtime* runtime);
	/// <summary>
	/// Obtains the effect state from the runtime specified and stors it inside this effectstate object
	/// </summary>
	/// <param name="runtime"></param>
	void obtainEffectState(reshade::api::effect_runtime* runtime);
	/// <summary>
	/// Migrated the contained variables' id's to their new id values using the passed in idsource
	/// </summary>
	/// <param name="idSource"></param>
	void migrateIds(const EffectState& idSource);
	void applyStateFromTo(reshade::api::effect_runtime* runtime, EffectState destinationEffect, float interpolationFactor);
	void setUniformIntVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, int valueToWrite);
	void setUniformFloatVariable(reshade::api::effect_runtime* runtime, const std::string& uniformName, float valueToWrite);

	std::string name() { return _name; }

private:
	void addUniform(reshade::api::effect_uniform_variable id, std::string name, float* values);

	std::string _name;
	// Names are usable across reloads of a preset. It might still be names aren't present after a reload (e.g. a shader changed). But for our use case this isn't important. 
	std::unordered_map<std::string, uint64_t> _uniformFloatVariableIdPerName;			// for Floats only
	std::unordered_map<std::string, DirectX::XMFLOAT4> _uniformFloatValuePerName;		// stored with their uniform name for this effect. We store floats as float4.
	std::unordered_map<std::string, uint64_t> _uniformVariableIdPerName;				// all variables of the effect, floats and others.
};


