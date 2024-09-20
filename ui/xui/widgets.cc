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
#include "widgets.hh"
#include "misc.hh"
#include "font-manager.hh"
#include "viewport-manager.hh"
#include "ui/xemu-os-utils.h"
#include "gl-helpers.hh"

void Separator()
{
    // XXX: IDK. Maybe there's a better way to draw a separator ( ImGui::Separator() ) that cuts through window
    //      padding... Just grab the draw list and draw the line with outer clip rect

    float thickness = 1 * g_viewport_mgr.m_scale;

    ImGuiWindow *window = ImGui::GetCurrentWindow();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImRect window_rect = window->Rect();
    ImVec2 size = ImVec2(window_rect.GetWidth(), thickness);

    ImVec2 p0(window_rect.Min.x, ImGui::GetCursorScreenPos().y);
    ImVec2 p1(p0.x + size.x, p0.y);
    ImGui::PushClipRect(window_rect.Min, window_rect.Max, false);
    draw_list->AddLine(p0, p1, ImGui::GetColorU32(ImGuiCol_Separator), thickness);
    ImGui::PopClipRect();
    ImGui::Dummy(size);
}

void SectionTitle(const char *title)
{
    ImGui::Spacing();
    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    ImGui::Text("%s", title);
    ImGui::PopFont();
    Separator();
}

float GetWidgetTitleDescriptionHeight(const char *title,
                                      const char *description)
{
    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    float h = ImGui::GetFrameHeight();
    ImGui::PopFont();

    if (description) {
        ImGuiStyle &style = ImGui::GetStyle();
        h += style.ItemInnerSpacing.y;
        ImGui::PushFont(g_font_mgr.m_default_font);
        h += ImGui::GetTextLineHeight();
        ImGui::PopFont();
    }

    return h;
}

void WidgetTitleDescription(const char *title, const char *description,
                            ImVec2 pos)
{
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImGuiStyle &style = ImGui::GetStyle();

    ImVec2 text_pos = pos;
    text_pos.x += style.FramePadding.x;
    text_pos.y += style.FramePadding.y;

    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    float title_height = ImGui::GetTextLineHeight();
    draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), title);
    ImGui::PopFont();

    if (description) {
        text_pos.y += title_height + style.ItemInnerSpacing.y;

        ImGui::PushFont(g_font_mgr.m_default_font);
        draw_list->AddText(text_pos, ImGui::GetColorU32(ImVec4(0.94f, 0.94f, 0.94f, 0.70f)), description);
        ImGui::PopFont();
    }
}

void WidgetTitleDescriptionItem(const char *str_id, const char *description)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetColumnWidth(),
                GetWidgetTitleDescriptionHeight(str_id, description));
    WidgetTitleDescription(str_id, description, p);

    // XXX: Internal API
    ImRect bb(p, ImVec2(p.x + size.x, p.y + size.y));
    ImGui::ItemSize(size, 0.0f);
    ImGui::ItemAdd(bb, 0);
}

float GetSliderRadius(ImVec2 size)
{
    return size.y * 0.5;
}

float GetSliderTrackXOffset(ImVec2 size)
{
    return GetSliderRadius(size);
}

float GetSliderTrackWidth(ImVec2 size)
{
    return size.x - GetSliderRadius(size) * 2;
}

float GetSliderValueForMousePos(ImVec2 mouse, ImVec2 pos, ImVec2 size)
{
    return (mouse.x - pos.x - GetSliderTrackXOffset(size)) /
           GetSliderTrackWidth(size);
}

