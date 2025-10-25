#include "skit_selector.h"
#include "logging_manager.h"
#include <algorithm>
#include <cmath>
#include <Arduino.h>

static constexpr const char* TAG = "SkitSelector";

// Constructor: Initializes the SkitSelector with a list of parsed skits
SkitSelector::SkitSelector(const std::vector<ParsedSkit> &skits)
    : m_lastPlayedSkitName("") // Initialize to an empty string
{
    for (const auto &skit : skits)
    {
        m_skitStats.push_back({skit, 0, 0});
    }
    LOG_INFO(TAG, "Initialized with %u skits", static_cast<unsigned>(m_skitStats.size()));
}

// Selects the next skit to be played based on weighted random selection
ParsedSkit SkitSelector::selectNextSkit()
{
    unsigned long currentTime = millis();
    sortSkitsByWeight();

    // Define the maximum size of the selection pool
    int maxPoolSize = std::min(3, static_cast<int>(m_skitStats.size()));

    // Build a selection pool excluding the last played skit
    std::vector<SkitStats *> availableSkits;
    for (auto &skitStat : m_skitStats)
    {
        if (skitStat.skit.audioFile != m_lastPlayedSkitName)
        {
            availableSkits.push_back(&skitStat);
            if (availableSkits.size() >= maxPoolSize)
                break; // Limit the pool size to maxPoolSize
        }
    }

    // If no skits are available after exclusion and multiple skits exist, include all skits
    if (availableSkits.empty() && m_skitStats.size() > 1)
    {
        for (auto &skitStat : m_skitStats)
        {
            availableSkits.push_back(&skitStat);
            if (availableSkits.size() >= maxPoolSize)
                break;
        }
    }

    // If only one skit exists, it will be selected regardless of previous play
    // Otherwise, select randomly from the available pool
    int selectedIndex;
    if (availableSkits.empty())
    {
        // Only one skit exists
        selectedIndex = 0;
    }
    else
    {
        int randomIdx = random(availableSkits.size());
        // Find the index of the selected skit in m_skitStats
        selectedIndex = std::distance(
            m_skitStats.begin(),
            std::find_if(m_skitStats.begin(), m_skitStats.end(),
                         [&](const SkitStats &stat)
                         { return &stat == availableSkits[randomIdx]; }));
    }

    // Update the selected skit's play count and last played time
    auto &selectedSkit = m_skitStats[selectedIndex];
    selectedSkit.playCount++;
    selectedSkit.lastPlayedTime = currentTime;
    m_lastPlayedSkitName = selectedSkit.skit.audioFile;

    LOG_INFO(TAG, "Selected skit: %s (play count: %d)", 
             selectedSkit.skit.audioFile.c_str(), selectedSkit.playCount);

    return selectedSkit.skit;
}

// Updates the play count and last played time for a specific skit
void SkitSelector::updateSkitPlayCount(const String &skitName)
{
    auto it = std::find_if(m_skitStats.begin(), m_skitStats.end(),
                           [&skitName](const SkitStats &stats)
                           { return stats.skit.audioFile == skitName; });
    if (it != m_skitStats.end())
    {
        it->playCount++;
        it->lastPlayedTime = millis();
        m_lastPlayedSkitName = it->skit.audioFile; // Ensure consistency
        LOG_DEBUG(TAG, "Updated play count for skit: %s (count: %d)", 
                  skitName.c_str(), it->playCount);
    }
}

// Calculates the weight of a skit based on its play count and last played time
double SkitSelector::calculateSkitWeight(const SkitStats &stats, unsigned long currentTime)
{
    // Use logarithmic time factor to gradually increase priority over time
    double timeFactor = log(static_cast<double>(currentTime - stats.lastPlayedTime) + 1.0);
    // Inverse play count factor to prioritize less frequently played skits
    double playCountFactor = 1.0 / (static_cast<double>(stats.playCount) + 1.0);
    return timeFactor * playCountFactor;
}

// Sorts the skits by their calculated weights in descending order
void SkitSelector::sortSkitsByWeight()
{
    unsigned long currentTime = millis();
    std::sort(m_skitStats.begin(), m_skitStats.end(),
              [this, currentTime](const SkitStats &a, const SkitStats &b) -> bool
              {
                  return calculateSkitWeight(a, currentTime) > calculateSkitWeight(b, currentTime);
              });
}


