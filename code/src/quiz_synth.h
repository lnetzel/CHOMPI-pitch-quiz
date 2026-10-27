#pragma once

#include "Synthesis/oscillator.h"
#include "Filters/tone.h"
#include <cmath>

namespace quiz_synth
{
class Oscillator
{
  public:
    Oscillator()
    {
        oscillator_.Init(48000.0f);
        oscillator_.SetAmp(1.0f);
        SetSquare(false);
        Reset();
    }

    void SetSquare(bool square)
    {
        square_ = square;
        oscillator_.SetWaveform(square ? daisysp::Oscillator::WAVE_POLYBLEP_SQUARE
                                       : daisysp::Oscillator::WAVE_TRI);
    }

    void Reset()
    {
        oscillator_.Reset();
        for(auto& filter : lowpass_)
        {
            filter.Init(48000.0f);
            filter.SetFreq(2000.0f);
        }
    }

    float Process(float frequency)
    {
        oscillator_.SetFreq(frequency);
        const float sample = oscillator_.Process();
        const float softened = lowpass_[1].Process(lowpass_[0].Process(sample));
        return square_ ? softened : sample;
    }

  private:
    daisysp::Oscillator oscillator_{};
    daisysp::Tone lowpass_[2];
    bool square_ = false;
};

class FreePlay
{
  public:
    static constexpr unsigned kVoiceCount = 6;
    static constexpr float kOutputGain = 0.22f / kVoiceCount;
    static constexpr unsigned kReleaseSamples = 24000;
    static constexpr unsigned kAttackSamples = 960;
    static constexpr unsigned kTransitionSamples = 240;

    void Clear()
    {
        for(auto& voice : voices_)
            voice.active = false;
    }

    void NoteOn(unsigned note)
    {
        if(note > 127)
            return;
        Voice* selected = nullptr;
        for(auto& voice : voices_)
            if(voice.active && voice.note == note)
            {
                selected = &voice;
                break;
            }
        if(!selected)
            for(auto& voice : voices_)
                if(!voice.active)
                {
                    selected = &voice;
                    break;
                }
        if(!selected)
            for(auto& voice : voices_)
                if(!selected || (voice.releasing && !selected->releasing)
                   || (voice.releasing == selected->releasing && voice.age < selected->age))
                    selected = &voice;

        selected->transition_level = selected->active ? selected->last_output : 0.0f;
        selected->transition_remaining = kTransitionSamples;
        selected->note = note;
        selected->frequency = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
        selected->envelope = 0.0f;
        selected->remaining = 0;
        selected->age = ++age_;
        selected->active = true;
        selected->releasing = false;
        selected->oscillator.Reset();
    }

    void NoteOff(unsigned note)
    {
        for(auto& voice : voices_)
            if(voice.active && voice.note == note && !voice.releasing)
            {
                voice.releasing = true;
                voice.release_level = voice.envelope;
                voice.remaining = kReleaseSamples;
            }
    }

    unsigned ActiveVoices() const
    {
        unsigned count = 0;
        for(const auto& voice : voices_)
            count += voice.active ? 1 : 0;
        return count;
    }

    bool IsHeld(unsigned note) const
    {
        for(const auto& voice : voices_)
            if(voice.active && !voice.releasing && voice.note == note)
                return true;
        return false;
    }

    float Process()
    {
        float output = 0.0f;
        for(auto& voice : voices_)
        {
            if(!voice.active)
                continue;
            if(voice.releasing)
            {
                voice.envelope = voice.release_level * voice.remaining / kReleaseSamples;
                if(--voice.remaining == 0)
                    voice.active = false;
            }
            else
            {
                voice.envelope += 1.0f / kAttackSamples;
                if(voice.envelope > 1.0f)
                    voice.envelope = 1.0f;
            }
            float sample = voice.envelope * voice.oscillator.Process(voice.frequency);
            if(voice.transition_remaining > 0)
            {
                sample += voice.transition_level * voice.transition_remaining / kTransitionSamples;
                --voice.transition_remaining;
            }
            voice.last_output = sample;
            output += sample;
        }
        return output * kOutputGain;
    }

  private:
    struct Voice
    {
        Oscillator oscillator;
        float frequency = 0.0f;
        float envelope = 0.0f;
        float release_level = 0.0f;
        float last_output = 0.0f;
        float transition_level = 0.0f;
        unsigned note = 0;
        unsigned remaining = 0;
        unsigned transition_remaining = 0;
        uint32_t age = 0;
        bool active = false;
        bool releasing = false;
    };
    Voice voices_[kVoiceCount];
    uint32_t age_ = 0;
};
}