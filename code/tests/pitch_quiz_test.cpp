#include "../src/pitch_quiz.h"
#include "../src/quiz_controls.h"
#include "../src/quiz_display.h"

#include <cassert>
#include <iostream>

void FinishSounds(PitchQuiz& quiz)
{
    for(unsigned i = 0; i < 5 * PitchQuiz::kSampleRate; ++i)
    {
        const float sample = quiz.Process();
        assert(std::isfinite(sample));
        assert(std::fabs(sample) <= 0.22f);
        if(quiz.GetState() == PitchQuiz::State::Listening
              || quiz.GetState() == PitchQuiz::State::Idle
              || quiz.GetState() == PitchQuiz::State::LevelComplete)
            return;
    }
    assert(false);
}

bool IsNaturalNote(unsigned note)
{
    switch(note % 12)
    {
        case 0:
        case 2:
        case 4:
        case 5:
        case 7:
        case 9:
        case 11:
            return true;
        default:
            return false;
    }
}

void VerifyLevelCompletion(PitchQuiz& quiz)
{
    const unsigned selected_level = quiz.CurrentLevel();
    assert(quiz.GetState() == PitchQuiz::State::LevelComplete);
    const uint32_t results = quiz.Progress();
    const unsigned selected_range = quiz.Range();
    assert(quiz_controls::PackUi(quiz, false) & quiz_controls::kResultHoldMask);
    for(unsigned sample = 0; sample < 2 * PitchQuiz::kSampleRate - 1; ++sample)
        assert(quiz.Process() == 0.0f);
    assert(quiz.GetState() == PitchQuiz::State::LevelComplete);
    assert(quiz.Progress() == results);
    quiz.Press(quiz.Target());
    assert(quiz.Progress() == results);
    assert(quiz.Process() == 0.0f);
    assert(quiz.CurrentLevel() == selected_level);
    assert(quiz.Range() == selected_range);
    assert(quiz.GetState() == PitchQuiz::State::Idle);
    assert(quiz.Completed() == PitchQuiz::kRoundLength);
    assert((quiz_controls::PackUi(quiz, false) & quiz_controls::kResultHoldMask) == 0);
    assert((quiz_controls::PackUi(quiz, false) & quiz_controls::kActiveMask) == 0);
    assert(quiz.Progress() == results);
    assert(quiz.Volume() == 0.25f);

    quiz.NoteOn(60);
    float energy = 0.0f;
    for(unsigned sample = 0; sample < 4800; ++sample)
        energy += std::fabs(quiz.Process());
    assert(energy > 1.0f);
    assert(quiz.Progress() == results);
    quiz.NoteOff(60);
    for(unsigned sample = 0; sample < quiz_synth::FreePlay::kReleaseSamples; ++sample)
        quiz.Process();
    assert(quiz.Process() == 0.0f);
}

void CheckControlRouting()
{
    PitchQuiz quiz;
    bool rising[40] = {};
    rising[quiz_controls::kBlackKeySwitches[1]] = true;
    quiz_controls::Process(quiz, rising, true, 123);
    assert(quiz.CurrentLevel() == 2);
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
    assert(quiz.Attempts() == 0);
    rising[quiz_controls::kBlackKeySwitches[1]] = false;
    FinishSounds(quiz);
    quiz_controls::Process(quiz, rising, true, 456);
    quiz_controls::Process(quiz, rising, false, 456);
    assert(quiz.GetState() == PitchQuiz::State::Listening);
    assert(quiz.Attempts() == 0);

    for(unsigned black = 2; black < 10; ++black)
    {
        rising[quiz_controls::kBlackKeySwitches[black]] = true;
        quiz_controls::Process(quiz, rising, true, 456);
        rising[quiz_controls::kBlackKeySwitches[black]] = false;
        assert(quiz.CurrentLevel() == 2);
        assert(quiz.GetState() == PitchQuiz::State::Listening);
        assert(quiz.Attempts() == 0);
    }
    rising[15] = true;
    quiz_controls::Process(quiz, rising, true, 456);
    assert(quiz.Attempts() == 0);
    quiz_controls::Process(quiz, rising, false, 456);
    assert(quiz.Attempts() == 1);
    rising[15] = false;

    for(unsigned button : {quiz_controls::kPlaySwitch, quiz_controls::kLoopSwitch})
    {
        rising[button] = true;
        quiz_controls::Process(quiz, rising, false, 456);
        rising[button] = false;
        assert(quiz.CurrentLevel() == 2);
        assert(quiz.Completed() == 0 && quiz.Attempts() == 1);
        assert(quiz.GetState() == PitchQuiz::State::Answer);
    }
    FinishSounds(quiz);
    const unsigned replay_target = quiz.Target();
    rising[quiz_controls::kLoopSwitch] = true;
    quiz_controls::Process(quiz, rising, false, 456);
    rising[quiz_controls::kLoopSwitch] = false;
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
    assert(quiz.Attempts() == 1 && quiz.Target() == replay_target);
    rising[7] = true;
    rising[12] = true;
    rising[quiz_controls::kPlaySwitch] = true;
    quiz_controls::Process(quiz, rising, true, 789);
    assert(quiz.CurrentLevel() == 1);
    PitchQuiz expected;
    expected.Start(789);
    assert(quiz.Target() == expected.Target());
    const uint32_t ui = quiz_controls::PackUi(quiz, true);
    assert((ui & quiz_controls::kProgressMask) == 0);
    assert(((ui >> quiz_controls::kLevelShift) & 15) == 1);
    assert(ui & quiz_controls::kShiftHeldMask);
    assert(ui & quiz_controls::kActiveMask);
}

