//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include "imgui.h"

#include "avd_options.h"
#include <cstddef>
#include "../application.h"
#include "../utilities.h"
#include "../widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    void BuildAvdOptionsWindow(Context &context) {
        if (!context.UI.ShowOptionsPanel) {
            return;
        }

        constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        const bool hasSelection = context.Catalog.SelectedAvd >= 0 && context.Catalog.SelectedAvd < context.Catalog.Avds.size();
        const std::string panelTitle = hasSelection
                                           ? Platform::StrConcat(Platform::TrFormat("Options - {0}", context.Catalog.Avds.at(context.Catalog.SelectedAvd).DisplayName), "###Options")
                                           : Platform::TrWindow("Options", "Options");

        ImGui::Begin(panelTitle.c_str(), nullptr, FLAGS);

        if (!hasSelection) {
            ImGui::TextDisabled("%s", Platform::Tr("Select an AVD to configure options"));
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
            if (CollapsingHeader(Platform::Tr(categoryItem.c_str()), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent(20.0F);

                for (auto &[flag, displayName, description, enabled, type, category, hint, textInput, items, selectedItem]: options) {
                    if (categoryItem != category) {
                        continue;
                    }

                    ImGui::PushID(flag.c_str());

                    const bool wasEnabled = enabled;
                    SubtitledCheckbox(flag.c_str(), &enabled, Platform::Tr(displayName.c_str()), nullptr, description.empty() ? nullptr : Platform::Tr(description.c_str()));
                    if (wasEnabled != enabled) {
                        optionsChanged = true;
                    }

                    if (enabled) {
                        switch (type) {
                            case Core::OptionType::TextInput: {
                                ImGui::SetNextItemWidth(-1.0F);
                                char buffer[256];
                                CopyToBuffer(buffer, sizeof(buffer), textInput);
                                if (ImGui::InputTextWithHint("##val", Platform::Tr(hint.c_str()), buffer, sizeof(buffer))) {
                                    textInput = buffer;
                                    optionsChanged = true;
                                }
                                break;
                            }

                            case Core::OptionType::Selection: {
                                std::vector<const char *> labels;
                                labels.reserve(items.size());
                                for (const std::string &item: items) {
                                    labels.push_back(Core::EmulatorOptionItemDisplayLabel(flag, item));
                                }
                                if (RoundedCombo(
                                        "##selection",
                                        labels.at(static_cast<std::size_t>(selectedItem)),
                                        labels.data(),
                                        static_cast<int>(labels.size()),
                                        selectedItem
                                    )) {
                                    optionsChanged = true;
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
