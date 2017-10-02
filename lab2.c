/*  Names: Sydney Bahs, Tom Saad, Matthew Scheffer
    Section: 2
    Date: 10/10/17
    File name: lab2.c
    Description: This program runs a game with two game modes. A random number
	between 0 and 7 inclusive is generated, and this will either be displayed
	as a binary number with 3 LEDs or as a hex digit on the terminal. In the
	Bin -> Hex mode, the player must press the correct digit on the keyboard
	corresponding to the binary number displayed by the LEDs. In Hex -> Bin
	mode, the player must press the correct pushbuttons that will display the
	hex number properly on the LEDs as binary. The LEDs should light on or off
	to show what value is being input. When the player is finished inputing a
	binary number, they should press a 4th pushbutton that will tell the
	program that they locked in their final answer. A timer on the 8051 chip
	measures response time and scores depending on speed and accuracy. In the
	Hex -> Bin mode, the allowed time is controlled by a potentiometer reading.
	Each game should consist of 8 rounds of either mode. At the end of a game,
	the program goes back and displays the original instructions to the player,
	asking which mode they want to play and direction on how to continue.
*/

#include <c8051_SDCC.h> // include files. This file is available online
#include <stdio.h>
#include <stdlib.h>

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------
void Port_Init(void);					//Initialize ports for I/O
void Timer_Init(void);					//Initialize Timer0 w/SYSCLK and 16bit mode
void Interrupt_Init();					//Initialize interrupts for Timer0
void Timer0_ISR(void) __interrupt 1;	//Increments T0_overflows
unsigned char random(void);				//Generates a random integer 0-7
void Hex_To_Bin(void);					//Runs game mode with button inputs
void Bin_To_Hex(void);					//Runs game mode with terminal input
void AD_Convert(void);					//Converts Analog Pot signal to Digital

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

//sbits for input devices
__sbit __at 0xA0 PB0;	//Pushbutton configured for P2.0
__sbit __at 0xA1 PB1;	//Pushbutton configured for P2.1
__sbit __at 0xA2 PB2;	//Pushbutton configured for P2.2
__sbit __at 0xA3 PB3;	//Pushbutton configured for P2.3
__sbit __at 0xA4 SS;	//Pushbutton configured for P2.4

//sbits for output devices
__sbit __at 0xB0 LED0;	//LED configured for P3.0
__sbit __at 0xB1 LED1;	//LED configured for P3.1
__sbit __at 0xB2 LED2;	//LED configured for P3.2
__sbit __at 0xB3 LED3;	//LED configured for P3.3
__sbit __at 0xB4 BLED;	//BILED configured for P3.4
__sbit __at 0xB5 BUZZ;	//Buzzer configured for P3.5

unsigned int T0_overflows, sub_count, wait_time;