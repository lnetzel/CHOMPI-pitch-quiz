#pragma once

#include "daisy_seed.h"
#include "pitch_quiz.h"
#include "quiz_display.h"
#include "stm32h7xx_hal.h"

namespace keyboard_leds
{
// CHOMPI keyboard: D18 / PA7, TIM3_CH2, GRB; black keys are LEDs 0..9.
// Timing and mapping: CHOMPI-Club/CHOMPI, TAPE temp_led_stuff.h / NormalPage.h.
constexpr unsigned kPadding = 144;
constexpr unsigned kSymbols = 25 * 24 + 2 * kPadding;
constexpr unsigned kPanelLedCount = 10;
constexpr unsigned kPanelSymbols = kPanelLedCount * 24 + 2 * kPadding;
static uint32_t DMA_BUFFER_MEM_SECTION symbols[kSymbols];
static uint32_t DMA_BUFFER_MEM_SECTION panel_symbols[kPanelSymbols];
static TIM_HandleTypeDef timer;
static DMA_HandleTypeDef dma;
static TIM_HandleTypeDef panel_timer;
static DMA_HandleTypeDef panel_dma;

static constexpr unsigned kKeyLeds[25] = {
    24, 0, 23, 1, 22, 21, 2, 20, 3, 19, 4, 18, 17,
    5, 16, 6, 15, 14, 7, 13, 8, 12, 9, 11, 10
};
static constexpr unsigned kWhiteKeyLeds[15] = {
    24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10
};

inline void EncodeLed(uint32_t* destination, uint8_t red, uint8_t green, uint8_t blue)
{
    const uint8_t grb[] = {green, red, blue};
    for(unsigned color = 0; color < 3; ++color)
        for(unsigned bit = 0; bit < 8; ++bit)
            destination[color * 8 + bit]
                = (grb[color] & (0x80 >> bit)) ? 20 : 10;
}

inline void EncodePanelLed(uint32_t* symbols,
                           unsigned logical_led,
                           uint8_t red,
                           uint8_t green,
                           uint8_t blue)
{
    uint32_t* destination = &symbols[kPadding + logical_led * 24];
    const uint8_t rgb[] = {red, green, blue};
    for(unsigned color = 0; color < 3; ++color)
        for(unsigned bit = 0; bit < 8; ++bit)
            destination[color * 8 + bit]
                = (rgb[color] & (0x80 >> bit)) ? 20 : 10;
}

inline void Init()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM5_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    GPIO_InitTypeDef pin = {};
    pin.Pin = GPIO_PIN_7;
    pin.Mode = GPIO_MODE_AF_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    pin.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &pin);

    GPIO_InitTypeDef panel_pin = {};
    panel_pin.Pin = GPIO_PIN_3;
    panel_pin.Mode = GPIO_MODE_AF_PP;
    panel_pin.Pull = GPIO_NOPULL;
    panel_pin.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    panel_pin.Alternate = GPIO_AF2_TIM5;
    HAL_GPIO_Init(GPIOA, &panel_pin);

    timer.Instance = TIM3;
    timer.Init.Prescaler = 7;
    timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    timer.Init.Period = (daisy::System::GetPClk1Freq() * 2 / 8 / 833332) - 1;
    timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&timer);
    TIM_OC_InitTypeDef channel = {};
    channel.OCMode = TIM_OCMODE_PWM1;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    HAL_TIM_PWM_ConfigChannel(&timer, &channel, TIM_CHANNEL_2);

    panel_timer.Instance = TIM5;
    panel_timer.Init.Prescaler = 7;
    panel_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    panel_timer.Init.Period = timer.Init.Period;
    panel_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&panel_timer);
    HAL_TIM_PWM_ConfigChannel(&panel_timer, &channel, TIM_CHANNEL_4);

    // Poll DMA completion in the foreground; no interrupt or audio blocking.
    dma.Instance = DMA2_Stream5;
    dma.Init.Request = DMA_REQUEST_TIM3_UP;
    dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    dma.Init.PeriphInc = DMA_PINC_DISABLE;
    dma.Init.MemInc = DMA_MINC_ENABLE;
    dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    dma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    dma.Init.Mode = DMA_NORMAL;
    dma.Init.Priority = DMA_PRIORITY_LOW;
    dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&dma);
    HAL_TIM_PWM_Start(&timer, TIM_CHANNEL_2);

    panel_dma.Instance = DMA2_Stream6;
    panel_dma.Init.Request = DMA_REQUEST_TIM5_CH4;
    panel_dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    panel_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    panel_dma.Init.MemInc = DMA_MINC_ENABLE;
    panel_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    panel_dma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    panel_dma.Init.Mode = DMA_NORMAL;
    panel_dma.Init.Priority = DMA_PRIORITY_LOW;
    panel_dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&panel_dma);
    HAL_TIM_PWM_Start(&panel_timer, TIM_CHANNEL_4);
}

