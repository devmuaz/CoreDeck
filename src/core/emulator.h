//
// Created by AbdulMuaz Aqeel on 03/04/2026.
//

#ifndef EMU_LAUNCHER_EMULATOR_H
#define EMU_LAUNCHER_EMULATOR_H

#include <string>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <memory>

#include "log_buffer.h"
#include "sdk.h"
#include "process.h"

namespace CoreDeck {
    struct EmulatorInstance {
        std::string AvdName;
        ProcessId Pid;
        int ConsolePort = 0;
        bool IsRunning = false;
        bool Stopping = false;
        std::shared_ptr<LogBuffer> Log;
        std::thread ReaderThread;
        std::thread StopThread;
        std::shared_ptr<std::atomic<bool> > StopRequested;
    };

    class EmulatorManager {
    public:
        explicit EmulatorManager(SdkInfo sdk);

        ~EmulatorManager();

        bool Launch(const std::string &avdName, const std::vector<std::string> &args);

        bool Stop(const std::string &avdName);

        bool IsStopping(const std::string &avdName);

        bool IsRunning(const std::string &avdName);

        std::shared_ptr<LogBuffer> GetLog(const std::string &avdName);

        void Update();

        void SetSdk(SdkInfo sdk);

    private:
        SdkInfo m_Sdk;
        std::mutex m_Mutex;
        std::unordered_map<std::string, EmulatorInstance> m_Instances;
    };
}

#endif //EMU_LAUNCHER_EMULATOR_H
