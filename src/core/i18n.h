//
// Created by AbdulMuaz Aqeel on 29/09/2026.
//

#ifndef COREDECK_I18N_H
#define COREDECK_I18N_H

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "utilities.h"

namespace CoreDeck {
    void SetLocalesDirectory(std::string directory);

    void SetLanguage(const std::string &preference);

    std::string LanguagePreference();

    std::string ActiveLanguage();

    std::string SystemLanguage();

    std::vector<std::string> AvailableLanguages();

    std::string LanguageEndonym(const std::string &tag);

    const char *Tr(const char *msgid);

    const char *TrC(const char *context, const char *msgid);

    const char *TrN(const char *singular, const char *plural, int count);

    std::string FormatTranslated(const char *msgid, const std::vector<std::string> &args);

    std::string FormatTranslatedPlural(
        const char *singular,
        const char *plural,
        int count,
        const std::vector<std::string> &args
    );

    inline std::string ToFormatArg(const std::string &value) {
        return value;
    }

    inline std::string ToFormatArg(std::string_view value) {
        return std::string(value);
    }

    inline std::string ToFormatArg(const char *value) {
        return value == nullptr ? std::string() : std::string(value);
    }

    template<typename T>
    std::string ToFormatArg(T value)
        requires(std::is_arithmetic_v<T>)
    {
        return std::to_string(value);
    }

    template<typename... Args>
    std::string TrFormat(const char *msgid, Args &&...args) {
        std::vector<std::string> values;
        values.reserve(sizeof...(Args));
        (values.push_back(ToFormatArg(std::forward<Args>(args))), ...);
        return FormatTranslated(msgid, values);
    }

    template<typename... Args>
    std::string TrFormatN(const char *singular, const char *plural, int count, Args &&...args) {
        std::vector<std::string> values;
        values.reserve(sizeof...(Args));
        (values.push_back(ToFormatArg(std::forward<Args>(args))), ...);
        return FormatTranslatedPlural(singular, plural, count, values);
    }

    inline std::string TrWindow(const char *title, const char *id) {
        const char *translated = Tr(title);
        if (translated == title && std::string_view(title) == id) {
            return title;
        }
        return StrConcat(translated, "###", id);
    }

    constexpr const char *TrNoop(const char *message) {
        return message;
    }
}

#endif // COREDECK_I18N_H
