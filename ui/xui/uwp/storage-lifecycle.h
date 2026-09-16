/*
 * Runtime-neutral storage and lifecycle contracts for the UWP host.
 *
 * The callbacks are deliberately a C ABI.  A WinRT host can adapt
 * ApplicationData::Current->LocalFolder and Package::Current->InstalledLocation
 * without exposing C++/CX types to the emulator core or to contract tests.
 */

#ifndef XEMU_UWP_STORAGE_LIFECYCLE_H
#define XEMU_UWP_STORAGE_LIFECYCLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XemuUwpStorageLocation {
    XEMU_UWP_STORAGE_LOCAL_FOLDER = 0,
    XEMU_UWP_STORAGE_INSTALLED_LOCATION = 1,
} XemuUwpStorageLocation;

typedef bool (*XemuUwpResolvePath)(void *opaque,
                                   XemuUwpStorageLocation location,
                                   const char *relative_path, char *path,
                                   size_t path_capacity);

typedef struct XemuUwpStorageCallbacks {
    void *opaque;
    XemuUwpResolvePath resolve_path;
} XemuUwpStorageCallbacks;

typedef struct XemuUwpStorage {
    XemuUwpStorageCallbacks callbacks;
    uint32_t initialized;
} XemuUwpStorage;

/* The adapter owns the returned path buffer; this layer never allocates or
 * retains a WinRT StorageFolder.  relative_path must be a relative path with
 * no empty, ".", or ".." component. */
bool xemu_uwp_storage_init(XemuUwpStorage *storage,
                           const XemuUwpStorageCallbacks *callbacks);
bool xemu_uwp_storage_resolve(const XemuUwpStorage *storage,
                              XemuUwpStorageLocation location,
                              const char *relative_path, char *path,
                              size_t path_capacity);

typedef enum XemuUwpLifecycleState {
    XEMU_UWP_LIFECYCLE_CREATED = 0,
    XEMU_UWP_LIFECYCLE_RUNNING = 1,
    XEMU_UWP_LIFECYCLE_SUSPENDED = 2,
    XEMU_UWP_LIFECYCLE_STOPPING = 3,
    XEMU_UWP_LIFECYCLE_STOPPED = 4,
} XemuUwpLifecycleState;

typedef bool (*XemuUwpLifecycleCallback)(void *opaque);

typedef struct XemuUwpLifecycleCallbacks {
    void *opaque;
    XemuUwpLifecycleCallback started;
    XemuUwpLifecycleCallback suspended;
    XemuUwpLifecycleCallback resumed;
    XemuUwpLifecycleCallback stopped;
} XemuUwpLifecycleCallbacks;

typedef struct XemuUwpLifecycle {
    XemuUwpLifecycleCallbacks callbacks;
    XemuUwpLifecycleState state;
} XemuUwpLifecycle;

void xemu_uwp_lifecycle_init(XemuUwpLifecycle *lifecycle,
                             const XemuUwpLifecycleCallbacks *callbacks);
XemuUwpLifecycleState
xemu_uwp_lifecycle_state(const XemuUwpLifecycle *lifecycle);
bool xemu_uwp_lifecycle_start(XemuUwpLifecycle *lifecycle);
bool xemu_uwp_lifecycle_suspend(XemuUwpLifecycle *lifecycle);
bool xemu_uwp_lifecycle_resume(XemuUwpLifecycle *lifecycle);
bool xemu_uwp_lifecycle_stop(XemuUwpLifecycle *lifecycle);

#ifdef __cplusplus
} /* extern "C" */

static_assert(sizeof(XemuUwpStorageLocation) == sizeof(int),
              "storage location must retain the C enum ABI");
static_assert(sizeof(XemuUwpLifecycleState) == sizeof(int),
              "lifecycle state must retain the C enum ABI");
#endif

#endif /* XEMU_UWP_STORAGE_LIFECYCLE_H */
