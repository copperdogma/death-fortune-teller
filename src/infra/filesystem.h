#ifndef INFRA_FILESYSTEM_H
#define INFRA_FILESYSTEM_H

#include <Arduino.h>
#include <memory>

namespace infra {

class IFile {
public:
    virtual ~IFile() = default;

    virtual bool available() = 0;
    virtual String readString() = 0;
    virtual String readStringUntil(char delimiter) = 0;
    virtual void close() = 0;
};

class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    virtual bool exists(const char *path) const = 0;
    virtual std::unique_ptr<IFile> open(const char *path, const char *mode) = 0;
};

} // namespace infra

#endif // INFRA_FILESYSTEM_H
