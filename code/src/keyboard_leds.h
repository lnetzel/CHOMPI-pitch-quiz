#pragma once

#include "daisy_seed.h"
#include "stm32h7xx_hal.h"

namespace keyboard_leds
{
// CHOMPI keyboard: D18 / PA7, TIM3_CH2, GRB; black keys are LEDs 0..9.
// Timing and mapping: CHOMPI-Club/CHOMPI, TAPE temp_led_stuff.h / NormalPage.h.
constexpr unsigned kPadding = 144;
constexpr unsigned kSymbols = 25 * 24 + 2 * kPadding;
static uint32_t DMA_BUFFER_MEM_SECTION symbols[kSymbols];
static TIM_HandleTypeDef timer;
static DMA_HandleTypeDef dma;

inline void Init()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    GPIO_InitTypeDef pin = {};
    pin.Pin = GPIO_PIN_7;
    pin.Mode = GPIO_MODE_AF_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    pin.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &pin);

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
}

inline bool Show(uint32_t progress)
{
    for(unsigned led = 0; led < 25; ++led)
    {
        const unsigned result = led < 10 ? (progress >> (2 * led)) & 3 : 0;
        const uint8_t grb[] = {
            static_cast<uint8_t>(result == 1 ? 32 : 0),
            static_cast<uint8_t>(result == 2 ? 32 : 0),
            0
        };
        for(unsigned color = 0; color < 3; ++color)
            for(unsigned bit = 0; bit < 8; ++bit)
                symbols[kPadding + led * 24 + color * 8 + bit]
                    = (grb[color] & (0x80 >> bit)) ? 20 : 10;
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
    return ok;
}
}
