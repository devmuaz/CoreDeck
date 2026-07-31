#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/paths.h"
#include "core/sdk.h"
#include "core/sdk_bootstrap.h"

using namespace CoreDeck;

namespace {
    std::filesystem::path MakeScratchDir(const std::string &name) {
        const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("coredeck_bootstrap_" + name);
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        return dir;
    }

    void WriteExecutable(const std::filesystem::path &path) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "#!/bin/sh\n";
        out.close();
        std::filesystem::permissions(
            path,
            std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
            std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
            std::filesystem::perms::others_exec,
            std::filesystem::perm_options::add
        );
    }

    // Mirrors what extracting the real cmdline-tools archive leaves behind.
    void CreateFakeCmdlineTools(const std::filesystem::path &destDir) {
        const std::filesystem::path bin = destDir / "bin";
#if defined(_WIN32)
        WriteExecutable(bin / "avdmanager.bat");
        WriteExecutable(bin / "sdkmanager.bat");
#else
        WriteExecutable(bin / "avdmanager");
        WriteExecutable(bin / "sdkmanager");
#endif
    }

    void CreateFakeEmulator(const std::string &installRoot) {
        WriteExecutable(
            std::filesystem::path(installRoot) / "emulator" / ("emulator" + Paths::GetExecutableExtension())
        );
    }

    JdkInfo ValidJdk() {
        JdkInfo jdk;
        jdk.JavaHome = "/fake/jdk";
        jdk.JavaBin = "/fake/jdk/bin/java";
        jdk.VersionString = "openjdk version \"17.0.11\"";
        jdk.MajorVersion = 17;
        jdk.IsFound = true;
        jdk.IsValid = true;
        jdk.Source = JdkSource::Override;
        return jdk;
    }

    constexpr const char *FAKE_ARCHIVE_BODY = "fake-cmdline-tools-archive";

    // Deps that walk the whole pipeline successfully; individual tests override one step.
    BootstrapDeps MakeFakeDeps(const std::string &installRoot) {
        BootstrapDeps deps = DefaultBootstrapDeps();

        deps.Download = [](
            const std::string & /*url*/,
            const std::string &destPath,
            const std::function<bool(std::uint64_t, std::uint64_t)> &onProgress,
            std::string &error
        ) {
            std::filesystem::create_directories(std::filesystem::path(destPath).parent_path());
            std::ofstream out(destPath, std::ios::binary | std::ios::trunc);
            out << FAKE_ARCHIVE_BODY;
            out.close();

            if (onProgress && !onProgress(64, 128)) {
                error = "cancelled";
                return false;
            }
            if (onProgress && !onProgress(128, 128)) {
                error = "cancelled";
                return false;
            }
            return true;
        };

        deps.FileSha256 = [](const std::string & /*path*/) {
            return GetBundledCmdlineToolsRelease().Sha256;
        };

        deps.Extract = [](
            const std::string & /*zipPath*/,
            const std::string &destDir,
            const std::function<bool(float)> &onProgress,
            std::string &error
        ) {
            CreateFakeCmdlineTools(destDir);
            if (onProgress && !onProgress(1.0F)) {
                error = "cancelled";
                return false;
            }
            return true;
        };

        deps.CheckLicenses = [](const SdkInfo &) { return LicenseStatus::SomeUnaccepted; };
        deps.AcceptLicenses = [](const SdkInfo &) { return true; };

        deps.InstallPackages = [installRoot](
            const SdkInfo & /*sdk*/,
            const std::string &sdkRoot,
            const std::vector<std::string> &packages,
            const std::shared_ptr<BootstrapProgressData> & /*progress*/
        ) {
            REQUIRE(sdkRoot == installRoot);
            REQUIRE_FALSE(packages.empty());
            CreateFakeEmulator(sdkRoot);
            return true;
        };

        return deps;
    }

    BootstrapError ErrorOf(const std::shared_ptr<BootstrapProgressData> &progress) {
        std::lock_guard lock(progress->Mutex);
        return progress->Error;
    }

    BootstrapStage StageOf(const std::shared_ptr<BootstrapProgressData> &progress) {
        std::lock_guard lock(progress->Mutex);
        return progress->Stage;
    }
}

