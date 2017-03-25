// Arduino Mechanical Televisor
// Andrew Davie, March 2017



#include "televisor.h"
#include "StreamAudioVideoFromSD.h"
#include <SdFat.h>
#include <PID_v1.h>

#define RPM 3000


File myFileX;
SdFat SDX;
StreamAudioVideoFromSD wav;

double brightness = 128;


const int PIN_MOTOR = 3;

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

  //36
  //mutr07
  //who
  setupSD2();
//  wav.play("22050c.wav");
  wav.play("1.wav");
  
  MOTOR_DUTY = 255;           // start motor spinning so PID can kick in in interrupt

}

//------------------------------------------------------------------------------------------------------
// Comparator interrupt for IR sensing
// seems to use TIMER1 resources

void setupIRComparator() {
  
  ACSR &= ~(
      bit(ACIE)         // disable interrupts on AC
    | bit(ACD)          // switch on the AC
    | bit(ACIS0)        // falling trigger
    );

  ACSR |=
      bit(ACIS1)
    | bit(ACIE)         // re-enable interrupts on AC
    ;
    
  SREG |= bit(SREG_I);        // GLOBALLY enable interrupts
}

//-- Analog comparator interrupt service routine -------------------------------------------------
// Triggered by sync hole on IR sensor connected to A0 pin
// @750Hz, all going well, but < 1KHz should be expected
// Note, until the motor moves off timer 0, the millis() value will be wrong, so speed calc is wrong
// TODO: move motor --> timer 3

volatile unsigned long timeDiff = 0;
volatile unsigned long lastDetectedIR = 0;

// Appears to co-exist with timer 1 ICP (interrupt) so there goes that timer.

volatile int icr;

ISR(ANALOG_COMP_vect) {

  // TODO: millis() is not returning expected value due to prescalar
  //  prolly have to use the frequency(20kHz) as an adjustment.

//  icr = ICR1;
//  Serial.println(icr);
  
  timeDiff = millis();
  if (timeDiff-lastDetectedIR > 10)
    PID_currentRPM = 60. * (1000. / (timeDiff - lastDetectedIR));         // current RPM
  lastDetectedIR = timeDiff;  

  
}

//------------------------------------------------------------------------------------------------------
// PID to adjust motor speed

void setupPID() {
  PID_currentRPM = 0;
  PID_desiredRPM = RPM;       // kludged "about right" rpm based on incorrect speed calculations
  PID_motorDuty = 255;        // maximum duty
  
  rpmPID.SetOutputLimits(0, 255);
  rpmPID.SetMode(AUTOMATIC);              // turn on PID
}

//------------------------------------------------------------------------------------------------------
// PWM to control motor speed @ 20 kHz

void setupMotorPWM() {

  pinMode(PIN_MOTOR, OUTPUT);
//  pinMode(PIN_LED,OUTPUT);

/*  // Setup the PWM
  // WGM02|WGM01|WGM00  = waveform generation 
  //                    = fast PCM with OCRA controlling the counter maximum
  // CS01               = clock scalar /8
  // COMA               = toggle on compare match (01)

  TCCR0A = bit(COM0A0) | bit(COM0B1) | bit(WGM01) | bit(WGM00);
  TCCR0B =  bit(WGM02) | bit(CS01);

  OCR0A = 99;          // the compare limit (reduce this for higher frequency PWM)  - F = 16MHz/scalar/(OCR0A+1)
  
  // Output A is fixed frequency - use B instead
  // Output B is based on the duty cycle written to OCR0B;  i.e., (OCRB+1)/(OCRA+1)%
  
  OCR0B = 0;            // start with duty 0%
*/

/*  
 *   
 */// The timer runs as a normal PWM with two outputs - one for the motor and one for the LED
  // The prescalar is set to /1, giving a frequency of 16000000/256 = 62500 Hz
  // The motor will run off PIN 3 ("MOTOR_DUTY")
  // The LED will run off PIN 11 ("LED_DUTY")

  // see pp.133 ATmega32U4 data sheet for WGM table
  // %101 = fast PWM, 8-bit
  
  TCCR0A = 0
          | bit(COM0A1)           // COM %10 = non-inverted
          | bit(COM0B1)           // COM %10 = non-inverted
          | bit(WGM01)
          | bit(WGM00)
          ;
        
  TCCR0B = 0
          //| bit(WGM02)
          | bit(CS01)            // prescalar %010 = /1 = 16000000/256/1 = 62.5K
          ;

  MOTOR_DUTY = 0;                 // 0..255
}

//------------------------------------------------------------------------------------------------------

void loop() {
  wav.readAudioVideoFromSD();

/*  if ( Serial.available() > 0 ) {

    char key = Serial.read();
    if (key == '+') {
      Serial.print("brighter = ");
      brightness -=20;
      if (brightness<1)
        brightness = 1;
      Serial.println(brightness);
    }
    else if (key == '-') {
      Serial.print("darker =");
      brightness += 20;
      Serial.println(brightness);
    }
  }
*/

  // Only need to recalculate the PID
  rpmPID.Compute();
  MOTOR_DUTY = PID_motorDuty;         // PWM duty value for motor control
  
}






