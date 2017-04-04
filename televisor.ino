
// Arduino Mechanical Televisor
// Andrew Davie, March 2017

#include "televisor.h"
#include "StreamAudioVideoFromSD.h"
#include <SdFat.h>
#include <PID_v1.h>

//#include <SoftwareSerial.h>
#include <Nextion.h>
//#define nextion Serial1

//Nextion myNextion(nextion, 9600); //create a Nextion object named myNextion using the nextion serial port @ 9600bps


File nbtv;
SdFat nbtvSD;
StreamAudioVideoFromSD wav;

volatile double PID_desiredRPM, PID_motorDuty;
volatile double PID_currentRPM;
PID rpmPID(&PID_currentRPM, &PID_motorDuty, &PID_desiredRPM,10,1,25, DIRECT);

//------------------------------------------------------------------------------------------------------

void setup() {

  #ifdef DEBUG
    Serial.begin(9600);
    Serial1.begin(9600);      // Nextion
    
    while (!Serial) {
      SysCall::yield();
    }
  #endif
  
  setupIRComparator();
  setupPID();
  setupMotorPWM();
  setupFastPwm();
  setupSdCard();

//  myNextion.init();

  wav.play("19200.wav");
  //wav.play("porridge.wav");
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

//  String message = myNextion.listen(); //check for message
//  if(message != ""){ // if a message is received...
//    Serial.println(message); //...print it out
//  }
}







