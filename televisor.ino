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

//#define DEBUG                     // diagnostics to serial terminal (WARNING: busy-waits)
#define NEXTION                   // include Nextion LCD functionality
//#define GENERIC                   // allow NON 8-bit files to be played (deprecated)

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

#define PIN_MOTOR 3


//////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef NEXTION

#include "NexHardware.h"
#include "NexSlider.h"
#include "NexText.h"
#include "NexVariable.h"
#include "NexButton.h"
#include "NexDualStateButton.h"
#include "NexPicture.h"
#include "NexPage.h"


uint32_t lastSeekPosition;
//int phase = 0;
long lastSeconds;
uint32_t shiftFrame = 60;
uint32_t selection;             // selected track # from menu
boolean showInfo = false;



// Page 2 - The control menu
NexSlider seekerSlider = NexSlider(2, 21, "s");
NexSlider contrastSlider = NexSlider(2, 7, "c");
NexSlider volumeSlider = NexSlider(2, 8, "v");
NexDSButton gamma = NexDSButton(2, 15, "g");
NexDSButton repeat = NexDSButton(2, 17, "r");
NexButton closeButton = NexButton(2, 16, "x");
NexButton frameButton = NexButton(2, 23, "f");
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////


#define MODE_INIT 0
#define MODE_TITLE 1
#define MODE_TITLE_INIT 2
#define MODE_SELECT_TRACK 3
#define MODE_PLAY 4
#define MODE_SHIFTER 5
#define MODE_Q 6

int uiMode = MODE_TITLE_INIT;

//////////////////////////////////////////////////////////////////////////////////////////////////////

int phasePid = 0;
int customBrightness = 0;
uint32_t customGamma = true;
uint32_t customRepeat = 0;
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

long position;        // WAV file start of data chunk (for rewind/seek positioning)


//////////////////////////////////////////////////////////////////////////////////////////////////////
// PID...

// PID (from http://brettbeauregard.com/blog/2011/04/improving-the-beginners-pid-introduction/)

double lastTime = 0;
double errSum = 0;
double lastErr;
double kp, ki, kd;
double motorSpeed;


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

    // attempt to reduce overshoot with a clean slate
   lastTime = ((double)playbackAbsolute)/singleFrame;
   lastErr = 0;
   errSum = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

#include <SdFat.h>
File nbtv;
SdFat nbtvSD;


void play(char* filename, unsigned long seekPoint=0);
boolean getFileN(int n,int s, char *name, boolean reset, boolean strip);

//////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef NEXTION

// The callback "happens" when there is a change in the scroll position of the dialog.  This
// means that we need to update one or all of the 8 text lines with new contents. When the up
// or down arrows are pressed, the Nextion automatically does the scrolling up/down for the
// lines that are visible on the screen already, and we only need to update the top (0 for up)
// or the bottom (7 for down) line.  However, if the slider was used, we have to update all
// of the lines (8) - so this is somewhat slower in updating.  Not too bad though.

#define REFRESH_TOP_LINE 0
#define REFRESH_BOTTOM_LINE 7
#define REFRESH_ALL_LINES 9

void refresh(int base, int i) {
  char nx[64];
  strcpy(nx, "f0.txt=\"");
  nx[1] = '0'+ i;
  composeMenuItem(base + i, sizeof(nx)-8, nx+8, true);
  strcat(nx,"\"");
  sendCommand(nx);
}

