//
// Created by AbdulMuaz Aqeel on 06/05/2026.
//

#include <regex>

#include "log_filter.h"
#include "utilities.h"

namespace CoreDeck {
    namespace {
        void AppendLine(std::string &out, const std::string &line) {
            out.append(line);
            out.push_back('\n');
        }

        void CollectSubstringMatches(const std::string &line, const std::size_t &lineStart, const std::string &needle, const bool &caseSensitive, std::vector<LogMatch> &out) {
            if (needle.empty()) {
                return;
            }

            const std::string haystack = caseSensitive ? line : LowerCopy(line);
            const std::string pattern = caseSensitive ? needle : LowerCopy(needle);

            std::size_t pos = 0;
            while ((pos = haystack.find(pattern, pos)) != std::string::npos) {
                LogMatch m;
                m.StartOffset = lineStart + pos;
                m.EndOffset = m.StartOffset + pattern.size();
                out.push_back(m);
                pos += pattern.size();
            }
        }

        void CollectRegexMatches(const std::string &line, const std::size_t lineStart, const std::regex &re, std::vector<LogMatch> &out) {
            const auto end = std::sregex_iterator{};
            for (auto it = std::sregex_iterator(line.begin(), line.end(), re); it != end; ++it) {
                if (it->length() == 0) {
                    continue;
                }
                LogMatch m;
                m.StartOffset = lineStart + static_cast<std::size_t>(it->position());
                m.EndOffset = m.StartOffset + static_cast<std::size_t>(it->length());
                out.push_back(m);
            }
        }
    }


    LogFilterResult FilterLog(const std::vector<std::string> &lines, const LogFilterOptions &options) {
        LogFilterResult result;
        result.Joined.reserve(lines.size() * 80);

        const bool hasQuery = !options.Query.empty();

        std::regex compiled;
        if (hasQuery && options.UseRegex) {
            try {
                auto flags = std::regex::ECMAScript;
                if (!options.CaseSensitive) {
                    flags |= std::regex::icase;
                }
                compiled = std::regex(options.Query, flags);
            } catch (const std::regex_error &e) {
                result.RegexValid = false;
                result.RegexError = e.what();
            }
        }

        const bool collectMatches = hasQuery && (options.UseRegex ? result.RegexValid : true);

        for (const auto &line: lines) {
            const std::size_t lineStart = result.Joined.size();
            if (collectMatches) {
                if (options.UseRegex) {
                    CollectRegexMatches(line, lineStart, compiled, result.Matches);
                } else {
                    CollectSubstringMatches(line, lineStart, options.Query, options.CaseSensitive, result.Matches);
                }
            }
            AppendLine(result.Joined, line);
        }

        return result;
    }
}