inline bool Show(uint32_t progress,
                 uint32_t pressed_keys,
                 uint32_t volume_percent,
                 bool show_volume_bar,
                 uint8_t play_blue,
                 unsigned current_level,
                 bool shift_held,
                 bool result_hold = false,
                 uint32_t result_elapsed_ms = 0)
{
    for(unsigned led = 0; led < 25; ++led)
    {
        const unsigned result = led < 10 ? (progress >> (2 * led)) & 3 : 0;
        EncodeLed(&symbols[kPadding + led * 24],
                  result == 2 ? 32 : 0,
                  result == 1 ? 32 : 0,
                  0);
    }

    if(show_volume_bar)
    {
        const unsigned lit = (volume_percent * 15 + 50) / 100;
        for(unsigned white = 0; white < lit && white < 15; ++white)
        {
            const uint8_t red = white >= 11 ? 48 : (white >= 7 ? 48 : 0);
            const uint8_t green = white >= 11 ? 0 : (white >= 7 ? 29 : 48);
            EncodeLed(&symbols[kPadding + kWhiteKeyLeds[white] * 24],
                      red,
                      green,
                      0);
        }
    }
    for(unsigned key = 0; key < 25; ++key)
        if(pressed_keys & (1u << key))
            EncodeLed(&symbols[kPadding + kKeyLeds[key] * 24], 64, 64, 64);

    if(result_hold)
        for(unsigned led = 0; led < PitchQuiz::kRoundLength; ++led)
        {
            const auto color = quiz_display::ResultColor(progress, led, true, result_elapsed_ms);
            EncodeLed(&symbols[kPadding + led * 24], color.red, color.green, color.blue);
        }

    if(shift_held)
        for(unsigned led = 0; led < 10; ++led)
        {
            const uint8_t blue = led < PitchQuiz::kLevelCount
                                     ? (led + 1 == current_level ? 64 : 8)
                                     : 0;
            EncodeLed(&symbols[kPadding + led * 24], 0, 0, blue);
        }

    if(HAL_DMA_Start(&dma,
                     reinterpret_cast<uint32_t>(symbols),
                     reinterpret_cast<uint32_t>(&TIM3->CCR2),
                     kSymbols) != HAL_OK)
        return false;
    __HAL_TIM_ENABLE_DMA(&timer, TIM_DMA_UPDATE);
    const bool ok = HAL_DMA_PollForTransfer(&dma, HAL_DMA_FULL_TRANSFER, 10) == HAL_OK;
    __HAL_TIM_DISABLE_DMA(&timer, TIM_DMA_UPDATE);
    if(!ok)
        HAL_DMA_Abort(&dma);
    __HAL_TIM_SET_COMPARE(&timer, TIM_CHANNEL_2, 0);
    daisy::System::Delay(1);

    for(unsigned i = 0; i < kPanelSymbols; ++i)
        panel_symbols[i] = 0;
    for(unsigned led = 0; led < kPanelLedCount; ++led)
        EncodePanelLed(panel_symbols, led, 0, 0, 0);
    const float volume = (volume_percent > 100 ? 100 : volume_percent) / 100.0f;
    const uint8_t level = static_cast<uint8_t>(volume * 255.0f);
    uint8_t red = 0;
    uint8_t green = level;
    if(volume > 0.75f)
    {
        red = level;
        green = 0;
    }
    else if(volume > 0.5f)
        red = level;
    EncodePanelLed(panel_symbols, 7, 0, 0, play_blue);
    EncodePanelLed(panel_symbols, 9, red, green, 0);
    if(HAL_DMA_Start(&panel_dma,
                     reinterpret_cast<uint32_t>(panel_symbols),
                     reinterpret_cast<uint32_t>(&TIM5->CCR4),
                     kPanelSymbols) != HAL_OK)
        return false;
    __HAL_TIM_ENABLE_DMA(&panel_timer, TIM_DMA_CC4);
    const bool panel_ok
        = HAL_DMA_PollForTransfer(&panel_dma, HAL_DMA_FULL_TRANSFER, 10) == HAL_OK;
    __HAL_TIM_DISABLE_DMA(&panel_timer, TIM_DMA_CC4);
    if(!panel_ok)
        HAL_DMA_Abort(&panel_dma);
    __HAL_TIM_SET_COMPARE(&panel_timer, TIM_CHANNEL_4, 0);
    daisy::System::Delay(1);
    return ok && panel_ok;
}
}
