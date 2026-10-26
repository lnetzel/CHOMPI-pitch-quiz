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

inline Color ResultColor(uint32_t progress,
                         unsigned led,
                         bool result_hold,
                         uint32_t elapsed_ms)
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

    const unsigned result = (progress >> (2 * led)) & 3;
    return {static_cast<uint8_t>(result == 2 ? 32 : 0),
            static_cast<uint8_t>(result == 1 ? 32 : 0),
            0};
}
}