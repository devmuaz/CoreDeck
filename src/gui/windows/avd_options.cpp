//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"

#include "avd_options.h"
#include "../application.h"
#include "../widgets.h"
#include "../theme.h"

namespace CoreDeck {
    void BuildAvdOptionsWindow(Context &context) {
        if (!context.UI.ShowOptionsPanel) return;

        constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        std::string panelTitle = "Options";
        if (context.Catalog.SelectedAvd >= 0 && context.Catalog.SelectedAvd < context.Catalog.Avds.size()) {
            panelTitle = "Options - " + context.Catalog.Avds[context.Catalog.SelectedAvd].DisplayName;
        }

        ImGui::Begin(
            (panelTitle + "###Options").c_str(),
            nullptr,
            panelFlags
        );

        if (context.Catalog.SelectedAvd < 0) {
            ImGui::TextDisabled("Select an AVD to configure options");
            ImGui::End();
            return;
        }

        auto &options = GetDefaultAvdOptions(context);
        bool optionsChanged = false;

        std::vector<std::string> categories;
        for (const auto &option: options) {
            if (std::ranges::find(categories, option.Category) == categories.end()) {
                categories.push_back(option.Category);
            }
        }

        for (const auto &category: categories) {
            if (CollapsingHeader(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent(20.0f);

                for (auto &[Flag, DisplayName, Description, Enabled, Type, Category, Hint, TextInput, Items,
                         SelectedItem]
                     : options) {
                    if (category != Category) continue;

                    ImGui::PushID(Flag.c_str());

                    const bool wasEnabled = Enabled;
                    ImGui::Checkbox(DisplayName.c_str(), &Enabled);
                    if (wasEnabled != Enabled) optionsChanged = true;

                    ImGui::SameLine();
                    ImGui::TextColored(HexColor("#66666B"), Icons::Info);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Description.c_str());

                    if (Enabled) {
                        switch (Type) {
                            case OptionType::TextInput: {
                                ImGui::SetNextItemWidth(-1.0f);
                                char buffer[256];
                                strncpy(buffer, TextInput.c_str(), sizeof(buffer) - 1);
                                buffer[sizeof(buffer) - 1] = '\0';
                                if (ImGui::InputTextWithHint("##val", Hint.c_str(), buffer, sizeof(buffer))) {
                                    TextInput = buffer;
                                    optionsChanged = true;
                                }
                                break;
                            }

                            case OptionType::Selection: {
                                ImGui::SetNextItemWidth(-1.0f);
                                if (ImGui::BeginCombo("##selection", Items[SelectedItem].c_str())) {
                                    for (int i = 0; i < Items.size(); ++i) {
                                        const bool isSelected = SelectedItem == i;
                                        if (ImGui::Selectable(Items[i].c_str(), isSelected)) {
                                            SelectedItem = i;
                                            optionsChanged = true;
                                        }
                                        if (isSelected) ImGui::SetItemDefaultFocus();
                                    }
                                    ImGui::EndCombo();
                                }
                                break;
                            }

                            default:
                                break;
                        }
                    }

                    ImGui::PopID();
                }

                ImGui::Unindent(20.0f);
            }
        }

        if (optionsChanged) SaveAvdOptions(context, context.Catalog.Avds[context.Catalog.SelectedAvd].Name);
        ImGui::End();
    }
}
