/* Contract tests intentionally use no Windows Runtime or desktop API. */

#include "storage-lifecycle.h"

#include <assert.h>
#include <string.h>

extern "C" int xemu_uwp_storage_c_abi_probe(void);

static_assert(sizeof(XemuUwpStorageCallbacks) == 2 * sizeof(void *),
              "storage callback ABI changed");
static_assert(sizeof(XemuUwpLifecycleCallbacks) == 5 * sizeof(void *),
              "lifecycle callback ABI changed");

namespace {

struct Fixture {
    unsigned calls = 0;
};

bool Resolve(void *opaque, XemuUwpStorageLocation location,
             const char *relative, char *out, size_t capacity)
{
    Fixture *fixture = static_cast<Fixture *>(opaque);
    ++fixture->calls;
    const char *prefix =
        location == XEMU_UWP_STORAGE_LOCAL_FOLDER ? "local:/" : "installed:/";
    const size_t needed = strlen(prefix) + strlen(relative) + 1;
    if (needed > capacity) {
        return false;
    }
    strcpy(out, prefix);
    strcat(out, relative);
    return true;
}

bool Count(void *opaque)
{
    ++static_cast<Fixture *>(opaque)->calls;
    return true;
}

} // namespace

int main()
{
    assert(xemu_uwp_storage_c_abi_probe() > 0);
    Fixture fixture;
    XemuUwpStorage storage = {};
    const XemuUwpStorageCallbacks storage_callbacks = { &fixture, Resolve };
    assert(xemu_uwp_storage_init(&storage, &storage_callbacks));
    char path[64] = {};
    assert(xemu_uwp_storage_resolve(&storage, XEMU_UWP_STORAGE_LOCAL_FOLDER,
                                    "saves/state", path, sizeof(path)));
    assert(strcmp(path, "local:/saves/state") == 0);
    assert(!xemu_uwp_storage_resolve(&storage,
                                     XEMU_UWP_STORAGE_INSTALLED_LOCATION,
                                     "../escape", path, sizeof(path)));

    XemuUwpLifecycle lifecycle = {};
    const XemuUwpLifecycleCallbacks lifecycle_callbacks = { &fixture, Count,
                                                            Count, Count,
                                                            Count };
    xemu_uwp_lifecycle_init(&lifecycle, &lifecycle_callbacks);
    assert(xemu_uwp_lifecycle_state(&lifecycle) == XEMU_UWP_LIFECYCLE_CREATED);
    assert(xemu_uwp_lifecycle_start(&lifecycle));
    assert(xemu_uwp_lifecycle_suspend(&lifecycle));
    assert(xemu_uwp_lifecycle_resume(&lifecycle));
    assert(xemu_uwp_lifecycle_stop(&lifecycle));
    assert(xemu_uwp_lifecycle_state(&lifecycle) == XEMU_UWP_LIFECYCLE_STOPPED);
    assert(fixture.calls == 5);
    return 0;
}
