#include "sd_mmc_filesystem.h"
#include <utility>

namespace infra {

SDMMCFile::SDMMCFile(File file)
    : m_file(std::move(file))
{
}

bool SDMMCFile::available() {
    return m_file && m_file.available();
}

String SDMMCFile::readString() {
    if (!m_file) {
        return String();
    }
    return m_file.readString();
}

String SDMMCFile::readStringUntil(char delimiter) {
    if (!m_file) {
        return String();
    }
    return m_file.readStringUntil(delimiter);
}

void SDMMCFile::close() {
    if (m_file) {
        m_file.close();
    }
}

bool SDMMCFileSystem::exists(const char *path) const {
    return SD_MMC.exists(path);
}

std::unique_ptr<IFile> SDMMCFileSystem::open(const char *path, const char *mode) {
    File file = SD_MMC.open(path, mode);
    if (!file) {
        return nullptr;
    }
    return std::unique_ptr<IFile>(new SDMMCFile(std::move(file)));
}

} // namespace infra
