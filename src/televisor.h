#ifndef _TELEVISOR_
#define _TELEVISOR_

// *************************** Arduino Library Support ************************************************************************
#include <Arduino.h>

// *************************** SD Card Library Support ************************************************************************
// Uses the SdFat Library at https://github.com/greiman/SdFat
// backup at http://www.taswegian.com/SdFat-master.zip

// *************************** Hardware details and settings  *****************************************************************
// Arduino pinout/connections:  https://www.taswegian.com/NBTV/forum/viewtopic.php?f=28&t=2298
//                              Pname  Description
#define PIN_MOTOR        3   // D3     Motor PWM Pin D3
#define SD_CS_PIN        4   // D4     Chip select for SD card   
#define PIN_SOUND_ENABLE 5   // D5     Enable sudio amp when high
//                           // D7     IR_IN, opto sensor - Analog comparator
#define DEBUG_PIN       12   // D12    Used for scoping performance
// digitalWrite(DEBUG_PIN, HIGH);
//                           // D13    Led PWM - PWM from TIMER4
// SD Card Clock             // SCK    SD card uses this pin
// SD Card Master In         // MISO   SD card uses this pin
// SD Card Master Out        // MOSI   SD card uses this pin
// To Nextion RX pin         // TX1    Send serial to Nextion
// To Nextion TX pin         // RX1    Get serial from Nextion

// Special pin defines
#define PWM187k 1   // 187500 Hz pwm frequency for TIMER4 on pin 

// *************************** CONFIGURATION... *******************************************************************************
// I think this doesnt work because either too much ram used or my fasrer round robin

#define CIRCULAR_BUFFER_SIZE 256    // must be a multiple of sample size, I think its the power of e.g. 128 256 512
// right now 256 is too much
// so 128 runs but no control work in play mode but they do work to select the song
// if buffer runs out you get small random lines in the image and out of frame
// (i.e, (video+audio)*bytes per)) = 4 for 16-bit, and 2 for 8-bit

// *************************** Tunings PID Motor speed and sync ***************************************************************
#define KP_SPEED          1.0F      // Speed proportional - this is how agressivly it moves towards and out of sycn frame - vertical
#define KP_FRAME          0.2F      // Frame proportional - this is how agressivly it moves towards and out of sycn frame - horizontal
#define KI_SPEED          0.01F     // Speed integral - slow/long term speed stability
#define MAX_FRAME_ERR    50         // if the PID error is smaller than this then frame locking will commence
#define LARGE_SPEED_ERR  50         // if speed error is large then turn off I (integral)

#include <SdFat.h>
#endif

