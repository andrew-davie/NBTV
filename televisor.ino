//////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino Mechanical Televisor                                                                     //
// This program implements all the functions needed for a working mechanical televisor using an a   //
// Arduino Micro. For more information see https://www.taswegian.com/NBTV/forum/viewforum.php?f=28  //
// program by Andrew Davie (andrew@taswegian.com), March 2017                                       //
//////////////////////////////////////////////////////////////////////////////////////////////////////

// LIBRARIES...

// Uses the Nextion Library at https://github.com/itead/ITEADLIB_Arduino_Nextion
// backup at http://www.taswegian.com/ITEADLIB_Arduino_Nextion-master.zip
// 1: see: http://support.iteadstudio.com/support/discussions/topics/11000012238
//    --> Delete NexUpload.h and NexUpload.cpp from Nextion library folder
// 2. Disable DEBUG_SERIAL_ENABLE in NexConfig.h
// 3. set   #define nexSerial Serial1 in NexConfig.h

// Uses the SdFat Library at https://github.com/greiman/SdFat
// backup at http://www.taswegian.com/SdFat-master.zip

// HARDWARE...
// Arduino pinout/connections:  https://www.taswegian.com/NBTV/forum/viewtopic.php?f=28&t=2298


//////////////////////////////////////////////////////////////////////////////////////////////////////
// COMPILATION SWITCHES...

// DEBUG controls the output of diagnostic strings to the serial terminal.
// SHOULD NOT BE ENABLED FOR FINAL BUILD! Don't forget to open the serial terminal if DEBUG is active,
// because in this case the program busy-waits for the terminal availability before starting.

#define DEBUG                     // diagnostics to serial terminal (WARNING: busy-waits)
#define NEXTION                   // include Nextion LCD functionality
#define SDX                       // include SD card functionality
#define SHOW_WAV_STATS            // show the WAV file header details as it is loaded
#define PID                       // enable PID code

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CONFIGURATION...

#define CIRCULAR_BUFFER_SIZE 512  // must be a multiple of sample size
                                  // (i.e, (video+audio)*bytes per)) = 4 for 16-bit, and 2 for 8-bit

// Frequency modes for TIMER4
#define PWM187k 1   // 187500 Hz
#define PWM94k  2   //  93750 Hz
#define PWM47k  3   //  46875 Hz
#define PWM23k  4   //  23437 Hz
#define PWM12k  5   //  11719 Hz
#define PWM6k   6   //   5859 Hz
#define PWM3k   7   //   2930 Hz

//////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef NEXTION

#include "NexSlider.h"
#include "NexText.h"
#include "NexButton.h"
#include "NexCheckbox.h"

uint32_t lastSeekPosition = 0;
int phase = 0;
long lastSeconds = 0;

NexSlider seekerSlider = NexSlider(0, 12, "seek");
NexSlider brightnessSlider = NexSlider(0, 8, "brightness");
NexSlider contrastSlider = NexSlider(0, 10, "contrast");
NexSlider volumeSlider = NexSlider(0, 11, "volume");
NexCheckbox gammaCheckbox = NexCheckbox(0, 3, "gamma");
NexButton stopButton = NexButton(0, 4, "stopButton");
NexText timePos = NexText(0, 9, "timePos");

#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////

int customBrightness = 0;
boolean customGamma = true;
long customContrast2 = 256;      // a X.Y fractional  (8 bit fractions) so 256 = 1.0f   and 512 = 2.0f
long customVolume = 256;         // ditto


volatile byte circularAudioVideoBuffer[CIRCULAR_BUFFER_SIZE];
volatile unsigned int bitsPerSample;
volatile unsigned long playbackAbsolute;
volatile unsigned int pbp = 0;

unsigned long videoLength;
volatile unsigned long streamAbsolute;
volatile unsigned int bufferOffset = 0;
unsigned long sampleRate;
unsigned long singleFrame;

volatile unsigned long timeDiff = 0;
volatile unsigned long lastDetectedIR = 0;

boolean alreadyStreaming = false;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PID...

#ifdef PID

// PID (from http://brettbeauregard.com/blog/2011/04/improving-the-beginners-pid-introduction/)

double lastTime = 0;
double Input, Output, Setpoint;
double errSum = 0;
double lastErr;
double kp, ki, kd;

