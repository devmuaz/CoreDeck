//
// Created by AbdulMuaz Aqeel on 15/04/2026.
//

#ifndef COREDECK_FILE_DIALOG_H
#define COREDECK_FILE_DIALOG_H

#include <optional>
#include <string>

namespace CoreDeck::FileDialog {
    std::optional<std::string> PickFolder(const std::string &title, const std::string &defaultPath = "");
}

#endif //COREDECK_FILE_DIALOG_H
