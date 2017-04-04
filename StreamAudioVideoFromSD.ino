#include "televisor.h"
#include "StreamAudioVideoFromSD.h"
#include <SdFat.h>


extern File nbtv;
extern SdFat nbtvSD;
extern double brightness;

volatile byte circularAudioVideoBuffer[CIRCULAR_BUFFER_SIZE];
volatile unsigned long playbackAbsolute = 0;
volatile unsigned int bitsPerSample;
unsigned long streamAbsolute = 0;

extern const uint8_t gamma8[];


StreamAudioVideoFromSD::StreamAudioVideoFromSD() {

//  pinMode(PIN_AUDIO, OUTPUT);     // speakerPin  -- TODO  (will be pin 10)

  //TODO: junk this after testing!
  pinMode(3,OUTPUT);
  pinMode(5,OUTPUT);
  pinMode(6,OUTPUT);
  pinMode(9,OUTPUT);
  pinMode(10,OUTPUT);
  pinMode(11,OUTPUT);
  pinMode(12,OUTPUT);   //? 
  pinMode(13,OUTPUT);  

  
  sampleRate = 0;
  resolution = 0;
  streamMaximum = 0;

  playbackAbsolute = 0;
  streamAbsolute = 0;
}



//#define SHOW_WAV_STATS

boolean StreamAudioVideoFromSD::wavInfo(char* filename) {

    nbtv = nbtvSD.open(filename);
    if (!nbtv) {
        #if defined(DEBUG)
          Serial.print("Error when opening file '");
          Serial.print(filename);
          Serial.print("'");
        #endif
        return false;
    }


    char data[4];
    nbtv.read(data,4);

    if (strncmp(data,"RIFF",4)) {
      #ifdef SHOW_WAV_STATS
        Serial.println("Error: WAV has no RIFF header");
      #endif
      return false;
    }

    long chunkSize;
    nbtv.read(&chunkSize,4);
    #ifdef SHOW_WAV_STATS
      Serial.print("WAV size: ");
      Serial.println(chunkSize+8);
    #endif
    
    nbtv.read(data,4);
    if (strncmp(data,"WAVE",4)) {
      #ifdef DEBUG
        Serial.println("Error: WAVE header incorrect");
      #endif
      return false;
    }

    long position;
    while (true) {

      nbtv.read(data,4);        // read next chunk header

      #ifdef SHOW_WAV_STATS
        for (int i=0; i<4; i++){
          Serial.print("'");
          Serial.print(data[i]);
          Serial.print("'");
        }
        Serial.println();
      #endif
      
      if (!strncmp(data,"nbtv",4)) {
        #ifdef SHOW_WAV_STATS
          Serial.println("'nbtv' chunk");
        #endif
        nbtv.read(&chunkSize,4);
        #ifdef SHOW_WAV_STATS
          Serial.print("size: ");
          Serial.println(chunkSize);
        #endif
        position = nbtv.position();
        nbtv.seek(position+chunkSize);
        continue;
      }

      if (!strncmp(data,"fmt ",4)) {
        #ifdef SHOW_WAV_STATS
          Serial.println("'fmt ' chunk");
        #endif

        nbtv.read(&chunkSize,4);
        #ifdef SHOW_WAV_STATS
          Serial.print("Size: ");
          Serial.println(chunkSize);
        #endif
        
        position = nbtv.position();
        
        int audioFormat;
        nbtv.read(&audioFormat,2);
        #ifdef SHOW_WAV_STATS
          Serial.print("Audio format: ");
          Serial.println(audioFormat);
        #endif
        int numChannels;
        nbtv.read(&numChannels,2);
        #ifdef SHOW_WAV_STATS
          Serial.print("# chans: ");
          Serial.println(numChannels);
        #endif
        //        long sampleRate;
        nbtv.read(&sampleRate,4);
        #ifdef SHOW_WAV_STATS
          Serial.print("rate: ");
          Serial.println(sampleRate);
        #endif
        long byteRate;
        nbtv.read(&byteRate,4);
        #ifdef SHOW_WAV_STATS
          Serial.print("byte rate: ");
          Serial.println(byteRate);
        #endif
        int blockAlign;
        nbtv.read(&blockAlign,2);
        #ifdef SHOW_WAV_STATS
          Serial.print("align: ");
          Serial.println(blockAlign);
        #endif
        nbtv.read(&bitsPerSample,2);
        #ifdef SHOW_WAV_STATS
          Serial.print("bps: ");
          Serial.println(bitsPerSample);
        #endif
        
        // Potential "ExtraParamSize/ExtraParams" ignored because PCM

        nbtv.seek(position+chunkSize);
        continue;
      }

      if (!strncmp(data,"data",4)) {                // backwards "data"
        #ifdef SHOW_WAV_STATS
          Serial.println("data chunk");
        #endif
        
        nbtv.read(&chunkSize,4);
        #ifdef SHOW_WAV_STATS
          Serial.print("size: ");
          Serial.println(chunkSize);
        #endif
        
        //position = nbtv.position();
        // and now the file pointer should be pointing at actual sound data - we can return

        break;
      }
      
      #ifdef DEBUG      
        Serial.print("unrecognised chunk in WAV: '");
        for (int i=0; i<4; i++)
          Serial.print(data[i]);
        Serial.println("'");
      #endif
      return false;        
    }

    
    
    return true;
}


