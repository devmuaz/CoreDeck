//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#ifndef COREDECK_LOG_BUFFER_H
#define COREDECK_LOG_BUFFER_H

#include <string>
#include <deque>
#include <cstdint>
#include <mutex>
#include <vector>

namespace CoreDeck::Platform {
    class LogBuffer {
    public:
        LogBuffer(const LogBuffer &) = delete;
        LogBuffer(LogBuffer &&) = delete;
        LogBuffer &operator=(const LogBuffer &) = delete;
        LogBuffer &operator=(LogBuffer &&) = delete;

        explicit LogBuffer(std::size_t maxLines = 1000);

        ~LogBuffer() = default;

        void Push(const std::string &line);

        std::vector<std::string> GetLines();

        void Clear();

        bool HasNewContent();

        void ResetNewContentFlag();

        std::size_t Size();

        std::uint64_t Revision();

        std::uint64_t DroppedLines();

    private:
        std::deque<std::string> m_Lines;
        std::size_t m_MaxLines;
        std::mutex m_Mutex;
        bool m_HasNew = false;
        std::uint64_t m_Revision = 0;
        std::uint64_t m_DroppedLines = 0;
    };
}

#endif // COREDECK_LOG_BUFFER_H
