// NOTE using 70% ram or more can make the upload fail, hold the reset button down and release when upload starts to fix it
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino Mechanical Televisor                                                                                               //
// This program implements all the functions needed for a working mechanical televisor using an a                             //
// Arduino Micro. For more information see https://www.taswegian.com/NBTV/forum/viewforum.php?f=28                            //

// program by Andrew Davie (andrew@taswegian.com), March 2017...
// and Keith Colson (Keith@edns.co.nz), September 2017...

// Notes:
// 1. Strings in the code actually use RAM.  To save RAM, use the macro F("string")
// 2. Debug mode is entered by sending a character on the console within two seconds of startup


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "televisor.h" // the main header file

// *************************** Nextion Library Support (touch screen) *********************************************************
#include "nex.h"       // custom file to offload of all the nextion source code

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

boolean debug = false;

uint32_t selection = 0;             // selected track # from menu
uint32_t lastSeekPosition;
long lastSeconds;
uint32_t shiftFrame = 75; //110;

long dataPosition;        // WAV file start of data chunk (for rewind/seek positioning)

// PID variables
double integral;
double lastTime;       // this is used to calculate delta time

boolean nexAttached;    //?????

int customBrightness = 0;
uint32_t customGamma = true;                          // technically this could/should be volatile
uint32_t customRepeat = 0;
volatile long customContrast2 = 256;                  // a X.Y fractional  (8 bit fractions) so 256 = 1.0f   and 512 = 2.0f

byte logVolume = 0;

//volatile this probably slows it down a lot and is not needed especialy if it is coded right
// How so?

volatile byte circularAudioVideoBuffer[CIRCULAR_BUFFER_SIZE];
volatile unsigned long playbackAbsolute;
volatile unsigned short pbp = 0;

unsigned long videoLength;
volatile unsigned long streamAbsolute;
volatile unsigned int bufferOffset = 0;
unsigned long sampleRate;
long singleFrame;
short bytesPerSample;               // bytes per sample

volatile unsigned long lastDetectedIR = 0;

File nbtv;
SdFat nbtvSD;

void play(char *filename, unsigned long seekPoint = 0);
boolean getFileN(int n, int s, char *name, boolean reset, boolean strip);

int countFiles();
void resetStream(long seeker);
void setupFastPwm(int mode);
void setupMotorPWM();
void qInit(char *name);
void drawTime(char *name, int sec);
void composeMenuItem(int item, int s, char *p, boolean hunt);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
    82, 83, 84, 85, 87, 88, 89, 90, 91, 92, 94, 95, 96, 97, 99, 100,
    101, 102, 104, 105, 106, 107, 109, 110, 111, 113, 114, 115, 117, 118, 119, 121,
    122, 123, 125, 126, 128, 129, 130, 132, 133, 135, 136, 138, 139, 141, 142, 144,
    145, 147, 148, 150, 151, 153, 154, 156, 157, 159, 160, 162, 164, 165, 167, 168,
    170, 172, 173, 175, 177, 178, 180, 182, 183, 185, 187, 188, 190, 192, 194, 195,
    197, 199, 201, 202, 204, 206, 208, 209, 211, 213, 215, 217, 219, 220, 222, 224,
    226, 228, 230, 232, 234, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255


};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// PID (from http://brettbeauregard.com/blog/2011/04/improving-the-beginners-pid-introduction/)
byte calculatePI(double speedErr, double frameErr) { // pass in error
// could limit the max pwm change to smooth things
    double now = ( (double) playbackAbsolute ) / singleFrame;
    double timeChange = now - lastTime;
    double proportional;

    if (lastTime == 0) {                 // avoids glitch on first round
        lastTime = now;
        return 0;                        // return zero for error (no motor action will be taken)
    }
    proportional = KP_SPEED * speedErr;  // do this multiplication just once

    integral += ( frameErr * timeChange ); // this is the integral value

    if (speedErr > LARGE_SPEED_ERR)      // if speed error is large then turn off integral
        integral = 0;                    // reset the integral

    if (speedErr < MAX_FRAME_ERR)        // if speed error is small then tune the frame
        proportional = proportional + ( frameErr * KP_FRAME ); // this is how aggressive it hunts the frame edge

    double motorSpeed = proportional + ( KI_SPEED * integral ) + 0.5;

    if (motorSpeed > 255)                // could convert to byte before doing the next lines on a double! saves CPU
        motorSpeed = 255;
    if (motorSpeed < 1)                  // if smaller than 1
        motorSpeed = 1;                  // return 1 for no error

    lastTime = now;
    return (byte) motorSpeed;
}



