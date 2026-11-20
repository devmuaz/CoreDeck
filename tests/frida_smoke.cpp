#include <cstdlib>
#include <iostream>
#include <string>

#include "core/frida/frida_host.h"

int main() {
    CoreDeck::Core::FridaHost host;
    std::string error;
    const bool ok = host.Probe(error);
    if (!ok) {
        std::cerr << error << '\n';
    }
    std::_Exit(ok ? 0 : 1);
}
