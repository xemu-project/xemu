//
// xemu User Interface - Cross-Platform Auto-Update
//
// Copyright (C) 2020-2025 Matt Borgerson
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
#include "common.hh"
#include "update.hh"
#include "viewport-manager.hh"

extern "C" {
#include "qemu/osdep.h"
#include "qemu/http.h"
#include "xemu-version.h"
#include <SDL_filesystem.h>
}

#include <stdio.h>
#include <stdlib.h>

// Cross-platform includes
#if defined(_WIN32)
#include <windows.h>
#include <process.h>
#include "util/miniz/miniz.h"
#elif defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#if defined(__linux__) && defined(HAVE_LIBARCHIVE)
#include <archive.h>
#include <archive_entry.h>
#else // macOS or Linux without libarchive
#include "util/miniz/miniz.h"
#endif
#endif

#define DPRINTF(fmt, ...) fprintf(stderr, "[AUTO-UPDATE] " fmt, ##__VA_ARGS__);

// Forward declarations
static bool extract_zip(const unsigned char *data, size_t data_len, const char *base_path);
#if defined(__linux__) && defined(HAVE_LIBARCHIVE)
static bool extract_tar_gz(const unsigned char *data, size_t data_len, const char *base_path);
#endif
#if defined(__linux__)
static bool extract_appimage(const unsigned char *data, size_t data_len, const char *base_path);
#endif

// Platform-specific configuration
const char *version_url = "https://raw.githubusercontent.com/xemu-project/xemu/ppa-snapshot/XEMU_VERSION";

// Update format detection
typedef enum {
    UPDATE_FORMAT_ZIP,
    UPDATE_FORMAT_TAR_GZ,
    UPDATE_FORMAT_APPIMAGE
} UpdateFormat;

#if defined(_WIN32)
    #if defined(__x86_64__)
        const char *download_url = "https://github.com/xemu-project/xemu/releases/latest/download/xemu-win-x86_64-release.zip";
        const char *executable_name = "xemu.exe";
        const char *backup_name = "xemu-previous.exe";
        const UpdateFormat update_format = UPDATE_FORMAT_ZIP;
    #elif defined(__aarch64__)
        const char *download_url = "https://github.com/xemu-project/xemu/releases/latest/download/xemu-win-aarch64-release.zip";
        const char *executable_name = "xemu.exe";
        const char *backup_name = "xemu-previous.exe";
        const UpdateFormat update_format = UPDATE_FORMAT_ZIP;
    #else
        #error Unknown Windows architecture
    #endif
#elif defined(__linux__)
    // Check if running as AppImage
    #if defined(__x86_64__)
        const char *tar_download_url = "https://github.com/xemu-project/xemu/releases/latest/download/xemu-linux-x86_64-release.tar.gz";
        const char *appimage_download_url = "https://github.com/xemu-project/xemu/releases/latest/download/xemu-x86_64.AppImage";
        const char *executable_name = "xemu";
        const char *backup_name = "xemu-previous";
    #elif defined(__aarch64__)
        const char *tar_download_url = "https://github.com/xemu-project/xemu/releases/latest/download/xemu-linux-aarch64-release.tar.gz";
        const char *appimage_download_url = "https://github.com/xemu-project/xemu/releases/latest/download/xemu-aarch64.AppImage";
        const char *executable_name = "xemu";
        const char *backup_name = "xemu-previous";
    #else
        #error Unknown Linux architecture
    #endif
    // Will be set dynamically based on current execution context
    static const char *download_url = nullptr;
    static UpdateFormat update_format = UPDATE_FORMAT_ZIP; // Default fallback
#elif defined(__APPLE__)
    #if defined(__x86_64__)
        const char *download_url = "https://github.com/xemu-project/xemu/releases/latest/download/xemu-macos-x86_64-release.zip";
        const char *executable_name = "xemu";
        const char *backup_name = "xemu-previous";
        const UpdateFormat update_format = UPDATE_FORMAT_ZIP;
    #elif defined(__aarch64__) || defined(__arm64__)
        const char *download_url = "https://github.com/xemu-project/xemu/releases/latest/download/xemu-macos-aarch64-release.zip";
        const char *executable_name = "xemu";
        const char *backup_name = "xemu-previous";
        const UpdateFormat update_format = UPDATE_FORMAT_ZIP;
    #else
        #error Unknown macOS architecture
    #endif
#else
    #error Unsupported platform for auto-update
#endif

AutoUpdateWindow update_window;

