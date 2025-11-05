#ifndef INFRA_SD_MMC_FILESYSTEM_H
#define INFRA_SD_MMC_FILESYSTEM_H

#include "filesystem.h"
#include <SD_MMC.h>

namespace infra {

class SDMMCFile : public IFile {
public:
    explicit SDMMCFile(File file);
    ~SDMMCFile() override = default;

    bool available() override;
    String readString() override;
    String readStringUntil(char delimiter) override;
    void close() override;

private:
    File m_file;
};

class SDMMCFileSystem : public IFileSystem {
public:
    SDMMCFileSystem() = default;
    ~SDMMCFileSystem() override = default;

    bool exists(const char *path) const override;
    std::unique_ptr<IFile> open(const char *path, const char *mode) override;
};

} // namespace infra

#endif // INFRA_SD_MMC_FILESYSTEM_H
