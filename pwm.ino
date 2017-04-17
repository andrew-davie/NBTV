
//------------------------------------------------------------------------------------------------------
// PWM to control motor speed

void setupMotorPWM() {

  #define PIN_MOTOR 3
  
  pinMode( PIN_MOTOR, OUTPUT );

  // If we're using an IRL540 MOSFET for driving the motor, then it's possible for the floating pin to cause
  // the motor to spin. So we make sure the pin is set LOW ASAP.
  
  digitalWrite( PIN_MOTOR, LOW );
  
  // The timer runs as a normal PWM with two outputs - one for the motor and one for the LED
  // The prescalar is set to /1, giving a frequency of 16000000/256 = 62500 Hz
  // The motor will run off PIN 3 ("MOTOR_DUTY")
  // The LED will run off PIN 11 ("LED_DUTY")  <<< NOT. LED runs from 

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

  OCR0B = 255;           // start motor spinning so PID can kick in in interrupt
}


//------------------------------------------------------------------------------------------------------
// PID to adjust motor speed

void setupPID() {
  
  PID_currentError = 0;
  PID_desiredError = 0; //singleFrame;
  PID_motorDuty = 255;                // maximum duty to kick-start motor and get IR fired up
  
  rpmPID.SetOutputLimits(0, 75);
  rpmPID.SetMode(AUTOMATIC);              // turn on PID
}

//-----------------------------------------------------------------------------------------------------
// Super dooper high speed PWM (187.5 kHz) using timer 4, used for the LED (pin 13) and audio (pin 6).
// Timer 4 uses a PLL as input so that its clock frequency can be up to 96 MHz on standard Arduino
// Leonardo/Micro. We limit imput frequency to 48 MHz to generate 187.5 kHz PWM. Can double to 375 kHz.

// Frequency modes for TIMER4
#define PWM187k 1   // 187500 Hz
#define PWM94k  2   //  93750 Hz
#define PWM47k  3   //  46875 Hz
#define PWM23k  4   //  23437 Hz
#define PWM12k  5   //  11719 Hz
#define PWM6k   6   //   5859 Hz
#define PWM3k   7   //   2930 Hz

void pwm613configure(int mode) {

  TCCR4A = 0;
  TCCR4B = mode;
  TCCR4C = 0;
  TCCR4D = 0;

  PLLFRQ=(PLLFRQ&0xCF)|0x30;      // Setup PLL, and use 96MHz / 2 = 48MHz
// PLLFRQ=(PLLFRQ&0xCF)|0x10;     // Will double all frequencies

  OCR4C = 255;                    // Target count for Timer 4 PWM
}

void setupFastPwm() {
  pwm613configure(PWM187k);       // configure timer 4 pins 6,13 to 187 kHz
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
// Triggered by sync hole on IR sensor connected to pin 7 (AC)

volatile unsigned long timeDiff = 0;
volatile unsigned long lastDetectedIR = 0;
volatile boolean IR = false;
//volatile double delta = 0;
volatile long desiredTime = 0;

// Appears to co-exist with timer 1 ICP (interrupt) so there goes that timer.

volatile double deltaSample;
extern long singleFrame;


ISR(ANALOG_COMP_vect) {

  timeDiff = playbackAbsolute;
  double deltaSample = timeDiff - lastDetectedIR;
  if (deltaSample > 1000) {         // cater for potential "bounce" on IR detect by not accepting closely spaced signals
    lastDetectedIR = timeDiff;

    desiredTime += singleFrame;

    interrupts();
    
    IR = true;

    PID_currentError = deltaSample;    
    rpmPID.Compute();

    OCR0B = 50; //(byte)(PID_motorDuty+0.5);        // write motor PWM duty
  }
}


