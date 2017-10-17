#ifndef _NEX_H_
#define _NEX_H_

// Uses the Nextion Library at https://github.com/itead/ITEADLIB_Arduino_Nextion
// backup at http://www.taswegian.com/ITEADLIB_Arduino_Nextion-master.zip
#include "NexHardware.h"
#include "NexSlider.h"
#include "NexText.h"
#include "NexVariable.h"
#include "NexButton.h"
#include "NexDualStateButton.h"
#include "NexPicture.h"
#include "NexPage.h"

#define NEXTION                                       // include Nextion LCD functionality
#define DEBUG_TIMING                                  // hardware pin toggle debugging


// 1: see: http://support.iteadstudio.com/support/discussions/topics/11000012238
//    --> Delete NexUpload.h and NexUpload.cpp from Nextion library folder
// 2. Disable DEBUG_SERIAL_ENABLE in NexConfig.h
// 3. set   #define nexSerial Serial1 in NexConfig.h
// NexHardware.cpp change  dbSerialBegin(9600);    nexSerial.begin(9600);
//                  to   dbSerialBegin(115200);  nexSerial.begin(115200); to

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// states - add proper state machine
// buttons can be signals that run with in states
// use enum here?
#define MODE_INIT 0
#define MODE_TITLE 1
#define MODE_TITLE_INIT 2
#define MODE_SELECT_TRACK 3
#define MODE_PLAY 4
#define MODE_SHIFTER 5
#define MODE_INFO_SCREEN 6

// Puublic Variables
extern boolean nextion;
extern boolean showInfo;
extern int uiMode;

// Puublic Classes
extern NexSlider seekerSlider;
extern NexSlider contrastSlider;
extern NexSlider volumeSlider;
extern NexDSButton gamma;
extern NexDSButton repeat;
extern NexButton stopButton;
extern NexButton frameButton;    //????

// these wont need externing later?
extern NexTouch *controlListen[];

extern NexSlider brightnessSlider;
extern NexButton shiftButton;
extern NexButton qButton;

// Calback prototypes
void contrastCallback     (void *);
void volumeCallback       (void *);
void gammaCallback        (void *);
void repeatCallback       (void *);
void stopButtonCallback   (void *);
void seekerSliderCallback (void *);
void infoCallback         (void *);
void shiftCallback        (void *);
void brightnessCallback   (void *);


// Function prototypes
void SetupListenBuffer(void);
void nextionAttach(void);

#endif
