//
// xemu User Interface
//
// Copyright (C) 2020-2022 Matt Borgerson
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "notifications.hh"
#include "common.hh"

#include "../xemu-notifications.h"

NotificationManager notification_manager;

NotificationManager::NotificationManager()
{
    m_active = false;
}

void NotificationManager::QueueNotification(const char *msg)
{
    m_notification_queue.push_back(strdup(msg));
}

void NotificationManager::QueueError(const char *msg)
{
    m_error_queue.push_back(strdup(msg));
}

void NotificationManager::Draw()
{
    uint32_t now = SDL_GetTicks();

    if (m_active) {
        // Currently displaying a notification
        float t =
            (m_notification_end_time - now) / (float)kNotificationDuration;
        if (t > 1.0) {
            // Notification delivered, free it
            free((void *)m_msg);
            m_active = false;
        } else {
            // Notification should be displayed
            DrawNotification(t, m_msg);
        }
    } else {
        // Check to see if a notification is pending
        if (m_notification_queue.size() > 0) {
            m_msg = m_notification_queue[0];
            m_active = true;
            m_notification_end_time = now + kNotificationDuration;
            m_notification_queue.pop_front();
        }
    }

    ImGuiIO& io = ImGui::GetIO();

    if (m_error_queue.size() > 0) {
        ImGui::OpenPopup("Error");
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x/2, io.DisplaySize.y/2),
                                ImGuiCond_Always, ImVec2(0.5, 0.5));
    }
    if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s", m_error_queue[0]);
        ImGui::Dummy(ImVec2(0,16));
        ImGui::SetItemDefaultFocus();
        ImGuiStyle &style = ImGui::GetStyle();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-(120+2*style.FramePadding.x));
        if (ImGui::Button("Ok", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            free((void*)m_error_queue[0]);
            m_error_queue.pop_front();
        }
        ImGui::EndPopup();
    }
}

void NotificationManager::DrawNotification(float t, const char *msg)
{
    if (!g_config.display.ui.show_notifications) {
        return;
    }

    const float DISTANCE = 10.0f;
    static int corner = 1;
    ImGuiIO& io = ImGui::GetIO();
    if (corner != -1)
    {
        ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE, (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
        window_pos.y = g_main_menu_height + DISTANCE;
        ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    }

    const float fade_in  = 0.1;
    const float fade_out = 0.9;
    float fade = 0;

    if (t < fade_in) {
        // Linear fade in
        fade = t/fade_in;
    } else if (t >= fade_out) {
        // Linear fade out
        fade = 1-(t-fade_out)/(1-fade_out);
    } else {
        // Constant
        fade = 1.0;
    }

    ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
    color.w *= fade;
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0,0,0,fade*0.9f));
    ImGui::PushStyleColor(ImGuiCol_Border, color);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::SetNextWindowBgAlpha(0.90f * fade);
    if (ImGui::Begin("Notification", NULL,
        ImGuiWindowFlags_Tooltip |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs
        ))
    {
        ImGui::Text("%s", msg);
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::End();
}

/* External interface, exposed via xemu-notifications.h */

void xemu_queue_notification(const char *msg)
{
    notification_manager.QueueNotification(msg);
}

void xemu_queue_error_message(const char *msg)
{
    notification_manager.QueueError(msg);
}