void Compute() {

  double now = ((double)playbackAbsolute) / singleFrame;
  double timeChange = now - lastTime;
  if (timeChange > 0) {
    
    double error = Setpoint - Input;
    errSum += (error * timeChange);
    double dErr = (error - lastErr) / timeChange;
    Output = kp * error + ki * errSum + kd * dErr;
  
    if (Output > 255)
      Output = 255;
    if (Output < 0)
      Output = 0;
    
    lastErr = error;
    lastTime = now;
  }
}
  
void SetTunings(double Kp, double Ki, double Kd) {
   kp = Kp;
   ki = Ki;
   kd = Kd;
}

#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef SDX
#include <SdFat.h>
File nbtv;
SdFat nbtvSD;
#endif


void play(char* filename, unsigned long seekPoint=0);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// The Arduino calls setup() once, and then loop() repeatedly

int startupPhase = 0;

void setup() {

#ifdef DEBUG
  Serial.begin(9600);
  while (!Serial)
    SysCall::yield();
  Serial.println(F("Televisor Active!"));
#endif

#ifdef NEXTION
  nexInit();              // Initialise Nextion LCD
#endif
    
  setupIRComparator();
  setupMotorPWM();
  setupFastPwm(PWM187k);
  setupStreamAudioVideoFromSD();
  
#ifdef PID
    SetTunings(1.5,0.5,1);
//  SetTunings(1.5,0.25,0.5);
//  SetTunings(2,0.1,1);      // quite good
#endif

  play((char *)"who.nbtv8.wav");
//  play((char *)"who.nbtv8.wav");
//  play((char *)"porridgeX.nbtv8.wav");
}


