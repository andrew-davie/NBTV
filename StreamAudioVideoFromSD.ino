#include "televisor.h"
#include "StreamAudioVideoFromSD.h"

volatile byte circularAudioVideoBuffer[CIRCULAR_BUFFER_SIZE];
volatile unsigned int bitsPerSample;
volatile unsigned long playbackAbsolute;
unsigned long streamAbsolute;
unsigned long sampleRate;

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


StreamAudioVideoFromSD::StreamAudioVideoFromSD() {

//  pinMode(PIN_AUDIO, OUTPUT);     // speakerPin  -- TODO  (will be pin 10)

  // SD card initialisation
  
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


boolean StreamAudioVideoFromSD::wavInfo(char* filename) {

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
        nbtv.read(&bitsPerSample,2);
        #ifdef SHOW_WAV_STATS
          Serial.print(F("bps: "));
          Serial.println(bitsPerSample);
        #endif
        
        // Potential "ExtraParamSize/ExtraParams" ignored because PCM

        nbtv.seek(position+chunkSize);
        continue;
      }

      if (!strncmp(data,"data",4)) {                // backwards "data"
        #ifdef SHOW_WAV_STATS
          Serial.println(F("data chunk"));
        #endif
        
        nbtv.read(&chunkSize,4);
        #ifdef SHOW_WAV_STATS
          Serial.print(F("size: "));
          Serial.println(chunkSize);
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

    return true;
  #else
    return false;
  #endif
}


void StreamAudioVideoFromSD::play(char* filename, unsigned long seekPoint) {

#ifdef SDX
  //stopPlayback();
  if (!wavInfo(filename))
      return;
  
  if (seekPoint) {
      long seekTo = sampleRate * bitsPerSample * 2 * seekPoint / 8 + nbtv.position();
      #ifdef DEBUG
        Serial.print("Seek to ");
        Serial.println(seekTo);
      #endif
      
      for (long i = 0; i < seekTo; i++)
        nbtv.read(circularAudioVideoBuffer, 1);
      //nbtv.seek( seekTo );
  }
  playbackAbsolute = 0;
  streamAbsolute = 0;
  nbtv.read( circularAudioVideoBuffer, CIRCULAR_BUFFER_SIZE );      // pre-fill the circular buffer so it's valid
 
  noInterrupts();
  
  // Start the timer(s)
  // Uses /1 divisor, but counting up to 'resolution' each time  --> frequency!
    
  ICR3 = (int)((16000000. / sampleRate)+0.5);         // playback frequency
  TCCR3A = _BV(WGM31) | _BV(COM3A1);
  TCCR3B = _BV(WGM33) | _BV(WGM32) | _BV(CS30);       // see pp.133 ATmega32U4 manual for table
  TIMSK3 |= (_BV(ICIE3) | _BV(TOIE3));            //WGM = 1110 --> FAST PWM with TOP in ICR :)
  
  interrupts();
#endif
}

boolean alreadyStreaming = false;

ISR(TIMER3_CAPT_vect) {

  #ifdef SDX
    if (!alreadyStreaming) {
      alreadyStreaming = true;    // prevent THIS interrupt from interrupting itself...
      interrupts();               // but allow other interrupts while this one is running
  
    // This code tries to fill up the unused part(s) of the circular buffer that contains the streaming audio & video
    // from the WAV file being played. There are two pointers; 'streamPointer' which is the beginning of the buffer to
    // which data can be written, and 'playbackPointer' which is the interrupt's current pointer to the sample to play.
    // Note that 'playbackPointer' can (and will!) change while this routine is streaming data from the SD.  The hope
    // is that we can read data from the SD to the buffer fast enough to keep the interrupt happy.
    // Some safeguards and statistics are included;  if the interrupt "catches up" to the stream pointer, then that means
    // that the interrupt has nothing to play; in that case it can't do anything but stop and wait - this will cause
    // an image glitch and force resynch on the disc rotation.
    
      unsigned long bytesToStream = playbackAbsolute - streamAbsolute;
      if (bytesToStream > STREAM_THRESHOLD) {
        long bufferOffset = streamAbsolute % CIRCULAR_BUFFER_SIZE;

        if ( bufferOffset + bytesToStream >= CIRCULAR_BUFFER_SIZE )
          bytesToStream = CIRCULAR_BUFFER_SIZE - bufferOffset;
        
        nbtv.read( circularAudioVideoBuffer + bufferOffset, bytesToStream );
        
        streamAbsolute += bytesToStream;
      }
      
      alreadyStreaming = false;
    }

    #endif
    
}

//--------------------------------------------------------------------------------------------------------------------
// TIMER3_OFV_vect
// This is an interrupt that runs at the frequency of the WAV file data (i.e, 22050Hz, 44100Hz, ...)
// It takes data from the circular buffer 'circularAudioVideoBuffer' and sends it to the LED (and soon, to the speaker).
// TODO:
// * Send audio to speaker
// * Add brightness control
// * Add contrast (how?)
// * Handle sync pulses and clamping

// Klaas:
/*
- Contrast control = multiply by a certain value.
- DC-restoring = clamping.
- Brightness control = add / subtract a certain value.
- Truncation, that is limiting from 00 to FF,
- This clips off the sync pulses too.
- Gamma correction.
- PWM setting.
*/

int customBrightness = 0;
double customContrast = 1.0;
boolean customGamma = true;

ISR(TIMER3_OVF_vect) {

  int bright;
  int pbp = playbackAbsolute % CIRCULAR_BUFFER_SIZE;

  if (bitsPerSample==16) {

    playbackAbsolute += 4;

    int *bp = (int *)(circularAudioVideoBuffer+pbp);
    int b2 = *bp;

    #ifdef BACKFILL_BUFFER
      *bp = 0;        // mask any buffer overflow
    #endif
      
    b2 *= customContrast;
  
    // Trim off anything below zero - typically these are the sync pulses, but that's not guaranteed
    if (b2 < 0)
      b2 = 0;
    
    bright = b2 / 64;        // The /64 downshifts. multiply up for contrast adjust

  } else { // assume 8-bit, NBTV WAV file format

    playbackAbsolute += 2;
    bright = circularAudioVideoBuffer[pbp];
    #ifdef BACKFILL_BUFFER
      circularAudioVideoBuffer[pbp] = 0;        // mask any overflow by using black
    #endif
    bright *= customContrast;
  }


  bright += customBrightness;

  if (bright < 0)
    bright = 0;
  else if (bright > 255)
    bright = 255;   

  if (customGamma)
    bright = pgm_read_byte(&gamma8[bright]);
    
  // Send brightness to LED array
  OCR4A = (byte) (bright & 0xFF);
  DDRC|=1<<7;    // Set Output Mode C7
  TCCR4A=0x82;  // Activate channel A
}



