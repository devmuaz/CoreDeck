//
// Created by AbdulMuaz Aqeel on 24/07/2026.
//

#include "jdk.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>

#include "paths.h"
#include "process.h"

namespace CoreDeck {
    namespace {
#if defined(_WIN32)
        constexpr char PATH_SEPARATOR = ';';
#else
        constexpr char PATH_SEPARATOR = ':';
#endif

        std::string FirstLine(const std::string &text) {
            const auto pos = text.find_first_of("\r\n");
            std::string line = (pos == std::string::npos) ? text : text.substr(0, pos);
            while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
                line.pop_back();
            }
            return line;
        }

        int ParseMajorVersion(const std::string &output) {
            const auto quote1 = output.find('"');
            if (quote1 == std::string::npos) {
                return 0;
            }
            const auto quote2 = output.find('"', quote1 + 1);
            if (quote2 == std::string::npos) {
                return 0;
            }

            const std::string version = output.substr(quote1 + 1, quote2 - quote1 - 1);

            std::vector<int> parts;
            std::string current;
            for (const char c: version) {
                if (c == '.') {
                    parts.push_back(static_cast<int>(std::strtol(current.c_str(), nullptr, 10)));
                    current.clear();
                } else if (std::isdigit(static_cast<unsigned char>(c))) {
                    current.push_back(c);
                } else {
                    break;
                }
            }
            if (!current.empty()) {
                parts.push_back(static_cast<int>(std::strtol(current.c_str(), nullptr, 10)));
            }

            if (parts.empty()) {
                return 0;
            }
            if (parts[0] == 1 && parts.size() >= 2) {
                return parts[1];
            }
            return parts[0];
        }

        std::string JavaBinaryIn(const std::string &home) {
            return Paths::JoinPaths({home, "bin", "java" + Paths::GetExecutableExtension()});
        }

        void AddSubdirCandidates(
            std::vector<std::string> &out,
            const std::string &parent,
            const std::string &suffix
        ) {
            std::error_code ec;
            if (!std::filesystem::exists(parent, ec) || !std::filesystem::is_directory(parent, ec)) {
                return;
            }
            for (const auto &entry: std::filesystem::directory_iterator(parent, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_directory(ec)) {
                    continue;
                }
                out.push_back(suffix.empty() ? entry.path().string() : Paths::JoinPaths({entry.path().string(), suffix}));
            }
        }

        std::vector<std::string> CandidateJdkHomes() {
            std::vector<std::string> homes;
            const std::string home = Paths::GetHomeDirectory();

#if defined(_WIN32)
            const char *programFiles = std::getenv("ProgramFiles");
            const char *localAppData = std::getenv("LOCALAPPDATA");
            if (programFiles) {
                const std::string pf(programFiles);
                AddSubdirCandidates(homes, Paths::JoinPaths({pf, "Java"}), "");
                AddSubdirCandidates(homes, Paths::JoinPaths({pf, "Eclipse Adoptium"}), "");
                AddSubdirCandidates(homes, Paths::JoinPaths({pf, "Microsoft"}), "");
                AddSubdirCandidates(homes, Paths::JoinPaths({pf, "Amazon Corretto"}), "");
                AddSubdirCandidates(homes, Paths::JoinPaths({pf, "Zulu"}), "");
                homes.push_back(Paths::JoinPaths({pf, "Android", "Android Studio", "jbr"}));
            }
            if (localAppData) {
                homes.push_back(Paths::JoinPaths({std::string(localAppData), "Programs", "Android Studio", "jbr"}));
            }
#elif defined(__APPLE__)
            AddSubdirCandidates(homes, "/Library/Java/JavaVirtualMachines", "Contents/Home");
            if (!home.empty()) {
                AddSubdirCandidates(homes, Paths::JoinPaths({home, "Library", "Java", "JavaVirtualMachines"}), "Contents/Home");
            }
            homes.emplace_back("/Applications/Android Studio.app/Contents/jbr/Contents/Home");

            if (std::filesystem::exists("/usr/libexec/java_home")) {
                const std::string out = FirstLine(RunCommandArgs("/usr/libexec/java_home", {}));
                if (!out.empty()) {
                    homes.push_back(out);
                }
            }
#else
            AddSubdirCandidates(homes, "/usr/lib/jvm", "");
            AddSubdirCandidates(homes, "/usr/lib64/jvm", "");
            AddSubdirCandidates(homes, "/opt", "");
            homes.emplace_back("/opt/android-studio/jbr");
            homes.emplace_back("/usr/local/android-studio/jbr");
            if (!home.empty()) {
                homes.push_back(Paths::JoinPaths({home, "android-studio", "jbr"}));
            }
#endif
            return homes;
        }

