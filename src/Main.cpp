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

// Change ImGui texture ID type to that of a 'reshade::api::resource_view' handle
#ifdef IGCS32BIT
#define ImTextureID ImU32
#else
#define ImTextureID unsigned long long 
#endif

#include "stdafx.h"
#include <imgui.h>
#include <reshade.hpp>
#include <iomanip>
#include <ios>
#include <Psapi.h>
#include <sstream>
#include <string>

#include "CameraToolsData.h"
#include "CDataFile.h"
#include "DepthOfFieldController.h"
#include "ScreenshotController.h"
#include "ScreenshotSettings.h"
#include "OverlayControl.h"
#include "ReshadeStateController.h"
#include "ThreadSafeQueue.h"
#include "WorkItem.h"

using namespace reshade::api;

// externs for reshade
extern "C" __declspec(dllexport) const char *NAME = "IGCS Connector";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Add-on which allows you to connect Reshade with IGCS built camera tools to exchange data and issue commands.";

// externs for IGCS
extern "C" __declspec(dllexport) bool connectFromCameraTools();
extern "C" __declspec(dllexport) LPBYTE getDataFromCameraToolsBuffer();
extern "C" __declspec(dllexport) void addCameraPath();
extern "C" __declspec(dllexport) void appendStateSnapshotAfterSnapshotOnPath(int pathIndex, int indexToAppendAfter);
extern "C" __declspec(dllexport) void appendStateSnapshotToPath(int pathIndex);
extern "C" __declspec(dllexport) void clearPaths();
extern "C" __declspec(dllexport) void insertStateSnapshotBeforeSnapshotOnPath(int pathIndex, int indexToInsertBefore);
extern "C" __declspec(dllexport) void removeCameraPath(int pathIndex);
extern "C" __declspec(dllexport) void removeStateSnapshotFromPath(int pathIndex, int stateIndex);
extern "C" __declspec(dllexport) void setReshadeStateInterpolated(int pathIndex, int fromStateIndex, int toStateIndex, float interpolationFactor);
extern "C" __declspec(dllexport) void setReshadeState(int pathIndex, int stateIndex);
extern "C" __declspec(dllexport) void updateStateSnapshotOnPath(int pathIndex, int stateIndex);

#define SETTINGS_FILE_NAME "IgcsConnector.ini"

static LPBYTE g_dataFromCameraToolsBuffer = nullptr;		// 8192 bytes buffer
static CameraToolsConnector g_cameraToolsConnector;
static ScreenshotSettings g_screenshotSettings;
static ScreenshotController g_screenshotController(g_cameraToolsConnector);
static DepthOfFieldController g_depthOfFieldController(g_cameraToolsConnector);
static ReshadeStateController g_reshadeStateController;
static IGCS::ThreadSafeQueue<WorkItem> g_presentWorkQueue;
static bool g_recordReshadeState = true;

/// <summary>
/// Entry point for IGCS camera tools. Call this to initialize the buffers. Obtain the buffers using the getDataFrom/ToCameraToolsBuffer functions
/// </summary>
/// <returns>true if the allocation went OK, false otherwise. Returns true as well if this function was already called.</returns>
bool connectFromCameraTools()
{
	if(nullptr != g_dataFromCameraToolsBuffer)
	{
		// already connected
		return true;
	}
	// malloc 8K buffers
	g_dataFromCameraToolsBuffer = (LPBYTE)calloc(8 * 1024, 1);

	// connect back to the camera tools
	g_cameraToolsConnector.connectToCameraTools();

	return g_dataFromCameraToolsBuffer!=nullptr;
}


/// <summary>
/// Gets the pointer to the buffer (8KB) for data to this addon from the camera tools.
/// </summary>
/// <returns>valid pointer or nullptr if connectFromCameraTools hasn't been called yet</returns>
LPBYTE getDataFromCameraToolsBuffer()
{
	return g_dataFromCameraToolsBuffer;
}


/// <summary>
/// Clears all contained camera paths
/// </summary>
void clearPaths()
{
	g_reshadeStateController.clearPaths();
}


/// <summary>
/// Adds a camera path to the reshade state controller
/// </summary>
void addCameraPath()
{
	if(!g_recordReshadeState)
	{
		return;
	}
	g_reshadeStateController.addCameraPath();
}


/// <summary>
/// Removes the path with the index specified
/// </summary>
/// <param name="pathIndex"></param>
void removeCameraPath(int pathIndex)
{
	g_reshadeStateController.removeCameraPath(pathIndex);
}


/// <summary>
/// Appends the current state to the path with the index specified
/// </summary>
/// <param name="pathIndex"></param>
void appendStateSnapshotToPath(int pathIndex)
{
	if(!g_recordReshadeState)
	{
		return;
	}

	// done deferred.
	g_presentWorkQueue.push({ [pathIndex](effect_runtime* lambdaRuntime) {g_reshadeStateController.appendStateSnapshotToPath(pathIndex, lambdaRuntime); } });
}


/// <summary>
/// Inserts the current state to the path with the index specified before the snapshot with the index specified. 
/// </summary>
/// <param name="pathIndex"></param>
/// <param name="indexToInsertBefore"></param>
void insertStateSnapshotBeforeSnapshotOnPath(int pathIndex, int indexToInsertBefore)
{
	if(!g_recordReshadeState)
	{
		return;
	}
	// done deferred
	g_presentWorkQueue.push({ [pathIndex, indexToInsertBefore](effect_runtime* lambdaRuntime) {g_reshadeStateController.insertStateSnapshotBeforeSnapshotOnPath(pathIndex, indexToInsertBefore, lambdaRuntime); } });
}