void writeMenuStrings(void * = NULL) {

  
  // Ask the Nextion which line(s) require updating.  See the REFRESH_ equates at top of file.
  uint32_t updateReq;
  NexVariable requiredUpdate = NexVariable(1,15,"ru");
  if (!requiredUpdate.getValue(&updateReq))
    updateReq = REFRESH_ALL_LINES;                       // do all as a fallback  
  
  // Get the "base" from the Nextion. This is the index, or starting file # of the first line
  // in the selection dialog. As we scroll up, this decreases to 0. As we scroll down, it
  // increases to the maximum file # (held in the slider's max value).
  
  uint32_t base;
  NexVariable baseVar = NexVariable(1,14,"bx");
  if (!baseVar.getValue(&base))
    base = 0;

  // Based on 'requiredUpdate' from the Nextion we either update only the top line (when up arrow pressed),
  // only the bottom line (when down arrow pressed), or we update ALL of the lines (slider dragged). The
  // latter is kind of slow, but that doesn't really matter.  Could switch the serial speed to 115200 bps to
  // make this super-quick, but it works just fine at default 9600 bps.


  if (updateReq == REFRESH_ALL_LINES)
    for (int i=0; i<8; i++)
      refresh(base, i);
  else
    refresh(base, updateReq);
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////

void prepareControlPage() {

  // Fill in the one-time-only objects on the page

  char nx[64];

  sprintf(nx,"g.val=%d",(int)customGamma);
  sendCommand(nx);
  sprintf(nx,"r.val=%d",(int)customRepeat);
  sendCommand(nx);

  // Set "i" button ORANGE if there's no text
  sprintf(nx,"q.pic=%d", showInfo ? 13 : 20);
  sendCommand(nx);
  sprintf(nx,"q.pic2=%d", showInfo ? 15 : 20);
  sendCommand(nx);

  drawTime((char*)"tmax",videoLength/38400);     // 2 bytes/sample, 48 samples/line, 32 lines/frame, 12.5 frames/second
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void commenceSelectedTrack(boolean firstTime=true) {

  char nx[64];
  if (getFileN(selection,sizeof(nx),nx, true, false)) {

    if (firstTime) {
      customRepeat = false;            // 0 on new track, or keep existing if repeating
      qInit(nx);                       // read and buffer INFO lines --> nextion
    }
  
    
    lastSeconds =
    lastSeekPosition = 99999;

       

    setupMotorPWM();

    setupFastPwm(PWM187k);

    play(nx);

    // Strip the extension off the file name and send the result (title of the track) to the nextion
    // stord in the stn (stored track name) variable. This is loaded by the Nextion at start of page 2 display
    
    char nx2[64];
    strcpy(nx2,"stn.txt=\"");
    strcat(nx2,nx);
    strcpy(strstr(nx2,".wav"),"\"");
    sendCommand(nx2);                           // --> stn.txt="track name"

    if (firstTime) {

      customBrightness = 0;
      customContrast2 = 256;
      customVolume = 256;
      customGamma = true;

      uiMode = MODE_PLAY;
      sendCommand("page 2");
      prepareControlPage();
    }
    
  } else {
    #ifdef DEBUG
      Serial.println(F("E: not found"));
    #endif
  }
  
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void trackSelectCallback(void *) {
  NexVariable selectedItem = NexVariable(1,10,"si");
  if (selectedItem.getValue(&selection))
    commenceSelectedTrack(true);
}


void brightnessCallback(void *) {
  uint32_t value;
  NexSlider brightnessSlider = NexSlider(2, 5, "b");
  if (brightnessSlider.getValue(&value))
    customBrightness = (int)(256.*(value-128.)/128.);
}


void seekerSliderCallback(void *) {
  Serial.println("seekerslidercallback");
  uint32_t seekTo;
  if (seekerSlider.getValue(&seekTo)) {
    long seekPos = videoLength / 255. * seekTo;
    resetStream(seekPos);
  }
}


void contrastCallback(void *) {
  uint32_t value;
  if (contrastSlider.getValue(&value))
    customContrast2 = value << 1;
}


void volumeCallback(void *) {
  uint32_t value;
  if (volumeSlider.getValue(&value))
    customVolume = value << 1;
}


void gammaCallback(void *) {
  gamma.getValue(&customGamma);
}


void repeatCallback(void *) {
  repeat.getValue(&customRepeat);
}


void shiftButtonCallback(void *) {
  uiMode = MODE_SHIFTER;
}


void closeButtonCallback(void *) {

  TCCR4B =                            // effectively turn off LED interrupt
  TCCR3A =
  TCCR3B = 0;                         // turn off motor buffer stuffer and playback
  TCCR0A = 0;                         // motor PWM OFF
  OCR0A = 0;                          // motor OFF (just in case PWM had a spike)
  
  nbtv.close();                       // close any previously playing/streaming file
  
  OCR4A = 0;                          // blacken LED (=screen)
  DDRC |= 1<<7;                       // Set Output Mode C7
  TCCR4A = 0x82;                      // Activate channel A

  uiMode = MODE_TITLE_INIT;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void qInit(char *name) {

  // Copy video description (if any) from text file to the global variables on the
  // menu page. These are later copied to the description (Q) page lines on init of
  // that page by the Nextion itself. We do it here as globals so that we don't need
  // to flip Nextion pages, and we get auto-init on page draw.

  // Drop off the '.wav' and append '.txt' to give us the description filename
  char nx[64];
  strcpy(nx,name);
  strcpy(strstr(nx,".wav"),".txt");

  // grab the video details and write to the Q page
  File explain = nbtvSD.open(nx);
  showInfo = explain;
  
  for (int i=0; i<12; i++) {
     sprintf(nx,"v%02d.txt=\"", i);
     char *p = nx+9;
     if (explain) {
       do {
        explain.read(p,1);
       } while (*p++ != 10);
       p--;
     }
     strcpy(p,"\"");
     sendCommand(nx);
  }
  explain.close();
}


void qCallback(void *) {
  sendCommand("page 4");
  uiMode = MODE_Q;
}

void qExitCallback(void *) {
  sendCommand("page 2");
  uiMode = MODE_PLAY;
}


void shifterCallback(void *) {
  NexVariable shift = NexVariable(3, 1, "s");
  shift.getValue(&shiftFrame);
//  Serial.println(shiftFrame);
}


void shiftCallback(void *) {
  // Enter shifter page
  sendCommand("page 3");
  // TODO: calc H, V and init H,V,S
  uiMode = MODE_SHIFTER;
}


void titleCallback(void *) {

  sendCommand("page 1");        // --> MENU

  // Count the number of menu items and then set the maximum range for the slider
  // We subtract 9 from the count because there are 9 lines already visible in the window

  int menuSize = countFiles();
  NexSlider trackSlider = NexSlider(1,11,"h0");
  if (!trackSlider.setMaxval(menuSize > 9 ? menuSize - 9 : 0)) {
    #ifdef DEBUG
      Serial.println(F("E: slide"));
    #endif
  }
  
  writeMenuStrings();           // defaults to REFRESH_ALL_LINES, so screen is populated
  uiMode = MODE_INIT;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// The Arduino starts with a one-time call to setup(), so this is where the Televisor is initialised.
// That's followed by calls to loop() whenever there is free time.

void setup() {

  #ifdef DEBUG
    Serial.begin(9600);
    while (!Serial)
      SysCall::yield();
    Serial.println(F("!"));
  #endif

  // Setup access to the SD card
  #define SD_CS_PIN 4
  pinMode(SS, OUTPUT);
  if (!nbtvSD.begin(SD_CS_PIN)) {
    #ifdef DEBUG
      Serial.println(F("SD failed!"));
    #endif
  }


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


  // Page 2 - controls
  contrastSlider.attachPop(contrastCallback, &contrastSlider);
  volumeSlider.attachPop(volumeCallback, &volumeSlider);
  gamma.attachPush(gammaCallback, &gamma);
  repeat.attachPush(repeatCallback, &repeat);
  closeButton.attachPop(closeButtonCallback, &closeButton);
  seekerSlider.attachPop(seekerSliderCallback, &seekerSlider);
  
  
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

int countFiles() {

  char name[64];

  int count=0;
  FatFile *vwd = nbtvSD.vwd();
  vwd->rewind();
  
  SdFile file;
  while (file.openNext(vwd, O_READ)) {
    file.getName(name,sizeof(name)-1);
    file.close();
    if (*name != '.' && strstr(name,".wav"))
        count++; 
  }
  return count;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

boolean getFileN(int n,int s, char *name, boolean hunt = true, boolean strip = true) {

  FatFile *vwd = nbtvSD.vwd();
  if (hunt)
    vwd->rewind();
  
  SdFile file;
  while (file.openNext(vwd, O_READ)) {
    file.getName(name,s);
    file.close();
    if (*name != '.') {
      char *px = strstr(name,".wav");
      if (px) {
        if (strip)
          *px = 0;
        if (!hunt || (hunt && !n--))
          return true;
      }
    }
  }
  return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void composeMenuItem(int item,int s, char *p, boolean hunt) {
    sprintf(p,"%2d ",item+1);
    if (!getFileN(item,s-3,p+3, hunt))
      *p = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PWM to control motor speed

void setupMotorPWM() {

  
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

  OCR0B = 255;                      // full throttle!
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

  // 3072 = 1 rotation
  // 1/32 = 48 (one scanline hole difference)
  
  double deltaSample = timeDiff - lastDetectedIR;
  if (deltaSample > 1000) {     // cater for potential "bounce" on IR detect by not accepting closely spaced signals
  
    interrupts();

    lastDetectedIR = timeDiff;

     switch (phasePid) {
  
        case 0:

          // Phase 0 is the beginning - if the disc is stopped, then this runs the motor at full-speed until the rotation
          // is 'nearly right' - taking into account the physical characteristics of the drive chain.  This will vary
          // from televisor to televisor. What we want to do is have the disc as close to possible to correct speed
          // AND not accellerating when we switch to the PID.

          // @19200 Hz, there are 19200/12.5 samples per frame = 1536 samples
          // with two bytes per sample (video+audio bytes) that's 3072 bytes per frame

          if (deltaSample < 3072+100) {       // spin up to "slightly fast"
            SetTunings(1.5,.35,1);                                  // The PID that does the regular synching
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
            // into thinking the disc is running slower than it really is by telling it that the time between now and
            // the last detected rotation is actually longer than measured. A consequence of this is that the PID will try
            // to speed the disc up a fraction, which will in turn mean the video is playing slightly slower relative
            // to the timing of the disc, and the image will shift left and up.  We also (gosh this is elegant) get the
            // ability to shift the image up/down.  So we can hardwire the exact framing vertical of the displayed image,
            // too.

//              shiftFrame = 60; //tmp
              uint32_t pbDelta = timeDiff % singleFrame;            // how inaccurate is framing?            
              if (pbDelta > shiftFrame) {                           // if we need to 'vertically' adjust (OR horizontally)
                int adjust = (pbDelta - shiftFrame ) / 8 + 1;     // use how far out of wonk as a speed control
                if (adjust > 64)                                  // but cap it because otherwise we overwhelm the PID
                  adjust = 64;
                lastDetectedIR -= adjust;                         // trick the PID into thinking the disc is fast
              }
      
              OCR0B = calculatePID(deltaSample-singleFrame);    // write the new motor PID speed      
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

boolean wavInfo(char* filename) {

  nbtv = nbtvSD.open(filename);
  if (!nbtv) {
    return false;
  }


  nbtv.rewind();
  
  
  uint32_t header;

  nbtv.read(&header,4);
  if (header != 0x46464952) { //'FFIR'
    return false;
  }

  long chunkSize;
  nbtv.read(&chunkSize,4);
  
  nbtv.read(&header,4);
  if (header != 0x45564157) { //'EVAW'
    return false;
  }

  while (true) {

    nbtv.read(&header,4);        // read next chunk header
    
    if (header == 0x7674626e) { //'vtbn'
      nbtv.read(&chunkSize,4);
      position = nbtv.position();
      nbtv.seek(position+chunkSize);
      continue;
    }

    else if (header == 0x20746D66) { //' tmf'

      nbtv.read(&chunkSize,4);
      position = nbtv.position();
      
      int audioFormat;
      nbtv.read(&audioFormat,2);
      
      int numChannels;
      nbtv.read(&numChannels,2);

      nbtv.read(&sampleRate,4);
      
      long byteRate;
      nbtv.read(&byteRate,4);
      
      int blockAlign;
      nbtv.read(&blockAlign,2);
      
      nbtv.read((void *)&bitsPerSample,2);
      
      // Potential "ExtraParamSize/ExtraParams" ignored because PCM

      nbtv.seek(position+chunkSize);
      continue;
    }

    else if (header == 0x61746164) { //'atad'
      nbtv.read(&videoLength,4);
      position = nbtv.position();
      // and now the file pointer should be pointing at actual sound data - we can return

      break;
    }
    
    return false;        
  }

  singleFrame = sampleRate * 2 * bitsPerSample / 8 / 12.5;
  return true;
}


void resetStream(long seeker) {
  noInterrupts();
  nbtv.seek(position+seeker);
  streamAbsolute =
  playbackAbsolute = seeker;
  lastDetectedIR = playbackAbsolute;
  pbp =
  bufferOffset = 0;

  lastTime = ((double)playbackAbsolute) / singleFrame;                    // keep the PID happy
  nbtv.read((void *)circularAudioVideoBuffer, CIRCULAR_BUFFER_SIZE);      // pre-fill the circular buffer so it's valid
  interrupts();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void play(char* filename, unsigned long seekPoint) {

  if (wavInfo(filename)) {
  
    phasePid =
    lastErr =
    lastTime =
    errSum = 0;

    noInterrupts();

    long seeker = singleFrame * seekPoint * 12.5;
    resetStream(seeker);
   
    // Start the timer(s)
    // Uses /1 divisor, but counting up to 'resolution' each time  --> frequency!
      
    ICR3 = (int)((16000000. / sampleRate)+0.5);         // playback frequency
    TCCR3A = _BV(WGM31) | _BV(COM3A1);
    TCCR3B = _BV(WGM33) | _BV(WGM32) | _BV(CS30);       // see pp.133 ATmega32U4 manual for table
    TIMSK3 |= (_BV(ICIE3) | _BV(TOIE3));                //WGM = 1110 --> FAST PWM with TOP in ICR :)
    
    interrupts();
  }
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
  
  static boolean alreadyStreaming = false;
  if (!alreadyStreaming) {
    alreadyStreaming = true;    // prevent THIS interrupt from interrupting itself...
    interrupts();               // but allow other interrupts (the audio/video write)
    
    unsigned int bytesToStream = playbackAbsolute - streamAbsolute;
    if (bytesToStream > 32) {                       // theory: more efficient to do bigger blocks
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
#ifdef GENERIC
if (bitsPerSample==16) {

    audio = *(int *)(circularAudioVideoBuffer+pbp+2) * customVolume;
    audio >>= 16;
    bright = *(int *)(circularAudioVideoBuffer+pbp) * customContrast2;
    bright >>= 14;
    
    playbackAbsolute += 4;
    pbp += 4;
    
  } else

#endif
  { // assume 8-bit, NBTV WAV file format

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

void drawTime(char *name, int sec) {

  int s = sec % 60;
  int m = sec / 60;

  char cmd[32];
  sprintf(cmd,"%s.txt=\"%2d:%02d\"",name,m,s);
  sendCommand(cmd);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

// Check to see if the data for the video has run out
// If so, then we stop the video, the motor, and then handle possible repeat button.
// and remain in the same (sub) mode we might already have been in (INFO or SHIFT, for example)

void handleEndOfVideo(int nextMode) {

  if (playbackAbsolute >= videoLength) {              // Check for end of movie
      closeButtonCallback(NULL);                      // stop everyting (same as 'pressing' stop button)
      if (customRepeat) {                             // BUT we might have the repeat button ON
        commenceSelectedTrack(false);                 // in which case we restart the track (NOT first time)
        uiMode = nextMode;                            // and drop back into the previous UI mode
      }
  }        
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  
  switch (uiMode) {

    case MODE_TITLE_INIT:
      sendCommand("page 0");
      uiMode = MODE_TITLE;
      break;

    case MODE_TITLE:
      {
        #ifdef NEXTION
          NexPage titleScreen = NexPage(0, 0, "T");
          titleScreen.attachPop(titleCallback, &titleScreen);
          NexTouch *titleList[] = { &titleScreen, NULL };
          nexLoop(titleList);
        #endif
      }
      break;

    case MODE_INIT:
      setupIRComparator();
//     setupMotorPWM();
//      setupFastPwm(PWM187k);
      SetTunings(1.5,0,0);
      uiMode = MODE_SELECT_TRACK;
      break;


    case MODE_SELECT_TRACK:           // file selection from menu
      #ifdef NEXTION
      {
        NexText f =  NexText(1, 1, "f0");
        NexSlider h0 = NexSlider(1, 11, "h0");
        NexTouch *menu[] = { &f, &h0, NULL };

        f.attachPop(trackSelectCallback, menu[0]);
        h0.attachPop(writeMenuStrings, menu[1]);
        
        nexLoop(menu);
      }
      #endif
      break;


    case MODE_PLAY:           // movie is playing
      #ifdef NEXTION
      {
        NexButton qButton = NexButton(2, 22, "q");
        if (showInfo)
          qButton.attachPop(qCallback, &qButton);

        NexButton shiftButton = NexButton(2, 23, "f");
        shiftButton.attachPop(shiftCallback, &shiftButton);

        NexSlider brightnessSlider = NexSlider(2, 5, "b");
        brightnessSlider.attachPop(brightnessCallback, &brightnessSlider);

        NexTouch *controlListen[] = {
          &seekerSlider, &brightnessSlider, &contrastSlider, &volumeSlider, &gamma,
          &repeat, &closeButton, &shiftButton, &qButton, NULL
        };
        nexLoop(controlListen);

        handleEndOfVideo(MODE_PLAY);
      
        // Display elapsed time in format MM:SS
        long seconds = playbackAbsolute / singleFrame / 12.5;
        if (seconds != lastSeconds) {
          lastSeconds = seconds;

          drawTime((char*)"timePos",seconds);

          // Adjust the seekbar position to the current playback position
          uint32_t seekPosition = 256.*playbackAbsolute/videoLength; //(playbackAbsolute << 8) / videoLength;      //
          if (seekPosition != lastSeekPosition) {
            seekerSlider.setValue(seekPosition);
            lastSeekPosition = seekPosition;
          }
          
        }
      }
      #endif
      break;

    case MODE_SHIFTER:
      {
        NexPicture shifter = NexPicture(3, 14, "p0");
        shifter.attachPop(shifterCallback, &shifter);
        NexButton OK = NexButton(3, 7, "OK");
        OK.attachPop(qExitCallback, &OK);
        NexTouch *list[] = { &shifter, &OK, NULL };
        nexLoop(list);

        handleEndOfVideo(MODE_SHIFTER);
      }
      break;
    
    case MODE_Q:
      #ifdef NEXTION
      {
        NexTouch info = NexTouch(4, 0, "I");
        info.attachPop(qExitCallback, &info);
        NexTouch *qListen[] = { &info, NULL };
        nexLoop(qListen);

        handleEndOfVideo(MODE_Q);
      }
      #endif
      break;

    default:
      break;
    
  }
}

// EOF

