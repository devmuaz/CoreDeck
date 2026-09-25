//
// Created by AbdulMuaz Aqeel on 22/09/2026.
//

#include <cstdlib>

#include "sdk_manager.h"
#include "process.h"
#include "utilities.h"

namespace CoreDeck {
    namespace {
        // Java's PrintStream buffers 8192 characters and auto-flushes on newline.
        // A wider COLUMNS makes each Android CLI progress redraw spill that buffer.
        constexpr const char *SDK_MANAGER_PROGRESS_COLUMNS = "16384";

        std::string TrimProgressText(const std::string &text) {
            const auto start = text.find_first_not_of(" \t");
            if (start == std::string::npos) {
                return {};
            }
            const auto end = text.find_last_not_of(" \t");
            return text.substr(start, end - start + 1);
        }

        std::string FractionAfterPercent(const std::string &line, const size_t pctPos) {
            const auto open = line.find('(', pctPos);
            if (open == std::string::npos) {
                return {};
            }
            const auto close = line.find(')', open);
            if (close == std::string::npos || close <= open + 1) {
                return {};
            }

            std::string inner = TrimProgressText(line.substr(open + 1, close - open - 1));
            const auto slash = inner.find('/');
            if (slash == std::string::npos) {
                return inner;
            }
            return TrimProgressText(inner.substr(0, slash)) + " / " + TrimProgressText(inner.substr(slash + 1));
        }

        std::string StatusForProgress(const std::string &line, const size_t pctPos) {
            const std::string fraction = FractionAfterPercent(line, pctPos);
            const bool unzipping = line.find("Unzipping") != std::string::npos;
            if (!fraction.empty()) {
                return (unzipping ? "Unzipping " : "Downloading ") + fraction;
            }

            std::string after;
            if (pctPos + 1 < line.size()) {
                after = TrimProgressText(line.substr(pctPos + 1));
            }
            if (!after.empty() && !after.starts_with("ETA:")) {
                return after;
            }
            if (unzipping) {
                return "Unzipping...";
            }
            return "Downloading...";
        }

        bool IsProgressPercent(const std::string &line, const size_t pctPos) {
            if (pctPos == 0 || line[pctPos] != '%') {
                return false;
            }
            const char after = pctPos + 1 < line.size() ? line[pctPos + 1] : '\0';
            if (after != '\0' && after != ' ' && after != '\t' && after != '(') {
                return false;
            }

            size_t start = pctPos;
            while (start > 0 && line[start - 1] >= '0' && line[start - 1] <= '9') {
                --start;
            }
            return start != pctPos && pctPos - start <= 3;
        }
    }

    SdkManagerProgressLine ParseSdkManagerProgressLine(const std::string &line) {
        SdkManagerProgressLine result;
        size_t pctPos = line.rfind('%');
        while (pctPos != std::string::npos) {
            if (IsProgressPercent(line, pctPos)) {
                size_t start = pctPos;
                while (start > 0 && line[start - 1] >= '0' && line[start - 1] <= '9') {
                    --start;
                }
                const int pct = static_cast<int>(std::strtol(line.c_str() + start, nullptr, 10));
                if (pct >= 0 && pct <= 100) {
                    result.HasPercent = true;
                    result.Percent = pct;
                    result.Status = StatusForProgress(line, pctPos);
                    return result;
                }
            }
            if (pctPos == 0) {
                break;
            }
            pctPos = line.rfind('%', pctPos - 1);
        }
        return result;
    }

    EnvVars SdkManagerInstallEnvironment(EnvVars env) {
        for (auto &var: env) {
            if (var.Name == "COLUMNS") {
                var.Value = SDK_MANAGER_PROGRESS_COLUMNS;
                return env;
            }
        }
        env.insert(env.begin(), {.Name = "COLUMNS", .Value = SDK_MANAGER_PROGRESS_COLUMNS});
        return env;
    }

    std::optional<std::string> RunSdkManager(
        const SdkInfo &sdk,
        const std::vector<std::string> &args,
        const std::string &stdinData
    ) {
        if (sdk.SdkManagerPath.empty()) {
            return std::nullopt;
        }

        return RunCommandArgs(sdk.SdkManagerPath, args, stdinData, sdk.ToolEnv);
    }

    bool RunSdkManagerInstall(
        const SdkInfo &sdk,
        const std::vector<std::string> &args,
        const std::string &stdinInput,
        const SdkManagerProgressSink &onLine
    ) {
        if (sdk.SdkManagerPath.empty()) {
            return false;
        }

        StreamCommandArgs(
            sdk.SdkManagerPath,
            args,
            stdinInput,
            [&onLine](const std::string &line) {
                if (onLine) {
                    onLine(ParseSdkManagerProgressLine(line), line);
                }
            },
            SdkManagerInstallEnvironment(sdk.ToolEnv)
        );
        return true;
    }

    LicenseStatus InterpretSdkLicenseOutput(const std::string &output) {
        if (output.find("All SDK package licenses accepted") != std::string::npos) {
            return LicenseStatus::AllAccepted;
        }
        if (output.find("The --licenses option is no longer needed") != std::string::npos) {
            return LicenseStatus::AllAccepted;
        }
        if (output.find("licenses not accepted") != std::string::npos) {
            return LicenseStatus::SomeUnaccepted;
        }
        return LicenseStatus::CheckFailed;
    }

    LicenseStatus CheckSdkLicenses(const SdkInfo &sdk) {
        const auto output = RunSdkManager(sdk, {"--licenses"}, "N\n");
        if (!output) {
            return LicenseStatus::CheckFailed;
        }
        return InterpretSdkLicenseOutput(*output);
    }

    bool AcceptSdkLicenses(const SdkInfo &sdk) {
        std::string yes;
        yes.reserve(static_cast<size_t>(64 * 2));
        for (int i = 0; i < 64; ++i) {
            yes += "y\n";
        }

        const auto output = RunSdkManager(sdk, {"--licenses"}, yes);
        return output && InterpretSdkLicenseOutput(*output) == LicenseStatus::AllAccepted;
    }
}
