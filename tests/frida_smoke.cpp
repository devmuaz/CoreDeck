#include <cstdlib>
#include <iostream>

#include <frida-core.h>

int main() {
    frida_init();
    const char *version = frida_version_string();
    const bool ok = version != nullptr && version[0] != '\0';
    if (!ok) {
        std::cerr << "Frida did not report a version.\n";
    }
    frida_deinit();
    std::_Exit(ok ? 0 : 1);
}
