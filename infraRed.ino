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
volatile boolean IR = false;

// Appears to co-exist with timer 1 ICP (interrupt) so there goes that timer.

ISR(ANALOG_COMP_vect) {

  // TODO: millis() is not returning expected value due to prescalar
  //  prolly have to use the frequency(20kHz) as an adjustment.

  timeDiff = millis();
  if (timeDiff-lastDetectedIR > 10) {
    IR = true;
    PID_currentRPM = timeDiff - lastDetectedIR; //60. * (1000. / (timeDiff - lastDetectedIR));         // current RPM
    lastDetectedIR = timeDiff;  
  }
  
  // Since there's a new disc speed known, we can recalculate the desired motor speed
  rpmPID.Compute();
  MOTOR_DUTY = 59 ;//PID_motorDuty;         // PWM duty value for motor control  
}


