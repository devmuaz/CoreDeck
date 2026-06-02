//
// Created by AbdulMuaz Aqeel on 12/05/2026.
//

#ifdef __APPLE__
#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/resource.h>
#include <unistd.h>
#include <algorithm>

#elif defined(__linux__)
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>

#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

#include "process_stats.h"

namespace CoreDeck {

    constexpr auto TICK_INTERVAL = std::chrono::milliseconds(1000);

    namespace {
#if defined(__APPLE__)
        bool ReadProcessSnapshot(ProcessId pid, std::uint64_t &cpuTimeNs, std::uint64_t &rssBytes, std::uint64_t &diskReadBytes, std::uint64_t &diskWriteBytes) {
            proc_taskinfo info{};
            const int rc = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &info, sizeof(info));
            if (rc < static_cast<int>(sizeof(info))) {
                return false;
            }

            cpuTimeNs = info.pti_total_user + info.pti_total_system;
            rssBytes = info.pti_resident_size;

            rusage_info_current ru{};
            if (proc_pid_rusage(pid, RUSAGE_INFO_CURRENT, reinterpret_cast<rusage_info_t *>(&ru)) == 0) {
                diskReadBytes = ru.ri_diskio_bytesread;
                diskWriteBytes = ru.ri_diskio_byteswritten;
            } else {
                diskReadBytes = 0;
                diskWriteBytes = 0;
            }
            return true;
        }
#elif defined(__linux__)
        bool ReadProcessSnapshot(ProcessId pid, std::uint64_t &cpuTimeNs, std::uint64_t &rssBytes, std::uint64_t &diskReadBytes, std::uint64_t &diskWriteBytes) {
            char path[64];
            std::snprintf(path, sizeof(path), "/proc/%d/stat", static_cast<int>(pid));
            std::ifstream stat(path);
            if (!stat.is_open()) return false;
            std::string contents((std::istreambuf_iterator<char>(stat)), std::istreambuf_iterator<char>());

            const auto rparen = contents.rfind(')');
            if (rparen == std::string::npos) return false;
            std::istringstream rest(contents.substr(rparen + 1));

            char state = 0;
            unsigned long long utime = 0, stime = 0;
            rest >> state;
            for (int i = 0; i < 10; ++i) {
                unsigned long long discard = 0;
                rest >> discard;
            }
            rest >> utime >> stime;
            if (!rest) return false;

            const long ticks = sysconf(_SC_CLK_TCK);
            if (ticks <= 0) return false;
            const double nsPerTick = 1e9 / static_cast<double>(ticks);
            cpuTimeNs = static_cast<std::uint64_t>(
                (static_cast<double>(utime) + static_cast<double>(stime)) * nsPerTick
            );

            std::snprintf(path, sizeof(path), "/proc/%d/statm", static_cast<int>(pid));
            if (std::ifstream statm(path); statm.is_open()) {
                unsigned long long size = 0, rssPages = 0;
                statm >> size >> rssPages;
                rssBytes = rssPages * static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
            } else {
                rssBytes = 0;
            }

            diskReadBytes = 0;
            diskWriteBytes = 0;
            std::snprintf(path, sizeof(path), "/proc/%d/io", static_cast<int>(pid));
            if (std::ifstream io(path); io.is_open()) {
                std::string key;
                unsigned long long value = 0;
                while (io >> key >> value) {
                    if (key == "read_bytes:") diskReadBytes = value;
                    else if (key == "write_bytes:") diskWriteBytes = value;
                }
            }
            return true;
        }
#elif defined(_WIN32)
        std::uint64_t FileTimeToUint(const FILETIME &ft) {
            ULARGE_INTEGER u;
            u.LowPart = ft.dwLowDateTime;
            u.HighPart = ft.dwHighDateTime;
            return u.QuadPart;
        }

        bool ReadOneProcessSnapshot(ProcessId pid, std::uint64_t &cpuTimeNs, std::uint64_t &rssBytes, std::uint64_t &diskReadBytes, std::uint64_t &diskWriteBytes) {
            const HANDLE h = OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                FALSE,
                pid
            );
            if (!h) return false;

            FILETIME creation{}, exitTime{}, kernel{}, user{};
            if (!GetProcessTimes(h, &creation, &exitTime, &kernel, &user)) {
                CloseHandle(h);
                return false;
            }
            cpuTimeNs = (FileTimeToUint(kernel) + FileTimeToUint(user)) * 100ULL;