/// <summary>
/// Appends the current state to the path with the index specified after the snapshot with the index specified
/// </summary>
/// <param name="pathIndex"></param>
/// <param name="indexToAppendAfter"></param>
void appendStateSnapshotAfterSnapshotOnPath(int pathIndex, int indexToAppendAfter)
{
	if(!g_recordReshadeState)
	{
		return;
	}
	// done deferred
	g_presentWorkQueue.push({ [pathIndex, indexToAppendAfter](effect_runtime* lambdaRuntime) {g_reshadeStateController.appendStateSnapshotAfterSnapshotOnPath(pathIndex, indexToAppendAfter, lambdaRuntime); } });
}


/// <summary>
/// Update the state at offset stateIndex on path with index pathIndex to the current state
/// </summary>
/// <param name="pathIndex"></param>
/// <param name="stateIndex"></param>
void updateStateSnapshotOnPath(int pathIndex, int stateIndex)
{
	if(!g_recordReshadeState)
	{
		return;
	}
	// done deferred.
	g_presentWorkQueue.push({ [pathIndex, stateIndex](effect_runtime* lambdaRuntime) {g_reshadeStateController.updateStateSnapshotOnPath(pathIndex, stateIndex, lambdaRuntime); } });
}


/// <summary>
/// Removes the state at index stateIndex from the path at index pathIndex
/// </summary>
/// <param name="pathIndex"></param>
/// <param name="stateIndex"></param>
void removeStateSnapshotFromPath(int pathIndex, int stateIndex)
{
	g_reshadeStateController.removeStateSnapshotFromPath(pathIndex, stateIndex);
}


/// <summary>
/// Sets reshade's effect state to the interpolation of the two states at offset fromStateIndex and toStateIndex on path with index pathIndex, using the interpolation factor specified. 
/// </summary>
/// <param name="pathIndex"></param>
/// <param name="fromStateIndex"></param>
/// <param name="toStateIndex"></param>
/// <param name="interpolationFactor">if 0.0 the state will be fromState, if 1.0 the state will be toState, any value between 0 and 1 will be an interpolation using lerp of fromState and toState</param>
void setReshadeStateInterpolated(int pathIndex, int fromStateIndex, int toStateIndex, float interpolationFactor)
{
	if(!g_recordReshadeState)
	{
		return;
	}

	// done deferred
	g_presentWorkQueue.push({ [pathIndex, fromStateIndex, toStateIndex, interpolationFactor](effect_runtime* lambdaRuntime)
	{
		g_reshadeStateController.setReshadeState(pathIndex, fromStateIndex, toStateIndex, interpolationFactor, lambdaRuntime);
	} });
}


/// <summary>
/// Sets reshade's effect state to the state on index stateIndex on path with index pathIndex
/// </summary>
/// <param name="pathIndex"></param>
/// <param name="stateIndex"></param>
void setReshadeState(int pathIndex, int stateIndex)
{
	if(!g_recordReshadeState)
	{
		return;
	}

	// done deferred
	g_presentWorkQueue.push({ [pathIndex, stateIndex](effect_runtime* lambdaRuntime) {g_reshadeStateController.setReshadeState(pathIndex, stateIndex, lambdaRuntime); } });
}



void handleWorkQueue(effect_runtime* runtime)
{
	for(;;)
	{
		auto topElement = g_presentWorkQueue.pop();
		if(!topElement.has_value())
		{
			break;
		}
		auto workItem = topElement.value();
		workItem.perform(runtime);
	}
}


static void onReshadePresent(effect_runtime* runtime)
{
	g_screenshotController.presentCalled();

	// handle our work.
	handleWorkQueue(runtime);
}


static void onReshadeOverlay(effect_runtime* runtime)
{
	// first let the screenshot controller grab screenshots
	g_screenshotController.reshadeEffectsRendered(runtime);

	// then we'll render our own overlays if needed
	OverlayControl::renderOverlay();
	g_depthOfFieldController.renderOverlay();		// if it has something to display it can do that here
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
	g_screenshotController.configure(g_screenshotSettings.screenshotFolder, g_screenshotSettings.numberOfFramesToWaitBetweenSteps, (ScreenshotFiletype)g_screenshotSettings.screenshotFileType);
	const auto cameraData = (CameraToolsData*)g_dataFromCameraToolsBuffer;
	switch(g_screenshotSettings.typeOfScreenshot)
	{
	case (int)ScreenshotType::HorizontalPanorama:
		g_screenshotController.startHorizontalPanoramaShot(g_screenshotSettings.pano_totalAngleDegrees, g_screenshotSettings.pano_overlapPercentagePerShot, cameraData->fov, isTestRun);
		break;
	case (int)ScreenshotType::MultiShot:
		g_screenshotController.startLightfieldShot(g_screenshotSettings.lightField_distanceBetweenShots, g_screenshotSettings.lightField_numberOfShotsToTake, isTestRun);
		break;
#ifdef _DEBUG
	case (int)ScreenshotType::DebugGrid:
		g_screenshotController.startDebugGridShot();
		break;
#endif
	}
}


void loadGeneralSettingsFromIniFile(CDataFile& iniFile)
{
	if(iniFile.GetValue("RecordReshadeState", "General").length() > 0)
	{
		g_recordReshadeState = iniFile.GetBool("RecordReshadeState", "General");
	}

	// more settings here
}


