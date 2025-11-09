#include "cli_service.h"

CliService::CliService(Stream &input, CommandHandler handler)
    : m_input(input), m_handler(std::move(handler)), m_buffer() {
}

void CliService::poll() {
    while (m_input.available() > 0) {
        char c = static_cast<char>(m_input.read());
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (m_buffer.length() > 0) {
                m_queue.push(m_buffer);
                m_buffer.clear();
            }
        } else {
            m_buffer += c;
        }
    }
    processQueue();
}

void CliService::enqueueCommand(const String &command) {
    if (command.length() == 0) {
        return;
    }
    m_queue.push(command);
    processQueue();
}

bool CliService::hasPending() const {
    return !m_queue.empty() || m_buffer.length() > 0;
}

void CliService::processQueue() {
    while (!m_queue.empty()) {
        if (m_handler) {
            m_handler(m_queue.front());
        }
        m_queue.pop();
    }
}
