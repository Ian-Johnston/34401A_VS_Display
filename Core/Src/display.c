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
	char textBeforeDP[16];
	char textBetween[16];
	char textAfterComma[16];
	char textAfterDP[16];

	uint16_t blink_mask = dmm_blink_mask;
	static uint32_t last_blink_ms = 0;
	static uint8_t blink_phase = 1;
	uint32_t now = HAL_GetTick();

	int i;
	int dp_pos = -1;
	int comma_pos = -1;
	int unit_pos = 9;

	// check for non ASCII chars and make local copy
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

	// find comma before shifting units
	for (i = 0; text[i] != 0; i++)
	{
		if (text[i] == ',')
		{
			comma_pos = i;
			break;
		}
	}

	if (comma_pos >= 0)
		unit_pos = 10;     // comma adds one character before the units

	ShiftUnitsRight1At(text, unit_pos);		// Shift last 4 chars to the right by 1 char if match criteria
	ShiftUnitsRight2At(text, unit_pos);		// Shift last 4 chars to the right by 2 chars if match criteria
	FixUnitText(text);						// Fix units

	SetTextColors(MainColourFore, BackgroundColour);
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
		Xpos_MAIN,
		Ypos_MAIN
	);

	// find decimal point and comma after unit shifting
	dp_pos = -1;
	comma_pos = -1;

	// find decimal point and comma
	for (i = 0; text[i] != 0; i++)
	{
		if (text[i] == '.')
			dp_pos = i;

		if (text[i] == ',')
			comma_pos = i;
	}

	// detect if DP changed and if so erase artefact
	if (dp_pos != last_dp_pos)
	{
		int dp_x2 = 70;
		int dp_y2 = Ypos_MAIN + (dp_pos * MAIN_CHAR_ADVANCE) + 2;

		DrawLine(dp_x2, dp_y2 + 0, dp_x2 + 100, dp_y2 + 0, 0x00, 0x00, 0x00);
		DrawLine(dp_x2, dp_y2 + 1, dp_x2 + 100, dp_y2 + 1, 0x00, 0x00, 0x00);
		DrawLine(dp_x2, dp_y2 + 2, dp_x2 + 100, dp_y2 + 2, 0x00, 0x00, 0x00);
		DrawLine(dp_x2, dp_y2 + 3, dp_x2 + 100, dp_y2 + 3, 0x00, 0x00, 0x00);
		DrawLine(dp_x2, dp_y2 + 4, dp_x2 + 100, dp_y2 + 4, 0x00, 0x00, 0x00);
		DrawLine(dp_x2, dp_y2 + 5, dp_x2 + 100, dp_y2 + 5, 0x00, 0x00, 0x00);

		DrawLine(dp_x2, dp_y2 + 6, dp_x2 + 100, dp_y2 + 6, 0x00, 0x00, 0x00);
		DrawLine(dp_x2, dp_y2 + 7, dp_x2 + 100, dp_y2 + 7, 0x00, 0x00, 0x00);
		last_dp_pos = dp_pos;
	}

	// detect if comma changed and if so erase artefact
	if (comma_pos != last_comma_pos)
	{
		int comma_x2 = 70;
		int comma_y2 = Ypos_MAIN + ((comma_pos - 1) * MAIN_CHAR_ADVANCE) + COMMA_DOT_OFFSET;

		DrawLine(comma_x2, comma_y2 + 0, comma_x2 + 100, comma_y2 + 0, 0x00, 0x00, 0x00);
		DrawLine(comma_x2, comma_y2 + 1, comma_x2 + 100, comma_y2 + 1, 0x00, 0x00, 0x00);
		DrawLine(comma_x2, comma_y2 + 2, comma_x2 + 100, comma_y2 + 2, 0x00, 0x00, 0x00);
		DrawLine(comma_x2, comma_y2 + 3, comma_x2 + 100, comma_y2 + 3, 0x00, 0x00, 0x00);
		DrawLine(comma_x2, comma_y2 + 4, comma_x2 + 100, comma_y2 + 4, 0x00, 0x00, 0x00);
		DrawLine(comma_x2, comma_y2 + 5, comma_x2 + 100, comma_y2 + 5, 0x00, 0x00, 0x00);
		DrawLine(comma_x2, comma_y2 + 6, comma_x2 + 100, comma_y2 + 6, 0x00, 0x00, 0x00);

		DrawLine(comma_x2, comma_y2 + 7, comma_x2 + 100, comma_y2 + 7, 0x00, 0x00, 0x00);
		DrawLine(comma_x2, comma_y2 + 8, comma_x2 + 100, comma_y2 + 8, 0x00, 0x00, 0x00);
		DrawLine(comma_x2, comma_y2 + 9, comma_x2 + 100, comma_y2 + 9, 0x00, 0x00, 0x00);

		DrawLine(comma_x2, comma_y2 + 10, comma_x2 + 100, comma_y2 + 10, 0x00, 0x00, 0x00);
		DrawLine(comma_x2, comma_y2 + 11, comma_x2 + 100, comma_y2 + 11, 0x00, 0x00, 0x00);
		last_comma_pos = comma_pos;
	}

	// Case: decimal point and comma present
	if (dp_pos >= 0 && comma_pos > dp_pos)
	{
		// text before decimal point
		memcpy(textBeforeDP, text, dp_pos);
		textBeforeDP[dp_pos] = 0;

		// text between decimal point and comma
		memcpy(textBetween, &text[dp_pos + 1], comma_pos - dp_pos - 1);
		textBetween[comma_pos - dp_pos - 1] = 0;

		// text after comma
		strcpy(textAfterComma, &text[comma_pos + 1]);

		WaitForTextReady();
		DrawText(textBeforeDP);

		// draw decimal point manually
		{
			int dp_x = 143;
			int dp_y = Ypos_MAIN + (dp_pos * MAIN_CHAR_ADVANCE);

			DrawLine(dp_x, dp_y + 0, dp_x + 10, dp_y + 0, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 1, dp_x + 10, dp_y + 1, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 2, dp_x + 10, dp_y + 2, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 3, dp_x + 10, dp_y + 3, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 4, dp_x + 10, dp_y + 4, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 5, dp_x + 10, dp_y + 5, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 6, dp_x + 10, dp_y + 6, 0xFF, 0xFF, 0xFF);

			DrawLine(dp_x, dp_y + 7, dp_x + 10, dp_y + 7, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 8, dp_x + 10, dp_y + 8, 0xFF, 0xFF, 0xFF);
		}

		// text after decimal point, before comma
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
			Xpos_MAIN,
			Ypos_MAIN + (dp_pos * MAIN_CHAR_ADVANCE) + DP_GAP
		);

		WaitForTextReady();
		DrawText(textBetween);

		// draw comma manually
		{
			int comma_x = 143;
			int comma_y = Ypos_MAIN + ((comma_pos - 1) * MAIN_CHAR_ADVANCE) + COMMA_DOT_OFFSET;

			DrawLine(comma_x, comma_y + 0, comma_x + 15, comma_y + 0, 0xFF, 0xFF, 0xFF);
			DrawLine(comma_x, comma_y + 1, comma_x + 15, comma_y + 1, 0xFF, 0xFF, 0xFF);
			DrawLine(comma_x, comma_y + 2, comma_x + 15, comma_y + 2, 0xFF, 0xFF, 0xFF);

			DrawLine(comma_x, comma_y + 3, comma_x + 15, comma_y + 3, 0xFF, 0xFF, 0xFF);

			DrawLine(comma_x, comma_y + 4, comma_x + 10, comma_y + 4, 0xFF, 0xFF, 0xFF);
			DrawLine(comma_x, comma_y + 5, comma_x + 10, comma_y + 5, 0xFF, 0xFF, 0xFF);
			DrawLine(comma_x, comma_y + 6, comma_x + 10, comma_y + 6, 0xFF, 0xFF, 0xFF);
			DrawLine(comma_x, comma_y + 7, comma_x + 10, comma_y + 7, 0xFF, 0xFF, 0xFF);
			DrawLine(comma_x, comma_y + 8, comma_x + 10, comma_y + 8, 0xFF, 0xFF, 0xFF);

			DrawLine(comma_x, comma_y + 9, comma_x + 10, comma_y + 9, 0xFF, 0xFF, 0xFF);
		}

		// text after comma
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
			Xpos_MAIN,
			Ypos_MAIN + ((comma_pos - 1) * MAIN_CHAR_ADVANCE) + COMMA_GAP
		);

		WaitForTextReady();
		DrawText(textAfterComma);
	}

	// Case: decimal point only
	else if (dp_pos >= 0)

	{
		memcpy(textBeforeDP, text, dp_pos);
		textBeforeDP[dp_pos] = 0;

		strcpy(textAfterDP, &text[dp_pos + 1]);

		WaitForTextReady();
		DrawText(textBeforeDP);

		// draw decimal point manually
		{
			int dp_x = 143;
			int dp_y = Ypos_MAIN + (dp_pos * MAIN_CHAR_ADVANCE);

			DrawLine(dp_x, dp_y + 0, dp_x + 10, dp_y + 0, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 1, dp_x + 10, dp_y + 1, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 2, dp_x + 10, dp_y + 2, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 3, dp_x + 10, dp_y + 3, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 4, dp_x + 10, dp_y + 4, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 5, dp_x + 10, dp_y + 5, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 6, dp_x + 10, dp_y + 6, 0xFF, 0xFF, 0xFF);

			DrawLine(dp_x, dp_y + 7, dp_x + 10, dp_y + 7, 0xFF, 0xFF, 0xFF);
			DrawLine(dp_x, dp_y + 8, dp_x + 10, dp_y + 8, 0xFF, 0xFF, 0xFF);
		}

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
			Xpos_MAIN,
			Ypos_MAIN + (dp_pos * MAIN_CHAR_ADVANCE) + DP_GAP
		);

		WaitForTextReady();
		DrawText(textAfterDP);
	}

	// Case: no decimal point
	else

	{
		WaitForTextReady();
		DrawText(text);
	}
}


