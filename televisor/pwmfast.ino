/**********************************************************
                Fast PWM Test
Demostrates the generation of high speed PWM
using timers 1 and 4
There are two pieces of code:
One for pins 9, 10 and 11 using TIMER 1
with frequencies up to 62kHz
Other for pins 6 and 13 using TIMER 4
with frequencies up to 187kHz
History:
  12/12/2014 - Version 1.0
  22/12/2014 - Adding a missing OCR4C value
  
***********************************************************/

/**********************************************************
   Fast PWM on pins 9,10,11 (TIMER 1)
   
   Do not use analogWrite to pins 9, 10 or 11 if using 
   this functions as they use the same timer.
   
   Those functions will probably conflict with the 
   servo library.
   
   Uses 5 PWM frequencies between 61Hz and 62.5kHz
   
**********************************************************/

// Frequency modes for TIMER1
#define PWM62k   1   //62500 Hz
#define PWM8k    2   // 7812 Hz
#define PWM1k    3   //  976 Hz
#define PWM244   4   //  244 Hz
#define PWM61    5   //   61 Hz



/**********************************************************
   Fast PWM on pins 6, 13 (High Speed TIMER 4)
   
   Do not use analogWrite to pins 6 or 13 if using 
   this functions as they use the same timer.
   
   Those functions will conflict with the 
   MSTIMER2 library.
   Uses 7 PWM frequencies between 2930Hz and 187.5kHz
   
   Timer 4 uses a PLL as input so that its clock frequency
   can be up to 96MHz on standard Arduino Leonardo.
   We limit imput frequency to 48MHz to generate 187.5kHz PWM
   If needed, we can double that up to 375kHz
**********************************************************/

// Frequency modes for TIMER4
#define PWM187k 1   // 187500 Hz
#define PWM94k  2   //  93750 Hz
#define PWM47k  3   //  46875 Hz
#define PWM23k  4   //  23437 Hz
#define PWM12k  5   //  11719 Hz
#define PWM6k   6   //   5859 Hz
#define PWM3k   7   //   2930 Hz

// Direct PWM change variables
#define PWM6        OCR4D
#define PWM13       OCR4A

// Terminal count
#define PWM6_13_MAX OCR4C

// Configure the PWM clock
// The argument is one of the 7 previously defined modes

void pwm613configure(int mode) {

  TCCR4A=0;
  TCCR4B=mode;
  TCCR4C=0;
  TCCR4D=0;

  // PLL Configuration
  // Use 96MHz / 2 = 48MHz
  PLLFRQ=(PLLFRQ&0xCF)|0x30;
// PLLFRQ=(PLLFRQ&0xCF)|0x10; // Will double all frequencies

  OCR4C=255;      // Terminal count for Timer 4 PWM
}

// Set PWM to D6 (Timer4 D)
// Argument is PWM between 0 and 255
//void pwmSet6(int value) {
//  OCR4D=value;   // Set PWM value
//  DDRD|=1<<7;    // Set Output Mode D7
//  TCCR4C|=0x09;   // Activate channel D
//}

// Set PWM to D13 (Timer4 A)
// Argument is PWM between 0 and 255
//void pwmSet13(int value) {
//  OCR4A=value;   // Set PWM value
//  DDRC|=1<<7;    // Set Output Mode C7
//  TCCR4A=0x82;  // Activate channel A
//}

/*************** ADDITIONAL DEFINITIONS ******************/

// Macro to converts from duty (0..100) to PWM (0..255)
//#define DUTY2PWM(x)  ((255*(x))/100)

/**********************************************************/

// Start of demostration code
// Generates 4 PWM signals, two constant and two variable

void setupFastPwm()
{

// Configure Timer 4 (Pins 6 and 13)
// It will operate at 187kHz
// Valid options are:
//     PWM187k, PWM94k, PWM47k, PWM23k, PWM12k, PWM6k and PWM3k
pwm613configure(PWM187k);

// Generate PWM at pin 13 with 75% duty
// We need to call pwm613configure
// We don't here the DUTY2PWM macro
// We set a PWM value (0..255) instead
//pwmSet13(192);

// Prepare pin 6 to use PWM
// We need to call pwm613configure
// For now, we set it at 0%
//pwmSet6(0);
}


