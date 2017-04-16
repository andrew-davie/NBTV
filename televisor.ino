
// Arduino Mechanical Televisor
// Andrew Davie, March 2017

#include "televisor.h"
#include "StreamAudioVideoFromSD.h"
#include "pidv2.h"

volatile double PID_desiredError, PID_motorDuty, PID_currentError;
PID rpmPID(&PID_currentError, &PID_motorDuty, &PID_desiredError,-1,0,0,DIRECT);

//------------------------------------------------------------------------------------------------------

void setup() {

  #ifdef DEBUG

    // >>> Don't forget to open the serial terminal if DEBUG outputs are active! <<<
  
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
  setupStreamAudioVideoFromSD();
  

//  play("MUTR07x5.nbtv8.wav");
//  play("36b.nbtv8.wav");
//  play("40.nbtv8.wav");
//  play("45.nbtv8.wav");
  play("who.nbtv8.wav");
//  play("22050c.wav");
  
//  wav.play("whoclip4.wav");
//  wav.play("porridgeX.wav");
//  wav.play("19200.wav");
  
  setupPID();

}

void loop() {

  Serial.println("loop");
  
  #ifdef NEXTION
    NextionUiLoop();
  #endif

  extern volatile boolean IR;
//  if (IR) {
    IR = false;

    extern boolean alreadyStreaming;
    Serial.println(alreadyStreaming);
//  }
}

