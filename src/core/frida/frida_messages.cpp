//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#include "core/frida/frida_messages.h"

#include <cctype>

#include <rfl/Generic.hpp>
#include <rfl/json.hpp>

namespace CoreDeck::Core {
    namespace {
        struct ModuleRecord {
            std::string name; // NOLINT(readability-identifier-naming)
            std::string base; // NOLINT(readability-identifier-naming)
            std::uint64_t size = 0; // NOLINT(readability-identifier-naming)
            std::string path; // NOLINT(readability-identifier-naming)
        };

        struct ModuleBody {
            std::vector<ModuleRecord> modules; // NOLINT(readability-identifier-naming)
        };

        struct ExportRecord {
            std::string name; // NOLINT(readability-identifier-naming)
            std::string address; // NOLINT(readability-identifier-naming)
            std::string type; // NOLINT(readability-identifier-naming)
        };

        struct ExportBody {
            std::vector<ExportRecord> exports; // NOLINT(readability-identifier-naming)
        };

        struct NameBody {
            std::vector<std::string> classes; // NOLINT(readability-identifier-naming)
            std::vector<std::string> methods; // NOLINT(readability-identifier-naming)
        };

        struct HookRecord {
            std::uint64_t id = 0; // NOLINT(readability-identifier-naming)
            std::string target; // NOLINT(readability-identifier-naming)
        };

        struct HookBody {
            std::vector<HookRecord> hooks; // NOLINT(readability-identifier-naming)
        };

        struct HitRecord {
            std::string address; // NOLINT(readability-identifier-naming)
            std::uint64_t size = 0; // NOLINT(readability-identifier-naming)
        };

        struct HitBody {
            std::vector<HitRecord> hits; // NOLINT(readability-identifier-naming)
        };

        void AppendJsonString(std::string &out, const std::string_view value) {
            out.push_back('"');
            for (const unsigned char ch: value) {
                switch (ch) {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        if (ch < 0x20U) {
                            constexpr std::string_view HEX = "0123456789abcdef";
                            out += "\\u00";
                            out.push_back(HEX.at(ch >> 4U));
                            out.push_back(HEX.at(ch & 0x0FU));
                        } else {
                            out.push_back(static_cast<char>(ch));
                        }
                        break;
                }
            }
            out.push_back('"');
        }

        const rfl::Generic *Field(const rfl::Generic::Object &object, const std::string_view key) {
            for (const auto &entry: object) {
                if (entry.first == key) {
                    return &entry.second;
                }
            }
            return nullptr;
        }

        std::string AsString(const rfl::Generic &value) {
            if (const auto text = value.to_string()) {
                return text.value();
            }
            return {};
        }

        std::uint64_t AsUint(const rfl::Generic &value) {
            if (const auto number = value.to_int64()) {
                return number.value() < 0 ? 0 : static_cast<std::uint64_t>(number.value());
            }
            return 0;
        }

        bool AsBool(const rfl::Generic &value) {
            if (const auto *flag = std::get_if<bool>(&value.get())) {
                return *flag;
            }
            return false;
        }

        std::string WriteValue(const rfl::Generic &value) {
            try {
                return rfl::json::write(value);
            } catch (...) {
                return {};
            }
        }
    }

    std::string FridaAgentRequest(const FridaAgentCall &call) {
        std::string json = R"({"type":"coredeck","id":)";
        json += std::to_string(call.Id);
        json += ",\"method\":";
        AppendJsonString(json, call.Method);
        json += ",\"module\":";
        AppendJsonString(json, call.Module);
        json += ",\"symbol\":";
        AppendJsonString(json, call.Symbol);
        json += ",\"address\":";
        AppendJsonString(json, call.Address);
        json += ",\"query\":";
        AppendJsonString(json, call.Query);
        json += ",\"className\":";
        AppendJsonString(json, call.ClassName);
        json += ",\"pattern\":";
        AppendJsonString(json, call.Pattern);
        json += ",\"start\":";
        AppendJsonString(json, call.Start);
        json += ",\"size\":";
        json += std::to_string(call.Size);
        json += ",\"hookId\":";
        json += std::to_string(call.HookId);
        json += ",\"logArgs\":";
        json += call.LogArgs ? "true" : "false";
        json += ",\"backtrace\":";
        json += call.Backtrace ? "true" : "false";
        json.push_back('}');
        return json;
    }

