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
#include <reshade.hpp>
#include <iomanip>
#include <ios>
#include <Psapi.h>
#include <sstream>
#include <string>

#include "CameraToolsData.h"
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
							if(ImGui::Button("Start depth-of-field session"))
							{
								g_depthOfFieldController.startSession(runtime);
							}
						}
						else
						{
							ImGui::Text("Camera disabled so no depth-of-field session can be started");
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

							float focusDeltas[2] = { (g_depthOfFieldController.getXFocusDelta()), (g_depthOfFieldController.getYFocusDelta()) };
							changed = ImGui::DragFloat2("Focus deltas X, Y", focusDeltas, 0.0001f, -1.0f, 1.0f, "%.4f");
							if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
							{
								ImGui::SetTooltip("Use these two values to align the two images\non the spot you want to have in focus");
							}
							if(changed)
							{
								g_depthOfFieldController.setXYFocusDelta(runtime, focusDeltas[0], focusDeltas[1]);
							}
							int numberOfFramesToWaitPerFrame = g_depthOfFieldController.getNumberOfFramesToWaitPerFrame();
							changed = ImGui::DragInt("Number of frames to wait per frame", &numberOfFramesToWaitPerFrame, 1, 1, 20);
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
#if _DEBUG
							// In debug build we can also pick linear
							changed = ImGui::Combo("Blur type", &blurType, "Linear\0Circular\0\0");
							if(changed)
							{
								g_depthOfFieldController.setBlurType((DepthOfFieldBlurType)blurType);
							}
#endif
							int quality = g_depthOfFieldController.getQuality();
							changed = ImGui::DragInt("Quality", &quality, 1, 1, 100);
							if(changed)
							{
								g_depthOfFieldController.setQuality(quality);
							}
							int numberOfPointsInnermostCircle = g_depthOfFieldController.getNumberOfPointsInnermostRing();
							changed = ImGui::DragInt("Number of points of innermost circle", &numberOfPointsInnermostCircle, 1, 1, 100);
							if(changed)
							{
								g_depthOfFieldController.setNumberOfPointsInnermostRing(numberOfPointsInnermostCircle);
							}

							int renderOrder = (int)g_depthOfFieldController.getRenderOrder();
							changed = ImGui::Combo("Render order", &renderOrder, "Inner to outer circle\0Outer to inner circle\0Random\0\0");
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

							switch(blurType)
							{
								case (int)DepthOfFieldBlurType::Linear:
									g_depthOfFieldController.createLinearDoFPoints();
									break;
								case (int)DepthOfFieldBlurType::Circular:
									g_depthOfFieldController.createCircleDoFPoints();
									break;
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
			ImGui::InputFloat3("Camera coordinates", cameraData->coordinates.values, "%.1f", ImGuiInputTextFlags_ReadOnly);
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
	g_depthOfFieldController.reshadeBeginEffectsCalled(runtime);
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
		reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(onReshadeReloadEffects);
		reshade::register_overlay(nullptr, &displaySettings);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_event<reshade::addon_event::reshade_present>(onReshadePresent);
		reshade::unregister_event<reshade::addon_event::reshade_overlay>(onReshadeOverlay);
		reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(onReshadeBeginEffects);
		reshade::unregister_event<reshade::addon_event::reshade_reloaded_effects>(onReshadeReloadEffects);
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
