//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"

#include "avd_options.h"
#include <cstddef>
#include "../application.h"
#include "../widgets.h"

namespace CoreDeck {
    void BuildAvdOptionsWindow(Context &context) {
        if (!context.UI.ShowOptionsPanel) {
            return;
        }

        constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        std::string panelTitle = "Options";
        if (context.Catalog.SelectedAvd >= 0 && context.Catalog.SelectedAvd < context.Catalog.Avds.size()) {
            panelTitle = "Options - " + context.Catalog.Avds.at(context.Catalog.SelectedAvd).DisplayName;
        }

        ImGui::Begin((panelTitle + "###Options").c_str(), nullptr, FLAGS);

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

        for (const auto &categoryItem: categories) {
            if (CollapsingHeader(categoryItem.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent(20.0F);

                for (auto &[flag, displayName, description, enabled, type, category, hint, textInput, items, selectedItem]: options) {
                    if (categoryItem != category) {
                        continue;
                    }

                    ImGui::PushID(flag.c_str());

                    const bool wasEnabled = enabled;
                    SubtitledCheckbox(flag.c_str(), &enabled, displayName.c_str(), nullptr, description.c_str());
                    if (wasEnabled != enabled) {
                        optionsChanged = true;
                    }

                    if (enabled) {
                        switch (type) {
                            case OptionType::TextInput: {
                                ImGui::SetNextItemWidth(-1.0F);
                                char buffer[256];
                                strncpy(buffer, textInput.c_str(), sizeof(buffer) - 1);
                                buffer[sizeof(buffer) - 1] = '\0';
                                if (ImGui::InputTextWithHint("##val", hint.c_str(), buffer, sizeof(buffer))) {
                                    textInput = buffer;
                                    optionsChanged = true;
                                }
                                break;
                            }

                            case OptionType::Selection: {
                                ImGui::SetNextItemWidth(-1.0F);
                                const char *selectedLabel = EmulatorOptionItemDisplayLabel(flag, items.at(selectedItem));
                                ComboStyle cs;
                                if (ImGui::BeginCombo("##selection", selectedLabel)) {
                                    for (int i = 0; i < items.size(); ++i) {
                                        const bool isSelected = selectedItem == i;
                                        const char *itemLabel = EmulatorOptionItemDisplayLabel(flag, items.at(i));
                                        if (RoundedSelectable(itemLabel, isSelected)) {
                                            selectedItem = i;
                                            optionsChanged = true;
                                        }
                                        if (isSelected) {
                                            ImGui::SetItemDefaultFocus();
                                        }
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

                ImGui::Unindent(20.0F);
            }
        }

        if (optionsChanged) {
            SaveAvdOptions(context, context.Catalog.Avds.at(context.Catalog.SelectedAvd).Name);
        }
        ImGui::End();
    }
}