    FridaParsedMessage ParseFridaMessage(const std::string_view json) {
        FridaParsedMessage parsed;
        const auto root = rfl::json::read<rfl::Generic>(json);
        if (!root) {
            parsed.ConsoleKind = FridaConsoleKind::Error;
            parsed.Console = "Unreadable Frida message.";
            return parsed;
        }
        const auto object = root.value().to_object();
        if (!object) {
            parsed.Console = std::string(json);
            return parsed;
        }
        const rfl::Generic *typeField = Field(object.value(), "type");
        const std::string type = typeField != nullptr ? AsString(*typeField) : std::string{};
        const rfl::Generic *payloadField = Field(object.value(), "payload");
        if (type == "error") {
            parsed.ConsoleKind = FridaConsoleKind::Error;
            const rfl::Generic *description = Field(object.value(), "description");
            const rfl::Generic *stack = Field(object.value(), "stack");
            parsed.Console = description != nullptr ? AsString(*description) : std::string(json);
            if (stack != nullptr) {
                const std::string stackText = AsString(*stack);
                if (!stackText.empty()) {
                    parsed.Console.push_back('\n');
                    parsed.Console += stackText;
                }
            }
            return parsed;
        }
        if (type == "log") {
            parsed.ConsoleKind = FridaConsoleKind::Log;
            parsed.Console = payloadField != nullptr ? AsString(*payloadField) : std::string{};
            if (parsed.Console.empty() && payloadField != nullptr) {
                parsed.Console = WriteValue(*payloadField);
            }
            return parsed;
        }
        if (payloadField == nullptr) {
            parsed.Console = std::string(json);
            return parsed;
        }
        const auto payload = payloadField->to_object();
        if (!payload) {
            parsed.ConsoleKind = FridaConsoleKind::Send;
            parsed.Console = AsString(*payloadField);
            if (parsed.Console.empty()) {
                parsed.Console = WriteValue(*payloadField);
            }
            return parsed;
        }
        const rfl::Generic *kind = Field(payload.value(), "kind");
        if (kind != nullptr && AsString(*kind) == "hook") {
            parsed.IsHook = true;
            parsed.ConsoleKind = FridaConsoleKind::Hook;
            const rfl::Generic *hookId = Field(payload.value(), "hookId");
            const rfl::Generic *text = Field(payload.value(), "text");
            parsed.HookId = hookId != nullptr ? AsUint(*hookId) : 0;
            parsed.Console = text != nullptr ? AsString(*text) : std::string{};
            return parsed;
        }
        const rfl::Generic *id = Field(payload.value(), "id");
        const rfl::Generic *ok = Field(payload.value(), "ok");
        if (id != nullptr && ok != nullptr) {
            parsed.Rpc = true;
            parsed.Id = AsUint(*id);
            parsed.Ok = AsBool(*ok);
            const rfl::Generic *error = Field(payload.value(), "error");
            const rfl::Generic *bytes = Field(payload.value(), "bytes");
            const rfl::Generic *result = Field(payload.value(), "result");
            parsed.HasBytes = bytes != nullptr && AsBool(*bytes);
            parsed.Error = error != nullptr ? AsString(*error) : std::string{};
            if (result != nullptr && !parsed.HasBytes) {
                parsed.ResultJson = WriteValue(*result);
            }
            return parsed;
        }
        parsed.ConsoleKind = FridaConsoleKind::Send;
        parsed.Console = WriteValue(*payloadField);
        return parsed;
    }

    std::vector<FridaModuleInfo> ParseFridaModules(const std::string &json) {
        const auto parsed = rfl::json::read<ModuleBody>(json);
        if (!parsed) {
            return {};
        }
        std::vector<FridaModuleInfo> modules;
        modules.reserve(parsed.value().modules.size());
        for (const ModuleRecord &record: parsed.value().modules) {
            modules.push_back({.Name = record.name, .Base = record.base, .Size = record.size, .Path = record.path});
        }
        return modules;
    }