void StreamAudioVideoFromSD::play(char* filename, unsigned long seekPoint = 0) {

  //stopPlayback();
  if (!wavInfo(filename))
      return;
  
  Serial.println("Seeking");
  
  if (seekPoint > 0) {
      seekPoint = (sampleRate * seekPoint) + nbtv.position();
      nbtv.seek(seekPoint); // skip the header info
  }
  
  // Pre-read as much as possible for playback. Then off goes 'playbackAbsolute' and streamAbsolute will try to catch up

  playbackAbsolute = 0;
  nbtv.read( circularAudioVideoBuffer, CIRCULAR_BUFFER_SIZE );      // pre-cache as many samples as possible
     
  #ifdef DEBUG
    Serial.print("Sample rate = ");
    Serial.println(sampleRate);
  #endif
  
  resolution =  (int)((16000000. / sampleRate)+0.5);
  
  byte tmp = (nbtv.read() + nbtv.peek()) / 2;
  
  noInterrupts();
  
  // Start the timer(s)
  // Uses /1 divisor, but counting up to 'resolution' each time  --> frequency!
    
  ICR3 = resolution;
  TCCR3A = _BV(WGM31) | _BV(COM3A1);

  // see pp.133 ATmega32U4 manual for table
  
  TCCR3B = _BV(WGM33) | _BV(WGM32) | _BV(CS30);
  
  //WGM = 1110 --> FAST PWM with TOP in ICR :)
  
  TIMSK3 |= (/*_BV(ICIE3) |*/ _BV(TOIE3));
  
  interrupts();
}


//ISR(TIMER3_CAPT_vect) {
//}

//#ifdef DEBUG
//  volatile boolean criticalTimingError = false;
//  int criticalTimingCounter = 0;
//#endif


//--------------------------------------------------------------------------------------------------------------------
// TIMER3_OFV_vect
// This is an interrupt that runs at the frequency of the WAV file data (i.e, 22050Hz, 44100Hz, ...)
// It takes data from the circular buffer 'circularAudioVideoBuffer' and sends it to the LED (and soon, to the speaker).
// TODO:
// * Send audio to speaker
// * Add brightness control
// * Add contrast (how?)
// * Handle sync pulses and clamping

