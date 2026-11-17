#include <iostream>
#include <string>

#include "core/frida/frida_host.h"

int main() {
    CoreDeck::Core::FridaHost host;
    std::string error;
    if (!host.Probe(error)) {
        std::cerr << error << '\n';
        return 1;
    }
    return 0;
}
