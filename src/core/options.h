//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef EMU_LAUNCHER_OPTIONS_H
#define EMU_LAUNCHER_OPTIONS_H

#include <string>
#include <vector>
#include <rfl.hpp>

namespace CoreDeck {
    enum class OptionType : uint8_t {
        Default = 0,
        TextInput,
        Selection,
    };

    namespace OptionCategory {
        constexpr const char *DISPLAY = "Display";
        constexpr const char *PERFORMANCE = "Performance";
        constexpr const char *BOOT = "Boot";
        constexpr const char *AUDIO = "Audio";
        constexpr const char *NETWORK = "Network";
        constexpr const char *ADVANCED = "Advanced";
        constexpr const char *CAMERA = "Camera";
        constexpr const char *SYSTEM = "System";
        constexpr const char *LOCATION = "Location";
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

    struct OptionValueLabel {
        const char *Label;
        const char *Value;
    };

    const std::vector<OptionValueLabel> &GpuModeOptions();

    const char *GpuModeDisplayLabel(const std::string &value);

    const char *ScreenModeDisplayLabel(const std::string &value);

    const char *EmulatorOptionItemDisplayLabel(const std::string &flag, const std::string &value);

    std::vector<EmulatorOption> GetEmulatorOptions();

    std::vector<std::string> BuildArgs(const std::string &avdName, const std::vector<EmulatorOption> &options);

    void SaveOptionsToFile(const std::string &filePath, const std::vector<EmulatorOption> &options);

    std::vector<EmulatorOption> LoadOptionsFromFile(const std::string &filePath);

    std::string GetOptionsConfigPath(const std::string &avdName);

    void EnsureOptionsConfigDirectoryExists();
}

#endif // EMU_LAUNCHER_OPTIONS_H
