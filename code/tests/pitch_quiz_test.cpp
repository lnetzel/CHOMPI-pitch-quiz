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

void AnswerTarget(PitchQuiz& quiz)
{
    for(unsigned voice = 0; voice < quiz.TargetVoiceCount(); ++voice)
        quiz.Press(quiz.TargetNote(voice));
    for(unsigned voice = 0; voice < quiz.TargetVoiceCount(); ++voice)
        quiz.NoteOff(quiz.TargetNote(voice));
}

unsigned WrongNote(const PitchQuiz& quiz, unsigned excluded = 128, unsigned also_excluded = 128)
{
    for(unsigned note = 48; note <= 72; ++note)
    {
        bool matches_target = false;
        for(unsigned voice = 0; voice < quiz.TargetVoiceCount(); ++voice)
            matches_target |= note == quiz.TargetNote(voice);
        if(!matches_target && note != excluded && note != also_excluded)
            return note;
    }
    assert(false);
    return 0;
}

void AnswerWrong(PitchQuiz& quiz)
{
    unsigned wrong_notes[3] = {};
    for(unsigned voice = 0; voice < quiz.TargetVoiceCount(); ++voice)
    {
        wrong_notes[voice] = WrongNote(quiz,
                                       voice > 0 ? wrong_notes[0] : 128,
                                       voice > 1 ? wrong_notes[1] : 128);
        quiz.Press(wrong_notes[voice]);
    }
    for(unsigned voice = 0; voice < quiz.TargetVoiceCount(); ++voice)
        quiz.NoteOff(wrong_notes[voice]);
}

unsigned CountRisingCrossings(PitchQuiz& quiz, unsigned samples)
{
    unsigned crossings = 0;
    float previous = 0.0f;
    for(unsigned sample = 0; sample < samples; ++sample)
    {
        const float output = quiz.Process();
        if(previous <= 0.0f && output > 0.0f)
            ++crossings;
        previous = output;
    }
    return crossings;
}

