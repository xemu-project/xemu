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

#include <algorithm>
#include <cstdio>
#include <iterator>

using namespace xemu_memory_tools_internal;

size_t MemoryToolsWindow::FindRegionForVirtual(uint32_t address) const
{
    const uint64_t a = address;
    const auto it = std::upper_bound(
        m_memory_map_regions.begin(), m_memory_map_regions.end(), a,
        [](uint64_t value, const MemoryMapRegion &region) {
            return value < region.virtual_start;
        });
    if (it == m_memory_map_regions.begin()) {
        return (size_t)-1;
    }

    const auto candidate = std::prev(it);
    if (a >= candidate->virtual_start && a < candidate->virtual_end_exclusive) {
        return (size_t)std::distance(m_memory_map_regions.begin(), candidate);
    }
    return (size_t)-1;
}

size_t MemoryToolsWindow::FindRegionForPhysical(uint32_t address) const
{
    const uint64_t a = address;

    // Preserve the alias explicitly selected in the center pane whenever that
    // mapping still covers the physical byte being viewed.
    if (m_active_map_region < m_memory_map_regions.size()) {
        const MemoryMapRegion &active = m_memory_map_regions[m_active_map_region];
        if (a >= active.physical_start && a < active.physical_end_exclusive) {
            return m_active_map_region;
        }
    }

    for (size_t i = 0; i < m_memory_map_regions.size(); ++i) {
        const MemoryMapRegion &region = m_memory_map_regions[i];
        if (a >= region.physical_start && a < region.physical_end_exclusive) {
            return i;
        }
    }
    return (size_t)-1;
}

void MemoryToolsWindow::SelectMemoryMapRegion(size_t index, bool jump_to_start)
{
    if (index >= m_memory_map_regions.size()) {
        return;
    }

    m_active_map_region = index;
    const MemoryMapRegion &region = m_memory_map_regions[index];

    if (jump_to_start) {
        m_physical_viewer.address = (uint32_t)region.physical_start;
        m_virtual_viewer.address = (uint32_t)region.virtual_start;
        m_physical_viewer.request_scroll = true;
        m_virtual_viewer.request_scroll = true;
        SetHexText(m_physical_viewer.address_text,
                   sizeof(m_physical_viewer.address_text),
                   m_physical_viewer.address);
        SetHexText(m_virtual_viewer.address_text,
                   sizeof(m_virtual_viewer.address_text),
                   m_virtual_viewer.address);

        m_have_memory_selection = true;
        m_have_selected_physical = true;
        m_have_selected_virtual = true;
        m_selected_physical_address = (uint32_t)region.physical_start;
        m_selected_virtual_address = (uint32_t)region.virtual_start;
        m_memory_edit_space = AddressSpace::Virtual;
        m_memory_edit_text[0] = '\0';
        m_memory_edit_focus_requested = false;
    }

    SetHexText(m_map_physical_text, sizeof(m_map_physical_text),
               (uint32_t)region.physical_start);
    SetHexText(m_map_virtual_text, sizeof(m_map_virtual_text),
               (uint32_t)region.virtual_start);
    LookupPhysicalAliases();
}

bool MemoryToolsWindow::SyncPhysicalFromVirtual(uint32_t virtual_address)
{
    if (!m_memory_map_valid) {
        return false;
    }

    const size_t index = FindRegionForVirtual(virtual_address);
    if (index == (size_t)-1) {
        return false;
    }

    const MemoryMapRegion &region = m_memory_map_regions[index];
    const uint64_t physical = region.physical_start +
                              ((uint64_t)virtual_address - region.virtual_start);
    if (physical > 0xFFFFFFFFull) {
        return false;
    }

    m_active_map_region = index;
    m_physical_viewer.address = (uint32_t)physical & ~0xFu;
    m_physical_viewer.request_scroll = true;
    SetHexText(m_physical_viewer.address_text,
               sizeof(m_physical_viewer.address_text),
               m_physical_viewer.address);
    SetHexText(m_map_virtual_text, sizeof(m_map_virtual_text), virtual_address);
    SetHexText(m_map_physical_text, sizeof(m_map_physical_text),
               (uint32_t)physical);
    LookupPhysicalAliases();
    return true;
}

