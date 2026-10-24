#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>


#include "platform/paths.h"
#include "app/paths.h"
#include "app/constants.h"

using namespace CoreDeck::Platform;
using namespace CoreDeck::App;
using namespace CoreDeck;

TEST_CASE("JoinPaths joins components with the platform separator", "[paths][join]") {
    const std::string joined = Platform::JoinPaths({"a", "b", "c"});
#ifdef _WIN32
    REQUIRE(joined == "a\\b\\c");
#else
    REQUIRE(joined == "a/b/c");
#endif
}

TEST_CASE("JoinPaths skips empty components", "[paths][join]") {
    const std::string joined = Platform::JoinPaths({"root", "", "leaf"});
#ifdef _WIN32
    REQUIRE(joined == "root\\leaf");
#else
    REQUIRE(joined == "root/leaf");
#endif
}

TEST_CASE("JoinPaths returns empty for empty input", "[paths][join]") {
    REQUIRE(Platform::JoinPaths({}).empty());
}

TEST_CASE("JoinPaths with a single component returns it unchanged", "[paths][join]") {
    REQUIRE(Platform::JoinPaths({"solo"}) == "solo");
}

TEST_CASE("NormalizePath collapses redundant segments", "[paths][normalize]") {
#ifdef _WIN32
    REQUIRE(Platform::NormalizePath("a\\b\\..\\c") == "a\\c");
    REQUIRE(Platform::NormalizePath("a\\.\\b") == "a\\b");
#else
    REQUIRE(Platform::NormalizePath("a/b/../c") == "a/c");
    REQUIRE(Platform::NormalizePath("a/./b") == "a/b");
#endif
}

TEST_CASE("NormalizePath leaves an already-clean path alone", "[paths][normalize]") {
#ifdef _WIN32
    REQUIRE(Platform::NormalizePath("a\\b\\c") == "a\\b\\c");
#else
    REQUIRE(Platform::NormalizePath("a/b/c") == "a/b/c");
#endif
}

TEST_CASE("GetCurrentPlatform reports the platform this test runs on", "[paths][platform]") {
#ifdef _WIN32
    REQUIRE(GetCurrentOs() == Os::Windows);
#elif defined(__APPLE__)
    REQUIRE(GetCurrentOs() == Os::MacOS);
#elif defined(__linux__)
    REQUIRE(GetCurrentOs() == Os::Linux);
#endif
}

TEST_CASE("GetPlatformName returns a non-empty, known string", "[paths][platform]") {
    const std::string name = GetOsName();
    REQUIRE_FALSE(name.empty());
    REQUIRE((name == "Windows" || name == "macOS" || name == "Linux" || name == "Unknown"));
}

TEST_CASE("GetExecutableExtension matches platform convention", "[paths][platform]") {
#ifdef _WIN32
    REQUIRE(Platform::GetExecutableExtension() == ".exe");
#else
    REQUIRE(Platform::GetExecutableExtension().empty());
#endif
}

TEST_CASE("GetNullDevice returns the correct per-platform device", "[paths][platform]") {
#ifdef _WIN32
    REQUIRE(Platform::GetNullDevice() == "NUL");
#else
    REQUIRE(Platform::GetNullDevice() == "/dev/null");
#endif
}

namespace {
    void SetEnvVar(const char *name, const char *value) {
#ifdef _WIN32
        _putenv_s(name, value);
#else
        setenv(name, value, 1); // NOLINT(concurrency-mt-unsafe)
#endif
    }

    void UnsetEnvVar(const char *name) {
#ifdef _WIN32
        _putenv_s(name, "");
#else
        unsetenv(name); // NOLINT(concurrency-mt-unsafe)
#endif
    }

    class ScopedEnvVar {
    public:
        ScopedEnvVar(ScopedEnvVar &&) = delete;
        ScopedEnvVar &operator=(ScopedEnvVar &&) = delete;

        ScopedEnvVar(const char *name, const char *value) : m_Name(name) {
            if (const char *previous = std::getenv(name)) { // NOLINT(concurrency-mt-unsafe)
                m_Previous = previous;
                m_HadPrevious = true;
            }
            if (value != nullptr) {
                SetEnvVar(name, value);
            } else {
                UnsetEnvVar(name);
            }
        }

        ~ScopedEnvVar() {
            if (m_HadPrevious) {
                SetEnvVar(m_Name.c_str(), m_Previous.c_str());
            } else {
                UnsetEnvVar(m_Name.c_str());
            }
        }

        ScopedEnvVar(const ScopedEnvVar &) = delete;
        ScopedEnvVar &operator=(const ScopedEnvVar &) = delete;

    private:
        std::string m_Name;
        std::string m_Previous;
        bool m_HadPrevious = false;
    };
}

TEST_CASE("GetAssetsDirectory uses COREDECK_ASSETS_DIR when set", "[paths][assets]") {
    const std::string dir = std::filesystem::temp_directory_path().string();
    const ScopedEnvVar env(ENV_COREDECK_ASSETS_DIR, dir.c_str());
    REQUIRE(App::GetAssetsDirectory() == dir);
}

TEST_CASE("GetAssetsDirectory keeps a set path that is not a directory", "[paths][assets]") {
    const ScopedEnvVar env(ENV_COREDECK_ASSETS_DIR, "/usr/share/coredeck");
    REQUIRE(App::GetAssetsDirectory() == "/usr/share/coredeck");
}

TEST_CASE("GetAssetsDirectory ignores an empty COREDECK_ASSETS_DIR", "[paths][assets]") {
    const ScopedEnvVar env(ENV_COREDECK_ASSETS_DIR, "");
    REQUIRE(App::GetAssetsDirectory() == Platform::JoinPaths({App::GetResourcesDirectory(), "assets"}));
}

TEST_CASE("GetAssetsDirectory falls back to assets beside the resources directory", "[paths][assets]") {
    const ScopedEnvVar env(ENV_COREDECK_ASSETS_DIR, nullptr);
    REQUIRE(App::GetAssetsDirectory() == Platform::JoinPaths({App::GetResourcesDirectory(), "assets"}));
}