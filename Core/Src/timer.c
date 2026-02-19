/**
  ******************************************************************************
  * @file    timer.c
  * @brief   Minimal timer helpers for the 34401A sniffing template build
  ******************************************************************************
*/

#include "timer.h"

// Keep these globals for compatibility with your existing project structure.
// If nothing uses them, they can stay at 0 forever.
volatile uint8_t timer_flag = 0;
volatile uint8_t task_ready = 0;

// logReady is declared extern in main.c as well; define it here if you want
// a single home for it. If it's defined elsewhere already, comment this out.
volatile uint8_t logReady = 0;


//------------------------------------------------------------------------------
// Optional TIM2 init (only needed if you actually want a periodic interrupt)
// This config matches your earlier commented code: 10 kHz timer tick.
//------------------------------------------------------------------------------
void TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    TIM2->PSC = 7200 - 1;   // 72 MHz / 7200 = 10 kHz
    TIM2->ARR = 5000 - 1;   // default 500 ms
    TIM2->DIER |= TIM_DIER_UIE;
    TIM2->CR1 |= TIM_CR1_CEN;

    HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR &= ~TIM_SR_UIF;
        timer_flag = 1;
    }
}


//------------------------------------------------------------------------------
// Timer 2 - Dynamic timer duration setting function
// Assumes TIM2 tick is 10 kHz (1 tick = 0.1 ms)
//------------------------------------------------------------------------------
void SetTimerDuration(uint16_t ms)
{
    TIM2->ARR = (10 * ms) - 1;   // 10 ticks per ms at 10 kHz
    TIM2->EGR = TIM_EGR_UG;      // apply immediately
}