ISR(TIMER3_OVF_vect) {

  int pbp = playbackAbsolute % CIRCULAR_BUFFER_SIZE;

  if (bitsPerSample==16) {

    int b2 = ( circularAudioVideoBuffer[pbp] << 8 ) | circularAudioVideoBuffer[pbp+1];  
  
    // Trim off anything below zero - typically these are the sync pulses, but that's not guaranteed
    if (b2 < 0)
      b2 = 0;
    
    int brightAdjusted = b2 / 64 + 20;     // <-- acts like a contrast knob
    if (brightAdjusted > 255)
      brightAdjusted = 255;   
  
    // Send brightness to LED array
    OCR4A = pgm_read_byte(&gamma8[brightAdjusted]);              
    DDRC|=1<<7;    // Set Output Mode C7
    TCCR4A=0x82;  // Activate channel A

    // Move along to the next sample
    playbackAbsolute += 4;

  } else { // assume 8-bit, NBTV WAV file format

    byte b2 = circularAudioVideoBuffer[pbp+1];
    OCR4A = pgm_read_byte(&gamma8[b2]);
    DDRC|=1<<7;    // Set Output Mode C7
    TCCR4A=0x82;  // Activate channel A
  
    // Move along to the next sample
    playbackAbsolute += 2;
  }
  
}

//--------------------------------------------------------------------------------------------------------------------
// readAudioVideoFromSD

// This function tries to fill up the unused part(s) of the circular buffer that contains the streaming audio & video
// from the WAV file being played. There are two pointers; 'streamPointer' which is the beginning of the buffer to
// which data can be written, and 'playbackPointer' which is the interrupt's current pointer to the sample to play.
// Note that 'playbackPointer' can (and will!) change while this routine is streaming data from the SD.  The hope
// is that we can read data from the SD to the buffer fast enough to keep the interrupt happy.
// Some safeguards and statistics are included;  if the interrupt "catches up" to the stream pointer, then that means
// that the interrupt has nothing to play; in that case it can't do anything but stop and wait - this will cause
// an image glitch and force resynch on the disc rotation.

void StreamAudioVideoFromSD::readAudioVideoFromSD() {
  
//  #ifdef DEBUG
//
//    // Catch (and clear) any timing/overflow error signalled by the interrupt
//    if ( criticalTimingError ) {
//      criticalTimingError = false;
//      criticalTimingCounter++;
//      streamMaximum = 0;                          // reset for a better look at what happens immediately after
//      Serial.print("Critical timing error #");
//      Serial.println(criticalTimingCounter);
//    }
//  #endif

  unsigned long bytesToStream = playbackAbsolute - streamAbsolute;

  //Serial.println(bytesToStream);
  
  if (bytesToStream) {



   #ifdef DEBUG

//      Serial.print("PBA: ");
//      Serial.print(playbackAbsolute);
//      Serial.print(" STRA: ");
//      Serial.print(streamAbsolute);
      

      // Diagnose the largest 'gap' which needs to be filled. This shows in effect how well the SD streaming
      // is going and gives the worst-case buffer size requirement. Of course we could run out of space and that
      // would be a bit of a shame. So we probably need to use the fastest SD card we can find.
      
//      if ( bytesToStream > streamMaximum ) {
//        streamMaximum = bytesToStream;
//        Serial.print("MAX = ");
//        Serial.println(streamMaximum);

//        if ( streamMaximum >= CIRCULAR_BUFFER_SIZE )
//          Serial.println("CRITICAL: buffer too small!" );
//        }
    #endif

    // Handle end of (circular) buffer by splitting into separate reads from SD card
    long bufferOffset = streamAbsolute % CIRCULAR_BUFFER_SIZE;
    if ( bufferOffset + bytesToStream > CIRCULAR_BUFFER_SIZE )
      bytesToStream = CIRCULAR_BUFFER_SIZE - bufferOffset;

//    Serial.print(" ");
//    Serial.println(bytesToStream);
    
    // Grab the approrpiate amount of bytes from the SD WAV file
    nbtv.read( circularAudioVideoBuffer + bufferOffset, bytesToStream );
    
    streamAbsolute += bytesToStream;
    bytesToStream = playbackAbsolute - streamAbsolute;
  }
}


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

