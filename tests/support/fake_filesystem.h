#pragma once

#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include "infra/filesystem.h"

class FakeFile : public infra::IFile {
public:
    explicit FakeFile(const std::string &content)
        : m_closed(false)
    {
        std::stringstream ss(content);
        std::string line;
        while (std::getline(ss, line)) {
            m_lines.push(line);
        }
        if (!content.empty() && content.back() == '\n') {
            m_lines.push(std::string());
        }
    }

    bool available() override {
        return !m_closed && !m_lines.empty();
    }

    String readString() override {
        if (m_closed) {
            return String();
        }
        std::string aggregated;
        while (!m_lines.empty()) {
            if (!aggregated.empty()) {
                aggregated.push_back('\n');
            }
            aggregated += m_lines.front();
            m_lines.pop();
        }
        return String(aggregated);
    }

    String readStringUntil(char) override {
        if (m_closed || m_lines.empty()) {
            return String();
        }
        std::string next = m_lines.front();
        m_lines.pop();
        return String(next);
    }

    void close() override {
        m_closed = true;
        while (!m_lines.empty()) {
            m_lines.pop();
        }
    }

private:
    std::queue<std::string> m_lines;
    bool m_closed;
};

class FakeFileSystem : public infra::IFileSystem {
public:
    void addFile(const std::string &path, const std::string &content) {
        m_files[path] = content;
    }

    bool exists(const char *path) const override {
        return path && m_files.find(path) != m_files.end();
    }

    std::unique_ptr<infra::IFile> open(const char *path, const char *) override {
        auto it = m_files.find(path ? path : "");
        if (it == m_files.end()) {
            return nullptr;
        }
        return std::unique_ptr<infra::IFile>(new FakeFile(it->second));
    }

private:
    std::map<std::string, std::string> m_files;
};
