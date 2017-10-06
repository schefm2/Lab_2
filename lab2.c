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

    Extra Credit things:
    We added an extra LED and pushbutton. This means the values the player is
    asked to convert actually get past 7, which means they get to actually
    interact with hex, not just binary and decimal.
    We also have a score calculation based on time taken to answer, with
    wrong answers penalized.
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
void ADC_Init();						//Initialize the ADC converter
void Timer0_ISR(void) __interrupt 1;	//Increments T0_overflows

void Game_Start(void);					//Opening print statements and instructions
void Mode_Select(void);					//Selects which game mode is to be played,
										//And sets game speed
void The_Seeder(void);					//Generates a pseudorandom seed for rand()
unsigned char random(void);				//Generates a random integer 0-15
void Hex_To_Bin(void);					//Runs game mode with button inputs
void Bin_To_Hex(void);					//Runs game mode with terminal input
void manipulateLEDs(void);              //Read player button pushes, light LEDs
unsigned int enteredBinary(void);       //Calculates answer from pushbuttons/LEDs

//Converts Analog Pot signal to Digital
unsigned char read_AD_input(unsigned char pin_number);

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

//sbits for input devices
__sbit __at 0xA0 PB0;	//Pushbutton configured for P2.0
__sbit __at 0xA1 PB1;	//Pushbutton configured for P2.1
__sbit __at 0xA2 PB2;	//Pushbutton configured for P2.2
__sbit __at 0xA3 PB3;	//Pushbutton configured for P2.3
__sbit __at 0xA4 SS1;	//Pushbutton configured for P2.4
__sbit __at 0xA5 SS2;	//Pushbutton configured for P2.5

//sbits for output devices
__sbit __at 0xB0 LED0;	//LED configured for P3.0
__sbit __at 0xB1 LED1;	//LED configured for P3.1
__sbit __at 0xB2 LED2;	//LED configured for P3.2
__sbit __at 0xB3 LED3;	//LED configured for P3.3
__sbit __at 0xB4 BLED1;	//BILED configured for P3.4 & P3.6
__sbit __at 0xB6 BLED2;	//BILED configured for P3.6 & P3.4
__sbit __at 0xB5 BUZZ;	//Buzzer configured for P3.5

float wait_converter;	//Allows wait_time to function properly 

unsigned int T0_overflows;      //Timer 0 overflow count
unsigned int sub_count;         //used to refer to time started, like in debouncing
unsigned int score;             //score for the game
unsigned int wait_time;         //time between questions, determined by A/DC

unsigned char SS1MEM;           //state of slide switch 1
unsigned char SS2MEM;           //state of slide switch 2
unsigned char toConvert;        //the number we give player to convert
unsigned char rounds;           //what round we're on
unsigned char num_right;        //number of rounds correctly answered
unsigned char bin_val_sub;      //number in binary submitted

//**************
void main(void)
{
    Sys_Init();			//System initilization
    Port_Init();		//Initialize ports 1, 2, 3
    Interrupt_Init();	//Enable Timer0 interrupts
    Timer_Init();		//Initialize Timer0
    ADC_Init();			//Initialize ADC Converter (must be after Timer_Init())
    putchar(' ');		//Mystery magic

    The_Seeder();		//Generates pseudorandom seed for rand()

    //begins infinite loop
    while(1)
    {
        Game_Start();
        Mode_Select();
    }

}

//**********************************
void Port_Init(void)
{
    P3MDOUT |= 0x7F;	//Sets P3.0 - P3.6 as outputs

    P2MDOUT &= 0xC0;	//Sets P2.0 - P2.5 as inputs
    P2 |= ~0xC0;		//Sets high impedance for P2.0 - P2.5

    P1MDOUT &= 0xFD;	//Sets P1.1 as input
    P1 |= ~0xFD;		//Sets high impedance P1.1 
    P1MDIN &= 0xFD;		//Sets P1.1 as analog input
}

//**********************************
void Timer_Init(void)
{
    CKCON |= 0x08;  // Timer0 uses SYSCLK as source
    TMOD &= 0xF0;   // clear the 4 least significant bits
    TMOD |= 0x01;   // Timer0 in mode 1 (16 bit)
    TR0 = 0;		// Stop Timer0
    TMR0 = 0;		// Clear high & low byte of T0
}

//**********************************
void Interrupt_Init()
{
    EA = 1;
    ET0 = 1;
}

