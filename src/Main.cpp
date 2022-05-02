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

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#define ImTextureID unsigned long long // Change ImGui texture ID type to that of a 'reshade::api::resource_view' handle

#include "stdafx.h"
#include <imgui.h>
#include <iomanip>
#include <ios>
#include <Psapi.h>
#include <reshade.hpp>
#include <sstream>
#include <string>

#include "CameraToolsData.h"
#include "ScreenshotController.h"
#include "ScreenshotSettings.h"
#include "OverlayControl.h"

using namespace reshade::api;

// externs for reshade
extern "C" __declspec(dllexport) const char *NAME = "IGCS Connector";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Add-on which allows you to connect Reshade with IGCS built camera tools to exchange data and issue commands.";

// externs for IGCS
extern "C" __declspec(dllexport) bool connectFromCameraTools();
extern "C" __declspec(dllexport) LPBYTE getDataFromCameraToolsBuffer();



static LPBYTE g_dataFromCameraToolsBuffer = nullptr;		// 8192 bytes buffer
static ScreenshotSettings g_screenshotSettings;
static ScreenshotController g_screenshotController;

/// <summary>
/// Entry point for IGCS camera tools. Call this to initialize the buffers. Obtain the buffers using the getDataFrom/ToCameraToolsBuffer functions
/// </summary>
/// <returns>true if the allocation went OK, false otherwise. Returns true as well if this function was already called.</returns>
static bool connectFromCameraTools()
{
	if(nullptr != g_dataFromCameraToolsBuffer)
	{
		// already connected
		return true;
	}
	// malloc 8K buffers
	g_dataFromCameraToolsBuffer = (LPBYTE)calloc(8 * 1024, 1);

	// connect back to the camera tools
	g_screenshotController.connectToCameraTools();

	return g_dataFromCameraToolsBuffer!=nullptr;
}



/// <summary>
/// Gets the pointer to the buffer (8KB) for data to this addon from the camera tools.
/// </summary>
/// <returns>valid pointer or nullptr if connectFromCameraTools hasn't been called yet</returns>
static LPBYTE getDataFromCameraToolsBuffer()
{
	return g_dataFromCameraToolsBuffer;
}


static void onReshadePresent(effect_runtime* runtime)
{
	g_screenshotController.presentCalled();
}


static void onReshadeOverlay(effect_runtime* runtime)
{
	// first let the screenshot controller grab screenshots
	g_screenshotController.reshadeEffectsRendered(runtime);

	// then we'll render our own overlay if needed
	OverlayControl::renderOverlay();
}


static void showHelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(450.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}


static void startScreenshotSession(bool isTestRun)
{
	g_screenshotController.configure(g_screenshotSettings.screenshotFolder, g_screenshotSettings.numberOfFramesToWaitBetweenSteps);
	const auto cameraData = (CameraToolsData*)g_dataFromCameraToolsBuffer;
	switch(g_screenshotSettings.typeOfScreenshot)
	{
	case (int)ScreenshotType::HorizontalPanorama:
		g_screenshotController.startHorizontalPanoramaShot(g_screenshotSettings.pano_totalAngleDegrees, g_screenshotSettings.pano_overlapPercentagePerShot, cameraData->fov, isTestRun);
		break;
	case (int)ScreenshotType::CubemapProjectionPanorama:
		break;
	case (int)ScreenshotType::Lightfield:
		g_screenshotController.startLightfieldShot(g_screenshotSettings.lightField_distanceBetweenShots, g_screenshotSettings.lightField_numberOfShotsToTake, isTestRun);
		break;
	}
}