void ReachState(PitchQuiz& quiz, PitchQuiz::State state)
{
    for(unsigned sample = 0; sample < 5 * PitchQuiz::kSampleRate; ++sample)
    {
        if(quiz.GetState() == state)
            return;
        quiz.Process();
    }
    assert(false);
}

void CheckLevelRestarts()
{
    for(PitchQuiz::State state : {
            PitchQuiz::State::Idle, PitchQuiz::State::Prompt,
            PitchQuiz::State::Listening, PitchQuiz::State::Answer,
            PitchQuiz::State::Feedback, PitchQuiz::State::Pause,
            PitchQuiz::State::LevelComplete})
    {
        PitchQuiz quiz;
        if(state != PitchQuiz::State::Idle)
            quiz.Start(123);
        if(state == PitchQuiz::State::LevelComplete)
        {
            FinishSounds(quiz);
            for(unsigned question = 0; question < PitchQuiz::kRoundLength; ++question)
            {
                quiz.Press(quiz.Target());
                FinishSounds(quiz);
            }
        }
        else if(state != PitchQuiz::State::Idle && state != PitchQuiz::State::Prompt)
        {
            FinishSounds(quiz);
            if(state != PitchQuiz::State::Listening)
            {
                quiz.Press(quiz.Target());
                ReachState(quiz, state);
            }
        }
        assert(quiz.GetState() == state);
        const uint32_t results = quiz.Progress();
        const unsigned target = quiz.Target();
        assert(!quiz.StartLevel(10, 999));
        assert(quiz.GetState() == state);
        assert(quiz.Target() == target && quiz.Progress() == results);

        quiz.SetVolume(0.4f);
        bool rising[40] = {};
        rising[quiz_controls::kBlackKeySwitches[1]] = true;
        quiz_controls::Process(quiz, rising, true, 456);
        assert(quiz.CurrentLevel() == 2);
        assert(quiz.Completed() == 0 && quiz.Attempts() == 0 && quiz.Progress() == 0);
        assert(quiz.Volume() == 0.4f);
        assert(quiz.GetState() == PitchQuiz::State::Prompt);
        FinishSounds(quiz);
        for(unsigned sample = 0; sample < 3 * PitchQuiz::kSampleRate; ++sample)
            assert(quiz.Process() == 0.0f);
        assert(quiz.GetState() == PitchQuiz::State::Listening);
        assert(quiz.CurrentLevel() == 2);

        quiz_controls::Process(quiz, rising, true, 789);
        assert(quiz.CurrentLevel() == 2);
        assert(quiz.GetState() == PitchQuiz::State::Prompt);
        rising[quiz_controls::kBlackKeySwitches[1]] = false;
        rising[quiz_controls::kBlackKeySwitches[0]] = true;
        quiz_controls::Process(quiz, rising, true, 789);
        assert(quiz.CurrentLevel() == 1);
        assert(quiz.GetState() == PitchQuiz::State::Prompt);
    }
}

void CheckResultAnimations()
{
    constexpr uint32_t perfect = ((1u << 20) - 1) / 3;
    const uint32_t nine_correct = (perfect & ~3u) | 2u;
    for(unsigned score = 0; score < 10; ++score)
    {
        uint32_t results = 0;
        for(unsigned question = 0; question < 10; ++question)
            results |= (question < score ? 1u : 2u) << (2 * question);
        for(unsigned led = 0; led < 10; ++led)
        {
            const auto on = quiz_display::ResultColor(results, led, true, 500);
            assert(on.red == (led < score ? 0 : 32));
            assert(on.green == (led < score ? 32 : 0));
            assert(on.blue == 0);
            const auto off = quiz_display::ResultColor(results, led, true, 1750);
            assert(off.red == 0 && off.green == 0 && off.blue == 0);
        }
    }
    for(unsigned led = 0; led < 10; ++led)
    {
        const auto on = quiz_display::ResultColor(nine_correct, led, true, 0);
        assert(on.red == (led == 0 ? 32 : 0));
        assert(on.green == (led == 0 ? 0 : 32));
        assert(on.blue == 0);
        const auto off = quiz_display::ResultColor(nine_correct, led, true, 250);
        assert(off.red == 0 && off.green == 0 && off.blue == 0);
        const auto steady = quiz_display::ResultColor(perfect, led, false, 250);
        assert(steady.red == 0 && steady.green == 32 && steady.blue == 0);
    }
    bool has_red = false;
    bool has_green = false;
    bool has_blue = false;
    for(unsigned elapsed = 0; elapsed < 2000; elapsed += 33)
        for(unsigned led = 0; led < 10; ++led)
        {
            const auto color = quiz_display::ResultColor(perfect, led, true, elapsed);
            assert(color.red <= 32 && color.green <= 32 && color.blue <= 32);
            assert(color.red || color.green || color.blue);
            has_red |= color.red > 0;
            has_green |= color.green > 0;
            has_blue |= color.blue > 0;
        }
    assert(has_red && has_green && has_blue);
    const auto start = quiz_display::ResultColor(perfect, 0, true, 0);
    const auto later = quiz_display::ResultColor(perfect, 0, true, 500);
    assert(start.red != later.red || start.green != later.green || start.blue != later.blue);
    const auto white_key = quiz_display::ResultColor(perfect, 10, true, 500);
    assert(white_key.red == 0 && white_key.green == 0 && white_key.blue == 0);
}

