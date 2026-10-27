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
    if(shift_held && rising[kLoopSwitch])
        return;
    if(shift_held)
        for(unsigned level = 0; level < PitchQuiz::kLevelCount; ++level)
            if(rising[kBlackKeySwitches[level]])
            {
                quiz.StartLevel(level + 1, seed);
                return;
            }

    if(rising[kPlaySwitch])
    {
        if(!shift_held && quiz.GetState() == PitchQuiz::State::Idle)
            quiz.Start(seed);
    }
    else if(rising[kLoopSwitch])
    {
        if(!shift_held)
            quiz.ReplayTarget();
    }
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

class Transport
{
  public:
    enum class Reboot { None, Confirming, Requested };
    static constexpr uint32_t kExitHoldMs = 2000;

    bool Update(PitchQuiz& quiz,
                const bool (&rising)[40],
                const bool (&held)[40],
                uint32_t now)
    {
        if(reboot_ == Reboot::Requested)
            return true;
        if(reboot_ == Reboot::Confirming)
        {
            exit_holding_ = false;
            quiz.StopFreePlay();
            if(rising[kLoopSwitch])
                reboot_ = Reboot::None;
            else if(rising[kShiftSwitch])
            {
                reboot_ = Reboot::Requested;
                quiz.ExitToIdle();
            }
            return true;
        }
        if(held[kShiftSwitch] && rising[kLoopSwitch])
        {
            reboot_ = Reboot::Confirming;
            exit_holding_ = false;
            quiz.StopFreePlay();
            return true;
        }

        if(!held[kPlaySwitch])
        {
            exit_holding_ = false;
            play_from_idle_ = false;
        }
        if(rising[kPlaySwitch])
        {
            play_from_idle_ = quiz.GetState() == PitchQuiz::State::Idle;
            exit_holding_ = !play_from_idle_ && !held[kShiftSwitch];
            exit_started_ = now;
        }
        if(held[kShiftSwitch] || quiz.GetState() == PitchQuiz::State::Idle)
            exit_holding_ = false;
        if(exit_holding_ && !play_from_idle_ && now - exit_started_ >= kExitHoldMs)
        {
            quiz.ExitToIdle();
            exit_holding_ = false;
            return true;
        }
        return false;
    }

    bool ExitHolding() const { return exit_holding_; }

    unsigned ExitKeys(uint32_t now) const
    {
        if(!exit_holding_)
            return 0;
        const uint32_t elapsed = now - exit_started_;
        return elapsed >= kExitHoldMs ? 15 : elapsed * 15 / kExitHoldMs;
    }

    Reboot RebootState() const { return reboot_; }

    uint32_t PackUi(uint32_t now) const
    {
        return ExitKeys(now) | (exit_holding_ ? 1u << 4 : 0)
               | (reboot_ == Reboot::Confirming ? 1u << 5 : 0)
               | (reboot_ == Reboot::Requested ? 1u << 6 : 0);
    }

  private:
    Reboot reboot_ = Reboot::None;
    bool play_from_idle_ = false;
    bool exit_holding_ = false;
    uint32_t exit_started_ = 0;
};

static constexpr uint32_t kProgressMask = (1u << 20) - 1;
inline void TurnRange(PitchQuiz& quiz, int turn)
{
    const int range = static_cast<int>(quiz.Range()) + turn;
    quiz.SetRange(range < 1 ? 1 : static_cast<unsigned>(range));
}

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