bool MemoryToolsWindow::SyncVirtualFromPhysical(uint32_t physical_address)
{
    if (!m_memory_map_valid) {
        return false;
    }

    const size_t index = FindRegionForPhysical(physical_address);
    if (index == (size_t)-1) {
        return false;
    }

    const MemoryMapRegion &region = m_memory_map_regions[index];
    const uint64_t virtual_address = region.virtual_start +
                                     ((uint64_t)physical_address -
                                      region.physical_start);
    if (virtual_address > 0xFFFFFFFFull) {
        return false;
    }

    m_active_map_region = index;
    m_virtual_viewer.address = (uint32_t)virtual_address & ~0xFu;
    m_virtual_viewer.request_scroll = true;
    SetHexText(m_virtual_viewer.address_text,
               sizeof(m_virtual_viewer.address_text),
               m_virtual_viewer.address);
    SetHexText(m_map_physical_text, sizeof(m_map_physical_text),
               physical_address);
    SetHexText(m_map_virtual_text, sizeof(m_map_virtual_text),
               (uint32_t)virtual_address);
    LookupPhysicalAliases();
    return true;
}

bool MemoryToolsWindow::SelectedAddressForSpace(AddressSpace space,
                                                    uint32_t &address) const
{
    if (!m_have_memory_selection) {
        return false;
    }

    if (space == AddressSpace::Physical) {
        if (!m_have_selected_physical) {
            return false;
        }
        address = m_selected_physical_address;
        return true;
    }

    if (!m_have_selected_virtual) {
        return false;
    }
    address = m_selected_virtual_address;
    return true;
}

void MemoryToolsWindow::SelectMemoryByte(AddressSpace space, uint32_t address)
{
    m_have_memory_selection = true;
    m_memory_edit_space = space;
    m_memory_edit_text[0] = '\0';
    m_memory_edit_focus_requested = false;

    if (space == AddressSpace::Physical) {
        m_selected_physical_address = address;
        m_have_selected_physical = true;
        m_have_selected_virtual = false;

        if (m_memory_map_valid) {
            const size_t index = FindRegionForPhysical(address);
            if (index != (size_t)-1) {
                const MemoryMapRegion &region = m_memory_map_regions[index];
                const uint64_t virtual_address =
                    region.virtual_start + ((uint64_t)address - region.physical_start);
                if (virtual_address <= 0xFFFFFFFFull) {
                    const bool region_changed = m_active_map_region != index;
                    m_active_map_region = index;
                    m_selected_virtual_address = (uint32_t)virtual_address;
                    m_have_selected_virtual = true;

                    const bool already_visible =
                        !region_changed && m_virtual_viewer.visible_range_valid &&
                        virtual_address >= m_virtual_viewer.visible_start &&
                        virtual_address < m_virtual_viewer.visible_end_exclusive;
                    if (already_visible) {
                        m_virtual_viewer.request_scroll = false;
                    } else {
                        m_virtual_viewer.address =
                            (uint32_t)virtual_address & ~0xFu;
                        m_virtual_viewer.request_scroll = true;
                        SetHexText(m_virtual_viewer.address_text,
                                   sizeof(m_virtual_viewer.address_text),
                                   m_virtual_viewer.address);
                    }
                    SetHexText(m_map_virtual_text, sizeof(m_map_virtual_text),
                               (uint32_t)virtual_address);
                    SetHexText(m_map_physical_text, sizeof(m_map_physical_text),
                               address);
                    LookupPhysicalAliases();
                }
            }
        }
    } else {
        m_selected_virtual_address = address;
        m_have_selected_virtual = true;
        m_have_selected_physical = false;

        if (m_memory_map_valid) {
            const size_t index = FindRegionForVirtual(address);
            if (index != (size_t)-1) {
                const MemoryMapRegion &region = m_memory_map_regions[index];
                const uint64_t physical_address =
                    region.physical_start + ((uint64_t)address - region.virtual_start);
                if (physical_address <= 0xFFFFFFFFull) {
                    const bool region_changed = m_active_map_region != index;
                    m_active_map_region = index;
                    m_selected_physical_address = (uint32_t)physical_address;
                    m_have_selected_physical = true;

                    const bool already_visible =
                        !region_changed && m_physical_viewer.visible_range_valid &&
                        physical_address >= m_physical_viewer.visible_start &&
                        physical_address < m_physical_viewer.visible_end_exclusive;
                    if (already_visible) {
                        m_physical_viewer.request_scroll = false;
                    } else {
                        m_physical_viewer.address =
                            (uint32_t)physical_address & ~0xFu;
                        m_physical_viewer.request_scroll = true;
                        SetHexText(m_physical_viewer.address_text,
                                   sizeof(m_physical_viewer.address_text),
                                   m_physical_viewer.address);
                    }
                    SetHexText(m_map_virtual_text, sizeof(m_map_virtual_text),
                               address);
                    SetHexText(m_map_physical_text, sizeof(m_map_physical_text),
                               (uint32_t)physical_address);
                    LookupPhysicalAliases();
                }
            }
        }
    }
}

