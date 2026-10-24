//
// Created by AbdulMuaz Aqeel on 04/04/2026.
//

#ifndef COREDECK_OPTIONS_H
#define COREDECK_OPTIONS_H

#include <string>
#include <vector>
#include <rfl.hpp>

#include "platform/i18n.h"

namespace CoreDeck::Core {
    enum class OptionType : uint8_t {
        Default = 0,
        TextInput,
        Selection,
    };

    namespace OptionCategory {
        constexpr const char *DISPLAY = Platform::TrNoop("Display");
        constexpr const char *PERFORMANCE = Platform::TrNoop("Performance");
        constexpr const char *BOOT = Platform::TrNoop("Boot");
        constexpr const char *AUDIO = Platform::TrNoop("Audio");
        constexpr const char *NETWORK = Platform::TrNoop("Network");
        constexpr const char *ADVANCED = Platform::TrNoop("Advanced");
        constexpr const char *CAMERA = Platform::TrNoop("Camera");
        constexpr const char *SYSTEM = Platform::TrNoop("System");
        constexpr const char *LOCATION = Platform::TrNoop("Location");
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

}

#endif // COREDECK_OPTIONS_H
