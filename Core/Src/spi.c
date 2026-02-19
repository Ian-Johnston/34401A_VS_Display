/**
  ******************************************************************************
  * @file    spi.c
  * @brief   This file provides code for the configuration
  *          of the SPI instances.
  ******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "spi.h"

/* SPI Handles */
SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;


// SPI1 init function
void MX_SPI1_Init(void)                                     // LCD (LT7680)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;            // Full-duplex for 4-wire SPI
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;                // 8-bit data frame
    hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;             // Clock idle high according to LT7680 timing diagram
    hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;                  // Data captured on 2nd edge according to LT7680 timing diagram (CPOL=1)
    hspi1.Init.NSS = SPI_NSS_SOFT;                          // Software-controlled CS
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; // Prescaler for SPI speed default = 8 which is 9Mhz, well below LT7680 max of 50Mhz SPI CLK
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;                 // Most significant bit first
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;                 // No TI mode
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE; // No CRC calculation
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}


/*
void MX_SPI1_Init(void) {
    __HAL_RCC_SPI1_CLK_ENABLE();  // Enable SPI1 clock
    // Do not configure other settings
}
*/


// SPI MSP Init
void HAL_SPI_MspInit(SPI_HandleTypeDef* spiHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    if (spiHandle->Instance == SPI1)                           // LCD (LT7680)
    {
        /* SPI1 clock enable */
        __HAL_RCC_SPI1_CLK_ENABLE();

        /* Enable GPIOA clock */
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /** SPI1 GPIO Configuration
        PA5 ------> SPI1_SCK
        PA6 ------> SPI1_MISO
        PA7 ------> SPI1_MOSI
        PA4 ------> SPI1_NSS (CS, software-controlled)
        */
        // Configure SCK
        GPIO_InitStruct.Pin = SPI_SCK_PIN;     // PA5
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;       // Alternate function push-pull SPI functions
        GPIO_InitStruct.Pull = GPIO_NOPULL;           // Pull-up resistor disabled
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; // High speed for SPI
        HAL_GPIO_Init(SPI_SCK_PORT, &GPIO_InitStruct);

        // Configure MISO (SPI Master In Slave Out)
        GPIO_InitStruct.Pin = SPI_MISO_PIN;    // PA6
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;       // Input mode for MISO
        GPIO_InitStruct.Pull = GPIO_PULLUP;           // Pull-up resistor enabled for input
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; // High speed for SPI
        HAL_GPIO_Init(SPI_MISO_PORT, &GPIO_InitStruct);

        // Configure MOSI (SPI Master Out Slave In)
        GPIO_InitStruct.Pin = SPI_MOSI_PIN;    // PA7
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;       // Alternate function push-pull SPI functions
        GPIO_InitStruct.Pull = GPIO_NOPULL;           // Pull-up resistor disabled
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; // High speed for SPI
        HAL_GPIO_Init(SPI_MOSI_PORT, &GPIO_InitStruct);

        // Configure CS (NSS) as general-purpose output
        GPIO_InitStruct.Pin = SPI_CS_PIN;      // PA4
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;   // Output mode
        GPIO_InitStruct.Pull = GPIO_NOPULL;           // Pull-up resistor disabled
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; // High speed for SPI
        HAL_GPIO_Init(SPI_CS_PORT, &GPIO_InitStruct);

        /* SPI1 DMA Init */
        hdma_spi1_tx.Instance = DMA1_Channel3;
        hdma_spi1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_spi1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_spi1_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_spi1_tx.Init.Mode = DMA_NORMAL;
        hdma_spi1_tx.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_spi1_tx) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_LINKDMA(spiHandle, hdmatx, hdma_spi1_tx);
    }

}

/* SPI MSP DeInit */
void HAL_SPI_MspDeInit(SPI_HandleTypeDef* spiHandle)
{
    if (spiHandle->Instance == SPI1)                            // LCD (LT7680)
    {
        /* SPI1 clock disable */
        __HAL_RCC_SPI1_CLK_DISABLE();

        /** SPI1 GPIO Configuration
        PA5 ------> SPI1_SCK
        PA6 ------> SPI1_MISO
        PA7 ------> SPI1_MOSI
        PA4 ------> SPI1_NSS (CS)
        */
        HAL_GPIO_DeInit(SPI_SCK_PORT, SPI_SCK_PIN | SPI_MOSI_PIN | SPI_MISO_PIN | SPI_CS_PIN);

        /* SPI1 DMA DeInit */
        HAL_DMA_DeInit(spiHandle->hdmatx);
    }

}


