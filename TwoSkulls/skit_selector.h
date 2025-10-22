#ifndef SKIT_SELECTOR_H
#define SKIT_SELECTOR_H

#include <vector>
#include <string>
#include "parsed_skit.h"

// SkitSelector class manages the selection and playback of skits
// It uses a weighted random selection algorithm to ensure variety and fairness in skit playback
class SkitSelector
{
public:
    // Constructor: Initializes the SkitSelector with a list of parsed skits
    SkitSelector(const std::vector<ParsedSkit> &skits);

    // Selects the next skit to be played based on weighted random selection
    // Returns: A ParsedSkit object representing the selected skit
    ParsedSkit selectNextSkit();

    // Updates the play count and last played time for a specific skit
    // Param: skitName - The name of the skit to update
    void updateSkitPlayCount(const String &skitName);

private:
    // Struct to hold statistics for each skit
    struct SkitStats
    {
        ParsedSkit skit;
        int playCount;
        unsigned long lastPlayedTime;
    };

    // Vector to store statistics for all skits
    std::vector<SkitStats> m_skitStats;

    // Stores the name of the last played skit
    String m_lastPlayedSkitName;

    // Calculates the weight of a skit based on its play count and last played time
    // This weight is used to prioritize skits that haven't been played recently or frequently
    // Params:
    //   stats - The SkitStats object for the skit
    //   currentTime - The current time in milliseconds
    // Returns: A double representing the calculated weight
    double calculateSkitWeight(const SkitStats &stats, unsigned long currentTime);

    // Sorts the skits by their calculated weights in descending order
    void sortSkitsByWeight();
};

#endif // SKIT_SELECTOR_H