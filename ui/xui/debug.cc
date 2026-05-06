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
#include <inttypes.h>
#include <algorithm>
#include <numeric>
#include <vector>

#include "debug.hh"
#include "common.hh"
#include "misc.hh"
#include "font-manager.hh"
#include "viewport-manager.hh"
#include "actions.hh"

#define MAX_VOICES 256

DebugApuWindow::DebugApuWindow() : m_is_open(false)
{
}

void DebugApuWindow::Draw()
{
    if (!m_is_open)
        return;

    ImGui::SetNextWindowContentSize(ImVec2(600.0f*g_viewport_mgr.m_scale, 0.0f));
    if (!ImGui::Begin("Audio Debug", &m_is_open,
                      ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    const struct McpxApuDebug *dbg = mcpx_apu_get_debug_info();


    ImGui::Columns(2, "", false);
    ImGui::SetColumnWidth(0, 360*g_viewport_mgr.m_scale);

    int now = SDL_GetTicks() % 1000;
    float t = now/1000.0f;
    float freq = 1;
    float v = fabs(sin(M_PI*t*freq));
    float c_active = mix(0.4, 0.97, v);
    float c_inactive = 0.2f;

    int voice_monitor = -1;
    int voice_info = -1;
    int voice_mute = -1;

    ImGui::PushFont(g_font_mgr.m_fixed_width_font);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2*g_viewport_mgr.m_scale, 2*g_viewport_mgr.m_scale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4*g_viewport_mgr.m_scale, 4*g_viewport_mgr.m_scale));
    for (int i = 0; i < MAX_VOICES; i++) {
        if (i % 16) {
            ImGui::SameLine();
        }

        float c, s, h;
        h = 0.6;
        if (dbg->vp.v[i].active) {
            if (dbg->vp.v[i].paused) {
                c = c_inactive;
                s = 0.4;
            } else {
                c = c_active;
                s = 0.7;
            }
            if (mcpx_apu_debug_is_muted(i)) {
                h = 1.0;
            }
        } else {
            c = c_inactive;
            s = 0;
        }

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(h, s, c));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(h, s, 0.8));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(h, 0.8f, 1.0));
        char buf[12];
        snprintf(buf, sizeof(buf), "%02x", i);
        ImGui::Button(buf);
        if (/*dbg->vp.v[i].active &&*/ ImGui::IsItemHovered()) {
            voice_monitor = i;
            voice_info = i;
        }
        if (ImGui::IsItemClicked(1)) {
            voice_mute = i;
        }
        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }
    ImGui::PopStyleVar(3);
    ImGui::PopFont();

    if (voice_info >= 0) {
        const struct McpxApuDebugVoice *voice = &dbg->vp.v[voice_info];
        ImGui::BeginTooltip();
        bool is_paused = voice->paused;
        ImGui::Text("Voice 0x%x/%d %s", voice_info, voice_info, is_paused ? "(Paused)" : "");
        ImGui::SameLine();
        ImGui::Text(voice->stereo ? "Stereo" : "Mono");

        ImGui::Separator();
        ImGui::PushFont(g_font_mgr.m_fixed_width_font);

        const char *noyes[2] = { "NO", "YES" };
        ImGui::Text("Stream: %-3s Loop: %-3s Persist: %-3s Multipass: %-3s "
                    "Linked: %-3s",
                    noyes[voice->stream], noyes[voice->loop],
                    noyes[voice->persist], noyes[voice->multipass],
                    noyes[voice->linked]);

        const char *cs[4] = { "1 byte", "2 bytes", "ADPCM", "4 bytes" };
        const char *ss[4] = {
            "Unsigned 8b PCM",
            "Signed 16b PCM",
            "Signed 24b PCM",
            "Signed 32b PCM"
        };

        assert(voice->container_size < 4);
        assert(voice->sample_size < 4);
        const char *spb_or_bin_label = "Samples per Block";
        unsigned int spb_or_bin = voice->samples_per_block;
        if (voice->multipass) {
            spb_or_bin_label = "Multipass Bin";
            spb_or_bin = voice->multipass_bin;
        }
        ImGui::Text("Container Size: %s, Sample Size: %s, %s: %d",
                    cs[voice->container_size], ss[voice->sample_size],
                    spb_or_bin_label, spb_or_bin);
        ImGui::Text("Rate: %f (%d Hz)", voice->rate, (int)(48000.0/voice->rate));
        ImGui::Text("EBO=%d CBO=%d LBO=%d BA=%x",
            voice->ebo, voice->cbo, voice->lbo, voice->ba);
        ImGui::Text("Mix: ");
        for (int i = 0; i < 8; i++) {
            if (i == 4) ImGui::Text("     ");
            ImGui::SameLine();
            char buf[64];
            if (voice->vol[i] == 0xFFF) {
                snprintf(buf, sizeof(buf),
                    "Bin %2d (MUTE) ", voice->bin[i]);
            } else {
                snprintf(buf, sizeof(buf),
                    "Bin %2d (-%.3f) ", voice->bin[i],
                    (float)((voice->vol[i] >> 6) & 0x3f) +
                    (float)((voice->vol[i] >> 0) & 0x3f) / 64.0);
            }
            ImGui::Text("%-17s", buf);
        }

        int mon = mcpx_apu_debug_get_monitor();
        if (mon == MCPX_APU_DEBUG_MON_VP) {
            if (voice->multipass_dst_voice != 0xFFFF) {
                ImGui::Text("Multipass Dest Voice: 0x%02x",
                            voice->multipass_dst_voice);
            }
            if (voice->multipass) {
                ImGui::Text("Multipass Src Voices:");
                int n = 0;

                for (int i = 0; i < MAX_VOICES; i++) {
                    if (dbg->vp.v[i].multipass_dst_voice == voice_info) {
                        if (n > 0 && ((n & 7) == 0)) {
                            ImGui::Text("                     ");
                        }
                        ImGui::SameLine();
                        ImGui::Text("0x%02x", i);
                        n++;
                    }
                }
            }
        }

        ImGui::PopFont();
        ImGui::EndTooltip();
    }

    if (voice_monitor >= 0) {
        mcpx_apu_debug_isolate_voice(voice_monitor);
    } else {
        mcpx_apu_debug_clear_isolations();
    }
    if (voice_mute >= 0) {
        mcpx_apu_debug_toggle_mute(voice_mute);
    }

    ImGui::NextColumn();

    static int mon = 0;
    mon = (int)mcpx_apu_debug_get_monitor();
    if (ImGui::Combo("Monitor", &mon, "AC97\0VP Only\0GP Only\0EP Only\0GP/EP if enabled\0")) {
        mcpx_apu_debug_set_monitor((McpxApuDebugMonitorPoint)mon);
    }

    static bool gp_realtime;
    gp_realtime = dbg->gp_realtime;
    if (ImGui::Checkbox("GP Realtime\n", &gp_realtime)) {
        mcpx_apu_debug_set_gp_realtime_enabled(gp_realtime);
    }

    static bool ep_realtime;
    ep_realtime = dbg->ep_realtime;
    if (ImGui::Checkbox("EP Realtime\n", &ep_realtime)) {
        mcpx_apu_debug_set_ep_realtime_enabled(ep_realtime);
    }

    ImGui::Checkbox("HRTF Filtering\n", &g_config.audio.hrtf);

    ImGui::PushFont(g_font_mgr.m_fixed_width_font);

    bool color = (dbg->utilization > 0.9);
    if (color) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,0,0,1));
    ImGui::Text("Utilization: %.2f%%", (dbg->utilization*100));
    if (color) ImGui::PopStyleColor();

    ImGui::Text("Frames:      %04d", dbg->frames_processed);
    if (ImGui::TreeNode("Throttle")) {
        ImGui::Text("Deviation: %" PRId64 "/%" PRId64 "/%" PRId64 " us",
                    dbg->throttle.deviation.min_us,
                    dbg->throttle.deviation.avg_us,
                    dbg->throttle.deviation.max_us);
        ImGui::Text("Latency:   %.1f/%.1f/%.1f ms",
                    dbg->throttle.latency.min_ms,
                    dbg->throttle.latency.avg_ms,
                    dbg->throttle.latency.max_ms);
        float high = dbg->throttle.latency.high_ms;
        if (high > 0) {
            float scale = fmax(high, dbg->throttle.latency.max_ms);
            float h = ImGui::GetFrameHeight() * 0.25f;

            ImGui::Text("Queue/Pacing:");
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            float low_x = pos.x + w * (dbg->throttle.latency.low_ms / scale);
            float high_x = pos.x + w * (high / scale);
            float min_x = pos.x + w * (dbg->throttle.latency.min_ms / scale);
            float avg_x = pos.x + w * (dbg->throttle.latency.avg_ms / scale);
            float max_x = pos.x + w * (dbg->throttle.latency.max_ms / scale);
            const ImU32 col_bg   = IM_COL32(40, 40, 40, 255);
            const ImU32 col_band = IM_COL32(60, 120, 60, 255);
            const ImU32 col_avg  = IM_COL32(100, 220, 100, 255);
            const ImU32 col_low  = IM_COL32(255, 255, 0, 180);
            const ImU32 col_high = IM_COL32(255, 80, 80, 180);

            ImDrawList *dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), col_bg);
            dl->AddRectFilled(ImVec2(min_x, pos.y), ImVec2(max_x, pos.y + h),
                              col_band);
            dl->AddLine(ImVec2(low_x, pos.y), ImVec2(low_x, pos.y + h),
                        col_low, 1.0f);
            dl->AddLine(ImVec2(avg_x, pos.y), ImVec2(avg_x, pos.y + h),
                        col_avg, 2.0f);
            dl->AddLine(ImVec2(high_x, pos.y), ImVec2(high_x, pos.y + h),
                        col_high, 1.0f);
            ImGui::Dummy(ImVec2(w, h));

            pos.y += h * 1.25;

            const ImU32 col_speedup = col_low;
            const ImU32 col_ok      = col_band;
            const ImU32 col_backoff = col_high;
            float sp = dbg->throttle.pacing.speedup;
            float ok = dbg->throttle.pacing.ok;
            float bo = dbg->throttle.pacing.backoff;
            float sp_x = pos.x + w * sp;
            float ok_x = sp_x + w * ok;
            dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), col_bg);
            if (sp > 0) {
                dl->AddRectFilled(pos, ImVec2(sp_x, pos.y + h), col_speedup);
            }
            if (ok > 0) {
                dl->AddRectFilled(ImVec2(sp_x, pos.y), ImVec2(ok_x, pos.y + h),
                                  col_ok);
            }
            if (bo > 0) {
                dl->AddRectFilled(ImVec2(ok_x, pos.y), ImVec2(pos.x + w, pos.y + h),
                                  col_backoff);
            }
            ImGui::Dummy(ImVec2(w, h));
        }
        ImGui::TreePop();
    }
    ImGui::Text("VP:          %4d us", dbg->vp.total_worker_time_us);
    if (ImGui::TreeNode("VP Workers")) {
        ImGui::Text(" W: #  us");
        ImGui::SameLine();
        ImGui::Text(" W: #  us");
        for (int i = 0; i < dbg->vp.num_workers; i++) {
            if (i % 2) ImGui::SameLine();
            ImGui::Text("%2d:%2d %3d", i, dbg->vp.workers[i].num_voices,
                        dbg->vp.workers[i].time_us);
        }
        ImGui::TreePop();
    }
    ImGui::Text("GP Cycles:   %04d", dbg->gp.cycles);
    ImGui::Text("EP Cycles:   %04d", dbg->ep.cycles);

    ImGui::PopFont();
    ImGui::Columns(1);
    ImGui::End();
}

