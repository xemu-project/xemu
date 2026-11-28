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
#include <winnls.h>

#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

void xemu_i18n_register_embedded_catalogs(void);

struct XemuCatalog {
    std::string language;
    std::unordered_map<std::string_view, std::string_view> messages;
};

static std::vector<XemuCatalog> &xemu_catalogs()
{
    static std::vector<XemuCatalog> catalogs;
    return catalogs;
}

static const XemuCatalog *xemu_active_catalog;

static uint32_t ReadU32(const unsigned char *data, bool swapped)
{
    uint32_t value = static_cast<uint32_t>(data[0]) |
                     (static_cast<uint32_t>(data[1]) << 8) |
                     (static_cast<uint32_t>(data[2]) << 16) |
                     (static_cast<uint32_t>(data[3]) << 24);
    return swapped ? __builtin_bswap32(value) : value;
}

static std::string NormalizeLanguage(std::string language)
{
    for (char &character : language) {
        if (character == '_') {
            character = '-';
        } else if (character >= 'A' && character <= 'Z') {
            character += 'a' - 'A';
        }
    }
    const size_t modifier = language.find('@');
    if (modifier != std::string::npos) {
        language[modifier] = '-';
        if (language.substr(modifier + 1) == "latin") {
            language.replace(modifier + 1, std::string::npos, "latn");
        }
    }
    return language;
}

static bool ReadLanguageName(const wchar_t *language, std::string &result)
{
    const int required = WideCharToMultiByte(CP_UTF8, 0, language, -1, nullptr,
                                              0, nullptr, nullptr);
    if (required <= 1) {
        return false;
    }
    std::vector<char> utf8(required);
    if (!WideCharToMultiByte(CP_UTF8, 0, language, -1, utf8.data(), required,
                             nullptr, nullptr)) {
        return false;
    }
    result.assign(utf8.data());
    return true;
}

static std::vector<std::string> PreferredLanguages()
{
    std::vector<std::string> languages;
    ULONG count = 0;
    ULONG size = 0;
    const BOOL queried = GetUserPreferredUILanguages(
        MUI_LANGUAGE_NAME, &count, nullptr, &size);
    if (size > 0 && (queried || GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
        std::vector<wchar_t> buffer(size);
        if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, buffer.data(),
                                        &size)) {
            for (const wchar_t *language = buffer.data(); *language;
                 language += wcslen(language) + 1) {
                std::string utf8;
                if (ReadLanguageName(language, utf8)) {
                    languages.push_back(NormalizeLanguage(std::move(utf8)));
                }
            }
        }
    }

    if (languages.empty()) {
        wchar_t language[LOCALE_NAME_MAX_LENGTH];
        if (GetUserDefaultLocaleName(language, G_N_ELEMENTS(language))) {
            std::string utf8;
            if (ReadLanguageName(language, utf8)) {
                languages.push_back(NormalizeLanguage(std::move(utf8)));
            }
        }
    }
    return languages;
}

void xemu_i18n_register_catalog(const char *language,
                                const unsigned char *data, size_t size)
{
    if (!language || !data || size < 28) {
        return;
    }

    const uint32_t magic = ReadU32(data, false);
    const bool swapped = magic == 0xde120495;
    if (magic != 0x950412de && !swapped) {
        return;
    }

    const uint32_t count = ReadU32(data + 8, swapped);
    const uint32_t originals = ReadU32(data + 12, swapped);
    const uint32_t translations = ReadU32(data + 16, swapped);
    if (count > (size - 28) / 8 || originals > size - count * 8 ||
        translations > size - count * 8) {
        return;
    }

    XemuCatalog catalog;
    catalog.language = NormalizeLanguage(language);
    catalog.messages.reserve(count);
    for (uint32_t index = 0; index < count; ++index) {
        const unsigned char *original = data + originals + index * 8;
        const unsigned char *translation = data + translations + index * 8;
        const uint32_t original_length = ReadU32(original, swapped);
        const uint32_t original_offset = ReadU32(original + 4, swapped);
        const uint32_t translation_length = ReadU32(translation, swapped);
        const uint32_t translation_offset = ReadU32(translation + 4, swapped);
        if (!original_length || !translation_length) {
            continue;
        }
        if (original_offset >= size || original_length >= size - original_offset ||
            translation_offset >= size ||
            translation_length >= size - translation_offset ||
            data[original_offset + original_length] != 0 ||
            data[translation_offset + translation_length] != 0) {
            return;
        }

        const char *original_text = reinterpret_cast<const char *>(data + original_offset);
        const char *translation_text = reinterpret_cast<const char *>(data + translation_offset);
        const char *original_end = static_cast<const char *>(
            memchr(original_text, '\0', original_length));
        const char *translation_end = static_cast<const char *>(
            memchr(translation_text, '\0', translation_length));
        const size_t singular_length = original_end
            ? original_end - original_text : original_length;
        const size_t translated_length = translation_end
            ? translation_end - translation_text : translation_length;
        if (singular_length && translated_length) {
            catalog.messages.emplace(
                std::string_view(original_text, singular_length),
                std::string_view(translation_text, translated_length));
        }
    }
    if (!catalog.messages.empty()) {
        xemu_catalogs().push_back(std::move(catalog));
    }
}

void xemu_i18n_select_catalog(void)
{
    xemu_i18n_register_embedded_catalogs();
    const std::vector<std::string> languages = PreferredLanguages();
    const std::vector<XemuCatalog> &catalogs = xemu_catalogs();

    for (const std::string &language : languages) {
        for (const XemuCatalog &catalog : catalogs) {
            if (catalog.language == language) {
                xemu_active_catalog = &catalog;
                return;
            }
        }
    }
    for (const std::string &language : languages) {
        std::string fallback = language;
        while (!fallback.empty()) {
            for (const XemuCatalog &catalog : catalogs) {
                if (catalog.language == fallback) {
                    xemu_active_catalog = &catalog;
                    return;
                }
            }
            const size_t separator = fallback.rfind('-');
            if (separator == std::string::npos) {
                break;
            }
            fallback.resize(separator);
        }
    }
}

const char *xemu_i18n_gettext(const char *message)
{
    if (xemu_active_catalog) {
        const auto translation = xemu_active_catalog->messages.find(message);
        if (translation != xemu_active_catalog->messages.end()) {
            return translation->second.data();
        }
    }
    return message;
}

#endif
