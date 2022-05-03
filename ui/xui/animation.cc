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
#include <cmath>
#include "common.hh"
#include "animation.hh"

Animation::Animation(float duration)
: m_duration(duration)
{
    Reset();
}

void Animation::Reset()
{
    m_acc = 0;
}

void Animation::SetDuration(float duration)
{
    m_duration = duration;
}

void Animation::Step()
{
    if (g_config.display.ui.use_animations) {
        ImGuiIO &io = ImGui::GetIO();
        m_acc += io.DeltaTime;
    } else {
        m_acc = m_duration;
    }
}

bool Animation::IsComplete()
{
    return m_acc >= m_duration;
}

float Animation::GetLinearValue()
{
    if (m_acc < m_duration) {
        return m_acc / m_duration;
    } else {
        return 1.0;
    }
}

void Animation::SetLinearValue(float t)
{
    m_acc = t * m_duration;
}

float Animation::GetSinInterpolatedValue()
{
    return sin(GetLinearValue() * M_PI * 0.5);
}

EasingAnimation::EasingAnimation(float ease_in_duration, float ease_out_duration)
: m_state(AnimationState::PreEasingIn),
  m_duration_out(ease_out_duration),
  m_duration_in(ease_in_duration) {}

void EasingAnimation::EaseIn()
{
    EaseIn(m_duration_in);
}

void EasingAnimation::EaseIn(float duration)
{
    if (duration == 0) {
        m_state = AnimationState::Idle;
        return;
    }
    float t = m_animation.GetLinearValue();
    m_animation.SetDuration(duration);
    if (m_state == AnimationState::EasingOut) {
        m_animation.SetLinearValue(1-t);
    } else if (m_state != AnimationState::EasingIn) {
        m_animation.Reset();
    }
    m_state = AnimationState::EasingIn;
}

void EasingAnimation::EaseOut()
{
    EaseOut(m_duration_out);
}

void EasingAnimation::EaseOut(float duration)
{
    if (duration == 0) {
        m_state = AnimationState::PostEasingOut;
        return;
    }
    float t = m_animation.GetLinearValue();
    m_animation.SetDuration(duration);
    if (m_state == AnimationState::EasingIn) {
        m_animation.SetLinearValue(1-t);
    } else if (m_state != AnimationState::EasingOut) {
        m_animation.Reset();
    }
    m_state = AnimationState::EasingOut;
}

void EasingAnimation::Step()
{
    if (m_state == AnimationState::EasingIn ||
        m_state == AnimationState::EasingOut) {
        m_animation.Step();
        if (m_animation.IsComplete()) {
            if (m_state == AnimationState::EasingIn) {
                m_state = AnimationState::Idle;
            } else if (m_state == AnimationState::EasingOut) {
                m_state = AnimationState::PostEasingOut;
            }
        }
    }
}

float EasingAnimation::GetLinearValue()
{
    switch (m_state) {
    case AnimationState::PreEasingIn: return 0;
    case AnimationState::EasingIn: return m_animation.GetLinearValue();
    case AnimationState::Idle: return 1;
    case AnimationState::EasingOut: return 1 - m_animation.GetLinearValue();
    case AnimationState::PostEasingOut: return 0;
    default: return 0;
    }
}

float EasingAnimation::GetSinInterpolatedValue()
{
    return sin(GetLinearValue() * M_PI * 0.5);
}

bool EasingAnimation::IsAnimating()
{
    return m_state == AnimationState::EasingIn ||
           m_state == AnimationState::EasingOut;
}

bool EasingAnimation::IsComplete()
{
    return m_state == AnimationState::PostEasingOut;
}