// Utility structure for realtime plot
struct ScrollingBuffer {
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;
    ScrollingBuffer() {
        MaxSize = 2000;
        Offset  = 0;
        Data.reserve(MaxSize);
    }
    void AddPoint(float x, float y) {
        if (Data.size() < MaxSize)
            Data.push_back(ImVec2(x,y));
        else {
            Data[Offset] = ImVec2(x,y);
            Offset =  (Offset + 1) % MaxSize;
        }
    }
    void Erase() {
        if (Data.size() > 0) {
            Data.shrink(0);
            Offset  = 0;
        }
    }
};

DebugVideoWindow::DebugVideoWindow()
{
    m_is_open = false;
    m_transparent = false;
    m_position_restored = false;
    m_resize_init_complete = false;
    m_prev_scale = g_viewport_mgr.m_scale;

    m_hovered_counter_index = -1;
    m_legend_sort_mode = LegendSortMode::DEFAULT;

    m_legend_names.reserve(NV2A_PROF__COUNT + NUM_EXTRA_COUNTERS);
    m_counter_index_to_value.reserve(NV2A_PROF__COUNT + NUM_EXTRA_COUNTERS);
    for (int i = 0; i < NV2A_PROF__COUNT + NUM_EXTRA_COUNTERS; ++i) {
        m_counter_visible[i] = true;
        if (i < NV2A_PROF__COUNT) {
            m_legend_names.push_back(nv2a_profile_get_counter_name(i));
        } else if (i == INDEX_MSPF) {
            m_legend_names.push_back("MSPF");
        }
        m_counter_index_to_value.push_back({ i, 0 });
    }

    m_legend_indices_sorted_az.resize(m_legend_names.size());
    std::iota(m_legend_indices_sorted_az.begin(), m_legend_indices_sorted_az.end(), 0);
    std::sort(m_legend_indices_sorted_az.begin(), m_legend_indices_sorted_az.end(), [&](size_t a, size_t b) {
        return g_ascii_strcasecmp(m_legend_names[a], m_legend_names[b]) < 0;
    });
}

