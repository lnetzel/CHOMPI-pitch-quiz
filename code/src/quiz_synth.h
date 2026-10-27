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

    void ClearHeld()
    {
        for(auto& voice : voices_)
            if(!voice.game_voice)
                voice.active = false;
    }

    void NoteOn(unsigned note)
    {
        StartVoice(note, 0, kReleaseSamples, kAttackSamples, false, false, false, true);
    }

    void PlayNote(unsigned note,
                  unsigned sustain_samples,
                  unsigned release_samples,
                  bool square = false)
    {
        StartVoice(note,
                   sustain_samples,
                   release_samples,
                   kAttackSamples,
                   square,
                   true,
                   true,
                   false);
    }

    void NoteOff(unsigned note, unsigned release_samples = kReleaseSamples)
    {
        for(auto& voice : voices_)
            if(voice.active && voice.note == note && !voice.releasing && !voice.one_shot)
                ReleaseVoice(voice, release_samples);
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
            if(voice.one_shot && !voice.releasing && voice.sustain_remaining == 0)
                ReleaseVoice(voice, voice.release_length);
            if(!voice.active)
                continue;
            if(voice.releasing)
            {
                voice.envelope = voice.release_level * voice.remaining / voice.release_length;
                if(--voice.remaining == 0)
                    voice.active = false;
            }
            else
            {
                voice.envelope += 1.0f / voice.attack_length;
                if(voice.envelope > 1.0f)
                    voice.envelope = 1.0f;
                if(voice.one_shot && voice.sustain_remaining > 0)
                    --voice.sustain_remaining;
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
                unsigned sustain_remaining = 0;
                unsigned release_length = kReleaseSamples;
                unsigned attack_length = kAttackSamples;
                unsigned transition_remaining = 0;
                uint32_t age = 0;
                bool active = false;
                bool releasing = false;
                bool one_shot = false;
                bool game_voice = false;
        };

    void StartVoice(unsigned note,
                    unsigned sustain_samples,
                    unsigned release_samples,
                    unsigned attack_samples,
                    bool square,
                    bool one_shot,
                    bool game_voice,
                    bool reuse_held)
    {
        if(note > 127)
            return;
        Voice* selected = nullptr;
        if(reuse_held)
            for(auto& voice : voices_)
                if(voice.active && !voice.releasing && !voice.one_shot && voice.note == note)
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
        selected->release_level = 0.0f;
        selected->remaining = 0;
        selected->sustain_remaining = sustain_samples;
        selected->release_length = release_samples ? release_samples : 1;
        selected->attack_length = attack_samples ? attack_samples : 1;
        selected->age = ++age_;
        selected->active = true;
        selected->releasing = false;
        selected->one_shot = one_shot;
        selected->game_voice = game_voice;
        selected->oscillator.SetSquare(square);
        selected->oscillator.Reset();
    }

    static void ReleaseVoice(Voice& voice, unsigned release_samples)
    {
        voice.releasing = true;
        voice.one_shot = false;
        voice.release_level = voice.envelope;
        voice.release_length = release_samples;
        voice.remaining = release_samples;
        if(release_samples == 0)
            voice.active = false;
    }

    Voice voices_[kVoiceCount];
    uint32_t age_ = 0;
};
}