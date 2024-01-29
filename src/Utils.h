////////////////////////////////////////////////////////////////////////////////////////////////////////
// Part of Injectable Generic Camera System
// Copyright(c) 2019, Frans Bouma
// All rights reserved.
// https://github.com/FransBouma/InjectableGenericCameraSystem
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
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <reshade.hpp>
#include "stdafx.h"
#include <vector>

namespace IGCS::Utils
{
	float degreesToRadians(float angleInDegrees);
	std::string formatString(const char* fmt, ...);
	std::string formatStringVa(const char* fmt, va_list args);
	void logLineToReshade(const reshade::log_level logLevel, const char* fmt, ...);

	BYTE CharToByte(char c);
	bool stringStartsWith(const char *a, const char *b);
	std::vector<float> XMFloat4x4ToFlatVector(const DirectX::XMMATRIX& toConvert);

	/// <summary>
	/// Special clamp variant, which returns the min value if value is smaller than min value, and max if the value is larger than max. Max is optional, if it's e.g. 0 or equal to Min, only the min
	///	value is effective. 
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="value"></param>
	/// <param name="min"></param>
	/// <param name="max"></param>
	/// <returns></returns>
	template <typename T>
	T clampEx(T value, T min, T max)
	{
		return
			(value < min)
			? min
			: (value < max)
			? value
			: max;
	}

	/// <summary>
	/// Implements classic lerp functionality to smoothly transition between two values based on a factor between 0 and 1
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="x"></param>
	/// <param name="y"></param>
	/// <param name="s"></param>
	/// <returns></returns>
	template <typename T>
	T lerp(T x, T y, T s)
	{
		return (x + (s * (y - x)));
	}
}