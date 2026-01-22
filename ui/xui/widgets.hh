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
#include <functional>
#include <SDL3/SDL_dialog.h>
#include "common.hh"

void Separator();
void SectionTitle(const char *title);
float GetWidgetTitleDescriptionHeight(const char *title,
                                      const char *description);
void WidgetTitleDescription(const char *title, const char *description,
                            ImVec2 pos);
void WidgetTitleDescriptionItem(const char *str_id,
                                const char *description = nullptr);
float GetSliderRadius(ImVec2 size);
float GetSliderTrackXOffset(ImVec2 size);
float GetSliderTrackWidth(ImVec2 size);
float GetSliderValueForMousePos(ImVec2 mouse, ImVec2 pos, ImVec2 size);
void DrawSlider(float v, bool hovered, ImVec2 pos, ImVec2 size);
void DrawToggle(bool enabled, bool hovered, ImVec2 pos, ImVec2 size);
bool Toggle(const char *str_id, bool *v, const char *description = nullptr);
void Slider(const char *str_id, float *v, const char *description = nullptr);
void FilePicker(const char *str_id, const char *current_path,
                const SDL_DialogFileFilter *filters, int nfilters, bool dir,
                std::function<void(const char *new_path)> on_select);
void DrawComboChevron();
void PrepareComboTitleDescription(const char *label, const char *description,
                                  float combo_size_ratio);
bool ChevronCombo(const char *label, int *current_item,
                  bool (*items_getter)(void *, int, const char **), void *data,
                  int items_count, const char *description = NULL);
bool ChevronCombo(const char* label, int* current_item, const char* items_separated_by_zeros, const char *description = NULL);
void Hyperlink(const char *text, const char *url);
void HelpMarker(const char* desc);
void Logo();
