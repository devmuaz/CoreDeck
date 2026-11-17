//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#include "gui/frida/frida_modules_tab.h"

#include <cmath>
#include <string>
#include <vector>

#include "gui/frida/frida_controller.h"
#include "gui/frida/frida_widgets.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"
#include "platform/strings.h"

namespace CoreDeck::Gui::Frida {
    namespace {
        void ModuleToolbar(Context &context, FridaDeviceSession &session) {
            const std::string refreshLabel = IconWithLabel(Icons::REFRESH, Platform::Tr("Refresh"));
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float buttonW = ActionWidth(refreshLabel.c_str());
            const float fieldW = std::max(1.0F, ImGui::GetContentRegionAvail().x - buttonW - spacing);
            SearchField("##FridaModuleFilter", Platform::Tr("Filter modules"), session.ModuleFilter, sizeof(session.ModuleFilter), fieldW);
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(refreshLabel.c_str(), session.Attached, ImVec2(buttonW, 0.0F))) {
                RequestFridaModules(context, session);
            }
        }

        float FittedColumnWidth(const char *header, const std::vector<std::string> &values) {
            constexpr float SIDE_GAP = 4.0F;
            float width = ImGui::CalcTextSize("0x5a5299204000").x;
            width = std::max(width, ImGui::CalcTextSize(header != nullptr ? header : "").x);
            for (const std::string &value: values) {
                width = std::max(width, ImGui::CalcTextSize(value.c_str()).x);
            }
            return std::ceil(width + (SIDE_GAP * 2.0F));
        }

        void RememberExport(FridaDeviceSession &session, const FridaExportRow &item) {
            CopyToBuffer(session.HookAddress, sizeof(session.HookAddress), item.Address);
            CopyToBuffer(session.HookSymbol, sizeof(session.HookSymbol), item.Name);
            if (session.SelectedModule >= 0 && std::cmp_less(session.SelectedModule, session.Modules.size())) {
                CopyToBuffer(
                    session.HookModule,
                    sizeof(session.HookModule),
                    session.Modules.at(static_cast<std::size_t>(session.SelectedModule)).Name
                );
            }
            CopyToBuffer(session.MemoryAddress, sizeof(session.MemoryAddress), item.Address);
        }

        void ModuleBrowser(Context &context, FridaDeviceSession &session) {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const PickerTableStyle tableStyle;
            const float height = std::max(Em(8.0F), ImGui::GetContentRegionAvail().y);
            const float column = std::max(Em(18.0F), (ImGui::GetContentRegionAvail().x - spacing) * 0.46F);
            ImGui::BeginChild("##FridaModuleColumn", ImVec2(column, height), ImGuiChildFlags_None, ImGuiWindowFlags_None);
            const bool modulesOpen = BeginPickerTable("##FridaModuleFrame", "##FridaModules", 2, std::max(1.0F, ImGui::GetContentRegionAvail().y));
            if (modulesOpen) {
                const std::string moduleColumn = Platform::StrConcat("  ", Platform::Tr("Module"));
                std::vector<std::string> bases;
                bases.reserve(session.Modules.size());
                for (const FridaModuleRow &module: session.Modules) {
                    bases.push_back(module.Base);
                }
                ImGui::TableSetupColumn(moduleColumn.c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0F);
                ImGui::TableSetupColumn(
                    Platform::Tr("Base"),
                    ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize,
                    FittedColumnWidth(Platform::Tr("Base"), bases)
                );
                ImGui::TableHeadersRow();
                for (int index = 0; std::cmp_less(index, session.Modules.size()); ++index) {
                    const FridaModuleRow &module = session.Modules.at(static_cast<std::size_t>(index));
                    if (session.ModuleFilter[0] != '\0' && !Platform::ContainsIgnoreCase(module.Name, session.ModuleFilter)) {
                        continue;
                    }
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    const std::string label = Platform::StrConcat("  ", module.Name, "##FridaModule", std::to_string(index));
                    if (ImGui::Selectable(label.c_str(), session.SelectedModule == index, ImGuiSelectableFlags_SpanAllColumns)) {
                        session.SelectedModule = index;
                        RequestFridaExports(context, session, module.Name);
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(module.Base.c_str());
                }
            }
            EndPickerTable(modulesOpen);
            ImGui::EndChild();
            ImGui::SameLine(0.0F, spacing);
            ImGui::BeginChild("##FridaExportColumn", ImVec2(0.0F, height), ImGuiChildFlags_None, ImGuiWindowFlags_None);
            const std::string memoryLabel = IconWithLabel(Icons::EYE, Platform::Tr("Open in Memory"));
            const std::string hookLabel = IconWithLabel(Icons::PLUG, Platform::Tr("Hook Export"));
            const float memoryW = ActionWidth(memoryLabel.c_str());
            const float hookW = ActionWidth(hookLabel.c_str());
            const float actionRow = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
            const bool exportsOpen = BeginPickerTable(
                "##FridaExportFrame",
                "##FridaExports",
                2,
                std::max(1.0F, ImGui::GetContentRegionAvail().y - actionRow)
            );
            if (exportsOpen) {
                const std::string exportColumn = Platform::StrConcat("  ", Platform::Tr("Export"));
                std::vector<std::string> addresses;
                addresses.reserve(session.Exports.size());
                for (const FridaExportRow &item: session.Exports) {
                    addresses.push_back(item.Address);
                }
                ImGui::TableSetupColumn(exportColumn.c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0F);
                ImGui::TableSetupColumn(
                    Platform::Tr("Address"),
                    ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize,
                    FittedColumnWidth(Platform::Tr("Address"), addresses)
                );
                ImGui::TableHeadersRow();
                for (int index = 0; std::cmp_less(index, session.Exports.size()); ++index) {
                    const FridaExportRow &item = session.Exports.at(static_cast<std::size_t>(index));
                    const bool chosen = session.HookSymbol == item.Name && std::string(session.MemoryAddress) == item.Address;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    const std::string nameLabel = Platform::StrConcat("  ", item.Name, "##FridaExport", std::to_string(index));
                    if (ImGui::Selectable(nameLabel.c_str(), chosen, ImGuiSelectableFlags_SpanAllColumns)) {
                        RememberExport(session, item);
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(item.Address.c_str());
                }
            }
            EndPickerTable(exportsOpen);
            if (PickerButton(memoryLabel.c_str(), session.MemoryAddress[0] != '\0', ImVec2(memoryW, 0.0F))) {
                RequestFridaRead(context, session);
                session.Tab = FridaWorkspaceTab::Memory;
            }
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(hookLabel.c_str(), session.HookSymbol[0] != '\0', ImVec2(hookW, 0.0F))) {
                session.HookNative = true;
                session.Tab = FridaWorkspaceTab::Hooks;
            }
            ImGui::EndChild();
        }
    }

    void BuildFridaModulesTab(Context &context, FridaDeviceSession &session) {
        ModuleToolbar(context, session);
        SpaceBelowToolbar();
        ModuleBrowser(context, session);
    }
}
