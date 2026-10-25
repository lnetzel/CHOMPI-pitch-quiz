#include "daisy_seed.h"
#include "dev/sr_4021.h"
#include "keyboard_leds.h"
#include "pitch_quiz.h"

#include <atomic>

using namespace daisy;

namespace
{
DaisySeed board;
SaiHandle external_sai_handle;
ShiftRegister4021<5> buttons;
PitchQuiz quiz;
std::atomic<uint32_t> progress{0};
static_assert(ATOMIC_INT_LOCK_FREE == 2, "Audio requires lock-free progress updates");

void ScanControls()
{
    // Ascending C3..C5, from CHOMPI TAPE hardware.h / NormalPage.h.
    static constexpr unsigned keys[] = {
        15, 7, 8, 12, 9, 10, 13, 11, 14, 16, 21, 17, 18,
        22, 19, 23, 20, 24, 29, 25, 30, 26, 31, 27, 28
    };
    static uint8_t history[40] = {};
    static bool held[40] = {};
    bool rising[40] = {};
    buttons.Update();
    for(unsigned i = 0; i < 40; ++i)
    {
        history[i] = static_cast<uint8_t>((history[i] << 1) | !buttons.State(i));
        if(history[i] == 0xff && !held[i])
        {
            held[i] = true;
            rising[i] = true;
        }
        else if(history[i] == 0)
            held[i] = false;
    }
    if(rising[33])
        quiz.Start(System::GetNow());
    else
        for(unsigned key = 0; key < 25; ++key)
            if(rising[keys[key]])
                quiz.Press(key + 48);
}

void AudioCallback(AudioHandle::InputBuffer,
                   AudioHandle::OutputBuffer output,
                   size_t size)
{
    static unsigned control_frames = 0;
    for(size_t frame = 0; frame < size; ++frame)
    {
        if(control_frames++ == 0)
            ScanControls();
        if(control_frames == PitchQuiz::kSampleRate / 1000)
            control_frames = 0;
        const float sample = quiz.Process();
        for(size_t channel = 0; channel < 4; ++channel)
            output[channel][frame] = sample;
    }
    progress.store(quiz.Progress(), std::memory_order_relaxed);
}

void InitExternalAudio()
{
    SaiHandle::Config config;
    config.periph = SaiHandle::Config::Peripheral::SAI_2;
    config.sr = SaiHandle::Config::SampleRate::SAI_48KHZ;
    config.bit_depth = SaiHandle::Config::BitDepth::SAI_24BIT;
    config.a_sync = SaiHandle::Config::Sync::SLAVE;
    config.b_sync = SaiHandle::Config::Sync::MASTER;
    config.a_dir = SaiHandle::Config::Direction::TRANSMIT;
    config.b_dir = SaiHandle::Config::Direction::RECEIVE;
    config.pin_config.fs = daisy::seed::D27;
    config.pin_config.mclk = daisy::seed::D24;
    config.pin_config.sck = daisy::seed::D28;
    config.pin_config.sb = daisy::seed::D25;
    config.pin_config.sa = daisy::seed::D26;
    external_sai_handle.Init(config);

    AudioHandle::Config audio_config;
    audio_config.blocksize = 24;
    audio_config.samplerate = SaiHandle::Config::SampleRate::SAI_48KHZ;
    audio_config.postgain = 1.0f;
    board.audio_handle.Init(audio_config, board.AudioSaiHandle(), external_sai_handle);
}
}

int main()
{
    board.Init(true);
    InitExternalAudio();
    ShiftRegister4021<5>::Config controls;
    controls.clk = seed::D8;
    controls.latch = seed::D7;
    controls.data[0] = seed::D9;
    buttons.Init(controls);
    keyboard_leds::Init();
    keyboard_leds::Show(0);
    board.StartAudio(AudioCallback);

    uint32_t displayed = 0;
    while(true)
    {
        const uint32_t current = progress.load(std::memory_order_relaxed);
        if(current != displayed && keyboard_leds::Show(current))
            displayed = current;
        System::Delay(1);
    }
}