TEST_CASE("GetBundledCmdlineToolsRelease pins an archive for this platform", "[bootstrap][release]") {
    const CmdlineToolsRelease release = GetBundledCmdlineToolsRelease();

    REQUIRE_FALSE(release.Version.empty());
    REQUIRE_FALSE(release.FileName.empty());
    REQUIRE_FALSE(release.DownloadUrl.empty());
    REQUIRE(release.DownloadUrl.starts_with("https://dl.google.com/android/repository/"));
    REQUIRE(release.FileName.ends_with(".zip"));
    REQUIRE(release.Sha256.size() == 64);
    REQUIRE(release.DownloadSize > 0);
}

TEST_CASE("BootstrapErrorMessage covers every error", "[bootstrap][errors]") {
    constexpr BootstrapError ALL[] = {
        BootstrapError::None,
        BootstrapError::InvalidInstallRoot,
        BootstrapError::InsufficientDiskSpace,
        BootstrapError::UnsupportedPlatform,
        BootstrapError::NetworkFailed,
        BootstrapError::ChecksumMismatch,
        BootstrapError::ExtractFailed,
        BootstrapError::JdkRequired,
        BootstrapError::SdkManagerMissing,
        BootstrapError::LicenseCheckFailed,
        BootstrapError::LicenseAcceptFailed,
        BootstrapError::PackageInstallFailed,
        BootstrapError::EmulatorMissingAfterInstall,
        BootstrapError::Cancelled,
        BootstrapError::Unknown,
    };

    for (const BootstrapError error: ALL) {
        REQUIRE(std::string(BootstrapErrorMessage(error)).size() > 0);
    }
}

TEST_CASE("BootstrapStagingDirectory lives under the install root", "[bootstrap][paths]") {
    const std::string staging = BootstrapStagingDirectory("/tmp/sdk");
    REQUIRE(staging.starts_with("/tmp/sdk"));
    REQUIRE(staging != "/tmp/sdk");
}

