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

volatile double motorDutyWhole = 0;
volatile double motorDutyFrac = 0;

volatile unsigned long timeDiff = 0;
volatile unsigned long lastDetectedIR = 0;
volatile boolean IR = false;
volatile double delta = 0;
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

    MOTOR_DUTY = 55; //(byte)(PID_motorDuty+0.5);
  }
}


double _Kp = 10;
double _Ki = 0;
double _Kd = 0.01;
double _pre_error = 0;
double _integral = 0;

double error;
double Pout;
double Iout;
double derivative;
double Dout;
double output;

   //pid(((double)(timeDiff-lastDetectedIR))/100.,0,PID_currentError); //(int)(PID_motorDuty+0.5);
 

double pid( double _dt, double setpoint, double pv ) {
    
    error = setpoint - pv;
    Pout = _Kp * error;
    _integral += error * _dt;
    Iout = _Ki * _integral;
    derivative = (error - _pre_error) / _dt;
    Dout = _Kd * derivative;
    output = Pout + Iout + Dout;

    if( output > 255 )
        output = 255;
    else if( output < 0 )
        output = 0;

    // Save error to previous error
    _pre_error = error;
    return output;
}