static void displaySettings(reshade::api::effect_runtime *runtime)
{
	if(ImGui::CollapsingHeader("General info and help"))
	{
		ImGui::PushTextWrapPos();
		ImGui::TextUnformatted("Help text");
		ImGui::PopTextWrapPos();
	}

	ImGui::AlignTextToFramePadding();
	if(ImGui::CollapsingHeader("Screenshot features", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if(g_screenshotController.cameraToolsConnected() && nullptr!=g_dataFromCameraToolsBuffer)
		{
			switch(g_screenshotController.getState())
			{
			case ScreenshotControllerState::Off:
				{	
					ImGui::InputText("Screenshot output directory", g_screenshotSettings.screenshotFolder, 256);
					ImGui::SliderInt("Number of frames to wait between steps", &g_screenshotSettings.numberOfFramesToWaitBetweenSteps, 1, 100);
					ImGui::Combo("Multi-screenshot type", &g_screenshotSettings.typeOfScreenshot, "360 degrees panorama\0Horizontal panorama\0Lightfield\0\0");
					switch(g_screenshotSettings.typeOfScreenshot)
					{
					case (int)ScreenshotType::CubemapProjectionPanorama:
						break;
					case (int)ScreenshotType::HorizontalPanorama:
						ImGui::SliderFloat("Total field of view in panorama (in degrees)", &g_screenshotSettings.pano_totalAngleDegrees, 30.0f, 360.0f, "%.1f");
						ImGui::SliderFloat("Percentage of overlap between shots", &g_screenshotSettings.pano_overlapPercentagePerShot, 0.1f, 99.0f, "%.1f");
						break;
					case (int)ScreenshotType::Lightfield:
						ImGui::SliderFloat("Distance between Lightfield shots", &g_screenshotSettings.lightField_distanceBetweenShots, 0.0f, 5.0f, "%.3f");
						ImGui::SliderInt("Number of shots to take", &g_screenshotSettings.lightField_numberOfShotsToTake, 0, 60);
						break;
						// others: ignore.
					}
					if(ImGui::Button("Start screenshot session"))
					{
						startScreenshotSession(false);
					}
					ImGui::SameLine();
					if(ImGui::Button("Start test run"))
					{
						startScreenshotSession(true);
					}
				}
				break;
			case ScreenshotControllerState::InSession:
				{
					if(ImGui::Button("Cancel session"))
					{
						g_screenshotController.cancelSession();
					}
				}
				break;
			case ScreenshotControllerState::Canceling:
				ImGui::Text("Cancelling session...");
				break;
			case ScreenshotControllerState::SavingShots:
				ImGui::Text("Saving shots...");
				break;
			}
		}
		else
		{
			ImGui::Text("Camera tools not available");
		}
	}

	ImGui::AlignTextToFramePadding();
	if(ImGui::CollapsingHeader("Camera tools info"))
	{
		if(nullptr == g_dataFromCameraToolsBuffer)
		{
			ImGui::Text("Camera data not available");
		}
		else
		{
			// display camera info
			const auto cameraData = (CameraToolsData*)g_dataFromCameraToolsBuffer;
			std::ostringstream stringStream;
			stringStream << std::fixed << std::setprecision(2) << cameraData->fov;
			const std::string fovAsString = stringStream.str();
			ImGui::InputText("FoV (degrees)", (char*)fovAsString.c_str(), fovAsString.length(), ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Camera coordinates", cameraData->coordinates, "%.1f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat4("Camera look quaternion", cameraData->lookQuaternion, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Rotation matrix row 1 (forward)", cameraData->getRotationMatrixRow1().values, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Rotation matrix row 2 (right)", cameraData->getRotationMatrixRow2().values, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Rotation matrix row 3 (up)", cameraData->getRotationMatrixRow3().values, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Inv. Rot matrix row 1 (forward)", cameraData->getInverseRotationMatrixRow1().values, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Inv. Rot matrix row 2 (right)", cameraData->getInverseRotationMatrixRow2().values, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Inv. Rot matrix row 3 (up)", cameraData->getInverseRotationMatrixRow3().values, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("World up vector", cameraData->up, "%.1f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("World right vector", cameraData->right, "%.1f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("World forward vector", cameraData->forward, "%.1f", ImGuiInputTextFlags_ReadOnly);
		}
	}
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
		{
			return FALSE;
		}
		reshade::register_event<reshade::addon_event::reshade_present>(onReshadePresent);
		reshade::register_event<reshade::addon_event::reshade_overlay>(onReshadeOverlay);
		reshade::register_overlay(nullptr, &displaySettings);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_event<reshade::addon_event::reshade_present>(onReshadePresent);
		reshade::unregister_overlay(nullptr, &displaySettings);
		reshade::unregister_addon(hModule);
		if(nullptr!=g_dataFromCameraToolsBuffer)
		{
			free(g_dataFromCameraToolsBuffer);
		}
		break;
	}

	return TRUE;
}
