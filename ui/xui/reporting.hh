//
// xemu Reporting
//
// Title compatibility and bug report submission.
//
// Copyright (C) 2020-2021 Matt Borgerson
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
#pragma once

#include <string>
#include <stdint.h>

#include "xemu-xbe.h"

class CompatibilityReport {
public:
	std::string token;
	std::string xemu_version;
	std::string xemu_branch;
	std::string xemu_commit;
	std::string xemu_date;
	std::string os_platform;
	std::string os_version;
	std::string cpu;
	std::string gl_vendor;
	std::string gl_renderer;
	std::string gl_version;
	std::string gl_shading_language_version;
	std::string compat_rating;
	std::string compat_comments;
	std::string xbe_headers;

private:
	std::string serialized;
	int result_code;
	std::string result_msg;

public:
	CompatibilityReport();
	~CompatibilityReport();
	bool Send();
	int GetResultCode() { return result_code; }
	std::string &GetResultMessage() { return result_msg; }
	const std::string &GetSerializedReport();
	void SetXbeData(struct xbe *xbe);
};
