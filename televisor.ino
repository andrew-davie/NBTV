
// Arduino Mechanical Televisor
// Andrew Davie, March 2017

#include "televisor.h"
#include "StreamAudioVideoFromSD.h"
#include <PID_v1.h>

//#include <SoftwareSerial.h>
//SoftwareSerial NexSerial(0,1);// RX, TX

StreamAudioVideoFromSD wav;

volatile double PID_desiredRPM, PID_motorDuty;
volatile double PID_currentRPM;
PID rpmPID(&PID_currentRPM, &PID_motorDuty, &PID_desiredRPM,10,1,25, DIRECT);

//------------------------------------------------------------------------------------------------------

void setup() {

  #ifdef DEBUG
    Serial.begin(9600);
    
    while (!Serial) {
      SysCall::yield();
    }
  #endif

  NextionUiSetup();
    
  setupIRComparator();
  setupPID();
  setupMotorPWM();
  setupFastPwm();

  wav.play("22050c.wav");       // this conflicts with NEXTION
//  wav.play("porridge3.wav");       // this conflicts with NEXTION
}

void loop() {
  wav.readAudioVideoFromSD();  
  //NextionUiLoop();
}







