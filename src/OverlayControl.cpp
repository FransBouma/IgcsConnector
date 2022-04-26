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
#include "OverlayControl.h"
#include "imgui.h"
#include <reshade.hpp>
#include <algorithm>
#include <mutex>
#include <shared_mutex>

using namespace std;

namespace OverlayControl
{
	//-----------------------------------------------
	// private structs
	struct Notification
	{
		string notificationText;
		float timeFirstDisplayed;		// if < 0, never displayed
	};

	//-----------------------------------------------
	// statics
	static vector<Notification> _activeNotifications;
	static std::shared_mutex _notificationsMutex;
	
	//-----------------------------------------------
	// forward declarations
	void renderNotifications();
	void updateNotificationStore();

	//-----------------------------------------------
	// code

	void addNotification(string notificationText)
	{
		std::unique_lock ulock(_notificationsMutex);
		Notification toAdd;
		toAdd.notificationText = notificationText;
		toAdd.timeFirstDisplayed = -1.0f;
		_activeNotifications.push_back(toAdd);
	}

	void renderOverlay()
	{
		renderNotifications();
	}


	void renderNotifications()
	{
		updateNotificationStore();
		{
			std::shared_lock sLock(_notificationsMutex);
			if(_activeNotifications.size() > 0)
			{
				ImGui::SetNextWindowBgAlpha(0.9f);
				ImGui::SetNextWindowPos(ImVec2(10, 10));
				if(ImGui::Begin("IgcsConnector_Notifications", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
				{
					for(vector<Notification>::iterator it = _activeNotifications.begin(); it < _activeNotifications.end(); ++it)
					{
						ImGui::Text(it->notificationText.c_str());
					}
				}
				ImGui::End();
			}
		}
	}


	// will purge all outdated notifications from the set of active notifications. Be sure to call this from within a mutex lock, as notifications are added from another thread
	void updateNotificationStore()
	{
		float currentRenderTime = ImGui::GetTime();		// time overall,  in seconds

		// for now we'll use 2 seconds (2.0f). First purge everything that's first rendered more than delta T ago
		float maxTimeOnScreen = 2.0f;
		std::unique_lock ulock(_notificationsMutex);
		std::erase_if(_activeNotifications,
			         [currentRenderTime, maxTimeOnScreen](Notification const & x) -> bool 
			         {
				         return (x.timeFirstDisplayed > 0.0f && (currentRenderTime - x.timeFirstDisplayed) > maxTimeOnScreen); 
			         });
		// now update all notifications which have a negative time (so haven't been shown) and set them to the current time.
		std::ranges::for_each(_activeNotifications, 
		                 [currentRenderTime](Notification & x) 
		                 { 
			                 if (x.timeFirstDisplayed < 0.0f) 
			                 {
				                 x.timeFirstDisplayed = currentRenderTime;
			                 }
		                 });
	}
}

