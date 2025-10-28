#ifndef AUDIO_DIRECTORY_SELECTOR_H
#define AUDIO_DIRECTORY_SELECTOR_H

#include <Arduino.h>
#include <vector>

class AudioDirectorySelector {
public:
    AudioDirectorySelector();

    // Selects a clip from the given directory using weighted random logic.
    // Returns an empty String if no playable clips are available.
    String selectClip(const char *directory, const char *description = nullptr);

    // Reset playback statistics for a directory (used by self-tests).
    void resetStats(const char *directory);

private:
    struct ClipStats {
        String path;
        int playCount;
        unsigned long lastPlayedMs;
    };

    struct CategoryState {
        String directory;
        std::vector<ClipStats> clips;
        String lastPlayedPath;
    };

    std::vector<CategoryState> m_categories;

    CategoryState &getOrCreateCategory(const char *directory);
    CategoryState *findCategory(const char *directory);
    void refreshCategoryClips(CategoryState &state, const char *description);
    double calculateClipWeight(const ClipStats &clip, unsigned long currentTime) const;
};

#endif // AUDIO_DIRECTORY_SELECTOR_H
