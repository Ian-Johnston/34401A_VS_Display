/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   GPIO configuration
  ******************************************************************************
*/

#include "gpio.h"
#include "spi.h"
#include "lt7680.h"
#include "main.h"

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    __HAL_RCC_AFIO_CLK_ENABLE();  // Needed for EXTI routing on F1

    // LED - Test output pin
    HAL_GPIO_WritePin(TEST_OUT_GPIO_Port, TEST_OUT_Pin, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = TEST_OUT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TEST_OUT_GPIO_Port, &GPIO_InitStruct);

    // LT7680A-R Reset
    GPIO_InitStruct.Pin = RESET_PIN;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RESET_PORT, &GPIO_InitStruct);


    //==============================================================================================
    // 34401A front panel sniff pins
    //==============================================================================================

    // PB13 SCK interrupt (falling edge)
    GPIO_InitStruct.Pin = FP_SCK_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;        // was RISING prior to trying to fix CONT mode SHIFT issue
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(FP_GPIO_Port, &GPIO_InitStruct);

    // PB14 / PB15 data pins (inputs)
    GPIO_InitStruct.Pin = FP_DIN_Pin | FP_DOUT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(FP_GPIO_Port, &GPIO_InitStruct);

    // PB12 INT (optional input)
    GPIO_InitStruct.Pin = FP_INT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(FP_GPIO_Port, &GPIO_InitStruct);


    //==============================================================================================
    // ST7701S LCD direct SPI (bit-bang)
    //==============================================================================================

    GPIO_InitStruct.Pin = LCD_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_CS_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LCD_SCK_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_SCK_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LCD_SDI_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_SDI_Port, &GPIO_InitStruct);


    //==============================================================================================
    // EXTI interrupt init (PB13 is on EXTI15_10)
    //==============================================================================================

    HAL_NVIC_SetPriority(FP_EXTI_IRQn, 0, 0);   // << changed to highest priority for sniffing
    HAL_NVIC_EnableIRQ(FP_EXTI_IRQn);

    // Clear any pending EXTI on PB13 before we start
    __HAL_GPIO_EXTI_CLEAR_IT(FP_SCK_Pin);


    //==============================================================================================
    // Digital inputs (general purpose)
    //==============================================================================================
    
    // Configure GPIO pin B0 - SHIFT annunciator enable/disable
    GPIO_InitStruct.Pin = GPIO_PIN_0;       // Select pin B0
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; // Set as input
    GPIO_InitStruct.Pull = GPIO_PULLUP;   // Enable pull-up resistor
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // Low speed is sufficient for input
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Configure GPIO pin B1 - unused
    GPIO_InitStruct.Pin = GPIO_PIN_1;       // Select pin B1
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; // Set as input
    GPIO_InitStruct.Pull = GPIO_PULLUP;   // Enable pull-up resistor
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // Low speed is sufficient for input
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
