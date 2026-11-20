#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "core/avd/emulator_options.h"

using namespace CoreDeck::Core;

namespace {
    std::filesystem::path TempOptionsFile(const char *name) {
        const auto path = std::filesystem::temp_directory_path() / name;
        std::error_code error;
        std::filesystem::remove(path, error);
        return path;
    }

    std::string ReadFile(const std::filesystem::path &path) {
        std::ifstream in(path);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }
}

TEST_CASE("emulator options serialize with snake_case keys", "[emulator_options]") {
    const auto path = TempOptionsFile("coredeck-emulator-options.json");
    EmulatorOption option;
    option.Flag = "-dpi-device";
    option.DisplayName = "Device DPI";
    option.Enabled = true;
    option.Type = OptionType::TextInput;
    option.InputValue = "420";
    option.SelectedItem = 2;

    SaveOptionsToFile(path.string(), {option});
    const std::string json = ReadFile(path);
    REQUIRE(json.find("\"flag\":\"-dpi-device\"") != std::string::npos);
    REQUIRE(json.find("\"display_name\":\"Device DPI\"") != std::string::npos);
    REQUIRE(json.find("\"input_value\":\"420\"") != std::string::npos);
    REQUIRE(json.find("\"selected_item\":2") != std::string::npos);
    REQUIRE(json.find("InputValue") == std::string::npos);
    REQUIRE(json.find("SelectedItem") == std::string::npos);
    REQUIRE(json.find("DisplayName") == std::string::npos);

    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST_CASE("old emulator option files are removed", "[emulator_options]") {
    const auto path = TempOptionsFile("coredeck-emulator-options-old.json");
    {
        std::ofstream out(path);
        REQUIRE(out.is_open());
        out << R"([{"Flag":"-gpu","DisplayName":"GPU Mode","InputValue":"host","SelectedItem":1}])";
    }

    const std::vector<EmulatorOption> options = LoadOptionsFromFile(path.string());
    REQUIRE_FALSE(std::filesystem::exists(path));
    REQUIRE(options.size() > 1);
    REQUIRE(options.front().Flag.value() == "-gpu");
    REQUIRE(options.front().SelectedItem.value() == 0);
}