void CheckWaveformLowPass()
{
    const uint8_t raw_waveforms[] = {
        daisysp::Oscillator::WAVE_TRI,
        daisysp::Oscillator::WAVE_POLYBLEP_SAW,
        daisysp::Oscillator::WAVE_POLYBLEP_SQUARE
    };
    for(unsigned waveform = 0; waveform < quiz_synth::kWaveformCount; ++waveform)
    {
        for(float frequency : {130.8128f, 440.0f, 523.2511f})
        {
            quiz_synth::Oscillator filtered;
            filtered.SetWaveform(static_cast<quiz_synth::Waveform>(waveform));
            daisysp::Oscillator raw{};
            raw.Init(48000.0f);
            raw.SetAmp(1.0f);
            raw.SetFreq(frequency);
            raw.SetWaveform(raw_waveforms[waveform]);
            float raw_energy = 0.0f;
            float filtered_energy = 0.0f;
            float raw_difference = 0.0f;
            float filtered_difference = 0.0f;
            float previous_raw = 0.0f;
            float previous_filtered = 0.0f;
            for(unsigned sample = 0; sample < 24000; ++sample)
            {
                const float original = raw.Process();
                const float output = filtered.Process(frequency);
                assert(std::isfinite(output) && std::fabs(output) <= 1.00001f);
                if(waveform == 0)
                    assert(output == original);
                if(sample > 4096)
                {
                    raw_energy += original * original;
                    filtered_energy += output * output;
                    raw_difference += (original - previous_raw) * (original - previous_raw);
                    filtered_difference += (output - previous_filtered) * (output - previous_filtered);
                }
                previous_raw = original;
                previous_filtered = output;
            }
            if(waveform != 0)
            {
                assert(filtered_difference < raw_difference * 0.5f);
                assert(filtered_energy > raw_energy * 0.4f);
                assert(filtered_energy < raw_energy);
            }
            filtered.Reset();
            const float restarted = filtered.Process(frequency);
            quiz_synth::Oscillator fresh;
            fresh.SetWaveform(static_cast<quiz_synth::Waveform>(waveform));
            assert(restarted == fresh.Process(frequency));
        }
    }
}

void CheckWaveforms()
{
    float energies[3] = {};
    for(unsigned index = 0; index < quiz_synth::kWaveformCount; ++index)
    {
        PitchQuiz quiz;
        const auto waveform = static_cast<quiz_synth::Waveform>(index);
        quiz.SetWaveform(waveform);
        quiz.Start(123);
        assert(quiz.Waveform() == waveform);
        for(unsigned sample = 0; sample < PitchQuiz::kSampleRate / 2; ++sample)
        {
            const float output = quiz.Process();
            assert(std::isfinite(output));
            assert(std::fabs(output) <= 0.22f);
            energies[index] += output * output;
        }
        assert(energies[index] > 1.0f);
        quiz.SetWaveform(static_cast<quiz_synth::Waveform>(99));
        assert(quiz.Waveform() == waveform);
    }
    assert(std::fabs(energies[2] - energies[0]) > 0.5f);
}

void CheckGameModeVolume()
{
    PitchQuiz quiz;
    quiz.SetVolume(1.0f);
    quiz.Start(123);
    float energy = 0.0f;
    for(unsigned sample = 0; sample < PitchQuiz::kSampleRate; ++sample)
    {
        const float output = quiz.Process();
        assert(std::fabs(output) <= quiz_synth::FreePlay::kOutputGain);
        energy += output * output;
    }
    assert(energy > 1.0f);
}

