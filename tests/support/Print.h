#ifndef PRINT_STUB_H
#define PRINT_STUB_H

#include <cstddef>
#include <cstdint>

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t) { return 1; }
    size_t write(const char *buffer) {
        size_t len = 0;
        if (buffer) {
            while (buffer[len] != '\0') {
                write(static_cast<uint8_t>(buffer[len]));
                ++len;
            }
        }
        return len;
    }
};

#endif  // PRINT_STUB_H

