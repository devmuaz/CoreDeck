#ifndef COREDECK_EMULATOR_CONSOLE_H
#define COREDECK_EMULATOR_CONSOLE_H

namespace CoreDeck::EmulatorConsole {
    int FindFreePort(int startPort = 5554, int endPort = 5584);

    bool IsAvailable(int port, int timeoutMs = 250);

    bool SendKill(int port, int timeoutMs = 2000);
}

#endif
