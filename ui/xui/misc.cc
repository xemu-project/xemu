//
// xemu User Interface
//
// Copyright (C) 2026 Matt Borgerson
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
#include "misc.hh"
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_platform_defines.h>
#include <filesystem>
#include <string>
#include <memory>

#if defined(SDL_PLATFORM_LINUX)
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <thread>
#endif

static void RunOnMainThread(std::function<void()> &&func)
{
    auto p = new std::function<void()>(std::move(func));
    if (!SDL_RunOnMainThread(
            [](void *userdata) {
                std::unique_ptr<std::function<void()>> f(
                    static_cast<std::function<void()> *>(userdata));
                xemu_main_loop_lock();
                (*f)();
                xemu_main_loop_unlock();
            },
            p, false)) {
        delete p;
    }
}

#if defined(SDL_PLATFORM_LINUX)
// Check if running as root or with elevated file capabilities (setcap)
static bool ShouldBypassXDGPortal()
{
    // Force reset process dumpability to handle privilege drop state transitions cleanly
    prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);

    // 1. Check for root or effective root execution
    if (getuid() == 0 || geteuid() == 0) {
        return true;
    }

    // 2. Check CapEff (Effective), CapPrm (Permitted), and CapInh (Inheritable)
    // If ANY capabilities are active or were dropped incorrectly,
    // fallback to Zenity to avoid D-Bus auth mismatch faults.
    try {
        std::ifstream file("/proc/self/status");
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.rfind("CapEff:", 0) == 0 ||
                    line.rfind("CapPrm:", 0) == 0 ||
                    line.rfind("CapInh:", 0) == 0) {

                    std::string cap_str = line.substr(7);
                    // Drop internal allocations/exceptions by switching to strtoull
                    char* endptr = nullptr;
                    unsigned long long cap_val = std::strtoull(cap_str.c_str(), &endptr, 16);
                    if (cap_val != 0) {
                       return true; // Capabilities are present; bypass portal
                    }
                }
            }
        }
    } catch (...) {}
    return false;
}

// Safer alternative to popen using explicit fork/execvp to completely eliminate
// shell injection exploits and handle process termination gracefully.
static void RunZenityFallbackAsync(const std::vector<std::string>& extra_args,
                                   std::string title, std::string default_location,
                                   FileDialogCallback callback)
{
    // Capture state and run inside a worker thread to keep the emulator rendering loop ticking
    std::thread([extra_args, title = std::move(title), loc = std::move(default_location), callback = std::move(callback)]() {
        int pipe_fd[2];
        if (pipe(pipe_fd) < 0) return;

        pid_t pid = fork();
        if (pid < 0) {
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            return;
        }

        if (pid == 0) { // Child Process Execution Area
            close(pipe_fd[0]);
            dup2(pipe_fd[1], STDOUT_FILENO); // Redirect stdout to our reading pipe
            close(pipe_fd[1]);

            // Prevent Zenity from spawning terminal warning noise
            setenv("G_MESSAGES_DEBUG", "", 1);

            // Construct array vector arguments securely for execvp
            std::vector<std::string> args = {"zenity", "--file-selection", "--title=" + title};
            for (const auto& arg : extra_args) {
                args.push_back(arg);
            }
            if (!loc.empty()) {
                args.push_back("--filename=" + loc);
            }

            // Convert array structures to standard null-terminated pointer arrays matching C layouts
            std::vector<char*> c_args;
            for (const auto& s : args) {
                c_args.push_back(const_cast<char*>(s.c_str()));
            }
            c_args.push_back(nullptr);

            execvp("zenity", c_args.data());
            _exit(1); // Exit child immediately if zenity binary isn't installed
        }
        // Parent Thread Execution Area
        close(pipe_fd[1]);
        std::string result = "";
        char read_buf[512];
        ssize_t bytes_read;

        while ((bytes_read = read(pipe_fd[0], read_buf, sizeof(read_buf) - 1)) > 0) {
            read_buf[bytes_read] = '\0';
            result += read_buf;
        }
        close(pipe_fd[0]);

        // Reap child process to ensure we do not leave trailing zombie allocations in memory
        int status;
        waitpid(pid, &status, 0);

        // Sanitize string output
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0 && !result.empty()) {
            RunOnMainThread([callback = std::move(callback), result = std::move(result)]() {
                callback(result.c_str());
            });
        }
    }).detach();
}
#endif