// Memory Viewer rendering/UI methods are owned by memory-tools-memory-ui.cc.

void MemoryToolsWindow::JumpViewerTo(AddressSpace space, uint32_t address)
{
    ViewerState &viewer = space == AddressSpace::Virtual
                              ? m_virtual_viewer
                              : m_physical_viewer;
    viewer.address = address & ~0xFu;
    viewer.request_scroll = true;
    SetHexText(viewer.address_text, sizeof(viewer.address_text), viewer.address);

    if (space == AddressSpace::Virtual) {
        SyncPhysicalFromVirtual(address);
    } else {
        SyncVirtualFromPhysical(address);
    }

    char message[96];
    std::snprintf(message, sizeof(message),
                  "Memory jump to %08X", address);
    viewer.status = message;
}

bool MemoryToolsWindow::CollectRamVirtualPages(
    uint64_t ram_size, std::vector<VirtualPageMapping> &pages,
    bool &used_page_table_snapshot) const
{
    constexpr uint64_t kVirtualSize = 0x100000000ull;
    constexpr uint64_t kVirtualPage = 0x1000ull;

    pages.clear();
    used_page_table_snapshot = false;

    XemuCheatVirtualMapping *mappings = nullptr;
    size_t mapping_count = 0;
    if (xemu_cheat_collect_ram_virtual_mappings(ram_size, &mappings,
                                                &mapping_count)) {
        for (size_t i = 0; i < mapping_count; ++i) {
            const auto &mapping = mappings[i];
            for (uint64_t offset = 0; offset < mapping.length;
                 offset += kVirtualPage) {
                const uint64_t virtual_address =
                    (uint64_t)mapping.virtual_start + offset;
                const uint64_t physical_address =
                    mapping.physical_start + offset;
                if (virtual_address >= kVirtualSize || physical_address >= ram_size) {
                    break;
                }
                pages.push_back({(uint32_t)virtual_address,
                                 physical_address & ~(kVirtualPage - 1)});
            }
        }
        xemu_cheat_free_virtual_mappings(mappings);

        if (!pages.empty()) {
            std::sort(pages.begin(), pages.end(),
                      [](const VirtualPageMapping &a,
                         const VirtualPageMapping &b) {
                          if (a.virtual_page != b.virtual_page) {
                              return a.virtual_page < b.virtual_page;
                          }
                          return a.physical_page < b.physical_page;
                      });
            pages.erase(std::unique(
                            pages.begin(), pages.end(),
                            [](const VirtualPageMapping &a,
                               const VirtualPageMapping &b) {
                                return a.virtual_page == b.virtual_page &&
                                       a.physical_page == b.physical_page;
                            }),
                        pages.end());
            used_page_table_snapshot = true;
            return true;
        }
    } else {
        xemu_cheat_free_virtual_mappings(mappings);
    }

    /* Compatibility fallback for accelerators/targets where QEMU cannot expose
     * a page-table snapshot. This is the exact legacy 4 GiB page-probe path. */
    if (!xemu_cheat_prepare_virtual_map()) {
        return false;
    }
    const size_t physical_page_count =
        (size_t)((ram_size + kVirtualPage - 1) / kVirtualPage);
    pages.reserve(physical_page_count * 2);
    for (uint64_t virtual_address = 0;
         virtual_address < kVirtualSize;
         virtual_address += kVirtualPage) {
        uint64_t physical_address = 0;
        if (xemu_cheat_virtual_to_physical((uint32_t)virtual_address,
                                           &physical_address) &&
            physical_address < ram_size) {
            pages.push_back({(uint32_t)virtual_address,
                             physical_address & ~(kVirtualPage - 1)});
        }
    }
    return true;
}

