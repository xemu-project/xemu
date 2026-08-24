//
// xemu Memory Viewer / Search / x86 Debugger
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "memory-tools.hh"
#include "memory-tools-internal.hh"
#include "current-game.hh"
#include "cheat-engine-memory.h"
#include "cheat-engine.hh"
#include "../font-manager.hh"
#include "../misc.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

#include <glib.h>
#include <glib/gstdio.h>

using namespace xemu_memory_tools_internal;

std::string MemoryToolsWindow::DumpDirectory() const
{
    char executable_dir[4096] = {};
    if (!xemu_cheat_get_executable_dir(executable_dir, sizeof(executable_dir))) {
        return {};
    }

    gchar *path = g_build_filename(executable_dir, "Ram-Dumps", nullptr);
    std::string result = path ? path : std::string();
    g_free(path);
    return result;
}

std::string MemoryToolsWindow::DumpStem() const
{
    const auto &game = current_game_manager.Get();
    std::string stem;
    if (game.valid) {
        stem = CurrentGameManager::FormatDatabaseGameId(game.title_id);
        if (!game.header_sha256.empty()) {
            stem += "-";
            stem += game.header_sha256.substr(0, std::min<size_t>(16, game.header_sha256.size()));
        }
    } else {
        stem = "NO-XBE";
    }
    stem += "-";
    stem += make_dump_timestamp();
    return stem;
}

void MemoryToolsWindow::DumpCurrentPage(AddressSpace space, uint32_t address)
{
    XemuDebugGuestPauseGuard guest_pause;

    const std::string directory = DumpDirectory();
    if (directory.empty()) {
        m_dump_status = "Could not determine the xemu RAM dump directory.";
        return;
    }
    if (g_mkdir_with_parents(directory.c_str(), 0755) != 0) {
        m_dump_status = "Could not create RAM dump directory: " + directory;
        return;
    }

    const uint32_t page_base = address & 0xFFFFF000u;
    char filename[192];
    std::snprintf(filename, sizeof(filename), "%s-%s-%08X-PAGE.bin",
                  DumpStem().c_str(),
                  space == AddressSpace::Virtual ? "VIRTUAL" : "PHYSICAL",
                  page_base);
    gchar *path_c = g_build_filename(directory.c_str(), filename, nullptr);
    const std::string path = path_c ? path_c : filename;
    g_free(path_c);

    size_t failed_pages = 0;
    const bool ok = DumpRange(space, page_base, kPageSize, path, failed_pages);

    char message[512];
    if (ok && failed_pages == 0) {
        std::snprintf(message, sizeof(message),
                      "Current %s page %08X-%08X dumped to: %s",
                      space == AddressSpace::Virtual ? "Virtual" : "Physical",
                      page_base, page_base + 0xFFFu, path.c_str());
    } else if (ok) {
        std::snprintf(message, sizeof(message),
                      "Current %s page %08X-%08X dumped with unreadable data zero-filled: %s",
                      space == AddressSpace::Virtual ? "Virtual" : "Physical",
                      page_base, page_base + 0xFFFu, path.c_str());
    } else {
        std::snprintf(message, sizeof(message),
                      "Failed to dump current %s page %08X-%08X.",
                      space == AddressSpace::Virtual ? "Virtual" : "Physical",
                      page_base, page_base + 0xFFFu);
    }
    m_dump_status = message;

}

bool MemoryToolsWindow::DumpRange(AddressSpace space, uint32_t base,
                                  uint64_t size, const std::string &path,
                                  size_t &failed_pages) const
{
    failed_pages = 0;
    FILE *fp = g_fopen(path.c_str(), "wb");
    if (!fp) {
        return false;
    }

    constexpr size_t kDumpPage = 0x1000;
    uint8_t buffer[kDumpPage];
    bool file_ok = true;

    for (uint64_t offset = 0; offset < size; offset += kDumpPage) {
        const size_t amount = (size_t)std::min<uint64_t>(kDumpPage, size - offset);
        const uint32_t address = base + (uint32_t)offset;
        if (!Read(space, address, buffer, amount)) {
            std::memset(buffer, 0, amount);
            ++failed_pages;
        }
        if (std::fwrite(buffer, 1, amount, fp) != amount) {
            file_ok = false;
            break;
        }
    }

    if (std::fclose(fp) != 0) {
        file_ok = false;
    }
    if (!file_ok) {
        g_remove(path.c_str());
    }
    return file_ok;
}

