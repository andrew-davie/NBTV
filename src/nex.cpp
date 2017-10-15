#include "nex.h"

// Private Variables

// Public Variables
int uiMode = MODE_TITLE_INIT;
boolean nextion  = false;    // assume no nextion connected until found
boolean showInfo;            // info txt file present?


// listen class addresses buffer used in nexLoop to generate callbacks
NexTouch *controlListen[10];//*controlListen[10];

NexSlider    volumeSlider     = NexSlider   (2,  8, "v");      
NexSlider    seekerSlider     = NexSlider   (2, 21, "s");
NexSlider    brightnessSlider = NexSlider   (2,  5, "b");
NexSlider    contrastSlider   = NexSlider   (2,  7, "c");
NexButton    stopButton       = NexButton   (2, 16, "x");
NexButton    frameButton      = NexButton   (2, 23, "f"); //is this a duplicate or shift?
NexButton    shiftButton      = NexButton   (2, 23, "f");
NexButton    infoButton       = NexButton   (2, 22, "q");  // was qButton
NexDSButton  gamma            = NexDSButton (2, 15, "g");
NexDSButton  repeat           = NexDSButton (2, 17, "r");

void SetupListenBuffer(void){
  // pass in the state and put the if nextion in here too
  // this sets it up for play
  //could probably leave a lot of these connected all the time
  controlListen[0] = &stopButton;
  controlListen[1] = &seekerSlider;  
  controlListen[2] = &brightnessSlider;   
  controlListen[3] = &contrastSlider;      
  controlListen[4] = &volumeSlider;    
  controlListen[5] = &gamma;  
  controlListen[6] = &repeat;  
  controlListen[7] = &shiftButton;   
  controlListen[8] = &infoButton;      
  controlListen[9] = NULL;    
  //NexTouch *controlListen[] = {&stopButton, &seekerSlider, &brightnessSlider, &contrastSlider, &volumeSlider, &gamma, &repeat, &shiftButton, &infoButton, NULL};
  // until stop button works we have not fixed it
    
  if (showInfo){
    infoButton.attachPop(infoCallback, &infoButton);   // info button  
  }
  shiftButton.attachPop(shiftCallback, &shiftButton);  
  brightnessSlider.attachPop(brightnessCallback, &brightnessSlider); 
}

void infoCallback(void *) {        // rename qCallback to something useful
  sendCommand("page 4");        // show info page should rename page 4
  uiMode = MODE_INFO_SCREEN;    // set mode as info page
}

void nextionAttach(void){
  contrastSlider.attachPop(contrastCallback, &contrastSlider);
  volumeSlider.attachPop(volumeCallback, &volumeSlider);
  gamma.attachPush(gammaCallback, &gamma);
  repeat.attachPush(repeatCallback, &repeat);
  stopButton.attachPush(stopButtonCallback, &stopButton);
  seekerSlider.attachPop(seekerSliderCallback, &seekerSlider);
}


