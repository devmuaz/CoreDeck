//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#ifndef COREDECK_CORE_FRIDA_FRIDA_MESSAGES_H
#define COREDECK_CORE_FRIDA_FRIDA_MESSAGES_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CoreDeck::Core {
    enum class FridaConsoleKind : uint8_t {
        Log,
        Send,
        Error,
        Hook,
    };

    struct FridaParsedMessage {
        bool Rpc = false;
        std::uint64_t Id = 0;
        bool Ok = false;
        bool HasBytes = false;
        std::string Error;
        std::string ResultJson;
        std::string Console;
        FridaConsoleKind ConsoleKind = FridaConsoleKind::Log;
        bool IsHook = false;
        std::uint64_t HookId = 0;
    };

    struct FridaModuleInfo {
        std::string Name;
        std::string Base;
        std::uint64_t Size = 0;
        std::string Path;
    };

    struct FridaExportInfo {
        std::string Name;
        std::string Address;
        std::string Type;
    };

    struct FridaHookInfo {
        std::uint64_t Id = 0;
        std::string Target;
    };

    struct FridaScanHit {
        std::string Address;
        std::uint64_t Size = 0;
    };

    struct FridaAgentCall {
        std::uint64_t Id = 0;
        std::string Method;
        std::string Module;
        std::string Symbol;
        std::string Address;
        std::string Query;
        std::string ClassName;
        std::string Pattern;
        std::string Start;
        std::uint32_t Size = 0;
        std::uint64_t HookId = 0;
        bool LogArgs = false;
        bool Backtrace = false;
    };

    std::string FridaAgentRequest(const FridaAgentCall &call);

    FridaParsedMessage ParseFridaMessage(std::string_view json);

    std::vector<FridaModuleInfo> ParseFridaModules(const std::string &json);

    std::vector<FridaExportInfo> ParseFridaExports(const std::string &json);

    std::vector<std::string> ParseFridaNames(const std::string &json, std::string_view field);

    std::vector<FridaHookInfo> ParseFridaHooks(const std::string &json);

    FridaHookInfo ParseFridaHook(const std::string &json);

    std::vector<FridaScanHit> ParseFridaHits(const std::string &json);

    std::optional<std::string> FridaNormalizeHex(std::string_view input);

    std::string FridaHexPattern(std::string_view text, bool utf16);

    bool FridaLooksLikeHex(std::string_view input);
}

#endif // COREDECK_CORE_FRIDA_FRIDA_MESSAGES_H
