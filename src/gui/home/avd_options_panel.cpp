//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"

#include "gui/home/avd_options_panel.h"
#include "gui/shell/application.h"
#include "gui/widgets/widgets.h"
#include "platform/i18n.h"

namespace CoreDeck::Gui {
    namespace {
        constexpr ImGuiWindowFlags OPTIONS_PANEL_FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

        bool HasSelectedAvd(const Context &context) {
            const auto &catalog = context.Workspaces.Home.Catalog;
            return catalog.SelectedAvd >= 0 && std::cmp_less(catalog.SelectedAvd, catalog.Avds.size());
        }

        std::string OptionsPanelTitle(const Context &context) {
            if (!HasSelectedAvd(context)) {
                return Platform::TrWindow("Options", "Options");
            }
            const auto &avd = context.Workspaces.Home.Catalog.Avds.at(context.Workspaces.Home.Catalog.SelectedAvd);
            return Platform::StrConcat(Platform::TrFormat("Options - {0}", avd.DisplayName), "###Options");
        }

        std::vector<std::string> OptionCategories(const std::vector<Core::EmulatorOption> &options) {
            std::vector<std::string> categories;
            for (const auto &option: options) {
                if (std::ranges::find(categories, option.Category.value()) == categories.end()) {
                    categories.push_back(option.Category.value());
                }
            }
            return categories;
        }

        bool TextOption(Core::EmulatorOption &option) {
            ImGui::SetNextItemWidth(-1.0F);
            char buffer[256];
            CopyToBuffer(buffer, sizeof(buffer), option.InputValue.value());
            if (!ImGui::InputTextWithHint("##val", Platform::Tr(option.Hint.value().c_str()), buffer, sizeof(buffer))) {
                return false;
            }
            option.InputValue = buffer;
            return true;
        }

        bool SelectionOption(Core::EmulatorOption &option) {
            const std::vector<std::string> &items = option.Items.value();
            std::vector<const char *> labels;
            labels.reserve(items.size());
            for (const std::string &item: items) {
                labels.push_back(Core::EmulatorOptionItemDisplayLabel(option.Flag.value(), item));
            }
            return RoundedCombo(
                "##selection",
                labels.at(static_cast<std::size_t>(option.SelectedItem.value())),
                labels.data(),
                static_cast<int>(labels.size()),
                option.SelectedItem.value()
            );
        }

        bool OptionValue(Core::EmulatorOption &option) {
            switch (option.Type.value()) {
                case Core::OptionType::TextInput:
                    return TextOption(option);
                case Core::OptionType::Selection:
                    return SelectionOption(option);
                default:
                    return false;
            }
        }

        bool OptionRow(Core::EmulatorOption &option) {
            ImGui::PushID(option.Flag.value().c_str());
            const bool wasEnabled = option.Enabled.value();
            const std::string &description = option.Description.value();
            SubtitledCheckbox(
                option.Flag.value().c_str(),
                &option.Enabled.value(),
                Platform::Tr(option.DisplayName.value().c_str()),
                nullptr,
                description.empty() ? nullptr : Platform::Tr(description.c_str())
            );
            bool changed = wasEnabled != option.Enabled.value();
            if (option.Enabled.value()) {
                changed = OptionValue(option) || changed;
            }
            ImGui::PopID();
            return changed;
        }

        bool OptionCategory(const std::string &category, std::vector<Core::EmulatorOption> &options) {
            if (!CollapsingHeader(Platform::Tr(category.c_str()), ImGuiTreeNodeFlags_DefaultOpen)) {
                return false;
            }

            ImGui::Indent(20.0F);
            bool changed = false;
            for (auto &option: options) {
                if (option.Category.value() != category) {
                    continue;
                }
                if (OptionRow(option)) {
                    changed = true;
                }
            }
            ImGui::Unindent(20.0F);
            return changed;
        }
    }

    void BuildAvdOptionsPanel(Context &context) {
        if (!context.Shell.OptionsPanel) {
            return;
        }

        ImGui::Begin(OptionsPanelTitle(context).c_str(), nullptr, OPTIONS_PANEL_FLAGS);
        if (!HasSelectedAvd(context)) {
            ImGui::TextDisabled("%s", Platform::Tr("Select an AVD to configure options"));
            ImGui::End();
            return;
        }

        auto &options = GetDefaultAvdOptions(context);
        bool optionsChanged = false;
        for (const std::string &category: OptionCategories(options)) {
            if (OptionCategory(category, options)) {
                optionsChanged = true;
            }
        }
        if (optionsChanged) {
            SaveAvdOptions(context, context.Workspaces.Home.Catalog.Avds.at(context.Workspaces.Home.Catalog.SelectedAvd).Name);
        }
        ImGui::End();
    }
}
