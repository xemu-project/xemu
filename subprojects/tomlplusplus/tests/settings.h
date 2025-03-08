// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT
#pragma once

// toml++ config
#define TOML_UNDEF_MACROS 0
#ifndef TOML_HEADER_ONLY
#define TOML_HEADER_ONLY 0
#endif
#ifndef TOML_SHARED_LIB
#define TOML_SHARED_LIB 0
#endif
#ifndef USE_SINGLE_HEADER
#define USE_SINGLE_HEADER 0
#endif
#if defined(LEAK_TESTS) && LEAK_TESTS
#define TOML_CONFIG_HEADER "leakproof.h"
#else
#undef LEAK_TESTS
#define LEAK_TESTS 0
#endif
#ifdef _MSC_VER
#define TOML_CALLCONV __stdcall // just to test that TOML_CALLCONV doesn't cause linker failures
#endif

// catch2 config
#define CATCH_CONFIG_CPP11_TO_STRING
#define CATCH_CONFIG_CPP17_OPTIONAL
#define CATCH_CONFIG_CPP17_STRING_VIEW
#define CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_CONSOLE_WIDTH 120
#define CATCH_CONFIG_CPP11_TO_STRING
#define CATCH_CONFIG_DISABLE_MATCHERS
#define CATCH_CONFIG_NO_NOMINMAX

// windows.h config (included transitively by catch2 on windows)
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOATOM			  // Atom Manager routines
#define NOBITMAP		  //
#define NOCLIPBOARD		  // Clipboard routines
#define NOCOLOR			  // Screen colors
#define NOCOMM			  // COMM driver routines
#define NOCTLMGR		  // Control and Dialog routines
#define NODEFERWINDOWPOS  // DeferWindowPos routines
#define NODRAWTEXT		  // DrawText() and DT_*
#define NOGDI			  // All GDI defines and routines
#define NOGDICAPMASKS	  // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOHELP			  // Help engine interface.
#define NOICONS			  // IDI_*
#define NOKANJI			  // Kanji support stuff.
#define NOKEYSTATES		  // MK_*
#define NOKERNEL		  // All KERNEL defines and routines
#define NOMB			  // MB_* and MessageBox()
#define NOMCX			  // Modem Configuration Extensions
#define NOMENUS			  // MF_*
#define NOMEMMGR		  // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE		  // typedef METAFILEPICT
#define NOMSG			  // typedef MSG and associated routines
#define NOOPENFILE		  // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOPROFILER		  // Profiler interface.
#define NORASTEROPS		  // Binary and Tertiary raster ops
#define NOSCROLL		  // SB_* and scrolling routines
#define NOSERVICE		  // All Service Controller routines, SERVICE_ equates, etc.
#define NOSHOWWINDOW	  // SW_*
#define NOSOUND			  // Sound driver routines
#define NOSYSCOMMANDS	  // SC_*
#define NOSYSMETRICS	  // SM_*
#define NOTEXTMETRIC	  // typedef TEXTMETRIC and associated routines
#define NOUSER			  // All USER defines and routines
#define NOVIRTUALKEYCODES // VK_*
#define NOWH			  // SetWindowsHook and WH_*
#define NOWINOFFSETS	  // GWL_*, GCL_*, associated routines
#define NOWINMESSAGES	  // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES		  // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
//#define NOMINMAX        // Macros min(a,b) and max(a,b)
//#define NONLS           // All NLS defines and routines
#endif

// test harness stuff
#ifndef SHOULD_HAVE_FP16
#define SHOULD_HAVE_FP16 0
#endif
#ifndef SHOULD_HAVE_FLOAT16
#define SHOULD_HAVE_FLOAT16 0
#endif
#ifndef SHOULD_HAVE_FLOAT128
#define SHOULD_HAVE_FLOAT128 0
#endif
#ifndef SHOULD_HAVE_INT128
#define SHOULD_HAVE_INT128 0
#endif
#ifndef SHOULD_HAVE_EXCEPTIONS
#define SHOULD_HAVE_EXCEPTIONS 1
#endif
