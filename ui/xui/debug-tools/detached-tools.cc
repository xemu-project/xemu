//
// xemu Detached Debug Tool Windows
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "detached-tools.hh"

#include "../common.hh"
#include "cheat-engine.hh"
#include "current-game.hh"
#include "hdd-directory.hh"
#include "memory-tools.hh"

#include <cstdio>

namespace {

struct DetachedToolWindow {
    const char *title = nullptr;
    int default_width = 900;
    int default_height = 700;
    int min_width = 640;
    int min_height = 480;
    bool *open = nullptr;
    void (*draw)() = nullptr;

    SDL_Window *window = nullptr;
    SDL_GLContext gl_context = nullptr;
    ImGuiContext *imgui_context = nullptr;
    bool shown = false;
    bool frame_ready = false;
};

SDL_Window *g_main_window = nullptr;
SDL_GLContext g_main_gl_context = nullptr;
ImGuiContext *g_main_imgui_context = nullptr;
SDL_Cursor *g_detached_default_cursor = nullptr;

void DrawCheatEngine()
{
    cheat_engine_window.Draw(true);
}

void DrawMemoryTools()
{
    memory_tools_window.Draw(true);
}

void DrawCurrentGame()
{
    current_game_manager.Draw(true);
}

void DrawHddDirectory()
{
    hdd_directory_window.Draw(true);
}

DetachedToolWindow g_cheat_tool = {
    "xemu - RAW Cheat Engine",
    980, 760,
    720, 520,
    &cheat_engine_window.is_open,
    DrawCheatEngine,
};

DetachedToolWindow g_memory_tool = {
    "xemu - Memory Viewer / Search / x86 Debugger",
    1320, 860,
    900, 620,
    &memory_tools_window.is_open,
    DrawMemoryTools,
};

DetachedToolWindow g_current_game_tool = {
    "xemu - Current Game",
    900, 620,
    700, 480,
    &current_game_manager.is_open,
    DrawCurrentGame,
};

DetachedToolWindow g_hdd_tool = {
    "xemu - Xbox HDD Directory",
    1100, 760,
    760, 520,
    &hdd_directory_window.is_open,
    DrawHddDirectory,
};

DetachedToolWindow *g_tools[] = {
    &g_cheat_tool,
    &g_memory_tool,
    &g_current_game_tool,
    &g_hdd_tool,
};

void EnsureDesktopCursor(DetachedToolWindow &tool)
{
    if (!tool.window) {
        return;
    }
    SDL_SetWindowRelativeMouseMode(tool.window, false);
    SDL_SetWindowMouseGrab(tool.window, false);
    if (g_detached_default_cursor) {
        SDL_SetCursor(g_detached_default_cursor);
    }
    SDL_ShowCursor();
}

void RestoreMainContexts()
{
    if (g_main_imgui_context) {
        ImGui::SetCurrentContext(g_main_imgui_context);
    }
    if (g_main_window && g_main_gl_context) {
        SDL_GL_MakeCurrent(g_main_window, g_main_gl_context);
    }
}

bool CreateToolWindow(DetachedToolWindow &tool)
{
    if (tool.window && tool.gl_context && tool.imgui_context) {
        return true;
    }

    RestoreMainContexts();

    SDL_WindowFlags flags = (SDL_WindowFlags)(
        SDL_WINDOW_OPENGL |
        SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_HIGH_PIXEL_DENSITY |
        SDL_WINDOW_HIDDEN);

    tool.window = SDL_CreateWindow(tool.title, tool.default_width,
                                   tool.default_height, flags);
    if (!tool.window) {
        std::fprintf(stderr, "Failed to create detached tool window '%s': %s\n",
                     tool.title, SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(tool.window, tool.min_width, tool.min_height);
    SDL_SetWindowRelativeMouseMode(tool.window, false);
    SDL_SetWindowMouseGrab(tool.window, false);

    // The NV2A offscreen renderer may leave SDL's share-with-current-context
    // attribute enabled. Detached tools intentionally own independent GL
    // resources/font textures, so temporarily disable sharing while creating
    // their contexts and then restore the process setting.
    int previous_share = 0;
    SDL_GL_GetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, &previous_share);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
    tool.gl_context = SDL_GL_CreateContext(tool.window);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, previous_share);
    if (!tool.gl_context) {
        std::fprintf(stderr, "Failed to create OpenGL context for '%s': %s\n",
                     tool.title, SDL_GetError());
        SDL_DestroyWindow(tool.window);
        tool.window = nullptr;
        RestoreMainContexts();
        return false;
    }

    SDL_GL_MakeCurrent(tool.window, tool.gl_context);
    SDL_GL_SetSwapInterval(0);

    tool.imgui_context = ImGui::CreateContext();
    ImGui::SetCurrentContext(tool.imgui_context);

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Detached contexts do not use gamepad navigation or change the process-
    // global SDL mouse cursor. This avoids the shared-resource portions of the
    // SDL backend that are not useful for these desktop debugging windows.
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();

    // Match the main xemu style without sharing its font atlas. Sharing the
    // atlas would require shared OpenGL texture objects between the contexts.
    if (g_main_imgui_context) {
        ImGuiStyle main_style;
        ImGui::SetCurrentContext(g_main_imgui_context);
        main_style = ImGui::GetStyle();
        ImGui::SetCurrentContext(tool.imgui_context);
        ImGui::GetStyle() = main_style;
        ImGui::GetStyle().WindowRounding = 0.0f;
    } else {
        ImGui::StyleColorsDark();
    }

    if (!ImGui_ImplSDL3_InitForOpenGL(tool.window, tool.gl_context)) {
        std::fprintf(stderr, "Failed to initialize ImGui SDL backend for '%s'\n",
                     tool.title);
        ImGui::DestroyContext(tool.imgui_context);
        tool.imgui_context = nullptr;
        SDL_GL_DestroyContext(tool.gl_context);
        tool.gl_context = nullptr;
        SDL_DestroyWindow(tool.window);
        tool.window = nullptr;
        RestoreMainContexts();
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 150")) {
        std::fprintf(stderr, "Failed to initialize ImGui OpenGL backend for '%s'\n",
                     tool.title);
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(tool.imgui_context);
        tool.imgui_context = nullptr;
        SDL_GL_DestroyContext(tool.gl_context);
        tool.gl_context = nullptr;
        SDL_DestroyWindow(tool.window);
        tool.window = nullptr;
        RestoreMainContexts();
        return false;
    }

    RestoreMainContexts();
    return true;
}

void DestroyToolWindow(DetachedToolWindow &tool)
{
    if (tool.imgui_context) {
        ImGui::SetCurrentContext(tool.imgui_context);
        if (tool.window && tool.gl_context) {
            SDL_GL_MakeCurrent(tool.window, tool.gl_context);
        }
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(tool.imgui_context);
        tool.imgui_context = nullptr;
    }
    if (tool.gl_context) {
        SDL_GL_DestroyContext(tool.gl_context);
        tool.gl_context = nullptr;
    }
    if (tool.window) {
        SDL_DestroyWindow(tool.window);
        tool.window = nullptr;
    }
    tool.shown = false;
    tool.frame_ready = false;
}

void BuildToolFrame(DetachedToolWindow &tool)
{
    tool.frame_ready = false;

    if (!tool.open || !*tool.open) {
        if (tool.window && tool.shown) {
            SDL_HideWindow(tool.window);
            tool.shown = false;
        }
        return;
    }

    if (!CreateToolWindow(tool)) {
        *tool.open = false;
        return;
    }

    if (!tool.shown) {
        SDL_ShowWindow(tool.window);
        SDL_RaiseWindow(tool.window);
        tool.shown = true;
    }

    const SDL_WindowFlags focus_flags = SDL_GetWindowFlags(tool.window);
    if (focus_flags & SDL_WINDOW_MOUSE_FOCUS) {
        EnsureDesktopCursor(tool);
    }

    ImGui::SetCurrentContext(tool.imgui_context);
    SDL_GL_MakeCurrent(tool.window, tool.gl_context);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    tool.draw();

    ImGui::Render();
    tool.frame_ready = true;
}

void RenderToolFrame(DetachedToolWindow &tool)
{
    if (!tool.frame_ready || !tool.window || !tool.gl_context ||
        !tool.imgui_context || !tool.open || !*tool.open) {
        return;
    }

    const SDL_WindowFlags flags = SDL_GetWindowFlags(tool.window);
    if ((flags & SDL_WINDOW_HIDDEN) || (flags & SDL_WINDOW_MINIMIZED)) {
        tool.frame_ready = false;
        return;
    }

    ImGui::SetCurrentContext(tool.imgui_context);
    SDL_GL_MakeCurrent(tool.window, tool.gl_context);

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(tool.window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.10f, 0.10f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(tool.window);
    tool.frame_ready = false;
}

} // namespace

void detached_tools_init(SDL_Window *main_window, void *main_gl_context)
{
    g_main_window = main_window;
    g_main_gl_context = (SDL_GLContext)main_gl_context;
    g_main_imgui_context = ImGui::GetCurrentContext();
    if (!g_detached_default_cursor) {
        g_detached_default_cursor =
            SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    }
}

void detached_tools_cleanup()
{
    for (DetachedToolWindow *tool : g_tools) {
        DestroyToolWindow(*tool);
    }
    if (g_detached_default_cursor) {
        SDL_DestroyCursor(g_detached_default_cursor);
        g_detached_default_cursor = nullptr;
    }
    RestoreMainContexts();
    g_main_window = nullptr;
    g_main_gl_context = nullptr;
    g_main_imgui_context = nullptr;
}

bool detached_tools_process_sdl_event(SDL_Event *event)
{
    if (!event) {
        return false;
    }

    SDL_Window *event_window = SDL_GetWindowFromEvent(event);
    for (DetachedToolWindow *tool : g_tools) {
        if (!tool->window || !tool->imgui_context ||
            event_window != tool->window) {
            continue;
        }

        // A detached tool owns this event. Do not feed the same SDL event to
        // the main xemu ImGui backend as well: the main backend can otherwise
        // apply its captured/hidden cursor state to the desktop tool window.
        ImGui::SetCurrentContext(tool->imgui_context);
        ImGui_ImplSDL3_ProcessEvent(event);

        switch (event->type) {
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
            // xemu's game window may intentionally hide the process-global SDL
            // cursor while the guest owns the mouse. Force a normal desktop
            // pointer whenever input belongs to one of our detached tools.
            EnsureDesktopCursor(*tool);
            break;
        default:
            break;
        }

        if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            if (tool->open) {
                *tool->open = false;
            }
            SDL_HideWindow(tool->window);
            tool->shown = false;
            tool->frame_ready = false;
        }

        if (g_main_imgui_context) {
            ImGui::SetCurrentContext(g_main_imgui_context);
        }
        return true;
    }

    if (g_main_imgui_context) {
        ImGui::SetCurrentContext(g_main_imgui_context);
    }
    return false;
}

bool detached_tools_owns_window_id(SDL_WindowID window_id)
{
    if (!window_id) {
        return false;
    }

    for (DetachedToolWindow *tool : g_tools) {
        if (tool->window && SDL_GetWindowID(tool->window) == window_id) {
            return true;
        }
    }
    return false;
}

void detached_tools_build_frames()
{
    for (DetachedToolWindow *tool : g_tools) {
        BuildToolFrame(*tool);
    }
    RestoreMainContexts();
}

void detached_tools_render_frames()
{
    for (DetachedToolWindow *tool : g_tools) {
        RenderToolFrame(*tool);
    }
    RestoreMainContexts();
}