bool MemoryToolsWindow::ScanMappedVirtualRam(
    uint64_t ram_size, std::vector<VirtualDumpRegion> &regions,
    uint64_t &mapped_pages) const
{
    constexpr uint64_t kVirtualPage = 0x1000ull;

    regions.clear();
    mapped_pages = 0;

    std::vector<VirtualPageMapping> pages;
    bool used_page_table_snapshot = false;
    if (!CollectRamVirtualPages(ram_size, pages, used_page_table_snapshot)) {
        return false;
    }
    (void)used_page_table_snapshot;

    for (const auto &page : pages) {
        ++mapped_pages;
        const uint64_t virtual_address = page.virtual_page;
        if (!regions.empty() &&
            regions.back().end_exclusive == virtual_address) {
            regions.back().end_exclusive += kVirtualPage;
        } else {
            VirtualDumpRegion region;
            region.start = virtual_address;
            region.end_exclusive = virtual_address + kVirtualPage;
            regions.push_back(region);
        }
    }
    return true;
}

bool MemoryToolsWindow::WriteVirtualMapIndex(
    const std::string &path, uint64_t ram_size,
    const std::vector<VirtualDumpRegion> &regions,
    uint64_t mapped_pages,
    const std::vector<std::string> &region_files) const
{
    FILE *fp = g_fopen(path.c_str(), "wb");
    if (!fp) {
        return false;
    }

    const auto &game = current_game_manager.Get();
    bool ok = true;
    auto put = [&](const char *format, auto... args) {
        if (ok && std::fprintf(fp, format, args...) < 0) {
            ok = false;
        }
    };

    put("xemu Mapped Virtual RAM Dump\n");
    put("============================\n\n");
    if (game.valid) {
        const std::string game_id = CurrentGameManager::FormatDatabaseGameId(game.title_id);
        put("Game: %s\n", game.title_name.c_str());
        put("GameID: %s\n", game_id.c_str());
        put("TitleID: %08X\n", game.title_id);
        put("XBE Header SHA-256: %s\n", game.header_sha256.c_str());
    } else {
        put("Game: <no valid XBE detected>\n");
    }
    put("Installed physical RAM: %llu bytes (%llu MB)\n",
        (unsigned long long)ram_size,
        (unsigned long long)(ram_size / (1024ull * 1024ull)));
    put("Virtual scan range: 00000000-FFFFFFFF\n");
    put("Page size: 00001000\n");
    put("RAM-backed mapped pages: %llu\n",
        (unsigned long long)mapped_pages);
    put("RAM-backed mapped bytes (aliases included): %llu\n",
        (unsigned long long)(mapped_pages * 0x1000ull));
    put("Contiguous virtual regions: %zu\n\n", regions.size());
    put("Each .bin starts at file offset 00000000. Add the region's virtual\n");
    put("start address to a file offset to recover the guest virtual address.\n");
    put("Only mappings backed by installed Xbox RAM are included; MMIO/device\n");
    put("mappings are intentionally excluded. Physical pages inside a virtual\n");
    put("region are not required to be physically contiguous.\n\n");

    put("#   Virtual Start-End       Size (bytes)       File\n");
    put("--  ---------------------   ----------------   ----\n");
    for (size_t i = 0; i < regions.size(); ++i) {
        const uint64_t size = regions[i].end_exclusive - regions[i].start;
        const uint64_t end_inclusive = regions[i].end_exclusive - 1;
        const char *file = i < region_files.size() ? region_files[i].c_str() : "<missing>";
        put("%02zu  %08llX-%08llX   %016llX   %s\n",
            i + 1,
            (unsigned long long)regions[i].start,
            (unsigned long long)end_inclusive,
            (unsigned long long)size,
            file);
    }

    if (std::fclose(fp) != 0) {
        ok = false;
    }
    if (!ok) {
        g_remove(path.c_str());
    }
    return ok;
}

void MemoryToolsWindow::DumpPhysicalRam()
{
    DumpRam(true, false);
}

void MemoryToolsWindow::DumpMappedVirtualRam()
{
    DumpRam(false, true);
}

void MemoryToolsWindow::DumpFullRam()
{
    DumpRam(true, true);
}

