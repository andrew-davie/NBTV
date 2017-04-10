
#ifndef televisor_h
#define televisor_h


/*
 * Arduino Mechanical TV
 * by Andrew Davie
 * For a complete build history see https://www.taswegian.com/NBTV/forum/viewforum.php?f=28
 */

#define DEBUG
//#define NEXTION     
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
#define CIRCULAR_BUFFER_SIZE 256
#define STREAM_THRESHOLD 0          /* non-zero will delay streaming until this many bytes needed */
//#define BACKFILL_BUFFER             /* pads buffer with PREVIOUS scanline pixels as it goes */

//-----------------------------------------------------------------------------------------


//#define SHOW_WAV_STATS


#endif



