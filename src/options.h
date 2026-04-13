//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_OPTIONS_H
#define EMU_LAUNCHER_OPTIONS_H

#include <string>
#include <vector>
#include <rfl.hpp>

namespace CoreDeck {
    enum class OptionType {
        Default = 0,
        TextInput,
        Selection,
    };

    namespace OptionCategory {
        constexpr const char *Display = "Display";
        constexpr const char *Performance = "Performance";
        constexpr const char *Boot = "Boot";
        constexpr const char *Audio = "Audio";
        constexpr const char *Network = "Network";
        constexpr const char *Advanced = "Advanced";
        constexpr const char *Camera = "Camera";
    }

    struct EmulatorOption {
        std::string Flag;
        std::string DisplayName;
        std::string Description;
        bool Enabled = false;
        OptionType Type;
        std::string Category;

        // TextInput
        std::string Hint;
        std::string InputValue;

        // Selection
        std::vector<std::string> Items;
        int SelectedItem = 0;
    };

    std::vector<EmulatorOption> GetEmulatorOptions();

    std::vector<std::string> BuildArgs(const std::string &avdName, const std::vector<EmulatorOption> &options);

    void SaveOptionsToFile(const std::string &filePath, const std::vector<EmulatorOption> &options);

    std::vector<EmulatorOption> LoadOptionsFromFile(const std::string &filePath);

    std::string GetOptionsConfigPath(const std::string &avdName);

    void EnsureOptionsConfigDirectoryExists();
}

#endif //EMU_LAUNCHER_OPTIONS_H
