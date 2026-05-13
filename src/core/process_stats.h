//
// Created by AbdulMuaz Aqeel on 12/05/2026.
//

#ifndef COREDECK_PROCESS_STATS_H
#define COREDECK_PROCESS_STATS_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "process.h"

namespace CoreDeck {
    struct ProcessSample {
        double TimestampSec = 0.0;
        float CpuPercent = 0.0f;
        std::uint64_t RssBytes = 0;
        std::uint64_t DiskReadBytesPerSec = 0;
        std::uint64_t DiskWriteBytesPerSec = 0;
        bool Valid = false;
    };

    inline constexpr std::size_t PROCESS_STATS_HISTORY = 60;

    class ProcessStatsSampler {
    public:
        ProcessStatsSampler() = default;
        ~ProcessStatsSampler();

        ProcessStatsSampler(const ProcessStatsSampler &) = delete;
        ProcessStatsSampler &operator=(const ProcessStatsSampler &) = delete;

        void Start();

        void Stop();

        void Track(ProcessId pid);

        void Untrack(ProcessId pid);

        ProcessSample Latest(ProcessId pid) const;

        void CopyCpuHistory(ProcessId pid, std::vector<float> &out) const;

        void CopyRssHistoryMb(ProcessId pid, std::vector<float> &out) const;

        std::chrono::seconds Uptime(ProcessId pid) const;

    private:
        struct Entry {
            std::vector<float> CpuHistory;
            std::vector<float> RssHistoryMb;
            std::size_t HistoryWrite = 0;
            std::size_t HistoryFilled = 0;
            ProcessSample Latest;
            std::chrono::steady_clock::time_point TrackedAt{};
            std::uint64_t PrevCpuTimeNs = 0;
            std::chrono::steady_clock::time_point PrevSampleTime{};
            bool HasPrevCpu = false;
            std::uint64_t PrevDiskReadBytes = 0;
            std::uint64_t PrevDiskWriteBytes = 0;
            bool HasPrevDisk = false;
        };

        void m_WorkerLoop();

        void m_SampleOne(ProcessId pid, Entry &entry, std::chrono::steady_clock::time_point now);

        mutable std::mutex m_Mutex;
        std::unordered_map<ProcessId, Entry> m_Entries;
        std::thread m_Worker;
        std::atomic<bool> m_Running{false};
    };
}

#endif // COREDECK_PROCESS_STATS_H
