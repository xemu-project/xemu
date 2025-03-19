#include "qemu/osdep.h"
#include "qemu/mstring.h"

#include <stdarg.h>

void mstring_append_fmt(MString *qstring, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    mstring_append_va(qstring, fmt, ap);
    va_end(ap);
}

MString *mstring_from_fmt(const char *fmt, ...)
{
    MString *ret = mstring_new();
    va_list ap;
    va_start(ap, fmt);
    mstring_append_va(ret, fmt, ap);
    va_end(ap);

    return ret;
}

void mstring_append_va(MString *qstring, const char *fmt, va_list va)
{
    char scratch[256];

    va_list ap;
    va_copy(ap, va);
    const int len = vsnprintf(scratch, sizeof(scratch), fmt, ap);
    va_end(ap);

    if (len == 0) {
        return;
    } else if (len < sizeof(scratch)) {
        mstring_append(qstring, scratch);
        return;
    }

    /* overflowed out scratch buffer, alloc and try again */
    char *buf = g_malloc(len + 1);
    va_copy(ap, va);
    vsnprintf(buf, len + 1, fmt, ap);
    va_end(ap);

    mstring_append(qstring, buf);
    g_free(buf);
}