// AppImage detection and configuration
#if defined(__linux__)
static bool is_running_as_appimage() {
    // Check if APPIMAGE environment variable is set
    const char *appimage_path = getenv("APPIMAGE");
    return appimage_path != nullptr && strlen(appimage_path) > 0;
}

static void configure_linux_update_urls() {
    if (is_running_as_appimage()) {
        download_url = appimage_download_url;
        update_format = UPDATE_FORMAT_APPIMAGE;
        DPRINTF("Detected AppImage execution, using AppImage updates\n");
    } else {
#if defined(HAVE_LIBARCHIVE)
        download_url = tar_download_url;
        update_format = UPDATE_FORMAT_TAR_GZ;
        DPRINTF("Detected regular installation, using tar.gz updates\n");
#else
        // Fallback to ZIP format if libarchive not available
        DPRINTF("libarchive not available, auto-updater disabled for regular installation\n");
        download_url = nullptr;
        update_format = UPDATE_FORMAT_ZIP;
#endif
    }
}
#endif

// Cross-platform utility functions
static bool create_directories(const char *path) {
    if (!path || strlen(path) == 0) {
        return false;
    }
    
#if defined(_WIN32)
    char *path_copy = g_strdup(path);
    char *dir = dirname(path_copy);
    BOOL result = CreateDirectoryA(dir, NULL);
    DWORD error = GetLastError();
    bool success = result || (error == ERROR_ALREADY_EXISTS);
    if (!success) {
        DPRINTF("Failed to create directory %s: error %lu\n", dir, error);
    }
    g_free(path_copy);
    return success;
#else
    char *path_copy = g_strdup(path);
    char *dir = dirname(path_copy);
    int result = mkdir(dir, 0755);
    bool success = (result == 0) || (errno == EEXIST);
    if (!success) {
        DPRINTF("Failed to create directory %s: %s\n", dir, strerror(errno));
    }
    g_free(path_copy);
    return success;
#endif
}

static bool move_file(const char *src, const char *dst) {
    if (!src || !dst) {
        return false;
    }
    
#if defined(_WIN32)
    BOOL result = MoveFileExA(src, dst, MOVEFILE_REPLACE_EXISTING);
    if (!result) {
        DPRINTF("Failed to move file %s to %s: error %lu\n", src, dst, GetLastError());
    }
    return result != 0;
#else
    int result = rename(src, dst);
    if (result != 0) {
        DPRINTF("Failed to move file %s to %s: %s\n", src, dst, strerror(errno));
    }
    return result == 0;
#endif
}

static bool set_executable_permission(const char *path) {
    if (!path) {
        return false;
    }
    
#if defined(_WIN32)
    return true; // Windows doesn't use Unix permissions
#else
    int result = chmod(path, 0755);
    if (result != 0) {
        DPRINTF("Failed to set executable permission on %s: %s\n", path, strerror(errno));
    }
    return result == 0;
#endif
}

// Cross-platform archive extraction
#if defined(__linux__) && defined(HAVE_LIBARCHIVE)
static bool extract_tar_gz(const unsigned char *data, size_t data_len, const char *base_path) {
    struct archive *a = archive_read_new();
    struct archive *ext = archive_write_disk_new();
    struct archive_entry *entry;
    int r;
    bool success = true;

    if (!a || !ext) {
        DPRINTF("Failed to create archive objects\n");
        if (a) archive_read_free(a);
        if (ext) archive_write_free(ext);
        return false;
    }

    archive_read_support_filter_gzip(a);
    archive_read_support_format_tar(a);
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS);

    r = archive_read_open_memory(a, data, data_len);
    if (r != ARCHIVE_OK) {
        DPRINTF("archive_read_open_memory failed: %s\n", archive_error_string(a));
        archive_read_free(a);
        archive_write_free(ext);
        return false;
    }

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *filename = archive_entry_pathname(entry);
        if (!filename) {
            DPRINTF("Invalid filename in archive entry\n");
            continue;
        }
        
        char *dst_path = g_strdup_printf("%s%s", base_path, filename);
        
        DPRINTF("extracting %s to %s\n", filename, dst_path);
        
        // Handle executable backup
        if (!strcmp(filename, executable_name)) {
            char *backup_path = g_strdup_printf("%s%s", base_path, backup_name);
            if (!move_file(dst_path, backup_path)) {
                DPRINTF("Warning: Failed to backup current executable\n");
            }
            g_free(backup_path);
        }
        
        if (!create_directories(dst_path)) {
            DPRINTF("Failed to create directories for %s\n", dst_path);
            g_free(dst_path);
            success = false;
            continue;
        }
        
        archive_entry_set_pathname(entry, dst_path);
        
        r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK) {
            DPRINTF("archive_write_header failed: %s\n", archive_error_string(ext));
            g_free(dst_path);
            success = false;
            continue;
        }
        
        if (archive_entry_size(entry) > 0) {
            const void *buff;
            size_t size;
            la_int64_t offset;
            
            while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
                if (archive_write_data_block(ext, buff, size, offset) != ARCHIVE_OK) {
                    DPRINTF("archive_write_data_block failed\n");
                    success = false;
                    break;
                }
            }
        }
        
        archive_write_finish_entry(ext);
        
        // Set executable permission if needed
        if (!strcmp(filename, executable_name)) {
            if (!set_executable_permission(dst_path)) {
                DPRINTF("Warning: Failed to set executable permission\n");
            }
        }
        
        g_free(dst_path);
    }

    archive_read_free(a);
    archive_write_free(ext);
    return success;
}
#endif

