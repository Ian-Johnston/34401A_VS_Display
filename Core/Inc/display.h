/**
  ******************************************************************************
  * @file    display.h
  * @brief   This file contains all the function prototypes for
  *          the display.c file
  ******************************************************************************
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stddef.h>
#include "main.h"

// External global variable
extern char G[48];
//extern _Bool Annunc[19];
extern uint32_t MainColourFore;
extern uint32_t AuxColourFore;
extern uint32_t AnnunColourFore;

// Function prototypes
void DisplayMain(void);
void DisplayAuxFirstHalf(void);
void DisplayAuxSecondHalf(void);
void DisplayAnnunciatorsHalf(void);


// Display coords
#define Xpos_MAIN				35			// These are actually the Y position because LCD is rotated 90deg in use. Values in pixels.
#define Ypos_MAIN				0			// start at far left
#define Xpos_ANNUNC				165
#define Xpos_SPLASH				142
#define Ypos_SPLASH				220


#endif // DISPLAY_H