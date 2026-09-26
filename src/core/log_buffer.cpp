//
// Created by AbdulMuaz Aqeel on 05/04/2026.
//

#include "log_buffer.h"

namespace CoreDeck {
    LogBuffer::LogBuffer(const std::size_t maxLines) : m_MaxLines(maxLines) {
    }

    void LogBuffer::Push(const std::string &line) {
        std::scoped_lock lock(m_Mutex);
        m_Lines.push_back(line);
        if (m_Lines.size() > m_MaxLines) {
            m_Lines.pop_front();
        }
        m_HasNew = true;
    }

    std::vector<std::string> LogBuffer::GetLines() {
        std::scoped_lock lock(m_Mutex);
        return {m_Lines.begin(), m_Lines.end()};
    }

    void LogBuffer::Clear() {
        std::scoped_lock lock(m_Mutex);
        m_Lines.clear();
        m_HasNew = false;
    }

    bool LogBuffer::HasNewContent() {
        std::scoped_lock lock(m_Mutex);
        return m_HasNew;
    }

    void LogBuffer::ResetNewContentFlag() {
        std::scoped_lock lock(m_Mutex);
        m_HasNew = false;
    }
}