void saveGeneralSettingsToIniFile(CDataFile& iniFile)
{
	iniFile.SetBool("RecordReshadeState", g_recordReshadeState, "", "General");

	// more settings here
}


void loadIniFile()
{
	CDataFile iniFile;
	if(!iniFile.Load(SETTINGS_FILE_NAME))
	{
		// not there
		return;
	}

	g_depthOfFieldController.loadIniFileData(iniFile);
	loadGeneralSettingsFromIniFile(iniFile);
}



void saveIniFile()
{
	CDataFile iniFile;
	g_depthOfFieldController.saveIniFileData(iniFile);

	saveGeneralSettingsToIniFile(iniFile);

	iniFile.SetFileName(SETTINGS_FILE_NAME);
	iniFile.Save();
}


bool requiredTechniqueEnabled(const std::string& effectName, const std::string& techniqueName , effect_runtime* runtime)
{
	const auto techniqueHandle = runtime->find_technique(effectName.c_str(), techniqueName.c_str());
	if(techniqueHandle.handle <= 0)
	{
		return false;
	}

	return runtime->get_technique_state(techniqueHandle);
}


/// Based on Reshade's functions doing the same thing but simplified as we only need it for ints
///	Replaces controls like ImGui::DragInt("Quality", &quality, 1, 1, 100);
///	Returns true if changed, false otherwise.
static bool intDragWithButtons(const char* label, int* value, int speed, int min, int max, const char* toolTip=nullptr)
{
	const float button_size = ImGui::GetFrameHeight();
	const float button_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(label);

	ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - (2 * (button_spacing + button_size)));
	bool toReturn = ImGui::DragInt("##v", value, speed, min, max);
	if(nullptr!=toolTip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		ImGui::SetTooltip(toolTip);
	}
	ImGui::SameLine(0, button_spacing);
	if(ImGui::Button("<", ImVec2(button_size, 0)))
	{
		*value -= speed;
		toReturn = true;
	}
	ImGui::SameLine(0, button_spacing);
	if(ImGui::Button(">", ImVec2(button_size, 0)))
	{
		*value += speed;
		toReturn = true;
	}

	ImGui::PopID();
	ImGui::SameLine(0, button_spacing);
	ImGui::TextUnformatted(label);
	ImGui::EndGroup();
	return toReturn;
}