bool MemoryToolsWindow::RefreshMemoryMap()
{
    constexpr uint64_t kVirtualPage = 0x1000ull;

    m_virtual_page_map.clear();
    m_physical_alias_page_index.clear();
    m_memory_map_regions.clear();
    m_physical_aliases.clear();
    m_memory_map_valid = false;
    m_active_map_region = (size_t)-1;
    m_memory_map_unique_physical_pages = 0;

    const uint64_t ram_size = xemu_cheat_ram_size();
    if (ram_size == 0 || ram_size > 0x40000000ull) {
        m_memory_map_status = "Could not determine a valid Xbox RAM size.";
        return false;
    }
    if (!xemu_cheat_cpu_available()) {
        m_memory_map_status = "Xbox CPU is not available.";
        return false;
    }

    bool used_page_table_snapshot = false;
    if (!CollectRamVirtualPages(ram_size, m_virtual_page_map,
                                used_page_table_snapshot)) {
        m_memory_map_status = "Could not synchronize/read the Xbox page tables.";
        return false;
    }

    const size_t physical_page_count =
        (size_t)((ram_size + kVirtualPage - 1) / kVirtualPage);
    std::vector<uint8_t> seen_physical_pages(physical_page_count, 0);
    m_physical_alias_page_index.reserve(m_virtual_page_map.size());
    m_memory_map_regions.reserve(m_virtual_page_map.size() / 16 + 1);

    bool in_region = false;
    MemoryMapRegion current = {};
    for (const auto &mapping : m_virtual_page_map) {
        const uint64_t virtual_address = mapping.virtual_page;
        const uint64_t physical_page = mapping.physical_page;

        m_physical_alias_page_index.push_back(
            {(uint32_t)physical_page, mapping.virtual_page});

        const size_t physical_index = (size_t)(physical_page / kVirtualPage);
        if (physical_index < seen_physical_pages.size()) {
            seen_physical_pages[physical_index] = 1;
        }

        /* A displayed region is only merged when BOTH sides are contiguous. */
        if (!in_region ||
            current.virtual_end_exclusive != virtual_address ||
            current.physical_end_exclusive != physical_page) {
            if (in_region) {
                m_memory_map_regions.push_back(current);
            }
            current.virtual_start = virtual_address;
            current.virtual_end_exclusive = virtual_address + kVirtualPage;
            current.physical_start = physical_page;
            current.physical_end_exclusive = physical_page + kVirtualPage;
            in_region = true;
        } else {
            current.virtual_end_exclusive += kVirtualPage;
            current.physical_end_exclusive += kVirtualPage;
        }
    }
    if (in_region) {
        m_memory_map_regions.push_back(current);
    }

    std::sort(m_physical_alias_page_index.begin(),
              m_physical_alias_page_index.end(),
              [](const PhysicalAliasPage &a, const PhysicalAliasPage &b) {
                  if (a.physical_page != b.physical_page) {
                      return a.physical_page < b.physical_page;
                  }
                  return a.virtual_page < b.virtual_page;
              });

    m_memory_map_unique_physical_pages =
        (uint64_t)std::count(seen_physical_pages.begin(),
                             seen_physical_pages.end(), (uint8_t)1);
    m_memory_map_ram_size = ram_size;
    m_memory_map_generation = current_game_manager.Generation();
    m_memory_map_valid = true;

    size_t initial_region = FindRegionForVirtual(m_virtual_viewer.address);
    if (initial_region == (size_t)-1) {
        initial_region = FindRegionForPhysical(m_physical_viewer.address);
    }
    if (initial_region == (size_t)-1 && !m_memory_map_regions.empty()) {
        initial_region = 0;
    }
    if (initial_region != (size_t)-1) {
        SelectMemoryMapRegion(initial_region, false);
        SyncVirtualFromPhysical(m_physical_viewer.address);
    }

    const uint64_t mapped_pages = (uint64_t)m_virtual_page_map.size();
    const uint64_t alias_pages = mapped_pages >= m_memory_map_unique_physical_pages
                                     ? mapped_pages - m_memory_map_unique_physical_pages
                                     : 0;
    char status[384];
    std::snprintf(status, sizeof(status),
                  "Map refreshed via %s: %llu RAM-backed virtual page(s), %llu unique physical page(s), "
                  "%llu alias page(s), %zu linear region(s).",
                  used_page_table_snapshot ? "page-table snapshot" : "legacy page scan",
                  (unsigned long long)mapped_pages,
                  (unsigned long long)m_memory_map_unique_physical_pages,
                  (unsigned long long)alias_pages,
                  m_memory_map_regions.size());
    m_memory_map_status = status;

    return true;
}

