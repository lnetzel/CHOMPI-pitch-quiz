#include "../src/pitch_quiz.h"

#include <cassert>
#include <iostream>

void FinishSounds(PitchQuiz& quiz)
{
    for(unsigned i = 0; i < 2 * PitchQuiz::kSampleRate; ++i)
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

int main()
{
    PitchQuiz quiz;
    quiz.Press(60);
    assert(quiz.GetState() == PitchQuiz::State::Idle);
    assert(quiz.Process() == 0.0f);
    quiz.Start(123);
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
        assert(note >= 60 && note <= 71);
        assert(note != 61 && note != 63 && note != 66 && note != 68 && note != 70);
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
