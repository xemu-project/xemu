/*
 * Adapted from the Wireshark project
 *
 * Routines supporting the use of Windows friendly interface names within Wireshark
 * Copyright 2011-2012, Mike Garratt <wireshark@evn.co.nz>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifdef _WIN32

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include <ntddndis.h>
#include <netioapi.h>

#ifndef NDIS_IF_MAX_STRING_SIZE
#define NDIS_IF_MAX_STRING_SIZE IF_MAX_STRING_SIZE   /* =256 in <ifdef.h> */
#endif

#ifndef NETIO_STATUS
#define NETIO_STATUS DWORD
#endif

#include "capture_win_ifnames.h"

static int gethexdigit(const char *p)
{
    if(*p >= '0' && *p <= '9'){
        return *p - '0';
    }else if(*p >= 'A' && *p <= 'F'){
        return *p - 'A' + 0xA;
    }else if(*p >= 'a' && *p <= 'f'){
        return *p - 'a' + 0xa;
    }else{
        return -1; /* Not a hex digit */
    }
}

static gboolean get8hexdigits(const char *p, DWORD *d)
{
    int digit;
    DWORD val;
    int i;

    val = 0;
    for(i = 0; i < 8; i++){
        digit = gethexdigit(p++);
        if(digit == -1){
            return FALSE; /* Not a hex digit */
        }
        val = (val << 4) | digit;
    }
    *d = val;
    return TRUE;
}

static gboolean get4hexdigits(const char *p, WORD *w)
{
    int digit;
    WORD val;
    int i;

    val = 0;
    for(i = 0; i < 4; i++){
        digit = gethexdigit(p++);
        if(digit == -1){
            return FALSE; /* Not a hex digit */
        }
        val = (val << 4) | digit;
    }
    *w = val;
    return TRUE;
}

/*
 * If a string is a GUID in {}, fill in a GUID structure with the GUID
 * value and return TRUE; otherwise, if the string is not a valid GUID
 * in {}, return FALSE.
 */
gboolean
parse_as_guid(const char *guid_text, GUID *guid)
{
    int i;
    int digit1, digit2;

    if(*guid_text != '{'){
        return FALSE; /* Nope, not enclosed in {} */
    }
    guid_text++;
    /* There must be 8 hex digits; if so, they go into guid->Data1 */
    if(!get8hexdigits(guid_text, &guid->Data1)){
        return FALSE; /* nope, not 8 hex digits */
    }
    guid_text += 8;
    /* Now there must be a hyphen */
    if(*guid_text != '-'){
        return FALSE; /* Nope */
    }
    guid_text++;
    /* There must be 4 hex digits; if so, they go into guid->Data2 */
    if(!get4hexdigits(guid_text, &guid->Data2)){
        return FALSE; /* nope, not 4 hex digits */
    }
    guid_text += 4;
    /* Now there must be a hyphen */
    if(*guid_text != '-'){
        return FALSE; /* Nope */
    }
    guid_text++;
    /* There must be 4 hex digits; if so, they go into guid->Data3 */
    if(!get4hexdigits(guid_text, &guid->Data3)){
        return FALSE; /* nope, not 4 hex digits */
    }
    guid_text += 4;
    /* Now there must be a hyphen */
    if(*guid_text != '-'){
        return FALSE; /* Nope */
    }
    guid_text++;
    /*
     * There must be 4 hex digits; if so, they go into the first 2 bytes
     * of guid->Data4.
     */
    for(i = 0; i < 2; i++){
        digit1 = gethexdigit(guid_text);
        if(digit1 == -1){
            return FALSE; /* Not a hex digit */
        }
        guid_text++;
        digit2 = gethexdigit(guid_text);
        if(digit2 == -1){
            return FALSE; /* Not a hex digit */
        }
        guid_text++;
        guid->Data4[i] = (digit1 << 4)|(digit2);
    }
    /* Now there must be a hyphen */
    if(*guid_text != '-'){
        return FALSE; /* Nope */
    }
    guid_text++;
    /*
     * There must be 12 hex digits; if so,t hey go into the next 6 bytes
     * of guid->Data4.
     */
    for(i = 0; i < 6; i++){
        digit1 = gethexdigit(guid_text);
        if(digit1 == -1){
            return FALSE; /* Not a hex digit */
        }
        guid_text++;
        digit2 = gethexdigit(guid_text);
        if(digit2 == -1){
            return FALSE; /* Not a hex digit */
        }
        guid_text++;
        guid->Data4[i+2] = (digit1 << 4)|(digit2);
    }
    /* Now there must be a closing } */
    if(*guid_text != '}'){
        return FALSE; /* Nope */
    }
    guid_text++;
    /* And that must be the end of the string */
    if(*guid_text != '\0'){
        return FALSE; /* Nope */
    }
    return TRUE;
}

/**********************************************************************************/
/* Get the friendly name for the given GUID */
char *
get_interface_friendly_name_from_device_guid(__in GUID *guid)
{
    HRESULT hr;

    /* Need to convert an Interface GUID to the interface friendly name (e.g. "Local Area Connection")
    * The functions required to do this all reside within iphlpapi.dll
    */

    NET_LUID InterfaceLuid;
    hr = ConvertInterfaceGuidToLuid(guid, &InterfaceLuid);
    if(hr == NO_ERROR) {
        /* guid->luid success */
        WCHAR wName[NDIS_IF_MAX_STRING_SIZE + 1];
        hr = ConvertInterfaceLuidToAlias(&InterfaceLuid, wName, NDIS_IF_MAX_STRING_SIZE+1);
        if(hr == NO_ERROR) {
            /* luid->friendly name success */

            /* Get the required buffer size, and then convert the string
            * from UTF-16 to UTF-8. */
            int size;
            char *name;
            size = WideCharToMultiByte(CP_UTF8, 0, wName, -1, NULL, 0, NULL, NULL);
            if(size != 0) {
                name = (char *) g_malloc(size);
                if (name != NULL) {
                    size = WideCharToMultiByte(CP_UTF8, 0, wName, -1, name, size, NULL, NULL);
                    if(size != 0) {
                        return name;
                    }
                    /* Failed, clean up the allocation */
                    g_free(name);
                }
            }
        }
    }

    /* Failed to get a name */
    return NULL;
}

/*
 * Given an interface name, try to extract the GUID from it and parse it.
 * If that fails, return NULL; if that succeeds, attempt to get the
 * friendly name for the interface in question.  If that fails, return
 * NULL, otherwise return the friendly name, allocated with g_malloc()
 * (so that it must be freed with g_free()).
 */
char *
get_windows_interface_friendly_name(const char *interface_devicename)
{
    const char* guid_text;
    GUID guid;

    /* Extract the guid text from the interface device name */
    if(strncmp("\\Device\\NPF_", interface_devicename, 12)==0){
        guid_text=interface_devicename+12; /* skip over the '\Device\NPF_' prefix, assume the rest is the guid text */
    }else{
        guid_text=interface_devicename;
    }

    if (!parse_as_guid(guid_text, &guid)){
        return NULL; /* not a GUID, so no friendly name */
    }

    /* guid okay, get the interface friendly name associated with the guid */
    return get_interface_friendly_name_from_device_guid(&guid);
}

/**************************************************************************************/
#endif