static void displaySettings(reshade::api::effect_runtime* runtime)
{
	ImGui::AlignTextToFramePadding();
	const auto cameraData = (CameraToolsData*)g_dataFromCameraToolsBuffer;
	if(ImGui::CollapsingHeader("Screenshot features"))
	{
		if(g_cameraToolsConnector.cameraToolsConnected() && nullptr != cameraData)
		{
			switch(g_screenshotController.getState())
			{
				case ScreenshotControllerState::Off:
					{
						ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.5f);
						ImGui::AlignTextToFramePadding();
						ImGui::InputText("Screenshot output directory", g_screenshotSettings.screenshotFolder, 256);
						ImGui::SliderInt("Number of frames to wait between steps", &g_screenshotSettings.numberOfFramesToWaitBetweenSteps, 1, 100);
#ifdef _DEBUG
						ImGui::Combo("Multi-screenshot type", &g_screenshotSettings.typeOfScreenshot, "Horizontal panorama\0Lightfield\0DEBUG: Grid\0");
#else
						ImGui::Combo("Multi-screenshot type", &g_screenshotSettings.typeOfScreenshot, "Horizontal panorama\0Lightfield\0\0");
#endif
						ImGui::Combo("File type", &g_screenshotSettings.screenshotFileType, "Bmp\0Jpeg\0Png\0\0");
						switch(g_screenshotSettings.typeOfScreenshot)
						{
							case (int)ScreenshotType::HorizontalPanorama:
								ImGui::SliderFloat("Total field of view in panorama (in degrees)", &g_screenshotSettings.pano_totalAngleDegrees, 30.0f, 360.0f, "%.1f");
								ImGui::SliderFloat("Percentage of overlap between shots", &g_screenshotSettings.pano_overlapPercentagePerShot, 0.1f, 99.0f, "%.1f");
								break;
							case (int)ScreenshotType::MultiShot:
								ImGui::SliderFloat("Distance between Lightfield shots", &g_screenshotSettings.lightField_distanceBetweenShots, 0.0f, 5.0f, "%.3f");
								ImGui::SliderInt("Number of shots to take", &g_screenshotSettings.lightField_numberOfShotsToTake, 0, 60);
								break;
								// others: ignore.
						}
						ImGui::PopItemWidth();
						if(cameraData->cameraEnabled)
						{
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
						else
						{
							ImGui::Text("Camera disabled so no screenshot session can be started");
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
	if(ImGui::CollapsingHeader("Depth of Field control", ImGuiTreeNodeFlags_DefaultOpen) && nullptr != g_dataFromCameraToolsBuffer)
	{
		// start: button with 'Start session'
		// step 1: set value A and B and show Render button
		// step 2: rendering: show 'Done' and 'Cancel' buttons
		// end: go back to the start step.
		if(g_cameraToolsConnector.cameraToolsConnected() && nullptr!=cameraData)
		{
			switch(g_depthOfFieldController.getState())
			{
				case DepthOfFieldControllerState::Off:
					{
						// always pass the state to the shader, so it'll reset to 'OFF' if the user accidentally quit the game without clicking done
						g_depthOfFieldController.writeVariableStateToShader(runtime);

						if(cameraData->cameraEnabled)
						{
							if(requiredTechniqueEnabled("IgcsDof.fx", "IgcsDOF", runtime))
							{
								if(ImGui::Button("Start depth-of-field session"))
								{
									g_depthOfFieldController.startSession(runtime);
								}
							}
							else
							{
								ImGui::TextWrapped("On the ReShade 'Home' tab, please enable 'IgcsDOF' and place it at the bottom of the list by dragging it there.");
							}
						}
						else
						{
							ImGui::TextWrapped("The camera is currently disabled so no depth-of-field session can be started");
						}
					}
					break;
				case DepthOfFieldControllerState::Setup:
					{
						if(cameraData->cameraEnabled)
						{
							ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.40f);
							ImGui::AlignTextToFramePadding();

							ImGui::SeparatorText("Focusing");
							float maxBokehSize = g_depthOfFieldController.getMaxBokehSize();
							bool changed = ImGui::DragFloat("Max. bokeh size", &maxBokehSize, 0.001f, 0.001f, 10.0f, "%.3f");
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("Use this value to define the maximum bokeh size.");
							}
							if(changed)
							{
								g_depthOfFieldController.setMaxBokehSize(runtime, maxBokehSize);
							}

							float focusDelta = g_depthOfFieldController.getXFocusDelta();
							changed = ImGui::DragFloat("Focus delta X", &focusDelta, 0.00005f, -1.0f, 1.0f, "%.5f");
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("Use this value to align the two images\non the spot you want to have in focus");
							}
							if(changed)
							{
								g_depthOfFieldController.setXFocusDelta(runtime, focusDelta);
							}
							int frameWaitType = (int)g_depthOfFieldController.getFrameWaitType();
							changed = ImGui::Combo("Frame wait type", &frameWaitType, "Fast\0Classic (slower)\0\0");
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("Fast is using a system which renders at full frame rate.\nYou need to pick the number of frames to wait value that gives a sharp focus area.\n\nClassic is slower, it uses the number of frames to wait to delay the next frame\nso the higher the value, the slower the rendering. Classis is more reliable to have sharp focus areas.");
							}
							if(changed)
							{
								g_depthOfFieldController.setFrameWaitType((DepthOfFieldFrameWaitType)frameWaitType);
							}
							std::string toolTipText = "";
							switch(frameWaitType)
							{
								case (int)DepthOfFieldFrameWaitType::Fast:
									toolTipText = "Use this value to define the blend delay.\nUsually 1 or 2. For engines with a long render pipeline you might to\nneed to increase this value. Increasing this value doesn't increase the render time.";
									break;
								case (int)DepthOfFieldFrameWaitType::Classic:
									toolTipText = "Use this value to specify a delay during blending a frame.\nUsually 1 or higher but if the engine uses a lot of temporal effects\nyou might need to increase this value.\nIncreasing this value will increase the render time.";

									break;
							}
							int numberOfFramesToWaitPerFrame = g_depthOfFieldController.getNumberOfFramesToWaitPerFrame();
							changed = intDragWithButtons("Number of frames to wait per frame", &numberOfFramesToWaitPerFrame, 1, 1, 20, toolTipText.c_str());
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip(toolTipText.c_str());
							}
							if(changed)
							{
								g_depthOfFieldController.setNumberOfFramesToWaitPerFrame(numberOfFramesToWaitPerFrame);
							}

							ImGui::SeparatorText("Magnifier");
							auto& magnifierSettings = g_depthOfFieldController.getMagnifierSettings();
							ImGui::Checkbox("Show magnifier", &magnifierSettings.ShowMagnifier);
							ImGui::DragFloat("Magnification factor", &magnifierSettings.MagnificationFactor, 1.0f, 1.0f, 10.0f, "%.0f");

							float tempValues[2] = { magnifierSettings.WidthMagnifierArea, magnifierSettings.HeightMagnifierArea };
							changed = ImGui::DragFloat2("Magnifier area size", tempValues, 0.001f, 0.01f, 1.0f);
							if(changed)
							{
								magnifierSettings.WidthMagnifierArea = tempValues[0];
								magnifierSettings.HeightMagnifierArea = tempValues[1];
							}
							tempValues[0] = magnifierSettings.XMagnifierLocation;
							tempValues[1] = magnifierSettings.YMagnifierLocation;
							changed = ImGui::DragFloat2("Magnifier location", tempValues, 0.001f, 0.01f, 1.0f);
							if(changed)
							{
								magnifierSettings.XMagnifierLocation = tempValues[0];
								magnifierSettings.YMagnifierLocation = tempValues[1];
							}

							ImGui::SeparatorText("Bokeh setup");
							int blurType = (int)g_depthOfFieldController.getBlurType();
							changed = ImGui::Combo("Blur type", &blurType, "Aperture shaped\0Circular\0\0");
							if(changed)
							{
								g_depthOfFieldController.setBlurType((DepthOfFieldBlurType)blurType);
							}

							int quality = g_depthOfFieldController.getQuality();
							changed = intDragWithButtons("Quality", &quality, 1, 1, 100);
							if(changed)
							{
								g_depthOfFieldController.setQuality(quality);
							}
							switch((DepthOfFieldBlurType)blurType)
							{
								case DepthOfFieldBlurType::ApertureShape:
									{
										bool shapeSettingsChanged = false;
										auto& shapeSettings = g_depthOfFieldController.getApertureShapeSettings();
										shapeSettingsChanged |= intDragWithButtons("Number of vertices", &shapeSettings.NumberOfVertices, 1, 3, 10);
										shapeSettingsChanged |= ImGui::DragFloat("Rounding factor", &shapeSettings.RoundFactor, 0.001f, 0.0f, 1.0f);
										shapeSettingsChanged |= ImGui::DragFloat("Rotation angle", &shapeSettings.RotationAngle, 0.001f, 0.0f, 1.0f);		// multiplier to 2PI.

										if(shapeSettingsChanged)
										{
											g_depthOfFieldController.invalidateShapePoints();
										}
									}
									break;
								case DepthOfFieldBlurType::Circular:
									{
										int numberOfPointsInnermostCircle = g_depthOfFieldController.getNumberOfPointsInnermostRing();
										changed = intDragWithButtons("Number of points of innermost ring", &numberOfPointsInnermostCircle, 1, 1, 100);
										if(changed)
										{
											g_depthOfFieldController.setNumberOfPointsInnermostRing(numberOfPointsInnermostCircle);
										}
									}
									break;
							}
							float ringAngleOffset = g_depthOfFieldController.getRingAngleOffset();
							changed = ImGui::DragFloat("Ring angle offset", &ringAngleOffset, 0.001f, -0.015f, 0.015f);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("This offset lets you rotate rings relative\nto each other to avoid the common grid pattern with lower\namount of rings.");
							}
							if(changed)
							{
								g_depthOfFieldController.setRingAngleOffset(ringAngleOffset);
							}
							float anamorphicFactor = g_depthOfFieldController.getAnamorphicFactor();
							changed = ImGui::DragFloat("Anamorphic factor", &anamorphicFactor, 0.001f, 0.01f, 1.0f);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("Mainly meant for circular shapes\nAt 1.0 it gives perfect round bokehs,\n at a lower value it gives vertical ellipses.");
							}
							if(changed)
							{
								g_depthOfFieldController.setAnamorphicFactor(anamorphicFactor);
							}							

							float sphericalAberrationDimFactor = g_depthOfFieldController.getSphericalAberrationDimFactor();
							changed = ImGui::DragFloat("Spherical aberration dim factor", &sphericalAberrationDimFactor, 0.001f, 0.0f, 1.0f);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("The factor for how much dimming center pixels in bokeh highlights will receive.");
							}
							if(changed)
							{
								g_depthOfFieldController.setSphericalAberrationDimFactor(sphericalAberrationDimFactor);
							}

							float fringeIntensity = g_depthOfFieldController.getFringeIntensity();
							changed = ImGui::DragFloat("Fringe intensity", &fringeIntensity, 0.001f, 0.0f, 1.0f);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("Intensity of bokeh outline.\nUsing a value close to 1.0 could lead to having your screen go black\nduring the render phase. This is normal.");
							}
							if(changed)
							{
								g_depthOfFieldController.setFringeIntensity(fringeIntensity);
							}

							float fringeWidth = g_depthOfFieldController.getFringeWidth();
							changed = ImGui::DragFloat("Fringe width", &fringeWidth, 0.001f, 0.0f, 1.0f);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("Width of bokeh outline");
							}
							if(changed)
							{
								g_depthOfFieldController.setFringeWidth(fringeWidth);
							}
							int caType = (int)g_depthOfFieldController.getCAType();
							changed = ImGui::Combo("Chromatic aberration type", &caType, "Red-Green-Blue\0Red-Green\0Red-Blue\0Blue-Green\0\0");
							if(changed)
							{
								g_depthOfFieldController.setCAType((DepthOfFieldCAType)caType);
							}
							float caStrength = g_depthOfFieldController.getCAStrength();
							changed = ImGui::DragFloat("Chromatic aberration strength", &caStrength, 0.000f, 0.0f, 1.0f);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("The strength of the chromatic aberration on the edges of the bokeh highlights");
							}
							if(changed)
							{
								g_depthOfFieldController.setCAStrength(caStrength);
							}
							float caWidth = g_depthOfFieldController.getCAWidth();
							changed = ImGui::DragFloat("Chromatic aberration width", &caWidth, 0.001f, 0.0f, 1.0f);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("Width of outer edge of the bokeh highlight\nto which chromatic aberration is applied.");
							}
							if(changed)
							{
								g_depthOfFieldController.setCAWidth(caWidth);
							}
							float catEyeBokehIntensity = g_depthOfFieldController.getCatEyeBokehIntensity();
							changed = ImGui::DragFloat("Cateye bokeh intensity", &catEyeBokehIntensity, 0.001f, -1.0f, 1.0f);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("The intensity of the cat-eye effect in the bokeh.\nNegative values cut the bokeh shapes from the inside\nPositive values cut the bokeh shapes from the outside\nZero means no cateye bokeh.");
							}
							if(changed)
							{
								g_depthOfFieldController.setCatEyeBokehIntensity(catEyeBokehIntensity);
							}
							float catEyeRadiusStart = g_depthOfFieldController.getCatEyeRadiusStart();
							changed = ImGui::DragFloat("Cateye bokeh radius start", &catEyeRadiusStart, 0.001f, 0.0f, 1.0f);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("The distance from the center of the screen where the cateye bokeh effect has to start.\nThis value has to be smaller than the Cateye bokeh radius end value.");
							}
							if(changed)
							{
								g_depthOfFieldController.setCatEyeRadiusStart(catEyeRadiusStart);
							}
							float catEyeRadiusEnd = g_depthOfFieldController.getCatEyeRadiusEnd();
							changed = ImGui::DragFloat("Cateye bokeh radius end", &catEyeRadiusEnd, 0.001f, 0.0f, 1.0f);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("The distance from the center of the screen where the cateye bokeh effect has to end.\nThis value has to be bigger than the Cateye bokeh radius start value.");
							}
							if(changed)
							{
								g_depthOfFieldController.setCatEyeRadiusEnd(catEyeRadiusEnd);
							}
							bool addCatEyeVignette = g_depthOfFieldController.getAddCatEyeVignette();
							changed = ImGui::Checkbox("Add a vignette darkening to the cateye bokeh", &addCatEyeVignette);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("If checked, a vignette is applied to the cateye bokeh effect.\nHas no effect if cateye bokeh intensity is (close to) 0.");
							}
							if(changed)
							{
								g_depthOfFieldController.setAddCatEyeVignette(addCatEyeVignette);
							}

							int renderOrder = (int)g_depthOfFieldController.getRenderOrder();
							changed = ImGui::Combo("Render order", &renderOrder, "Inner to outer ring\0Outer to inner ring\0Random\0\0");
							if(changed)
							{
								g_depthOfFieldController.setRenderOrder((DepthOfFieldRenderOrder)renderOrder);
							}

							float highlightBoostFactor = g_depthOfFieldController.getHighlightBoostFactor();
							changed = ImGui::DragFloat("Highlight boost factor", &highlightBoostFactor, 0.001f, 0.0f, 1.0f, "%.3f");
							if(changed)
							{
								g_depthOfFieldController.setHighlightBoostFactor(highlightBoostFactor);
							}
							float highlightGammaFactor = g_depthOfFieldController.getHighlightGammaFactor();
							changed = ImGui::DragFloat("Highlight gamma factor", &highlightGammaFactor, 0.001f, 0.1f, 5.0f, "%.3f");
							if(changed)
							{
								g_depthOfFieldController.setHighlightGammaFactor(highlightGammaFactor);
							}

							// show the shape canvas
							ImGui::Text("Blur shape. Number of shots to take: %d", g_depthOfFieldController.getTotalNumberOfStepsToTake());
							ImGui::InvisibleButton("canvas", ImVec2(250.0f, 250.0f), ImGuiButtonFlags_None);
							const ImVec2 topLeftCoords = ImGui::GetItemRectMin();
							const ImVec2 bottomRightCoords = ImGui::GetItemRectMax();
							ImDrawList* drawList = ImGui::GetWindowDrawList();
							drawList->AddRectFilled(topLeftCoords, bottomRightCoords, IM_COL32(50, 50, 50, 255));
							drawList->AddRect(topLeftCoords, bottomRightCoords, IM_COL32(255, 255, 255, 255));
							g_depthOfFieldController.drawShape(drawList, topLeftCoords, 250.0f);

							ImGui::Separator();
							bool showProgressBarAsOverlay = g_depthOfFieldController.getShowProgressBarAsOverlay();
							changed = ImGui::Checkbox("Show progress bar as overlay", &showProgressBarAsOverlay);
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("If checked, the progress bar will show in the top left corner\notherwise the progress bar will be shown here in the Reshade overlay.");
							}
							if(changed)
							{
								g_depthOfFieldController.setShowProgressBarAsOverlay(showProgressBarAsOverlay);
							}
							if(ImGui::Button("Start render"))
							{
								g_depthOfFieldController.startRender(runtime);
							}
							ImGui::SameLine();
							if(ImGui::Button("Cancel"))
							{
								g_depthOfFieldController.endSession(runtime);
							}
