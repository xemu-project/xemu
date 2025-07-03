/*
 * NVIDIA Application Profile Management
 */
#ifndef NVAPI_H
#define NVAPI_H

#include <windows.h>
#include <stdbool.h>

typedef struct NvApiProfileOpts {
    const wchar_t *executable_name;
    const wchar_t *profile_name;
    bool threaded_optimizations;
} NvApiProfileOpts;

void nvapi_setup_profile(NvApiProfileOpts opts);

#endif
