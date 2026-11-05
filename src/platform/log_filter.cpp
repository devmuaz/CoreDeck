//
// Created by AbdulMuaz Aqeel on 06/05/2026.
//

#include <regex>
#include <utility>

#include "log_filter.h"
#include "utilities.h"

namespace CoreDeck::Platform {
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

        struct CompiledQuery {
            bool Valid = true;
            std::string Error;
            std::regex Pattern;
        };

        CompiledQuery CompileQuery(const std::string &query, const bool caseSensitive) {
            CompiledQuery compiled;
            try {
                auto flags = std::regex::ECMAScript;
                if (!caseSensitive) {
                    flags |= std::regex::icase;
                }
                compiled.Pattern = std::regex(query, flags);
            } catch (const std::regex_error &error) {
                compiled.Valid = false;
                compiled.Error = error.what();
            }
            return compiled;
        }

        bool LineMatches(const std::string &line, const LogFilterOptions &options, const std::regex *pattern) {
            if (options.LineFilterRegex) {
                return pattern != nullptr && std::regex_search(line, *pattern);
            }
            if (options.CaseSensitive) {
                return line.find(options.LineFilter) != std::string::npos;
            }
            return ContainsIgnoreCase(line, options.LineFilter);
        }

        void CollectRegexMatches(const std::string &line, const std::size_t lineStart, const std::regex &re, std::vector<LogMatch> &out) {
            const auto end = std::sregex_iterator{};
            for (auto it = std::sregex_iterator(line.begin(), line.end(), re); it != end; ++it) {
                // empty() is true only when the match failed. A zero-width hit such as "^" still has one submatch.
                if (it->length() == 0) { // NOLINT(readability-container-size-empty)
                    continue;
                }
                LogMatch m;
                m.StartOffset = lineStart + static_cast<std::size_t>(it->position());
                m.EndOffset = m.StartOffset + static_cast<std::size_t>(it->length());
                out.push_back(m);
            }
        }
    }


    LogQueryCheck CheckLogQuery(const std::string &query, const bool useRegex, const bool caseSensitive) {
        LogQueryCheck check;
        if (!useRegex || query.empty()) {
            return check;
        }
        const CompiledQuery compiled = CompileQuery(query, caseSensitive);
        check.Valid = compiled.Valid;
        check.Error = compiled.Error;
        return check;
    }

    LogFilterResult FilterLog(const std::vector<std::string> &lines, const LogFilterOptions &options) {
        LogFilterResult result;
        const std::vector<std::string> *visible = &lines;
        std::vector<std::string> filtered;
        if (!options.LineFilter.empty()) {
            const CompiledQuery compiled = options.LineFilterRegex
                                               ? CompileQuery(options.LineFilter, options.CaseSensitive)
                                               : CompiledQuery{};
            if (options.LineFilterRegex && !compiled.Valid) {
                result.LineFilterValid = false;
                result.LineFilterError = compiled.Error;
            } else {
                filtered.reserve(lines.size());
                const std::regex *pattern = options.LineFilterRegex ? &compiled.Pattern : nullptr;
                for (const auto &line: lines) {
                    if (LineMatches(line, options, pattern)) {
                        filtered.push_back(line);
                    }
                }
                visible = &filtered;
            }
        }

        result.Joined.reserve(visible->size() * 80);
        const bool hasQuery = !options.Query.empty();
        std::regex searchPattern;
        if (hasQuery && options.UseRegex) {
            CompiledQuery compiled = CompileQuery(options.Query, options.CaseSensitive);
            if (!compiled.Valid) {
                result.RegexValid = false;
                result.RegexError = std::move(compiled.Error);
            } else {
                searchPattern = std::move(compiled.Pattern);
            }
        }

        const bool collectMatches = hasQuery && (options.UseRegex ? result.RegexValid : true);
        for (const auto &line: *visible) {
            const std::size_t lineStart = result.Joined.size();
            if (collectMatches) {
                if (options.UseRegex) {
                    CollectRegexMatches(line, lineStart, searchPattern, result.Matches);
                } else {
                    CollectSubstringMatches(line, lineStart, options.Query, options.CaseSensitive, result.Matches);
                }
            }
            AppendLine(result.Joined, line);
        }

        return result;
    }
}