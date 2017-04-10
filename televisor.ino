
// Arduino Mechanical Televisor
// Andrew Davie, March 2017

#include "televisor.h"
#include "StreamAudioVideoFromSD.h"
#include <PID_v1.h>


StreamAudioVideoFromSD wav;

volatile double PID_desiredError, PID_motorDuty, PID_currentError;
PID rpmPID(&PID_currentError, &PID_motorDuty, &PID_desiredError,100,100,500,DIRECT);

//------------------------------------------------------------------------------------------------------

void setup() {

//    delay(3000);

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
  setupPID();
  setupFastPwm();

  wav.play("22050c.wav");
//  wav.play("porridge3.wav");
}

void loop() {
  #ifdef NEXTION
    NextionUiLoop();
  #endif

  extern volatile boolean IR;
  extern volatile long showMe;
  if (IR) {
    IR = false;
    Serial.print(PID_currentError);
    Serial.print(",");
    Serial.println(PID_motorDuty);
  }


}