#if defined(__linux__)
static bool extract_appimage(const unsigned char *data, size_t data_len, const char *base_path) {
    // For AppImage, we just need to save the file and set permissions
    const char *appimage_path = getenv("APPIMAGE");
    if (!appimage_path) {
        DPRINTF("APPIMAGE environment variable not set\n");
        return false;
    }

    // Create backup of current AppImage
    char *backup_path = g_strdup_printf("%s.previous", appimage_path);
    if (!move_file(appimage_path, backup_path)) {
        DPRINTF("Failed to backup current AppImage\n");
        g_free(backup_path);
        return false;
    }
    g_free(backup_path);

    // Write new AppImage
    FILE *f = fopen(appimage_path, "wb");
    if (!f) {
        DPRINTF("Failed to open %s for writing: %s\n", appimage_path, strerror(errno));
        return false;
    }

    size_t written = fwrite(data, 1, data_len, f);
    fclose(f);

    if (written != data_len) {
        DPRINTF("Failed to write complete AppImage file (wrote %zu of %zu bytes)\n", written, data_len);
        return false;
    }

    // Set executable permission
    if (!set_executable_permission(appimage_path)) {
        DPRINTF("Failed to set executable permission on AppImage\n");
        return false;
    }

    DPRINTF("Successfully updated AppImage: %s\n", appimage_path);
    return true;
}
#endif

static bool extract_zip(const unsigned char *data, size_t data_len, const char *base_path) {
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    bool success = true;
    
    if (!mz_zip_reader_init_mem(&zip, data, data_len, 0)) {
        DPRINTF("mz_zip_reader_init_mem failed\n");
        return false;
    }

    mz_uint num_files = mz_zip_reader_get_num_files(&zip);
    DPRINTF("Extracting %u files from ZIP archive\n", num_files);
    
    for (mz_uint file_idx = 0; file_idx < num_files; file_idx++) {
        mz_zip_archive_file_stat fstat;
        if (!mz_zip_reader_file_stat(&zip, file_idx, &fstat)) {
            DPRINTF("mz_zip_reader_file_stat failed for file #%d\n", file_idx);
            success = false;
            continue;
        }

        if (fstat.m_filename[strlen(fstat.m_filename)-1] == '/') {
            // Create directory
            char *dir_path = g_strdup_printf("%s%s", base_path, fstat.m_filename);
            if (!create_directories(dir_path)) {
                DPRINTF("Failed to create directory %s\n", dir_path);
                success = false;
            }
            g_free(dir_path);
            continue;
        }

        char *dst_path = g_strdup_printf("%s%s", base_path, fstat.m_filename);
        DPRINTF("extracting %s to %s\n", fstat.m_filename, dst_path);

        // Handle executable backup
        if (!strcmp(fstat.m_filename, executable_name)) {
            char *backup_path = g_strdup_printf("%s%s", base_path, backup_name);
            if (!move_file(dst_path, backup_path)) {
                DPRINTF("Warning: Failed to backup current executable\n");
            }
            g_free(backup_path);
        }

        if (!create_directories(dst_path)) {
            DPRINTF("Failed to create directories for %s\n", dst_path);
            g_free(dst_path);
            success = false;
            continue;
        }

        if (!mz_zip_reader_extract_to_file(&zip, file_idx, dst_path, 0)) {
            DPRINTF("mz_zip_reader_extract_to_file failed to create %s\n", dst_path);
            g_free(dst_path);
            success = false;
            continue;
        }

        // Set executable permission if needed
        if (!strcmp(fstat.m_filename, executable_name)) {
            if (!set_executable_permission(dst_path)) {
                DPRINTF("Warning: Failed to set executable permission\n");
            }
        }

        g_free(dst_path);
    }

    mz_zip_reader_end(&zip);
    return success;
}

