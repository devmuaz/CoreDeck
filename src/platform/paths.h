//
// Created by AbdulMuaz Aqeel on 09/04/2026.
//

#ifndef COREDECK_PATHS_H
#define COREDECK_PATHS_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace CoreDeck::Platform {
    enum class Os : uint8_t {
        Windows,
        MacOS,
        Linux,
        Unknown,
    };

    Os GetCurrentOs();

    const char *GetOsName();

    std::string GetHomeDirectory();

    std::string GetConfigDirectory();

    std::string_view GetNullDevice();

    std::string_view GetExecutableExtension();

    inline std::string WithExecutableExtension(const std::string_view name) {
        std::string result(name);
        result.append(GetExecutableExtension());
        return result;
    }

    std::string GetExecutableDirectory();

    std::string JoinPaths(const std::vector<std::string> &components);

    std::string NormalizePath(const std::string &path);

    bool SameFilePath(const std::string &left, const std::string &right);
}

#endif // COREDECK_PATHS_H
