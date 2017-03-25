
#ifndef StreamAudioVideoFromSD_h
#define StreamAudioVideoFromSD_h

#include <Arduino.h>
//#include <SD.h>

#define MOTOR_DUTY        OCR0B
#define LED_DUTY          OCR0A

const int PIN_LED = 11;
//const int PIN_AUDIO = 10;

#define CIRCULAR_BUFFER_SIZE 512
  /* HANDLE_TAGS - This options allows proper playback of WAV files with embedded metadata*/
#define HANDLE_TAGS



/****************** ADVANCED USER DEFINES ********************************
   See https://github.com/TMRh20/StreamAudioVideoFromSD/wiki for info on usage


   /* Enables 16-bit samples, which can be used for stereo playback, or to produce a
     pseudo 16-bit output.                                                   */
#define STEREO_OR_16BIT

   /* In Normal single track mode, MODE2 will modify the behavior of STEREO or 16BIT output
        With MODE2 off, stereo tracks will be played using 2 pins, for 2 speakers in non-complimentary mode (pin to ground)
      With MODE2 on, stereo tracks will be played using 4 pins, for 2 speakers in complimentary mode
    In MULTI dual track mode, MODE2 will use a second timer for the second track.
      With MODE2 off, each track will use a separate pin, but the same timer
                                                                             */
class StreamAudioVideoFromSD {

private:
  unsigned int sampleRate;
  unsigned int resolution;
  int streamMaximum;

public:
  
  StreamAudioVideoFromSD();
  void readAudioVideoFromSD();
    boolean wavInfo(char* filename);
  void play(char* filename, unsigned long seekPoint=0);
};

#endif




