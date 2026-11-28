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
#pragma once

#include "qemu/osdep.h"

#ifndef GETTEXT_PACKAGE
#define GETTEXT_PACKAGE "xemu"
#endif

#include <glib/gi18n.h>

#if defined(_WIN32)
const char *xemu_i18n_gettext(const char *message);

#undef _
#define _(String) xemu_i18n_gettext(String)
#endif
