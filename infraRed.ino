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
volatile double delta = 0;
volatile long desiredTime = 0;

// Appears to co-exist with timer 1 ICP (interrupt) so there goes that timer.

ISR(ANALOG_COMP_vect) {

  timeDiff = playbackAbsolute;
  if (timeDiff - lastDetectedIR > 1000) {         // cater for potential "bounce" on IR detect by not accepting closely spaced signals
    IR = true;

    long singleFrame = sampleRate * 2 * bitsPerSample / 8 / 12.5;
    desiredTime += singleFrame;  
    
    delta = (double)desiredTime - (double)timeDiff;
    PID_currentError = delta/100000.;

    lastDetectedIR = timeDiff;
  }
  rpmPID.Compute();
  MOTOR_DUTY = PID_motorDuty;

}

