#include "../src/pitch_quiz.h"

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
           || quiz.GetState() == PitchQuiz::State::Idle)
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

int main()
{
    PitchQuiz selection_quiz;
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
    assert(quiz.GetState() == PitchQuiz::State::Idle);
    const uint32_t results = quiz.Progress();
    quiz.Press(quiz.Target());
    assert(quiz.Progress() == results);
    assert(quiz.GetResult(10) == PitchQuiz::Result::Pending);

    quiz.Start(0);
    assert(quiz.Completed() == 0);
    assert(quiz.Progress() == 0);
    FinishSounds(quiz);
    for(unsigned round = 0; round < PitchQuiz::kRoundLength; ++round)
    {
        quiz.Press(quiz.Target());
        FinishSounds(quiz);
        assert(quiz.GetResult(round) == PitchQuiz::Result::Correct);
    }
    assert(quiz.GetState() == PitchQuiz::State::Idle);
    quiz.Start(123);
    FinishSounds(quiz);
    quiz.Press(quiz.Target() + 1);
    quiz.Start(456);
    assert(quiz.Attempts() == 0 && quiz.Completed() == 0);
    assert(quiz.GetState() == PitchQuiz::State::Prompt);
    std::cout << "Pitch quiz gameplay and audio checks passed\n";
}