static int LastFrameHistoryIndex()
{
    return (g_nv2a_stats.frame_ptr + NV2A_PROF_NUM_FRAMES - 1) %
           NV2A_PROF_NUM_FRAMES;
}

void DebugVideoWindow::Draw()
{
    if (!m_is_open)
        return;

    if (!m_position_restored) {
        ImGui::SetNextWindowPos(ImVec2(g_config.display.debug.video.x_pos,
                                       g_config.display.debug.video.y_pos),
                                ImGuiCond_Once, ImVec2(0, 0));
        m_transparent = g_config.display.debug.video.transparency;
        m_position_restored = true;
    }

    float alpha = m_transparent ? 0.2 : 1.0;
    PushWindowTransparencySettings(m_transparent, 0.2);

    if (!m_resize_init_complete || (g_viewport_mgr.m_scale != m_prev_scale)) {
        ImGui::SetNextWindowSize(ImVec2(
            g_config.display.debug.video.x_winsize * g_viewport_mgr.m_scale,
            g_config.display.debug.video.y_winsize * g_viewport_mgr.m_scale));
        m_resize_init_complete = true;
    }
    m_prev_scale = g_viewport_mgr.m_scale;

    if (ImGui::Begin("Video Debug", &m_is_open)) {
        bool running = runstate_is_running();
        if (ImGui::Button(running ? "Pause emulation" : "Resume")) {
            ActionTogglePause();
        }

        double x_start, x_end;
        static ImPlotAxisFlags rt_axis = ImPlotAxisFlags_NoTickLabels;
        ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(5,5));
        ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
        static ScrollingBuffer fps;
        static float t = 0;
        if (running) {
            t += ImGui::GetIO().DeltaTime;
            fps.AddPoint(t, g_nv2a_stats.increment_fps);
        }
        x_start = t - 10.0;
        x_end = t;

        float plot_width = 0.5 * (ImGui::GetWindowSize().x -
                                  2 * ImGui::GetStyle().WindowPadding.x -
                                  ImGui::GetStyle().ItemSpacing.x);

        ImGui::SetNextWindowBgAlpha(alpha);
        if (ImPlot::BeginPlot("##ScrollingFPS", ImVec2(plot_width,75*g_viewport_mgr.m_scale))) {
            ImPlot::SetupAxes(NULL, NULL, rt_axis, rt_axis | ImPlotAxisFlags_Lock);
            ImPlot::SetupAxesLimits(x_start, x_end, 0, 65, ImPlotCond_Always);
            if (fps.Data.size() > 0) {
                ImPlot::PlotShaded("##fps", &fps.Data[0].x, &fps.Data[0].y, fps.Data.size(), 0, 0, fps.Offset, 2 * sizeof(float));
                ImPlot::PlotLine("##fps", &fps.Data[0].x, &fps.Data[0].y, fps.Data.size(), 0, fps.Offset, 2 * sizeof(float));
            }
            ImPlot::Annotation(x_start, 65, ImPlot::GetLastItemColor(), ImVec2(0,0), true, "FPS: %d", g_nv2a_stats.increment_fps);
            ImPlot::EndPlot();
        }

        ImGui::SameLine();

        x_end = g_nv2a_stats.frame_count;
        x_start = x_end - NV2A_PROF_NUM_FRAMES;

        ImPlot::PushStyleColor(ImPlotCol_Line, ImPlot::GetColormapColor(1));
        ImGui::SetNextWindowBgAlpha(alpha);
        if (ImPlot::BeginPlot("##ScrollingMSPF", ImVec2(plot_width,75*g_viewport_mgr.m_scale))) {
            ImPlot::SetupAxes(NULL, NULL, rt_axis, rt_axis | ImPlotAxisFlags_Lock);
            ImPlot::SetupAxesLimits(x_start, x_end, 0, 100, ImPlotCond_Always);
            ImPlot::PlotShaded("##mspf", &g_nv2a_stats.frame_history[0].mspf, NV2A_PROF_NUM_FRAMES, 0, 1, x_start, 0, g_nv2a_stats.frame_ptr, sizeof(g_nv2a_stats.frame_working));
            ImPlot::PlotLine("##mspf", &g_nv2a_stats.frame_history[0].mspf, NV2A_PROF_NUM_FRAMES, 1, x_start, 0, g_nv2a_stats.frame_ptr, sizeof(g_nv2a_stats.frame_working));
            ImPlot::Annotation(x_start, 100, ImPlot::GetLastItemColor(), ImVec2(0,0), true, "MSPF: %d", g_nv2a_stats.frame_history[LastFrameHistoryIndex()].mspf);
            ImPlot::EndPlot();
        }
        ImPlot::PopStyleColor();

        ImGui::SetNextItemOpen(g_config.display.debug.video.advanced_tree_state,
                               ImGuiCond_Once);
        g_config.display.debug.video.advanced_tree_state =
            ImGui::TreeNode("Advanced");

        if (g_config.display.debug.video.advanced_tree_state) {
            ImGui::SetNextWindowBgAlpha(alpha);
            DrawAdvancedContent();
            ImGui::TreePop();
        }

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(2)) {
            m_transparent = !m_transparent;
        }

        ImPlot::PopStyleVar(2);

        ImVec2 debug_window_pos = ImGui::GetWindowPos();
        g_config.display.debug.video.x_pos = debug_window_pos.x;
        g_config.display.debug.video.y_pos = debug_window_pos.y;

        ImVec2 debug_window_size = ImGui::GetWindowSize();
        g_config.display.debug.video.x_winsize =
            debug_window_size.x / g_viewport_mgr.m_scale;
        g_config.display.debug.video.y_winsize =
            debug_window_size.y / g_viewport_mgr.m_scale;
        g_config.display.debug.video.transparency = m_transparent;
    }
    ImGui::End();
    ImGui::PopStyleColor(5);
}