TEST_CASE("ProbeAndroidSdk reports partial and complete SDK trees", "[bootstrap][probe]") {
    const std::filesystem::path root = MakeScratchDir("probe");

    SECTION("empty directory") {
        const SdkInfo sdk = ProbeAndroidSdk(root.string());
        REQUIRE_FALSE(sdk.IsFound);
        REQUIRE(sdk.SdkManagerPath.empty());
    }

    SECTION("cmdline-tools only") {
        CreateFakeCmdlineTools(root / "cmdline-tools" / "latest");
        const SdkInfo sdk = ProbeAndroidSdk(root.string());
        REQUIRE_FALSE(sdk.IsFound);
        REQUIRE_FALSE(sdk.SdkManagerPath.empty());
        REQUIRE_FALSE(sdk.AvdManagerPath.empty());
    }

    SECTION("cmdline-tools and emulator") {
        CreateFakeCmdlineTools(root / "cmdline-tools" / "latest");
        CreateFakeEmulator(root.string());
        const SdkInfo sdk = ProbeAndroidSdk(root.string());
        REQUIRE(sdk.IsFound);
        REQUIRE_FALSE(sdk.SdkManagerPath.empty());
    }

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk installs a usable SDK on the happy path", "[bootstrap][pipeline]") {
    const std::filesystem::path root = MakeScratchDir("happy");
    const BootstrapPlan plan{.InstallRoot = root.string()};
    const auto progress = std::make_shared<BootstrapProgressData>();

    REQUIRE(BootstrapAndroidSdk(plan, ValidJdk(), progress, MakeFakeDeps(root.string())));

    REQUIRE(ErrorOf(progress) == BootstrapError::None);
    REQUIRE(StageOf(progress) == BootstrapStage::Succeeded);
    {
        std::lock_guard lock(progress->Mutex);
        REQUIRE(progress->Finished);
        REQUIRE(progress->Succeeded);
        REQUIRE(progress->Percent == 1.0F);
    }

    REQUIRE(std::filesystem::exists(root / "cmdline-tools" / "latest" / "bin"));
    REQUIRE(ProbeAndroidSdk(root.string()).IsFound);
    REQUIRE_FALSE(std::filesystem::exists(BootstrapStagingDirectory(root.string())));

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk advances stages in order with a monotonic percent", "[bootstrap][pipeline]") {
    const std::filesystem::path root = MakeScratchDir("stages");
    const BootstrapPlan plan{.InstallRoot = root.string()};
    const auto progress = std::make_shared<BootstrapProgressData>();

    std::vector<BootstrapStage> observed;
    float lastPercent = -1.0F;
    const auto observe = [&observed, &lastPercent, &progress] {
        std::lock_guard lock(progress->Mutex);
        observed.push_back(progress->Stage);
        REQUIRE(progress->Percent >= lastPercent);
        lastPercent = progress->Percent;
    };

    BootstrapDeps deps = MakeFakeDeps(root.string());
    const auto download = deps.Download;
    deps.Download = [&observe, download](auto &&url, auto &&dest, auto &&onProgress, auto &error) {
        observe();
        return download(url, dest, onProgress, error);
    };
    const auto sha = deps.FileSha256;
    deps.FileSha256 = [&observe, sha](const std::string &path) {
        observe();
        return sha(path);
    };
    const auto extract = deps.Extract;
    deps.Extract = [&observe, extract](auto &&zip, auto &&dest, auto &&onProgress, auto &error) {
        observe();
        return extract(zip, dest, onProgress, error);
    };
    const auto check = deps.CheckLicenses;
    deps.CheckLicenses = [&observe, check](const SdkInfo &sdk) {
        observe();
        return check(sdk);
    };
    const auto install = deps.InstallPackages;
    deps.InstallPackages = [&observe, install](auto &&sdk, auto &&root2, auto &&packages, auto &&prog) {
        observe();
        return install(sdk, root2, packages, prog);
    };

    REQUIRE(BootstrapAndroidSdk(plan, ValidJdk(), progress, deps));

    REQUIRE(observed == std::vector{
                BootstrapStage::DownloadingCmdlineTools,
                BootstrapStage::Verifying,
                BootstrapStage::Extracting,
                BootstrapStage::AcceptingLicenses,
                BootstrapStage::InstallingPackages,
            });

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk requires a JDK 17 or newer before downloading", "[bootstrap][jdk]") {
    const std::filesystem::path root = MakeScratchDir("jdk");
    const BootstrapPlan plan{.InstallRoot = root.string()};
    const auto progress = std::make_shared<BootstrapProgressData>();

    bool downloadCalled = false;
    BootstrapDeps deps = MakeFakeDeps(root.string());
    deps.Download = [&downloadCalled](auto &&, auto &&, auto &&, auto &) {
        downloadCalled = true;
        return true;
    };

    SECTION("no JDK at all") {
        REQUIRE_FALSE(BootstrapAndroidSdk(plan, JdkInfo{}, progress, deps));
    }

    SECTION("JDK older than 17") {
        JdkInfo old = ValidJdk();
        old.MajorVersion = 8;
        old.IsValid = false;
        REQUIRE_FALSE(BootstrapAndroidSdk(plan, old, progress, deps));
    }

    REQUIRE(ErrorOf(progress) == BootstrapError::JdkRequired);
    REQUIRE_FALSE(downloadCalled);

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk rejects an empty install root", "[bootstrap][pipeline]") {
    const auto progress = std::make_shared<BootstrapProgressData>();
    const BootstrapPlan plan{.InstallRoot = ""};

    REQUIRE_FALSE(BootstrapAndroidSdk(plan, ValidJdk(), progress, MakeFakeDeps("")));
    REQUIRE(ErrorOf(progress) == BootstrapError::InvalidInstallRoot);
}

