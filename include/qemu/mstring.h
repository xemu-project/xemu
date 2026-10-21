#ifndef MSTRING_H
#define MSTRING_H

#include "qemu/osdep.h"
#include "glib.h"

typedef struct {
    GString *gstr;
    int refcnt;
} MString;

static inline void mstring_ref(MString *mstr)
{
    mstr->refcnt++;
}

static inline void mstring_unref(MString *mstr)
{
    mstr->refcnt--;
    if (mstr->refcnt == 0) {
        g_string_free(mstr->gstr, true);
        g_free(mstr);
    }
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(MString, mstring_unref)

static inline MString *mstring_new(void)
{
    MString *mstr = g_malloc(sizeof(MString));
    mstr->refcnt = 1;
    mstr->gstr = g_string_new("");
    return mstr;
}

static inline MString *mstring_from_str(const char *str)
{
    MString *mstr = g_malloc(sizeof(MString));
    mstr->refcnt = 1;
    mstr->gstr = g_string_new(str);
    return mstr;
}

static inline MString * G_GNUC_PRINTF(1, 2)
mstring_from_fmt(const char *fmt, ...)
{
    MString *mstr = g_malloc(sizeof(MString));
    mstr->refcnt = 1;

    va_list args;
    va_start(args, fmt);
    // FIXME: Use g_string_new_take (GLib 2.78+)
    g_autofree gchar *str = g_strdup_vprintf(fmt, args);
    mstr->gstr = g_string_new(str);
    va_end(args);

    return mstr;
}

static inline void mstring_append(MString *mstr, const char *str)
{
    g_string_append(mstr->gstr, str);
}

static inline void G_GNUC_PRINTF(2, 3)
mstring_append_fmt(MString *mstr, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    g_string_append_vprintf(mstr->gstr, fmt, args);
    va_end(args);
}

static inline const gchar *mstring_get_str(MString *mstr)
{
    return mstr->gstr->str;
}

static inline size_t mstring_get_length(MString *mstr)
{
    return mstr->gstr->len;
}

#endif
