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
    static constexpr unsigned kLevelCount = 10;
    static constexpr unsigned kChordsLevelCount = 8;
    static constexpr float kReleaseSeconds = 0.25f;
    static constexpr unsigned kFeedbackFirstNoteSamples = 150 * kSampleRate / 1000;
    static constexpr unsigned kFeedbackClickReleaseSamples = 10 * kSampleRate / 1000;
    static constexpr unsigned kFeedbackSustainSamples = 450 * kSampleRate / 1000;
    static constexpr unsigned kFeedbackReleaseSamples = 100 * kSampleRate / 1000;
    static constexpr unsigned kSuccessSignalSamples
        = kFeedbackSustainSamples + kFeedbackReleaseSamples;
    static constexpr unsigned kPerfectReplayIntervalSamples = kSampleRate;
    static constexpr uint32_t kPerfectProgress = ((1u << (2 * kRoundLength)) - 1) / 3;

    enum class State { Idle, Prompt, Listening, Answer, Feedback, Pause, LevelComplete };
    enum class Result { Pending, Correct, Wrong };
    // PerfectPitch: toggle UP. Chords: toggle DOWN (intervals and triads).
    enum class Mode { PerfectPitch, Chords };

    void Start(uint32_t seed)
    {
        free_play_.Clear();
        pressed_chord_mask_ = 0;
        random_ = seed ? seed : 1;
        ResetLevel();
    }

    bool StartLevel(unsigned level, uint32_t seed)
    {
        if(level == 0 || level > CurrentLevelCount())
            return false;
        level_ = level;
        Start(seed);
        return true;
    }

    // Flipping the mode toggle exits the current round immediately.
    void SetMode(Mode mode)
    {
        if(mode == mode_)
            return;
        mode_ = mode;
        if(level_ > CurrentLevelCount())
            level_ = CurrentLevelCount();
        ExitToIdle();
    }

    Mode CurrentMode() const { return mode_; }

    unsigned CurrentLevelCount() const
    {
        return mode_ == Mode::Chords ? kChordsLevelCount : kLevelCount;
    }

    void Press(unsigned note)
    {
        if(state_ != State::Listening || note > 127)
            return;
        if(TargetVoiceCount() > 1)
        {
            if(note < 48 || note > 72)
                return;
            const uint32_t note_bit = 1u << (note - 48);
            if(pressed_chord_mask_ & note_bit)
                return;
            pressed_chord_mask_ |= note_bit;
            ++pressed_chord_count_;
            free_play_.NoteOn(note);
            if(pressed_chord_count_ < TargetVoiceCount())
                return;

            uint32_t target_mask = 0;
            for(unsigned voice = 0; voice < TargetVoiceCount(); ++voice)
                target_mask |= 1u << (target_notes_[voice] - 48);
            correct_ = pressed_chord_mask_ == target_mask;
            ++attempts_;
            Begin(State::Answer, 0.25f);
            for(unsigned offset = 0; offset <= 24; ++offset)
                if(pressed_chord_mask_ & (1u << offset))
                    free_play_.PlayNote(48 + offset,
                                        length_,
                                        static_cast<unsigned>(kReleaseSeconds * kSampleRate));
            pressed_chord_mask_ = 0;
            pressed_chord_count_ = 0;
            return;
        }

        correct_ = note == target_notes_[0];
        ++attempts_;
        Begin(State::Answer, 0.25f, note, kReleaseSeconds);
    }

    bool ReplayTarget()
    {
        if(state_ != State::Listening)
            return false;
        pressed_chord_mask_ = 0;
        pressed_chord_count_ = 0;
        BeginTarget(State::Prompt, 0.7f, kReleaseSeconds);
        return true;
    }

    void ExitToIdle()
    {
        free_play_.Clear();
        pressed_chord_mask_ = 0;
        pressed_chord_count_ = 0;
        state_ = State::Idle;
    }

    void SetVolume(float volume)
    {
        volume_ = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
    }

    float Volume() const { return volume_; }

    void NoteOn(unsigned note)
    {
        if(state_ == State::Idle)
            free_play_.NoteOn(note);
    }

    void NoteOff(unsigned note)
    {
        if(TargetVoiceCount() > 1 && state_ == State::Listening && note >= 48 && note <= 72)
        {
            const uint32_t note_bit = 1u << (note - 48);
            if(pressed_chord_mask_ & note_bit)
                --pressed_chord_count_;
            pressed_chord_mask_ &= ~(1u << (note - 48));
        }
        free_play_.NoteOff(note);
    }
    void StopFreePlay()
    {
        free_play_.ClearHeld();
        if(state_ == State::Listening)
        {
            pressed_chord_mask_ = 0;
            pressed_chord_count_ = 0;
        }
    }

    float Process()
    {
        if(state_ == State::Idle)
            return volume_ * free_play_.Process();
        if(state_ == State::Feedback)
        {
            if(position_ == 0)
                StartFeedbackNote(correct_, false);
            else if(position_ == kFeedbackFirstNoteSamples)
                StartFeedbackNote(correct_, true);
        }
        else if(state_ == State::LevelComplete && IsPerfectRound())
        {
            const unsigned replay_position = position_ % kPerfectReplayIntervalSamples;
            if(position_ < 2 * kPerfectReplayIntervalSamples)
            {
                if(replay_position == 0)
                    StartFeedbackNote(true, false);
                else if(replay_position == kFeedbackFirstNoteSamples)
                    StartFeedbackNote(true, true);
            }
        }

        const float sample = volume_ * free_play_.Process();
        if(state_ != State::Idle && state_ != State::Listening)
        {
            if(++position_ >= length_)
                Advance();
        }
        return sample;
    }

    State GetState() const { return state_; }
    unsigned CurrentLevel() const { return level_; }
    unsigned Target() const { return target_notes_[0]; }
    unsigned TargetVoiceCount() const
    {
        return mode_ == Mode::Chords ? ChordsVoiceCountFor(level_)
                                     : VoiceCountFor(level_);
    }
    unsigned TargetNote(unsigned voice) const
    {
        return voice < TargetVoiceCount() ? target_notes_[voice] : 0;
    }
    unsigned TargetRangeDifficulty() const
    {
        // Chords mode has no range constraint: roots jump freely.
        return mode_ == Mode::Chords ? 0 : RangeFor(level_);
    }
    bool UsesBlackKeys() const
    {
        // Chords-mode targets may include black keys at every level; only the
        // level-1 root is restricted to white keys.
        return mode_ == Mode::Chords ? true : HasBlackKeys(level_);
    }
    unsigned Attempts() const { return attempts_; }
    unsigned PendingWrongAttempts() const
    {
        return !correct_ && attempts_ < kMaxAttempts ? attempts_ : 0;
    }
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
                pressed_chord_mask_ = 0;
                pressed_chord_count_ = 0;
                has_previous_target_ = false;
                NextNote();
        }

    static float Frequency(unsigned note)
    {
        return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
    }

    void StartFeedbackNote(bool success, bool second)
    {
        const unsigned note = second ? (success ? 88 : 83) : (success ? 83 : 88);
        const unsigned sustain = second ? kFeedbackSustainSamples - kFeedbackFirstNoteSamples
                                        : kFeedbackFirstNoteSamples;
        const unsigned release = second && success ? kFeedbackReleaseSamples
                                                    : kFeedbackClickReleaseSamples;
        free_play_.PlayNote(note, sustain, release, true);
    }

    bool IsPerfectRound() const { return progress_ == kPerfectProgress; }

    static unsigned VoiceCountFor(unsigned level)
    {
        if(level <= 4)
            return 1;
        if(level <= 8)
            return 2;
        return 3;
    }

    static unsigned ChordsVoiceCountFor(unsigned level)
    {
        return level <= 5 ? 2 : 3;
    }

    static unsigned RangeFor(unsigned level)
    {
        if(level == 2 || level == 4 || level == 6 || level == 8 || level == 9)
            return 12;
        return level == 10 ? 14 : 7;
    }

    static bool HasBlackKeys(unsigned level)
    {
        return level == 3 || level == 4 || level == 7 || level == 8 || level == 10;
    }

    unsigned RandomBelow(unsigned limit)
    {
        random_ ^= random_ << 13;
        random_ ^= random_ >> 17;
        random_ ^= random_ << 5;
        return random_ % limit;
    }

    static unsigned WhiteNote(unsigned index)
    {
        static constexpr unsigned white_notes[] = {
            48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72
        };
        return white_notes[index];
    }

    unsigned NoteFromIndex(unsigned index) const
    {
        if(HasBlackKeys(level_))
            return 48 + index;
        return WhiteNote(index);
    }

    void Begin(State state,
               float sustain_seconds,
               unsigned note = 128,
               float release_seconds = 0.0f)
    {
        state_ = state;
        position_ = 0;
        sustain_length_ = static_cast<unsigned>(sustain_seconds * kSampleRate);
        length_ = sustain_length_;
        if(note <= 127)
            free_play_.PlayNote(note,
                                sustain_length_,
                                static_cast<unsigned>(release_seconds * kSampleRate));
    }

    void BeginTarget(State state, float sustain_seconds, float release_seconds)
    {
        Begin(state, sustain_seconds, target_notes_[0], release_seconds);
        for(unsigned voice = 1; voice < TargetVoiceCount(); ++voice)
            free_play_.PlayNote(target_notes_[voice],
                                static_cast<unsigned>(sustain_seconds * kSampleRate),
                                static_cast<unsigned>(release_seconds * kSampleRate));
    }

    void NextNote()
    {
        if(mode_ == Mode::Chords)
            NextChordTarget();
        else
            NextPitchTarget();
        has_previous_target_ = true;
        attempts_ = 0;
        pressed_chord_mask_ = 0;
        pressed_chord_count_ = 0;
        BeginTarget(State::Prompt, 0.7f, kReleaseSeconds);
    }

    void NextPitchTarget()
    {
        const unsigned count = HasBlackKeys(level_) ? 25 : 15;
        const unsigned range = RangeFor(level_);
        unsigned first = 0;
        unsigned last = count - 1;
        if(has_previous_target_)
        {
            first = target_index_ > range ? target_index_ - range : 0;
            last = target_index_ + range < count ? target_index_ + range : count - 1;
        }
        target_index_ = first + RandomBelow(last - first + 1);
        target_notes_[0] = NoteFromIndex(target_index_);
        for(unsigned voice = 1; voice < TargetVoiceCount(); ++voice)
        {
            const unsigned chord_first = target_index_ > range ? target_index_ - range : 0;
            const unsigned chord_last = target_index_ + range < count
                                            ? target_index_ + range : count - 1;
            unsigned index = 0;
            bool already_used = false;
            do
            {
                index = chord_first + RandomBelow(chord_last - chord_first + 1);
                already_used = false;
                for(unsigned previous = 0; previous < voice; ++previous)
                    already_used |= target_notes_[previous] == NoteFromIndex(index);
            }
            while(already_used);
            target_notes_[voice] = NoteFromIndex(index);
        }
    }

    // Chords mode (levels.md): a random root plus fixed intervals above it.
    // Levels 1-2 perfect fifths, 3 major thirds, 4 minor thirds, 5 both,
    // 6 major triads, 7 minor triads, 8 both. Level 1 uses white-key roots.
    void NextChordTarget()
    {
        unsigned second = 7;
        if(level_ == 3 || level_ == 6)
            second = 4;
        else if(level_ == 4 || level_ == 7)
            second = 3;
        else if(level_ == 5 || level_ == 8)
            second = RandomBelow(2) == 0 ? 4 : 3;
        const bool three_voices = level_ > 5;
        const unsigned top = three_voices ? 7 : second;
        const unsigned root_max = 72 - top;
        unsigned root = 48;
        if(level_ == 1)
        {
            unsigned white_count = 0;
            while(white_count < 15 && WhiteNote(white_count) <= root_max)
                ++white_count;
            root = WhiteNote(RandomBelow(white_count));
        }
        else
        {
            root = 48 + RandomBelow(root_max - 48 + 1);
        }
        target_notes_[0] = root;
        target_notes_[1] = root + second;
        if(three_voices)
            target_notes_[2] = root + 7;
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
                Begin(State::Feedback, 0.45f);
                break;
            case State::Feedback:
                if(completed_ == kRoundLength)
                {
                    Begin(State::LevelComplete, 2.0f);
                }
                else
                {
                    Begin(State::Pause, 0.3f);
                }
                break;
            case State::Pause:
                if(completed_ == kRoundLength)
                    state_ = State::Idle;
                else if(correct_ || attempts_ == kMaxAttempts)
                    NextNote();
                else
                    BeginTarget(State::Prompt, 0.7f, kReleaseSeconds);
                break;
            case State::LevelComplete:
                state_ = State::Idle;
                break;
            default:
                break;
        }
    }

    State state_ = State::Idle;
    Mode mode_ = Mode::PerfectPitch;
    unsigned level_ = 1;
    uint32_t random_ = 1;
    uint32_t progress_ = 0;
    unsigned completed_ = 0;
    unsigned target_notes_[3] = {60, 60, 60};
    unsigned target_index_ = 0;
    uint32_t pressed_chord_mask_ = 0;
    unsigned pressed_chord_count_ = 0;
    bool has_previous_target_ = false;
    unsigned attempts_ = 0;
    unsigned position_ = 0;
    unsigned length_ = 0;
    unsigned sustain_length_ = 0;
    quiz_synth::FreePlay free_play_;
    float volume_ = 0.25f;
    bool correct_ = false;
};
