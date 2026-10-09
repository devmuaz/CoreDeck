//
// Created by AbdulMuaz Aqeel on 29/09/2026.
//

#include "i18n.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <shared_mutex>

#include <libintl.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace CoreDeck {
    namespace {
        constexpr const char *TEXT_DOMAIN = "coredeck";
        constexpr const char *TEXT_DOMAIN_FILE = "coredeck.mo";

        struct State {
            std::shared_mutex Mutex;
            std::string Directory;
            std::string Preference;
            std::string Active = "en";
        };

        State &GetState() {
            static State state;
            return state;
        }

        std::string NormalizeLanguageTag(std::string tag) {
            for (char &ch: tag) {
                if (ch == '_') {
                    ch = '-';
                }
            }
            const auto encoding = tag.find('.');
            if (encoding != std::string::npos) {
                tag.resize(encoding);
            }
            const auto modifier = tag.find('@');
            if (modifier != std::string::npos) {
                tag.resize(modifier);
            }
            while (!tag.empty() && tag.back() == '-') {
                tag.pop_back();
            }
            if (tag.empty() || tag == "C" || tag == "POSIX") {
                return "en";
            }
            return tag;
        }

        std::string PosixLanguageTag(std::string tag) {
            for (char &ch: tag) {
                if (ch == '-') {
                    ch = '_';
                }
            }
            return tag;
        }

        bool IsLanguageToken(const std::string &tag) {
            if (tag.empty() || tag.size() > 32) {
                return false;
            }
            return std::ranges::all_of(tag, [](const char ch) {
                const auto value = static_cast<unsigned char>(ch);
                return std::isalnum(value) != 0 || ch == '-' || ch == '_';
            });
        }

        void AddCandidate(std::vector<std::string> &candidates, const std::string &tag) {
            if (tag.empty()) {
                return;
            }
            if (std::ranges::find(candidates, tag) == candidates.end()) {
                candidates.push_back(tag);
            }
        }

        std::vector<std::string> LanguageCandidates(const std::string &tag) {
            std::vector<std::string> candidates;
            AddCandidate(candidates, tag);
            if (tag == "zh-CN" || tag == "zh-SG") {
                AddCandidate(candidates, "zh-Hans");
            } else if (tag == "zh-TW" || tag == "zh-HK" || tag == "zh-MO") {
                AddCandidate(candidates, "zh-Hant");
            }

            std::vector<std::string> parts;
            std::size_t start = 0;
            while (start < tag.size()) {
                const auto dash = tag.find('-', start);
                if (dash == std::string::npos) {
                    parts.push_back(tag.substr(start));
                    break;
                }
                parts.push_back(tag.substr(start, dash - start));
                start = dash + 1;
            }
            if (parts.size() >= 3) {
                AddCandidate(candidates, parts.at(0) + "-" + parts.at(1));
            }
            if (!parts.empty()) {
                AddCandidate(candidates, parts.at(0));
            }
            return candidates;
        }

        std::filesystem::path CatalogFile(const std::filesystem::path &root, const std::string &tag) {
            std::error_code error;
            std::filesystem::path direct = root / tag / "LC_MESSAGES" / TEXT_DOMAIN_FILE;
            if (std::filesystem::is_regular_file(direct, error)) {
                return direct;
            }
            const std::string posix = PosixLanguageTag(tag);
            std::filesystem::path alternate = root / posix / "LC_MESSAGES" / TEXT_DOMAIN_FILE;
            if (alternate != direct && std::filesystem::is_regular_file(alternate, error)) {
                return alternate;
            }
            return {};
        }

#ifdef _WIN32
        using CrtPutEnvS = int(__cdecl *)(const char *, const char *);
        using CrtGetEnv = char *(__cdecl *) (const char *);

        struct UniversalCrt {
            CrtPutEnvS PutEnv = nullptr;
            CrtGetEnv GetEnv = nullptr;
        };

        // intl-8.dll is built against the Universal CRT. CoreDeck.exe links the
        // static CRT, so _putenv_s here updates a different environment than the
        // one libintl's getenv reads. Language switches then stay on English.
        const UniversalCrt &GetUniversalCrt() {
            static const UniversalCrt crt = []() {
                UniversalCrt found;
                const char *modules[] = {
                    "ucrtbase.dll",
                    "api-ms-win-crt-environment-l1-1-0.dll",
                };
                for (const char *module: modules) {
                    const HMODULE handle = GetModuleHandleA(module);
                    if (handle == nullptr) {
                        continue;
                    }
                    found.PutEnv = reinterpret_cast<CrtPutEnvS>(GetProcAddress(handle, "_putenv_s"));
                    found.GetEnv = reinterpret_cast<CrtGetEnv>(GetProcAddress(handle, "getenv"));
                    if (found.PutEnv != nullptr) {
                        break;
                    }
                }
                return found;
            }();
            return crt;
        }

        void PublishEnvironment(const char *name, const char *value) {
            _putenv_s(name, value);
            if (const CrtPutEnvS put = GetUniversalCrt().PutEnv; put != nullptr) {
                put(name, value);
            }
        }
#endif

        bool IsCLocaleName(const char *value) {
            if (value == nullptr || value[0] == '\0') {
                return false;
            }
            return std::strcmp(value, "C") == 0 || std::strcmp(value, "POSIX") == 0 || std::strncmp(value, "C.", 2) == 0;
        }

        void PublishLanguage(const std::string &tag) {
            const std::string posix = PosixLanguageTag(tag);
            std::string languages = "en";
            if (IsLanguageToken(tag)) {
                languages = tag;
                if (IsLanguageToken(posix) && posix != tag) {
                    languages += ':';
                    languages += posix;
                }
            }
            const char *catalog = IsLanguageToken(tag) ? tag.c_str() : "en";
#ifdef _WIN32
            if (const CrtGetEnv get = GetUniversalCrt().GetEnv; get != nullptr && IsCLocaleName(get("LC_ALL"))) {
                // libintl ignores LANGUAGE while LC_ALL is the C locale.
                PublishEnvironment("LC_ALL", "");
            }
            PublishEnvironment("LANGUAGE", languages.c_str());
            // The catalog folder is the tag itself (zh-Hans, not zh_Hans). libintl
            // uses this name when LANGUAGE is absent.
            PublishEnvironment("LC_MESSAGES", catalog);
            PublishEnvironment("LANG", catalog);
#else
            (void) catalog;
            setenv("LANGUAGE", languages.c_str(), 1); // NOLINT(concurrency-mt-unsafe)
#endif
        }

        void ActivateMessageLocale() {
#ifdef _WIN32
            setlocale(LC_MESSAGES, ""); // NOLINT(concurrency-mt-unsafe)
#else
            if (const char *lcAll = std::getenv("LC_ALL"); IsCLocaleName(lcAll)) { // NOLINT(concurrency-mt-unsafe)
                unsetenv("LC_ALL"); // NOLINT(concurrency-mt-unsafe)
            }
            const char *applied = setlocale(LC_MESSAGES, ""); // NOLINT(concurrency-mt-unsafe)
            if (applied != nullptr && !IsCLocaleName(applied)) {
                return;
            }
            // libintl ignores LANGUAGE while LC_MESSAGES is C or C.UTF-8.
            // A UTF-8 messages locale still leaves LC_NUMERIC on the decimal dot.
            const char *candidates[] = {"en_US.UTF-8", "en_US.utf8", "en_US"};
            for (const char *candidate: candidates) {
                setenv("LC_MESSAGES", candidate, 1); // NOLINT(concurrency-mt-unsafe)
                applied = setlocale(LC_MESSAGES, ""); // NOLINT(concurrency-mt-unsafe)
                if (applied != nullptr && !IsCLocaleName(applied)) {
                    return;
                }
            }
            unsetenv("LC_MESSAGES"); // NOLINT(concurrency-mt-unsafe)
            (void) setlocale(LC_MESSAGES, ""); // NOLINT(concurrency-mt-unsafe)
#endif
        }

        void BindDomain(const std::string &directory) {
            if (directory.empty()) {
                return;
            }
            bindtextdomain(TEXT_DOMAIN, directory.c_str());
            bind_textdomain_codeset(TEXT_DOMAIN, "UTF-8");
            textdomain(TEXT_DOMAIN);
        }

#ifdef _WIN32
        std::string NarrowUtf16(const std::wstring &value) {
            if (value.empty()) {
                return {};
            }
            const int size = WideCharToMultiByte(
                CP_UTF8,
                0,
                value.c_str(),
                static_cast<int>(value.size()),
                nullptr,
                0,
                nullptr,
                nullptr
            );
            if (size <= 0) {
                return {};
            }
            std::string text(static_cast<std::size_t>(size), '\0');
            WideCharToMultiByte(
                CP_UTF8,
                0,
                value.c_str(),
                static_cast<int>(value.size()),
                text.data(),
                size,
                nullptr,
                nullptr
            );
            return text;
        }
#endif

        std::string DetectSystemLanguage() {
#ifdef _WIN32
            ULONG count = 0;
            ULONG length = 0;
            if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, nullptr, &length) || length == 0) {
                return "en";
            }
            std::wstring names(length, L'\0');
            if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, names.data(), &length)) {
                return "en";
            }
            const auto end = names.find(L'\0');
            const std::wstring first = end == std::wstring::npos ? names : names.substr(0, end);
            const std::string tag = NarrowUtf16(first);
            return tag.empty() ? "en" : NormalizeLanguageTag(tag);