void CheckFreePlay()
{
    for(unsigned waveform = 0; waveform < quiz_synth::kWaveformCount; ++waveform)
    {
        quiz_synth::FreePlay retrigger;
        retrigger.SetWaveform(static_cast<quiz_synth::Waveform>(waveform));
        retrigger.NoteOn(60);
        float previous = 0.0f;
        for(unsigned sample = 0; sample < 3000; ++sample)
            previous = retrigger.Process();
        retrigger.NoteOff(60);
        retrigger.NoteOn(60);
        assert(std::fabs(retrigger.Process() - previous) < 0.0001f);

        quiz_synth::FreePlay stolen;
        stolen.SetWaveform(static_cast<quiz_synth::Waveform>(waveform));
        stolen.NoteOn(60);
        for(unsigned note = 61; note < 66; ++note)
            stolen.NoteOn(note);
        for(unsigned sample = 0; sample < 3000; ++sample)
            previous = stolen.Process();
        stolen.NoteOn(67);
        assert(std::fabs(stolen.Process() - previous) < 0.004f);
    }

    for(unsigned waveform = 0; waveform < quiz_synth::kWaveformCount; ++waveform)
    {
        quiz_synth::FreePlay synth;
        synth.SetWaveform(static_cast<quiz_synth::Waveform>(waveform));
        synth.NoteOn(128);
        assert(synth.ActiveVoices() == 0);
        for(unsigned note = 60; note < 66; ++note)
            synth.NoteOn(note);
        assert(synth.ActiveVoices() == 6);
        for(unsigned sample = 0; sample < 48000; ++sample)
        {
            const float output = synth.Process();
            assert(std::isfinite(output) && std::fabs(output) <= 0.22001f);
        }
        synth.NoteOn(66);
        assert(synth.ActiveVoices() == 6);
        assert(!synth.IsHeld(60) && synth.IsHeld(66));
        synth.NoteOff(60);
        assert(synth.IsHeld(66));
        for(unsigned note = 61; note <= 66; ++note)
            synth.NoteOff(note);
        for(unsigned sample = 0; sample < quiz_synth::FreePlay::kReleaseSamples - 1; ++sample)
            synth.Process();
        assert(synth.ActiveVoices() == 6);
        synth.Process();
        assert(synth.ActiveVoices() == 0);
        assert(synth.Process() == 0.0f);
    }

    PitchQuiz quiz;
    quiz.NoteOn(60);
    float energy = 0.0f;
    for(unsigned sample = 0; sample < 4800; ++sample)
        energy += std::fabs(quiz.Process());
    assert(energy > 1.0f);
    assert(quiz.GetState() == PitchQuiz::State::Idle && quiz.Attempts() == 0);
    quiz.SetWaveform(quiz_synth::Waveform::Saw);
    quiz.Start(123);
    assert(quiz.Waveform() == quiz_synth::Waveform::Saw);
    FinishSounds(quiz);
    assert(quiz.Process() == 0.0f);
    quiz.NoteOn(60);
    assert(quiz.Process() == 0.0f);
}

void CheckKeyboardAndWaveformControls()
{
    quiz_controls::ShiftEncoder encoder;
    for(unsigned sample = 0; sample < 4; ++sample)
        encoder.Update(false, false);
    assert(encoder.Update(true, false) == 0);
    assert(encoder.Update(false, false) == 0);
    assert(encoder.Update(false, false) == 1);
    assert(encoder.Update(false, true) == 0);
    assert(encoder.Update(false, false) == 0);
    assert(encoder.Update(false, false) == -1);
    for(unsigned waveform = 0; waveform < 3; ++waveform)
        for(unsigned white = 0; white < 15; ++white)
        {
            const auto color = quiz_display::WaveformColor(white, waveform);
            const unsigned expected = white / 5 == waveform ? 48 : 0;
            assert(color.red == expected && color.green == expected && color.blue == expected);
        }

    PitchQuiz quiz;
    bool rising[40] = {};
    bool falling[40] = {};
    rising[15] = true;
    quiz_controls::Process(quiz, rising, false, 1, falling);
    rising[15] = false;
    float energy = 0.0f;
    for(unsigned sample = 0; sample < 4800; ++sample)
        energy += std::fabs(quiz.Process());
    assert(energy > 1.0f);
    falling[15] = true;
    quiz_controls::Process(quiz, rising, false, 2, falling);
    falling[15] = false;
    for(unsigned sample = 0; sample < quiz_synth::FreePlay::kReleaseSamples; ++sample)
        quiz.Process();
    assert(quiz.Process() == 0.0f);

    rising[15] = true;
    quiz_controls::Process(quiz, rising, false, 3, falling);
    rising[15] = false;
    quiz_controls::Process(quiz, rising, true, 4, falling);
    assert(quiz.Process() == 0.0f);
    quiz_controls::Process(quiz, rising, false, 5, falling);
    assert(quiz.Process() == 0.0f);

    quiz_controls::TurnWaveform(quiz, -1);
    assert(quiz.Waveform() == quiz_synth::Waveform::Triangle);
    quiz_controls::TurnWaveform(quiz, 1);
    assert(quiz.Waveform() == quiz_synth::Waveform::Saw);
    quiz_controls::TurnWaveform(quiz, 1);
    quiz_controls::TurnWaveform(quiz, 1);
    assert(quiz.Waveform() == quiz_synth::Waveform::Square);
    const auto ui = quiz_controls::PackUi(quiz, false);
    assert(((ui >> quiz_controls::kWaveformShift) & 3) == 2);
    assert((ui & quiz_controls::kActiveMask) == 0);
    rising[quiz_controls::kPlaySwitch] = true;
    quiz_controls::Process(quiz, rising, false, 6, falling);
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
    assert(quiz.Waveform() == quiz_synth::Waveform::Square);
    quiz_controls::TurnWaveform(quiz, -1);
    assert(quiz.Waveform() == quiz_synth::Waveform::Saw);
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
}

