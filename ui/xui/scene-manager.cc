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
#include "scene-manager.hh"

SceneManager g_scene_mgr;

SceneManager::SceneManager()
{
    m_active_scene = nullptr;
}

void SceneManager::PushScene(Scene &scene)
{
    m_scenes.insert(m_scenes.begin(), &scene);
}

bool SceneManager::IsDisplayingScene()
{
    return m_active_scene != nullptr || m_scenes.size() > 0;
}

bool SceneManager::Draw()
{
    if (m_active_scene) {
        bool finished = !m_active_scene->Draw();
        if (finished) {
            m_active_scene = nullptr;
        }
        return true;
    } else if (m_scenes.size()) {
        m_active_scene = m_scenes.back();
        m_scenes.pop_back();
        m_active_scene->Show();
    }
    return false;
}
