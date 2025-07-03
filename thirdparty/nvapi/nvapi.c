/*
 * NVIDIA Application Profile Management
 *
 * Based on implementation from Godot Engine:
 *
 * Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). 
 * Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  
 *                                                                        
 * Permission is hereby granted, free of charge, to any person obtaining  
 * a copy of this software and associated documentation files (the        
 * "Software"), to deal in the Software without restriction, including    
 * without limitation the rights to use, copy, modify, merge, publish,    
 * distribute, sublicense, and/or sell copies of the Software, and to     
 * permit persons to whom the Software is furnished to do so, subject to  
 * the following conditions:                                              
 *                                                                        
 * The above copyright notice and this permission notice shall be         
 * included in all copies or substantial portions of the Software.        
 *                                                                        
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 
 */
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvapi.h"
#include "nvapi_defs.h"

const int OGL_THREAD_CONTROL_ID = 0x20C1221E;
const int OGL_THREAD_CONTROL_DISABLE = 0x00000002;
const int OGL_THREAD_CONTROL_ENABLE = 0x00000001;
const int VRR_MODE_ID = 0x1194F158;
const int VRR_MODE_FULLSCREEN_ONLY = 0x1;

typedef int(__cdecl *NvAPI_Initialize_t)(void);
typedef int(__cdecl *NvAPI_Unload_t)(void);
typedef int(__cdecl *NvAPI_GetErrorMessage_t)(unsigned int, NvAPI_ShortString);
typedef int(__cdecl *NvAPI_DRS_CreateSession_t)(NvDRSSessionHandle *);
typedef int(__cdecl *NvAPI_DRS_DestroySession_t)(NvDRSSessionHandle);
typedef int(__cdecl *NvAPI_DRS_LoadSettings_t)(NvDRSSessionHandle);
typedef int(__cdecl *NvAPI_DRS_CreateProfile_t)(NvDRSSessionHandle, NVDRS_PROFILE *, NvDRSProfileHandle *);
typedef int(__cdecl *NvAPI_DRS_CreateApplication_t)(NvDRSSessionHandle, NvDRSProfileHandle, NVDRS_APPLICATION *);
typedef int(__cdecl *NvAPI_DRS_SaveSettings_t)(NvDRSSessionHandle);
typedef int(__cdecl *NvAPI_DRS_SetSetting_t)(NvDRSSessionHandle, NvDRSProfileHandle, NVDRS_SETTING *);
typedef int(__cdecl *NvAPI_DRS_FindProfileByName_t)(NvDRSSessionHandle, NvAPI_UnicodeString, NvDRSProfileHandle *);
typedef int(__cdecl *NvAPI_DRS_GetApplicationInfo_t)(NvDRSSessionHandle, NvDRSProfileHandle, NvAPI_UnicodeString, NVDRS_APPLICATION *);
typedef int(__cdecl *NvAPI_DRS_DeleteProfile_t)(NvDRSSessionHandle, NvDRSProfileHandle);
NvAPI_GetErrorMessage_t NvAPI_GetErrorMessage__;

static bool nvapi_err_check(const char *msg, int status)
{
	if (status != 0) {
		fprintf(stderr, "%s: %s (%d)\n", __func__, msg, status);
		return false;
	}
	return true;
}

static void print_verbose(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
}