unsigned TargetIndex(unsigned note, unsigned level)
{
    if(level == 2)
        return note - 48;
    unsigned index = 0;
    for(unsigned candidate = 48; candidate < note; ++candidate)
        if(IsNaturalNote(candidate))
            ++index;
    return index;
}

void CheckTargetRange()
{
    for(unsigned range = 1; range <= 15; ++range)
        for(unsigned white = 0; white < 15; ++white)
        {
            const auto color = quiz_display::RangeColor(white, range);
            assert(color.blue == 0);
            assert(color.red <= 48 && color.green <= 48);
            assert((color.red != 0 || color.green != 0) == (white < range));
        }
    const auto easy = quiz_display::RangeColor(0, 15);
    const auto middle = quiz_display::RangeColor(7, 15);
    const auto hard = quiz_display::RangeColor(14, 15);
    assert(easy.red == 0 && easy.green == 48);
    assert(middle.red == 48 && middle.green == 24);
    assert(hard.red == 48 && hard.green == 0);

    PitchQuiz bounds;
    assert(bounds.Range() == 5);
    quiz_controls::TurnRange(bounds, 1);
    assert(bounds.Range() == 6);
    quiz_controls::TurnRange(bounds, -1);
    assert(bounds.Range() == 5);
    bounds.SetRange(0);
    assert(bounds.Range() == 1);
    quiz_controls::TurnRange(bounds, -1);
    assert(bounds.Range() == 1);
    bounds.SetRange(99);
    assert(bounds.Range() == 15);
    quiz_controls::TurnRange(bounds, 1);
    assert(bounds.Range() == 15);

    for(unsigned level : {1u, 2u})
        for(unsigned range : {1u, 5u, 8u, 15u})
            for(uint32_t seed = 1; seed <= 8; ++seed)
            {
                PitchQuiz quiz;
                quiz.SetRange(range);
                assert(quiz.StartLevel(level, seed));
                FinishSounds(quiz);
                for(unsigned question = 0; question < PitchQuiz::kRoundLength - 1; ++question)
                {
                    const unsigned previous = TargetIndex(quiz.Target(), level);
                    quiz.Press(quiz.Target());
                    FinishSounds(quiz);
                    assert(quiz.Target() >= 48 && quiz.Target() <= 72);
                    if(level == 1)
                        assert(IsNaturalNote(quiz.Target()));
                    const unsigned next = TargetIndex(quiz.Target(), level);
                    assert((next > previous ? next - previous : previous - next) <= range);
                }
                assert(quiz.Range() == range);
                quiz.Start(123);
                assert(quiz.Range() == range);
            }

    bounds.SetRange(1);
    bounds.Start(123);
    FinishSounds(bounds);
    const unsigned current = bounds.Target();
    bounds.SetRange(8);
    assert(bounds.Target() == current);
    assert(bounds.StartLevel(2, 123));
    assert(bounds.Range() == 8);
    assert(bounds.StartLevel(1, 123));
    FinishSounds(bounds);
    for(unsigned question = 0; question < PitchQuiz::kRoundLength; ++question)
    {
        bounds.Press(bounds.Target());
        FinishSounds(bounds);
    }
    VerifyLevelCompletion(bounds);
    assert(bounds.Range() == 8);
}

void CheckEveryLevelCompletion()
{
    for(unsigned level = 1; level <= PitchQuiz::kLevelCount; ++level)
        for(unsigned score : {0u, 9u, 10u})
        {
            PitchQuiz quiz;
            assert(quiz.StartLevel(level, 123));
            FinishSounds(quiz);
            for(unsigned question = 0; question < PitchQuiz::kRoundLength; ++question)
            {
                if(question < score)
                    quiz.Press(quiz.Target());
                else
                    for(unsigned attempt = 0; attempt < PitchQuiz::kMaxAttempts; ++attempt)
                    {
                        quiz.Press(quiz.Target() == 48 ? 49 : 48);
                        if(attempt + 1 < PitchQuiz::kMaxAttempts)
                            FinishSounds(quiz);
                    }
                FinishSounds(quiz);
            }
            assert(quiz.GetState() == PitchQuiz::State::LevelComplete);
            const auto ui = quiz_controls::PackUi(quiz, false);
            assert(ui & quiz_controls::kResultHoldMask);
            assert(ui & quiz_controls::kActiveMask);
            bool rainbow_has_blue = false;
            for(unsigned led = 0; led < 10; ++led)
            {
                const auto color = quiz_display::ResultColor(quiz.Progress(), led, true, 250);
                if(score == 10)
                {
                    assert(color.red || color.green || color.blue);
                    rainbow_has_blue |= color.blue != 0;
                }
                else
                    assert(color.red == 0 && color.green == 0 && color.blue == 0);
            }
            assert(rainbow_has_blue == (score == 10));
            VerifyLevelCompletion(quiz);
        }
}

