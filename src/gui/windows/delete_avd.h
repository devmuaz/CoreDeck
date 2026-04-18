//
// Created by AbdulMuaz Aqeel on 14/04/2026.
//

#ifndef COREDECK_DELETE_AVD_H
#define COREDECK_DELETE_AVD_H

#include "../context.h"

namespace CoreDeck {
    void StartDeleteAvdAsync(Context &context, const std::string &avdName);

    void BuildDeleteAvdWindow(Context &context);
}

#endif //COREDECK_DELETE_AVD_H
