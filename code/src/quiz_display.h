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

inline Color WaveformColor(unsigned white_key, unsigned waveform)
{
    if(white_key < 15 && waveform < quiz_synth::kWaveformCount
       && white_key / 5 == waveform)
        return {48, 48, 48};
    return {0, 0, 0};
}

inline Color RangeColor(unsigned white_key, unsigned range)
{
    if(white_key >= 15 || white_key >= range)
        return {0, 0, 0};
    if(white_key <= 7)
        return {static_cast<uint8_t>(48 * white_key / 7),
                static_cast<uint8_t>(48 - 24 * white_key / 7), 0};
    return {48, static_cast<uint8_t>(24 * (14 - white_key) / 7), 0};
}

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