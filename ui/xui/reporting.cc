//
// xemu Reporting
//
// Title compatibility and bug report submission.
//
// Copyright (C) 2020-2025 Matt Borgerson
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "qemu/osdep.h"
#include "qemu/http.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include "reporting.hh"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

static const char *compat_report_endpoint_url = "https://reports.xemu.app/compatibility";

CompatibilityReport::CompatibilityReport()
{
}

CompatibilityReport::~CompatibilityReport()
{
}

const std::string &CompatibilityReport::GetSerializedReport()
{
	json report = {
		{"token", token},
		{"xemu_version", xemu_version},
		{"xemu_branch", ""},
		{"xemu_commit", xemu_commit},
		{"xemu_date", xemu_date},
		{"os_platform", os_platform},
		{"os_version", os_version},
		{"cpu", cpu},
		{"gl_vendor", gl_vendor},
		{"gl_renderer", gl_renderer},
		{"gl_version", gl_version},
		{"gl_shading_language_version", gl_shading_language_version},
		{"compat_rating", compat_rating},
		{"compat_comments", compat_comments},
		{"xbe_headers", xbe_headers},
	};
	serialized = report.dump(2);
	return serialized;
}

bool CompatibilityReport::Send()
{
	const std::string &s = GetSerializedReport();

	int res = http_post_json(compat_report_endpoint_url, s.c_str(), NULL);
	if (res < 0) {
		result_code = -1;
		result_msg = "Failed to connect";
	    return false;
	}

	switch(res) {
	case 200:
		result_msg = "Ok";
		return true;
	case 400:
	case 411:
		result_msg = "Invalid request";
		return false;
	case 403:
		result_msg = "Invalid token";
		return false;
	case 409:
		result_msg = "Please upgrade to latest version";
		return false;
	case 413:
		result_msg = "Report too long";
		return false;
	default:
		result_msg = "Unknown error occurred";
		return false;
	}
}

void CompatibilityReport::SetXbeData(struct xbe *xbe)
{
	assert(xbe != NULL);
	assert(xbe->headers != NULL);
	assert(xbe->headers_len > 0);

    // base64 encode all XBE headers to be sent with the report
    gchar *buf = g_base64_encode(xbe->headers, xbe->headers_len);
    xbe_headers = buf;
    g_free(buf);
}