// Shift chars right by 1
// Enter the original four chars to be shifted
void ShiftUnitsRight1At(char* text1, int pos)
{
	static const char* unit4[] = {
		" VDC", "mVDC", " OHM", "KOHM", "MOHM", " ADC", "mADC", " AAC", "mAAC", "uSEC", "mSEC", " SEC", "mMIN", "mMAX", "mVAC", " VAC", " dBm",
		"END1"
	};

	for (int u = 0; strcmp(unit4[u], "END1") != 0; u++) {
		const char* p = unit4[u];

		if (text1[pos] == p[0] &&
			text1[pos + 1] == p[1] &&
			text1[pos + 2] == p[2] &&
			text1[pos + 3] == p[3]) {

			text1[pos + 4] = text1[pos + 3];
			text1[pos + 3] = text1[pos + 2];
			text1[pos + 2] = text1[pos + 1];
			text1[pos + 1] = text1[pos];
			text1[pos] = ' ';
			return;
		}
	}
}


// Shift chars right by 2
// Enter the original four chars to be shifted
void ShiftUnitsRight2At(char* text1, int pos)
{
	static const char* unit4[] = {
		" HZ ", "KHZ ", " dB ",
		"END2"
	};

	for (int u = 0; strcmp(unit4[u], "END2") != 0; u++) {
		const char* p = unit4[u];

		if (text1[pos] == p[0] &&
			text1[pos + 1] == p[1] &&
			text1[pos + 2] == p[2] &&
			text1[pos + 3] == p[3]) {

			text1[pos + 5] = '\0';
			text1[pos + 4] = text1[pos + 2];
			text1[pos + 3] = text1[pos + 1];
			text1[pos + 2] = text1[pos];
			text1[pos + 1] = ' ';
			text1[pos] = ' ';
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
		3,    // SMP
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

	// Digital input B0 high/open = SHIFT enabled, B0 low = hide SHIFT annunciator
	//uint8_t showShift = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_SET);
	uint8_t showShift = 0;

	for (int i = 0; i < 15; i++)
	{
		uint8_t annOn = ((dmm_ann_state & (uint16_t)(1U << i)) != 0);

		// Suppress SHIFT annunciator if B0 tied low
		if (i == 11 && !showShift)
			annOn = 0;

		if (annOn)
		{
			//	if (i == 9)											// ERROR				bug here, red artefacts appear to left of "CONT"
			//		SetTextColors(AnnunColourForeRed, 0x000000);
			//	else if (i >= 12)									// DIODE, CONT, 4Wire
			//		SetTextColors(AnnunColourForeYel, 0x000000);
			//	else
			SetTextColors(AnnunColourFore, 0x000000);
		}
		else
		{
			SetTextColors(0x000000, 0x000000);
		}

		int xpos = Xpos_ANNUNC;

		// Move DIODE, CONT, 4Wire left
		if (i >= 12)   // indices 12,13,14
			xpos = Xpos_ANNUNC - 178;		// coord height adjust

		ConfigureFontAndPosition(
			0b00,    // Internal CGROM
			0b00,    // Font size
			0b00,    // ISO 8859-1
			0,       // Full alignment enabled
			0,       // Chroma keying disabled
			1,       // Rotate 90 degrees counterclockwise
			0b01,    // Width multiplier
			0b10,    // Height multiplier
			5,       // Line spacing
			0,       // Character spacing
			xpos,     // Cursor X
			AnnuncYCoords[i]     // Cursor Y
		);

		WaitForTextReady();

		DrawText(AnnuncNames[i]);
	}
}
