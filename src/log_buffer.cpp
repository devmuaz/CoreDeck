//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#include "log_buffer.h"

namespace Emu {
    LogBuffer::LogBuffer(const std::size_t maxLines) : m_MaxLines(maxLines) {
    }

    void LogBuffer::Push(const std::string &line) {
        std::lock_guard lock(m_Mutex);
        m_Lines.push_back(line);
        if (m_Lines.size() > m_MaxLines) {
            m_Lines.pop_back();
        }
        m_HasNew = true;
    }

    std::vector<std::string> LogBuffer::GetLines() {
        std::lock_guard lock(m_Mutex);
        return {m_Lines.begin(), m_Lines.end()};
    }

    void LogBuffer::Clear() {
        std::lock_guard lock(m_Mutex);
        m_Lines.clear();
        m_HasNew = false;
    }

    bool LogBuffer::HasNewContent() {
        std::lock_guard lock(m_Mutex);
        return m_HasNew;
    }

    void LogBuffer::ResetNewContentFlag() {
        std::lock_guard lock(m_Mutex);
        m_HasNew = false;
    }
}
