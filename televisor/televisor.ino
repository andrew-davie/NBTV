// PWM testbed
// Creates a PWM on an output pin

void setup() {

  // Only enable the pin that's being used. Here are all the options for the Micro though...
  pinMode(3, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(13, OUTPUT);

  // Setup the PWM
  // WGM02|WGM01|WGM00  = waveform generation 
  //                    = fast PCM with OCRA controlling the counter maximum
  // CS01               = clock scalar /8
  // COMA               = toggle on compare match (01)

  TCCR0A = _BV(COM0A0) | _BV(COM0B1) | _BV(WGM01) | _BV(WGM00);
  TCCR0B =  _BV(WGM02) | _BV(CS01);

  OCR0A = 99;          // the compare limit (reduce this for higher frequency PWM)
  
  // frequency  = 16MHz/scalar/(OCR0A+1)
  //            = 16000000/8/256
  //            = 7812 Hz
  
  // if we change the scalar to 1 and OCR0A to 255...
  // frequency = 16000000/1/256
  //                 = 62.5KHz

  // if we change the scalar to 8 and OCR0A to 99...
  // frequency = 16000000/8/100
  //                 = 20KHz

  // and note, Output A is fixed frequency - use B instead
  // Output B is based on the duty cycle written to OCR0B;  i.e., (OCRB+1)/(OCRA+1)%
  
  OCR0B = 0;            // start with duty 0%

  // PWW is already running at this point. 
  // Code after here is just testing it works

  // Debug
  Serial.begin(9600);
  while (! Serial);
  Serial.println("Duty?");
}

int lastval=0;

void loop() {
  // Testing - set the duty value from 0 to OCR0A
  if (Serial.available()) {
    int duty = Serial.parseInt();
    OCR0B = duty;
    Serial.println(duty);
  }

  int val = analogRead(0);    // read the input pin
  if (val!=lastval) {
    Serial.println(val);             // debug value
    lastval = val;
  }
}
