#include "audio_directory_selector.h"
#include "logging_manager.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include "SD_MMC.h"

static constexpr const char *TAG = "AudioDirSel";

AudioDirectorySelector::AudioDirectorySelector() = default;

String AudioDirectorySelector::selectClip(const char *directory, const char *description) {
    if (!directory || directory[0] == '\0') {
        LOG_WARN(TAG, "Invalid directory provided for selection");
        return "";
    }

    CategoryState &state = getOrCreateCategory(directory);
    refreshCategoryClips(state, description);

    if (state.clips.empty()) {
        LOG_WARN(TAG, "No playable clips in %s%s%s",
                 directory,
                 description ? " (" : "",
                 description ? description : "");
        if (description) {
            LOG_WARN(TAG, "Hint: add at least one .wav file under %s", directory);
        }
        return "";
    }

    unsigned long now = millis();

    std::vector<size_t> order(state.clips.size());
    std::iota(order.begin(), order.end(), 0);

    std::sort(order.begin(), order.end(),
              [&](size_t lhs, size_t rhs) {
                  double wL = calculateClipWeight(state.clips[lhs], now);
                  double wR = calculateClipWeight(state.clips[rhs], now);
                  if (fabs(wL - wR) < 0.0001) {
                      return state.clips[lhs].path < state.clips[rhs].path;
                  }
                  return wL > wR;
              });

    const size_t maxPool = std::min<size_t>(3, order.size());
    std::vector<size_t> pool;
    pool.reserve(maxPool);

    for (size_t idx : order) {
        if (pool.size() >= maxPool) {
            break;
        }
        const ClipStats &candidate = state.clips[idx];
        if (state.clips.size() > 1 && candidate.path == state.lastPlayedPath) {
            continue;
        }
        pool.push_back(idx);
    }

    if (pool.empty()) {
        pool.assign(order.begin(), order.begin() + maxPool);
    }

    size_t choiceIdx = 0;
    if (pool.size() > 1) {
        choiceIdx = static_cast<size_t>(random(static_cast<long>(pool.size())));
        if (choiceIdx >= pool.size()) {
            choiceIdx = 0;
        }
    }

    ClipStats &selected = state.clips[pool[choiceIdx]];
    selected.playCount++;
    selected.lastPlayedMs = now;
    state.lastPlayedPath = selected.path;

    LOG_INFO(TAG, "Selected %s clip: %s (plays=%d)",
             description ? description : "audio",
             selected.path.c_str(),
             selected.playCount);

    return selected.path;
}

void AudioDirectorySelector::resetStats(const char *directory) {
    CategoryState *state = findCategory(directory);
    if (!state) {
        return;
    }
    for (auto &clip : state->clips) {
        clip.playCount = 0;
        clip.lastPlayedMs = 0;
    }
    state->lastPlayedPath = "";
}

AudioDirectorySelector::CategoryState &AudioDirectorySelector::getOrCreateCategory(const char *directory) {
    CategoryState *state = findCategory(directory);
    if (state) {
        return *state;
    }
    CategoryState fresh;
    fresh.directory = directory;
    fresh.lastPlayedPath = "";
    m_categories.push_back(fresh);
    return m_categories.back();
}

AudioDirectorySelector::CategoryState *AudioDirectorySelector::findCategory(const char *directory) {
    if (!directory) {
        return nullptr;
    }
    for (auto &category : m_categories) {
        if (category.directory.equals(directory)) {
            return &category;
        }
    }
    return nullptr;
}

void AudioDirectorySelector::refreshCategoryClips(CategoryState &state, const char *description) {
    File dir = SD_MMC.open(state.directory.c_str());
    if (!dir || !dir.isDirectory()) {
        LOG_WARN(TAG, "Directory missing or invalid: %s%s%s",
                 state.directory.c_str(),
                 description ? " (" : "",
                 description ? description : "");
        state.clips.clear();
        state.lastPlayedPath = "";
        if (dir) {
            dir.close();
        }
        return;
    }

    std::vector<String> discovered;
    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            name.trim();
            if (!name.startsWith(".")) {
                size_t sizeBytes = entry.size();
                if (sizeBytes > 0 && (name.endsWith(".wav") || name.endsWith(".WAV"))) {
                    String fullPath = state.directory;
                    if (!fullPath.endsWith("/")) {
                        fullPath += '/';
                    }
                    fullPath += name;
                    discovered.push_back(fullPath);
                }
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    std::vector<ClipStats> updated;
    updated.reserve(discovered.size());

    for (const String &path : discovered) {
        auto existing = std::find_if(state.clips.begin(), state.clips.end(),
                                     [&](const ClipStats &clip) { return clip.path == path; });
        if (existing != state.clips.end()) {
            updated.push_back(*existing);
        } else {
            ClipStats fresh{path, 0, 0};
            updated.push_back(fresh);
        }
    }

    state.clips.swap(updated);

    if (!state.lastPlayedPath.isEmpty()) {
        auto stillExists = std::find_if(state.clips.begin(), state.clips.end(),
                                        [&](const ClipStats &clip) { return clip.path == state.lastPlayedPath; });
        if (stillExists == state.clips.end()) {
            state.lastPlayedPath = "";
        }
    }
}

double AudioDirectorySelector::calculateClipWeight(const ClipStats &clip, unsigned long currentTime) const {
    double timeFactor = log(static_cast<double>(currentTime - clip.lastPlayedMs) + 1.0);
    double playCountFactor = 1.0 / (static_cast<double>(clip.playCount) + 1.0);
    return timeFactor * playCountFactor;
}
