
#ifndef televisor_h
#define televisor_h


/*
 * Arduino Mechanical TV
 * by Andrew Davie
 * For a complete build history see https://www.taswegian.com/NBTV/forum/viewforum.php?f=28
 */

#define DEBUG

    // >>> Don't forget to open the serial terminal if DEBUG outputs are active! <<<



#define NEXTION     
#define SDX

// SDCARD...
// Wiring:
// grey     GND
// purple   pin 4 selector
// blue     

//-----------------------------------------------------------------------------------------
// Arduino Micro pin usage for Televisor...

//  PIN       USE
//  3         Motor control duty
//  4         selector pin for SD
//  13        LED control           written by WAV code (VERY fast PWM)
//  10        AUDIO out????
//  7 (PE)    IR sensor input       analog comparitor
//-----------------------------------------------------------------------------------------


#define MOTOR_DUTY        OCR0B
#define LED_DUTY          OCR0A


//-----------------------------------------------------------------------------------------
#define CIRCULAR_BUFFER_SIZE 512
// buffer size must be a multiple of sample size (i.e, (video + audio) * bytes per)) = 4 for 16-bit, and 2 for 8-bit

//-----------------------------------------------------------------------------------------


#define SHOW_WAV_STATS


#endif