//**********************************
void ADC_Init(void)
{
    REF0CN = 0x03;	//Sets V_ref as 2.4V
    ADC1CN = 0x80;	//Enables AD/C converter

    //Gives capacitors in A/D converter time to charge
    T0_overflows = 0;		//Ensures overflow counter starts at 0
    TR0 = 1;				//Starts Timer0
    while(T0_overflows<20);	//Waits for ~60 ms
    TR0 = 0;				//Pauses Timer0

    //Resets timer0 and overflow counter
    TMR0 = 0;
    T0_overflows = 0;

    //Sets gain to 1
    ADC1CF |= 0x01;
    ADC1CF &= 0xFD;
}

//**********************************
void Timer0_ISR(void) __interrupt 1
{
    T0_overflows++;		//Increments timer0 overflow count
}

//**********************************
void Game_Start(void)
{
    //Sets all LEDs, BLED, and BUZZ off
    LED0 = 1;
    LED1 = 1;
    LED2 = 1;
    LED3 = 1;
    BLED1 = 1;
    BLED2 = 1;
    BUZZ = 1;

    //Beginning game instructions
    printf("To select the Bin to Hex game mode, move the leftmost slide switch to the left.\r\n");
    printf("To select the Hex to Bin game mode, move the leftmost slide switch to the right.\r\n");
    printf("Rotate the blue potentiometer clockwise to speed up the wait time for a game round.\r\n");
    printf("When you are ready to start the game, flip the slideswitch on the right.\r\n");

    SS2MEM = SS2;			//Stores position of enter slideswitch
    while (SS2 == SS2MEM);	//Waits until the enter slideswitch is flipped to start the game
}

void Mode_Select(void)
{
    /*
       The read_AD_input will return a value between
       0 and 255 inclusive. The desired range of
       wait time is 500 ms to 5000 ms. Since each
       overflow of our Timer0 last ~2.96 ms, this
       means we need overflow values from 168.75
       to 1687.5, or 169 to 1688 approximately.
       AD_Convert is turned into a percentage. At
       100 percent, wait_time will be 1519 + 169 = 1688,
       and at 0 percent wait_time will be 0 + 169 = 169. 
     */
    wait_converter = ((float)read_AD_input(1) / 255) * 1519 + 169;
	wait_time = wait_converter;

    if(SS1)
    {
        //Runs Hex to Bin game mode
        Hex_To_Bin();
    }
    else
    {
        //Runs Bin to Hex game mode
        Bin_To_Hex();
    }
}

//**********************************
void The_Seeder(void)
{
    SS1MEM = SS1;			//Records state of SS1
    TR0 = 1;				//Starts Timer0
    printf("\r\nPlease turn the leftmost slide switch to generate a random game\r\n");
    while(SS1 == SS1MEM);	//Waits until slide switch is used
    TR0 = 0;				//Stops Timer0
    srand(T0_overflows);	//Uses overflow counter to seed srand

    //Resets overflow timer and Timer0
    T0_overflows = 0;
    TMR0 = 0;
}


//**********************************
unsigned char random(void)
{
    //Returns a random char 0 to 15 inclusive
    return (rand() % 15);
}

//**********************************
unsigned char read_AD_input(unsigned char pin_number)
{
    AMX1SL = pin_number;
    ADC1CN &= ~0x20;			//Clears the A/D conversion complete bit
    ADC1CN |= 0x10;				//Starts A/D conversion
    while(!(ADC1CN & 0x20));	//Waits until conversion completes 
    return ADC1;
}

//**********************************
void Hex_To_Bin(void)
{
    printf("\r\nConvert from Hex values to Binary!\r\n");
    printf("Use the push buttons to light up the LEDs; lit LED = 1, unlit = 0.\r\n");
    printf("When ready, flip the enter switch!\r\n");

    //Init some vars
    score = 0;
    num_right = 0;
    rounds = 0;

    while (rounds++ < 8) //increments round after reading
    {
        //Round setup: 
        // turn off LEDs, BiLED, clear timer, store switch state
        LED0 = 1; //off
        LED1 = 1;
        LED2 = 1;
        LED3 = 1;
        BLED1 = 1; //both sides powered == off
        BLED2 = 1;
		TR0 = 0;	//pause timer
        TMR0 = 0; //clear timer
        SS2MEM = SS2; //store switch state
        T0_overflows = 0;

        toConvert = random(); //generate random number
        printf("Convert %x\r\n", toConvert);

        TR0 = 1; //start timer

        //while (SS2 == SS2MEM && T0_overflows < wait_time)
        while (T0_overflows < wait_time)
            //not yet submitted, wait until out of time to answer
        {
            //player manipulates LEDs to get binary answer
            manipulateLEDs();
            if (SS2 != SS2MEM)
            {
                break;
            }
        }
        TR0 = 0; //pause
        score += T0_overflows;

        //since all vars are global, don't need to assign...
        enteredBinary();

        //if right, light BiLED green
        //if wrong, light BiLED red, add penalized score

        if ( bin_val_sub == toConvert )
            //answer is right
        {
            //color is hardware dependent
            //green
            BLED1 = 0;
            BLED2 = 1;

            ++num_right;
        }

        else
            //answer is wrong
        {
            //red
            BLED1 = 1;
            BLED2 = 0;

            //add penalized score
            score += 2 * wait_time;
        }

        //some time before next round starts
        sub_count = T0_overflows;
        TR0 = 1;
        while (T0_overflows < sub_count + 675) { }
        TR0 = 0;
    }

    printf("You've completed the game! Your score was %d: you answered %d out of 8 right.\r\n",
            score, num_right);

}