void loop() {
#ifdef NEXTION
  NextionUiLoop();
#endif

  if (startupPhase == 1) {
    startupPhase++;
  }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PWM to control motor speed

void setupMotorPWM() {

  #define PIN_MOTOR 3
  
  pinMode( PIN_MOTOR, OUTPUT );

  // If we're using an IRL540 MOSFET for driving the motor, then it's possible for the floating pin to cause
  // the motor to spin. So we make sure the pin is set LOW ASAP.
  
  digitalWrite( PIN_MOTOR, LOW );
  
  // The timer runs as a normal PWM with two outputs but we just use one for the motor
  // The prescalar is set to /1, giving a frequency of 16000000/256 = 62500 Hz
  // The motor will run off PIN 3 ("MOTOR_DUTY")
  // see pp.133 ATmega32U4 data sheet for WGM table.  %101 = fast PWM, 8-bit
  
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



//////////////////////////////////////////////////////////////////////////////////////////////////////
// Super dooper high speed PWM (187.5 kHz) using timer 4, used for the LED (pin 13) and audio (pin 6).
// Timer 4 uses a PLL as input so that its clock frequency can be up to 96 MHz on standard Arduino
// Leonardo/Micro. We limit imput frequency to 48 MHz to generate 187.5 kHz PWM. Can double to 375 kHz.
// ref: http://r6500.blogspot.com/2014/12/fast-pwm-on-arduino-leonardo.html

void setupFastPwm(int mode) {

  TCCR4A = 0;
  TCCR4B = mode;
  TCCR4C = 0;
  TCCR4D = 0;

  PLLFRQ=(PLLFRQ&0xCF)|0x30;      // Setup PLL, and use 96MHz / 2 = 48MHz
// PLLFRQ=(PLLFRQ&0xCF)|0x10;     // Will double all frequencies

  OCR4C = 255;                    // Target count for Timer 4 PWM
}

//------------------------------------------------------------------------------------------------------
// Comparator interrupt for IR sensing
// precludes timer 1 usage

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
// Triggered by sync hole detected by IR sensor, connected to pin 7 (AC)

long pbDelta = 0;

ISR(ANALOG_COMP_vect) {

  timeDiff = playbackAbsolute;
  double deltaSample = timeDiff - lastDetectedIR;
  if (deltaSample > 1000) {         // cater for potential "bounce" on IR detect by not accepting closely spaced signals
    lastDetectedIR = timeDiff;

    interrupts();

    pbDelta = playbackAbsolute % singleFrame;
    Serial.println(pbDelta);
    if (pbDelta > 45) {
        lastDetectedIR-= 1;
    }
#ifdef PID

    Setpoint = 0;
    Input = singleFrame-deltaSample;

    if (!startupPhase && Input > -90) {
      Serial.println("switch");
      startupPhase++;
    }
    
    if (startupPhase) {
      Compute();          // PID
      OCR0B = (byte)(Output+0.5);        // write motor PWM duty
    }
#else
    OCR0B = 53;
#endif


    // Now we are at the very start of a frame, so to get synchrnoisation we can adjust the playback offset
    // so that the video/audio is ALSO at the start of a frame.  Depending on the delta we might move
    // the playback pointer earlier or later. Mmh.

    // The playback TIME should be pretty much sacrosanct, but we fine-adjust.
    // So have a bit of 'leeway" with 0 being bang-on, but we can 'drift' a bit + or -

    

  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// NEXTION LCD...

#ifdef NEXTION

void NextionUiLoop(void) {

  uint32_t value;

  // Cycle through reading the controls only doing one of them each loop. Designed to reduce CPU load.

  switch (phase++) {
    case 0:
      if (gammaCheckbox.getValue(&value))
        customGamma = (value!=0);
      break;
    case 1:  
      if (brightnessSlider.getValue(&value))
        customBrightness = (int)(256.*(value-128.)/128.);
      break;
    case 2:
      if (contrastSlider.getValue(&value))
        customContrast2 = value << 1;
      break;
    case 3:
      if (volumeSlider.getValue(&value))
        customVolume = value << 1;
      break;
        
    default:
      phase = 0;
      break;
  }
  
  // Adjust the seekbar position to the current playback position
  uint32_t seekPosition = (playbackAbsolute << 8) / videoLength;      //256*(double)playbackAbsolute/(double)videoLength;
  if (seekPosition != lastSeekPosition) {
    seekerSlider.setValue(seekPosition);
    lastSeekPosition = seekPosition;
  }

  // Display elapsed time in format MM:SS
  long seconds = playbackAbsolute / singleFrame / 12.5;
  if (seconds != lastSeconds) {
    lastSeconds = seconds;

    int s = seconds % 60;
    int m = seconds / 60;
  
    char times[6];
    times[0] = '0'+m/10;
    times[1] = '0'+m%10;
    times[2] = ':';
    times[3] = '0'+s/10;
    times[4] = '0'+s%10;
    times[5] = 0;

    timePos.setText(times);
  }
}

#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////


// Streaming from SD card

// Gamma correction using parabolic curve
// Table courtesy Klaas Robers

const uint8_t PROGMEM gamma8[] = {
    0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,
    2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  5,  5,
    5,  5,  6,  6,  6,  6,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17,
   17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25, 25,
   26, 27, 27, 28, 29, 29, 30, 31, 31, 32, 33, 33, 34, 35, 36, 36,
   37, 38, 39, 39, 40, 41, 42, 42, 43, 44, 45, 46, 47, 47, 48, 49,
   50, 51, 52, 53, 54, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
   65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 78, 79, 80, 81,
   82, 83, 84, 85, 87, 88, 89, 90, 91, 92, 94, 95, 96, 97, 99,100,
  101,102,104,105,106,107,109,110,111,113,114,115,117,118,119,121,
  122,123,125,126,128,129,130,132,133,135,136,138,139,141,142,144,
  145,147,148,150,151,153,154,156,157,159,160,162,164,165,167,168,
  170,172,173,175,177,178,180,182,183,185,187,188,190,192,194,195,
  197,199,201,202,204,206,208,209,211,213,215,217,219,220,222,224,
  226,228,230,232,234,235,237,239,241,243,245,247,249,251,253,255
  };

void setupStreamAudioVideoFromSD() {

#define SD_CS_PIN 4
  pinMode(SS, OUTPUT);

#ifdef SDX
  if (!nbtvSD.begin(SD_CS_PIN)) {
#ifdef DEBUG
    Serial.println(F("SD failed!"));
#endif
  }
#endif
  
}


boolean wavInfo(char* filename) {

#ifdef SDX

  nbtv = nbtvSD.open(filename);
  if (!nbtv) {
    #if defined(DEBUG)
      Serial.print(F("Error when opening file '"));
      Serial.print(filename);
      Serial.print(F("'"));
    #endif
    return false;
  }

  char data[4];
  nbtv.read(data,4);

  if (strncmp(data,"RIFF",4)) {
    #ifdef SHOW_WAV_STATS
      Serial.println(F("Error: WAV has no RIFF header"));
    #endif
    return false;
  }

  long chunkSize;
  nbtv.read(&chunkSize,4);
  #ifdef SHOW_WAV_STATS
    Serial.print(F("WAV size: "));
    Serial.println(chunkSize+8);
  #endif
  
  nbtv.read(data,4);
  if (strncmp(data,"WAVE",4)) {
    #ifdef DEBUG
      Serial.println(F("Error: WAVE header incorrect"));
    #endif
    return false;
  }

  long position;
  while (true) {

    nbtv.read(data,4);        // read next chunk header

    #ifdef SHOW_WAV_STATS
      for (int i=0; i<4; i++){
        Serial.print(F("'"));
        Serial.print(data[i]);
        Serial.print(F("'"));
      }
      Serial.println();
    #endif
    
    if (!strncmp(data,"nbtv",4)) {
      #ifdef SHOW_WAV_STATS
        Serial.println(F("'nbtv' chunk"));
      #endif
      nbtv.read(&chunkSize,4);
      #ifdef SHOW_WAV_STATS
        Serial.print(F("size: "));
        Serial.println(chunkSize);
      #endif
      position = nbtv.position();
      nbtv.seek(position+chunkSize);
      continue;
    }

    if (!strncmp(data,"fmt ",4)) {
      #ifdef SHOW_WAV_STATS
        Serial.println(F("'fmt ' chunk"));
      #endif

      nbtv.read(&chunkSize,4);
      #ifdef SHOW_WAV_STATS
        Serial.print(F("Size: "));
        Serial.println(chunkSize);
      #endif
      
      position = nbtv.position();
      
      int audioFormat;
      nbtv.read(&audioFormat,2);
      #ifdef SHOW_WAV_STATS
        Serial.print(F("Audio format: "));
        Serial.println(audioFormat);
      #endif
      int numChannels;
      nbtv.read(&numChannels,2);
      #ifdef SHOW_WAV_STATS
        Serial.print(F("# chans: "));
        Serial.println(numChannels);
      #endif

      nbtv.read(&sampleRate,4);
      #ifdef SHOW_WAV_STATS
        Serial.print(F("rate: "));
        Serial.println(sampleRate);
      #endif
      long byteRate;
      nbtv.read(&byteRate,4);
      #ifdef SHOW_WAV_STATS
        Serial.print(F("byte rate: "));
        Serial.println(byteRate);
      #endif
      int blockAlign;
      nbtv.read(&blockAlign,2);
      #ifdef SHOW_WAV_STATS
        Serial.print(F("align: "));
        Serial.println(blockAlign);
      #endif
      nbtv.read((void *)&bitsPerSample,2);
      #ifdef SHOW_WAV_STATS
        Serial.print(F("bps: "));
        Serial.println(bitsPerSample);
      #endif
      
      // Potential "ExtraParamSize/ExtraParams" ignored because PCM

      nbtv.seek(position+chunkSize);
      continue;
    }

    if (!strncmp(data,"data",4)) {
      #ifdef SHOW_WAV_STATS
        Serial.println(F("data chunk"));
      #endif
      
      nbtv.read(&videoLength,4);
      #ifdef SHOW_WAV_STATS
        Serial.print(F("video size: "));
        Serial.println(videoLength);
      #endif
      
      //position = nbtv.position();
      // and now the file pointer should be pointing at actual sound data - we can return

      break;
    }
    
    #ifdef DEBUG      
      Serial.print(F("unrecognised chunk in WAV: '"));
      for (int i=0; i<4; i++)
        Serial.print(data[i]);
      Serial.println(F("'"));
    #endif
    return false;        
  }

  singleFrame = sampleRate * 2 * bitsPerSample / 8 / 12.5;

  return true;
#else
  return false;
#endif
}


void play(char* filename, unsigned long seekPoint) {

#ifdef SDX

  if (!wavInfo(filename))
      return;
  
  if (seekPoint) {    //TODO; not working
    long seekTo = singleFrame * seekPoint * 12.5 + nbtv.position();
    #ifdef DEBUG
      Serial.print("Seek to ");
      Serial.println(seekTo);
    #endif
    nbtv.seek( seekTo );
  }

  playbackAbsolute = 0;
  streamAbsolute = 0;
  pbp = 0;
  bufferOffset = 0;
  
  nbtv.read( (void *)circularAudioVideoBuffer, CIRCULAR_BUFFER_SIZE );      // pre-fill the circular buffer so it's valid
 
  noInterrupts();
  
  // Start the timer(s)
  // Uses /1 divisor, but counting up to 'resolution' each time  --> frequency!
    
  ICR3 = (int)((16000000. / sampleRate)+0.5);         // playback frequency
  TCCR3A = _BV(WGM31) | _BV(COM3A1);
  TCCR3B = _BV(WGM33) | _BV(WGM32) | _BV(CS30);       // see pp.133 ATmega32U4 manual for table
  TIMSK3 |= (_BV(ICIE3) | _BV(TOIE3));                //WGM = 1110 --> FAST PWM with TOP in ICR :)
  
  interrupts();
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Buffer stuffer interrupt
// Operates at the frequency of the WAV file data (i.e, 19200Hz for NBTV8 format)
// This code tries to fill up the unused part(s) of the circular buffer that contains the streaming
// audio & video from the WAV file being played. 'bufferOffset' is the location of the next buffer
// write, and this wraps when it gets to the end of the buffer. The number of bytes to write is
// calculated from two playback pointers - 'playbackAbsolute' which is the current position in the
// audio/video stream that's ACTUALLY being shown/heard, and 'streamAbsolute' which is the position
// of the actually streamed data (less 1 actual buffer length, as it's pre-read at the start). Those
// give us a way to calculate the amount of free bytes ('bytesToStream') which need to be filled by
// reading data from the SD card. Note that 'playbackPointer' can (and will!) change while this
// routine is streaming data from the SD.  The hope is that we can read data from the SD to the buffer
// fast enough to keep the interrupt happy. If we can't - then we get glitches on the screen/audio

ISR(TIMER3_CAPT_vect) {
#ifdef SDX
  
  if (!alreadyStreaming) {
    alreadyStreaming = true;    // prevent THIS interrupt from interrupting itself...
    interrupts();               // but allow other interrupts (the audio/video write)
    
    unsigned int bytesToStream = playbackAbsolute - streamAbsolute;
    if (bytesToStream > 64) {                     // theory: more efficient to do bigger blocks
      bytesToStream = 64;                         // TODO: BUG! Remove this: no picture!! ???
      
      void *dest = (void *)(circularAudioVideoBuffer + bufferOffset);
      bufferOffset += bytesToStream;
      if ( bufferOffset >= CIRCULAR_BUFFER_SIZE ) {
        bytesToStream = CIRCULAR_BUFFER_SIZE - bufferOffset + bytesToStream;
        bufferOffset = 0;
      }
      nbtv.read( dest, bytesToStream );
      streamAbsolute += bytesToStream;
    }
    
    alreadyStreaming = false;
  }

#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Move forward by one sample (used to reposition frame)

void bumpPlayback(long delta) {

  if (bitsPerSample==16) {
    playbackAbsolute += 4*delta;
    pbp += 4*delta;
  } else {
    playbackAbsolute += 2*delta;
    pbp += 2*delta;
  }
  
  if (pbp >= CIRCULAR_BUFFER_SIZE)
    pbp = 0;  
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Data playback interrupt
// This is an interrupt that runs at the frequency of the WAV file data (i.e, 22050Hz, 44100Hz, ...)
// It takes data from the circular buffer 'circularAudioVideoBuffer' and sends it to the LED array
// and to the speaker. Handles 16-bit and 8-bit format sample data and adjust brightness, contrast,
// and volume on-the-fly.

ISR(TIMER3_OVF_vect) {

  long audio;
  long bright;
  if (bitsPerSample==16) {

    audio = *(int *)(circularAudioVideoBuffer+pbp+2) * customVolume;
    audio >>= 16;
    
    
    bright = *(int *)(circularAudioVideoBuffer+pbp) * customContrast2;
    bright >>= 14;
    
    playbackAbsolute += 4;
    pbp += 4;
    
  } else { // assume 8-bit, NBTV WAV file format

    audio = circularAudioVideoBuffer[pbp+1] * customVolume;
    audio >>= 8;

    bright = circularAudioVideoBuffer[pbp] * customContrast2;
    bright >>= 8;

    playbackAbsolute += 2;
    pbp += 2;

  }
  
  if (pbp >= CIRCULAR_BUFFER_SIZE)
    pbp = 0;

  bright += customBrightness;

  if (bright < 0)
    bright = 0;
  else if (bright > 255)
    bright = 255;   

  OCR4A = customGamma ? pgm_read_byte(&gamma8[bright]) : (byte)bright;
  DDRC |= 1<<7;                       // Set Output Mode C7
  TCCR4A = 0x82;                      // Activate channel A


  if (audio > 255)
    audio = 255;

  OCR4D = (byte) audio;               // Write the audio to pin 6 PWM duty cycle
  DDRD |= 1<<7;
  TCCR4C |= 0x09;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

// EOF

