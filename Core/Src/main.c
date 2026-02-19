/*
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  *
  * By Ian Johnston (IanSJohnston on YouTube),
  * for 3.71" 960x240 TFT LCD (ST7701S) and using LT7680 controller adaptor
  * Visual Studio 2022 with VisualGDB plugin:
  * To upload HEX from VS2022 = BUILD then PROGRAM AND START WITHOUT DEBUGGING
  * Use LIVE WATCH to view variables live debug
  *
  * For use with LT7680A-R & 240x960 TFT LCD
  *
  ******************************************************************************
  *
  * For 34401A sniffing:
  * ========================================
  * BluePill PB13 = SCK (EXTI rising edge)
  *          PB14 = FP DATA IN
  *          PB15 = FP DATA OUT
  *          PB12 = INT (optional)
  *
 */


 /* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "spi.h"
#include "gpio.h"
#include "decoder_34401a.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "lt7680.h"
#include "timer.h"
#include <stdbool.h>    // bool support, otherwise use _Bool
#include <stdlib.h>
#include "display.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_tim.h"
#include <stddef.h>

    TIM_HandleTypeDef htim3; // Definition of htim3

//******************************************************************************
// Variables

volatile uint8_t TEST1 = 0;
volatile uint8_t TEST2 = 0;
volatile uint8_t TEST3 = 0;

#define MAX_BUFFER_SIZE 256  // Define the size of the buffer

volatile uint32_t debugO2Callback = 0;

#define DISPLAY_LENGTH 12  // Number of characters in the main display

char displayString[DISPLAY_LENGTH + 1];  // +1 for null-terminator

uint8_t isaState = 0;
uint8_t inaState = 0;

extern volatile uint8_t logReady;

//******************************************************************************
// HP Symbols

// Placeholder definitions for missing symbols
#define Da 0x01
#define Db 0x02
#define Dc 0x04
#define Dd 0x08
#define De 0x10
#define Df 0x20
#define Dg1 0x40
#define Dg2 0x80
#define Dm 0x100
#define Ds 0x200
#define Dk 0x400
#define Dt 0x800
#define Dn 0x1000
#define Dr 0x2000


//******************************************************************************

// Flag indicating finish of SPI transmission to OLED
volatile uint8_t SPI1_TX_completed_flag = 1;

// Flag indicating finish of start-up initialization
volatile uint8_t Init_Completed_flag = 0;

// Private function prototypes
void SystemClock_Config(void);

//******************************************************************************

// LT7680A-R - SPI transmission finished interrupt callback
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi->Instance == SPI1)
    {
        SPI1_TX_completed_flag = 1;
    }
}


//************************************************************************************************************************************************************
//************************************************************************************************************************************************************

// Main
int main(void) {

    // Reset of all peripherals, Initializes the Flash interface and the Systick.
    HAL_Init();

    // Configure the system clock
    SystemClock_Config();

    // Initialize all configured peripherals
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI1_Init();     // LT7680A-R

    Decoder34401_Init();

    //MX_TIM3_Init();   // 3457A only (TIM3 input capture)
    //HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_4);  // 3457A only (PB1 = TIM3_CH4)

    // Pull CS high and SCLK low immediately after reset
    HAL_GPIO_WritePin(LCD_CS_Port, LCD_CS_Pin, GPIO_PIN_SET);         // Pull CS high
    HAL_GPIO_WritePin(LCD_SCK_Port, LCD_SCK_Pin, GPIO_PIN_RESET);     // CLK pin low

    // Pull LT7680 RESET pin high immediately after reset
    HAL_GPIO_WritePin(RESET_PORT, RESET_PIN, GPIO_PIN_SET);   // Release reset high

    HardwareReset();                // Reset LT7680 - Pull LCM_RESET low for 100ms and wait

    HAL_Delay(1000);

    BuyDisplay_Init();              // Initialize ST7701S BuyDisplay 3.71" driver IC

    HAL_Delay(100);

    SendAllToLT7680_LT();           // run subs to setup LT7680 based on Levetop info

    HAL_Delay(10);

    // Main loop timer
    //SetTimerDuration(35);          // 35 ms timed action set.......not used
    //HAL_Delay(5);
    //SetBacklightFull();
    ConfigurePWMAndSetBrightness(BACKLIGHTFULL);  // Configure Timer-1 and PWM-1 for backlighting. Settable 0-100%

    ClearScreen();                  // Again.....

    // Right wipe to clear random pixels down the far right hand side
    DrawLine(0, 959, 239, 959, 0x00, 0x00, 0x00);    // far right hand vertical line, black, 1 pixel line. (this line hidden!)
    DrawLine(0, 958, 239, 958, 0x00, 0x00, 0x00);    // (this line hidden!)
    DrawLine(0, 957, 239, 957, 0x00, 0x00, 0x00);
    DrawLine(0, 956, 239, 956, 0x00, 0x00, 0x00);
    DrawLine(0, 955, 239, 955, 0x00, 0x00, 0x00);
    DrawLine(0, 954, 239, 954, 0x00, 0x00, 0x00);
    DrawLine(0, 953, 239, 953, 0x00, 0x00, 0x00);
    DrawLine(0, 952, 239, 952, 0x00, 0x00, 0x00);
    DrawLine(0, 951, 239, 951, 0x00, 0x00, 0x00);

    ClearScreen();                  // Again.....

    // For 34401A sniffing: clear any pending EXTI flag for PB13 SCK
    __HAL_GPIO_EXTI_CLEAR_IT(FP_SCK_Pin);

    //**************************************************************************************************
    // Main loop initialize

    while (1) {         // While loop running continious, full speed

        // service decoder as fast as possible
        for (int i = 0; i < 5000; i++)
            Decoder34401_Process();

        HAL_GPIO_TogglePin(GPIOC, TEST_OUT_Pin); // Test LED toggle

        //DisplayMain();

        //HAL_Delay(10);

        //DisplayAnnunciators();

        //HAL_Delay(10);
    }

}


// System Clock Configuration
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
    RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

    //Initializes the RCC Oscillators according to the specified parameters in the RCC_OscInitTypeDef structure.
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    // Initializes the CPU, AHB and APB buses clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
        | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == FP_SCK_Pin)
    {
        Decoder34401_SckEdge();
    }
}


// This function is executed in case of error occurrence.
void Error_Handler(void) {
    // User can add his own implementation to report the HAL error return state
    __disable_irq();
    while (1)
    {
    }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