        JdkInfo FindBestDetectedJdk() {
            JdkInfo bestValid;
            JdkInfo bestAny;

            for (const auto &candidate: CandidateJdkHomes()) {
                if (candidate.empty()) {
                    continue;
                }
                JdkInfo info = InspectJdk(candidate);
                if (!info.IsFound) {
                    continue;
                }
                info.Source = JdkSource::Detected;

                if (info.MajorVersion > bestAny.MajorVersion) {
                    bestAny = info;
                }
                if (info.IsValid && info.MajorVersion > bestValid.MajorVersion) {
                    bestValid = info;
                }
            }

            return bestValid.IsFound ? bestValid : bestAny;
        }
    }

    JdkInfo InspectJdk(const std::string &javaHome) {
        JdkInfo info;
        if (javaHome.empty()) {
            return info;
        }

        std::string home = javaHome;
        std::string javaBin = JavaBinaryIn(home);

        if (!std::filesystem::exists(javaBin)) {

            const std::string altHome = Paths::JoinPaths({javaHome, "Contents", "Home"});
            if (const std::string altBin = JavaBinaryIn(altHome); std::filesystem::exists(altBin)) {
                home = altHome;
                javaBin = altBin;
            } else {
                info.JavaHome = javaHome;
                return info;
            }
        }

        info.JavaHome = home;
        info.JavaBin = javaBin;
        info.IsFound = true;


        const std::string output = RunCommandArgs(javaBin, {"-version"});
        info.VersionString = FirstLine(output);
        info.MajorVersion = ParseMajorVersion(output);
        info.IsValid = info.MajorVersion >= JDK_MINIMUM_MAJOR;
        return info;
    }

    JdkInfo DetectJdk() {

        if (const std::string override = Paths::Onboarding::LoadJdkPathOverride(); !override.empty()) {
            JdkInfo info = InspectJdk(override);
            info.JavaHome = info.JavaHome.empty() ? override : info.JavaHome;
            info.Source = JdkSource::Override;
            return info;
        }

        if (const char *javaHomeEnv = std::getenv("JAVA_HOME"); javaHomeEnv && *javaHomeEnv) { // NOLINT(concurrency-mt-unsafe)
            JdkInfo info = InspectJdk(javaHomeEnv);
            if (info.IsFound) {
                info.Source = JdkSource::JavaHomeEnv;
                if (info.IsValid) {
                    return info;
                }

                if (JdkInfo detected = FindBestDetectedJdk(); detected.IsValid) {
                    return detected;
                }
                return info;
            }
        }

        return FindBestDetectedJdk();
    }

    bool ShouldApplyJdk(const JdkInfo &jdk) {
        if (!jdk.IsFound || jdk.JavaHome.empty()) {
            return false;
        }
        switch (jdk.Source) {
            case JdkSource::Override:
            case JdkSource::JavaHomeEnv:
                return true;
            case JdkSource::Detected:
                return jdk.IsValid;
            case JdkSource::None:
            default:
                return false;
        }
    }

    EnvVars JavaToolEnvironment(const JdkInfo &jdk) {
        EnvVars env;
        if (jdk.JavaHome.empty()) {
            return env;
        }

        env.push_back({.Name = "JAVA_HOME", .Value = jdk.JavaHome});

        std::string binDir;
        if (!jdk.JavaBin.empty()) {
            binDir = std::filesystem::path(jdk.JavaBin).parent_path().string();
        } else {
            binDir = Paths::JoinPaths({jdk.JavaHome, "bin"});
        }

        if (!binDir.empty()) {
            std::string newPath = binDir;
            if (const char *existing = std::getenv("PATH"); existing && *existing) { // NOLINT(concurrency-mt-unsafe)
                newPath.push_back(PATH_SEPARATOR);
                newPath += existing;
            }
            env.push_back({.Name = "PATH", .Value = newPath});
        }

        return env;
    }

    void ApplyJdkToSdk(SdkInfo &sdk, const JdkInfo &jdk) {
        if (ShouldApplyJdk(jdk)) {
            sdk.ToolEnv = JavaToolEnvironment(jdk);
        } else {
            sdk.ToolEnv.clear();
        }
    }
}
