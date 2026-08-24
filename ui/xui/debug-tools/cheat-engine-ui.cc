//
// xemu RAW Cheat Engine - UI/frontend ownership
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "cheat-engine.hh"
#include "current-game.hh"
#include "mixed-checkbox.hh"
#include "tab-style.hh"

static const char *PreEntryStatusText(bool selected, bool applied)
{
    if (selected && applied) {
        return "[ACTIVATED]";
    }
    if (selected) {
        return "[ACTIVATED - RESET REQUIRED]";
    }
    if (applied) {
        return "[DEACTIVATED - RESET REQUIRED]";
    }
    return "[INACTIVE]";
}

void CheatEngineWindow::SetGroupSelected(int group_index, bool selected)
{
    if (group_index < 0 || (size_t)group_index >= m_groups.size()) {
        return;
    }
    const CheatGroup &group = m_groups[(size_t)group_index];
    for (size_t cheat : group.cheats) {
        if (cheat < m_blocks.size() && !m_blocks[cheat].preentry) {
            m_blocks[cheat].selected = selected;
            if (!selected) {
                /* Unchecking is an unconditional restore boundary for persistent
                 * F0/F1 hooks. Do not make original-byte restoration depend on
                 * the global Live Cheats UI state. */
                m_blocks[cheat].enabled = false;
                DeactivateFHooksForBlock(cheat);
            } else if (m_live_cheats_enabled) {
                m_blocks[cheat].enabled = true;
            }
        }
    }
    for (int child : group.child_groups) {
        SetGroupSelected(child, selected);
    }
    if (m_live_cheats_enabled) {
        m_switches.clear();
    }
}

void CheatEngineWindow::SetPatchGroupSelected(int group_index, bool selected)
{
    if (group_index < 0 || (size_t)group_index >= m_groups.size()) {
        return;
    }
    const CheatGroup &group = m_groups[(size_t)group_index];
    for (size_t cheat : group.cheats) {
        if (cheat < m_blocks.size() && m_blocks[cheat].preentry) {
            m_blocks[cheat].selected = selected;
            RememberPreEntrySelection(m_blocks[cheat]);
        }
    }
    for (int child : group.child_groups) {
        SetPatchGroupSelected(child, selected);
    }
}

void CheatEngineWindow::DisableAllCheats(bool clear_selection)
{
    DeactivateLiveFHooks();

    for (auto &block : m_blocks) {
        if (block.preentry) {
            continue;
        }
        block.enabled = false;
        if (clear_selection) {
            block.selected = false;
        }
    }
    m_switches.clear();
}

void CheatEngineWindow::SetLiveCheatsEnabled(bool enabled)
{
    if (m_live_cheats_enabled == enabled) {
        return;
    }
    m_live_cheats_enabled = enabled;
    if (!enabled) {
        DisableAllCheats(true);
    }
}

void CheatEngineWindow::CountGroupSelection(int group_index, size_t &selected,
                                            size_t &total) const
{
    if (group_index < 0 || (size_t)group_index >= m_groups.size()) {
        return;
    }
    const CheatGroup &group = m_groups[(size_t)group_index];
    for (size_t cheat : group.cheats) {
        if (cheat < m_blocks.size() && !m_blocks[cheat].preentry) {
            ++total;
            if (m_blocks[cheat].selected) {
                ++selected;
            }
        }
    }
    for (int child : group.child_groups) {
        CountGroupSelection(child, selected, total);
    }
}

void CheatEngineWindow::CountPatchGroupSelection(int group_index,
                                                 size_t &selected,
                                                 size_t &total) const
{
    if (group_index < 0 || (size_t)group_index >= m_groups.size()) {
        return;
    }
    const CheatGroup &group = m_groups[(size_t)group_index];
    for (size_t cheat : group.cheats) {
        if (cheat < m_blocks.size() && m_blocks[cheat].preentry) {
            ++total;
            if (m_blocks[cheat].selected) {
                ++selected;
            }
        }
    }
    for (int child : group.child_groups) {
        CountPatchGroupSelection(child, selected, total);
    }
}