#elif __APPLE__
            CFArrayRef languages = CFLocaleCopyPreferredLanguages();
            if (languages == nullptr || CFArrayGetCount(languages) == 0) {
                if (languages != nullptr) {
                    CFRelease(languages);
                }
                return "en";
            }
            const auto *value = static_cast<CFStringRef>(CFArrayGetValueAtIndex(languages, 0));
            char buffer[128];
            const bool copied = CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8) != 0u;
            CFRelease(languages);
            if (!copied) {
                return "en";
            }
            return NormalizeLanguageTag(buffer);
#else
            const char *variables[] = {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"};
            for (const char *name: variables) {
                const char *value = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
                if (value == nullptr || value[0] == '\0') {
                    continue;
                }
                std::string first = value;
                const auto colon = first.find(':');
                if (colon != std::string::npos) {
                    first.resize(colon);
                }
                if (first.empty() || first == "C" || first == "POSIX") {
                    continue;
                }
                return NormalizeLanguageTag(std::move(first));
            }
            return "en";
#endif
        }

        std::string SystemLanguageTag() {
            static const std::string TAG = DetectSystemLanguage();
            return TAG;
        }

        std::optional<std::string> TryApplyPlaceholders(const std::string &pattern, const std::vector<std::string> &args) {
            std::string result;
            for (std::size_t index = 0; index < pattern.size();) {
                if (pattern.at(index) == '{' && index + 1 < pattern.size() && pattern.at(index + 1) == '{') {
                    result.push_back('{');
                    index += 2;
                    continue;
                }
                if (pattern.at(index) == '}' && index + 1 < pattern.size() && pattern.at(index + 1) == '}') {
                    result.push_back('}');
                    index += 2;
                    continue;
                }
                if (pattern.at(index) != '{') {
                    result.push_back(pattern.at(index));
                    ++index;
                    continue;
                }
                std::size_t cursor = index + 1;
                if (cursor >= pattern.size() || std::isdigit(static_cast<unsigned char>(pattern.at(cursor))) == 0) {
                    return std::nullopt;
                }
                int argIndex = 0;
                while (cursor < pattern.size() && std::isdigit(static_cast<unsigned char>(pattern.at(cursor))) != 0) {
                    argIndex = (argIndex * 10) + (pattern.at(cursor) - '0');
                    ++cursor;
                }
                if (cursor >= pattern.size() || pattern.at(cursor) != '}' || argIndex < 0 ||
                    static_cast<std::size_t>(argIndex) >= args.size()) {
                    return std::nullopt;
                }
                result += args.at(static_cast<std::size_t>(argIndex));
                index = cursor + 1;
            }
            return result;
        }

        const char *LookupMessage(const char *msgid) {
            if (msgid == nullptr) {
                return "";
            }
            const char *translated = dgettext(TEXT_DOMAIN, msgid);
            if (translated == nullptr || translated[0] == '\0') {
                return msgid;
            }
            return translated;
        }

        const char *LookupContext(const char *context, const char *msgid) {
            if (msgid == nullptr) {
                return "";
            }
            if (context == nullptr || context[0] == '\0') {
                return LookupMessage(msgid);
            }
            const std::string key = std::string(context) + '\x04' + msgid;
            const char *translated = dcgettext(TEXT_DOMAIN, key.c_str(), LC_MESSAGES);
            if (translated == nullptr || translated == key.c_str() || translated[0] == '\0') {
                return msgid;
            }
            return translated;
        }

        const char *LookupPlural(const char *singular, const char *plural, const int count) {
            if (singular == nullptr) {
                return "";
            }
            const char *fallback = count == 1 || plural == nullptr ? singular : plural;
            const auto amount = static_cast<uint64_t>(count < 0 ? 0 : count);
            const char *translated = dngettext(TEXT_DOMAIN, singular, plural == nullptr ? singular : plural, amount);
            if (translated == nullptr || translated[0] == '\0') {
                return fallback;
            }
            return translated;
        }
    }

    void SetLocalesDirectory(std::string directory) {
        std::string preference;
        {
            auto &state = GetState();
            std::unique_lock lock(state.Mutex);
            state.Directory = std::move(directory);
            preference = state.Preference;
        }
        SetLanguage(preference);
    }

    void SetLanguage(const std::string &preference) {
        const std::string requested = preference.empty() ? SystemLanguageTag() : NormalizeLanguageTag(preference);
        const std::vector<std::string> candidates = LanguageCandidates(requested);

        auto &state = GetState();
        std::unique_lock lock(state.Mutex);
        state.Preference = preference;

        std::string selected = "en";
        if (!state.Directory.empty()) {
            const std::filesystem::path root(state.Directory);
            for (const std::string &tag: candidates) {
                if (!CatalogFile(root, tag).empty()) {
                    selected = tag;
                    break;
                }
            }
        }

        PublishLanguage(selected);
        BindDomain(state.Directory);
        ActivateMessageLocale();
        state.Active = selected;
    }

    std::string LanguagePreference() {
        auto &state = GetState();
        std::shared_lock lock(state.Mutex);
        return state.Preference;
    }

    std::string ActiveLanguage() {
        auto &state = GetState();
        std::shared_lock lock(state.Mutex);
        return state.Active;
    }

    std::string SystemLanguage() {
        return SystemLanguageTag();
    }

    std::vector<std::string> AvailableLanguages() {
        std::string directory;
        {
            auto &state = GetState();
            std::shared_lock lock(state.Mutex);
            directory = state.Directory;
        }
        std::vector<std::string> tags;
        std::error_code error;
        if (directory.empty() || !std::filesystem::is_directory(directory, error)) {
            return tags;
        }
        for (const auto &entry: std::filesystem::directory_iterator(directory, error)) {
            if (error || !entry.is_directory()) {
                continue;
            }
            const std::filesystem::path catalog = entry.path() / "LC_MESSAGES" / TEXT_DOMAIN_FILE;
            if (!std::filesystem::is_regular_file(catalog, error)) {
                continue;
            }
            tags.push_back(NormalizeLanguageTag(entry.path().filename().string()));
        }
        std::ranges::sort(tags);
        tags.erase(std::ranges::unique(tags).begin(), tags.end());
        return tags;
    }

    std::string LanguageEndonym(const std::string &tag) {
        const std::string normalized = NormalizeLanguageTag(tag);
        static constexpr struct {
            const char *Tag;
            const char *Name;
        } NAMES[] = {
            {.Tag = "en", .Name = "English"},
            {.Tag = "zh-CN", .Name = "简体中文"},
            {.Tag = "zh-Hans", .Name = "简体中文"},
            {.Tag = "zh-SG", .Name = "简体中文"},
            {.Tag = "zh-TW", .Name = "繁體中文"},
            {.Tag = "zh-Hant", .Name = "繁體中文"},
            {.Tag = "zh-HK", .Name = "繁體中文（香港）"},
            {.Tag = "zh-MO", .Name = "繁體中文"},
            {.Tag = "ja", .Name = "日本語"},
            {.Tag = "ko", .Name = "한국어"},
            {.Tag = "ar", .Name = "العربية"},
            {.Tag = "de", .Name = "Deutsch"},
            {.Tag = "es", .Name = "Español"},
            {.Tag = "fr", .Name = "Français"},
            {.Tag = "it", .Name = "Italiano"},
            {.Tag = "pl", .Name = "Polski"},
            {.Tag = "pt-BR", .Name = "Português (Brasil)"},
            {.Tag = "pt", .Name = "Português"},
            {.Tag = "ru", .Name = "Русский"},
            {.Tag = "tr", .Name = "Türkçe"},
            {.Tag = "uk", .Name = "Українська"},
        };
        for (const auto &name: NAMES) {
            if (normalized == name.Tag) {
                return name.Name;
            }
        }
        return tag;
    }

    const char *Tr(const char *msgid) {
        auto &state = GetState();
        std::shared_lock lock(state.Mutex);
        return LookupMessage(msgid);
    }

    const char *TrC(const char *context, const char *msgid) {
        auto &state = GetState();
        std::shared_lock lock(state.Mutex);
        return LookupContext(context, msgid);
    }

    const char *TrN(const char *singular, const char *plural, const int count) {
        auto &state = GetState();
        std::shared_lock lock(state.Mutex);
        return LookupPlural(singular, plural, count);
    }

    std::string FormatTranslated(const char *msgid, const std::vector<std::string> &args) {
        if (msgid == nullptr) {
            return {};
        }
        std::string pattern;
        {
            auto &state = GetState();
            std::shared_lock lock(state.Mutex);
            pattern = LookupMessage(msgid);
        }
        if (const std::optional<std::string> formatted = TryApplyPlaceholders(pattern, args)) {
            return *formatted;
        }
        if (pattern != msgid) {
            if (const std::optional<std::string> english = TryApplyPlaceholders(msgid, args)) {
                return *english;
            }
        }
        return msgid;
    }

    std::string FormatTranslatedPlural(
        const char *singular,
        const char *plural,
        const int count,
        const std::vector<std::string> &args
    ) {
        if (singular == nullptr) {
            return {};
        }
        const char *fallback = count == 1 || plural == nullptr ? singular : plural;
        std::string pattern;
        {
            auto &state = GetState();
            std::shared_lock lock(state.Mutex);
            pattern = LookupPlural(singular, plural, count);
        }
        if (const std::optional<std::string> formatted = TryApplyPlaceholders(pattern, args)) {
            return *formatted;
        }
        if (const std::optional<std::string> english = TryApplyPlaceholders(fallback, args)) {
            return *english;
        }
        return fallback;
    }
}
