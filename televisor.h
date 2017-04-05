// Arduino Micro Mechanical Televisor
// Andrew Davie, March 2017
// Use the code however you wish

#ifndef televisor_h
#define televisor_h


/*
 * Arduino Mechanical TV
 * 
 * This code allows an Arduino Micro to act as a Mechanical Television.
 * A mechanically (scanned) televisino consists of a Nipkow disc (a spinning disc with a spiral of tiny holes)
 * in front of a light source. The light intensity is varied in synchronisation with the spinning of the disc, and
 * shines through the spiral holes as they "scan" over the light. Persistance of vision allows us to see a moving
 * 'televisor' picture. The modern Narrow Bandwidth Television (NBTV) standard is 12.5 frames per second, 32
 * scanlines.
 * This implementation plays NBTV video files stored on a SD card as WAV files. The left channel holds the video
 * information, and the right channel holds mono sound. The code supports a variety of frequencies, but 22050 Hz and
 * 44100 Hz are reasonable choices.
 * To build a televisor and use this code, you need a few very simple external circuits, specifically
 *   1. IR transmitter and detector
 *   2. Motor control
 *   3. LED driver
 *   4. SD card
 * For a complete build history see https://www.taswegian.com/NBTV/forum/viewforum.php?f=28
 */

#define DEBUG


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

// IFF the circular buffer is a power of 2 in size (it doesn't have to be) then we can define a mask to make the code
// slightly faster. Disable the following line if the buffer size is NOT a power of two, otherwise code will fail.
// Finally, the mask is OPTIONAL so use it only if you want "efficient" buffer wrapping. In quotes, because the code
// compiles longer with the ANDing option. Bizarre.

//#define CIRCULAR_BUFFER_MASK (CIRCULAR_BUFFER_SIZE-1)

//-----------------------------------------------------------------------------------------


//#define SHOW_WAV_STATS


#endif



