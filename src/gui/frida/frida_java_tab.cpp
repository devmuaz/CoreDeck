//
// Created by AbdulMuaz Aqeel on 16/11/2026.
//

#include "gui/frida/frida_java_tab.h"

#include "gui/frida/frida_controller.h"
#include "gui/frida/frida_widgets.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"
#include "platform/strings.h"

namespace CoreDeck::Gui::Frida {
    namespace {
        void JavaToolbar(Context &context, FridaDeviceSession &session) {
            const std::string searchLabel = IconWithLabel(Icons::SEARCH, Platform::Tr("Search"));
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float buttonW = ActionWidth(searchLabel.c_str());
            const float fieldW = std::max(1.0F, ImGui::GetContentRegionAvail().x - buttonW - spacing);
            SearchField("##FridaJavaQuery", Platform::Tr("Class filter"), session.JavaQuery, sizeof(session.JavaQuery), fieldW);
            ImGui::SameLine(0.0F, spacing);
            if (PrimaryButton(searchLabel.c_str(), session.Attached, ImVec2(buttonW, 0.0F))) {
                RequestJavaClasses(context, session);
            }
        }

        void JavaBrowser(Context &context, FridaDeviceSession &session) {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const PickerTableStyle tableStyle;
            const float height = std::max(Em(8.0F), ImGui::GetContentRegionAvail().y);
            const float column = std::max(Em(16.0F), (ImGui::GetContentRegionAvail().x - spacing) * 0.5F);
            ImGui::BeginChild("##FridaJavaClasses", ImVec2(column, height), ImGuiChildFlags_None, ImGuiWindowFlags_None);
            const bool classesOpen = BeginPickerTable("##FridaClassFrame", "##FridaClasses", 1, std::max(1.0F, ImGui::GetContentRegionAvail().y));
            if (classesOpen) {
                const std::string classColumn = Platform::StrConcat("  ", Platform::Tr("Classes"));
                ImGui::TableSetupColumn(classColumn.c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0F);
                ImGui::TableHeadersRow();
                for (int index = 0; std::cmp_less(index, session.JavaClasses.size()); ++index) {
                    const std::string &name = session.JavaClasses.at(static_cast<std::size_t>(index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    const std::string label = Platform::StrConcat("  ", name, "##FridaClass", std::to_string(index));
                    if (ImGui::Selectable(label.c_str(), session.SelectedClass == index, ImGuiSelectableFlags_SpanAllColumns)) {
                        session.SelectedClass = index;
                        RequestJavaMethods(context, session, name);
                    }
                }
            }
            EndPickerTable(classesOpen);
            ImGui::EndChild();
            ImGui::SameLine(0.0F, spacing);
            ImGui::BeginChild("##FridaJavaMethods", ImVec2(0.0F, height), ImGuiChildFlags_None, ImGuiWindowFlags_None);
            const std::string hookLabel = IconWithLabel(Icons::PLUG, Platform::Tr("Hook Method"));
            const float hookW = ActionWidth(hookLabel.c_str());
            const float hookRow = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
            const bool methodsOpen = BeginPickerTable(
                "##FridaMethodFrame",
                "##FridaMethods",
                1,
                std::max(1.0F, ImGui::GetContentRegionAvail().y - hookRow)
            );
            if (methodsOpen) {
                const std::string methodColumn = Platform::StrConcat("  ", Platform::Tr("Methods"));
                ImGui::TableSetupColumn(methodColumn.c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0F);
                ImGui::TableHeadersRow();
                for (int index = 0; std::cmp_less(index, session.JavaMethods.size()); ++index) {
                    const std::string &name = session.JavaMethods.at(static_cast<std::size_t>(index));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    const std::string label = Platform::StrConcat("  ", name, "##FridaMethod", std::to_string(index));
                    if (ImGui::Selectable(label.c_str(), session.SelectedMethod == index, ImGuiSelectableFlags_SpanAllColumns)) {
                        session.SelectedMethod = index;
                    }
                }
            }
            EndPickerTable(methodsOpen);
            const bool canHook = session.SelectedClass >= 0 && session.SelectedMethod >= 0;
            if (PrimaryButton(hookLabel.c_str(), canHook, ImVec2(hookW, 0.0F))) {
                const std::string &className = session.JavaClasses.at(static_cast<std::size_t>(session.SelectedClass));
                const std::string &methodName = session.JavaMethods.at(static_cast<std::size_t>(session.SelectedMethod));
                CopyToBuffer(session.HookClass, sizeof(session.HookClass), className);
                CopyToBuffer(session.HookMethod, sizeof(session.HookMethod), methodName);
                session.HookNative = false;
                session.Tab = FridaWorkspaceTab::Hooks;
            }
            ImGui::EndChild();
        }
    }

    void BuildFridaJavaTab(Context &context, FridaDeviceSession &session) {
        JavaToolbar(context, session);
        SpaceBelowToolbar();
        JavaBrowser(context, session);
    }
}