AutoUpdateWindow::AutoUpdateWindow()
{
    is_open = false;
#if defined(__linux__)
    // Configure Linux update URLs based on execution context
    configure_linux_update_urls();
#endif
}

void AutoUpdateWindow::CheckForUpdates()
{
    updater.check_for_update([this](){
        is_open |= updater.is_update_available();
    });
}

void AutoUpdateWindow::Draw()
{
    if (!is_open) return;
    ImGui::SetNextWindowContentSize(ImVec2(550.0f*g_viewport_mgr.m_scale, 0.0f));
    if (!ImGui::Begin("Update", &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowAppearing() && !updater.is_update_available()) {
        updater.check_for_update();
    }

    const char *status_msg[] = {
        "",
        "An error has occured. Try again.",
        "Checking for update...",
        "Downloading update...",
        "Update successful! Restart to launch updated version of xemu."
    };
    const char *available_msg[] = {
        "Update availability unknown.",
        "This version of xemu is up to date.",
        "An updated version of xemu is available!",
    };

    if (updater.get_status() == UPDATER_IDLE) {
        ImGui::Text("%s", available_msg[updater.get_update_availability()]);
    } else {
        ImGui::Text("%s", status_msg[updater.get_status()]);
    }

    if (updater.is_updating()) {
        ImGui::ProgressBar(updater.get_update_progress_percentage()/100.0f,
                           ImVec2(-1.0f, 0.0f));
    }

    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));

    float w = (130)*g_viewport_mgr.m_scale;
    float bw = w + (10)*g_viewport_mgr.m_scale;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth()-bw);

    if (updater.is_checking_for_update() || updater.is_updating()) {
        if (ImGui::Button("Cancel", ImVec2(w, 0))) {
            updater.cancel();
        }
    } else {
        if (updater.is_pending_restart()) {
            if (ImGui::Button("Restart", ImVec2(w, 0))) {
                updater.restart_to_updated();
            }
        } else if (updater.is_update_available()) {
            if (ImGui::Button("Update", ImVec2(w, 0))) {
                updater.update();
            }
        } else {
            if (ImGui::Button("Check for Update", ImVec2(w, 0))) {
                updater.check_for_update();
            }
        }
    }

    ImGui::End();
}

Updater::Updater()
{
    m_status = UPDATER_IDLE;
    m_update_availability = UPDATE_AVAILABILITY_UNKNOWN;
    m_update_percentage = 0;
    m_latest_version = "Unknown";
    m_should_cancel = false;
}

void Updater::check_for_update(UpdaterCallback on_complete)
{
    if (m_status == UPDATER_IDLE || m_status == UPDATER_ERROR) {
        m_on_complete = on_complete;
        qemu_thread_create(&m_thread, "update_worker",
                           &Updater::checker_thread_worker_func,
                           this, QEMU_THREAD_JOINABLE);
    }
}

void *Updater::checker_thread_worker_func(void *updater)
{
    ((Updater *)updater)->check_for_update_internal();
    return NULL;
}

void Updater::check_for_update_internal()
{
    m_status = UPDATER_CHECKING_FOR_UPDATE;
    
    g_autoptr(GByteArray) data = g_byte_array_new();
    int res = http_get(version_url, data, NULL, NULL);

    if (m_should_cancel) {
        m_should_cancel = false;
        m_status = UPDATER_IDLE;
        goto finished;
    } else if (res != 200) {
        DPRINTF("Version check failed with HTTP status %d\n", res);
        m_status = UPDATER_ERROR;
        goto finished;
    }

    // Ensure null-termination for version string
    g_byte_array_append(data, (const guint8*)"\0", 1);
    m_latest_version = std::string((const char *)data->data);
    
    // Trim whitespace from version string
    size_t start = m_latest_version.find_first_not_of(" \t\r\n");
    size_t end = m_latest_version.find_last_not_of(" \t\r\n");
    if (start != std::string::npos && end != std::string::npos) {
        m_latest_version = m_latest_version.substr(start, end - start + 1);
    }

    DPRINTF("Current version: %s, Latest version: %s\n", xemu_version, m_latest_version.c_str());

    if (m_latest_version != xemu_version) {
        m_update_availability = UPDATE_AVAILABLE;
    } else {
        m_update_availability = UPDATE_NOT_AVAILABLE;
    }

    m_status = UPDATER_IDLE;
finished:
    if (m_on_complete) {
        m_on_complete();
    }
}

