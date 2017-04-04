const unsigned int PIN_MOTOR = 3;
const unsigned int RPM = 1764;


//------------------------------------------------------------------------------------------------------
// PWM to control motor speed

void setupMotorPWM() {

  pinMode(PIN_MOTOR, OUTPUT);


  // If we're using an IRL540 MOSFET for driving the motor, then it's possible for the floating pin to cause the motor to spin
  // So we make sure the pin is set LOW ASAP.
  
  digitalWrite(PIN_MOTOR,LOW);
  
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
          | bit(CS02)            // prescalar %010 = /1 = 16000000/256/1 = 62.5K
          //| bit(CS00)
          ;

  MOTOR_DUTY = 255;           // start motor spinning so PID can kick in in interrupt
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




