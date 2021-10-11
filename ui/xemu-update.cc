/*
 * xemu Automatic Update
 *
 * Copyright (C) 2021 Matt Borgerson
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

#include <stdio.h>
#include <stdlib.h>
#include <SDL_filesystem.h>

#include "util/miniz/miniz.h"

#include "xemu-update.h"
#include "xemu-version.h"

#if defined(_WIN32)
const char *version_host = "raw.githubusercontent.com";
const char *version_uri = "/mborgerson/xemu/ppa-snapshot/XEMU_VERSION";
const char *download_host = "github.com";
const char *download_uri = "/mborgerson/xemu/releases/latest/download/xemu-win-release.zip";
#else
FIXME
#endif

#define CPPHTTPLIB_OPENSSL_SUPPORT 1
#include "httplib.h"

#define DPRINTF(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__);

Updater::Updater()
{
	m_status = UPDATER_IDLE;
	m_update_availability = UPDATE_AVAILABILITY_UNKNOWN;
	m_update_percentage = 0;
	m_latest_version = "Unknown";
	m_should_cancel = false;
}

void Updater::check_for_update(UpdaterCallback on_complete)
{
	if (m_status == UPDATER_IDLE || m_status == UPDATER_ERROR) {
		m_on_complete = on_complete;
		qemu_thread_create(&m_thread, "update_worker",
			               &Updater::checker_thread_worker_func,
			               this, QEMU_THREAD_JOINABLE);
	}
}

void *Updater::checker_thread_worker_func(void *updater)
{
	((Updater *)updater)->check_for_update_internal();
	return NULL;
}

void Updater::check_for_update_internal()
{
	httplib::SSLClient cli(version_host, 443);
	cli.set_follow_location(true);
	cli.set_timeout_sec(5);
	auto res = cli.Get(version_uri, [this](uint64_t len, uint64_t total) {
		m_update_percentage = len*100/total;
		return !m_should_cancel;
	});
	if (m_should_cancel) {
		m_should_cancel = false;
		m_status = UPDATER_IDLE;
		goto finished;
	} else if (!res || res->status != 200) {
		m_status = UPDATER_ERROR;
		goto finished;
	}

	if (strcmp(xemu_version, res->body.c_str())) {
		m_update_availability = UPDATE_AVAILABLE;
	} else {
		m_update_availability = UPDATE_NOT_AVAILABLE;
	}

	m_latest_version = res->body;
	m_status = UPDATER_IDLE;
finished:
	if (m_on_complete) {
		m_on_complete();
	}
}

void Updater::update()
{
	if (m_status == UPDATER_IDLE || m_status == UPDATER_ERROR) {
		m_status = UPDATER_UPDATING;
		qemu_thread_create(&m_thread, "update_worker",
			               &Updater::update_thread_worker_func,
			               this, QEMU_THREAD_JOINABLE);
	}
}

void *Updater::update_thread_worker_func(void *updater)
{
	((Updater *)updater)->update_internal();
	return NULL;
}

void Updater::update_internal()
{
	httplib::SSLClient cli(download_host, 443);
	cli.set_follow_location(true);
	cli.set_timeout_sec(5);
	auto res = cli.Get(download_uri, [this](uint64_t len, uint64_t total) {
		m_update_percentage = len*100/total;
		return !m_should_cancel;
	});

	if (m_should_cancel) {
		m_should_cancel = false;
		m_status = UPDATER_IDLE;
		return;
	} else if (!res || res->status != 200) {
		m_status = UPDATER_ERROR;
		return;
	}

	mz_zip_archive zip;
	mz_zip_zero_struct(&zip);
	if (!mz_zip_reader_init_mem(&zip, res->body.data(), res->body.size(), 0)) {
		DPRINTF("mz_zip_reader_init_mem failed\n");
		m_status = UPDATER_ERROR;
		return;
	}

	mz_uint num_files = mz_zip_reader_get_num_files(&zip);
	for (mz_uint file_idx = 0; file_idx < num_files; file_idx++) {
		mz_zip_archive_file_stat fstat;
		if (!mz_zip_reader_file_stat(&zip, file_idx, &fstat)) {
			DPRINTF("mz_zip_reader_file_stat failed for file #%d\n", file_idx);
			goto errored;
		}

		if (fstat.m_filename[strlen(fstat.m_filename)-1] == '/') {
			/* FIXME: mkdirs */
			DPRINTF("FIXME: subdirs not handled yet\n");
			goto errored;
		}

		char *dst_path = g_strdup_printf("%s%s", SDL_GetBasePath(), fstat.m_filename);
		DPRINTF("extracting %s to %s\n", fstat.m_filename, dst_path);

		if (!strcmp(fstat.m_filename, "xemu.exe")) {
			// We cannot overwrite current executable, but we can move it
			char *renamed_path = g_strdup_printf("%s%s", SDL_GetBasePath(), "xemu-previous.exe");
			MoveFileExA(dst_path, renamed_path, MOVEFILE_REPLACE_EXISTING);
			g_free(renamed_path);
		}

		if (!mz_zip_reader_extract_to_file(&zip, file_idx, dst_path, 0)) {
			DPRINTF("mz_zip_reader_extract_to_file failed to create %s\n", dst_path);
			g_free(dst_path);
			goto errored;
		}

		g_free(dst_path);
	}

	m_status = UPDATER_UPDATE_SUCCESSFUL;
	goto cleanup_zip;
errored:
	m_status = UPDATER_ERROR;
cleanup_zip:
	mz_zip_reader_end(&zip);
}

extern "C" {
extern char **gArgv;
}

void Updater::restart_to_updated()
{
	char *target_exec = g_strdup_printf("%s%s", SDL_GetBasePath(), "xemu.exe");
	DPRINTF("Restarting to updated executable %s\n", target_exec);
	_execv(target_exec, gArgv);
	DPRINTF("Launching updated executable failed\n");
	exit(1);
}