//**********************************
void Bin_To_Hex(void)
{
	
    for(unsigned char i=0; i<8; i++)
    {
        LED0 = 1; //off
        LED1 = 1;
        LED2 = 1;
        LED3 = 1;
        BLED1 = 1; //both sides powered == off
        BLED2 = 1;
        TR0 = 0;
        TMR0 = 0; //clear timer
        SS2MEM = SS2; //store switch state
        T0_overflows = 0;

        unsigned char input;
        unsigned char randNum = random(); //get random number to display
        unsigned char answer = randNum;
        printf("Convert %x\r\n", randNum);
        num_right = 0;

        
        for(unsigned char j=0; j<4; j++)
        {

            if((randNum%2)==1) //checks if last binary digit should be one
            {
                //if binaray digit being checked is one turn on corresponding LED
                if(j==0){LED3=0;}
                if(j==1){LED2=0;}
                if(j==2){LED1=0;}
                if(j==3){LED0=0;}
            }
            randNum = randNum>>;
        }
        TR0 = 1; //start timer
        input = getchar();
        TR0 = 0; //stop timer
        if(T0_overflows>wait_time)
        {
        }
        else
        {
            //check for ascii values 1-9
            if(int(input)>64)
            {
                //convert ascii of hex to decimal and compare to answer
                if((input-55== answer)
                {
                    //turn LED green
                    BLED1 = 0;
                    BLED2 = 1;
                    num_right++;
                    score +=  10 – (10*T0_overflows)/wait_time;

                }
                else
                {
                   //turn LED red
                    BLED1 = 1;
                    BLED2 = 0;
                }
            }

            //check for ascii values A-E
            else
            {
                //convert ascii of hex to decimal and compare to answer
                if((input-48)== answer)
                {
                    num_right++;
                    //turn LED green
                    BLED1=0;
                    BLED2=1;
                    score +=  10 – (10*T0_overflows)/wait_time;
                }
                else
                {
                    //turn LED red
                    BLED1 = 1;
                    BLED2 = 0;
                }
            }
        }

        //some time before next round starts
        TMR0 = 0; //clear timer
        T0_overflows=0;
        TR0 = 1;
        while (T0_overflows < 169) { }
        TR0 = 0;
        //turn off BILED
        BLED1=1;
        BLED2=1;
    }
    printf("You've completed the game! Your score was %d: you answered %d out of 8 right.\r\n",
            score, num_right);
    
    BUZZ=0;
    TR0 = 1;
    while (T0_overflows < 169) { }
    TR0 = 0;
    BUZZ=1;
}

//**********************************
void manipulateLEDs(void)
{
    if (!PB0)
    {
        sub_count = T0_overflows;
        while(T0_overflows < sub_count + 60) { }
        //debounce, don't want to change LED 100 times...
        LED0 = !LED0;
    }
    if (!PB1)
    {
        sub_count = T0_overflows;
        while(T0_overflows < sub_count + 60) { }
        LED1 = !LED1;
    }
    if (!PB2)
    {
        sub_count = T0_overflows;
        while(T0_overflows < sub_count + 60) { }
        LED2 = !LED2;
    }
    if (!PB3)
    {
        sub_count = T0_overflows;
        while(T0_overflows < sub_count + 60) { }
        LED3 = !LED3;
    }
}

//**********************************
unsigned int enteredBinary(void)
{
    bin_val_sub = 0; //clear any old data
    //Add from largest bit to smallest bit.
	if (!LED0) {bin_val_sub+= 8;}
	if (!LED1) {bin_val_sub+= 4;}
	if (!LED2) {bin_val_sub+= 2;}
	if (!LED3) {bin_val_sub+= 1;}
    return bin_val_sub;
}
