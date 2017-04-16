
#ifndef StreamAudioVideoFromSD_h
#define StreamAudioVideoFromSD_h

#include <SdFat.h>

#ifdef SDX
  File nbtv;
  SdFat nbtvSD;
#endif

void setupStreamAudioVideoFromSD();
boolean wavInfo(char* filename);
void play(char* filename, unsigned long seekPoint=0);

#endif




