//
// xemu User Interface - Cross-Platform Auto-Update Header
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
#pragma once

// Platform detection - support Windows, Linux, and macOS
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)

#include <string>
#include <stdint.h>
#include <functional>

extern "C" {
#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qemu/http.h"
}

typedef enum {
    UPDATE_AVAILABILITY_UNKNOWN,
    UPDATE_NOT_AVAILABLE,
    UPDATE_AVAILABLE
} UpdateAvailability;

typedef enum {
    UPDATER_IDLE,
    UPDATER_ERROR,
    UPDATER_CHECKING_FOR_UPDATE,
    UPDATER_UPDATING,
    UPDATER_UPDATE_SUCCESSFUL
} UpdateStatus;

// Update format types for different platforms
typedef enum {
    UPDATE_FORMAT_ZIP,
    UPDATE_FORMAT_TAR_GZ,
    UPDATE_FORMAT_APPIMAGE
} UpdateFormat;

using UpdaterCallback = std::function<void(void)>;

class Updater {
private:
    UpdateAvailability  m_update_availability;
    UpdateStatus        m_status;
    int                 m_update_percentage;
    QemuThread          m_thread;
    std::string         m_latest_version;
    bool                m_should_cancel;
    UpdaterCallback     m_on_complete;

protected:
    int progress_cb(http_progress_cb_info *progress_info);

public:
    Updater();
    
    // Status getters
    UpdateStatus get_status() { return m_status; }
    UpdateAvailability get_update_availability() { return m_update_availability; }
    std::string get_update_version() { return m_latest_version; }
    int get_update_progress_percentage() { return m_update_percentage; }
    
    // State checkers
    bool is_errored() { return m_status == UPDATER_ERROR; }
    bool is_pending_restart() { return m_status == UPDATER_UPDATE_SUCCESSFUL; }
    bool is_update_available() { return m_update_availability == UPDATE_AVAILABLE; }
    bool is_checking_for_update() { return m_status == UPDATER_CHECKING_FOR_UPDATE; }
    bool is_updating() { return m_status == UPDATER_UPDATING; }
    
    // Actions
    void cancel() { m_should_cancel = true; }
    void update();
    void check_for_update(UpdaterCallback on_complete = nullptr);
    void restart_to_updated(void);
    
    // Internal methods (public for thread access)
    void update_internal();
    void check_for_update_internal();
    
    // Static thread workers
    static void *update_thread_worker_func(void *updater);
    static void *checker_thread_worker_func(void *updater);
};

class AutoUpdateWindow {
protected:
    Updater updater;

public:
    bool is_open;
    
    AutoUpdateWindow();
    void CheckForUpdates();
    void Draw();
};

extern AutoUpdateWindow update_window;

#endif // Platform detection