#if _DEBUG
							if(ImGui::CollapsingHeader("Debug"))
							{
								float debugVal1 = g_depthOfFieldController.getDebugVal1();
								changed = ImGui::DragFloat("Debug val1", &debugVal1, 0.001f, -2.0f, 2.0f, "%.3f");
								if(changed)
								{
									g_depthOfFieldController.setDebugVal1(debugVal1);
								}
								float debugVal2 = g_depthOfFieldController.getDebugVal2();
								changed = ImGui::DragFloat("Debug val2", &debugVal2, 0.001f, -20.0f, 20.0f, "%.3f");
								if(changed)
								{
									g_depthOfFieldController.setDebugVal2(debugVal2);
								}
								bool debugBool1 = g_depthOfFieldController.getDebugBool1();
								changed = ImGui::Checkbox("Debug bool 1", &debugBool1);
								if(changed)
								{
									g_depthOfFieldController.setDebugBool1(debugBool1);
								}
								bool debugBool2 = g_depthOfFieldController.getDebugBool2();
								changed = ImGui::Checkbox("Debug bool 2", &debugBool2);
								if(changed)
								{
									g_depthOfFieldController.setDebugBool2(debugBool2);
								}
							}
#endif
							ImGui::PopItemWidth();
						}
						else
						{
							g_depthOfFieldController.endSession(runtime);
						}
					}
					break;
				case DepthOfFieldControllerState::Rendering:
					{
						if(!g_depthOfFieldController.getShowProgressBarAsOverlay())
						{
							g_depthOfFieldController.renderProgressBar();
						}

						const bool isPaused = g_depthOfFieldController.getRenderPaused();
						if(isPaused)
						{
							ImGui::Text("Rendering (Paused), please wait...");
							if(ImGui::Button("Unpause rendering"))
							{
								g_depthOfFieldController.setRenderPaused(false);
							}
						}
						else
						{
							ImGui::Text("Rendering, please wait...");
							if(ImGui::Button("Pause rendering"))
							{
								g_depthOfFieldController.setRenderPaused(true);
							}
						}
						ImGui::SameLine();
						if(ImGui::Button("Cancel"))
						{
							g_depthOfFieldController.endSession(runtime);
						}
					}
					break;
				case DepthOfFieldControllerState::Done:
					ImGui::Text("Done. You can now take a screenshot.\n\n");
					ImGui::Text("Click 'End session' to end this session.\nThis will remove the rendering result.");
					if(ImGui::Button("End session"))
					{
						g_depthOfFieldController.endSession(runtime);
						saveIniFile();
					}
					break;
				case DepthOfFieldControllerState::Cancelling:
					ImGui::Text("Cancelling session...");
					break;
			}
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
			std::ostringstream stringStream;
			stringStream << std::fixed << std::setprecision(2) << cameraData->fov;
			const std::string fovAsString = stringStream.str();
			ImGui::Text(cameraData->cameraEnabled ? "Camera enabled" : "Camera disabled");
			ImGui::Text(cameraData->cameraMovementLocked ? "Camera movement locked" : "Camera movement unlocked");
			ImGui::InputText("FoV (degrees)", (char*)fovAsString.c_str(), fovAsString.length(), ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Camera coordinates", cameraData->coordinates.values, "%.4f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat4("Camera look quaternion", cameraData->lookQuaternion.values, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Rotation matrix Right", cameraData->rotationMatrixRightVector.values, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Rotation matrix Up", cameraData->rotationMatrixUpVector.values, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat3("Rotation matrix Forward", cameraData->rotationMatrixForwardVector.values, "%.3f", ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat("Pitch (radians)", &cameraData->pitch, ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat("Yaw (radians)", &cameraData->yaw, ImGuiInputTextFlags_ReadOnly);
			ImGui::InputFloat("Roll (radians)", &cameraData->roll, ImGuiInputTextFlags_ReadOnly);
		}
	}
	ImGui::AlignTextToFramePadding();
	if(ImGui::CollapsingHeader("Camera path info"))
	{
		if(nullptr==g_dataFromCameraToolsBuffer)
		{
			ImGui::Text("Camera path info not available");
		}
		else
		{
			ImGui::Checkbox("Record ReShade state with camera nodes", &g_recordReshadeState);
			ImGui::Text("Number of saved ReShade states per path:");

			const auto numberOfPaths = g_reshadeStateController.numberOfPaths();
			if(numberOfPaths<=0)
			{
				ImGui::Text("None.");
			}
			else
			{
				for(int i = 0; i < numberOfPaths; i++)
				{
					// path no's are starting at 0 but for display purposes we start at 1.
					ImGui::Text("Path: %d. # of saved Reshade states: %d.", (i + 1), g_reshadeStateController.numberOfSnapshotsOnPath(i));
				}
			}
		}
	}
}


void sendCameraToolsDataToUniforms(effect_runtime* runtime)
{
	// the following source variables are defined with their types:
	// IGCS_cameraDataAvailable			bool
	// IGCS_cameraEnabled				bool
	// IGCS_cameraMovementLocked		bool
	// IGCS_cameraFoV					float		degrees
	// IGCS_cameraWorldPosition			float3
	// IGCS_cameraOrientation			float4		quaternion (x,y,z,w)
	// IGCS_cameraViewMatrix4x4			float4x4
	// IGCS_cameraProjectionMatrix4x4LH	float4x4	calculated from fov + aspect ratio + near of 0.1 and far of 10000.0, using left handed row major DirectX math
	// IGCS_cameraUp					float3		up vector of 3x3 part of view matrix
	// IGCS_cameraRight					float3		right vector of 3x3 part of view matrix
 	// IGCS_cameraForward				float3		forward vector of 3x3 part of view matrix
	// IGCS_cameraRotationPitch			float		radians
	// IGCS_cameraRotationYaw			float		radians
	// IGCS_cameraRotationRoll			float		radians
	const auto cameraData = (CameraToolsData*)g_dataFromCameraToolsBuffer;
	if(nullptr==cameraData)
	{
		runtime->enumerate_uniform_variables(nullptr, [](effect_runtime* runtime, effect_uniform_variable variable)
		{
			char source[32];
			if(runtime->get_annotation_string_from_uniform_variable(variable, "source", source) && std::strcmp(source, "IGCS_cameraDataAvailable") == 0)
			{
				runtime->set_uniform_value_bool(variable, false);
			}
		});
		return;
	}

	runtime->enumerate_uniform_variables(nullptr, [&cameraData](effect_runtime *runtime, effect_uniform_variable variable) 
	{
		char source[64];
		runtime->get_annotation_string_from_uniform_variable(variable, "source", source);
		if(std::strcmp(source, "IGCS_cameraDataAvailable") == 0)
		{
			runtime->set_uniform_value_bool(variable, true);
		}
		else if(std::strcmp(source, "IGCS_cameraEnabled") == 0)
		{
			runtime->set_uniform_value_bool(variable, cameraData->cameraEnabled);
		}
		else if(std::strcmp(source, "IGCS_cameraMovementLocked") == 0)
		{
			runtime->set_uniform_value_bool(variable, cameraData->cameraMovementLocked);
		}
		else if(std::strcmp(source, "IGCS_cameraFoV") == 0)
		{
			runtime->set_uniform_value_float(variable, cameraData->fov);
		}
		else if(std::strcmp(source, "IGCS_cameraWorldPosition") == 0)
		{
			runtime->set_uniform_value_float(variable, cameraData->coordinates.x(), cameraData->coordinates.y(), cameraData->coordinates.z());
		}
		else if(std::strcmp(source, "IGCS_cameraOrientation") == 0)
		{
			runtime->set_uniform_value_float(variable, cameraData->lookQuaternion.x(), cameraData->lookQuaternion.y(), cameraData->lookQuaternion.z(), cameraData->lookQuaternion.w());
		}
		else if(std::strcmp(source, "IGCS_cameraViewMatrix4x4") == 0)
		{
			const auto matrixAsFlatVector = cameraData->lookQuaternion.toFlatVector();
			runtime->set_uniform_value_float(variable, &matrixAsFlatVector[0], 16, 0);
		}
		else if(std::strcmp(source, "IGCS_cameraProjectionMatrix4x4LH") == 0)
		{
			const auto device = runtime->get_device();
			if(nullptr!=device && cameraData->fov>0)
			{
				const auto currentBackBuffer = runtime->get_current_back_buffer();
				if(currentBackBuffer.handle>0)
				{
					const auto description = device->get_resource_desc(currentBackBuffer);
					const auto width = static_cast<float>(description.texture.width);
					auto height = static_cast<float>(description.texture.height);
					if(height<DirectX::g_XMEpsilon.f[0])
					{
						height = 0.1;
					}
					const auto aspectRatio = width / height;
					const auto projectionMatrixLH = DirectX::XMMatrixPerspectiveFovLH(IGCS::Utils::degreesToRadians(cameraData->fov), aspectRatio, 0.1f, 10000.0f);
					const auto projectionMatrixAsFlatVector = IGCS::Utils::XMFloat4x4ToFlatVector(projectionMatrixLH);
					runtime->set_uniform_value_float(variable, &projectionMatrixAsFlatVector[0], 16, 0);
				}
			}
		}
		else if(std::strcmp(source, "IGCS_cameraRotationPitch") == 0)
		{
			runtime->set_uniform_value_float(variable, cameraData->pitch);
		}
		else if(std::strcmp(source, "IGCS_cameraRotationYaw") == 0)
		{
			runtime->set_uniform_value_float(variable, cameraData->yaw);
		}
		else if(std::strcmp(source, "IGCS_cameraRotationRoll") == 0)
		{
			runtime->set_uniform_value_float(variable, cameraData->roll);
		}
		else if(std::strcmp(source, "IGCS_cameraUp") == 0)
		{
			runtime->set_uniform_value_float(variable, cameraData->rotationMatrixUpVector.x(), cameraData->rotationMatrixUpVector.y(), cameraData->rotationMatrixUpVector.z());
		}
		else if(std::strcmp(source, "IGCS_cameraRight") == 0)
		{
			runtime->set_uniform_value_float(variable, cameraData->rotationMatrixRightVector.x(), cameraData->rotationMatrixRightVector.y(), cameraData->rotationMatrixRightVector.z());
		}
		else if(std::strcmp(source, "IGCS_cameraForward") == 0)
		{
			runtime->set_uniform_value_float(variable, cameraData->rotationMatrixForwardVector.x(), cameraData->rotationMatrixForwardVector.y(), cameraData->rotationMatrixForwardVector.z());
		}
		// add more here. 
	});
}


void onReshadeReloadEffects(effect_runtime* runtime)
{
	// This call can be made in various scenarios, but they have either one of 2 characteristics: 1) there are 0 effects or 2) there are effects but they're changing.
	// We can safely ignore the first one, as that's the one originating from the call to destroy_effects. All the other scenarios are from update_effects which is
	// called in on_present and will end up raising the event in multiple scenarios.
	g_reshadeStateController.migrateContainedHandles(runtime);
	g_depthOfFieldController.migrateReshadeState(runtime);
}

void onReshadeBeginEffects(effect_runtime* runtime, command_list* cmd_list, resource_view rtv, resource_view rtv_srgb)
{
	sendCameraToolsDataToUniforms(runtime);
	g_depthOfFieldController.reshadeBeginEffectsCalled(runtime);
}


void onReshadeFinishEffects(effect_runtime* runtime, command_list* cmd_list, resource_view rtv, resource_view rtv_srgb)
{
	g_depthOfFieldController.reshadeFinishEffectsCalled(runtime);
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
		reshade::register_event<reshade::addon_event::reshade_begin_effects>(onReshadeBeginEffects);
		reshade::register_event<reshade::addon_event::reshade_finish_effects>(onReshadeFinishEffects);
		reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(onReshadeReloadEffects);
		reshade::register_overlay(nullptr, &displaySettings);
		loadIniFile();
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_event<reshade::addon_event::reshade_present>(onReshadePresent);
		reshade::unregister_event<reshade::addon_event::reshade_overlay>(onReshadeOverlay);
		reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(onReshadeBeginEffects);
		reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(onReshadeFinishEffects);
		reshade::unregister_event<reshade::addon_event::reshade_reloaded_effects>(onReshadeReloadEffects);
		reshade::unregister_overlay(nullptr, &displaySettings);
		reshade::unregister_addon(hModule);
		if(nullptr!=g_dataFromCameraToolsBuffer)
		{
			free(g_dataFromCameraToolsBuffer);
		}
		saveIniFile();
		break;
	}

	return TRUE;
}
