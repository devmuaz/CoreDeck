//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#ifndef EMU_LAUNCHER_LOG_BUFFER_H
#define EMU_LAUNCHER_LOG_BUFFER_H

#include <string>
#include <deque>
#include <mutex>

namespace Emu {
    class LogBuffer {
    public:
        explicit LogBuffer(std::size_t maxLines = 1000);

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

#endif //EMU_LAUNCHER_LOG_BUFFER_H
