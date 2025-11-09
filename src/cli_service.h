#ifndef CLI_SERVICE_H
#define CLI_SERVICE_H

#include <Arduino.h>
#include <functional>
#include <queue>
#include <string>

class CliService {
public:
    using CommandHandler = std::function<void(const String&)>;

    CliService(Stream &input, CommandHandler handler);

    void poll();
    void enqueueCommand(const String &command);
    bool hasPending() const;

private:
    Stream &m_input;
    CommandHandler m_handler;
    String m_buffer;
    std::queue<String> m_queue;

    void processQueue();
};

#endif  // CLI_SERVICE_H