// Custom axis scale: log10(1 + x). Maps 0 -> 0 instead of 0 -> -inf so that
// counters that are zero for a frame draw a line to the bottom of the graph
// rather than creating a gap that looks like missing data.
static double AdvPlotForward(double v, void *)
{
    return std::log10(1.0 + std::max(0.0, v));
}

static double AdvPlotInverse(double v, void *)
{
    return std::pow(10.0, v) - 1.0;
}

static int AdvPlotFormatter(double value, char *buf, int size, void *)
{
    if (value < 0.5) {
        return snprintf(buf, size, "0");
    }
    return snprintf(buf, size, "%.4g", value);
}

int DebugVideoWindow::FindHoveredPlotLineIndex()
{
    ImPlotPoint mouse_pos = ImPlot::GetPlotMousePos();
    int x_idx = std::round(mouse_pos.x);

    if (x_idx < 0 || x_idx >= NV2A_PROF_NUM_FRAMES || mouse_pos.y < 0) {
        return -1;
    }

    int data_idx = (g_nv2a_stats.frame_ptr + x_idx) % NV2A_PROF_NUM_FRAMES;
    float best_dist = 0.1f;
    int best_item = -1;
    // Use the same log1p metric as the plot so hover distances are consistent.
    float log_mouse_y = std::log10(1.0f + std::max(0.0f, (float)mouse_pos.y));

    for (int i = 0; i < NV2A_PROF__COUNT + NUM_EXTRA_COUNTERS; ++i) {
        if (!m_counter_visible[i]) {
            continue;
        }

        float val;
        if (i < NV2A_PROF__COUNT) {
            val = g_nv2a_stats.frame_history[data_idx].counters[i];
        } else {
            val = g_nv2a_stats.frame_history[data_idx].mspf;
        }

        if (val < 0) {
            continue;
        }

        float dist = std::fabs(std::log10(1.0f + val) - log_mouse_y);
        if (dist < best_dist) {
            best_dist = dist;
            best_item = i;
        }
    }

    return best_item;
}

