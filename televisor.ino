// Arduino Mechanical Televisor
// Andrew Davie, March 2017

#include "televisor.h"
#include "StreamAudioVideoFromSD.h"
#include <SdFat.h>
#include <PID_v1.h>


File nbtv;
SdFat nbtvSD;
StreamAudioVideoFromSD wav;

volatile double PID_desiredRPM, PID_motorDuty;
volatile double PID_currentRPM;
PID rpmPID(&PID_currentRPM, &PID_motorDuty, &PID_desiredRPM,2,0,0, DIRECT);

//------------------------------------------------------------------------------------------------------

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
//  wav.play("5.wav");
  
}

extern volatile unsigned long lastDetectedIR;
int ircount = 0;

void loop() {
  wav.readAudioVideoFromSD();  

/*  extern volatile boolean IR;
  if (IR) {
    IR = false;
    Serial.print("speed ");
    Serial.println(PID_currentRPM);
    
  }
 */
//  Serial.println(PID_currentRPM);
}