void MemoryToolsWindow::LookupPhysicalAliases()
{
    m_physical_aliases.clear();

    uint32_t physical_address = 0;
    if (!ParseHexAddress(m_map_physical_text, physical_address)) {
        m_memory_map_status = "Invalid physical hexadecimal address.";
        return;
    }
    if (!m_memory_map_valid) {
        m_memory_map_status = "Build the Memory Map first.";
        return;
    }
    if ((uint64_t)physical_address >= m_memory_map_ram_size) {
        m_memory_map_status = "Physical address is outside installed Xbox RAM.";
        return;
    }

    const uint32_t physical_page = physical_address & ~0xFFFu;
    const uint32_t page_offset = physical_address & 0xFFFu;

    const auto first = std::lower_bound(
        m_physical_alias_page_index.begin(), m_physical_alias_page_index.end(),
        physical_page,
        [](const PhysicalAliasPage &mapping, uint32_t page) {
            return mapping.physical_page < page;
        });
    const auto last = std::upper_bound(
        first, m_physical_alias_page_index.end(), physical_page,
        [](uint32_t page, const PhysicalAliasPage &mapping) {
            return page < mapping.physical_page;
        });
    m_physical_aliases.reserve((size_t)std::distance(first, last));
    for (auto it = first; it != last; ++it) {
        m_physical_aliases.push_back(it->virtual_page + page_offset);
    }

    char status[256];
    if (m_physical_aliases.empty()) {
        std::snprintf(status, sizeof(status),
                      "Physical %08X has no RAM-backed virtual alias in this map snapshot.",
                      physical_address);
    } else {
        std::snprintf(status, sizeof(status),
                      "Physical %08X is visible at %zu virtual address(es).",
                      physical_address, m_physical_aliases.size());
    }
    m_memory_map_status = status;
}
