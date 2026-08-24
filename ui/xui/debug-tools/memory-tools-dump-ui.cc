//
// xemu Memory Viewer / Search / x86 Debugger - RAM Dump UI
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "memory-tools.hh"
#include "cheat-engine-memory.h"

#include <cstdint>
#include <string>

void MemoryToolsWindow::DrawDumpRam()
{
    const uint64_t ram_size = xemu_cheat_ram_size();
    const uint64_t ram_mb = ram_size / (1024ull * 1024ull);

    ImGui::TextWrapped("Choose which RAM view to dump. Full-range dump actions pause the guest before reading and leave it paused afterward for debugger inspection.");
    ImGui::Spacing();
    if (ram_size != 0) {
        ImGui::Text("Installed Xbox RAM: %llu MB", (unsigned long long)ram_mb);
        const uint32_t physical_end = (uint32_t)(ram_size - 1);
        ImGui::Text("Physical: 00000000-%08X -> one complete .bin", physical_end);
        ImGui::Text("Virtual:  00000000-FFFFFFFF -> mapped RAM-backed regions only");
        ImGui::TextDisabled("Mapped virtual regions are discovered from the running Xbox page tables; MMIO/device mappings are excluded and aliases are preserved.");
    } else {
        ImGui::TextDisabled("Xbox RAM size is not available yet.");
    }

    const std::string dump_directory = DumpDirectory();
    if (!dump_directory.empty()) {
        ImGui::TextDisabled("Output folder: %s", dump_directory.c_str());
    } else {
        ImGui::TextDisabled("Output folder: <xemu.exe directory>\\Ram-Dumps");
    }

    ImGui::Spacing();
    if (ImGui::Button("DUMP PHYSICAL", ImVec2(320.0f, 0.0f))) {
        DumpPhysicalRam();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("One complete PHYSICAL .bin");

    if (ImGui::Button("DUMP MAPPED VIRTUAL RAM", ImVec2(320.0f, 0.0f))) {
        DumpMappedVirtualRam();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("VIRTUAL region .bin files + VIRTUAL-MAP.txt");

    if (ImGui::Button("DUMP PHYSICAL + DUMP MAPPED VIRTUAL RAM", ImVec2(320.0f, 0.0f))) {
        DumpFullRam();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Creates both dump sets from the same paused guest state");

    if (!m_dump_status.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", m_dump_status.c_str());
    }
}
