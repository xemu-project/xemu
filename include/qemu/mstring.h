#ifndef MSTRING_H
#define MSTRING_H

#include "qemu/osdep.h"
#include <string.h>

typedef struct {
   int ref;
   gchar *string;
} MString;

void mstring_append_fmt(MString *mstring, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
MString *mstring_from_fmt(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void mstring_append_va(MString *mstring, const char *fmt, va_list va) __attribute__ ((format (printf, 2, 0)));

static inline
void mstring_ref(MString *mstr)
{
   mstr->ref++;
}

static inline
void mstring_unref(MString *mstr)
{
   mstr->ref--;
   if (!mstr->ref) {
      g_free(mstr->string);
      g_free(mstr);
   }
}

static inline
void mstring_append(MString *mstr, const char *str)
{
   gchar *n = g_strconcat(mstr->string, str, NULL);
   g_free(mstr->string);
   mstr->string = n;
}

static inline
MString *mstring_new(void)
{
   MString *mstr = g_malloc(sizeof(MString));
   mstr->ref = 1;
   mstr->string = g_strdup("");
   return mstr;
}

static inline
MString *mstring_from_str(const char *str)
{
   MString *mstr = g_malloc(sizeof(MString));
   mstr->ref = 1;
   mstr->string = g_strdup(str);
   return mstr;
}

static inline
const gchar *mstring_get_str(MString *mstr)
{
   return mstr->string;
}

static inline
size_t mstring_get_length(MString *mstr)
{
   return strlen(mstr->string);
}

#endif