void DrawSlider(float v, bool hovered, ImVec2 pos, ImVec2 size)
{
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    float radius = GetSliderRadius(size);
    float rounding = size.y * 0.25;
    float slot_half_height = size.y * 0.125;
    const bool circular_grab = false;

    ImU32 bg = hovered ? ImGui::GetColorU32(ImGuiCol_FrameBgActive)
                       : ImGui::GetColorU32(ImGuiCol_CheckMark);

    ImVec2 pmid(pos.x + radius + v*(size.x - radius*2), pos.y + size.y / 2);
    ImVec2 smin(pos.x + rounding, pmid.y - slot_half_height);
    ImVec2 smax(pmid.x, pmid.y + slot_half_height);
    draw_list->AddRectFilled(smin, smax, bg, rounding);

    bg = hovered ? ImGui::GetColorU32(ImGuiCol_FrameBgHovered)
                 : ImGui::GetColorU32(ImGuiCol_FrameBg);

    smin.x = pmid.x;
    smax.x = pos.x + size.x - rounding;
    draw_list->AddRectFilled(smin, smax, bg, rounding);

    if (circular_grab) {
       draw_list->AddCircleFilled(pmid, radius * 0.8, ImGui::GetColorU32(ImGuiCol_SliderGrab));
    } else {
        ImVec2 offs(radius*0.8, radius*0.8);
        draw_list->AddRectFilled(pmid - offs, pmid + offs, ImGui::GetColorU32(ImGuiCol_SliderGrab), rounding);
    }
}

void DrawToggle(bool enabled, bool hovered, ImVec2 pos, ImVec2 size)
{
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    float radius = size.y * 0.5;
    float rounding = size.y * 0.25;
    float slot_half_height = size.y * 0.5;
    const bool circular_grab = false;

    ImU32 bg = hovered ? ImGui::GetColorU32(enabled ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBgHovered)
                       : ImGui::GetColorU32(enabled ? ImGuiCol_CheckMark : ImGuiCol_FrameBg);

    ImVec2 pmid(pos.x + radius + (int)enabled * (size.x - radius * 2), pos.y + size.y / 2);
    ImVec2 smin(pos.x, pmid.y - slot_half_height);
    ImVec2 smax(pos.x + size.x, pmid.y + slot_half_height);
    draw_list->AddRectFilled(smin, smax, bg, rounding);

    if (circular_grab) {
        draw_list->AddCircleFilled(pmid, radius * 0.8, ImGui::GetColorU32(ImGuiCol_SliderGrab));
    } else {
        ImVec2 offs(radius*0.8, radius*0.8);
        draw_list->AddRectFilled(pmid - offs, pmid + offs, ImGui::GetColorU32(ImGuiCol_SliderGrab), rounding);
    }
}

bool Toggle(const char *str_id, bool *v, const char *description)
{
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);

    ImGuiStyle &style = ImGui::GetStyle();

    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    float title_height = ImGui::GetTextLineHeight();
    ImGui::PopFont();

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 bb(ImGui::GetColumnWidth(),
              GetWidgetTitleDescriptionHeight(str_id, description));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
    ImGui::PushID(str_id);
    bool status = ImGui::Button("###toggle_button", bb);
    if (status) {
        *v = !*v;
    }
    ImGui::PopID();
    ImGui::PopStyleVar();
    const ImVec2 p_min = ImGui::GetItemRectMin();
    const ImVec2 p_max = ImGui::GetItemRectMax();

    WidgetTitleDescription(str_id, description, p);

    float toggle_height = title_height * 0.9;
    ImVec2 toggle_size(toggle_height * 1.75, toggle_height);
    ImVec2 toggle_pos(p_max.x - toggle_size.x - style.FramePadding.x,
                      p_min.y + (title_height - toggle_size.y)/2 + style.FramePadding.y);
    DrawToggle(*v, ImGui::IsItemHovered(), toggle_pos, toggle_size);

    ImGui::PopStyleColor();

    return status;
}