void CheckReplayAndExit()
{
    PitchQuiz quiz;
    assert(!quiz.ReplayTarget());
    assert(quiz.StartLevel(2, 123));
    assert(!quiz.ReplayTarget());
    FinishSounds(quiz);
    const unsigned target = quiz.Target();
    assert(quiz.ReplayTarget());
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
    assert(quiz.Target() == target && quiz.Attempts() == 0 && quiz.Completed() == 0);
    assert(!quiz.ReplayTarget());
    FinishSounds(quiz);
    quiz.Press(target == 48 ? 49 : 48);
    assert(quiz.Attempts() == 1);
    assert(!quiz.ReplayTarget());
    ReachState(quiz, PitchQuiz::State::Feedback);
    assert(!quiz.ReplayTarget());
    ReachState(quiz, PitchQuiz::State::Pause);
    assert(!quiz.ReplayTarget());
    FinishSounds(quiz);
    assert(quiz.ReplayTarget());
    assert(quiz.Attempts() == 1 && quiz.Target() == target);
    quiz.SetVolume(0.4f);
    quiz.SetRange(8);
    quiz.SetWaveform(quiz_synth::Waveform::Saw);
    quiz.ExitToIdle();
    assert(quiz.GetState() == PitchQuiz::State::Idle);
    assert(quiz.CurrentLevel() == 2 && quiz.Attempts() == 1);
    assert(quiz.Volume() == 0.4f && quiz.Range() == 8);
    assert(quiz.Waveform() == quiz_synth::Waveform::Saw);
    assert(quiz.Process() == 0.0f);
    assert(!quiz.ReplayTarget());
    quiz.NoteOn(60);
    float energy = 0.0f;
    for(unsigned sample = 0; sample < 4800; ++sample)
        energy += std::fabs(quiz.Process());
    assert(energy > 1.0f);
}

void CheckTransportGestures()
{
    for(unsigned filled = 0; filled <= 15; ++filled)
        for(unsigned white = 0; white < 15; ++white)
        {
            const auto color = quiz_display::ExitHoldColor(white, filled);
            assert(color.red == 0 && color.green == 0);
            assert(color.blue == (white < filled ? 48 : 0));
        }
    quiz_display::TransportView view;
    view.shutdown = quiz_display::Shutdown::Fade;
    view.fade = 24;
    auto color = quiz_display::ShutdownColor(view);
    assert(color.red == 24 && color.green == 48 && color.blue == 24);
    color = quiz_display::ShutdownColor(view, true);
    assert(color.red == 0 && color.green == 0 && color.blue == 0);
    view.shutdown = quiz_display::Shutdown::White;
    color = quiz_display::ShutdownColor(view);
    assert(color.red == 48 && color.green == 48 && color.blue == 48);
    view.shutdown = quiz_display::Shutdown::Dark;
    color = quiz_display::ShutdownColor(view);
    assert(color.red == 0 && color.green == 0 && color.blue == 0);

    PitchQuiz quiz;
    quiz_controls::Transport transport;
    bool rising[40] = {};
    bool held[40] = {};
    const unsigned play = quiz_controls::kPlaySwitch;
    const unsigned loop = quiz_controls::kLoopSwitch;
    const unsigned shift = quiz_controls::kShiftSwitch;
    rising[loop] = true;
    quiz_controls::Process(quiz, rising, false, 1);
    assert(quiz.GetState() == PitchQuiz::State::Idle);
    rising[loop] = false;

    rising[play] = held[play] = true;
    assert(!transport.Update(quiz, rising, held, 100));
    quiz_controls::Process(quiz, rising, false, 100);
    rising[play] = false;
    assert(!transport.Update(quiz, rising, held, 2500));
    assert(!transport.ExitHolding());
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
    held[play] = false;
    transport.Update(quiz, rising, held, 2501);
    rising[play] = held[play] = true;
    transport.Update(quiz, rising, held, 3000);
    rising[play] = false;
    assert(transport.ExitHolding());
    assert(transport.ExitKeys(4000) == 7);
    assert((transport.PackUi(4000) & 15) == 7);
    assert(transport.PackUi(4000) & (1u << 4));
    transport.Update(quiz, rising, held, 4999);
    assert(quiz.GetState() != PitchQuiz::State::Idle);
    assert(transport.Update(quiz, rising, held, 5000));
    assert(quiz.GetState() == PitchQuiz::State::Idle);
    assert(!transport.ExitHolding());

    held[play] = false;
    transport.Update(quiz, rising, held, 5001);
    quiz.Start(1);
    rising[play] = held[play] = true;
    transport.Update(quiz, rising, held, 6000);
    rising[play] = false;
    held[play] = false;
    transport.Update(quiz, rising, held, 6500);
    transport.Update(quiz, rising, held, 9000);
    assert(!transport.ExitHolding());
    assert(quiz.GetState() != PitchQuiz::State::Idle);

    held[shift] = rising[shift] = true;
    held[loop] = rising[loop] = true;
    assert(transport.Update(quiz, rising, held, 10000));
    assert(transport.RebootState() == quiz_controls::Transport::Reboot::Confirming);
    assert(transport.PackUi(10000) & (1u << 5));
    rising[shift] = rising[loop] = false;
    transport.Update(quiz, rising, held, 11000);
    assert(transport.RebootState() == quiz_controls::Transport::Reboot::Confirming);
    held[loop] = false;
    transport.Update(quiz, rising, held, 11001);
    rising[loop] = held[loop] = true;
    transport.Update(quiz, rising, held, 11002);
    assert(transport.RebootState() == quiz_controls::Transport::Reboot::None);

    rising[loop] = false;
    held[loop] = false;
    transport.Update(quiz, rising, held, 11003);
    rising[loop] = held[loop] = true;
    transport.Update(quiz, rising, held, 12000);
    rising[loop] = false;
    held[shift] = false;
    transport.Update(quiz, rising, held, 12001);
    rising[shift] = held[shift] = true;
    assert(transport.Update(quiz, rising, held, 12002));
    assert(transport.RebootState() == quiz_controls::Transport::Reboot::Requested);
    assert(transport.PackUi(12002) & (1u << 6));
    assert(quiz.GetState() == PitchQuiz::State::Idle);

    quiz_controls::Transport wrapped;
    bool wrap_rising[40] = {};
    bool wrap_held[40] = {};
    quiz.Start(123);
    wrap_rising[play] = wrap_held[play] = true;
    const uint32_t start = 0xffffff00u;
    wrapped.Update(quiz, wrap_rising, wrap_held, start);
    wrap_rising[play] = false;
    assert(!wrapped.Update(quiz, wrap_rising, wrap_held, start + 1999u));
    assert(wrapped.Update(quiz, wrap_rising, wrap_held, start + 2000u));
    assert(quiz.GetState() == PitchQuiz::State::Idle);
}

