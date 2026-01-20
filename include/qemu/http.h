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

#ifndef QEMU_HTTP_H
#define QEMU_HTTP_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_progress_cb_info {
    int (*progress)(struct http_progress_cb_info *progress_info);
    void *userptr;
    size_t dlnow;
    size_t dltotal;
    size_t ulnow;
    size_t ultotal;
} http_progress_cb_info;

int http_get(const char *url, GByteArray *response_body,
             http_progress_cb_info *progress_info, Error **errp);
int http_post_json(const char *url, const char *json_data, Error **errp);

#ifdef __cplusplus
}
#endif

#endif
