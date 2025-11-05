#pragma once

#include <cstdio>

#define LOG_VERBOSE(tag, fmt, ...) do { std::fprintf(stderr, "[VERBOSE][%s] " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#define LOG_DEBUG(tag, fmt, ...)   do { std::fprintf(stderr, "[DEBUG][%s] " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#define LOG_INFO(tag, fmt, ...)    do { std::fprintf(stderr, "[INFO][%s] " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#define LOG_WARN(tag, fmt, ...)    do { std::fprintf(stderr, "[WARN][%s] " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#define LOG_ERROR(tag, fmt, ...)   do { std::fprintf(stderr, "[ERROR][%s] " fmt "\n", tag, ##__VA_ARGS__); } while (0)
