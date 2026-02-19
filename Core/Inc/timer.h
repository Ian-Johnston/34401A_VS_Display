/**
  ******************************************************************************
  * @file    timer.h
  * @brief   Minimal timer helpers for the 34401A sniffing template build
  ******************************************************************************
*/

#ifndef TIMER_H
#define TIMER_H

#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"

// If other modules still expect these externs, keep them here.
// (They were present in your original timer.h)
extern volatile uint8_t timer_flag;
extern volatile uint8_t task_ready;
extern volatile uint8_t logReady;

// Optional (only if you actually use TIM2 as an interrupt timer)
void TIM2_Init(void);
void TIM2_IRQHandler(void);

// Used in your original codebase; safe to keep even if unused.
void SetTimerDuration(uint16_t ms);

#endif // TIMER_H