void DebugVideoWindow::DrawAdvancedContent()
{
    int new_hovered_counter_index = -1;
    if (ImGui::BeginTable("##AdvancedCounters", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Legend", ImGuiTableColumnFlags_WidthFixed,
                                200 * g_viewport_mgr.m_scale);
        ImGui::TableSetupColumn("Plot", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        static const char *sort_modes[] = { "Default", "A-Z", "Z-A", "Value" };
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Sort");
        ImGui::SameLine();
        float button_width =
            ImGui::CalcTextSize("0").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetNextItemWidth(
            -(button_width * 2 + ImGui::GetStyle().ItemSpacing.x * 2));
        int sort_mode = static_cast<int>(m_legend_sort_mode);
        if (ImGui::Combo("##SortMode", &sort_mode, sort_modes,
                         IM_ARRAYSIZE(sort_modes))) {
            m_legend_sort_mode = static_cast<LegendSortMode>(sort_mode);
        }
        ImGui::SameLine();
        if (ImGui::Button("1")) {
            for (int i = 0; i < NV2A_PROF__COUNT + NUM_EXTRA_COUNTERS; ++i) {
                m_counter_visible[i] = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("0")) {
            for (int i = 0; i < NV2A_PROF__COUNT + NUM_EXTRA_COUNTERS; ++i) {
                m_counter_visible[i] = false;
            }
        }

        ImGui::BeginChild("##AdvancedLegend", ImVec2(0, -1), false,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1, 1, 1, 0.1f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1, 1, 1, 0.2f));

        auto DrawLegendItem = [this, &new_hovered_counter_index](int idx,
                                                                 int val) {
            bool enabled = m_counter_visible[idx];
            bool hovered = (m_hovered_counter_index == idx);

            ImVec4 text_col;
            if (enabled) {
                text_col = hovered ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) :
                                     ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            } else {
                text_col = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            }

            ImGui::PushFont(g_font_mgr.m_fixed_width_font);

            float swatch_size = ImGui::GetTextLineHeight();
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            ImVec2 p = ImGui::GetCursorScreenPos();

            if (enabled) {
                ImVec4 swatch_col = ImPlot::GetColormapColor(idx);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    p, ImVec2(p.x + swatch_size, p.y + swatch_size),
                    ImColor(swatch_col));
            }

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + swatch_size +
                                 spacing);

            ImGui::PushStyleColor(ImGuiCol_Text, text_col);
            char buf[256];
            snprintf(buf, sizeof(buf), "%s: %d###Lgnd%d", m_legend_names[idx],
                     val, idx);

            ImGui::Selectable(buf, &m_counter_visible[idx]);
            if (ImGui::IsItemHovered()) {
                new_hovered_counter_index = idx;
            }

            if (hovered && enabled) {
                ImVec2 min = ImGui::GetItemRectMin();
                ImVec2 max = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(min.x, max.y - 1), ImVec2(max.x, max.y - 1),
                    ImGui::GetColorU32(ImGuiCol_TitleBgActive), 2.0f);
            }

            ImGui::PopStyleColor();
            ImGui::PopFont();
        };

        auto GetCurrentVal = [](int idx) {
            if (idx < NV2A_PROF__COUNT) {
                return nv2a_profile_get_counter_value(idx);
            } else if (idx == INDEX_MSPF) {
                return g_nv2a_stats.frame_history[LastFrameHistoryIndex()].mspf;
            }
            return 0;
        };

        if (m_legend_sort_mode == LegendSortMode::ALPHABETICAL_AZ) {
            for (const auto &index : m_legend_indices_sorted_az) {
                DrawLegendItem(index, GetCurrentVal(index));
            }
        } else if (m_legend_sort_mode == LegendSortMode::ALPHABETICAL_ZA) {
            for (auto it = m_legend_indices_sorted_az.rbegin();
                 it != m_legend_indices_sorted_az.rend(); ++it) {
                DrawLegendItem(*it, GetCurrentVal(*it));
            }
        } else if (m_legend_sort_mode == LegendSortMode::VALUE) {
            for (int i = 0; i < NV2A_PROF__COUNT + NUM_EXTRA_COUNTERS; ++i) {
                m_counter_index_to_value[i] = { i, GetCurrentVal(i) };
            }

            std::sort(
                m_counter_index_to_value.begin(),
                m_counter_index_to_value.end(),
                [](const auto &a, const auto &b) { return a.value > b.value; });

            for (int i = 0; i < NV2A_PROF__COUNT + NUM_EXTRA_COUNTERS; ++i) {
                const auto &entry = m_counter_index_to_value[i];
                DrawLegendItem(entry.index, entry.value);
            }
        } else {
            DrawLegendItem(
                INDEX_MSPF,
                g_nv2a_stats.frame_history[LastFrameHistoryIndex()].mspf);
            for (int i = 0; i < NV2A_PROF__COUNT; ++i) {
                DrawLegendItem(i, GetCurrentVal(i));
            }
        }

        ImGui::Dummy(ImVec2(0, 10 * g_viewport_mgr.m_scale));

        ImGui::PopStyleColor(3);
        ImGui::EndChild();

        ImGui::TableNextColumn();

        ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0, 0.1f));
        if (ImPlot::BeginPlot("##AdvancedCountersPlot", ImVec2(-1, -1),
                              ImPlotFlags_NoLegend)) {
            ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_None,
                              ImPlotAxisFlags_AutoFit);
            // log10(1+x) scale: zero maps to 0 rather than -inf, so counters
            // that hit zero draw to the bottom instead of leaving a gap.
            ImPlot::SetupAxisScale(ImAxis_Y1, AdvPlotForward, AdvPlotInverse);
            ImPlot::SetupAxisFormat(ImAxis_Y1, AdvPlotFormatter);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, NV2A_PROF_NUM_FRAMES);

            if (ImPlot::IsPlotHovered()) {
                new_hovered_counter_index = FindHoveredPlotLineIndex();
            }

            int effective_hover = (new_hovered_counter_index != -1) ?
                                      new_hovered_counter_index :
                                      m_hovered_counter_index;

            for (int i = 0; i < NV2A_PROF__COUNT + NUM_EXTRA_COUNTERS; ++i) {
                if (!m_counter_visible[i]) {
                    continue;
                }
                ImGui::PushID(i);
                float weight = (effective_hover == i) ? 4.0f : 1.5f;
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, weight);
                ImPlot::PushStyleColor(ImPlotCol_Line,
                                       ImPlot::GetColormapColor(i));
                if (i < NV2A_PROF__COUNT) {
                    ImPlot::PlotLine(
                        "##counter", &g_nv2a_stats.frame_history[0].counters[i],
                        NV2A_PROF_NUM_FRAMES, 1, 0, 0, g_nv2a_stats.frame_ptr,
                        sizeof(g_nv2a_stats.frame_working));
                } else {
                    ImPlot::PlotLine(
                        "##counter", &g_nv2a_stats.frame_history[0].mspf,
                        NV2A_PROF_NUM_FRAMES, 1, 0, 0, g_nv2a_stats.frame_ptr,
                        sizeof(g_nv2a_stats.frame_working));
                }
                ImPlot::PopStyleColor();
                ImPlot::PopStyleVar();
                ImGui::PopID();
            }

            ImPlot::EndPlot();
        }
        ImPlot::PopStyleVar();
        ImGui::EndTable();
    }

    m_hovered_counter_index = new_hovered_counter_index;
}

DebugApuWindow apu_window;
DebugVideoWindow video_window;