// On windows we have to customize the NVIDIA application profile:
// * disable threaded optimization when using NVIDIA cards to avoid stuttering, see
//   https://stackoverflow.com/questions/36959508/nvidia-graphics-driver-causing-noticeable-frame-stuttering/37632948
//   https://github.com/Ryujinx/Ryujinx/blob/master/src/Ryujinx.Common/GraphicsDriver/NVThreadedOptimization.cs
// * disable G-SYNC in windowed mode, as it results in unstable editor refresh rates
void nvapi_setup_profile(NvApiProfileOpts opts)
{
	HMODULE nvapi = NULL;
#ifdef _WIN64
	nvapi = LoadLibraryA("nvapi64.dll");
#else
	nvapi = LoadLibraryA("nvapi.dll");
#endif

	if (nvapi == NULL) {
		print_verbose("NVAPI: nvapi.dll not found");
		return;
	}

	void *(__cdecl * NvAPI_QueryInterface)(unsigned int interface_id) = NULL;

	NvAPI_QueryInterface = (void *(__cdecl *)(unsigned int))(void *)GetProcAddress(nvapi, "nvapi_QueryInterface");

	if (NvAPI_QueryInterface == NULL) {
		print_verbose("Error getting NVAPI NvAPI_QueryInterface");
		return;
	}

	// Setup NVAPI function pointers
	NvAPI_Initialize_t NvAPI_Initialize = (NvAPI_Initialize_t)NvAPI_QueryInterface(0x0150E828);
	NvAPI_GetErrorMessage__ = (NvAPI_GetErrorMessage_t)NvAPI_QueryInterface(0x6C2D048C);
	NvAPI_DRS_CreateSession_t NvAPI_DRS_CreateSession = (NvAPI_DRS_CreateSession_t)NvAPI_QueryInterface(0x0694D52E);
	NvAPI_DRS_DestroySession_t NvAPI_DRS_DestroySession = (NvAPI_DRS_DestroySession_t)NvAPI_QueryInterface(0xDAD9CFF8);
	NvAPI_Unload_t NvAPI_Unload = (NvAPI_Unload_t)NvAPI_QueryInterface(0xD22BDD7E);
	NvAPI_DRS_LoadSettings_t NvAPI_DRS_LoadSettings = (NvAPI_DRS_LoadSettings_t)NvAPI_QueryInterface(0x375DBD6B);
	NvAPI_DRS_CreateProfile_t NvAPI_DRS_CreateProfile = (NvAPI_DRS_CreateProfile_t)NvAPI_QueryInterface(0xCC176068);
	NvAPI_DRS_CreateApplication_t NvAPI_DRS_CreateApplication = (NvAPI_DRS_CreateApplication_t)NvAPI_QueryInterface(0x4347A9DE);
	NvAPI_DRS_SaveSettings_t NvAPI_DRS_SaveSettings = (NvAPI_DRS_SaveSettings_t)NvAPI_QueryInterface(0xFCBC7E14);
	NvAPI_DRS_SetSetting_t NvAPI_DRS_SetSetting = (NvAPI_DRS_SetSetting_t)NvAPI_QueryInterface(0x577DD202);
	NvAPI_DRS_FindProfileByName_t NvAPI_DRS_FindProfileByName = (NvAPI_DRS_FindProfileByName_t)NvAPI_QueryInterface(0x7E4A9A0B);
	NvAPI_DRS_GetApplicationInfo_t NvAPI_DRS_GetApplicationInfo = (NvAPI_DRS_GetApplicationInfo_t)NvAPI_QueryInterface(0xED1F8C69);
#if 0
	NvAPI_DRS_DeleteProfile_t NvAPI_DRS_DeleteProfile = (NvAPI_DRS_DeleteProfile_t)NvAPI_QueryInterface(0x17093206);
#endif

	if (!nvapi_err_check("NVAPI: Init failed", NvAPI_Initialize())) {
		return;
	}

	print_verbose("NVAPI: Init OK!");

	NvDRSSessionHandle session_handle;

	if (NvAPI_DRS_CreateSession == NULL) {
		return;
	}

	if (!nvapi_err_check("NVAPI: Error creating DRS session", NvAPI_DRS_CreateSession(&session_handle))) {
		NvAPI_Unload();
		return;
	}

	if (!nvapi_err_check("NVAPI: Error loading DRS settings", NvAPI_DRS_LoadSettings(session_handle))) {
		NvAPI_DRS_DestroySession(session_handle);
		NvAPI_Unload();
		return;
	}

#if 0
	// A previous error in app creation logic could result in invalid profiles,
	// clean these if they exist before proceeding.
	NvDRSProfileHandle old_profile_handle;

	int old_status = NvAPI_DRS_FindProfileByName(session_handle, (NvU16 *)opts.profile_name, &old_profile_handle);

	if (old_status == 0) {
		print_verbose("NVAPI: Deleting old profile...");

		if (!nvapi_err_check("NVAPI: Error deleting old profile", NvAPI_DRS_DeleteProfile(session_handle, old_profile_handle))) {
			NvAPI_DRS_DestroySession(session_handle);
			NvAPI_Unload();
			return;
		}

		if (!nvapi_err_check("NVAPI: Error deleting old profile", NvAPI_DRS_SaveSettings(session_handle))) {
			NvAPI_DRS_DestroySession(session_handle);
			NvAPI_Unload();
			return;
		}
	}
#endif

	NvDRSProfileHandle profile_handle = NULL;

	int profile_status = NvAPI_DRS_FindProfileByName(session_handle, (NvU16 *)opts.profile_name, &profile_handle);

	if (profile_status != 0) {
		print_verbose("NVAPI: Profile not found, creating...");

		NVDRS_PROFILE profile_info;
		profile_info.version = NVDRS_PROFILE_VER;
		profile_info.isPredefined = 0;
		wcsncpy(profile_info.profileName, opts.profile_name, sizeof(profile_info.profileName) / sizeof(wchar_t));

		if (!nvapi_err_check("NVAPI: Error creating profile", NvAPI_DRS_CreateProfile(session_handle, &profile_info, &profile_handle))) {
			NvAPI_DRS_DestroySession(session_handle);
			NvAPI_Unload();
			return;
		}
	}

	NVDRS_APPLICATION_V4 app;
	app.version = NVDRS_APPLICATION_VER_V4;

	int app_status = NvAPI_DRS_GetApplicationInfo(session_handle, profile_handle, (NvU16 *)opts.executable_name, &app);

	if (app_status != 0) {
		print_verbose("NVAPI: Application not found in profile, creating...");

		app.isPredefined = 0;
		wcsncpy(app.appName, opts.executable_name, sizeof(app.appName) / sizeof(wchar_t));
		memcpy(app.launcher, L"", sizeof(wchar_t));
		memcpy(app.fileInFolder, L"", sizeof(wchar_t));

		if (!nvapi_err_check("NVAPI: Error creating application", NvAPI_DRS_CreateApplication(session_handle, profile_handle, &app))) {
			NvAPI_DRS_DestroySession(session_handle);
			NvAPI_Unload();
			return;
		}
	}

	NVDRS_SETTING ogl_thread_control_setting = {};
	ogl_thread_control_setting.version = NVDRS_SETTING_VER;
	ogl_thread_control_setting.settingId = OGL_THREAD_CONTROL_ID;
	ogl_thread_control_setting.settingType = NVDRS_DWORD_TYPE;
	ogl_thread_control_setting.u32CurrentValue = opts.threaded_optimizations ? OGL_THREAD_CONTROL_ENABLE : OGL_THREAD_CONTROL_DISABLE;

	if (!nvapi_err_check("NVAPI: Error calling NvAPI_DRS_SetSetting", NvAPI_DRS_SetSetting(session_handle, profile_handle, &ogl_thread_control_setting))) {
		NvAPI_DRS_DestroySession(session_handle);
		NvAPI_Unload();
		return;
	}

	NVDRS_SETTING vrr_mode_setting = {};
	vrr_mode_setting.version = NVDRS_SETTING_VER;
	vrr_mode_setting.settingId = VRR_MODE_ID;
	vrr_mode_setting.settingType = NVDRS_DWORD_TYPE;
	vrr_mode_setting.u32CurrentValue = VRR_MODE_FULLSCREEN_ONLY;

	if (!nvapi_err_check("NVAPI: Error calling NvAPI_DRS_SetSetting", NvAPI_DRS_SetSetting(session_handle, profile_handle, &vrr_mode_setting))) {
		NvAPI_DRS_DestroySession(session_handle);
		NvAPI_Unload();
		return;
	}

	if (!nvapi_err_check("NVAPI: Error saving settings", NvAPI_DRS_SaveSettings(session_handle))) {
		NvAPI_DRS_DestroySession(session_handle);
		NvAPI_Unload();
		return;
	}

	if (ogl_thread_control_setting.u32CurrentValue == OGL_THREAD_CONTROL_DISABLE) {
		print_verbose("NVAPI: Disabled OpenGL threaded optimization successfully");
	} else {
		print_verbose("NVAPI: Enabled OpenGL threaded optimization successfully");
	}
	print_verbose("NVAPI: Disabled G-SYNC for windowed mode successfully");

	NvAPI_DRS_DestroySession(session_handle);
}
