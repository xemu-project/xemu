#ifndef MSTRING_H
#define MSTRING_H

#include "qemu/osdep.h"
#include <string.h>

typedef struct {
   int ref;
   gchar *string;
} MString;

void mstring_append_fmt(MString *mstring, const char *fmt, ...);
MString *mstring_from_fmt(const char *fmt, ...);
void mstring_append_va(MString *mstring, const char *fmt, va_list va);

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
void mstring_append_chr(MString *mstr, char chr)
{
   mstring_append_fmt(mstr, "%c", chr);
}

static inline
void mstring_append_int(MString *mstr, int val)
{
   mstring_append_fmt(mstr, "%" PRId64, val);
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
