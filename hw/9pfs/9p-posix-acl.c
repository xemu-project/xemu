/*
 * 9p system.posix* xattr callback
 *
 * Copyright IBM, Corp. 2010
 *
 * Authors:
 * Aneesh Kumar K.V <aneesh.kumar@linux.vnet.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

/*
 * Not so fast! You might want to read the 9p developer docs first:
 * https://wiki.qemu.org/Documentation/9p
 */

#include "qemu/osdep.h"
#include "qemu/xattr.h"
#include "9p.h"
#include "fsdev/file-op-9p.h"
#include "9p-xattr.h"

#define MAP_ACL_ACCESS "user.virtfs.system.posix_acl_access"
#define MAP_ACL_DEFAULT "user.virtfs.system.posix_acl_default"
#define ACL_ACCESS "system.posix_acl_access"
#define ACL_DEFAULT "system.posix_acl_default"

static ssize_t mp_pacl_getxattr(FsContext *ctx, const char *path,
                                const char *name, void *value, size_t size)
{
    return local_getxattr_nofollow(ctx, path, MAP_ACL_ACCESS, value, size);
}

static ssize_t mp_pacl_listxattr(FsContext *ctx, const char *path,
                                 char *name, void *value, size_t osize)
{
    ssize_t len = sizeof(ACL_ACCESS);

    if (!value) {
        return len;
    }

    if (osize < len) {
        errno = ERANGE;
        return -1;
    }

    /* len includes the trailing NUL */
    memcpy(value, ACL_ACCESS, len);
    return 0;
}

static int mp_pacl_setxattr(FsContext *ctx, const char *path, const char *name,
                            void *value, size_t size, int flags)
{
    return local_setxattr_nofollow(ctx, path, MAP_ACL_ACCESS, value, size,
                                   flags);
}

static int mp_pacl_removexattr(FsContext *ctx,
                               const char *path, const char *name)
{
    int ret;

    ret = local_removexattr_nofollow(ctx, path, MAP_ACL_ACCESS);
    /*
     * macOS returns ENOATTR (!=ENODATA on macOS), whereas Linux returns
     * ENODATA (==ENOATTR on Linux), so checking for ENOATTR is fine
     */
    if (ret == -1 && errno == ENOATTR) {
        /*
         * We don't get ENODATA error when trying to remove a
         * posix acl that is not present. So don't throw the error
         * even in case of mapped security model
         */
        errno = 0;
        ret = 0;
    }
    return ret;
}

static ssize_t mp_dacl_getxattr(FsContext *ctx, const char *path,
                                const char *name, void *value, size_t size)
{
    return local_getxattr_nofollow(ctx, path, MAP_ACL_DEFAULT, value, size);
}

static ssize_t mp_dacl_listxattr(FsContext *ctx, const char *path,
                                 char *name, void *value, size_t osize)
{
    ssize_t len = sizeof(ACL_DEFAULT);

    if (!value) {
        return len;
    }

    if (osize < len) {
        errno = ERANGE;
        return -1;
    }

    /* len includes the trailing NUL */
    memcpy(value, ACL_DEFAULT, len);
    return 0;
}

static int mp_dacl_setxattr(FsContext *ctx, const char *path, const char *name,
                            void *value, size_t size, int flags)
{
    return local_setxattr_nofollow(ctx, path, MAP_ACL_DEFAULT, value, size,
                                   flags);
}

static int mp_dacl_removexattr(FsContext *ctx,
                               const char *path, const char *name)
{
    int ret;

    ret = local_removexattr_nofollow(ctx, path, MAP_ACL_DEFAULT);
    /*
     * macOS returns ENOATTR (!=ENODATA on macOS), whereas Linux returns
     * ENODATA (==ENOATTR on Linux), so checking for ENOATTR is fine
     */
    if (ret == -1 && errno == ENOATTR) {
        /*
         * We don't get ENODATA error when trying to remove a
         * posix acl that is not present. So don't throw the error
         * even in case of mapped security model
         */
        errno = 0;
        ret = 0;
    }
    return ret;
}


XattrOperations mapped_pacl_xattr = {
    .name = "system.posix_acl_access",
    .getxattr = mp_pacl_getxattr,
    .setxattr = mp_pacl_setxattr,
    .listxattr = mp_pacl_listxattr,
    .removexattr = mp_pacl_removexattr,
};

XattrOperations mapped_dacl_xattr = {
    .name = "system.posix_acl_default",
    .getxattr = mp_dacl_getxattr,
    .setxattr = mp_dacl_setxattr,
    .listxattr = mp_dacl_listxattr,
    .removexattr = mp_dacl_removexattr,
};

XattrOperations passthrough_acl_xattr = {
    .name = "system.posix_acl_",
    .getxattr = pt_getxattr,
    .setxattr = pt_setxattr,
    .listxattr = pt_listxattr,
    .removexattr = pt_removexattr,
};

XattrOperations none_acl_xattr = {
    .name = "system.posix_acl_",
    .getxattr = notsup_getxattr,
    .setxattr = notsup_setxattr,
    .listxattr = notsup_listxattr,
    .removexattr = notsup_removexattr,
};