void MemoryToolsWindow::DumpRam(bool dump_physical, bool dump_virtual)
{
    if (!dump_physical && !dump_virtual) {
        m_dump_status = "No RAM dump mode was selected.";
        return;
    }

    // Full-range dumps are intentionally taken from a stopped guest. This
    // keeps a combined physical + virtual dump internally consistent and also
    // makes the individual modes deterministic for debugger inspection. Leave
    // the VM paused afterward, matching the existing full-dump behavior.
    if (runstate_is_running()) {
        vm_stop(RUN_STATE_PAUSED);
    }

    const uint64_t ram_size = xemu_cheat_ram_size();
    if (ram_size == 0 || ram_size > 0x40000000ull) {
        m_dump_status = "Could not determine a valid Xbox RAM size.";
        return;
    }
    if (dump_virtual && !xemu_cheat_cpu_available()) {
        m_dump_status = "Xbox CPU is not available; mapped virtual RAM cannot be scanned. VM remains paused.";
        return;
    }

    const std::string directory = DumpDirectory();
    if (directory.empty()) {
        m_dump_status = "Could not determine the xemu executable directory.";
        return;
    }
    if (g_mkdir_with_parents(directory.c_str(), 0755) != 0) {
        m_dump_status = "Could not create RAM dump directory: " + directory;
        return;
    }

    std::vector<VirtualDumpRegion> regions;
    uint64_t mapped_pages = 0;
    if (dump_virtual && !ScanMappedVirtualRam(ram_size, regions, mapped_pages)) {
        m_dump_status = "Could not synchronize the Xbox CPU/page tables for virtual RAM mapping. VM remains paused.";
        return;
    }

    const std::string stem = DumpStem();

    bool physical_ok = true;
    size_t physical_failed = 0;
    if (dump_physical) {
        gchar *physical_c = g_build_filename(
            directory.c_str(),
            (stem + "-PHYSICAL-00000000.bin").c_str(), nullptr);
        const std::string physical_path = physical_c ? physical_c : "physical.bin";
        g_free(physical_c);

        physical_ok = DumpRange(AddressSpace::Physical, 0x00000000u,
                                ram_size, physical_path, physical_failed);
    }

    bool virtual_ok = true;
    bool map_ok = true;
    size_t virtual_failed = 0;
    std::vector<std::string> region_files;
    if (dump_virtual) {
        region_files.reserve(regions.size());
        for (const VirtualDumpRegion &region : regions) {
            const uint64_t end_inclusive = region.end_exclusive - 1;
            char filename[160];
            std::snprintf(filename, sizeof(filename),
                          "%s-VIRTUAL-%08llX-%08llX.bin",
                          stem.c_str(),
                          (unsigned long long)region.start,
                          (unsigned long long)end_inclusive);
            gchar *path_c = g_build_filename(directory.c_str(), filename, nullptr);
            const std::string path = path_c ? path_c : filename;
            g_free(path_c);

            size_t failed_pages = 0;
            const uint64_t region_size = region.end_exclusive - region.start;
            const bool region_ok = DumpRange(AddressSpace::Virtual,
                                             (uint32_t)region.start,
                                             region_size, path, failed_pages);
            virtual_failed += failed_pages;
            virtual_ok = virtual_ok && region_ok;
            region_files.emplace_back(filename);
        }

        gchar *map_c = g_build_filename(
            directory.c_str(),
            (stem + "-VIRTUAL-MAP.txt").c_str(), nullptr);
        const std::string map_path = map_c ? map_c : "virtual-map.txt";
        g_free(map_c);
        map_ok = WriteVirtualMapIndex(map_path, ram_size, regions,
                                      mapped_pages, region_files);
    }

    const bool ok = physical_ok && virtual_ok && map_ok;
    if (!ok) {
        if (dump_physical && dump_virtual) {
            m_dump_status = "Physical + mapped virtual RAM dump was only partially completed. VM remains paused. Check the output folder: " + directory;
        } else if (dump_physical) {
            m_dump_status = "Physical RAM dump failed or was only partially completed. VM remains paused. Check the output folder: " + directory;
        } else {
            m_dump_status = "Mapped virtual RAM dump failed or was only partially completed. VM remains paused. Check the output folder: " + directory;
        }
        return;
    }

    char message[640];
    if (dump_physical && dump_virtual) {
        std::snprintf(message, sizeof(message),
                      "Physical + mapped virtual RAM dump complete. VM remains paused. "
                      "Physical RAM: %llu MB. Mapped virtual RAM: %llu pages in %zu region(s), "
                      "%llu MB including aliases. Physical unreadable pages: %zu; "
                      "Virtual unreadable pages: %zu. Saved to: %s",
                      (unsigned long long)(ram_size / (1024ull * 1024ull)),
                      (unsigned long long)mapped_pages,
                      regions.size(),
                      (unsigned long long)((mapped_pages * 0x1000ull) / (1024ull * 1024ull)),
                      physical_failed, virtual_failed, directory.c_str());
    } else if (dump_physical) {
        std::snprintf(message, sizeof(message),
                      "Physical RAM dump complete. VM remains paused. Physical RAM: %llu MB. "
                      "Unreadable pages zero-filled: %zu. Saved to: %s",
                      (unsigned long long)(ram_size / (1024ull * 1024ull)),
                      physical_failed, directory.c_str());
    } else {
        std::snprintf(message, sizeof(message),
                      "Mapped virtual RAM dump complete. VM remains paused. %llu pages in %zu region(s), "
                      "%llu MB including aliases. Unreadable pages zero-filled: %zu. Saved to: %s",
                      (unsigned long long)mapped_pages,
                      regions.size(),
                      (unsigned long long)((mapped_pages * 0x1000ull) / (1024ull * 1024ull)),
                      virtual_failed, directory.c_str());
    }
    m_dump_status = message;
}

// RAM Dump rendering/UI is owned by memory-tools-dump-ui.cc.
