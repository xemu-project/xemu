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
#include "common.hh"

const ImVec2 EASE_VECTOR_DOWN  = ImVec2(0, -25);
const ImVec2 EASE_VECTOR_LEFT  = ImVec2(25, 0);
const ImVec2 EASE_VECTOR_RIGHT = ImVec2(-25, 0);

enum AnimationState
{
    PreEasingIn,
    EasingIn,
    Idle,
    EasingOut,
    PostEasingOut
};

// Step a value from 0 to 1 over some duration of time.
class Animation
{
protected:
    float m_duration;
    float m_acc;

public:
    Animation(float duration = 0);
    void Reset();
    void SetDuration(float duration);
    void Step();
    bool IsComplete();
    float GetLinearValue();
    void SetLinearValue(float t);
    float GetSinInterpolatedValue();
};

// Stateful animation sequence for easing in and out: 0->1->0
class EasingAnimation
{
protected:
    AnimationState m_state;
    Animation m_animation;
    float m_duration_out;
    float m_duration_in;

public:
    EasingAnimation(float ease_in_duration = 1.0, float ease_out_duration = 1.0);
    void EaseIn();
    void EaseIn(float duration);
    void EaseOut();
    void EaseOut(float duration);
    void Step();
    float GetLinearValue();
    float GetSinInterpolatedValue();
    bool IsAnimating();
    bool IsComplete();
};
