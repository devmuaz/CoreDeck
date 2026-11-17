//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#include "core/frida/frida_prelude.h"

namespace CoreDeck::Core {
    std::string WrapFridaSnippet(const std::string_view source) {
        std::string wrapped;
        wrapped.reserve(source.size() + 96);
        wrapped += "import Java from 'frida-java-bridge';\n";
        wrapped += "globalThis.Java = Java;\n\n";
        wrapped.append(source);
        if (wrapped.empty() || wrapped.back() != '\n') {
            wrapped.push_back('\n');
        }
        return wrapped;
    }
}
