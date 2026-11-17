//
// Created by AbdulMuaz Aqeel on 14/11/2026.
//

#ifndef COREDECK_CORE_FRIDA_FRIDA_PRELUDE_H
#define COREDECK_CORE_FRIDA_FRIDA_PRELUDE_H

#include <string>
#include <string_view>

namespace CoreDeck::Core {
    std::string WrapFridaSnippet(std::string_view source);
}

#endif // COREDECK_CORE_FRIDA_FRIDA_PRELUDE_H
