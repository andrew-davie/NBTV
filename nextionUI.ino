
#include "televisor.h"
#include "NexSlider.h"
#include "NexText.h"

extern int customBrightness;

NexSlider brightness = NexSlider(0, 8, "brightness");
NexSlider contrast = NexSlider(0, 10, "contrast");
NexSlider volume = NexSlider(0, 11, "volume");
//NexButton b3 = NexButton(0, 9, "b3");


NexTouch *nex_Listen_List[] = {
//    &seek,
//    &gamma,
    &brightness,
    &contrast,
    &volume,
    NULL
};

void brightnessCallback(void *ptr) {
    NexSlider *slider = (NexSlider *) ptr;
    long bright;
    slider->getValue(&bright);
    customBrightness = (int)(256.*(bright-50.)/50.);
    Serial.print(F("BRIGHTNESS = "));
    Serial.println(bright);
}

void contrastCallback(void *ptr) {
    NexSlider *slider = (NexSlider *) ptr;
    long contrast;
    slider->getValue(&contrast);
    Serial.print(F("CONTRAST= "));
    Serial.println(contrast);
}

void volumeCallback(void *ptr) {
    NexSlider *slider = (NexSlider *) ptr;
    long volume;
    slider->getValue(&volume);
    Serial.print(F("VOLUME ="));
    Serial.println(volume);
}

void NextionUiSetup(void) {
    nexInit();
//    seeker.attachPop(b0PopCallback, &seeker);
    brightness.attachPop(brightnessCallback, &brightness);
    contrast.attachPop(contrastCallback, &contrast);
    volume.attachPop(volumeCallback, &volume);
//    gamma.attachPop(b3PopCallback, &gamma);
}

void NextionUiLoop(void) {
    nexLoop(nex_Listen_List);
}



