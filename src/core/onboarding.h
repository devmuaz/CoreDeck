//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#ifndef COREDECK_ONBOARDING_H
#define COREDECK_ONBOARDING_H

#include <string>

namespace CoreDeck {
    bool IsFirstRunComplete();

    void MarkFirstRunComplete();

    bool ValidateSdkPath(const std::string &path);

    std::string LoadSdkPathOverride();

    void SaveSdkPathOverride(const std::string &path);

    void ClearSdkPathOverride();
}

#endif //COREDECK_ONBOARDING_H