void Slider(const char *str_id, float *v, const char *description)
{
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);

    ImGuiStyle &style = ImGui::GetStyle();
    ImGuiWindow *window = ImGui::GetCurrentWindow();

    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    float title_height = ImGui::GetTextLineHeight();
    ImGui::PopFont();

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetColumnWidth(),
                GetWidgetTitleDescriptionHeight(str_id, description));
    WidgetTitleDescription(str_id, description, p);

    // XXX: Internal API
    ImVec2 wpos = ImGui::GetCursorPos();
    ImRect bb(p, ImVec2(p.x + size.x, p.y + size.y));
    ImGui::ItemSize(size, 0.0f);
    ImGui::ItemAdd(bb, 0);
    ImGui::SetItemAllowOverlap();
    ImGui::SameLine(0, 0);

    ImVec2 slider_size(size.x * 0.4, title_height * 0.9);
    ImVec2 slider_pos(bb.Max.x - slider_size.x - style.FramePadding.x,
                      p.y + (title_height - slider_size.y)/2 + style.FramePadding.y);

    ImGui::SetCursorPos(ImVec2(wpos.x + size.x - slider_size.x - style.FramePadding.x,
                               wpos.y));

    ImGui::InvisibleButton("###slider", slider_size, 0);


    if (ImGui::IsItemHovered()) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadRStickLeft)) {
                *v -= 0.05;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadRStickRight)) {
                *v += 0.05;
        }

        if (
            ImGui::IsKeyDown(ImGuiKey_LeftArrow) ||
            ImGui::IsKeyDown(ImGuiKey_GamepadDpadLeft) ||
            ImGui::IsKeyDown(ImGuiKey_GamepadLStickLeft) ||
            ImGui::IsKeyDown(ImGuiKey_GamepadRStickLeft) ||
            ImGui::IsKeyDown(ImGuiKey_RightArrow) ||
            ImGui::IsKeyDown(ImGuiKey_GamepadDpadRight) ||
            ImGui::IsKeyDown(ImGuiKey_GamepadLStickRight) ||
            ImGui::IsKeyDown(ImGuiKey_GamepadRStickRight)
            ) {
            ImGui::NavMoveRequestCancel();
        }
    }

    if (ImGui::IsItemActive()) {
        ImVec2 mouse = ImGui::GetMousePos();
        *v = GetSliderValueForMousePos(mouse, slider_pos, slider_size);
    }
    *v = fmax(0, fmin(*v, 1));
    DrawSlider(*v, ImGui::IsItemHovered() || ImGui::IsItemActive(), slider_pos,
               slider_size);

    ImVec2 slider_max = ImVec2(slider_pos.x + slider_size.x, slider_pos.y + slider_size.y);
    ImGui::RenderNavHighlight(ImRect(slider_pos, slider_max), window->GetID("###slider"));

    ImGui::PopStyleColor();
}

bool FilePicker(const char *str_id, const char **buf, const char *filters,
                bool dir)
{
    bool changed = false;

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 cursor = ImGui::GetCursorPos();
    const char *desc = strlen(*buf) ? *buf : "(None Selected)";
    ImVec2 bb(ImGui::GetColumnWidth(),
              GetWidgetTitleDescriptionHeight(str_id, desc));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
    ImGui::PushID(str_id);
    bool status =
        ImGui::ButtonEx("###file_button", bb, ImGuiButtonFlags_AllowOverlap);
    ImGui::SetItemAllowOverlap();
    if (status) {
        int flags = NOC_FILE_DIALOG_OPEN;
        if (dir) flags |= NOC_FILE_DIALOG_DIR;
        const char *new_path =
            PausedFileOpen(flags, filters, *buf, NULL);
        if (new_path) {
            free((void*)*buf);
            *buf = strdup(new_path);
            desc = *buf;
            changed = true;
        }
    }
    ImGui::PopID();
    ImGui::PopStyleVar();

    WidgetTitleDescription(str_id, desc, p);

    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();

    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    ImGui::PushFont(g_font_mgr.m_menu_font);
    const char *icon = dir ? ICON_FA_FOLDER : ICON_FA_FILE;
    ImVec2 ts_icon = ImGui::CalcTextSize(icon);
    ImVec2 icon_pos = ImVec2(p1.x - style.FramePadding.x - ts_icon.x,
                              p0.y + (p1.y - p0.y - ts_icon.y) / 2);
    draw_list->AddText(icon_pos, ImGui::GetColorU32(ImGuiCol_Text), icon);

    ImVec2 ts_clear_icon = ImGui::CalcTextSize(ICON_FA_XMARK);
    ts_clear_icon.x += 2 * style.FramePadding.x;
    ImVec2 clear_icon_pos = ImVec2(cursor.x + bb.x - ts_icon.x - ts_clear_icon.x, cursor.y);

    auto prev_pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(clear_icon_pos);

    char *clear_button_id = g_strdup_printf("%s_clear", str_id);
    ImGui::PushID(clear_button_id);

    bool clear = ImGui::Button(ICON_FA_XMARK, ImVec2(ts_clear_icon.x, bb.y));
    if (clear) {
        free((void*)*buf);
        *buf = strdup("");
        changed = true;
    }

    ImGui::PopID();
    g_free(clear_button_id);

    ImGui::SetCursorPos(prev_pos);

    ImGui::PopFont();

    ImGui::PopStyleColor();

    return changed;
}

