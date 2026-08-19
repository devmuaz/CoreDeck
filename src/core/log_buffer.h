//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#ifndef COREDECK_LOG_BUFFER_H
#define COREDECK_LOG_BUFFER_H

#include <string>
#include <deque>
#include <mutex>
#include <vector>

namespace CoreDeck {
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

    private:
        std::deque<std::string> m_Lines;
        std::size_t m_MaxLines;
        std::mutex m_Mutex;
        bool m_HasNew = false;
    };
}

#endif // COREDECK_LOG_BUFFER_H
