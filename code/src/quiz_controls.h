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

class ShiftEncoder
{
    public:
        int Update(bool state_a, bool state_b)
        {
                history_a_ = static_cast<uint8_t>((history_a_ << 1) | state_a);
                history_b_ = static_cast<uint8_t>((history_b_ << 1) | state_b);
                if((history_a_ & 7) == 4 && (history_b_ & 3) == 0)
                        return 1;
                if((history_b_ & 7) == 4 && (history_a_ & 3) == 0)
                        return -1;
                return 0;
        }

    private:
        uint8_t history_a_ = 0xff;
        uint8_t history_b_ = 0xff;
};

inline void Process(PitchQuiz& quiz,
                    const bool (&rising)[40],
                    bool shift_held,
                    uint32_t seed,
                    const bool* falling = nullptr)
{
    if(falling)
        for(unsigned key = 0; key < 25; ++key)
            if(falling[kKeySwitches[key]])
                quiz.NoteOff(key + 48);
    if(shift_held)
        quiz.StopFreePlay();
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
            {
                if(quiz.GetState() == PitchQuiz::State::Idle)
                    quiz.NoteOn(key + 48);
                else
                    quiz.Press(key + 48);
            }
}

inline void TurnWaveform(PitchQuiz& quiz, int turn)
{
    int waveform = static_cast<int>(quiz.Waveform()) + turn;
    if(waveform < 0)
        waveform = 0;
    if(waveform >= static_cast<int>(quiz_synth::kWaveformCount))
        waveform = quiz_synth::kWaveformCount - 1;
    quiz.SetWaveform(static_cast<quiz_synth::Waveform>(waveform));
}

static constexpr uint32_t kProgressMask = (1u << 20) - 1;
static constexpr unsigned kLevelShift = 20;
static constexpr uint32_t kShiftHeldMask = 1u << 24;
static constexpr uint32_t kActiveMask = 1u << 25;
static constexpr uint32_t kResultHoldMask = 1u << 26;
static constexpr unsigned kWaveformShift = 27;

inline uint32_t PackUi(const PitchQuiz& quiz, bool shift_held)
{
    return quiz.Progress() | (quiz.CurrentLevel() << kLevelShift)
           | (shift_held ? kShiftHeldMask : 0)
           | (quiz.GetState() != PitchQuiz::State::Idle ? kActiveMask : 0)
           | (quiz.GetState() == PitchQuiz::State::LevelComplete ? kResultHoldMask : 0)
           | (static_cast<unsigned>(quiz.Waveform()) << kWaveformShift);
}
}