void DrawComboChevron()
{
    ImGui::PushFont(g_font_mgr.m_menu_font);
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    const char *icon = ICON_FA_CHEVRON_DOWN;
    ImVec2 ts_icon = ImGui::CalcTextSize(icon);
    ImGuiStyle &style = ImGui::GetStyle();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    draw_list->AddText(ImVec2(p1.x - style.FramePadding.x - ts_icon.x,
                              p0.y + (p1.y - p0.y - ts_icon.y) / 2),
                       ImGui::GetColorU32(ImGuiCol_Text), icon);
    ImGui::PopFont();
}

void PrepareComboTitleDescription(const char *label, const char *description,
                                  float combo_size_ratio)
{
    float width = ImGui::GetColumnWidth();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size(width, GetWidgetTitleDescriptionHeight(label, description));
    WidgetTitleDescription(label, description, pos);

    ImVec2 wpos = ImGui::GetCursorPos();
    ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, 0.0f);
    ImGui::ItemAdd(bb, 0);
    ImGui::SetItemAllowOverlap();
    ImGui::SameLine(0, 0);
    float combo_width = width * combo_size_ratio;
    ImGui::SetCursorPos(ImVec2(wpos.x + width - combo_width, wpos.y));
}

bool ChevronCombo(const char *label, int *current_item,
                  bool (*items_getter)(void *, int, const char **), void *data,
                  int items_count, const char *description)
{
    bool value_changed = false;
    float combo_width = ImGui::GetColumnWidth();
    if (*label != '#') {
        float combo_size_ratio = 0.4;
        PrepareComboTitleDescription(label, description, combo_size_ratio);
        combo_width *= combo_size_ratio;
    }

    ImGuiContext& g = *GImGui;
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(1, 0));

    // Call the getter to obtain the preview string which is a parameter to BeginCombo()
    const char* preview_value = NULL;
    if (*current_item >= 0 && *current_item < items_count)
        items_getter(data, *current_item, &preview_value);

    ImGui::SetNextItemWidth(combo_width);
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::PushID(label);
    if (ImGui::BeginCombo("###chevron_combo", preview_value, ImGuiComboFlags_NoArrowButton)) {
        // Display items
        // FIXME-OPT: Use clipper (but we need to disable it on the appearing frame to make sure our call to SetItemDefaultFocus() is processed)
        for (int i = 0; i < items_count; i++)
        {
            ImGui::PushID(i);
            const bool item_selected = (i == *current_item);
            const char* item_text;
            if (!items_getter(data, i, &item_text))
                item_text = "*Unknown item*";
            if (ImGui::Selectable(item_text, item_selected))
            {
                value_changed = true;
                *current_item = i;
            }
            if (item_selected)
                ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }

        ImGui::EndCombo();

        if (value_changed)
            ImGui::MarkItemEdited(g.LastItemData.ID);
    }
    ImGui::PopID();
    ImGui::PopFont();
    DrawComboChevron();
    ImGui::PopStyleVar();
    return value_changed;
}

