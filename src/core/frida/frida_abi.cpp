//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#include "core/frida/frida_abi.h"

#include <cctype>

#include "core/frida_release.h"
#include "platform/strings.h"

namespace CoreDeck::Core {
    namespace {
        std::string_view TrimView(std::string_view value) {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
                value.remove_prefix(1);
            }
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
                value.remove_suffix(1);
            }
            return value;
        }
    }

    const char *FridaAndroidAsset(const std::string_view abi) {
        const std::string_view trimmed = TrimView(abi);
        if (trimmed == "arm64-v8a") {
            return "android-arm64";
        }
        if (trimmed == "armeabi-v7a" || trimmed == "armeabi") {
            return "android-arm";
        }
        if (trimmed == "x86_64") {
            return "android-x86_64";
        }
        if (trimmed == "x86") {
            return "android-x86";
        }
        return nullptr;
    }

    const char *FirstFridaAndroidAsset(const std::string_view abiList) {
        std::size_t start = 0;
        while (start <= abiList.size()) {
            const std::size_t comma = abiList.find(',', start);
            const std::string_view item = abiList.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
            const char *asset = FridaAndroidAsset(item);
            if (asset != nullptr) {
                return asset;
            }
            if (comma == std::string_view::npos) {
                break;
            }
            start = comma + 1;
        }
        return nullptr;
    }

    std::string FridaServerArchiveName(const std::string_view asset) {
        const char *url = FridaServerUrl(asset);
        if (url == nullptr) {
            return {};
        }
        const std::string_view value(url);
        const std::size_t slash = value.rfind('/');
        if (slash == std::string_view::npos) {
            return std::string(value);
        }
        return std::string(value.substr(slash + 1));
    }

    const char *FridaServerUrl(const std::string_view asset) {
        if (asset == "android-arm") {
            return FRIDA_SERVER_URL_ANDROID_ARM;
        }
        if (asset == "android-arm64") {
            return FRIDA_SERVER_URL_ANDROID_ARM64;
        }
        if (asset == "android-x86") {
            return FRIDA_SERVER_URL_ANDROID_X86;
        }
        if (asset == "android-x86_64") {
            return FRIDA_SERVER_URL_ANDROID_X86_64;
        }
        return nullptr;
    }

    const char *FridaServerSha256(const std::string_view asset) {
        if (asset == "android-arm") {
            return FRIDA_SERVER_SHA256_ANDROID_ARM;
        }
        if (asset == "android-arm64") {
            return FRIDA_SERVER_SHA256_ANDROID_ARM64;
        }
        if (asset == "android-x86") {
            return FRIDA_SERVER_SHA256_ANDROID_X86;
        }
        if (asset == "android-x86_64") {
            return FRIDA_SERVER_SHA256_ANDROID_X86_64;
        }
        return nullptr;
    }

    std::optional<int> ParseForwardedPort(const std::string_view output) {
        const std::string_view trimmed = TrimView(output);
        if (trimmed.empty()) {
            return std::nullopt;
        }
        int port = 0;
        for (const char ch: trimmed) {
            if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
                return std::nullopt;
            }
            port = (port * 10) + (ch - '0');
            if (port > 65535) {
                return std::nullopt;
            }
        }
        if (port <= 0) {
            return std::nullopt;
        }
        return port;
    }

    std::string FridaDaemonCommand(const bool useSu) {
        std::string command = Platform::StrConcat(
            FRIDA_SERVER_REMOTE_PATH,
            " -D -l 127.0.0.1:",
            std::to_string(FRIDA_MANAGED_PORT),
            " >/dev/null 2>&1 </dev/null &"
        );
        if (!useSu) {
            return command;
        }
        return Platform::StrConcat("su -c '", command, "'");
    }

    std::string FridaKillCommand(const bool useSu) {
        std::string command = "pkill -f coredeck-frida-server";
        if (!useSu) {
            return command;
        }
        return Platform::StrConcat("su -c '", command, "'");
    }

    bool ShellIsRoot(const std::string_view idOutput) {
        return idOutput.find("uid=0") != std::string_view::npos;
    }

    FridaServerPresence FridaServersInProcessList(const std::string_view psOutput) {
        FridaServerPresence found;
        std::size_t start = 0;
        while (start < psOutput.size()) {
            const std::size_t end = psOutput.find('\n', start);
            const std::string_view line = psOutput.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
            if (line.find("coredeck-frida") != std::string_view::npos) {
                found.CoreDeck = true;
            } else if (line.find("frida-server") != std::string_view::npos) {
                found.Foreign = true;
            }
            if (end == std::string_view::npos) {
                break;
            }
            start = end + 1;
        }
        return found;
    }
}
