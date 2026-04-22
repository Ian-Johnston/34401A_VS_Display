/**
  ******************************************************************************
  * @file    display.c
  * @brief   This file provides code for the
  *          display MAIN, AUX, ANNUNCIATORS & SPLASH.
  ******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "spi.h"
#include "main.h"
#include "timer.h"
#include "lcd.h"
#include "lt7680.h"
#include "display.h"
#include <string.h>  // For strchr, strncpy
#include <stdio.h>   // For debugging (optional)
#include "decoder_34401a.h"

//#define DURATION_MS 5000     // 5 seconds in milliseconds
#define TIMER_INTERVAL_MS 35 // The interval of your timed sub in milliseconds

volatile char tft_main_debug[16];

// Display colours default
uint32_t MainColourFore = 0xFFFFFF;			// White FFFFFF
uint32_t AnnunColourFore = 0x00FF00;		// Green 00FF00
uint32_t AnnunColourForeYel = 0xFFFF00;		// Yellow FFFF00
uint32_t AnnunColourForeRed = 0xFF0000;		// Red FF0000
uint32_t BackgroundColour = 0x000000;		// Black 000000
uint32_t SplashIanJColourFore = 0xFFFF00;	// Yellow FFFF00


//************************************************************************************************************************************************************


void WaitForTextReady(void)
{
	uint8_t Registerdata;

	WriteRegister(0xBA);                      // SPI Master Status Register
	Registerdata = ReadData();

	while (Registerdata & (1 << 6)) {         // Tx FIFO Full
		WriteRegister(0xBA);
		Registerdata = ReadData();
	}
}


void DisplayMain(void)
{
	char text[16];
	uint16_t blink_mask = dmm_blink_mask;
	static uint32_t last_blink_ms = 0;
	static uint8_t blink_phase = 1;
	uint32_t now = HAL_GetTick();

	// check for non ASCII chars and make local copy
	int i;
	for (i = 0; i < 15; i++)
	{
		unsigned char c = (unsigned char)dmm_main[i];
		text[i] = dmm_main[i];

		if (c == 0)
			break;

		if (c < 0x20 || c > 0x7E)
			return;
	}
	text[15] = 0;

	if ((now - last_blink_ms) >= 250)
	{
		last_blink_ms = now;
		blink_phase ^= 1;
	}

	if (!blink_phase)
	{
		for (i = 0; i < 14; i++)
		{
			if (blink_mask & (1U << i))
				text[i] = ' ';
		}
	}

	SetTextColors(MainColourFore, BackgroundColour); // Foreground, Background
	ConfigureFontAndPosition(
		0b00,    // Internal CGROM
		0b10,    // Font size
		0b00,    // ISO 8859-1
		0,       // Full alignment enabled
		0,       // Chroma keying disabled
		1,       // Rotate 90 degrees counterclockwise
		0b11,    // Width multiplier
		0b11,    // Height multiplier
		1,       // Line spacing
		4,       // Character spacing
		Xpos_MAIN,     // Cursor X
		Ypos_MAIN      // Cursor Y
	);

	ShiftUnitsRight1(text);		// Shift last 4 chars to the right by 1 char if match criteria

	ShiftUnitsRight2(text);		// Shift last 4 chars to the right by 2 chars if match criteria

	FixUnitText(text);			// Fix units

	FixMainText(text);			// Replace misc text

	WaitForTextReady();			// Wait for comms with LT7680A-R to complete before sending mext batch

	DrawText(text);				// Send to LT7680A-R
}


// Shift chars right by 1
// Enter the original four chars to be shifted
void ShiftUnitsRight1(char* text1)
{
	static const char* unit4[] = {
		" VDC", "mVDC", " OHM", "KOHM", "MOHM", " ADC", "mADC", " AAC", "mAAC", "uSEC", "mSEC", " SEC", "mMIN", "mMAX", "mVAC", " VAC", " dBm",
		"END1"
	};

	for (int u = 0; u < (int)(sizeof(unit4) / sizeof(unit4[0])); u++) {
		const char* p = unit4[u];

		if (text1[9] == p[0] &&
			text1[10] == p[1] &&
			text1[11] == p[2] &&
			text1[12] == p[3]) {

			text1[13] = text1[12];
			text1[12] = text1[11];
			text1[11] = text1[10];
			text1[10] = text1[9];
			text1[9] = ' ';
			return;
		}
	}
}


// Shift chars right by 2
// Enter the original four chars to be shifted
void ShiftUnitsRight2(char* text1)
{
	static const char* unit4[] = {
		" HZ ", "KHZ ", " dB ",
		"END2"
	};

	for (int u = 0; u < (int)(sizeof(unit4) / sizeof(unit4[0])); u++) {
		const char* p = unit4[u];

		if (text1[9] == p[0] &&
			text1[10] == p[1] &&
			text1[11] == p[2] &&
			text1[12] == p[3]) {

			text1[14] = '\0';      // remove trailing space
			text1[13] = text1[11];
			text1[12] = text1[10];
			text1[11] = text1[9];
			text1[10] = ' ';
			text1[9] = ' ';
			return;
		}
	}
}


// Replace characters
// Enter the before & after of the 4 char text to be replaced
void FixUnitText(char* text1)
{
	static const struct {
		const char from[5];   // 4 chars + '\0'
		const char to[5];     // 4 chars + '\0'
	} rules[] = {
		{ "MSEC", "mSEC" },
		{ "  HZ", "  Hz" },
		{ " KHZ", " kHz" },
		{ " MHZ", " MHz" },
		{ "  DB", "  dB" },
		{ "MVAC", "mVAC" },
		{ "MVDC", "mVDC" },
		{ "KOHM", "kohm" },
		{ " OHM", " ohm" },
		{ "GOHM", "Gohm" },
		{ "MOHM", "Mohm" },
		{ "MAAC", "mAAC" },
		{ "MADC", "mADC" },
		{ "UADC", "\xB5""ADC" }   // µADC (0xB5 in ISO-8859-1)
	};

	for (int i = 0; text1[i + 3] != '\0'; i++) {
		for (int r = 0; r < (int)(sizeof(rules) / sizeof(rules[0])); r++) {
			if (text1[i] == rules[r].from[0] &&
				text1[i + 1] == rules[r].from[1] &&
				text1[i + 2] == rules[r].from[2] &&
				text1[i + 3] == rules[r].from[3]) {

				text1[i] = rules[r].to[0];
				text1[i + 1] = rules[r].to[1];
				text1[i + 2] = rules[r].to[2];
				text1[i + 3] = rules[r].to[3];
				return; // only one unit expected
			}
		}
	}
}


// Replace text without moving the following text
// Replacement can only expand into existing spaces where there is room
void FixMainText(char* text1)
{
	static const struct {
		const char* from;
		const char* to;
	} rules[] = {
		{ "OVL.D", "OVERLOAD" },
		{ "O.VLD", "OVERLOAD" }
	};

	const int rowLen = 14;

	for (int i = 0; i < rowLen; i++) {
		for (int r = 0; r < (int)(sizeof(rules) / sizeof(rules[0])); r++) {

			int fromLen = (int)strlen(rules[r].from);
			int toLen = (int)strlen(rules[r].to);

			if (i + fromLen <= rowLen &&
				strncmp(&text1[i], rules[r].from, fromLen) == 0) {

				// find next non-space after the matched text
				int limit = i + fromLen;
				while (limit < rowLen && text1[limit] == ' ')
					limit++;

				// available space for replacement = up to start of next text block
				int avail = limit - i;

				// write replacement, but do not overwrite following text
				for (int j = 0; j < avail; j++) {
					if (j < toLen)
						text1[i + j] = rules[r].to[j];
					else
						text1[i + j] = ' ';
				}

				return;
			}
		}
	}
}


void DisplayAnnunciators(void)
{
	const char* AnnuncNames[15] = {
		"SMP", "ADRS", "RMT", "MAN", "TRIG",
		"HOLD", "MEM", "RATIO", "MATH", "ERROR",
		"REAR", "SHIFT", "DIODE", "CONT", "4Wire"
	};

	int AnnuncYCoords[15] = {
		3,   // SMP
		60,   // ADRS
		140,  // RMT
		210,  // MAN
		275,  // TRIG
		355,  // HOLD
		435,  // MEM
		500,  // RATIO
		600,  // MATH
		685,  // ERROR
		780,  // REAR
		860,  // SHIFT

		660,  // DIODE
		770,  // CONT
		860   // 4Wire
	};

	for (int i = 0; i < 15; i++)
	{
		//if (1)
		if (dmm_ann_state & (1U << i))
		{
			if (i == 9)   // ERROR
				SetTextColors(AnnunColourForeRed, 0x000000);
			else if (i >= 12)   // DIODE, CONT, 4Wire
				SetTextColors(AnnunColourForeYel, 0x000000);
			else
				SetTextColors(AnnunColourFore, 0x000000);
		}
		else
		{
			SetTextColors(0x000000, 0x000000);
		}

		int xpos = Xpos_ANNUNC;

		// Move DIODE, CONT, 4Wire left
		if (i >= 12)   // indices 12,13,14
			xpos = Xpos_ANNUNC - 160;		// coord

		ConfigureFontAndPosition(
			0b00,    // Internal CGROM
			0b00,    // Font size
			0b00,    // ISO 8859-1
			0,       // Full alignment enabled
			0,       // Chroma keying disabled
			1,       // Rotate 90 degrees counterclockwise
			0b01,    // Width multiplier
			0b01,    // Height multiplier
			5,       // Line spacing
			0,       // Character spacing
			xpos,     // Cursor X
			AnnuncYCoords[i]     // Cursor Y
		);

		WaitForTextReady();

		DrawText(AnnuncNames[i]);
	}
}

