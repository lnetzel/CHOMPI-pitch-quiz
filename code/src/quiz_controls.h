#pragma once

#include "pitch_quiz.h"

namespace quiz_controls
{
static constexpr unsigned kShiftSwitch = 5;
static constexpr unsigned kPlaySwitch = 33;
static constexpr unsigned kLoopSwitch = 34;
static constexpr unsigned kKeySwitches[25] = {
    15, 7, 8, 12, 9, 10, 13, 11, 14, 16, 21, 17, 18,
    22, 19, 23, 20, 24, 29, 25, 30, 26, 31, 27, 28
};
static constexpr unsigned kBlackKeySwitches[10] = {
    7, 12, 13, 14, 21, 22, 23, 29, 30, 31
};
static_assert(PitchQuiz::kLevelCount <= 10, "Level selection has ten black keys");

inline void Process(PitchQuiz& quiz,
                    const bool (&rising)[40],
                    bool shift_held,
                    uint32_t seed)
{
    if(shift_held)
        for(unsigned level = 0; level < PitchQuiz::kLevelCount; ++level)
            if(rising[kBlackKeySwitches[level]])
            {
                quiz.StartLevel(level + 1, seed);
                return;
            }

    if(rising[kPlaySwitch] || rising[kLoopSwitch])
        quiz.Start(seed);
    else if(!shift_held)
        for(unsigned key = 0; key < 25; ++key)
            if(rising[kKeySwitches[key]])
                quiz.Press(key + 48);
}

static constexpr uint32_t kProgressMask = (1u << 20) - 1;
static constexpr unsigned kLevelShift = 20;
static constexpr uint32_t kShiftHeldMask = 1u << 24;
static constexpr uint32_t kActiveMask = 1u << 25;
static constexpr uint32_t kResultHoldMask = 1u << 26;

inline uint32_t PackUi(const PitchQuiz& quiz, bool shift_held)
{
    return quiz.Progress() | (quiz.CurrentLevel() << kLevelShift)
           | (shift_held ? kShiftHeldMask : 0)
           | (quiz.GetState() != PitchQuiz::State::Idle ? kActiveMask : 0)
           | (quiz.GetState() == PitchQuiz::State::LevelComplete ? kResultHoldMask : 0);
}
}