void CheckFeedbackSounds()
{
    for(bool correct : {false, true})
    {
        PitchQuiz quiz;
        quiz.Start(123);
        FinishSounds(quiz);
        for(unsigned sample = 0; sample < PitchQuiz::kReleaseSeconds * PitchQuiz::kSampleRate;
            ++sample)
            quiz.Process();
        const unsigned target = quiz.Target();
        const unsigned answer_note = correct ? target : target == 48 ? 49 : 48;
        quiz.Press(answer_note);
        quiz_synth::FreePlay reference;
        reference.PlayNote(answer_note,
                           PitchQuiz::kSampleRate / 4,
                           PitchQuiz::kReleaseSeconds * PitchQuiz::kSampleRate);
        const unsigned answer_sustain_samples = PitchQuiz::kSampleRate / 4;
        for(unsigned sample = 0; sample < answer_sustain_samples; ++sample)
        {
            quiz.Process();
            reference.Process();
        }
        assert(quiz.GetState() == PitchQuiz::State::Feedback);

        constexpr unsigned window = PitchQuiz::kSampleRate / 10;
        const unsigned answer_release_samples
            = static_cast<unsigned>(PitchQuiz::kReleaseSeconds * PitchQuiz::kSampleRate);
        const unsigned first_feedback_note = correct ? 83 : 88;
        reference.PlayNote(first_feedback_note,
                           PitchQuiz::kFeedbackFirstNoteSamples,
                           PitchQuiz::kFeedbackClickReleaseSamples,
                           true);
        for(unsigned sample = 0; sample < window; ++sample)
        {
            const float output = quiz.Process();
            const float expected = quiz.Volume() * reference.Process();
            assert(std::fabs(output - expected) < 1e-7f);
        }
        for(unsigned sample = window; sample < answer_release_samples; ++sample)
            quiz.Process();
        const unsigned second_note = CountRisingCrossings(quiz, window);
        const unsigned expected_second = correct ? 132 : 99;
        assert(second_note >= expected_second - 3 && second_note <= expected_second + 3);

        const unsigned signal_length = PitchQuiz::kFeedbackSustainSamples;
        const unsigned samples_checked = answer_release_samples + window;
        for(unsigned sample = samples_checked; sample < signal_length - 1; ++sample)
            quiz.Process();
        assert(quiz.GetState() == PitchQuiz::State::Feedback);
        quiz.Process();
        assert(quiz.GetState() == PitchQuiz::State::Pause);
        if(correct)
        {
            float release_energy = 0.0f;
            for(unsigned sample = 0; sample < PitchQuiz::kFeedbackReleaseSamples; ++sample)
            {
                const float output = quiz.Process();
                release_energy += output * output;
            }
            assert(release_energy > 0.0f);
        }
    }
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
    assert(quiz_controls::PackUi(quiz, false) & quiz_controls::kResultHoldMask);
    float first_replay_energy = 0.0f;
    float second_replay_energy = 0.0f;
    for(unsigned sample = 0; sample < 2 * PitchQuiz::kSampleRate - 1; ++sample)
    {
        const float output = quiz.Process();
        if(sample >= PitchQuiz::kSampleRate / 5
           && sample < PitchQuiz::kFeedbackSustainSamples)
            first_replay_energy += output * output;
        else if(sample >= PitchQuiz::kPerfectReplayIntervalSamples
                               + PitchQuiz::kSampleRate / 5
                && sample < PitchQuiz::kPerfectReplayIntervalSamples
                               + PitchQuiz::kFeedbackSustainSamples)
            second_replay_energy += output * output;
    }
    const bool perfect = results == PitchQuiz::kPerfectProgress;
    assert((first_replay_energy > 0.0f) == perfect);
    assert((second_replay_energy > 0.0f) == perfect);
    assert(quiz.GetState() == PitchQuiz::State::LevelComplete);
    assert(quiz.Progress() == results);
    quiz.Press(quiz.Target());
    assert(quiz.Progress() == results);
    assert(quiz.Process() == 0.0f);
    assert(quiz.CurrentLevel() == selected_level);
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

    for(unsigned level = 3; level <= PitchQuiz::kLevelCount; ++level)
    {
        rising[quiz_controls::kBlackKeySwitches[level - 1]] = true;
        quiz_controls::Process(quiz, rising, true, 456);
        rising[quiz_controls::kBlackKeySwitches[level - 1]] = false;
        assert(quiz.CurrentLevel() == level);
        assert(quiz.GetState() == PitchQuiz::State::Prompt);
        FinishSounds(quiz);
        assert(quiz.GetState() == PitchQuiz::State::Listening);
        assert(quiz.Attempts() == 0);
    }
    rising[quiz_controls::kBlackKeySwitches[4]] = true;
    quiz_controls::Process(quiz, rising, true, 456);
    rising[quiz_controls::kBlackKeySwitches[4]] = false;
    FinishSounds(quiz);
    assert(quiz.CurrentLevel() == 5);
    rising[15] = true;
    quiz_controls::Process(quiz, rising, true, 456);
    assert(quiz.Attempts() == 0);
    quiz_controls::Process(quiz, rising, false, 456);
    assert(quiz.GetState() == PitchQuiz::State::Listening);
    assert(quiz.Attempts() == 0);
    rising[15] = false;
    rising[quiz_controls::kKeySwitches[1]] = true;
    quiz_controls::Process(quiz, rising, false, 456);
    assert(quiz.Attempts() == 1);
    rising[quiz_controls::kKeySwitches[1]] = false;

    for(unsigned button : {quiz_controls::kPlaySwitch, quiz_controls::kLoopSwitch})
    {
        rising[button] = true;
        quiz_controls::Process(quiz, rising, false, 456);
        rising[button] = false;
        assert(quiz.CurrentLevel() == PitchQuiz::kLevelCount);
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
        assert(!quiz.StartLevel(11, 999));
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
        float prompt_tail_energy = 0.0f;
        for(unsigned sample = 0; sample < PitchQuiz::kReleaseSeconds * PitchQuiz::kSampleRate;
            ++sample)
        {
            const float output = quiz.Process();
            prompt_tail_energy += output * output;
        }
        assert(prompt_tail_energy > 0.0f);
        for(unsigned sample = 0;
            sample < 3 * PitchQuiz::kSampleRate
                         - PitchQuiz::kReleaseSeconds * PitchQuiz::kSampleRate;
            ++sample)
            assert(quiz.Process() == 0.0f);
        assert(quiz.GetState() == PitchQuiz::State::Listening);
        assert(quiz.CurrentLevel() == 2);


    PitchQuiz chord_selection;
    bool select_chord[40] = {};
    select_chord[quiz_controls::kBlackKeySwitches[4]] = true;
    quiz_controls::Process(chord_selection, select_chord, true, 123);
    assert(chord_selection.CurrentLevel() == 5);
    assert(chord_selection.TargetVoiceCount() == 2);

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

void CheckLoopIndicator()
{
    const auto idle = quiz_display::LoopColor(false, 1200);
    assert(idle.red == 0 && idle.green == 0 && idle.blue == 0);
    const auto dim = quiz_display::LoopColor(true, 0);
    const auto bright = quiz_display::LoopColor(true, 1200);
    const auto repeated = quiz_display::LoopColor(true, 2400);
    assert(dim.red == 4 && dim.green == 4 && dim.blue == 0);
    assert(bright.red == 48 && bright.green == 48 && bright.blue == 0);
    assert(repeated.red == dim.red && repeated.green == dim.green);
    const auto shift_dim = quiz_display::ShiftColor(0);
    const auto shift_bright = quiz_display::ShiftColor(1200);
    assert(shift_dim.red == 4 && shift_dim.green == 0 && shift_dim.blue == 0);
    assert(shift_bright.red == 32 && shift_bright.green == 0 && shift_bright.blue == 0);
}

void CheckAttemptColors()
{
    PitchQuiz quiz;
    quiz.Start(123);
    FinishSounds(quiz);
    const unsigned wrong_note = quiz.Target() == 127 ? quiz.Target() - 1 : quiz.Target() + 1;

    quiz.Press(wrong_note);
    unsigned wrong_attempts
        = (quiz_controls::PackUi(quiz, false) >> quiz_controls::kWrongAttemptsShift) & 3;
    assert(wrong_attempts == 1);
    auto color = quiz_display::ResultColor(quiz.Progress(), 0, false, 0, wrong_attempts);
    assert(color.red == 32 && color.green == 32 && color.blue == 0);
    FinishSounds(quiz);

    quiz.Press(wrong_note);
    wrong_attempts
        = (quiz_controls::PackUi(quiz, false) >> quiz_controls::kWrongAttemptsShift) & 3;
    assert(wrong_attempts == 2);
    color = quiz_display::ResultColor(quiz.Progress(), 0, false, 0, wrong_attempts);
    assert(color.red == 48 && color.green == 24 && color.blue == 0);
    FinishSounds(quiz);

    quiz.Press(wrong_note);
    ReachState(quiz, PitchQuiz::State::Feedback);
    assert(quiz.GetResult(0) == PitchQuiz::Result::Wrong);
    color = quiz_display::ResultColor(quiz.Progress(), 0, false, 0);
    assert(color.red == 32 && color.green == 0 && color.blue == 0);
}

void CheckWaveformLowPass()
{
    const uint8_t raw_waveforms[] = {
        daisysp::Oscillator::WAVE_TRI,
        daisysp::Oscillator::WAVE_POLYBLEP_SQUARE
    };
    for(unsigned waveform = 0; waveform < 2; ++waveform)
    {
        for(float frequency : {130.8128f, 440.0f, 523.2511f})
        {
            quiz_synth::Oscillator filtered;
            filtered.SetSquare(waveform == 1);
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
            fresh.SetSquare(waveform == 1);
            assert(restarted == fresh.Process(frequency));
        }
    }
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
    quiz_synth::FreePlay retrigger;
    retrigger.NoteOn(60);
    float previous = 0.0f;
    for(unsigned sample = 0; sample < 3000; ++sample)
        previous = retrigger.Process();
    retrigger.NoteOff(60);
    retrigger.NoteOn(60);
    assert(retrigger.ActiveVoices() == 2);
    assert(std::fabs(retrigger.Process() - previous) < 0.004f);

    quiz_synth::FreePlay stolen;
    stolen.NoteOn(60);
    for(unsigned note = 61; note < 66; ++note)
        stolen.NoteOn(note);
    for(unsigned sample = 0; sample < 3000; ++sample)
        previous = stolen.Process();
    stolen.NoteOn(67);
    assert(std::fabs(stolen.Process() - previous) < 0.004f);

    quiz_synth::FreePlay synth;
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

    PitchQuiz quiz;
    quiz.NoteOn(60);
    float energy = 0.0f;
    for(unsigned sample = 0; sample < 4800; ++sample)
        energy += std::fabs(quiz.Process());
    assert(energy > 1.0f);
    assert(quiz.GetState() == PitchQuiz::State::Idle && quiz.Attempts() == 0);
    quiz.Start(123);
    FinishSounds(quiz);
    float prompt_tail_energy = 0.0f;
    for(unsigned sample = 0; sample < PitchQuiz::kReleaseSeconds * PitchQuiz::kSampleRate;
        ++sample)
    {
        const float output = quiz.Process();
        prompt_tail_energy += output * output;
    }
    assert(prompt_tail_energy > 0.0f);
    assert(quiz.Process() == 0.0f);
    quiz.NoteOn(60);
    assert(quiz.Process() == 0.0f);
}

void CheckKeyboardControls()
{
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

    const auto ui = quiz_controls::PackUi(quiz, false);
    assert((ui >> 27) == 0);
    assert((ui & quiz_controls::kActiveMask) == 0);
    rising[quiz_controls::kPlaySwitch] = true;
    quiz_controls::Process(quiz, rising, false, 6, falling);
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
}

unsigned TargetIndex(unsigned note, unsigned level)
{
    if(level == 3 || level == 4 || level == 7 || level == 8 || level == 10)
        return note - 48;
    unsigned index = 0;
    for(unsigned candidate = 48; candidate < note; ++candidate)
        if(IsNaturalNote(candidate))
            ++index;
    return index;
}

void CheckLevelProfiles()
{
    static constexpr unsigned voices[] = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3};
    static constexpr unsigned difficulties[] = {7, 12, 7, 12, 7, 12, 7, 12, 12, 14};
    static constexpr bool chromatic[] = {
        false, false, true, true, false, false, true, true, false, true
    };
    assert(PitchQuiz::kLevelCount == 10);
    for(unsigned level = 1; level <= PitchQuiz::kLevelCount; ++level)
        for(uint32_t seed = 1; seed <= 8; ++seed)
        {
            PitchQuiz quiz;
            assert(quiz.StartLevel(level, seed));
            assert(quiz.TargetVoiceCount() == voices[level - 1]);
            assert(quiz.TargetRangeDifficulty() == difficulties[level - 1]);
            assert(quiz.UsesBlackKeys() == chromatic[level - 1]);
            FinishSounds(quiz);
            for(unsigned question = 0; question < PitchQuiz::kRoundLength; ++question)
            {
                const unsigned previous = TargetIndex(quiz.Target(), level);
                for(unsigned voice = 0; voice < quiz.TargetVoiceCount(); ++voice)
                {
                    const unsigned target = quiz.TargetNote(voice);
                    assert(target >= 48 && target <= 72);
                    if(!chromatic[level - 1])
                        assert(IsNaturalNote(target));
                    for(unsigned other = 0; other < voice; ++other)
                        assert(quiz.TargetNote(other) != target);
                }
                if(question + 1 == PitchQuiz::kRoundLength)
                    break;
                AnswerTarget(quiz);
                FinishSounds(quiz);
                const unsigned next = TargetIndex(quiz.Target(), level);
                assert((next > previous ? next - previous : previous - next)
                       <= difficulties[level - 1]);
            }
    }
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
                    AnswerTarget(quiz);
                else
                    for(unsigned attempt = 0; attempt < PitchQuiz::kMaxAttempts; ++attempt)
                    {
                        AnswerWrong(quiz);
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

void CheckMultiVoiceChordInput()
{
    PitchQuiz cancelled_chord;
    assert(cancelled_chord.StartLevel(5, 123));
    FinishSounds(cancelled_chord);
    cancelled_chord.Press(cancelled_chord.TargetNote(0));
    assert(cancelled_chord.Attempts() == 0);
    float single_note_energy = 0.0f;
    for(unsigned sample = 0; sample < PitchQuiz::kSampleRate / 10; ++sample)
    {
        const float output = cancelled_chord.Process();
        single_note_energy += output * output;
    }
    assert(single_note_energy > 0.0f);
    cancelled_chord.NoteOff(cancelled_chord.TargetNote(0));
    cancelled_chord.Press(cancelled_chord.TargetNote(1));
    assert(cancelled_chord.Attempts() == 0);
    cancelled_chord.NoteOff(cancelled_chord.TargetNote(1));
    assert(cancelled_chord.GetState() == PitchQuiz::State::Listening);

    PitchQuiz quiz;
    assert(quiz.StartLevel(5, 456));
    FinishSounds(quiz);
    const unsigned first_wrong = WrongNote(quiz);
    const unsigned second_wrong = WrongNote(quiz, first_wrong);
    quiz.Press(first_wrong);
    assert(quiz.GetState() == PitchQuiz::State::Listening);
    assert(quiz.Attempts() == 0);
    quiz.Press(second_wrong);
    assert(quiz.GetState() == PitchQuiz::State::Answer);
    assert(quiz.Attempts() == 1);
    quiz.NoteOff(first_wrong);
    quiz.NoteOff(second_wrong);
    FinishSounds(quiz);
    assert(quiz.GetState() == PitchQuiz::State::Listening);
    assert(quiz.Completed() == 0 && quiz.Attempts() == 1);

    AnswerTarget(quiz);
    assert(quiz.Attempts() == 2);
    FinishSounds(quiz);
    assert(quiz.Completed() == 1);
    assert(quiz.GetResult(0) == PitchQuiz::Result::Correct);

    PitchQuiz three_note_quiz;
    assert(three_note_quiz.StartLevel(9, 789));
    FinishSounds(three_note_quiz);
    assert(three_note_quiz.TargetVoiceCount() == 3);
    const unsigned first_wrong_note = WrongNote(three_note_quiz);
    const unsigned second_wrong_note = WrongNote(three_note_quiz, first_wrong_note);
    const unsigned third_wrong_note = WrongNote(three_note_quiz,
                                                first_wrong_note,
                                                second_wrong_note);
    three_note_quiz.Press(first_wrong_note);
    three_note_quiz.Press(second_wrong_note);
    assert(three_note_quiz.GetState() == PitchQuiz::State::Listening);
    assert(three_note_quiz.Attempts() == 0);
    three_note_quiz.Press(third_wrong_note);
    assert(three_note_quiz.GetState() == PitchQuiz::State::Answer);
    assert(three_note_quiz.Attempts() == 1);
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
    quiz.ExitToIdle();
    assert(quiz.GetState() == PitchQuiz::State::Idle);
    assert(quiz.CurrentLevel() == 2 && quiz.Attempts() == 1);
    assert(quiz.Volume() == 0.4f);
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
    CheckFeedbackSounds();
    CheckTransportGestures();
    CheckReplayAndExit();
    CheckEveryLevelCompletion();
    CheckWaveformLowPass();
    CheckLevelProfiles();
    CheckKeyboardControls();
    CheckFreePlay();
    CheckGameModeVolume();
    CheckResultAnimations();
    CheckLoopIndicator();
    CheckAttemptColors();
    CheckControlRouting();
    CheckLevelRestarts();
        CheckMultiVoiceChordInput();
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
        assert(selection_quiz.StartLevel(3, seed));
        const unsigned note = selection_quiz.Target();
        assert(note >= 48 && note <= 72);
        chromatic_notes[note - 48] = true;
    }
    for(bool selected : chromatic_notes)
        assert(selected);
    const unsigned previous_target = selection_quiz.Target();
    assert(!selection_quiz.StartLevel(0, 1));
    assert(!selection_quiz.StartLevel(11, 1));
    assert(selection_quiz.CurrentLevel() == 3);
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
                        = static_cast<unsigned>(0.7f * PitchQuiz::kSampleRate);
        for(unsigned i = 0; i < prompt_samples - 1; ++i)
            release_quiz.Process();
        assert(release_quiz.GetState() == PitchQuiz::State::Prompt);
        release_quiz.Process();
        assert(release_quiz.GetState() == PitchQuiz::State::Listening);
        release_quiz.StopFreePlay();
        float prompt_release_energy = 0.0f;
        for(unsigned sample = 0; sample < PitchQuiz::kSampleRate / 10; ++sample)
        {
            const float output = release_quiz.Process();
            prompt_release_energy += output * output;
        }
        assert(prompt_release_energy > 0.0f);
        release_quiz.Press(release_quiz.Target());
        const unsigned answer_samples
            = static_cast<unsigned>(0.25f * PitchQuiz::kSampleRate);
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
    assert(energy > 0.5f);
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
