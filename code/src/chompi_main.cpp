#include "daisy_seed.h"

using namespace daisy;

namespace
{
DaisySeed board;
SaiHandle external_sai_handle;

void AudioCallback(AudioHandle::InputBuffer input,
                   AudioHandle::OutputBuffer output,
                   size_t size)
{
    for(size_t frame = 0; frame < size; ++frame)
    {
        for(size_t channel = 0; channel < 4; ++channel)
            output[channel][frame] = input[channel][frame];
    }
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
    board.StartAudio(AudioCallback);

    while(true)
        System::Delay(1);
}