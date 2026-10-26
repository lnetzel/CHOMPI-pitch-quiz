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
std::atomic<uint32_t> pressed_keys{0};
std::atomic<uint32_t> volume_percent{25};
std::atomic<uint32_t> volume_bar_until{0};
std::atomic<bool> game_active{false};
static_assert(ATOMIC_INT_LOCK_FREE == 2, "Audio requires lock-free progress updates");

struct VolumeEncoder
{
    dsy_gpio a;
    dsy_gpio b;
    uint8_t a_history = 0xff;
    uint8_t b_history = 0xff;
    uint32_t last_update = 0;

    void Init(dsy_gpio_pin pin_a, dsy_gpio_pin pin_b)
    {
        a.pin = pin_a;
        a.mode = DSY_GPIO_MODE_INPUT;
        a.pull = DSY_GPIO_PULLUP;
        b.pin = pin_b;
        b.mode = DSY_GPIO_MODE_INPUT;
        b.pull = DSY_GPIO_PULLUP;
        dsy_gpio_init(&a);
        dsy_gpio_init(&b);
        last_update = System::GetNow();
    }

    int Update()
    {
        const uint32_t now = System::GetNow();
        if(now == last_update)
            return 0;
        last_update = now;
        a_history = static_cast<uint8_t>((a_history << 1) | dsy_gpio_read(&a));
        b_history = static_cast<uint8_t>((b_history << 1) | dsy_gpio_read(&b));
        if((a_history & 0x03) == 0x02 && (b_history & 0x03) == 0x00)
            return 1;
        if((b_history & 0x03) == 0x02 && (a_history & 0x03) == 0x00)
            return -1;
        return 0;
    }
};

VolumeEncoder volume_encoder;

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
        history[i] = static_cast<uint8_t>((history[i] << 1) | !buttons.RawState(i));
        if(history[i] == 0xff && !held[i])
        {
            held[i] = true;
            rising[i] = true;
        }
        else if(history[i] == 0)
            held[i] = false;
    }
    uint32_t key_mask = 0;
    for(unsigned key = 0; key < 25; ++key)
        if(held[keys[key]])
            key_mask |= 1u << key;
    pressed_keys.store(key_mask, std::memory_order_relaxed);

    if(rising[33] || rising[34])
        quiz.Start(System::GetNow());
    else
        for(unsigned key = 0; key < 25; ++key)
            if(rising[keys[key]])
                quiz.Press(key + 48);

    const int turn = volume_encoder.Update();
    if(turn != 0)
    {
        int volume = static_cast<int>(volume_percent.load(std::memory_order_relaxed));
        volume += turn * 5;
        if(volume < 0)
            volume = 0;
        else if(volume > 100)
            volume = 100;
        if(volume != static_cast<int>(volume_percent.load(std::memory_order_relaxed)))
        {
            volume_percent.store(static_cast<uint32_t>(volume), std::memory_order_relaxed);
            quiz.SetVolume(volume / 100.0f);
            volume_bar_until.store(System::GetNow() + 500, std::memory_order_relaxed);
        }
    }
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
    game_active.store(quiz.GetState() != PitchQuiz::State::Idle,
                      std::memory_order_relaxed);
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
    controls.dbc_size = 8;
    buttons.Init(controls);
    volume_encoder.Init(seed::D15, seed::D17);
    quiz.SetVolume(0.25f);
    keyboard_leds::Init();
    keyboard_leds::Show(0, 0, 25, false, false);
    board.StartAudio(AudioCallback);

    uint32_t displayed = 0;
    uint32_t displayed_keys = 0;
    uint32_t displayed_volume = 25;
    bool displayed_volume_bar = false;
    bool displayed_game_active = false;
    while(true)
    {
        const uint32_t current = progress.load(std::memory_order_relaxed);
        const uint32_t keys = pressed_keys.load(std::memory_order_relaxed);
        const uint32_t volume = volume_percent.load(std::memory_order_relaxed);
        const uint32_t until = volume_bar_until.load(std::memory_order_relaxed);
        const bool show_volume_bar = static_cast<int32_t>(until - System::GetNow()) > 0;
        const bool active = game_active.load(std::memory_order_relaxed);
        if((current != displayed || keys != displayed_keys
            || volume != displayed_volume || show_volume_bar != displayed_volume_bar
            || active != displayed_game_active)
           && keyboard_leds::Show(current, keys, volume, show_volume_bar, active))
        {
            displayed = current;
            displayed_keys = keys;
            displayed_volume = volume;
            displayed_volume_bar = show_volume_bar;
            displayed_game_active = active;
        }
        System::Delay(1);
    }
}