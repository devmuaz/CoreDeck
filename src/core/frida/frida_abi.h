//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#ifndef COREDECK_CORE_FRIDA_FRIDA_ABI_H
#define COREDECK_CORE_FRIDA_FRIDA_ABI_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace CoreDeck::Core {
    inline constexpr int FRIDA_MANAGED_PORT = 27099;
    inline constexpr int FRIDA_EXISTING_PORT = 27042;
    inline constexpr const char *FRIDA_SERVER_REMOTE_PATH = "/data/local/tmp/coredeck-frida-server";
    inline constexpr std::uint32_t FRIDA_READ_LIMIT = 1024U * 1024U;
    inline constexpr std::uint32_t FRIDA_PAGE_SIZE = 4096U;
    inline constexpr std::uint32_t FRIDA_SCAN_LIMIT = 16U * 1024U * 1024U;

    const char *FridaAndroidAsset(std::string_view abi);

    const char *FirstFridaAndroidAsset(std::string_view abiList);

    std::string FridaServerArchiveName(std::string_view asset);

    const char *FridaServerUrl(std::string_view asset);

    const char *FridaServerSha256(std::string_view asset);

    std::optional<int> ParseForwardedPort(std::string_view output);

    std::string FridaDaemonCommand(bool useSu);

    std::string FridaKillCommand(bool useSu);

    bool ShellIsRoot(std::string_view idOutput);

    struct FridaServerPresence {
        bool CoreDeck = false;
        bool Foreign = false;
    };

    FridaServerPresence FridaServersInProcessList(std::string_view psOutput);
}

#endif // COREDECK_CORE_FRIDA_FRIDA_ABI_H
