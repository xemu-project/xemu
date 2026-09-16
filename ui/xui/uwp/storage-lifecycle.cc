/* Runtime-neutral storage and lifecycle contracts for the UWP host. */

#include "storage-lifecycle.h"

#include <string.h>

namespace {

bool ValidRelativePath(const char *path)
{
    if (path == nullptr || path[0] == '\0' || path[0] == '/' ||
        path[0] == '\\') {
        return false;
    }

    const char *component = path;
    for (const char *cursor = path;; ++cursor) {
        if (*cursor != '/' && *cursor != '\\' && *cursor != '\0') {
            continue;
        }
        const size_t length = static_cast<size_t>(cursor - component);
        if (length == 0 || (length == 1 && component[0] == '.') ||
            (length == 2 && component[0] == '.' && component[1] == '.')) {
            return false;
        }
        if (*cursor == '\0') {
            return true;
        }
        component = cursor + 1;
    }
}

bool Invoke(XemuUwpLifecycle *lifecycle, XemuUwpLifecycleCallback callback,
            XemuUwpLifecycleState next)
{
    if (lifecycle == nullptr || callback == nullptr ||
        !callback(lifecycle->callbacks.opaque)) {
        return false;
    }
    lifecycle->state = next;
    return true;
}

} // namespace

extern "C" bool xemu_uwp_storage_init(XemuUwpStorage *storage,
                                      const XemuUwpStorageCallbacks *callbacks)
{
    if (storage == nullptr || callbacks == nullptr ||
        callbacks->resolve_path == nullptr) {
        return false;
    }
    storage->callbacks = *callbacks;
    storage->initialized = 1;
    return true;
}

extern "C" bool xemu_uwp_storage_resolve(const XemuUwpStorage *storage,
                                         XemuUwpStorageLocation location,
                                         const char *relative_path, char *path,
                                         size_t path_capacity)
{
    if (storage == nullptr || storage->initialized == 0 ||
        storage->callbacks.resolve_path == nullptr ||
        location < XEMU_UWP_STORAGE_LOCAL_FOLDER ||
        location > XEMU_UWP_STORAGE_INSTALLED_LOCATION ||
        !ValidRelativePath(relative_path) || path == nullptr ||
        path_capacity == 0) {
        return false;
    }
    path[0] = '\0';
    return storage->callbacks.resolve_path(storage->callbacks.opaque, location,
                                           relative_path, path,
                                           path_capacity) &&
           path[0] != '\0';
}

extern "C" void
xemu_uwp_lifecycle_init(XemuUwpLifecycle *lifecycle,
                        const XemuUwpLifecycleCallbacks *callbacks)
{
    if (lifecycle == nullptr) {
        return;
    }
    lifecycle->callbacks =
        callbacks != nullptr ? *callbacks : XemuUwpLifecycleCallbacks{};
    lifecycle->state = XEMU_UWP_LIFECYCLE_CREATED;
}

extern "C" XemuUwpLifecycleState
xemu_uwp_lifecycle_state(const XemuUwpLifecycle *lifecycle)
{
    return lifecycle != nullptr ? lifecycle->state : XEMU_UWP_LIFECYCLE_STOPPED;
}

extern "C" bool xemu_uwp_lifecycle_start(XemuUwpLifecycle *lifecycle)
{
    if (lifecycle == nullptr ||
        lifecycle->state != XEMU_UWP_LIFECYCLE_CREATED) {
        return false;
    }
    return Invoke(lifecycle, lifecycle->callbacks.started,
                  XEMU_UWP_LIFECYCLE_RUNNING);
}

extern "C" bool xemu_uwp_lifecycle_suspend(XemuUwpLifecycle *lifecycle)
{
    if (lifecycle == nullptr ||
        lifecycle->state != XEMU_UWP_LIFECYCLE_RUNNING) {
        return false;
    }
    return Invoke(lifecycle, lifecycle->callbacks.suspended,
                  XEMU_UWP_LIFECYCLE_SUSPENDED);
}

extern "C" bool xemu_uwp_lifecycle_resume(XemuUwpLifecycle *lifecycle)
{
    if (lifecycle == nullptr ||
        lifecycle->state != XEMU_UWP_LIFECYCLE_SUSPENDED) {
        return false;
    }
    return Invoke(lifecycle, lifecycle->callbacks.resumed,
                  XEMU_UWP_LIFECYCLE_RUNNING);
}

extern "C" bool xemu_uwp_lifecycle_stop(XemuUwpLifecycle *lifecycle)
{
    if (lifecycle == nullptr ||
        (lifecycle->state != XEMU_UWP_LIFECYCLE_RUNNING &&
         lifecycle->state != XEMU_UWP_LIFECYCLE_SUSPENDED)) {
        return false;
    }
    const XemuUwpLifecycleState previous = lifecycle->state;
    lifecycle->state = XEMU_UWP_LIFECYCLE_STOPPING;
    if (lifecycle->callbacks.stopped == nullptr ||
        !lifecycle->callbacks.stopped(lifecycle->callbacks.opaque)) {
        lifecycle->state = previous;
        return false;
    }
    lifecycle->state = XEMU_UWP_LIFECYCLE_STOPPED;
    return true;
}
