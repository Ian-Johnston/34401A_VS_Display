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

	// check for non ASCII chars
	int i;
	for (i = 0; dmm_main[i] != 0; i++)
	{
		unsigned char c = (unsigned char)dmm_main[i];

		if (c < 0x20 || c > 0x7E)
			return;
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

	ShiftUnitsRight1(dmm_main);		// Shift last 4 chars to the right by 1 char if match criteria

	ShiftUnitsRight2(dmm_main);		// Shift last 4 chars to the right by 2 chars if match criteria

	FixUnitText(dmm_main);			// Fix units

	WaitForTextReady();

	DrawText(dmm_main);
}


// Shift chars right by 1
// Enter the original four chars to be shifted
void ShiftUnitsRight1(char* text1)
{
	static const char* unit4[] = {
		" VDC", "mVDC", " OHM", "KOHM", "MOHM", " ADC", "mADC", " AAC", "mAAC", "uSEC", "mMIN", "mMAX",
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
		" HZ ", "KHZ ",
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
		{ "MSEC", "  ms" },
		{ " SEC", "   s" },
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


void DisplayAnnunciators(void)
{
	const char* AnnuncNames[15] = {
		"SMP", "ADRS", "RMT", "MAN", "TRIG",
		"HOLD", "MEM", "RATIO", "MATH", "ERROR",
		"REAR", "SHIFT", "DIODE", "CONT", "4W"
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

		700,  // DIODE
		810,  // CONT
		900   // 4W
	};

	for (int i = 0; i < 15; i++)
	{
		if (dmm_ann_state & (1U << i))
		{
			if (i == 9)   // ERROR
				SetTextColors(AnnunColourForeRed, 0x000000);
			else if (i >= 12)   // DIODE, CONT, 4W
				SetTextColors(AnnunColourForeYel, 0x000000);
			else
				SetTextColors(AnnunColourFore, 0x000000);
		}
		else
		{
			SetTextColors(0x000000, 0x000000);
		}

		int xpos = Xpos_ANNUNC;

		// Move DIODE, CONT, 4W left
		if (i >= 12)   // indices 12,13,14
			xpos = Xpos_ANNUNC - 160;

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


void DisplaySplash() {

	// IanJ
	SetTextColors(SplashIanJColourFore, BackgroundColour); // Foreground, Background
	ConfigureFontAndPosition(
		0b00,    // Internal CGROM
		0b00,    // Font size
		0b00,    // ISO 8859-1
		0,       // Full alignment enabled
		0,       // Chroma keying disabled
		1,       // Rotate 90 degrees counterclockwise
		0b00,    // Width multiplier
		0b01,    // Height multiplier
		1,       // Line spacing
		4,       // Character spacing
		Xpos_SPLASH,     // Cursor X
		Ypos_SPLASH      // Cursor Y
	);

	DrawText("Protocol by xi, TFT Upgrade by Ian Johnston") & '\0';

	HAL_Delay(10);

	// Main
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

	// Always draw exactly 14 characters (13 source + 1 added)
	char text1[15];   // 14 chars + terminator
	int i;

	// Copy the 13 source characters (displayWithPunct is always 13 chars)
	for (i = 0; i < 13; i++) {
		//		char c = displayWithPunct[i];
		//		text1[i] = (c == '\0') ? ' ' : c;
	}

	// Default: add trailing space as the 14th character
	text1[13] = ' ';
	text1[14] = '\0';

	memcpy(text1, "##############", 14);
	text1[14] = '\0';

	DrawText(text1);

	HAL_Delay(10);

	// Annunciators
	const char* AnnuncNames[12] = {
		"SMPL", "REM", "SRQ", "ADRS", "AC+DC", "4Wohm",
		"AZOFF", "MRNG", "MATH", "REAR", "ERR", "SHIFT"
	};

	// Set Y-position of the annunciators
	int AnnuncYCoords[12] = {
		10,   // SMPL
		87,   // REM
		151,  // SRQ
		212,  // ADRS
		289,  // AC+DC
		382,  // 4Wohm
		477,  // AZOFF
		571,  // MRNG
		649,  // MATH
		726,  // REAR
		803,  // ERR
		860   // SHIFT
	};

	for (int i = 0; i < 12; i++) {

		SetTextColors(AnnunColourFore, BackgroundColour); // ON
		ConfigureFontAndPosition(
			0b00,    // Internal CGROM
			0b00,    // 16-dot font size
			0b00,    // ISO 8859-1
			0,       // Full alignment enabled
			0,       // Chroma keying disabled
			1,       // Rotate 90 degrees counterclockwise
			0b01,    // Width X0
			0b01,    // Height X0
			5,       // Line spacing
			0,       // Character spacing
			Xpos_ANNUNC,
			AnnuncYCoords[i]
		);

		DrawText(AnnuncNames[i]);
		HAL_Delay(10);
	}

}