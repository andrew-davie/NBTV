// Arduino Mechanical Televisor
// Andrew Davie, March 2017

#include "televisor.h"
#include "StreamAudioVideoFromSD.h"
#include <SdFat.h>
#include <PID_v1.h>

#define RPM 3000


File nbtv;
SdFat nbtvSD;
StreamAudioVideoFromSD wav;


//-- PID --
double PID_desiredRPM, PID_motorDuty;
volatile double PID_currentRPM;
PID rpmPID(&PID_currentRPM, &PID_motorDuty, &PID_desiredRPM,25,1,0, DIRECT);

//------------------------------------------------------------------------------------------------------
// Setup...

void setup() {

  #ifdef DEBUG
    Serial.begin(9600);
    while (!Serial) {
      SysCall::yield();
    }
  #endif
  
  setupIRComparator();
  setupPID();
  setupMotorPWM();
  setupFastPwm();
  setupSdCard();

  wav.play("22050c.wav");
  
}


void loop() {
  wav.readAudioVideoFromSD();  
}