static void FileDialogCallbackWrapper(void *userdata,
                                      const char *const *filelist, int filter)
{
    std::unique_ptr<FileDialogCallback> callback(
        static_cast<FileDialogCallback *>(userdata));
    if (filelist && filelist[0]) {
        std::string path = filelist[0];
        RunOnMainThread([callback = std::move(*callback),
                         path = std::move(path)]() { callback(path.c_str()); });
    }
}

// Workaround SDL3 default_location handling:
// - Linux: only supports folder paths, not file paths as documented
// - Windows/macOS: directories need trailing separator for proper display
static std::string NormalizeDefaultLocation(const char *default_location)
{
    if (!default_location || !*default_location) {
        return {};
    }

    try {
        std::filesystem::path path(default_location);
#if defined(SDL_PLATFORM_LINUX)
        if (std::filesystem::is_regular_file(path)) {
            return path.parent_path().string();
        }
#elif defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_MACOS)
        if (std::filesystem::is_directory(path)) {
            return (path / "").string();
        }
        // Prevent a crash in SDL3 file dialog
        if (!std::filesystem::exists(path)) {
            return {};
        }
#endif
    } catch (...) {
        // Fall through to return original path
    }
    return default_location;
}

void ShowOpenFileDialog(const SDL_DialogFileFilter *filters, int nfilters,
                        const char *default_location,
                        FileDialogCallback callback)
{
    std::string normalized = NormalizeDefaultLocation(default_location);

#if defined(SDL_PLATFORM_LINUX)
    if (ShouldBypassXDGPortal()) {
        RunZenityFallbackAsync({}, "Open File", normalized, std::move(callback));
        return;
    }
#endif

    auto cb = std::make_unique<FileDialogCallback>(std::move(callback));
    SDL_ShowOpenFileDialog(FileDialogCallbackWrapper, cb.release(), xemu_get_window(),
                           filters, nfilters,
                           normalized.empty() ? nullptr : normalized.c_str(),
                           false);
}

void ShowSaveFileDialog(const SDL_DialogFileFilter *filters, int nfilters,
                        const char *default_location,
                        FileDialogCallback callback)
{
    std::string normalized = NormalizeDefaultLocation(default_location);

#if defined(SDL_PLATFORM_LINUX)
    if (ShouldBypassXDGPortal()) {
        RunZenityFallbackAsync({"--save", "--confirm-overwrite"}, "Save File", normalized, std::move(callback));
        return;
    }
#endif

    auto cb = std::make_unique<FileDialogCallback>(std::move(callback));
    SDL_ShowSaveFileDialog(FileDialogCallbackWrapper, cb.release(), xemu_get_window(),
                           filters, nfilters,
                           normalized.empty() ? nullptr : normalized.c_str());
}

void ShowOpenFolderDialog(const char *default_location,
                          FileDialogCallback callback)
{
    std::string normalized = NormalizeDefaultLocation(default_location);

#if defined(SDL_PLATFORM_LINUX)
    if (ShouldBypassXDGPortal()) {
        RunZenityFallbackAsync({"--directory"}, "Select Folder", normalized, std::move(callback));
        return;
    }
#endif

    auto cb = std::make_unique<FileDialogCallback>(std::move(callback));
    SDL_ShowOpenFolderDialog(FileDialogCallbackWrapper, cb.release(),
                             xemu_get_window(),
                             normalized.empty() ? nullptr : normalized.c_str(), false);
}
