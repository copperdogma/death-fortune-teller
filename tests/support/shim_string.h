#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <string>

class String {
public:
    using size_type = std::string::size_type;

    String() = default;
    String(const char *s) : m_data(s ? s : "") {}
    String(const std::string &s) : m_data(s) {}

    String &operator=(const char *s) {
        m_data = s ? s : "";
        return *this;
    }

    String &operator=(const std::string &s) {
        m_data = s;
        return *this;
    }

    String &operator=(const String &) = default;

    int length() const { return static_cast<int>(m_data.length()); }
    bool isEmpty() const { return m_data.empty(); }

    char operator[](size_type index) const { return m_data.at(index); }
    char &operator[](size_type index) { return m_data.at(index); }

    void trim() {
        auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
        auto start = std::find_if_not(m_data.begin(), m_data.end(), is_space);
        auto end = std::find_if_not(m_data.rbegin(), m_data.rend(), is_space).base();
        if (start >= end) {
            m_data.clear();
        } else {
            m_data.assign(start, end);
        }
    }

    int indexOf(char ch) const {
        auto pos = m_data.find(ch);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    int indexOf(const char *needle, int fromIndex = 0) const {
        if (!needle) {
            return -1;
        }
        size_type start = static_cast<size_type>(std::max(0, fromIndex));
        if (start >= m_data.size()) {
            return -1;
        }
        auto pos = m_data.find(needle, start);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    int indexOf(const String &needle, int fromIndex = 0) const {
        return indexOf(needle.c_str(), fromIndex);
    }

    String substring(int start) const {
        if (start < 0 || start > length()) {
            return String();
        }
        return String(m_data.substr(static_cast<size_type>(start)));
    }

    String substring(int start, int end) const {
        if (start < 0) start = 0;
        if (end < start) return String();
        size_type s = static_cast<size_type>(start);
        size_type e = static_cast<size_type>(std::min(end, length()));
        if (s >= m_data.size()) {
            return String();
        }
        return String(m_data.substr(s, e - s));
    }

    String &operator+=(const String &other) {
        m_data += other.m_data;
        return *this;
    }

    String &operator+=(const char *other) {
        m_data += other ? other : "";
        return *this;
    }

    friend String operator+(const String &lhs, const String &rhs) {
        return String(lhs.m_data + rhs.m_data);
    }

    const char *c_str() const { return m_data.c_str(); }

    String &toLowerCase() {
        std::transform(m_data.begin(), m_data.end(), m_data.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return *this;
    }

    int toInt() const {
        try {
            return std::stoi(m_data);
        } catch (const std::exception &) {
            return 0;
        }
    }

    float toFloat() const {
        try {
            return std::stof(m_data);
        } catch (const std::exception &) {
            return 0.0f;
        }
    }

    bool equalsIgnoreCase(const String &other) const {
        if (length() != other.length()) {
            return false;
        }
        for (size_type i = 0; i < m_data.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(m_data[i])) !=
                std::tolower(static_cast<unsigned char>(other.m_data[i]))) {
                return false;
            }
        }
        return true;
    }

    bool equalsIgnoreCase(const char *other) const {
        return equalsIgnoreCase(String(other));
    }

    bool operator==(const String &other) const { return m_data == other.m_data; }
    bool operator!=(const String &other) const { return m_data != other.m_data; }
    bool operator==(const char *other) const { return m_data == (other ? other : ""); }
    bool operator<(const String &other) const { return m_data < other.m_data; }

    std::string toStdString() const { return m_data; }

private:
    std::string m_data;
};

inline String operator+(const String &lhs, const char *rhs) {
    return String(lhs.toStdString() + (rhs ? std::string(rhs) : std::string()));
}

inline String operator+(const char *lhs, const String &rhs) {
    return String((lhs ? std::string(lhs) : std::string()) + rhs.toStdString());
}