    std::vector<FridaExportInfo> ParseFridaExports(const std::string &json) {
        const auto parsed = rfl::json::read<ExportBody>(json);
        if (!parsed) {
            return {};
        }
        std::vector<FridaExportInfo> exports;
        exports.reserve(parsed.value().exports.size());
        for (const ExportRecord &record: parsed.value().exports) {
            exports.push_back({.Name = record.name, .Address = record.address, .Type = record.type});
        }
        return exports;
    }

    std::vector<std::string> ParseFridaNames(const std::string &json, const std::string_view field) {
        const auto parsed = rfl::json::read<NameBody>(json);
        if (!parsed) {
            return {};
        }
        if (field == "methods") {
            return parsed.value().methods;
        }
        return parsed.value().classes;
    }

    std::vector<FridaHookInfo> ParseFridaHooks(const std::string &json) {
        const auto parsed = rfl::json::read<HookBody>(json);
        if (!parsed) {
            return {};
        }
        std::vector<FridaHookInfo> hooks;
        hooks.reserve(parsed.value().hooks.size());
        for (const HookRecord &record: parsed.value().hooks) {
            hooks.push_back({.Id = record.id, .Target = record.target});
        }
        return hooks;
    }

    FridaHookInfo ParseFridaHook(const std::string &json) {
        const auto parsed = rfl::json::read<HookRecord>(json);
        if (!parsed) {
            return {};
        }
        return {.Id = parsed.value().id, .Target = parsed.value().target};
    }

    std::vector<FridaScanHit> ParseFridaHits(const std::string &json) {
        const auto parsed = rfl::json::read<HitBody>(json);
        if (!parsed) {
            return {};
        }
        std::vector<FridaScanHit> hits;
        hits.reserve(parsed.value().hits.size());
        for (const HitRecord &record: parsed.value().hits) {
            hits.push_back({.Address = record.address, .Size = record.size});
        }
        return hits;
    }

    bool FridaLooksLikeHex(const std::string_view input) {
        bool sawDigit = false;
        for (const unsigned char ch: input) {
            if (std::isspace(ch) != 0) {
                continue;
            }
            if (std::isxdigit(ch) != 0) {
                sawDigit = true;
                continue;
            }
            if (ch == '?') {
                continue;
            }
            return false;
        }
        return sawDigit;
    }

    std::optional<std::string> FridaNormalizeHex(const std::string_view input) {
        std::string compact;
        compact.reserve(input.size());
        for (std::size_t index = 0; index < input.size();) {
            const auto ch = static_cast<unsigned char>(input.at(index));
            if (std::isspace(ch) != 0) {
                ++index;
                continue;
            }
            if (ch == '?') {
                if (index + 1 >= input.size() || input.at(index + 1) != '?') {
                    return std::nullopt;
                }
                compact += "??";
                index += 2;
                continue;
            }
            if (std::isxdigit(ch) == 0) {
                return std::nullopt;
            }
            if (index + 1 >= input.size() || std::isxdigit(static_cast<unsigned char>(input.at(index + 1))) == 0) {
                return std::nullopt;
            }
            compact.push_back(static_cast<char>(std::tolower(ch)));
            compact.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(input.at(index + 1)))));
            index += 2;
        }
        if (compact.empty()) {
            return std::nullopt;
        }
        std::string spaced;
        for (std::size_t index = 0; index < compact.size(); index += 2) {
            if (!spaced.empty()) {
                spaced.push_back(' ');
            }
            spaced.append(compact, index, 2);
        }
        return spaced;
    }

    std::string FridaHexPattern(const std::string_view text, const bool utf16) {
        std::string pattern;
        for (const unsigned char ch: text) {
            if (!pattern.empty()) {
                pattern.push_back(' ');
            }
            constexpr std::string_view HEX = "0123456789abcdef";
            pattern.push_back(HEX.at(ch >> 4U));
            pattern.push_back(HEX.at(ch & 0x0FU));
            if (utf16) {
                pattern += " 00";
            }
        }
        return pattern;
    }
}
