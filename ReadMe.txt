Sniffing idea originally from here:
https://github.com/openscopeproject/HP34401a-OLED-FW/

OLED version STM32 IO:
PB13 = CLK serial clock input
PB14 = DATA IN serial data line
PB15 = DATA OUT serial data line
PB12 = INT not used.

My TFT STM32 IO:
//==================================================================================================
// 34401A FRONT PANEL SNIFF PINS (as per decoder.cpp)
// PB13 = SCK (interrupt on rising edge)
// PB14 = Front panel data IN
// PB15 = Front panel data OUT
// PB12 = INT (optional, not required for basic sniffing)
//==================================================================================================
#define FP_INT_Pin         GPIO_PIN_12		// IGFPINT (pin 10 W601)
#define FP_SCK_Pin         GPIO_PIN_13		// IGFPSCK (pin 4 W601)
#define FP_DIN_Pin         GPIO_PIN_14		// IGFPDI (pin 6 W601)
#define FP_DOUT_Pin        GPIO_PIN_15		// FPD0 (pin 2 W601)
#define FP_GPIO_Port       GPIOB
#define FP_EXTI_IRQn       EXTI15_10_IRQn