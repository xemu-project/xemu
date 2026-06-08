/*
 * Simple HTTP handlers
 *
 * Copyright (c) 2025 Matt Borgerson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/http.h"
#include "xemu-version.h"

#include <curl/curl.h>
#include <fcntl.h>

// Ignore SSL certificate verification (for self-signed certs)
#define ALLOW_INSECURE_HOSTS 0

static bool libcurl_init_called = false;
static bool libcurl_init_success = false;
static char *xemu_user_agent = NULL;

static void libcurl_cleanup(void)
{
    curl_global_cleanup();
    g_free(xemu_user_agent);
}

static bool ensure_libcurl_initialized(Error **errp)
{
    if (!libcurl_init_called) {
        CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
        libcurl_init_called = true;
        if (res == CURLE_OK) {
            libcurl_init_success = true;
            xemu_user_agent = g_strdup_printf("xemu/%s", xemu_version);
            atexit(libcurl_cleanup);
        }
    }

    if (!libcurl_init_success) {
        error_setg(errp, "curl_global_init failed");
    }

    return libcurl_init_success;
}

static int http_progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                            curl_off_t ultotal, curl_off_t ulnow)
{
    http_progress_cb_info *info = clientp;

    info->dlnow = dlnow;
    info->dltotal = dltotal;

    info->ulnow = ulnow;
    info->ultotal = ultotal;

    return info->progress(info);
}

static size_t http_get_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    assert(size == 1); // Per CURLOPT_WRITEFUNCTION spec
    if (userdata) {
        g_byte_array_append((GByteArray *)userdata, ptr, nmemb);
    }
    return nmemb;
}

int http_get(const char *url, GByteArray *response_body,
             http_progress_cb_info *progress_info, Error **errp)
{
    if (!ensure_libcurl_initialized(errp)) {
        return -1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        error_setg(errp, "curl_easy_init failed");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_get_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
    curl_easy_setopt(curl, CURLOPT_USERAGENT, xemu_user_agent);
#if ALLOW_INSECURE_HOSTS
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif
    if (progress_info) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, http_progress_cb);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, (void *)progress_info);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    long http_response_code = -1;

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);
    } else {
        error_setg(errp, "curl_easy_perform failed with code %d", res);
    }

    curl_easy_cleanup(curl);

    return http_response_code;
}

int http_post_json(const char *url, const char *json_data, Error **errp)
{
    if (!ensure_libcurl_initialized(errp)) {
        return -1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        error_setg(errp, "curl_easy_init failed");
        return -1;
    }

    struct curl_slist *headers =
        curl_slist_append(NULL, "Content-Type: application/json");
    if (!headers) {
        error_setg(errp, "curl_slist_append failed");
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, xemu_user_agent);
#if ALLOW_INSECURE_HOSTS
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif

    long http_response_code = -1;

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);
    } else {
        error_setg(errp, "curl_easy_perform failed with code %d", res);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return http_response_code;
}