            PROCESS_MEMORY_COUNTERS_EX mem{};
            if (GetProcessMemoryInfo(h, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&mem), sizeof(mem))) {
                rssBytes = mem.WorkingSetSize;
            } else {
                rssBytes = 0;
            }

            IO_COUNTERS io{};
            if (GetProcessIoCounters(h, &io)) {
                diskReadBytes = io.ReadTransferCount;
                diskWriteBytes = io.WriteTransferCount;
            } else {
                diskReadBytes = 0;
                diskWriteBytes = 0;
            }

            CloseHandle(h);
            return true;
        }

        bool ReadProcessSnapshot(ProcessId pid, std::uint64_t &cpuTimeNs, std::uint64_t &rssBytes, std::uint64_t &diskReadBytes, std::uint64_t &diskWriteBytes) {
            std::vector<ProcessId> pids;
            CollectProcessTreePids(pid, pids);

            cpuTimeNs = 0;
            rssBytes = 0;
            diskReadBytes = 0;
            diskWriteBytes = 0;

            bool anyOk = false;
            for (const ProcessId treePid: pids) {
                std::uint64_t cpu = 0;
                std::uint64_t rss = 0;
                std::uint64_t diskRead = 0;
                std::uint64_t diskWrite = 0;
                if (!ReadOneProcessSnapshot(treePid, cpu, rss, diskRead, diskWrite)) {
                    continue;
                }
                cpuTimeNs += cpu;
                rssBytes += rss;
                diskReadBytes += diskRead;
                diskWriteBytes += diskWrite;
                anyOk = true;
            }
            return anyOk;
        }
#else
        static bool ReadProcessSnapshot(ProcessId, std::uint64_t &, std::uint64_t &, std::uint64_t &, std::uint64_t &) {
            return false;
        }
