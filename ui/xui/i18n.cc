//
// xemu User Interface Localization
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#include "i18n.hh"

#if defined(_WIN32)

#include <windows.h>

#include <cstdint>
#include <cstring>

extern const unsigned char xemu_sv_catalog[];
extern const size_t xemu_sv_catalog_size;

static uint32_t ReadU32(const unsigned char *data, bool swapped)
{
    uint32_t value = static_cast<uint32_t>(data[0]) |
                     (static_cast<uint32_t>(data[1]) << 8) |
                     (static_cast<uint32_t>(data[2]) << 16) |
                     (static_cast<uint32_t>(data[3]) << 24);
    return swapped ? __builtin_bswap32(value) : value;
}

static bool IsSwedishLocale()
{
    wchar_t language[LOCALE_NAME_MAX_LENGTH];
    return GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO639LANGNAME,
                           language, G_N_ELEMENTS(language)) &&
           wcscmp(language, L"sv") == 0;
}

const char *xemu_i18n_gettext(const char *message)
{
    if (!IsSwedishLocale() || xemu_sv_catalog_size < 28) {
        return g_dgettext(GETTEXT_PACKAGE, message);
    }

    const uint32_t magic = ReadU32(xemu_sv_catalog, false);
    const bool swapped = magic == 0xde120495;
    if (magic != 0x950412de && !swapped) {
        return g_dgettext(GETTEXT_PACKAGE, message);
    }

    const uint32_t count = ReadU32(xemu_sv_catalog + 8, swapped);
    const uint32_t originals = ReadU32(xemu_sv_catalog + 12, swapped);
    const uint32_t translations = ReadU32(xemu_sv_catalog + 16, swapped);
    if (count > (xemu_sv_catalog_size - 28) / 8 ||
        originals > xemu_sv_catalog_size - count * 8 ||
        translations > xemu_sv_catalog_size - count * 8) {
        return g_dgettext(GETTEXT_PACKAGE, message);
    }

    size_t first = 0;
    size_t last = count;
    const size_t message_length = strlen(message);
    while (first < last) {
        const size_t index = first + (last - first) / 2;
        const unsigned char *original = xemu_sv_catalog + originals + index * 8;
        const uint32_t length = ReadU32(original, swapped);
        const uint32_t offset = ReadU32(original + 4, swapped);
        if (offset > xemu_sv_catalog_size || length > xemu_sv_catalog_size - offset) {
            return g_dgettext(GETTEXT_PACKAGE, message);
        }

        const size_t comparable = message_length < length ? message_length : length;
        const int comparison = memcmp(message, xemu_sv_catalog + offset, comparable);
        if (comparison == 0 && message_length == length) {
            const unsigned char *translation = xemu_sv_catalog + translations + index * 8;
            const uint32_t translation_length = ReadU32(translation, swapped);
            const uint32_t translation_offset = ReadU32(translation + 4, swapped);
            if (translation_offset <= xemu_sv_catalog_size &&
                translation_length < xemu_sv_catalog_size - translation_offset) {
                return reinterpret_cast<const char *>(xemu_sv_catalog + translation_offset);
            }
            return g_dgettext(GETTEXT_PACKAGE, message);
        }
        if (comparison < 0 || (comparison == 0 && message_length < length)) {
            last = index;
        } else {
            first = index + 1;
        }
    }
    return g_dgettext(GETTEXT_PACKAGE, message);
}

#endif