void Updater::update()
{
#if defined(__linux__)
    if (!download_url) {
        DPRINTF("Auto-updater not available for this Linux installation type\n");
        m_status = UPDATER_ERROR;
        return;
    }
#endif

    if (m_status == UPDATER_IDLE || m_status == UPDATER_ERROR) {
        m_status = UPDATER_UPDATING;
        m_update_percentage = 0;
        qemu_thread_create(&m_thread, "update_worker",
                           &Updater::update_thread_worker_func,
                           this, QEMU_THREAD_JOINABLE);
    }
}

void *Updater::update_thread_worker_func(void *updater)
{
    ((Updater *)updater)->update_internal();
    return NULL;
}

int Updater::progress_cb(http_progress_cb_info *info)
{
    if (info->dltotal == 0) {
        m_update_percentage = 0;
    } else {
        m_update_percentage = (int)((info->dlnow * 100) / info->dltotal);
    }

    return m_should_cancel ? 1 : 0;
}

void Updater::update_internal()
{
    g_autoptr(GByteArray) data = g_byte_array_new();

    http_progress_cb_info progress_info;
    progress_info.userptr = this;
    progress_info.progress = [](http_progress_cb_info *info) {
        return static_cast<Updater *>(info->userptr)->progress_cb(info);
    };

    DPRINTF("Downloading update from: %s\n", download_url);
    int res = http_get(download_url, data, &progress_info, NULL);

    if (m_should_cancel) {
        m_should_cancel = false;
        m_status = UPDATER_IDLE;
        DPRINTF("Update cancelled by user\n");
        return;
    } else if (res != 200) {
        DPRINTF("Download failed with HTTP status %d\n", res);
        m_status = UPDATER_ERROR;
        return;
    }

    const char *base_path = SDL_GetBasePath();
    if (!base_path) {
        DPRINTF("Failed to get base path\n");
        m_status = UPDATER_ERROR;
        return;
    }

    bool extract_success = false;

    // Choose extraction method based on update format
    switch (update_format) {
#if defined(__linux__) && defined(HAVE_LIBARCHIVE)
        case UPDATE_FORMAT_TAR_GZ:
            extract_success = extract_tar_gz(data->data, data->len, base_path);
            break;
#endif
#if defined(__linux__)
        case UPDATE_FORMAT_APPIMAGE:
            extract_success = extract_appimage(data->data, data->len, base_path);
            break;
#endif
        case UPDATE_FORMAT_ZIP:
            extract_success = extract_zip(data->data, data->len, base_path);
            break;
        default:
            DPRINTF("Unknown or unsupported update format: %d\n", update_format);
            extract_success = false;
            break;
    }

    if (extract_success) {
        DPRINTF("Update extraction successful\n");
        m_status = UPDATER_UPDATE_SUCCESSFUL;
    } else {
        DPRINTF("Update extraction failed\n");
        m_status = UPDATER_ERROR;
    }
}

extern "C" {
extern char **gArgv;
}

void Updater::restart_to_updated()
{
#if defined(__linux__)
    if (update_format == UPDATE_FORMAT_APPIMAGE) {
        // For AppImage, restart using the APPIMAGE environment variable path
        const char *appimage_path = getenv("APPIMAGE");
        if (appimage_path) {
            DPRINTF("Restarting AppImage: %s\n", appimage_path);
            execv(appimage_path, gArgv);
            DPRINTF("AppImage restart failed: %s\n", strerror(errno));
            exit(1);
        } else {
            DPRINTF("APPIMAGE environment variable not found\n");
            exit(1);
        }
    }
#endif
    
    // Regular restart for non-AppImage cases
    const char *base_path = SDL_GetBasePath();
    if (!base_path) {
        DPRINTF("Failed to get base path for restart\n");
        exit(1);
    }
    
    char *target_exec = g_strdup_printf("%s%s", base_path, executable_name);
    
    DPRINTF("Restarting to updated executable %s\n", target_exec);

#if defined(_WIN32)
    _execv(target_exec, gArgv);
#else
    execv(target_exec, gArgv);
#endif

    DPRINTF("Launching updated executable failed: %s\n", strerror(errno));
    g_free(target_exec);
    exit(1);
}