//-- Analog comparator interrupt service routine -------------------------------------------------
// Triggered by sync hole detected by IR sensor, connected to pin 7 (AC)

ISR(ANALOG_COMP_vect)
{
//  Serial.println(F("IR"));
    unsigned long irAbsolute = playbackAbsolute;

    double deltaSample = irAbsolute - lastDetectedIR;
    // Serial.println(deltaSample); always = 3072 to 3074 when synced

    if (deltaSample < 1000)                           //debounce or stopped
        return;

    lastDetectedIR = irAbsolute;                      // store for use on next cycle

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

    int32_t pbDelta = irAbsolute % singleFrame;       // how inaccurate is framing?

    if (pbDelta >= singleFrame / 2)                   // its faster to re frame in the other direction
        pbDelta = -( singleFrame - pbDelta );         // this needs to be a negative number to get the other direction

    //Serial.println(pbDelta);  // max about 3100 nominal about 75 shiftFrame = 75 so thats the matching number!

    //TODO: The 3072 is hardwired, of course - needs to be variable so we can handle other frequencies

    double sError = ( deltaSample - ( 3072 - 7 ) );   // spead error - proportional with target of 3072 - bodge to get P perfectly balanced
    double fError = ( pbDelta - 120 + 96 );                 // frame error

    byte pidx = calculatePI(sError, fError);          // write the new motor PI speed

    if (pidx > 0)                                     // if it is a good return value
        OCR0B = pidx;                                 // set motor pwm value
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
             | bit(COM0A1)                            // COM %10 = non-inverted
             | bit(COM0B1)                            // COM %10 = non-inverted
             | bit(WGM01)
             | bit(WGM00)
    ;

    TCCR0B = 0
             //| bit(WGM02)
             | bit(CS02)                              // prescalar %010 = /1 = 16000000/256/1 = 62.5K
             //| bit(CS00)
    ;

    OCR0B = 0xFF;                                     // set motor pwm full throttle!
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The callback "happens" when there is a change in the scroll position of the dialog.  This
// means that we need to update one or all of the 8 text lines with new contents. When the up
// or down arrows are pressed, the Nextion automatically does the scrolling up/down for the
// lines that are visible on the screen already, and we only need to update the top (0 for up)
// or the bottom (7 for down) line.  However, if the slider was used, we have to update all
// of the lines (8) - so this is somewhat slower in updating.  Not too bad though.

#define REFRESH_ALL_LINES 9

void refresh(int base, int i) {
    char nx[64];
    strcpy(nx, "f0.txt=\"");
    nx[1] = '0' + i;
    composeMenuItem(base + i, sizeof( nx ) - 8, nx + 8, true);
    strcat(nx, "\"");
    sendCommand(nx);
}


void writeMenuStrings(void * = NULL) {

    // Ask the Nextion which line(s) require updating.
    // 0 or 7, or 9 for all

    uint32_t updateReq;
    NexVariable requiredUpdate = NexVariable(1, 15, "ru");
    if (!requiredUpdate.getValue(&updateReq))
        updateReq = REFRESH_ALL_LINES;                // do all as a fallback

    // Get the "base" from the Nextion. This is the index, or starting file # of the first line
    // in the selection dialog. As we scroll up, this decreases to 0. As we scroll down, it
    // increases to the maximum file # (held in the slider's max value).

    uint32_t base;
    NexVariable baseVar = NexVariable(1, 14, "bx");
    if (!baseVar.getValue(&base)) {
        if (debug)
            Serial.println(F("Failed getting base"));
        base = 0;
    }

    if (debug) {
        Serial.print(F("Base="));
        Serial.println(base);
    }

    // Based on 'requiredUpdate' from the Nextion we either update only the top line (when up arrow pressed),
    // only the bottom line (when down arrow pressed), or we update ALL of the lines (slider dragged). The
    // latter is kind of slow, but that doesn't really matter.

    if (updateReq == REFRESH_ALL_LINES)
        for (int i = 0; i < 8; i++)
            refresh(base, i);
    else
        refresh(base, updateReq);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void prepareControlPage() {

    // Fill in the one-time-only objects on the page

    char nx[64];

    sprintf(nx, "g.val=%d", (int) customGamma);
    sendCommand(nx);
    sprintf(nx, "r.val=%d", (int) customRepeat);
    sendCommand(nx);

    // Set "i" button ORANGE if there's no text
    sprintf(nx, "q.pic=%d", showInfo ? 13 : 20);
    sendCommand(nx);
    sprintf(nx, "q.pic2=%d", showInfo ? 15 : 20);
    sendCommand(nx);

    drawTime((char *) "tmax", videoLength / ( sampleRate * 2 ));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void commenceSelectedTrack(boolean firstTime = true) {

    char nx[64];

    // Restart from the beginning once the no-nextion version runs out of tracks to play
    if (!nextion && !getFileN(selection, sizeof( nx ), nx, true, false))
        selection = 0;

    if (getFileN(selection, sizeof( nx ), nx, true, false)) {

        if (firstTime) {
            customRepeat = false;                     // 0 on new track, or keep existing if repeating
            qInit(nx);                                // read and buffer INFO lines --> nextion
        }

        lastSeconds =
            lastSeekPosition = 99999;

        setupMotorPWM();                              // turn motor on at speed to spool up
        setupFastPwm(PWM187k);                        // get pwm ready for light source

        play(nx);

        digitalWrite(PIN_SOUND_ENABLE, HIGH);                                                                                   // enable amplifier



        // Strip the extension off the file name and send the result (title of the track) to the nextion
        // stored in the stn (stored track name) variable. This is loaded by the Nextion at start of page 2 display

        if (nextion) {
            char nx2[64];
            strcpy(nx2, "stn.txt=\"");
            strcat(nx2, nx);
            strcpy(strstr(nx2, ".wav"), "\"");
            sendCommand(nx2);                         // --> stn.txt="track name"
        }

        if (firstTime) {

            playbackAbsolute = 0;
            customBrightness = 0;
            customContrast2 = 256;
            customGamma = true;

            nexAttached = false;    //??
            uiMode = MODE_PLAY;

            if (nextion) {
                sendCommand("page 2");
                prepareControlPage();
            }
        }

    }
    else {

//    if (debug) {
//      Serial.println(selection);
//      Serial.println(F("E: /"));
//    }
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void trackSelectCallback(void *) {
    NexVariable selectedItem = NexVariable(1, 10, "si");
    if (selectedItem.getValue(&selection))
        commenceSelectedTrack(true);
}


void brightnessCallback(void *) {
    uint32_t value;
    NexSlider brightnessSlider = NexSlider(2, 5, "b");
    if (brightnessSlider.getValue(&value))
        customBrightness = (int) ( 256. * ( value - 128. ) / 128. );
}


void seekerSliderCallback(void *) {
    uint32_t seekTo;
    if (seekerSlider.getValue(&seekTo)) {
        long seekPos = (long) (
            ( (double) seekTo / 255. ) *
            ( (double) videoLength / singleFrame ) );
        resetStream(seekPos * singleFrame);
    }
}


void contrastCallback(void *) {
    uint32_t value;
    if (contrastSlider.getValue(&value))
        customContrast2 = value << 1;
}


void volumeCallback(void *) {
    uint32_t customVolume;
    volumeSlider.getValue(&customVolume);
    logVolume = pgm_read_byte(&gamma8[customVolume]);
}


void gammaCallback(void *) {
    gamma.getValue(&customGamma);
}


void repeatCallback(void *) {
    if (debug)
        Serial.println(F("Repeat"));
    repeat.getValue(&customRepeat);
}


void shiftButtonCallback(void *) {
    uiMode = MODE_SHIFTER;
}


void stopButtonCallback(void *) {

    // if (debug)
    //     Serial.println(F("Stop"));

    digitalWrite(PIN_SOUND_ENABLE, LOW);              // disable amplifier

    TCCR4B = 0;                                       // effectively turn off LED interrupt
    TCCR3A = 0;
    TCCR3B = 0;                                       // turn off motor buffer stuffer and playback
    TCCR0A = 0;                                       // motor PWM OFF
    OCR0A = 0;                                        // motor OFF (just in case PWM had a spike)

    nbtv.close();                                     // close any previously playing/streaming file

    OCR4A = 0;                                        // blacken LED (=screen)
    DDRC |= 1 << 7;                                   // Set Output Mode C7
    TCCR4A = 0x82;                                    // Activate channel A

    //DDRD |= 1 << 7;
    //TCCR4C |= 0x09;

    uiMode = nextion ? MODE_TITLE_INIT : MODE_INIT;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void qInit(char *name) {

    // Copy video description (if any) from text file to the global variables on the
    // menu page. These are later copied to the description (Q) page lines on init of
    // that page by the Nextion itself. We do it here as globals so that we don't need
    // to flip Nextion pages, and we get auto-init on page draw.

    // Drop off the '.wav' and append '.txt' to give us the description filename
    char nx[64];
    strcpy(nx, name);
    strcpy(strstr(nx, ".wav"), ".txt");

    // grab the video details and write to the Q page
    File explain = nbtvSD.open(nx);
    showInfo = explain;

    for (int i = 0; i < 12; i++) {
        sprintf(nx, "I.t%d.txt=\"", i);
        char *p;
        for (p = nx + 10; *p; p++) {}
        if (explain) {
            do {
                explain.read(p, 1);
            } while (*p++ != 10);
            p--;
        }
        strcpy(p, "\"");
        if (nextion)
            sendCommand(nx);
    }
    explain.close();
}

/*
   void qCallback(void *) {
   sendCommand("page 4");
   uiMode = MODE_INFO_SCREEN;
   }*/

void qExitCallback(void *) {
    sendCommand("page 2");

    nexAttached = false;
    uiMode = MODE_PLAY;
}


void shifterCallback(void *) {
    NexVariable shift = NexVariable(3, 1, "s");
    shift.getValue(&shiftFrame);
}


void shiftCallback(void *) {
    // Enter shifter page
    sendCommand("page 3");
    // TODO: calc H, V and init H,V,S
    uiMode = MODE_SHIFTER;
}


void titleCallback(void *) {

    sendCommand("page 1");                            // --> MENU

    // Count the number of menu items and then set the maximum range for the slider
    // We subtract 8 from the count because there are 9 lines already visible in the window

    int menuSize = countFiles();

    char nx[64];
    sprintf(nx, "h0.maxval=%d", menuSize > 9 ? menuSize - 8 : 0);
    sendCommand(nx);

    writeMenuStrings();                               // defaults to REFRESH_ALL_LINES, so screen is populated
    uiMode = MODE_INIT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The Arduino starts with a one-time call to setup(), so this is where the Televisor is initialised.
// That's followed by calls to loop() whenever there is free time.


void setup() {

    // Turn OFF the sound amp
    pinMode(PIN_SOUND_ENABLE, OUTPUT);
    digitalWrite(PIN_SOUND_ENABLE, LOW);
//    digitalWrite(PIN_SOUND_ENABLE, HIGH);

#ifdef DEBUG_TIMING
    pinMode(DEBUG_PIN, OUTPUT);
    digitalWrite(DEBUG_PIN, LOW);
#endif

    Serial.begin(115200);                             // why slow it down to 9600?

    // IMPORTANT: DEBUG MODE ENTERED BY SENDING CHARACTER TO ARDUINO VIA CONSOLE IN 1st 2 SECONDS
    // Determine if there is debugging is required by looking for the presence of a character in the
    // serial buffer. Thus, we don't "hang" the code if there's no serial port.  It won't matter if we
    // actually write to the port if there's nothing there.

    while (!Serial.available() && millis() < 2000) ;

    if (Serial.available()) {
        while (Serial.available())
            Serial.read();
        Serial.println(F("DEBUG"));
        debug = true;
    }

    // Setup access to the SD card
    pinMode(SS, OUTPUT);
    if (!nbtvSD.begin(SD_CS_PIN))
        if (debug)
            Serial.println(F("SD failed!"));

    nextion = nexInit();
    if (nextion)
        nextionAttach(); // does it really need to setup these callbacks, sounds like they should be done per state
    // maybe nex.cpp should be interface.cpp and purely handle the interface
    // but how to pass things back to the main?
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int countFiles() {

    char name[64];

    int count = 0;
    FatFile *vwd = nbtvSD.vwd();
    vwd->rewind();

    SdFile file;
    while (file.openNext(vwd, O_READ)) {
        file.getName(name, sizeof( name ) - 1);
        file.close();
        if (*name != '.' && strstr(name, ".wav"))
            count++;
    }

    return count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get a filename from the SD card.  Hidden (starting with ".") files are ignored. Any file with
// a ".wav" in the filename is considered a candidate. Function fills a character pointer with the
// filename.
// Parameters
// hunt - true: start from the beginning of the SD file list and return the "nth" file
//       false: just return the very next files

boolean getFileN(int n, int s, char *name, boolean hunt = true, boolean strip = true) {

    FatFile *vwd = nbtvSD.vwd();
    if (hunt)
        vwd->rewind();

    SdFile file;
    while (file.openNext(vwd, O_READ)) {
        file.getName(name, s);
        file.close();
        if (*name != '.') {
            char *px = strstr(name, ".wav");
            if (px) {
                if (strip)
                    *px = 0;
                if (!hunt || ( hunt && !n-- ))
                    return true;
            }
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void composeMenuItem(int item, int s, char *p, boolean hunt) {
    sprintf(p, "%2d ", item + 1);
    if (!getFileN(item, s - 3, p + 3, hunt))
        *p = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void resetStream(long seeker) {

    TIMSK3 &= ~_BV(TOIE3);                            // DISABLE playback interrupt
    digitalWrite(PIN_SOUND_ENABLE, LOW);              // disable amplifier

    nbtv.seek(dataPosition + seeker);                 // seek to new position

    streamAbsolute = playbackAbsolute = seeker;
    lastDetectedIR = playbackAbsolute - singleFrame + shiftFrame;
    lastTime = ( (double) playbackAbsolute ) / singleFrame;     // keep the PID happy

    pbp = 0;
    bufferOffset = 0;

    nbtv.read((void *) circularAudioVideoBuffer, CIRCULAR_BUFFER_SIZE);   // pre-fill the circular buffer so it's valid

    digitalWrite(PIN_SOUND_ENABLE, HIGH);             // enable amplifier

    TIMSK3 |= _BV(TOIE3);                             // ENABLE playback interrupt
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Super dooper high speed PWM (187.5 kHz) using timer 4, used for the LED (pin 13) and audio (pin 6). Timer 4 uses a PLL as
// input so that its clock frequency can be up to 96 MHz on standard Arduino Leonardo/Micro. We limit imput frequency to 48 MHz
// to generate 187.5 kHz PWM. Can double to 375 kHz. ref: http://r6500.blogspot.com/2014/12/fast-pwm-on-arduino-leonardo.html

void setupFastPwm(int mode) {

    TCCR4A = 0;
    TCCR4B = mode;
    TCCR4C = 0;
    TCCR4D = 0;                                       // normal waveform counting up to TOP (in OCR4C) - i.e., 255
    PLLFRQ = ( PLLFRQ & 0xCF ) | 0x30;                // Setup PLL, and use 96MHz / 2 = 48MHz
    OCR4C = 255;                                      // Target count for Timer 4 PWM
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comparator interrupt for IR sensing
// precludes timer 1 usage

void setupIRComparator() {

    ACSR &= ~(
        bit(ACIE)                                     // disable interrupts on AC
        | bit(ACD)                                    // switch on the AC
        | bit(ACIS0)                                  // falling trigger
        );

    ACSR |=
        bit(ACIS1)
        | bit(ACIE)                                   // re-enable interrupts on AC
    ;

// maybe....
    SREG |= bit(SREG_I);                            // GLOBALLY enable interrupts
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

boolean wavInfo(char *filename) {

    nbtv = nbtvSD.open(filename);
    if (!nbtv)
        return false;

    uint32_t header;

    // this should read into a struct
    nbtv.read(&header, 4);
    if (header != 0x46464952)                         //'FFIR'
        return false;

    long chunkSize;
    nbtv.read(&chunkSize, 4);

    nbtv.read(&header, 4);
    if (header != 0x45564157)                         //'EVAW'
        return false;

    while (true) {

        nbtv.read(&header, 4);                        // read next chunk header

        if (header == 0x7674626e) {                   //'vtbn'
            nbtv.read(&chunkSize, 4);
            unsigned long position = nbtv.position();
            nbtv.seek(position + chunkSize);
            continue;
        }

        else if (header == 0x20746D66) {              //' tmf'

            nbtv.read(&chunkSize, 4);
            unsigned long position = nbtv.position();

            int audioFormat;
            nbtv.read(&audioFormat, 2);

            int numChannels;
            nbtv.read(&numChannels, 2);

            nbtv.read(&sampleRate, 4);

            long byteRate;
            nbtv.read(&byteRate, 4);

            int blockAlign;
            nbtv.read(&blockAlign, 2);

            unsigned int bitsPerSample;
            nbtv.read((void *) &bitsPerSample, 2);

            bytesPerSample = bitsPerSample / 8;

            singleFrame = sampleRate * 2 * bytesPerSample / 12.5;

            // Potential "ExtraParamSize/ExtraParams" ignored because PCM

            nbtv.seek(position + chunkSize);
            continue;
        }

        else if (header == 0x61746164) {              //'atad'
            nbtv.read(&videoLength, 4);
            dataPosition = nbtv.position();
            // and now the file pointer should be pointing at actual sound data - we can return

            break;
        }

        return false;
    }

    return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void play(char *filename, unsigned long seekPoint) {

    if (debug)
        Serial.println(filename);

    if (wavInfo(filename)) {

        lastTime  = 0;
        integral  = 0;

        SREG &= ~bit(SREG_I);                         // GLOBALLY disable interrupts

        long seeker = singleFrame * seekPoint * 12.5;
        resetStream(seeker);

        // Uses /1 divisor, but counting up to 'resolution' each time  --> frequency!
        // This is timer 3, which is handling the playback of video and audio

        ICR3 = (int) ( ( 16000000. / sampleRate ) + 0.5 );    // playback frequency

        TCCR3A =
            _BV(WGM31)                                // Fast PWM (n1)   --> n3n2n1 == 111 --> Fast PWM
            | _BV(COM3A1)                             // compare output mode = nA1nA0 = 10 --> Clear OCnA/OCnB/OCnC on compare match
            // (set output to low level); match is when the counter == ICR3
        ;

        TCCR3B =
            _BV(WGM33)                                // Fast PWM (n3) see above
            | _BV(WGM32)                              // Fast PWM (n2) see above
            | _BV(CS30)                               // Clock select = /1 (no scaling) see pp.133 ATmega32U4 manual for table
        ;

        TIMSK3 |= (
            0
//			| _BV(ICIE3)                                  // ENABLE input capture interrupt
            | _BV(TOIE3)                              // ENABLE timer overflow interrupt
            );

        SREG |= bit(SREG_I);                          // GLOBALLY enable interrupts
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Data playback interrupt
// This is an interrupt that runs at the frequency of the WAV file data (i.e, 22050Hz, 44100Hz, ...)
// It takes data from the circular buffer 'circularAudioVideoBuffer' and sends it to the LED array
// and to the speaker. Handles 16-bit and 8-bit format sample data and adjust brightness, contrast,
// and volume on-the-fly.

static volatile unsigned long audio = 0x8000;       // "0x8000" is midway through range
static volatile long bright = 0;
static volatile boolean alreadyStreaming = false;


ISR(TIMER3_OVF_vect) {

    // assume 8-bit, NBTV WAV file format
    // Audio is pre-set to MAXIMUM volume - so we can't get any louder - only quieter
    // so 0xFF multiplier is acutaly 1x

    audio = ( (int) ( circularAudioVideoBuffer[pbp + 1] - 0x80 ) ) * logVolume + 0x8000;

    bright = circularAudioVideoBuffer[pbp] * customContrast2;
    bright >>= 8;
    bright += customBrightness;

    if (bright < 0)
        bright = 0;
    else if (bright > 255)
        bright = 255;

    playbackAbsolute += bytesPerSample * 2;

    #if ( CIRCULAR_BUFFER_MASK != 0 )
    pbp = ( pbp + bytesPerSample * 2 ) & CIRCULAR_BUFFER_MASK;
    #else
    pbp += bytesPerSample * 2;
    if (pbp >= CIRCULAR_BUFFER_SIZE)
        pbp = 0;
    #endif

    OCR4A = customGamma ? pgm_read_byte(&gamma8[bright]) : (byte) bright;
    //DDRC |= 0x80;                                   // Set Output Mode C7 (persistent!)
    TCCR4A = 0x82;                                    // Activate channel A

    OCR4D = ( (byte *) &audio )[1]; //(byte) ( audio >> 8 );                                                                                                    // Write the audio to pin 6 PWM duty cycle
    DDRD |= 0x80;
    TCCR4C |= 0x09;

    // This code tries to fill up the unused part(s) of the circular buffer that contains the streaming
    // audio & video from the WAV file being played. 'bufferOffset' is the location of the next buffer
    // write, and this wraps when it gets to the end of the buffer. The number of bytes to write is
    // calculated from two playback pointers - 'playbackAbsolute' which is the current position in the
    // audio/video stream that's ACTUALLY being shown/heard, and 'streamAbsolute' which is the position
    // of the actually streamed data (less 1 actual buffer length, as it's pre-read at the start). Those
    // give us a way to calculate the amount of free bytes ('bytesToStream') which need to be filled by
    // reading data from the SD card. Note that 'playbackPointer' can (and will!) change while this
    // routine is streaming data from the SD.  The hope is that we can read data from the SD to the buffer
    // fast enough to keep the playback happy. If we can't - then we get glitches on the screen/audio

    if (!alreadyStreaming) {
        alreadyStreaming = true;                      // prevent THIS *part of the code* from interrupting itself...
        interrupts();                                 // but allow this interrupt to interrupt itself

        unsigned int bytesToStream = playbackAbsolute - streamAbsolute;
        if (bytesToStream > 31) {                     // might be more efficient reading larger blocks!

            void *dest = (void *) ( circularAudioVideoBuffer + bufferOffset );
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void drawTime(char *name, int sec) {

    int s = sec % 60;
    int m = sec / 60;

    char cmd[32];
    sprintf(cmd, "%s.txt=\"%2d:%02d\"", name, m, s);
    sendCommand(cmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Check to see if the data for the video has run out
// If so, then we stop the video, the motor, and then handle possible repeat button.
// and remain in the same (sub) mode we might already have been in (INFO or SHIFT, for example)

void handleEndOfVideo(int nextMode) {

    if (playbackAbsolute >= videoLength) {            // Check for end of movie


        if (!nextion)
            selection++;                              // if there's no nextion/UI, cycle to next track

        stopButtonCallback(NULL);                     // stop everyting (same as 'pressing' stop button)

        if (!nextion || customRepeat) {               // BUT we might have the repeat button ON
            commenceSelectedTrack(false);             // in which case we restart the track (NOT first time)
            uiMode = nextMode;                        // and drop back into the previous UI mode
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
    switch (uiMode) {

    //--------------------------------------------------------------------------------------------------------------------------

    case MODE_TITLE_INIT:

        sendCommand("bkcmd=2");                       // Nextion: don't confirm commands!
        sendCommand("page 0");
        uiMode = MODE_TITLE;
        break;

    //--------------------------------------------------------------------------------------------------------------------------

    case MODE_TITLE:
    {

        NexPage titleScreen = NexPage(0, 0, "T");
        titleScreen.attachPop(titleCallback, &titleScreen);
        NexTouch *titleList[] = {
            &titleScreen, NULL
        };
        nexLoop(titleList);
    }
    break;

    //--------------------------------------------------------------------------------------------------------------------------

    case MODE_INIT:

        logVolume = pgm_read_byte(&gamma8[128]);      // init to 1/2 volume
        setupIRComparator();                          // looka like it enables the comparator
        uiMode = MODE_SELECT_TRACK;
        break;

    //--------------------------------------------------------------------------------------------------------------------------

    case MODE_SELECT_TRACK:                           // file selection from menu

        if (nextion) {


            bool skipWaste = false;

            NexText f =  NexText(1, 1, "f0");
            NexSlider h0 = NexSlider(1, 11, "h0");
            NexTouch *menu[] = {
                &f, &h0, NULL
            };

            if (!skipWaste)     //????
            {
                skipWaste = true;
                f.attachPop(trackSelectCallback, menu[0]); // doing these attaches is a waste
                h0.attachPop(writeMenuStrings, menu[1]);
            }

            nexLoop(menu);  // **** this makes the callback happen. basically polling

        }
        else {

            selection = 0;
            commenceSelectedTrack(true);
            customRepeat = true;                      // in this case, play all tracks and repeat
        }
        break;

    //--------------------------------------------------------------------------------------------------------------------------

    case MODE_PLAY:                                   // movie is playing

#ifdef DEBUG_TIMING
        digitalWrite(DEBUG_PIN, HIGH);              // round robin speed
        digitalWrite(DEBUG_PIN, LOW);
#endif

        //stuffer(); // will run from here now ADD #define for this
        // now the stuffer is part of the play interrupt, I think no longer to appear in main loop
        // only problem is it glitches when touching the screen
        // need to handle the nextion call backs smmothly next
        // ABOVE REDUNDANT



//        sCounter++;
//        if (sCounter > 254) { // only update the "time" once per n cycles

        // sCounter appears pointless - the code already only draws the time if the visible value changes


//            sCounter = 0;
        if (nextion) {
            // Display elapsed time in format MM:SS
            long seconds = playbackAbsolute / singleFrame / 12.5;



            if (nexAttached == false)
            {
                //????
                nexAttached = true;
                SetupListenBuffer();   // TODO: on state entry we set it up, on exit we clear it
            }
            nexLoop(controlListen);   // this works fine so it shows that you only need to attach once.

            // Draws the time and adjusts the seekbar ONLY if the visible second value changes

            if (seconds != lastSeconds) {
                lastSeconds = seconds;

                drawTime((char *) "timePos", seconds);

                // Adjust the seekbar position to the current playback position
                uint32_t seekPosition = 256. * playbackAbsolute / videoLength;
                if (seekPosition != lastSeekPosition) {
                    char cmd[] = "s.val=XXXXXXXXX";
                    utoa(seekPosition, cmd + 6, 10); // TODO: value is 0-255 right?  so we only need 3 chars... not 10
                    sendCommand(cmd);
                }
                lastSeekPosition = seekPosition;
            }

        }

        handleEndOfVideo(MODE_PLAY);
        break;

    //--------------------------------------------------------------------------------------------------------------------------

    case MODE_SHIFTER:

        // AXIOM! if (nextion)
    {
        NexPicture shifter = NexPicture(3, 14, "p0");
        shifter.attachPop(shifterCallback, &shifter);
        NexButton OK = NexButton(3, 7, "OK");
        OK.attachPop(qExitCallback, &OK);
        NexTouch *list[] = {
            &shifter, &OK, NULL
        };
        nexLoop(list);

        handleEndOfVideo(MODE_SHIFTER);
    }
    break;

    //--------------------------------------------------------------------------------------------------------------------------

    case MODE_INFO_SCREEN:

        // AXIOM! if (nextion)
    {
        NexTouch info = NexTouch(4, 0, "I");
        info.attachPop(qExitCallback, &info);
        NexTouch *qListen[] = {
            &info, NULL
        };
        nexLoop(qListen);

        handleEndOfVideo(MODE_INFO_SCREEN);
    }
    break;

    //--------------------------------------------------------------------------------------------------------------------------

    default:
        // ? how
        break;

    }
}

// EOF
