
#ifndef StreamAudioVideoFromSD_h
#define StreamAudioVideoFromSD_h


#define HANDLE_TAGS

class StreamAudioVideoFromSD {

private:
  unsigned int sampleRate;
  unsigned int resolution;
  unsigned long streamMaximum;

public:
  
  StreamAudioVideoFromSD();
  void readAudioVideoFromSD();
  boolean wavInfo(char* filename);
  void play(char* filename, unsigned long seekPoint=0);
};

#endif




