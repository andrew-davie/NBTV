
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
extern double customContrast;       // 1.0 = normal
extern boolean customGamma;         // true = use gamma lookup table

NexSlider seekerSlider = NexSlider(0, 12, "seek");
NexSlider brightnessSlider = NexSlider(0, 8, "brightness");
NexSlider contrastSlider = NexSlider(0, 10, "contrast");
NexSlider volumeSlider = NexSlider(0, 11, "volume");
NexCheckbox gammaCheckbox = NexCheckbox(0, 3, "gamma");
NexButton stopButton = NexButton(0, 4, "stopButton");
NexText timePos = NexText(0, 9, "timePos");

NexTouch *nex_Listen_List[] = {
    &stopButton,
    &seekerSlider,
    &contrastSlider,
    &brightnessSlider,
    &gammaCheckbox,
    &volumeSlider,
    NULL
};

void brightnessCallback(void *ptr) {
  NexSlider *slider = (NexSlider *) ptr;
  uint32_t bright;
  if (slider->getValue(&bright))
    customBrightness = (int)(256.*(bright-50.)/50.);
}

void contrastCallback(void *ptr) {
  NexSlider *slider = (NexSlider *) ptr;
  uint32_t contrast;
  slider->getValue(&contrast);
  customContrast = (double)(contrast/50.0);       // range: 0 - 2.0
}

boolean stopped = false;

void stopButtonCallback(void *ptr) {
  NexButton *stopButton = (NexButton *) ptr;
  stopped = !stopped;
//  Serial.print(F("STOP = "));
//  Serial.println(stopped);
}

void volumeCallback(void *ptr) {
  NexSlider *slider = (NexSlider *) ptr;
  uint32_t volume;
  slider->getValue(&volume);
//  Serial.print(F("VOLUME ="));
//  Serial.println(volume);
}

void seekerCallback(void *ptr) {
  NexSlider *slider = (NexSlider *) ptr;
  uint32_t seekPosition;
  slider->getValue(&seekPosition);
//  Serial.print(F("SEEK ="));
//  Serial.println(seekPosition);
}

void gammaCallback(void *ptr) {
  NexCheckbox *checkbox = (NexCheckbox *) ptr;
  uint32_t gamma;
  checkbox->getValue(&gamma);
  customGamma = (gamma!=0);
//  Serial.print(F("GAMMA ="));
//  Serial.println(gamma);
}

void NextionUiSetup(void) {
  nexInit();
  seekerSlider.attachPop(seekerCallback, &seekerSlider);
  brightnessSlider.attachPop(brightnessCallback, &brightnessSlider);
  contrastSlider.attachPop(contrastCallback, &contrastSlider);
  volumeSlider.attachPop(volumeCallback, &volumeSlider);
  gammaCheckbox.attachPop(gammaCallback, &gammaCheckbox);
  stopButton.attachPop(stopButtonCallback, &stopButton);
}

void NextionUiLoop(void) {
  nexLoop(nex_Listen_List);
}

#endif