// Getter for the old Combo() API: "item1\0item2\0item3\0"
static bool Items_SingleStringGetter(void* data, int idx, const char** out_text)
{
    // FIXME-OPT: we could pre-compute the indices to fasten this. But only 1 active combo means the waste is limited.
    const char* items_separated_by_zeros = (const char*)data;
    int items_count = 0;
    const char* p = items_separated_by_zeros;
    while (*p)
    {
        if (idx == items_count)
            break;
        p += strlen(p) + 1;
        items_count++;
    }
    if (!*p)
        return false;
    if (out_text)
        *out_text = p;
    return true;
}

// Combo box helper allowing to pass all items in a single string literal holding multiple zero-terminated items "item1\0item2\0"
bool ChevronCombo(const char* label, int* current_item, const char* items_separated_by_zeros, const char *description)
{
    int items_count = 0;
    const char* p = items_separated_by_zeros;       // FIXME-OPT: Avoid computing this, or at least only when combo is open
    while (*p)
    {
        p += strlen(p) + 1;
        items_count++;
    }
    bool value_changed = ChevronCombo(
        label, current_item, Items_SingleStringGetter,
        (void *)items_separated_by_zeros, items_count, description);
    return value_changed;
}

void Hyperlink(const char *text, const char *url)
{
    ImColor col;
    ImGui::Text("%s", text);
    if (ImGui::IsItemHovered()) {
        col = IM_COL32_WHITE;
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    } else {
        col = ImColor(127, 127, 127, 255);
    }

    ImVec2 max = ImGui::GetItemRectMax();
    ImVec2 min = ImGui::GetItemRectMin();
    min.x -= 1 * g_viewport_mgr.m_scale;
    min.y = max.y;
    max.x -= 1 * g_viewport_mgr.m_scale;
    ImGui::GetWindowDrawList()->AddLine(min, max, col, 1.0 * g_viewport_mgr.m_scale);

    if (ImGui::IsItemClicked()) {
        xemu_open_web_browser(url);
    }
}

void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void Logo()
{
    ImGui::SetCursorPosY(ImGui::GetCursorPosY()-25*g_viewport_mgr.m_scale);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-256*g_viewport_mgr.m_scale)/2);

    static uint32_t time_start = 0;
    static uint32_t offset = 0;
    uint32_t now = SDL_GetTicks();

    if (ImGui::IsWindowAppearing()) {
        time_start = now;
    }

    logo_fbo->Target();
    ImTextureID id = (ImTextureID)(intptr_t)logo_fbo->Texture();
    float t_w = 256.0;
    float t_h = 256.0;
    float x_off = 0;
    ImVec2 pos = ImGui::GetCursorPos();
    ImGui::Image(id,
        ImVec2((t_w-x_off)*g_viewport_mgr.m_scale, t_h*g_viewport_mgr.m_scale),
        ImVec2(x_off/t_w, t_h/t_h),
        ImVec2(t_w/t_w, 0));
    ImVec2 size = ImGui::GetItemRectSize();
    ImGui::SetCursorPos(pos);
    ImGui::InvisibleButton("###empty", ImVec2(size.x, size.y*0.8));
    if (ImGui::IsItemClicked()) {
        time_start = now;
        offset = 0;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 item_min = ImGui::GetItemRectMin();
        ImVec2 mouse = ImGui::GetMousePos();
        time_start = now;
        offset = 1500 * fmin(fmax(0, (mouse.x - item_min.x) / (size.x)), 1);
    }

    RenderLogo(now - time_start + offset);
    logo_fbo->Restore();
}
