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
#pragma once
#include <vector>
#include "scene.hh"

class SceneManager
{
protected:
    Scene *m_active_scene;
    std::vector<Scene *> m_scenes;

public:
    SceneManager();
    void PushScene(Scene &scene);
    bool IsDisplayingScene();
    bool Draw();
};

extern SceneManager g_scene_mgr;