#endif

        void CopyRingOldestFirst(const std::vector<float> &ring, std::size_t writeIdx, std::size_t filled, std::vector<float> &out) {
            const std::size_t size = ring.size();
            if (size == 0) {
                return;
            }
            const bool full = filled >= size;
            const std::size_t start = full ? writeIdx : 0;
            const std::size_t count = full ? size : filled;
            for (std::size_t i = 0; i < count; ++i) {
                out[i] = ring[(start + i) % size];
            }
        }
    }


    ProcessStatsSampler::~ProcessStatsSampler() {
        Stop();
    }

    void ProcessStatsSampler::Start() {
        if (m_Running.exchange(true)) {
            return;
        }
        m_Worker = std::thread(&ProcessStatsSampler::m_WorkerLoop, this);
    }

    void ProcessStatsSampler::Stop() {
        if (!m_Running.exchange(false)) {
            return;
        }
        if (m_Worker.joinable()) {
            m_Worker.join();
        }
    }

    void ProcessStatsSampler::Track(ProcessId pid) {
        std::lock_guard lock(m_Mutex);
        auto [it, inserted] = m_Entries.try_emplace(pid);
        if (inserted) {
            it->second.CpuHistory.assign(PROCESS_STATS_HISTORY, 0.0F);
            it->second.RssHistoryMb.assign(PROCESS_STATS_HISTORY, 0.0F);
            it->second.TrackedAt = std::chrono::steady_clock::now();
        }
    }

    void ProcessStatsSampler::Untrack(ProcessId pid) {
        std::lock_guard lock(m_Mutex);
        m_Entries.erase(pid);
    }

    ProcessSample ProcessStatsSampler::Latest(ProcessId pid) const {
        std::lock_guard lock(m_Mutex);
        if (const auto it = m_Entries.find(pid); it != m_Entries.end()) {
            return it->second.Latest;
        }
        return {};
    }


    void ProcessStatsSampler::CopyCpuHistory(ProcessId pid, std::vector<float> &out) const {
        out.assign(PROCESS_STATS_HISTORY, 0.0F);
        std::lock_guard lock(m_Mutex);
        const auto it = m_Entries.find(pid);
        if (it == m_Entries.end()) {
            return;
        }
        CopyRingOldestFirst(it->second.CpuHistory, it->second.HistoryWrite, it->second.HistoryFilled, out);
    }

    void ProcessStatsSampler::CopyRssHistoryMb(ProcessId pid, std::vector<float> &out) const {
        out.assign(PROCESS_STATS_HISTORY, 0.0F);
        std::lock_guard lock(m_Mutex);
        const auto it = m_Entries.find(pid);
        if (it == m_Entries.end()) {
            return;
        }
        CopyRingOldestFirst(it->second.RssHistoryMb, it->second.HistoryWrite, it->second.HistoryFilled, out);
    }

    std::chrono::seconds ProcessStatsSampler::Uptime(ProcessId pid) const {
        std::lock_guard lock(m_Mutex);
        const auto it = m_Entries.find(pid);
        if (it == m_Entries.end()) {
            return std::chrono::seconds(0);
        }
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - it->second.TrackedAt
        );
    }

    void ProcessStatsSampler::m_SampleOne(ProcessId pid, Entry &entry, std::chrono::steady_clock::time_point now) {
        std::uint64_t cpuTimeNs = 0;
        std::uint64_t rssBytes = 0;
        std::uint64_t diskRead = 0;
        std::uint64_t diskWrite = 0;

        if (!ReadProcessSnapshot(pid, cpuTimeNs, rssBytes, diskRead, diskWrite)) {
            entry.Latest.Valid = false;
            return;
        }

        double elapsedSec = 0.0;
        if (entry.HasPrevCpu) {
            elapsedSec = std::chrono::duration<double>(now - entry.PrevSampleTime).count();
        }

        float cpuPercent = 0.0F;
        if (entry.HasPrevCpu && elapsedSec > 0.0 &&
            cpuTimeNs >= entry.PrevCpuTimeNs) {
            const auto deltaNs = cpuTimeNs - entry.PrevCpuTimeNs;
            const double elapsedNs = elapsedSec * 1e9;
            cpuPercent = static_cast<float>((static_cast<double>(deltaNs) / elapsedNs) * 100.0);
            cpuPercent = std::max(cpuPercent, 0.0F);
        }

        std::uint64_t readRate = 0;
        std::uint64_t writeRate = 0;
        if (entry.HasPrevDisk && elapsedSec > 0.0) {
            if (diskRead >= entry.PrevDiskReadBytes) {
                readRate = static_cast<std::uint64_t>(
                    static_cast<double>(diskRead - entry.PrevDiskReadBytes) /
                    elapsedSec
                );
            }
            if (diskWrite >= entry.PrevDiskWriteBytes) {
                writeRate = static_cast<std::uint64_t>(
                    static_cast<double>(diskWrite - entry.PrevDiskWriteBytes) /
                    elapsedSec
                );
            }
        }

        entry.PrevCpuTimeNs = cpuTimeNs;
        entry.PrevSampleTime = now;
        entry.HasPrevCpu = true;
        entry.PrevDiskReadBytes = diskRead;
        entry.PrevDiskWriteBytes = diskWrite;
        entry.HasPrevDisk = true;

        ProcessSample sample;
        sample.TimestampSec = std::chrono::duration<double>(now.time_since_epoch()).count();
        sample.CpuPercent = cpuPercent;
        sample.RssBytes = rssBytes;
        sample.DiskReadBytesPerSec = readRate;
        sample.DiskWriteBytesPerSec = writeRate;
        sample.Valid = true;
        entry.Latest = sample;

        if (!entry.CpuHistory.empty()) {
            const float rssMb = static_cast<float>(rssBytes) / (1024.0F * 1024.0F);
            entry.CpuHistory[entry.HistoryWrite] = cpuPercent;
            if (entry.RssHistoryMb.size() == entry.CpuHistory.size()) {
                entry.RssHistoryMb[entry.HistoryWrite] = rssMb;
            }
            entry.HistoryWrite =
                (entry.HistoryWrite + 1) % entry.CpuHistory.size();
            if (entry.HistoryFilled < entry.CpuHistory.size()) {
                ++entry.HistoryFilled;
            }
        }
    }

    void ProcessStatsSampler::m_WorkerLoop() {
        constexpr auto SLICE = std::chrono::milliseconds(50);
        auto nextTick = std::chrono::steady_clock::now();
        while (m_Running.load()) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= nextTick) {
                std::vector<ProcessId> pids;
                {
                    std::lock_guard lock(m_Mutex);
                    pids.reserve(m_Entries.size());
                    for (const auto &kv: m_Entries) {
                        pids.push_back(kv.first);
                    }
                }

                for (const ProcessId pid: pids) {
                    std::lock_guard lock(m_Mutex);
                    auto it = m_Entries.find(pid);
                    if (it == m_Entries.end()) {
                        continue;
                    }
                    m_SampleOne(pid, it->second, now);
                }
                nextTick = now + TICK_INTERVAL;
            }
            std::this_thread::sleep_for(SLICE);
        }
    }
}
