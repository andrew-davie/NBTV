
// Arduino Mechanical Televisor
// Andrew Davie, March 2017

#include "televisor.h"
#include "StreamAudioVideoFromSD.h"
#include "pidv2.h"


StreamAudioVideoFromSD wav;

volatile double PID_desiredError, PID_motorDuty, PID_currentError;
PID rpmPID(&PID_currentError, &PID_motorDuty, &PID_desiredError,-1,0,0,DIRECT);

//------------------------------------------------------------------------------------------------------

void setup() {

  #ifdef DEBUG
    Serial.begin(9600);
    while (!Serial) {
      SysCall::yield();
    }
    Serial.println(F("Televisor Active!"));
  #endif

  #ifdef NEXTION
    NextionUiSetup();
  #endif
    
  setupIRComparator();
  setupMotorPWM();
  setupFastPwm();

  wav.play("22050c.wav");

  setupPID();

//  wav.play("porridge3.wav");
}

void loop() {
  #ifdef NEXTION
    NextionUiLoop();
  #endif

  extern volatile boolean IR;
  if (IR) {
    IR = false;
//    Serial.println(PID_currentError);
  }
}

