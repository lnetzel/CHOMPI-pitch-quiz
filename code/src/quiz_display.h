#pragma once

#include "pitch_quiz.h"

namespace quiz_display
{
struct Color
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

enum class Shutdown { None, Fade, White, Dark };

struct TransportView
{
    bool exit_holding = false;
    unsigned exit_keys = 0;
    bool reboot_confirming = false;
    bool blink_on = false;
    Shutdown shutdown = Shutdown::None;
    uint8_t fade = 0;
};

inline Color ExitHoldColor(unsigned white_key, unsigned filled)
{
    return white_key < 15 && white_key < filled ? Color{0, 0, 48} : Color{0, 0, 0};
}

inline Color ShutdownColor(const TransportView& transport, bool confirmation_key = false)
{
    if(transport.shutdown == Shutdown::White)
        return {48, 48, 48};
    if(transport.shutdown == Shutdown::Fade && !confirmation_key)
        return {transport.fade, 48, transport.fade};
    return {0, 0, 0};
}

inline Color LoopColor(bool game_active, uint32_t elapsed_ms)
{
    if(!game_active)
        return {0, 0, 0};
    const unsigned phase = elapsed_ms % 2400;
    const unsigned pulse = phase < 1200 ? phase : 2400 - phase;
    const uint8_t level = static_cast<uint8_t>(4 + pulse * 44 / 1200);
    return {level, level, 0};
}

// PLAY LED: blue pulse in Perfect Pitch mode, green pulse in Chords mode.
inline Color PlayColor(bool game_active, bool chords_mode, uint32_t elapsed_ms)
{
    const unsigned phase = elapsed_ms % 2000;
    const unsigned pulse = phase < 1000 ? phase : 2000 - phase;
    const uint8_t level
        = game_active ? 64 : static_cast<uint8_t>(6 + pulse * 14 / 1000);
    return chords_mode ? Color{0, level, 0} : Color{0, 0, level};
}

inline Color ShiftColor(uint32_t elapsed_ms)
{
    const unsigned phase = elapsed_ms % 2400;
    const unsigned pulse = phase < 1200 ? phase : 2400 - phase;
    return {static_cast<uint8_t>(4 + pulse * 28 / 1200), 0, 0};
}

inline Color ResultColor(uint32_t progress,
                         unsigned led,
                         bool result_hold,
                         uint32_t elapsed_ms,
                         unsigned wrong_attempts = 0)
{
    if(led >= PitchQuiz::kRoundLength)
        return {0, 0, 0};

    constexpr uint32_t all_correct = ((1u << (2 * PitchQuiz::kRoundLength)) - 1) / 3;
    if(result_hold && progress == all_correct)
    {
        const unsigned hue = (led * 1536 / PitchQuiz::kRoundLength
                              + (elapsed_ms % 2000) * 1536 / 2000) % 1536;
        const uint8_t rising = static_cast<uint8_t>((hue % 256) * 32 / 256);
        const uint8_t falling = static_cast<uint8_t>(32 - rising);
        switch(hue / 256)
        {
            case 0: return {32, rising, 0};
            case 1: return {falling, 32, 0};
            case 2: return {0, 32, rising};
            case 3: return {0, falling, 32};
            case 4: return {rising, 0, 32};
            default: return {32, 0, falling};
        }
    }

    if(result_hold && (elapsed_ms / 250) % 2 != 0)
        return {0, 0, 0};

    unsigned current_round = 0;
    while(current_round < PitchQuiz::kRoundLength
          && ((progress >> (2 * current_round)) & 3) != 0)
        ++current_round;
    if(!result_hold && led == current_round)
    {
        if(wrong_attempts == 1)
            return {32, 32, 0};
        if(wrong_attempts == 2)
            return {48, 24, 0};
    }

    const unsigned result = (progress >> (2 * led)) & 3;
    return {static_cast<uint8_t>(result == 2 ? 32 : 0),
            static_cast<uint8_t>(result == 1 ? 32 : 0),
            0};
}
}