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

void VerifyLevelTransition(PitchQuiz& quiz)
{
    assert(quiz.CurrentLevel() == 1);
    assert(quiz.GetState() == PitchQuiz::State::LevelComplete);
    const uint32_t results = quiz.Progress();
    assert(quiz_controls::PackUi(quiz, false) & quiz_controls::kResultHoldMask);
    for(unsigned sample = 0; sample < 2 * PitchQuiz::kSampleRate - 1; ++sample)
        assert(quiz.Process() == 0.0f);
    assert(quiz.GetState() == PitchQuiz::State::LevelComplete);
    assert(quiz.Progress() == results);
    quiz.Press(quiz.Target());
    assert(quiz.Progress() == results);
    assert(quiz.Process() == 0.0f);
    assert(quiz.CurrentLevel() == 2);
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
    assert(quiz.Completed() == 0 && quiz.Attempts() == 0);
    assert((quiz_controls::PackUi(quiz, false) & quiz_controls::kResultHoldMask) == 0);
    assert(quiz.Progress() == 0);
    assert(quiz.Volume() == 0.25f);
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

    for(unsigned restart : {quiz_controls::kPlaySwitch, quiz_controls::kLoopSwitch})
    {
        rising[restart] = true;
        quiz_controls::Process(quiz, rising, false, 456);
        rising[restart] = false;
        assert(quiz.CurrentLevel() == 2);
        assert(quiz.Completed() == 0 && quiz.Attempts() == 0);
        assert(quiz.GetState() == PitchQuiz::State::Prompt);
    }
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

void CheckFreePlay()
{
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

int main()
{
    CheckKeyboardAndWaveformControls();
    CheckFreePlay();
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
    VerifyLevelTransition(quiz);

    FinishSounds(quiz);
    for(unsigned question = 0; question < PitchQuiz::kRoundLength; ++question)
    {
        quiz.Press(quiz.Target());
        FinishSounds(quiz);
        assert(quiz.GetResult(question) == PitchQuiz::Result::Correct);
    }
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
    VerifyLevelTransition(quiz);
    assert(quiz.StartLevel(1, 123));
    FinishSounds(quiz);
    quiz.Press(quiz.Target() + 1);
    quiz.Start(456);
    assert(quiz.Attempts() == 0 && quiz.Completed() == 0);
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
    std::cout << "Pitch quiz gameplay and audio checks passed\n";
}
