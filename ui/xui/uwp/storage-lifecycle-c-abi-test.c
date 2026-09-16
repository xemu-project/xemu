/* C-side ABI probe; this file deliberately has no Windows dependencies. */

#include "storage-lifecycle.h"

_Static_assert(sizeof(XemuUwpStorageCallbacks) == 2 * sizeof(void *),
               "storage callback ABI changed");
_Static_assert(sizeof(XemuUwpLifecycleCallbacks) == 5 * sizeof(void *),
               "lifecycle callback ABI changed");
_Static_assert(XEMU_UWP_STORAGE_LOCAL_FOLDER == 0,
               "LocalFolder enum value changed");
_Static_assert(XEMU_UWP_STORAGE_INSTALLED_LOCATION == 1,
               "InstalledLocation enum value changed");

int xemu_uwp_storage_c_abi_probe(void)
{
    return (int)sizeof(XemuUwpStorage) + (int)sizeof(XemuUwpLifecycle);
}
