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
#include "stdafx.h"
#include "Utils.h"
#include <comdef.h>
#include <codecvt>

#pragma warning(disable : 4996)

using namespace std;

namespace IGCS::Utils
{

	BYTE CharToByte(char c)
	{
		BYTE b;
		sscanf_s(&c, "%hhx", &b);
		return b;
	}


	string formatString(const char *fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		string formattedArgs = formatStringVa(fmt, args);
		va_end(args);
		return formattedArgs;
	}


	string formatStringVa(const char* fmt, va_list args)
	{
		va_list args_copy;
		va_copy(args_copy, args);

		int len = vsnprintf(NULL, 0, fmt, args_copy);
		char* buffer = new char[len + 2];
		vsnprintf(buffer, len + 1, fmt, args_copy);
		string toReturn(buffer, len + 1);
		return toReturn;
	}

	bool stringStartsWith(const char *a, const char *b)
	{
		return strncmp(a, b, strlen(b)) == 0 ? 1 : 0;
	}
}