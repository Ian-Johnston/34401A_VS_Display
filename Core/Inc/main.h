/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  ******************************************************************************
*/

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

	/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include <stdint.h>

/* Exported functions prototypes ---------------------------------------------*/
	void Error_Handler(void);

	/* Private defines -----------------------------------------------------------*/
#define TEST_OUT_Pin GPIO_PIN_13                    // PC13 - LED
#define TEST_OUT_GPIO_Port GPIOC

// SPI Pin Definitions (LT7680 on SPI1)
#define SPI_SCK_PIN        GPIO_PIN_5       // SCK  = PA5
#define SPI_SCK_PORT       GPIOA
#define SPI_MOSI_PIN       GPIO_PIN_7       // MOSI = PA7
#define SPI_MOSI_PORT      GPIOA
#define SPI_MISO_PIN       GPIO_PIN_6       // MISO = PA6
#define SPI_MISO_PORT      GPIOA
#define SPI_CS_PIN         GPIO_PIN_4       // CS   = PA4
#define SPI_CS_PORT        GPIOA


//==================================================================================================
// 34401A FRONT PANEL SNIFF PINS (as per decoder.cpp)
// PB13 = SCK (interrupt on rising edge)
// PB14 = Front panel data IN
// PB15 = Front panel data OUT
// PB12 = INT (optional, not required for basic sniffing)
//==================================================================================================
#define FP_INT_Pin         GPIO_PIN_12
#define FP_SCK_Pin         GPIO_PIN_13
#define FP_DIN_Pin         GPIO_PIN_14
#define FP_DOUT_Pin        GPIO_PIN_15
#define FP_GPIO_Port       GPIOB
#define FP_EXTI_IRQn       EXTI15_10_IRQn


//==================================================================================================
// LCD Controller (ST7701S via bit-bang SPI to panel controller)
//==================================================================================================
	void DelayMicroseconds(uint16_t us);

	// Define LCD SPI pins - bit bang SPI port for connection to the LCD ST7701S controller
#define LCD_CS_Pin    GPIO_PIN_3    // PB3
#define LCD_SCK_Pin   GPIO_PIN_4    // PB4
#define LCD_SDI_Pin   GPIO_PIN_5    // PB5
#define LCD_CS_Port   GPIOB
#define LCD_SCK_Port  GPIOB
#define LCD_SDI_Port  GPIOB

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