int main()
{
    CheckTransportGestures();
    CheckReplayAndExit();
    CheckEveryLevelCompletion();
    CheckWaveformLowPass();
    CheckTargetRange();
    CheckKeyboardAndWaveformControls();
    CheckFreePlay();
    CheckGameModeVolume();
    CheckWaveforms();
    CheckResultAnimations();
    CheckControlRouting();
    CheckLevelRestarts();
    PitchQuiz selection_quiz;
    assert(selection_quiz.CurrentLevel() == 1);
    bool selected_notes[25] = {};
    for(uint32_t seed = 1; seed <= 4096; ++seed)
    {
        selection_quiz.Start(seed);
        const unsigned note = selection_quiz.Target();
        assert(note >= 48 && note <= 72);
        assert(IsNaturalNote(note));
        selected_notes[note - 48] = true;
    }
    for(unsigned note = 48; note <= 72; ++note)
        assert(selected_notes[note - 48] == IsNaturalNote(note));

    bool chromatic_notes[25] = {};
    for(uint32_t seed = 1; seed <= 4096; ++seed)
    {
        assert(selection_quiz.StartLevel(2, seed));
        const unsigned note = selection_quiz.Target();
        assert(note >= 48 && note <= 72);
        chromatic_notes[note - 48] = true;
    }
    for(bool selected : chromatic_notes)
        assert(selected);
    const unsigned previous_target = selection_quiz.Target();
    assert(!selection_quiz.StartLevel(0, 1));
    assert(!selection_quiz.StartLevel(3, 1));
    assert(selection_quiz.CurrentLevel() == 2);
    assert(selection_quiz.Target() == previous_target);

    PitchQuiz quiz;
    assert(quiz.Volume() == 0.25f);
    quiz.SetVolume(1.5f);
    assert(quiz.Volume() == 1.0f);
    quiz.SetVolume(-1.0f);
    assert(quiz.Volume() == 0.0f);
    quiz.SetVolume(0.25f);
    quiz.Press(60);
    assert(quiz.GetState() == PitchQuiz::State::Idle);
    assert(quiz.Process() == 0.0f);
    quiz.Start(123);
        PitchQuiz release_quiz;
        release_quiz.Start(1);
        const unsigned prompt_samples
            = static_cast<unsigned>(0.7f * PitchQuiz::kSampleRate)
              + static_cast<unsigned>(PitchQuiz::kReleaseSeconds * PitchQuiz::kSampleRate);
        for(unsigned i = 0; i < prompt_samples - 1; ++i)
            release_quiz.Process();
        assert(release_quiz.GetState() == PitchQuiz::State::Prompt);
        release_quiz.Process();
        assert(release_quiz.GetState() == PitchQuiz::State::Listening);
        release_quiz.Press(release_quiz.Target());
        const unsigned answer_samples
            = static_cast<unsigned>(0.25f * PitchQuiz::kSampleRate)
              + static_cast<unsigned>(PitchQuiz::kReleaseSeconds * PitchQuiz::kSampleRate);
        for(unsigned i = 0; i < answer_samples - 1; ++i)
            release_quiz.Process();
        assert(release_quiz.GetState() == PitchQuiz::State::Answer);
        release_quiz.Process();
        assert(release_quiz.GetState() == PitchQuiz::State::Feedback);
    assert(quiz.Target() >= 48 && quiz.Target() <= 72);
    assert(IsNaturalNote(quiz.Target()));
    quiz.Press(quiz.Target());
    assert(quiz.Attempts() == 0);
    float energy = 0.0f;
    unsigned crossings = 0;
    float previous = 0.0f;
    for(unsigned i = 0; i < PitchQuiz::kSampleRate / 2; ++i)
    {
        const float sample = quiz.Process();
        energy += sample * sample;
        if(previous <= 0.0f && sample > 0.0f)
            ++crossings;
        previous = sample;
    }
    const float expected = 440.0f * std::pow(2.0f, (quiz.Target() - 69.0f) / 12.0f);
    assert(energy > 1.0f);
    assert(std::fabs(crossings * 2.0f - expected) < 3.0f);
    FinishSounds(quiz);
    quiz.Press(128);
    assert(quiz.Attempts() == 0);

    const unsigned target = quiz.Target();
    for(unsigned attempt = 1; attempt <= 2; ++attempt)
    {
        quiz.Press(target + 1);
        quiz.Press(target);
        assert(quiz.Attempts() == attempt);
        FinishSounds(quiz);
        assert(quiz.Target() == target);
        assert(quiz.Completed() == 0);
        assert(quiz.GetResult(0) == PitchQuiz::Result::Pending);
    }
    quiz.Press(target);
    FinishSounds(quiz);
    assert(quiz.Completed() == 1);
    assert(quiz.GetResult(0) == PitchQuiz::Result::Correct);
    assert(quiz.Attempts() == 0);

    for(unsigned round = 1; round < PitchQuiz::kRoundLength; ++round)
    {
        const unsigned note = quiz.Target();
        assert(note >= 48 && note <= 72);
        assert(IsNaturalNote(note));
        for(unsigned attempt = 0; attempt < PitchQuiz::kMaxAttempts; ++attempt)
        {
            quiz.Press(note + 1);
            FinishSounds(quiz);
        }
        assert(quiz.Completed() == round + 1);
        assert(quiz.GetResult(round) == PitchQuiz::Result::Wrong);
    }
    assert(quiz.GetState() == PitchQuiz::State::LevelComplete);
    const uint32_t results = quiz.Progress();
    quiz.Press(quiz.Target());
    assert(quiz.Progress() == results);
    assert(quiz.GetResult(10) == PitchQuiz::Result::Pending);
    VerifyLevelCompletion(quiz);

    assert(quiz.StartLevel(2, 123));
    FinishSounds(quiz);
    for(unsigned question = 0; question < PitchQuiz::kRoundLength; ++question)
    {
        quiz.Press(quiz.Target());
        FinishSounds(quiz);
        assert(quiz.GetResult(question) == PitchQuiz::Result::Correct);
    }
    VerifyLevelCompletion(quiz);
    assert(quiz.GetState() == PitchQuiz::State::Idle);
    assert(quiz.CurrentLevel() == 2);
    assert(quiz.Completed() == PitchQuiz::kRoundLength);
    const uint32_t final_results = quiz.Progress();
    for(unsigned sample = 0; sample < 3 * PitchQuiz::kSampleRate; ++sample)
        assert(quiz.Process() == 0.0f);
    assert(quiz.Progress() == final_results);
    quiz.Start(42);
    assert(quiz.CurrentLevel() == 2);
    assert(quiz.Completed() == 0 && quiz.Progress() == 0);

    assert(quiz.StartLevel(1, 0));
    assert(quiz.Completed() == 0);
    assert(quiz.Progress() == 0);
    FinishSounds(quiz);
    for(unsigned round = 0; round < PitchQuiz::kRoundLength; ++round)
    {
        quiz.Press(quiz.Target());
        FinishSounds(quiz);
        assert(quiz.GetResult(round) == PitchQuiz::Result::Correct);
    }
    VerifyLevelCompletion(quiz);
    assert(quiz.StartLevel(1, 123));
    FinishSounds(quiz);
    quiz.Press(quiz.Target() + 1);
    quiz.Start(456);
    assert(quiz.Attempts() == 0 && quiz.Completed() == 0);
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
    std::cout << "Pitch quiz gameplay and audio checks passed\n";
}
