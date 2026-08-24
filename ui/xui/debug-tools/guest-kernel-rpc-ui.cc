//
// xemu Debug Tools - Guest Kernel RPC diagnostics UI
//
// Rendering/dialog ownership split from guest-kernel-rpc.cc during the final
// Debug Tools production cleanup. Runtime scheduling and filesystem execution
// remain in the core/completion/filesystem translation units.
//
#include "guest-kernel-rpc.hh"
#include "guest-kernel-rpc-status.hh"
#include "kernel-rpc-utils.hh"

#include <imgui.h>


using XemuGuestKernelRpcStatus::IrqlName;
using XemuGuestKernelRpcStatus::NtStatusName;

void GuestKernelRpcManager::DrawTestPanel()
{
    ImGui::TextUnformatted("Experimental Guest Kernel RPC Foundation");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Developer diagnostics only. Production HDD Create/Import/Delete/Rename/Move/Copy operations now live in the HDD browser. This panel retains the harmless IRQL proof, read-only file test and namespace diagnostics needed to troubleshoot the Guest Kernel RPC foundation.");
    ImGui::Spacing();

    const bool busy = IsBusy();
    ImGui::BeginDisabled(busy);
    if (ImGui::Button("RUN HARMLESS IRQL TEST")) {
        StartIrqlTest();
    }
    ImGui::SameLine();
    if (ImGui::Button("RUN READ-ONLY FILE TEST")) {
        StartReadOnlyFsTest();
    }
    ImGui::SameLine();
    if (ImGui::Button("RUN PATH DIAGNOSTICS")) {
        StartPathDiagnostics();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Text("Kernel Base        %08X", XemuKernelRpc::kXboxKernelBase);
    ImGui::Text("IRQL Ordinal       %u  KeGetCurrentIrql",
                XemuKernelRpc::kKeGetCurrentIrqlOrdinal);
    ImGui::Text("IRQL Address       %08X", m_export_address);
    ImGui::Text("Query Ordinal      %u  NtQueryFullAttributesFile",
                XemuKernelRpc::kNtQueryFullAttributesFileOrdinal);
    ImGui::Text("Query Address      %08X", m_query_export_address);
    ImGui::Text("RPC Virtual Base   %08X", m_arena.virtual_base);
    ImGui::Text("RPC Physical Base  %016llX",
                (unsigned long long)m_arena.physical_base);
    ImGui::Text("RPC Completion     %08X", m_completion_address);
    if (m_irql != 0xffffffffu) {
        ImGui::Text("Returned IRQL      %u  %s", m_irql, IrqlName(m_irql));
    }

    if (!m_fs_path.empty()) {
        ImGui::Separator();
        ImGui::Text("FATX Reference     %s", m_fs_path.c_str());
        if (!m_query_path.empty()) {
            ImGui::Text("Active Query       %s", m_query_path.c_str());
            ImGui::Text("Object Root        %08X%s", m_query_root_directory,
                        m_query_root_directory == XemuKernelRpc::kObDosDevicesDirectory
                            ? "  ObDosDevicesDirectory" : "  fully qualified");
            ImGui::Text("ANSI Length        %04X", m_query_ansi_length);
            ImGui::Text("ANSI Maximum       %04X", m_query_ansi_maximum);
            ImGui::Text("ANSI Buffer        %08X", m_query_ansi_buffer);
            ImGui::Text("Object Name        %08X", m_query_object_name);
            ImGui::Text("Object Attributes  %08X", m_query_object_attributes);
        }
        ImGui::Text("Safe Samples       %u", m_safe_point_attempts);
        ImGui::Text("Last Sample EIP    %08X", m_last_sample_eip);
        if (m_fs_test_mode != FsTestMode::KernelDeleteRecursiveFolder &&
            m_fs_test_mode != FsTestMode::KernelImportFolder) {
            ImGui::Text("Kernel NTSTATUS    %08X  %s", m_query_status,
                        NtStatusName(m_query_status));
            ImGui::Text("FATX Size          %llu", (unsigned long long)m_expected_file_size);
            ImGui::Text("Kernel Size        %llu", (unsigned long long)m_kernel_file_size);
            ImGui::Text("FATX Attributes    %02X", m_expected_attributes);
            ImGui::Text("Kernel Attributes  %08X", m_kernel_attributes);
        } else {
            ImGui::Text("Selected Size      %llu", (unsigned long long)m_expected_file_size);
            ImGui::Text("Selected Attrs     %02X", m_expected_attributes);
        }
    }

    if (!m_path_diagnostics.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Xbox Path / Namespace Diagnostics");
        if (ImGui::BeginTable("##KernelRpcPathDiagnostics", 7,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX)) {
            ImGui::TableSetupColumn("Test");
            ImGui::TableSetupColumn("Path");
            ImGui::TableSetupColumn("Root");
            ImGui::TableSetupColumn("NTSTATUS");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Attrs");
            ImGui::TableSetupColumn("Samples");
            ImGui::TableHeadersRow();
            for (size_t i = 0; i < m_path_diagnostics.size(); ++i) {
                const PathDiagnostic &item = m_path_diagnostics[i];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (i == m_path_diagnostic_index && IsBusy()) {
                    ImGui::Text("-> %s", item.label.c_str());
                } else {
                    ImGui::TextUnformatted(item.label.c_str());
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(item.path.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%08X", item.root_directory);
                ImGui::TableSetColumnIndex(3);
                if (item.completed) {
                    const char *name = NtStatusName(item.ntstatus);
                    if (name[0] != '\0') {
                        ImGui::Text("%08X %s", item.ntstatus, name);
                    } else {
                        ImGui::Text("%08X", item.ntstatus);
                    }
                } else {
                    ImGui::TextUnformatted("--------");
                }
                ImGui::TableSetColumnIndex(4);
                if (item.completed && (int32_t)item.ntstatus >= 0) {
                    ImGui::Text("%llu", (unsigned long long)item.file_size);
                } else {
                    ImGui::TextUnformatted("-");
                }
                ImGui::TableSetColumnIndex(5);
                if (item.completed && (int32_t)item.ntstatus >= 0) {
                    ImGui::Text("%08X", item.attributes);
                } else {
                    ImGui::TextUnformatted("-");
                }
                ImGui::TableSetColumnIndex(6);
                ImGui::Text("%u", item.safe_samples);
            }
            ImGui::EndTable();
        }
    }

    // Destructive create/delete test controls were retired from the visible
    // diagnostics surface after the production HDD browser paths were proven.
    // Their underlying compatibility helpers remain available to historical
    // regression tests, but normal users have one filesystem UI/ownership path.

    ImGui::Spacing();
    if (m_state == State::Passed) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "%s", m_status.c_str());
    } else if (m_state == State::Failed) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "%s", m_status.c_str());
    } else {
        ImGui::TextWrapped("%s", m_status.c_str());
    }
    ImGui::Spacing();
    ImGui::TextDisabled("The RPC arena remains separate from the 0x68000000 Type-F/CodeCave arena. Kernel Delete and Kernel Import use the native \\Device\\Harddisk0\\Partition1 namespace. Kernel Import is create-only and never falls back to raw FATX writes.");
}
