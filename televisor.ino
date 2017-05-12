//////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino Mechanical Televisor                                                                     //
// This program implements all the functions needed for a working mechanical televisor using an a   //
// Arduino Micro. For more information see https://www.taswegian.com/NBTV/forum/viewforum.php?f=28  //
// program by Andrew Davie (andrew@taswegian.com), March 2017                                       //
//////////////////////////////////////////////////////////////////////////////////////////////////////

//TODO: transition between selection of files - 1st only works, 2nd doesn't.




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
#define PID                       // enable PID code
#define LIST                      // show SD contents

#ifdef DEBUG
#define SHOW_WAV_STATS            // show the WAV file header details as it is loaded
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CONFIGURATION...

#define CIRCULAR_BUFFER_SIZE 256 //512  // must be a multiple of sample size
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
#include "NexVariable.h"
#include "NexButton.h"
#include "NexDualStateButton.h"
#include "NexPicture.h"


uint32_t lastSeekPosition = 0;
//int phase = 0;
long lastSeconds = 0;
uint32_t shiftFrame = 55;
uint32_t selection;             // selected track # from menu

NexVariable storedTrackName = NexVariable(2, 20, "stn");

// Page 3 - the shift dialog
NexTouch shiftUp = NexTouch(3, 1, "u");
NexTouch shiftLeft = NexTouch(3, 2, "l");
NexTouch shiftRight = NexTouch(3, 3, "r");
NexTouch shiftDown = NexTouch(3, 4, "d");
NexTouch shiftClose = NexPicture(3, 8, "p");
NexVariable shiftValue = NexVariable(3, 5, "s");

// Page 2 - The control menu
NexSlider seekerSlider = NexSlider(2, 4, "s");
NexSlider brightnessSlider = NexSlider(2, 2, "b");
NexSlider contrastSlider = NexSlider(2, 6, "c");
NexSlider volumeSlider = NexSlider(2, 7, "v");
NexDSButton gamma = NexDSButton(2, 16, "g");
NexText timePos = NexText(2, 3, "timePos");
NexText timeMax = NexText(2, 18, "tmax");
NexText trackName = NexText(2, 5, "t");
NexButton closeButton = NexButton(2, 17, "x");
NexPicture shiftPic = NexPicture(2, 14, "p6");

// Page 1 - The file selection dialog
NexButton t0 = NexButton(1, 1, "f0");
NexButton t1 = NexButton(1, 2, "f1");
NexButton t2 = NexButton(1, 3, "f2");
NexButton t3 = NexButton(1, 4, "f3");
NexButton t4 = NexButton(1, 5, "f4");
NexButton t5 = NexButton(1, 6, "f5");
NexButton t6 = NexButton(1, 12, "f6");
NexButton t7 = NexButton(1, 13, "f7");
NexButton t8 = NexButton(1, 16, "f8");

NexVariable baseVar = NexVariable(1,14,"bx");
NexVariable selectedItem = NexVariable(1,10,"si");
NexVariable requiredUpdate = NexVariable(1,15,"ru");
NexSlider trackSlider = NexSlider(1,11,"h0");

NexTouch *menuList[] = {
  // Warning: overloaded usage - t0-t8 MUST BE FIRST!
  &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, &t8, &trackSlider, NULL
};

// Page 0 - title screen
NexPicture titleScreen = NexPicture(0, 1, "p0");
NexTouch *titleList[] = { &titleScreen, NULL };

#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////


#define MODE_TITLE -1
#define MODE_INIT 0
#define MODE_SELECT_TRACK 1
#define MODE_PLAY 2
#define MODE_SHIFTER 3

int uiMode = MODE_TITLE;

//////////////////////////////////////////////////////////////////////////////////////////////////////

int phasePid = 0;
boolean alreadyStreaming = false;


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


//////////////////////////////////////////////////////////////////////////////////////////////////////
// PID...

#ifdef PID

// PID (from http://brettbeauregard.com/blog/2011/04/improving-the-beginners-pid-introduction/)

double lastTime = 0;
double errSum = 0;
double lastErr;
double kp, ki, kd;
double motorSpeed;

void initPid() {
  phasePid = 0;
  lastErr = 0;
  lastTime = 0;
  errSum = 0;
}

