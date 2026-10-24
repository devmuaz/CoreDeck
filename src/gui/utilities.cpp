//
// Created by AbdulMuaz Aqeel on 30/09/2026.
//

#include "utilities.h"

#include <cstring>
#include <filesystem>

#include "platform/i18n.h"
#include "core/jdk.h"
#include "platform/utilities.h"

namespace CoreDeck::Gui {
    namespace {
        bool g_SpinnerFrameRequested = false;
    }

    void RequestSpinnerFrame() {
        g_SpinnerFrameRequested = true;
    }

    bool ConsumeSpinnerFrameRequest() {
        const bool requested = g_SpinnerFrameRequested;
        g_SpinnerFrameRequested = false;
        return requested;
    }

    void CopyToBuffer(char *buffer, const std::size_t size, const std::string &value) {
        if (buffer == nullptr || size == 0) {
            return;
        }
        std::strncpy(buffer, value.c_str(), size - 1);
        buffer[size - 1] = '\0';
    }

    bool SameJdkHome(const std::string &left, const std::string &right) {
        if (left == right) {
            return true;
        }
        std::error_code error;
        const std::filesystem::path canonicalLeft = std::filesystem::weakly_canonical(left, error);
        const std::filesystem::path canonicalRight = std::filesystem::weakly_canonical(right, error);
        return !canonicalLeft.empty() && canonicalLeft == canonicalRight;
    }

    std::string JavaRuntimeLabel(const Core::JdkInfo &jdk) {
        const std::string lower = Platform::LowerCopy(jdk.VersionString);
        const char *vendor = lower.find("openjdk") != std::string::npos ? Platform::Tr("OpenJDK") : Platform::Tr("Java");
        if (jdk.MajorVersion > 0) {
            return Platform::TrFormat("{0} {1}", vendor, std::to_string(jdk.MajorVersion));
        }
        if (!jdk.VersionString.empty()) {
            return jdk.VersionString;
        }
        return vendor;
    }
}