void CheatEngineWindow::DrawCheat(size_t block_index)
{
    if (block_index >= m_blocks.size()) {
        return;
    }
    CheatBlock &block = m_blocks[block_index];
    if (block.preentry) {
        return;
    }
    ImGui::PushID((int)block_index);
    if (ImGui::Checkbox("##selected", &block.selected)) {
        if (!block.selected) {
            /* A user uncheck always means OFF, including immediate restoration
             * of any persistent F0/F1 hook bytes owned by this block. */
            block.enabled = false;
            DeactivateFHooksForBlock(block_index);
        } else if (m_live_cheats_enabled) {
            block.enabled = true;
        }
        if (m_live_cheats_enabled) {
            m_switches.clear();
        }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(block.name.c_str());
    if (block.enabled) {
        ImGui::SameLine();
        ImGui::TextDisabled("[ACTIVE]");
    }
    if (ImGui::IsItemHovered() &&
        (!block.description.empty() || !block.credits.empty())) {
        std::string tip;
        if (!block.description.empty()) {
            tip += block.description;
        }
        if (!block.credits.empty()) {
            if (!tip.empty()) tip += "\n\n";
            tip += "Credits: " + block.credits;
        }
        ImGui::SetTooltip("%s", tip.c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu RAW lines)", block.codes.size());
    ImGui::PopID();
}

void CheatEngineWindow::DrawPatch(size_t block_index)
{
    if (block_index >= m_blocks.size()) {
        return;
    }
    CheatBlock &block = m_blocks[block_index];
    if (!block.preentry) {
        return;
    }

    ImGui::PushID((int)block_index);
    if (ImGui::Checkbox("##patch-selected", &block.selected)) {
        RememberPreEntrySelection(block);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(block.name.c_str());
    const bool name_hovered = ImGui::IsItemHovered();
    ImGui::SameLine();
    const char *status = block.preentry_error.empty()
                             ? PreEntryStatusText(block.selected,
                                                  block.preentry_applied)
                             : "[ERROR - RESET REQUIRED]";
    ImGui::TextDisabled("%s", status);

    if (name_hovered &&
        (!block.preentry_error.empty() || !block.description.empty() ||
         !block.credits.empty())) {
        std::string tip;
        if (!block.preentry_error.empty()) {
            tip += "Last Patch error: " + block.preentry_error;
        }
        if (!block.description.empty()) {
            if (!tip.empty()) tip += "\n\n";
            tip += block.description;
        }
        if (!block.credits.empty()) {
            if (!tip.empty()) tip += "\n\n";
            tip += "Credits: " + block.credits;
        }
        ImGui::SetTooltip("%s", tip.c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu RAW lines)", block.codes.size());
    ImGui::PopID();
}

void CheatEngineWindow::DrawGroup(int group_index)
{
    if (group_index < 0 || (size_t)group_index >= m_groups.size()) {
        return;
    }
    CheatGroup &group = m_groups[(size_t)group_index];

    if (group_index == 0) {
        for (size_t cheat : group.cheats) {
            if (cheat < m_blocks.size() && !m_blocks[cheat].preentry) {
                DrawCheat(cheat);
            }
        }
        for (int child : group.child_groups) {
            DrawGroup(child);
        }
        return;
    }

    ImGui::PushID(group_index);
    size_t selected = 0, total = 0;
    CountGroupSelection(group_index, selected, total);
    if (total == 0) {
        ImGui::PopID();
        return;
    }
    const bool mixed_selection = selected != 0 && selected != total;
    bool group_checkbox = selected == total;
    if (XemuDebugUi::MixedCheckbox("##group-select", mixed_selection,
                                   &group_checkbox)) {
        SetGroupSelected(group_index, group_checkbox);
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.65f, 1.00f, 1.00f));
    bool open = ImGui::TreeNodeEx("##group-tree",
                                  ImGuiTreeNodeFlags_DefaultOpen |
                                  ImGuiTreeNodeFlags_SpanAvailWidth,
                                  "%s (%zu/%zu)", group.name.c_str(),
                                  selected, total);
    ImGui::PopStyleColor();
    if (open) {
        for (size_t cheat : group.cheats) {
            DrawCheat(cheat);
        }
        for (int child : group.child_groups) {
            DrawGroup(child);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void CheatEngineWindow::DrawPatchGroup(int group_index)
{
    if (group_index < 0 || (size_t)group_index >= m_groups.size()) {
        return;
    }
    CheatGroup &group = m_groups[(size_t)group_index];

    if (group_index == 0) {
        for (size_t cheat : group.cheats) {
            if (cheat < m_blocks.size() && m_blocks[cheat].preentry) {
                DrawPatch(cheat);
            }
        }
        for (int child : group.child_groups) {
            DrawPatchGroup(child);
        }
        return;
    }

    ImGui::PushID(group_index);
    size_t selected = 0, total = 0;
    CountPatchGroupSelection(group_index, selected, total);
    if (total == 0) {
        ImGui::PopID();
        return;
    }

    const bool mixed_selection = selected != 0 && selected != total;
    bool group_checkbox = selected == total;
    if (XemuDebugUi::MixedCheckbox("##patch-group-select", mixed_selection,
                                   &group_checkbox)) {
        SetPatchGroupSelected(group_index, group_checkbox);
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.65f, 1.00f, 1.00f));
    bool open = ImGui::TreeNodeEx("##patch-group-tree",
                                  ImGuiTreeNodeFlags_DefaultOpen |
                                  ImGuiTreeNodeFlags_SpanAvailWidth,
                                  "%s (%zu/%zu)", group.name.c_str(),
                                  selected, total);
    ImGui::PopStyleColor();
    if (open) {
        for (size_t cheat : group.cheats) {
            if (cheat < m_blocks.size() && m_blocks[cheat].preentry) {
                DrawPatch(cheat);
            }
        }
        for (int child : group.child_groups) {
            DrawPatchGroup(child);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void CheatEngineWindow::DrawMenuBar()
{
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open", nullptr)) {
            OpenCheatDirectory();
        }
        if (ImGui::MenuItem("Reload", nullptr)) {
            ReloadCurrentCheatFile();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Edit/Add Cheats", nullptr)) {
            EditOrCreateCurrentCheatFile();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Options")) {
        ImGui::MenuItem("Code-Type-Aware D/E", nullptr, &m_code_aware_skip);
        if (ImGui::MenuItem("Auto-load Current Game File", nullptr,
                            &m_auto_load_current_game) &&
            m_auto_load_current_game) {
            /* Enabling Auto-load mid-game may load the matching file, but must
             * never synthesize a startup edge or execute PREENTRY immediately. */
            LoadMatchingCurrentGameFile(false);
        }
        ImGui::MenuItem("Engine Enabled", nullptr, &m_engine_enabled);
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Help", nullptr)) {
        m_show_help = true;
    }

    ImGui::EndMenuBar();
}

void CheatEngineWindow::DrawHelpPopup()
{
    if (m_show_help) {
        ImGui::OpenPopup("Code Types & Examples");
        m_show_help = false;
    }

    ImGui::SetNextWindowSize(ImVec2(680, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopupModal("Code Types & Examples", nullptr,
                                ImGuiWindowFlags_NoCollapse)) {
        return;
    }

    ImGui::BeginChild("##CodeTypeHelp", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
    ImGui::TextWrapped(
        "Default address space: Virtual RAM. Addresses are used literally; "
        "no implicit 0x80000000 base is added. Type 9 can override the context.");
    ImGui::Separator();

    auto section = [](const char *title, const char *body) {
        ImGui::TextColored(ImVec4(0.30f, 0.65f, 1.00f, 1.00f), "%s", title);
        ImGui::TextUnformatted(body);
        ImGui::Spacing();
    };

    section("0 / 1 / 2 - Basic Writes",
            "0AAAAAAA  000000VV\n"
            "    0 = 8-bit Write\n"
            "    A = Virtual Address / Current Context Offset\n"
            "    V = Value\n\n"
            "1AAAAAAA  0000VVVV\n"
            "    1 = 16-bit Write\n"
            "    A = Virtual Address / Current Context Offset\n"
            "    V = Value\n\n"
            "2AAAAAAA  VVVVVVVV\n"
            "    2 = 32-bit Write\n"
            "    A = Virtual Address / Current Context Offset\n"
            "    V = Value\n\n"
            "Example\n"
            "    2006C32C  00AAE9C0");

    section("3 - Arithmetic",
            "30T0VVVV  0AAAAAAA\n"
            "    T = Type\n"
            "        0 = Byte Increment\n"
            "        1 = Byte Decrement\n"
            "        2 = Halfword Increment\n"
            "        3 = Halfword Decrement\n"
            "        4 = Word Increment (uses continuation line)\n"
            "        5 = Word Decrement (uses continuation line)\n"
            "    V = Amount for byte/halfword forms\n"
            "    A = Virtual Address / Current Context Offset");

    section("4 - 32-bit Serial Write",
            "4AAAAAAA  NNNNSSSS\n"
            "VVVVVVVV  IIIIIIII\n"
            "    A = Start Address / Current Context Offset\n"
            "    N = Number of Writes\n"
            "    S = Address Step (SSSS * 4)\n"
            "    V = Starting 32-bit Value\n"
            "    I = Value Increment per Write");

    section("5 - Byte Copy",
            "5AAAAAAA  NNNNNNNN\n"
            "DDDDDDDD  00000000\n"
            "    A = Source Address / Current Context Offset\n"
            "    N = Number of Bytes\n"
            "    D = Destination Address / Current Context Offset");

    section("6 - Pointer Write",
            "6AAAAAAA  VVVVVVVV\n"
            "000TNNNN  OOOOOOOO\n"
            "    A = Pointer Address / Current Context Offset\n"
            "    V = Value\n"
            "    T = Write Size\n"
            "        0 = 8-bit\n"
            "        1 = 16-bit\n"
            "        2 = 32-bit\n"
            "    N = Offset Count\n"
            "    O = Pointer Offset");

    section("7 - Bitwise",
            "7AAAAAAA  00T0VVVV\n"
            "    A = Virtual Address / Current Context Offset\n"
            "    T = Type\n"
            "        0 = Byte OR\n"
            "        1 = Halfword OR\n"
            "        2 = Byte AND\n"
            "        3 = Halfword AND\n"
            "        4 = Byte XOR\n"
            "        5 = Halfword XOR\n"
            "    V = Value");

    section("9 - Address Context",
            "9MCCCCCC  BBBBBBBB\n"
            "M = Type\n"
            "    0 = Virtual Base\n"
            "    1 = Physical Base\n"
            "    2 = Virtual Pointer Base\n"
            "    3 = Physical Pointer Base\n"
            "C = Count\n"
            "    000000 = Continue Until End of Cheat\n"
            "B = Offset / Base Address\n\n"
            "Normal codes already use Virtual RAM by default, so Type 90 is\n"
            "mainly useful when a relative Virtual Base is wanted.");

    section("A - Literal Raw-Byte Write",
            "AXXXXXXX  ZZZZZZZZ\n"
            "DDDDDDDD  DDDDDDDD\n"
            "    X = Virtual Address / Current Context Offset\n"
            "    Z = Total Number of Bytes to Write\n"
            "    D = Literal Bytes in the Exact Order Shown\n"
            "    Extra Lines = ceil(Z / 8)\n\n"
            "Example\n"
            "    A006C320  00000006\n"
            "    0F85A900  00000000\n"
            "Writes: 0F 85 A9 00 00 00");

    section("D / E - Conditions",
            "D = Level Conditional\n"
            "E = Edge-Toggle Conditional\n\n"
            "Code-Type-Aware D/E = ON\n"
            "    Skip counts operate on logical commands.\n\n"
            "Code-Type-Aware D/E = OFF\n"
            "    Skip counts operate on exact physical RAW lines.");

    section("F - Automatic External x86 Code Cave",
            "F0000000  AAAAAAAA   Recommended: x86 assembly\n"
            "<Intel-syntax instructions / labels>\n"
            "DEADCODE\n\n"
            "F1000000  AAAAAAAA   Advanced: aligned raw x86 bytes\n"
            "XXXXXXXX  YYYYYYYY\n"
            "...\n"
            "DEADCODE  000000NN\n\n"
            "    A = Virtual Hook Address / Virtual Type-9 Offset\n"
            "    NN = 01-08 valid bytes in the final F1 8-byte line\n"
            "    Low 24 F0/F1 bits are reserved and must be 000000.\n"
            "    A leading $ is optional on every Type-F line.\n\n"
            "xemu automatically allocates a 16-byte-aligned cave in its\n"
            "private 1 MiB mapped arena. 68000000-680EFFFF is code/DD space;\n"
            "680F0000-680FFFFF is reserved for private PRESERVE/T0-T7/TFLAGS state. The\n"
            "mapping is outside Xbox machine RAM. Capstone covers complete\n"
            "instructions until at least 5 hook bytes are available, then xemu\n"
            "writes JMP rel32 + NOP padding and generates the DEADCODE JMP back.\n"
            "Disabling safely restores/frees only that cave; other caves never\n"
            "move or shift.\n\n"
            "F0 supports labels, common integer/control-flow x86, DD static data,\n"
            "PRESERVEALL, PRESERVE <regs>, RESTORE, and RESTORE <regs>. DD data\n"
            "may follow DEADCODE and is physically placed after the generated\n"
            "return JMP inside the same allocation. Bare numbers are hexadecimal.\n"
            "PRESERVE supports EAX/EBX/ECX/EDX/ESI/EDI/EBP/EFLAGS; ESP/EIP are\n"
            "intentionally excluded. F0 also provides persistent virtual 32-bit\n"
            "scratch registers T0-T7 plus private TFLAGS. Common forms:\n"
            "mov T0,CarList; mov T1,[T0]; test T1,T1; cmp eax,T1; add T0,4.\n"
            "T-based flag writers capture their result into TFLAGS and restore\n"
            "the game's EFLAGS; following Jcc/SETcc/CMOVcc/LOOPcc consume TFLAGS.\n"
            "Each T-using F0 owns one private bank that persists until disable/reset.\n\n"
            "F1 always uses two 32-bit words per data line. DEADCODE's NN\n"
            "keeps only the first NN bytes of the final line; the remaining\n"
            "bytes must be 00 padding and are not written. The generated JMP\n"
            "back is placed immediately after the final valid byte.\n\n"
            "Example - money hook at 0009D411\n"
            "    $F0000000  0009D411\n"
            "    $mov dword ptr [eax+58],054C5638\n"
            "    $mov eax,ecx\n"
            "    $DEADCODE\n\n"
            "F0 data/preserve example\n"
            "    $PRESERVE ECX, EDX\n"
            "    $mov edx,CarList\n"
            "    ...\n"
            "    $RESTORE\n"
            "    $DEADCODE\n"
            "    $CarList:\n"
            "    $dd 01D28710, 8B80C5FC, E3BDE8CB\n\n"
            "Equivalent F1 raw form\n"
            "    $F1000000  0009D411\n"
            "    $C7405838  564C058B\n"
            "    $C1000000  00000000\n"
            "    $DEADCODE  00000001");

    section("CMP-style File Layout",
            "^1 = Hash: <XBE Header SHA-256>\n"
            "^2 = GameID: EA009E\n"
            "     4541009E is also accepted\n"
            "^3 = NAME: Game Name\n\n"
            "!Group Name:\n"
            "    +Cheat Name{Optional Description}\n"
            "    %Credits: Author>Author2\n"
            "    $XXXXXXXX YYYYYYYY\n"
            "!!");

    ImGui::EndChild();
    if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void CheatEngineWindow::Draw(bool detached)
{
    if (!is_open) {
        return;
    }

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_MenuBar;
    const char *window_name = "RAW Cheat Engine";
    bool *window_open = &is_open;
    if (detached) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        window_name = "##DetachedRawCheatEngine";
        window_open = nullptr;
    } else {
        ImGui::SetNextWindowSize(ImVec2(940, 700), ImGuiCond_FirstUseEver);
    }
    if (!ImGui::Begin(window_name, window_open, window_flags)) {
        ImGui::End();
        return;
    }

    DrawMenuBar();
    DrawHelpPopup();

    current_game_manager.DrawInlineSummary("cheat-engine-current-game");
    if (!m_file_status.empty()) {
        ImGui::TextDisabled("%s", m_file_status.c_str());
    }
    if (!m_engine_enabled) {
        ImGui::TextDisabled("Engine is disabled in Options.");
    }
    ImGui::Separator();

    if (ImGui::Button("Reload (Cheat File)")) {
        ReloadCurrentCheatFile();
    }
    ImGui::SameLine();

    if (!m_parse_messages.empty()) {
        ImGui::TextDisabled("Parse messages: %zu", m_parse_messages.size());
        if (ImGui::IsItemHovered()) {
            std::string messages;
            for (const auto &msg : m_parse_messages) {
                if (!messages.empty()) messages += "\n";
                messages += msg;
            }
            ImGui::SetTooltip("%s", messages.c_str());
        }
    }
    if (!m_last_runtime_message.empty()) {
        ImGui::TextWrapped("Runtime: %s", m_last_runtime_message.c_str());
    }
    if (!m_last_preentry_message.empty()) {
        ImGui::TextWrapped("Patch: %s", m_last_preentry_message.c_str());
    }

    ImGui::Spacing();
    XemuDebugUi::ScopedTabStyle tab_style;
    if (ImGui::BeginTabBar("##RawCheatEngineTabs")) {
        if (ImGui::BeginTabItem("Cheats")) {
            if (m_live_cheats_enabled) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.22f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.55f, 0.27f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.38f, 0.19f, 1.00f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.20f, 0.20f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.24f, 0.24f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.16f, 0.16f, 1.00f));
            }
            if (ImGui::Button(m_live_cheats_enabled ? "Enabled" : "Disabled")) {
                SetLiveCheatsEnabled(!m_live_cheats_enabled);
            }
            ImGui::PopStyleColor(3);

            size_t active_count = 0;
            for (const auto &block : m_blocks) {
                if (!block.preentry && block.enabled) {
                    ++active_count;
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Active: %zu", active_count);

            const bool tree_disabled = !m_live_cheats_enabled || !m_engine_enabled;
            if (tree_disabled) {
                ImGui::BeginDisabled();
            }
            ImGui::BeginChild("##CheatTree", ImVec2(-1, -1), true);
            size_t selected = 0, live_total = 0;
            CountGroupSelection(0, selected, live_total);
            if (live_total == 0) {
                ImGui::TextDisabled("No live cheats loaded for the current game.");
            } else {
                DrawGroup(0);
            }
            ImGui::EndChild();
            if (tree_disabled) {
                ImGui::EndDisabled();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Patch")) {
            ImGui::TextWrapped(
                "WARNING: Codes activated here require a game reset to fully activate. "
                "Activated patches are applied during the next game startup.");
            ImGui::Separator();

            size_t patch_selected = 0, patch_total = 0;
            CountPatchGroupSelection(0, patch_selected, patch_total);
            size_t patch_applied = 0;
            size_t patch_reset_required = 0;
            size_t patch_failed = 0;
            for (const CheatBlock &block : m_blocks) {
                if (!block.preentry) {
                    continue;
                }
                patch_applied += block.preentry_applied ? 1u : 0u;
                patch_failed += block.preentry_error.empty() ? 0u : 1u;
                patch_reset_required +=
                    (block.selected != block.preentry_applied) ? 1u : 0u;
            }
            ImGui::TextDisabled(
                "Selected: %zu/%zu | Applied: %zu | Reset Required: %zu | Failed: %zu",
                patch_selected, patch_total, patch_applied, patch_reset_required,
                patch_failed);
            if (patch_selected != 0 && !m_engine_enabled) {
                ImGui::TextWrapped(
                    "Engine Enabled is OFF: selected startup Patches will not apply "
                    "until the master engine option is enabled and the game is reset.");
            }
            ImGui::BeginChild("##PatchTree", ImVec2(-1, -1), true);
            if (patch_total == 0) {
                ImGui::TextDisabled(
                    "No :PREENTRY: patch codes loaded for the current game.");
            } else {
                DrawPatchGroup(0);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    tab_style.Restore();

    ImGui::End();
}