byte calculatePID(double error) {

  double now = ((double)playbackAbsolute) / singleFrame;
  double timeChange = now - lastTime;
  if (timeChange > 0) {
    errSum += (error * timeChange);
    double dErr = (error - lastErr) / timeChange;
    motorSpeed = kp * error + ki * errSum + kd * dErr;
  
    if (motorSpeed > 255)
      motorSpeed = 255;
    if (motorSpeed < 0)
      motorSpeed = 0;
    
    lastErr = error;
    lastTime = now;
  }

  return (byte)(motorSpeed+0.5);
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
boolean getFileN(int n,int s, char *name, boolean strip);

//////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef NEXTION

// The callback "happens" when there is a change in the scroll position of the dialog.  This
// means that we need to update one or all of the 8 text lines with new contents. When the up
// or down arrows are pressed, the Nextion automatically does the scrolling up/down for the
// lines that are visible on the screen already, and we only need to update the top (0 for up)
// or the bottom (7 for down) line.  However, if the slider was used, we have to update all
// of the lines (8) - so this is somewhat slower in updating.  Not too bad though.

#define REFRESH_TOP_LINE 0
#define REFRESH_BOTTOM_LINE 8
#define REFRESH_ALL_LINES 9


void writeMenuStrings(void * = NULL) {

#ifdef DEBUG
  Serial.println(F("writeMenuStrings()"));
#endif


  char nx[64];
  
  // Ask the Nextion which line(s) require updating.  See the REFRESH_ equates at top of file.
  uint32_t updateReq;
  if (!requiredUpdate.getValue(&updateReq)) {
    Serial.println(F("Warning: Could not read update requirement - defaulting to ALL"));
    updateReq = REFRESH_ALL_LINES;                       // do all as a fallback  
  }
  
  // Get the "base" from the Nextion. This is the index, or starting file # of the first line
  // in the selection dialog. As we scroll up, this decreases to 0. As we scroll down, it
  // increases to the maximum file # (held in the slider's max value).
  
  uint32_t base;
  if (!baseVar.getValue(&base)) {
    Serial.println(F("Error reading base"));
    base = 0;
  }    

  #ifdef DEBUG
    Serial.print(F("Base="));
    Serial.print(base);
    Serial.print(F(" Update="));
    Serial.println(updateReq);
  #endif
  
  // Based on 'requiredUpdate' from the Nextion we either update only the top line (when up arrow pressed),
  // only the bottom line (when down arrow pressed), or we update ALL of the lines (slider dragged). The
  // latter is kind of slow, but that doesn't really matter.  Could switch the serial speed to 115200 bps to
  // make this super-quick, but it works just fine at default 9600 bps.

  #ifdef DEBUG
    Serial.println(F("----------"));
  #endif
  
  switch (updateReq) {
    
    case REFRESH_TOP_LINE:
    case REFRESH_BOTTOM_LINE: {
        if (composeMenuItem(base + updateReq, sizeof(nx), nx)) {
          #ifdef DEBUG
            Serial.println(nx);
          #endif
          ((NexButton *) menuList[updateReq])->setText(nx);
        }
      }
      break;

    case REFRESH_ALL_LINES:
    default:
      for (int i=0; i<9; i++) {
        if (composeMenuItem(base + i, sizeof(nx), nx)) {
          #ifdef DEBUG
            Serial.println(nx);
          #endif
          ((NexButton *) menuList[i])->setText(nx);
        }
      }
      break;
  }
  #ifdef DEBUG
    Serial.println(F("----------"));
  #endif
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////

void prepareControlPage() {

  // Fill in the one-time-only objects on the page
  // --> Title, length, toggle states
  
  char nx[64];
  storedTrackName.getText(nx, sizeof(nx));
  trackName.setText(nx);
    
//  if (getFileN(selection,sizeof(nx),nx,true))        // name without extension
//    trackName.setText(nx);
//  else {
//    #ifdef DEBUG
//      Serial.println(F("Error retrieving shortened filename"));
//    #endif
//  }
                  
  // Display total track length
  drawTime(timeMax,videoLength/38400);     // 2 bytes/sample, 48 samples/line, 32 lines/frame, 12.5 frames/second
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void trackSelectCallback(void *) {

  // When a menu item is selected, we read the selected item absolute number from the Nextion
  // and use that to lookup the menu string itself, and display both of them on the serial.

#ifdef DEBUG
  Serial.println(F("trackSelectCallback()"));
#endif

  if (selectedItem.getValue(&selection)) {

    #ifdef DEBUG
      Serial.print(F("#"));
      Serial.print(selection);
      Serial.print(F(" = "));
    #endif
    
    char nx[64];
//    memset(nx,0,sizeof(nx));
    boolean found = getFileN(selection,sizeof(nx),nx, false);
    if (found) {
      #ifdef DEBUG
        Serial.println(nx);
      #endif

      interrupts();
      sendCommand("page 2");
      uiMode = MODE_PLAY;
      OCR0B = 255;


      setupFastPwm(PWM187k);
//      getFileN(selection, sizeof(nx), nx, false);
      play(nx);

      storedTrackName.setText(nx);
      prepareControlPage();

    } else {
      #ifdef DEBUG
        Serial.println(F("File not found"));
      #endif
    }
  } else {
    #ifdef DEBUG
      Serial.println(F("E: selection"));
    #endif
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void brightnessCallback(void *) {

//#ifdef DEBUG
//  Serial.println(F("brightnessCallback()"));
//#endif
  
  uint32_t value;
  if (brightnessSlider.getValue(&value))
    customBrightness = (int)(256.*(value-128.)/128.);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void contrastCallback(void *) {

//#ifdef DEBUG
//  Serial.println(F("contrastCallback()"));
//#endif

uint32_t value;
  if (contrastSlider.getValue(&value))
    customContrast2 = value << 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void volumeCallback(void *) {

//#ifdef DEBUG
//  Serial.println(F("volumeCallback()"));
//#endif

  uint32_t value;
  if (volumeSlider.getValue(&value))
    customVolume = value << 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void gammaCallback(void *) {

//#ifdef DEBUG
//  Serial.println(F("gammaCallback()"));
//#endif
  
  uint32_t value;
  if (gamma.getValue(&value))
    customGamma = (value!=0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void seekerCallback(void *) {
//      if (seekerSlider.getValue(&value))
// TODO: modify seek position based on seeker value
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void closeButtonCallback(void *) {

//#ifdef DEBUG
//  Serial.println(F("closeButtonCallback()"));
//#endif

  TCCR4B= 0;                          // effectively turn off LED interrupt
  TCCR3A = TCCR3B = 0;                // turn off motor buffer stuffer and playback

  alreadyStreaming = true;            // prevent SD reader from running

//  noInterrupts();
  nbtv.close();

  sendCommand("page 0");

  
  OCR0B = 0;        // stop motor
  
  // blacken screen
  OCR4A = 0;
  DDRC |= 1<<7;                       // Set Output Mode C7
  TCCR4A = 0x82;                      // Activate channel A
  
  
  uiMode = MODE_TITLE;
  
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void shiftStartCallback(void *) {

  //TODO: shifting not really functional

#ifdef DEBUG
  Serial.println(F("shiftStartCallback()"));
#endif

  sendCommand("page 3");
  uiMode = MODE_SHIFTER;

}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void shiftCallback(void *) {

#ifdef DEBUG
  Serial.println(F("shiftCallback()"));
#endif

  if (!shiftValue.getValue(&shiftFrame)) {
  #ifdef DEBUG
    Serial.println(F("Error retrieving shift value"));
  #endif
  } else {
    #ifdef DEBUG
      Serial.print(F("Shift Frame = "));
      Serial.println(shiftFrame);
    #endif
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void shiftCloseCallback(void *) {

//#ifdef DEBUG
//  Serial.println(F("shiftCloseCallback()"));
//#endif

  sendCommand("page 2");
  prepareControlPage();
  uiMode = MODE_PLAY;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void titleCallback(void *) {
#ifdef DEBUG
  Serial.println(F("titleCallback()"));
#endif
  sendCommand("page 1");

  // Count the number of menu items and then set the maximum range for the slider
  // We subtract 9 from the count because there are 9 lines already visible in the window

  int menuSize = countFiles();
  if (!trackSlider.setMaxval(menuSize > 9 ? menuSize - 9 : 0)) {
    #ifdef DEBUG
      Serial.println(F("Error setting slider maximum!"));
    #endif
  }
  
  writeMenuStrings();           // defaults to REFRESH_ALL_LINES, so screen is populated
  uiMode = MODE_INIT;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

#ifdef DEBUG
  Serial.begin(9600);
  while (!Serial)
    SysCall::yield();
  Serial.println(F("Televisor Active!"));
#endif

#ifdef SDX
  // Setup access to the SD card
  #define SD_CS_PIN 4
  pinMode(SS, OUTPUT);
  if (!nbtvSD.begin(SD_CS_PIN)) {
    #ifdef DEBUG
      Serial.println(F("SD failed!"));
    #endif
  }
#endif

#ifdef NEXTION

  nexInit();

  //////////////////////////////////////////////////////////////////////////////////////////
  // BUG:  I have no idea why the delay below is needed, but the communication with Nextion
  // fails if it's not there - specifically, the trackSlider.setMaxValue(...) fails.  This code
  // has generic communication problems with the Nextion but seemingly ONLY ON STARTUP - 
  // once the scrolling system is going, everything seems hunky dory.  So, somewhere there's
  // a mistake by yours truly...
  
//  delay(1000);
  while (nexSerial.available())
    nexSerial.read();
    
  //
  //////////////////////////////////////////////////////////////////////////////////////////

  // Page 0 - Title
  titleScreen.attachPop(titleCallback, &titleScreen);

  // Page 1 - Menu Selection
  trackSlider.attachPop(writeMenuStrings, &trackSlider);
  for (int i=0; i<9; i++)
    menuList[i]->attachPop(trackSelectCallback, menuList[i]);

  // Page 2 - controls
  seekerSlider.attachPop(seekerCallback, &seekerSlider);
  brightnessSlider.attachPop(brightnessCallback, &brightnessSlider);
  contrastSlider.attachPop(contrastCallback, &contrastSlider);
  volumeSlider.attachPop(volumeCallback, &volumeSlider);
  gamma.attachPop(gammaCallback, &gamma);
  closeButton.attachPop(closeButtonCallback, &closeButton);
  shiftPic.attachPop(shiftStartCallback, &shiftPic);

  // Page 3 - shifter
  shiftUp.attachPop(shiftCallback, &shiftUp);
  shiftLeft.attachPop(shiftCallback, &shiftLeft);
  shiftRight.attachPop(shiftCallback, &shiftRight);
  shiftDown.attachPop(shiftCallback, &shiftDown);
  shiftClose.attachPop(shiftCloseCallback, &shiftClose);
  
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

int countFiles() {

  char name[64];

//#ifdef DEBUG
//  Serial.println(F("Counting files"));
//#endif
  
  int count=0;
  FatFile *vwd = nbtvSD.vwd();
  vwd->rewind();
  
  SdFile file;
  while (file.openNext(vwd, O_READ)) {
//    memset(name,0,sizeof(name));
    file.getName(name,sizeof(name)-1);
    file.close();
    if (name[0] != 46 && strstr(name,".wav"))
        count++; 
  }
  return count;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

boolean getFileN(int n,int s, char *name, boolean strip = true) {

//#ifdef DEBUG
//  Serial.print(F("getFileN("));
//  Serial.print(n);
//  Serial.println(F(")"));
//#endif

  FatFile *vwd = nbtvSD.vwd();
  vwd->rewind();

  
  SdFile file;
  while (file.openNext(vwd, O_READ)) {
    file.getName(name,s-1);
    file.close();
    #ifdef DEBUG
//    Serial.println(name);
    #endif
    if (name[0] != 46) {
      char *px = strstr(name,".wav");
      if (px) {
        if (strip)
          *px = 0;
        if (!n--)
          return true;
      }
    }
  }
  return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

char *composeMenuItem(int item,int s, char *p) {
    memset(p,0,s);
    sprintf(p,"%2d ",item+1);
    if (!getFileN(item,s-3,p+3))
      *p = 0;
    return p;
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

  OCR0B = 0;           // start motor spinning so PID can kick in in interrupt
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


//////////////////////////////////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////////////////////////////////
//-- Analog comparator interrupt service routine -------------------------------------------------
// Triggered by sync hole detected by IR sensor, connected to pin 7 (AC)


ISR(ANALOG_COMP_vect) {

  timeDiff = playbackAbsolute;
  
  double deltaSample = timeDiff - lastDetectedIR;
  if (deltaSample > 1000) {         // cater for potential "bounce" on IR detect by not accepting closely spaced signals
  
    interrupts();

    lastDetectedIR = timeDiff;

     switch (phasePid) {
  
        case 0:

          // Phase 0 is the beginning - if the disc is stopped, then this runs the motor at full-speed until the rotation
          // is 'nearly right' - taking into account the physical characteristics of the drive chain.  This will vary
          // from televisor to televisor. What we want to do is have the disc as close to possible to correct speed
          // AND not accellerating when we switch to the PID.

          if (deltaSample < 3072+100) {
            SetTunings(1.5,.35,1);                    // The PID that does the regular synching
            phasePid++;
            lastTime =  ((double)playbackAbsolute) / singleFrame;
          }
          break;
        
        case 1:
          {
            // This next bit is super cool.  The PID synchronises the disc speed to 12.5 Hz (indirectly through the singleFrame
            // value, which is a count of #samples per rotation at 12.5 Hz).  Since the disc and the playback of the video
            // are supposed to be exactly in synch, we know that both SHOULD start a frame/rotation at exactly the same time.
            // So since we know we're at the start of a rotation (we just detected the synch hole), then we can compare
            // how 'out of phase' the video is by simply looking at the video playback sample # ('plabackAbsolute') and
            // use the sub-frame offset as an indicator of how inaccurate the framing is.  Once we know that, instead of
            // trying to change the video timing, we just change the disc speed. How do we do that?  By tricking the PID
            // into thinking the disc is running faster than it really is by telling it that the time between now and
            // the last detected rotation is actually shorter than measured. A consequence of this is that the PID will try
            // to slow the disc down a fraction, which will in turn mean the video is playing slightly faster relative
            // to the timing of the disc, and the image will shift up and left.  We also (gosh this is elegant) get the
            // ability to shift the image up/down.  So we can hardwire the exact framing vertical of the displayed image,
            // too.
            
            uint32_t pbDelta = timeDiff % singleFrame;            // how inaccurate is framing?
            if (pbDelta > shiftFrame) {                           // if we need to 'vertically' adjust (OR horizontally)
                int adjust = (pbDelta - shiftFrame ) / 8 + 1;     // use how far out of wonk as a speed control
                if (adjust > 64)                                  // but cap it because otherwise we overwhelm the PID
                  adjust = 64;
                lastDetectedIR -= adjust;                         // trick the PID into thinking the disc is fast
            }
      
            #ifdef PID 
              OCR0B = calculatePID(deltaSample-singleFrame);    // write the new motor PID speed
            #else
              OCR0B = 53;
            #endif
          }
          break;
  
        default:
          break;
      }

  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
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
  
//////////////////////////////////////////////////////////////////////////////////////////////////////

void error(char *data) {
  Serial.print(data[0]);
  Serial.print(data[1]);
  Serial.print(data[2]);
  Serial.println(data[3]);
}


boolean wavInfo(char* filename) {

#ifdef SDX

  nbtv = nbtvSD.open(filename);
  nbtv.rewind();
  
  if (!nbtv) {
    #if defined(DEBUG)
      Serial.print(F("Error when opening file '"));
      Serial.print(filename);
      Serial.print(F("'"));
    #endif
    return false;
  }

  char data[4];
  
//  uint32_t header;
  //nbtv.read(&header,4);
  nbtv.read(data,4);

  if (data[0]!= 'R' || data[1]!='I' || data[2]!='F' || data[3]!='F') {
//  if (header != 0x46464952) { //'FFIR'
    #ifdef SHOW_WAV_STATS
      Serial.println(F("Error: no RIFF header"));
      error(data);
    #endif
    return false;
  }

  long chunkSize;
  nbtv.read(&chunkSize,4);
  #ifdef SHOW_WAV_STATS
    Serial.print(F("WAV size: "));
    Serial.println(chunkSize+8);
  #endif
  
//  nbtv.read(&header,4);
//  if (header != 0x45564157) { //'EVAW'
    nbtv.read(data,4);
    if (data[0]!='W' || data[1]!='A' || data[2]!='V' || data[3]!='E') {
    #ifdef DEBUG
      Serial.println(F("Error: WAVE header incorrect"));
    #endif
    return false;
  }

  long position;
  while (true) {

//    nbtv.read(&header,4);        // read next chunk header
    nbtv.read(data,4);
    
//    if (header == 0x7674626e) { //'vtbn'
    if (data[0]=='n' && data[1]=='b' && data[2]=='t' && data[3]=='v') {
//    if (!strncmp(data,"nbtv",4)) {
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

//    if (header == 0x20746D66) { //' tmf'
    if (data[0]=='f' && data[1]=='m' && data[2]=='t' && data[3]==' ') {
//    if (!strncmp(data,"fmt ",4)) {
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

//    if (header == 0x61746164) { //'atad'
    if (data[0]=='d' && data[1]=='a' && data[2]=='t' && data[3]=='a') {
//    if (!strncmp(data,"data",4)) {
      #ifdef SHOW_WAV_STATS
        Serial.println(F("data chunk"));
      #endif
      
      nbtv.read(&videoLength,4);
      #ifdef SHOW_WAV_STATS
        Serial.print(F("video size (s): "));
        Serial.println(videoLength);
      #endif
      
      //position = nbtv.position();
      // and now the file pointer should be pointing at actual sound data - we can return

      break;
    }
    
    #ifdef DEBUG      
      Serial.print(F("unrecognised chunk in WAV"));
    #endif
    return false;        
  }

  singleFrame = sampleRate * 2 * bitsPerSample / 8 / 12.5;

  return true;
#else
  return false;
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void play(char* filename, unsigned long seekPoint) {
#ifdef SDX

  if (!wavInfo(filename))
      return;
  
  if (seekPoint) {    //TODO; not working
    long seekTo = singleFrame * seekPoint * 12.5 + nbtv.position();
    #ifdef DEBUG
      Serial.print(F("Seek to "));
      Serial.println(seekTo);
    #endif
    nbtv.seek( seekTo );
  }

  playbackAbsolute = 0;
  streamAbsolute = 0;
  pbp = 0;
  bufferOffset = 0;
  initPid();
  alreadyStreaming = false;
  
  nbtv.read((void *)circularAudioVideoBuffer, CIRCULAR_BUFFER_SIZE);      // pre-fill the circular buffer so it's valid
 
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
    if (bytesToStream > 32) {                     // theory: more efficient to do bigger blocks
      //bytesToStream = 64;                         // TODO: BUG! Remove this: no picture!! ???
      
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

void drawTime(NexText &t, int sec) {

  int s = sec % 60;
  int m = sec / 60;

  char times[6];

  if (m/10)
    times[0] = '0'+m/10;
  else
    times[0] = ' ';
  times[1] = '0'+m%10;
  times[2] = ':';
  times[3] = '0'+s/10;
  times[4] = '0'+s%10;
  times[5] = 0;

  t.setText(times);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  
  switch (uiMode) {

    case MODE_TITLE:
      #ifdef NEXTION
        nexLoop(titleList);
      #endif
      break;

    /////////////////////////////////////////////////////////////////////////////////////////////
    
    case MODE_INIT:
      setupIRComparator();
      setupMotorPWM();
//      setupFastPwm(PWM187k);
      #ifdef PID
        SetTunings(1.5,0,0);
      #endif
      uiMode = MODE_SELECT_TRACK;
      break;

    /////////////////////////////////////////////////////////////////////////////////////////////

    case MODE_SELECT_TRACK:           // file selection from menu
      #ifdef NEXTION
        nexLoop(menuList);
      #endif
      break;

    /////////////////////////////////////////////////////////////////////////////////////////////

    case MODE_PLAY:           // movie is playing
      #ifdef NEXTION
      {
        NexTouch *controlListen[] = {
          &seekerSlider, &brightnessSlider, &contrastSlider, &volumeSlider, &gamma, &closeButton, &shiftPic, NULL
        };
        nexLoop(controlListen);


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
          drawTime(timePos,seconds);
        }
      }
      #endif
      break;

    /////////////////////////////////////////////////////////////////////////////////////////////

    case MODE_SHIFTER:           // movie is playing and we're looking at the frame-shift dialog
      #ifdef NEXTION
      {
        NexTouch *shiftListen[] = { &shiftUp, &shiftLeft, &shiftRight, &shiftDown, &shiftClose, NULL };
        nexLoop(shiftListen);
      }
      #endif
      break;


    default:
      break;
    
  }
  

}


// EOF