TEST_CASE("BootstrapAndroidSdk fails cleanly when the download fails", "[bootstrap][pipeline]") {
    const std::filesystem::path root = MakeScratchDir("network");
    const BootstrapPlan plan{.InstallRoot = root.string()};
    const auto progress = std::make_shared<BootstrapProgressData>();

    BootstrapDeps deps = MakeFakeDeps(root.string());
    deps.Download = [](auto &&, auto &&, auto &&, std::string &error) {
        error = "connection reset";
        return false;
    };

    REQUIRE_FALSE(BootstrapAndroidSdk(plan, ValidJdk(), progress, deps));

    REQUIRE(ErrorOf(progress) == BootstrapError::NetworkFailed);
    REQUIRE(StageOf(progress) == BootstrapStage::Failed);
    REQUIRE_FALSE(std::filesystem::exists(BootstrapStagingDirectory(root.string())));
    REQUIRE_FALSE(std::filesystem::exists(root / "cmdline-tools"));

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk rejects an archive with the wrong checksum", "[bootstrap][pipeline]") {
    const std::filesystem::path root = MakeScratchDir("checksum");
    const BootstrapPlan plan{.InstallRoot = root.string()};
    const auto progress = std::make_shared<BootstrapProgressData>();

    BootstrapDeps deps = MakeFakeDeps(root.string());
    deps.FileSha256 = [](const std::string &) { return std::string(64, 'a'); };

    bool extractCalled = false;
    deps.Extract = [&extractCalled](auto &&, auto &&, auto &&, auto &) {
        extractCalled = true;
        return true;
    };

    REQUIRE_FALSE(BootstrapAndroidSdk(plan, ValidJdk(), progress, deps));

    REQUIRE(ErrorOf(progress) == BootstrapError::ChecksumMismatch);
    REQUIRE_FALSE(extractCalled);
    REQUIRE_FALSE(std::filesystem::exists(root / "cmdline-tools"));
    REQUIRE_FALSE(std::filesystem::exists(BootstrapStagingDirectory(root.string())));

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk reports extraction failures", "[bootstrap][pipeline]") {
    const std::filesystem::path root = MakeScratchDir("extract");
    const BootstrapPlan plan{.InstallRoot = root.string()};
    const auto progress = std::make_shared<BootstrapProgressData>();

    BootstrapDeps deps = MakeFakeDeps(root.string());
    deps.Extract = [](auto &&, auto &&, auto &&, std::string &error) {
        error = "corrupt archive";
        return false;
    };

    REQUIRE_FALSE(BootstrapAndroidSdk(plan, ValidJdk(), progress, deps));
    REQUIRE(ErrorOf(progress) == BootstrapError::ExtractFailed);

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk stops when the SDK Manager is missing after extraction", "[bootstrap][pipeline]") {
    const std::filesystem::path root = MakeScratchDir("nosdkmanager");
    const BootstrapPlan plan{.InstallRoot = root.string()};
    const auto progress = std::make_shared<BootstrapProgressData>();

    BootstrapDeps deps = MakeFakeDeps(root.string());
    deps.Extract = [](auto &&, const std::string &destDir, auto &&, auto &) {
        std::filesystem::create_directories(std::filesystem::path(destDir) / "lib");
        return true;
    };

    REQUIRE_FALSE(BootstrapAndroidSdk(plan, ValidJdk(), progress, deps));
    REQUIRE(ErrorOf(progress) == BootstrapError::SdkManagerMissing);

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk surfaces license failures", "[bootstrap][licenses]") {
    const std::filesystem::path root = MakeScratchDir("licenses");
    const BootstrapPlan plan{.InstallRoot = root.string()};

    SECTION("license state cannot be read") {
        const auto progress = std::make_shared<BootstrapProgressData>();
        BootstrapDeps deps = MakeFakeDeps(root.string());
        deps.CheckLicenses = [](const SdkInfo &) { return LicenseStatus::CheckFailed; };

        REQUIRE_FALSE(BootstrapAndroidSdk(plan, ValidJdk(), progress, deps));
        REQUIRE(ErrorOf(progress) == BootstrapError::LicenseCheckFailed);
    }

    SECTION("licenses are refused") {
        const auto progress = std::make_shared<BootstrapProgressData>();
        BootstrapDeps deps = MakeFakeDeps(root.string());
        deps.AcceptLicenses = [](const SdkInfo &) { return false; };

        REQUIRE_FALSE(BootstrapAndroidSdk(plan, ValidJdk(), progress, deps));
        REQUIRE(ErrorOf(progress) == BootstrapError::LicenseAcceptFailed);
    }

    SECTION("already accepted licenses skip the prompt") {
        const auto progress = std::make_shared<BootstrapProgressData>();
        bool acceptCalled = false;
        BootstrapDeps deps = MakeFakeDeps(root.string());
        deps.CheckLicenses = [](const SdkInfo &) { return LicenseStatus::AllAccepted; };
        deps.AcceptLicenses = [&acceptCalled](const SdkInfo &) {
            acceptCalled = true;
            return true;
        };

        REQUIRE(BootstrapAndroidSdk(plan, ValidJdk(), progress, deps));
        REQUIRE_FALSE(acceptCalled);
    }

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk reports package installation failures", "[bootstrap][pipeline]") {
    const std::filesystem::path root = MakeScratchDir("packages");
    const BootstrapPlan plan{.InstallRoot = root.string()};
    const auto progress = std::make_shared<BootstrapProgressData>();

    BootstrapDeps deps = MakeFakeDeps(root.string());
    deps.InstallPackages = [](auto &&, auto &&, auto &&, auto &&) { return false; };

    REQUIRE_FALSE(BootstrapAndroidSdk(plan, ValidJdk(), progress, deps));
    REQUIRE(ErrorOf(progress) == BootstrapError::PackageInstallFailed);

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk fails when the emulator is missing afterwards", "[bootstrap][pipeline]") {
    const std::filesystem::path root = MakeScratchDir("noemulator");
    const BootstrapPlan plan{.InstallRoot = root.string()};
    const auto progress = std::make_shared<BootstrapProgressData>();

    BootstrapDeps deps = MakeFakeDeps(root.string());
    deps.InstallPackages = [](auto &&, auto &&, auto &&, auto &&) { return true; };

    REQUIRE_FALSE(BootstrapAndroidSdk(plan, ValidJdk(), progress, deps));
    REQUIRE(ErrorOf(progress) == BootstrapError::EmulatorMissingAfterInstall);

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk honours a cancellation during download", "[bootstrap][cancel]") {
    const std::filesystem::path root = MakeScratchDir("cancel");
    const BootstrapPlan plan{.InstallRoot = root.string()};
    const auto progress = std::make_shared<BootstrapProgressData>();

    BootstrapDeps deps = MakeFakeDeps(root.string());
    deps.Download = [&progress](
        auto &&,
        const std::string &destPath,
        const std::function<bool(std::uint64_t, std::uint64_t)> &onProgress,
        std::string &error
    ) {
        std::filesystem::create_directories(std::filesystem::path(destPath).parent_path());
        std::ofstream(destPath, std::ios::binary) << "partial";
        {
            std::lock_guard lock(progress->Mutex);
            progress->CancelRequested = true;
        }
        if (onProgress && !onProgress(16, 128)) {
            error = "cancelled";
            return false;
        }
        return true;
    };

    REQUIRE_FALSE(BootstrapAndroidSdk(plan, ValidJdk(), progress, deps));

    REQUIRE(ErrorOf(progress) == BootstrapError::Cancelled);
    REQUIRE(StageOf(progress) == BootstrapStage::Cancelled);
    REQUIRE_FALSE(std::filesystem::exists(BootstrapStagingDirectory(root.string())));
    REQUIRE_FALSE(std::filesystem::exists(root / "cmdline-tools"));

    std::filesystem::remove_all(root);
}

TEST_CASE("BootstrapAndroidSdk can retry into a clean state after a failure", "[bootstrap][pipeline]") {
    const std::filesystem::path root = MakeScratchDir("retry");
    const BootstrapPlan plan{.InstallRoot = root.string()};

    const auto failed = std::make_shared<BootstrapProgressData>();
    BootstrapDeps broken = MakeFakeDeps(root.string());
    broken.Download = [](auto &&, auto &&, auto &&, std::string &error) {
        error = "offline";
        return false;
    };
    REQUIRE_FALSE(BootstrapAndroidSdk(plan, ValidJdk(), failed, broken));

    const auto retried = std::make_shared<BootstrapProgressData>();
    REQUIRE(BootstrapAndroidSdk(plan, ValidJdk(), retried, MakeFakeDeps(root.string())));
    REQUIRE(ProbeAndroidSdk(root.string()).IsFound);

    std::filesystem::remove_all(root);
}
