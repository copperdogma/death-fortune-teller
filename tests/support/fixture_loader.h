#pragma once

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

inline std::string loadFixture(const char *relativePath) {
    if (!relativePath) {
        throw std::invalid_argument("relativePath is null");
    }
    std::ifstream file(std::string("tests/fixtures/") + relativePath, std::ios::in | std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::string("Failed to open fixture: ") + relativePath);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
