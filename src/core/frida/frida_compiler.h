//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#ifndef COREDECK_CORE_FRIDA_FRIDA_COMPILER_H
#define COREDECK_CORE_FRIDA_FRIDA_COMPILER_H

#include <string>

namespace CoreDeck::Core {
    struct FridaCompileResult {
        bool Ok = false;
        std::string Message;
        std::string Bundle;
    };

    FridaCompileResult CompileFridaBundle(
        void **compiler,
        void *manager,
        const std::string &projectRoot,
        const std::string &entry
    );
}

#endif // COREDECK_CORE_FRIDA_FRIDA_COMPILER_H
