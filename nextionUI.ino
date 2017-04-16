
#include "televisor.h"
#ifdef NEXTION

// Note: Changes in Arduino library
// 0: URL: https://github.com/itead/ITEADLIB_Arduino_Nextion
// 1: RAM/Memory usage - linker problem? see: http://support.iteadstudio.com/support/discussions/topics/11000012238
//    --> Delete NexUpload.h and NexUpload.cpp from Nextion library folder
// 2. Disable DEBUG_SERIAL_ENABLE in NexConfig.h
// 3. set   #define nexSerial Serial1 in NexConfig.h


//#define NEXTION_SERIAL_REDEFINE
//#ifdef NEXTION_SERIAL_REDEFINE
//  #include <SoftwareSerial.h>
//  SoftwareSerial NexSerial(11,10);// RX, TX
//#endif

#include "NexSlider.h"
#include "NexText.h"
#include "NexButton.h"
#include "NexCheckbox.h"
#include "NexText.h"

extern int customBrightness;        // 0 = normal
extern long customContrast2;       // binary fraction 128 = 1.0 = normal
extern boolean customGamma;         // true = use gamma lookup table

extern unsigned long videoSampleLength;
uint32_t lastSeekPosition = 0;

NexSlider seekerSlider = NexSlider(0, 12, "seek");
NexSlider brightnessSlider = NexSlider(0, 8, "brightness");
NexSlider contrastSlider = NexSlider(0, 10, "contrast");
NexSlider volumeSlider = NexSlider(0, 11, "volume");
NexCheckbox gammaCheckbox = NexCheckbox(0, 3, "gamma");
NexButton stopButton = NexButton(0, 4, "stopButton");
NexText timePos = NexText(0, 9, "timePos");


boolean stopped = false;

//void stopButtonCallback(void *ptr) {
//  NexButton *stopButton = (NexButton *) ptr;
//  stopped = !stopped;
////  Serial.print(F("STOP = "));
////  Serial.println(stopped);
//}

//void volumeCallback(void *ptr) {
//  NexSlider *slider = (NexSlider *) ptr;
//  uint32_t volume;
//  slider->getValue(&volume);
////  Serial.print(F("VOLUME ="));
////  Serial.println(volume);
//}

//void seekerCallback(void *ptr) {
//  NexSlider *slider = (NexSlider *) ptr;
//  uint32_t seekPosition;
//  slider->getValue(&seekPosition);
////  Serial.print(F("SEEK ="));
////  Serial.println(seekPosition);
//}



void NextionUiSetup(void) {
  nexInit();
}

int phase = 0;
long lastSeconds = 0;

void NextionUiLoop(void) {

  uint32_t value;

  switch (phase++) {
    case 0:
      if (gammaCheckbox.getValue(&value))
        customGamma = (value!=0);
      break;
    case 1:  
      if (brightnessSlider.getValue(&value))
        customBrightness = (int)(256.*(value-128.)/128.);
      break;
    case 2:
      if (contrastSlider.getValue(&value))
        customContrast2 = value << 1;
      break;
    default:
      phase = 0;
      break;
  }
  
  // Adjust the seekbar position to the current playback position
  long seekPosition = 256*(double)playbackAbsolute/(double)videoSampleLength;
  if (seekPosition != lastSeekPosition) {
    seekerSlider.setValue(seekPosition);
    lastSeekPosition = seekPosition;
  }

  // Display elapsed time in format MM:SS
  long seconds = playbackAbsolute / singleFrame / 12.5;
  if (seconds != lastSeconds) {
    lastSeconds = seconds;

    int s = seconds % 60;
    int m = seconds / 60;
  
    char times[6];
    times[0] = '0'+m/10;
    times[1] = '0'+m%10;
    times[2] = ':';
    times[3] = '0'+s/10;
    times[4] = '0'+s%10;
    times[5] = 0;

    timePos.setText(times);
  }
}

#endif








