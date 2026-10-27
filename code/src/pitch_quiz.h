#pragma once

#include <cmath>
#include <cstdint>
#include "quiz_synth.h"

class PitchQuiz
{
  public:
    static constexpr unsigned kRoundLength = 10;
    static constexpr unsigned kMaxAttempts = 3;
    static constexpr unsigned kSampleRate = 48000;
    static constexpr float kReleaseSeconds = 0.5f;
    static constexpr unsigned kLevelCount = 2;

    enum class Level : unsigned { Natural = 1, Chromatic = 2 };
    enum class State { Idle, Prompt, Listening, Answer, Feedback, Pause, LevelComplete };
    enum class Result { Pending, Correct, Wrong };

    void Start(uint32_t seed)
    {
        free_play_.Clear();
        random_ = seed ? seed : 1;
        ResetLevel();
    }

    bool StartLevel(unsigned level, uint32_t seed)
    {
        if(level == 0 || level > kLevelCount)
            return false;
        level_ = static_cast<Level>(level);
        Start(seed);
        return true;
    }

    void Press(unsigned note)
    {
        if(state_ != State::Listening || note > 127)
            return;
        correct_ = note == target_;
        ++attempts_;
        Begin(State::Answer, 0.25f, Frequency(note), kReleaseSeconds);
    }

    bool ReplayTarget()
    {
        if(state_ != State::Listening)
            return false;
        Begin(State::Prompt, 0.7f, Frequency(target_), kReleaseSeconds);
        return true;
    }

    void ExitToIdle()
    {
        free_play_.Clear();
        state_ = State::Idle;
    }

    void SetVolume(float volume)
    {
        volume_ = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
    }

    float Volume() const { return volume_; }

    void SetRange(unsigned range)
    {
        range_ = range < 1 ? 1 : (range > 15 ? 15 : range);
    }

    unsigned Range() const { return range_; }

    void NoteOn(unsigned note)
    {
        if(state_ == State::Idle)
            free_play_.NoteOn(note);
    }

    void NoteOff(unsigned note) { free_play_.NoteOff(note); }
    void StopFreePlay() { free_play_.Clear(); }

    void SetWaveform(quiz_synth::Waveform waveform)
    {
        if(static_cast<unsigned>(waveform) >= quiz_synth::kWaveformCount)
            return;
        waveform_ = waveform;
        oscillator_.SetWaveform(waveform);
        free_play_.SetWaveform(waveform);
    }

    quiz_synth::Waveform Waveform() const { return waveform_; }

    float Process()
    {
        if(state_ == State::Idle)
            return volume_ * free_play_.Process();
        if(state_ == State::Listening)
            return 0.0f;

        float sample = 0.0f;
        if(state_ != State::Pause && state_ != State::LevelComplete)
        {
            const float elapsed = static_cast<float>(position_) / kSampleRate;
            float envelope = elapsed < 0.01f ? elapsed / 0.01f : 1.0f;
            if(release_length_ > 0 && position_ >= sustain_length_)
                envelope *= static_cast<float>(length_ - position_) / release_length_;
            float frequency = frequency_;
            if(state_ == State::Feedback)
            {
                const float fraction = static_cast<float>(position_) / length_;
                frequency *= correct_ ? (1.0f + fraction) : (1.0f - 0.5f * fraction);
            }
            sample = quiz_synth::FreePlay::kOutputGain * volume_ * envelope
                     * oscillator_.Process(frequency);
        }

        if(++position_ >= length_)
            Advance();
        return sample;
    }

    State GetState() const { return state_; }
    unsigned CurrentLevel() const { return static_cast<unsigned>(level_); }
    unsigned Target() const { return target_; }
    unsigned Attempts() const { return attempts_; }
    unsigned Completed() const { return completed_; }
    uint32_t Progress() const { return progress_; }
    Result GetResult(unsigned index) const
    {
        if(index >= kRoundLength)
            return Result::Pending;
        return static_cast<Result>((progress_ >> (2 * index)) & 3);
    }

  private:
        void ResetLevel()
        {
                progress_ = 0;
                completed_ = 0;
                correct_ = false;
                has_previous_target_ = false;
                NextNote();
        }

    static float Frequency(unsigned note)
    {
        return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
    }

    void Begin(State state,
               float sustain_seconds,
               float frequency = 0.0f,
               float release_seconds = 0.12f)
    {
        state_ = state;
        position_ = 0;
        sustain_length_ = static_cast<unsigned>(sustain_seconds * kSampleRate);
        release_length_ = static_cast<unsigned>(release_seconds * kSampleRate);
        length_ = sustain_length_ + release_length_;
        frequency_ = frequency;
        oscillator_.Reset();
    }

    void NextNote()
    {
        // Beginner targets are the natural notes C3 through C5.
        static constexpr unsigned notes[] = {
            48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72
        };
        random_ ^= random_ << 13;
        random_ ^= random_ >> 17;
        random_ ^= random_ << 5;
        const unsigned count = level_ == Level::Natural
                                   ? sizeof(notes) / sizeof(notes[0]) : 25;
        unsigned first = 0;
        unsigned last = count - 1;
        if(has_previous_target_)
        {
            first = target_index_ > range_ ? target_index_ - range_ : 0;
            last = target_index_ + range_ < count ? target_index_ + range_ : count - 1;
        }
        target_index_ = first + random_ % (last - first + 1);
        target_ = level_ == Level::Natural ? notes[target_index_] : 48 + target_index_;
        has_previous_target_ = true;
        attempts_ = 0;
        Begin(State::Prompt, 0.7f, Frequency(target_), kReleaseSeconds);
    }

    void Advance()
    {
        switch(state_)
        {
            case State::Prompt:
                state_ = State::Listening;
                break;
            case State::Answer:
                if(correct_ || attempts_ == kMaxAttempts)
                {
                    const Result result = correct_ ? Result::Correct : Result::Wrong;
                    progress_ |= static_cast<uint32_t>(result) << (2 * completed_);
                    ++completed_;
                }
                Begin(State::Feedback, 0.23f, correct_ ? 660.0f : 220.0f, 0.12f);
                break;
            case State::Feedback:
                if(completed_ == kRoundLength)
                    Begin(State::LevelComplete, 2.0f, 0.0f, 0.0f);
                else
                    Begin(State::Pause, 0.3f, 0.0f, 0.0f);
                break;
            case State::Pause:
                if(completed_ == kRoundLength)
                    state_ = State::Idle;
                else if(correct_ || attempts_ == kMaxAttempts)
                    NextNote();
                else
                    Begin(State::Prompt, 0.7f, Frequency(target_));
                break;
            case State::LevelComplete:
                state_ = State::Idle;
                break;
            default:
                break;
        }
    }

    State state_ = State::Idle;
    Level level_ = Level::Natural;
    uint32_t random_ = 1;
    uint32_t progress_ = 0;
    unsigned completed_ = 0;
    unsigned target_ = 60;
    unsigned target_index_ = 0;
    unsigned range_ = 5;
    bool has_previous_target_ = false;
    unsigned attempts_ = 0;
    unsigned position_ = 0;
    unsigned length_ = 0;
    unsigned sustain_length_ = 0;
    unsigned release_length_ = 0;
    float frequency_ = 0.0f;
    quiz_synth::Oscillator oscillator_;
    quiz_synth::FreePlay free_play_;
    quiz_synth::Waveform waveform_ = quiz_synth::Waveform::Triangle;
    float volume_ = 0.25f;
    bool correct_ = false;
};
