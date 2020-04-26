/*
 * xemu Reporting
 *
 * Title compatibility and bug report submission.
 *
 * Copyright (C) 2020 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XEMU_REPORTING_H
#define XEMU_REPORTING_H

#include <string>
#include <stdint.h>

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
	std::string memory;
	uint32_t xbe_timestamp;
	uint32_t xbe_cert_timestamp;
	uint32_t xbe_cert_title_id;
	uint32_t xbe_cert_region;
	uint32_t xbe_cert_disc_num;
	uint32_t xbe_cert_version;
	std::string compat_rating;
	std::string compat_comments;

	std::string serialized;

public:
	CompatibilityReport();
	~CompatibilityReport();
	void Send();
	const std::string &GetSerializedReport();
};

#endif
