//
// Created by AbdulMuaz Aqeel on 06/05/2026.
//

#ifndef COREDECK_LOG_FILTER_H
#define COREDECK_LOG_FILTER_H

#include <cstddef>
#include <string>
#include <vector>

namespace CoreDeck {
    struct LogFilterOptions {
        std::string Query;
        bool UseRegex = false;
        bool CaseSensitive = false;
    };

    struct LogMatch {
        std::size_t StartOffset = 0;
        std::size_t EndOffset = 0;
    };

    struct LogFilterResult {
        std::string Joined;
        std::vector<LogMatch> Matches;
        bool RegexValid = true;
        std::string RegexError;
    };

    LogFilterResult FilterLog(const std::vector<std::string> &lines, const LogFilterOptions &options);
}

#endif // COREDECK_LOG_FILTER_H