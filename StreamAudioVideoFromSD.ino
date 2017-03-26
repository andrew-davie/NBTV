#include "televisor.h"
#include "StreamAudioVideoFromSD.h"
#include <SdFat.h>


extern File nbtv;
extern SdFat nbtvSD;
extern double brightness;

volatile unsigned int playbackPointer = 0;
volatile int streamPointer = 0;
volatile byte circularAudioVideoBuffer[CIRCULAR_BUFFER_SIZE];

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
}





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

    nbtv.seek(8);
    char wavStr[] = { 'W', 'A', 'V', 'E' };
    for (byte i = 0; i < 4; i++) {
        char c = nbtv.read();
        if (c != wavStr[i]) {
#if defined(DEBUG)
            Serial.println("Error: WAVE header incorrect");
#endif
            return false;
        }
    }


    

    byte stereo, bps;
    nbtv.seek(22);
    stereo = nbtv.read();
    nbtv.seek(24);

    #if defined(DEBUG)
      Serial.print("STEREO=");
      Serial.println(stereo);
    #endif


    sampleRate = nbtv.read();
    sampleRate = nbtv.read() << 8 | sampleRate;

    #if defined(DEBUG)
      Serial.print("sampleRate=");
      Serial.println(sampleRate);
    #endif


      nbtv.seek(34);
      bps = nbtv.read();
      bps = nbtv.read() << 8 | bps;
  
      #if defined(DEBUG)
        Serial.print("BPS=");
        Serial.println(bps);
      #endif
      
    #if defined(HANDLE_TAGS)

      #if defined(DEBUG)
        Serial.println("TAG");
      #endif
          
      nbtv.seek(36);
      char datStr[4] = { 'd', 'a', 't', 'a' };
      for (byte i = 0; i < 4; i++) {
          if (nbtv.read() != datStr[i]) {
              nbtv.seek(40);
              unsigned int siz = nbtv.read();
              siz = (nbtv.read() << 8 | siz) + 2;
              nbtv.seek(nbtv.position() + siz);
              for (byte i = 0; i < 4; i++) {
                  if (nbtv.read() != datStr[i]) {
                      return 0;
                  }
              }
          }
      }
  
      unsigned long dataBytes = nbtv.read();
      for (byte i = 8; i < 32; i += 8) {
          dataBytes = nbtv.read() << i | dataBytes;
      }
  
//      dataEnd = sFile.size() - sFile.position() - dataBytes + buffSize;

    #else // No Tag handling

      #if defined(DEBUG)
        Serial.println("NO TAG");
      #endif
      
      seek(44);
//      dataEnd = buffSize;

    #endif

    #if defined(DEBUG)
      Serial.println("wavInfo(...) complete!");
    #endif
    
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
  
  // Pre-read as much as possible for playback. Then off goes 'playbackPointer' and streamPointer will try to catch up
  streamPointer = playbackPointer = 0;
  nbtv.read( circularAudioVideoBuffer, CIRCULAR_BUFFER_SIZE );
     
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

#ifdef DEBUG
  volatile boolean criticalTimingError = false;
  int criticalTimingCounter = 0;
#endif


//--------------------------------------------------------------------------------------------------------------------
// TIMER3_OFV_vect
// This is an interrupt that runs at the frequency of the WAV file data (i.e, 22050Hz, 44100Hz, ...)
// It takes data from the circular buffer 'circularAudioVideoBuffer' and sends it to the LED (and soon, to the speaker).
// TODO:
// * Send audio to speaker
// * Add brightness control
// * Add contrast (how?)
// * Gamma correct
// * Handle sync pulses and clamping

ISR(TIMER3_OVF_vect) {

  int b2 = ( circularAudioVideoBuffer[playbackPointer] << 8 ) | circularAudioVideoBuffer[playbackPointer+1];

  // Trim off anything below zero - typically these are the sync pulses, but that's not guaranteed
  if (b2 < 0)
    b2 = 0;

  // Send brightness to LED array
  OCR4A = (byte)(b2>>5);              
  DDRC|=1<<7;    // Set Output Mode C7
  TCCR4A=0x82;  // Activate channel A
  
  // Move along to the next sample
  playbackPointer = ( playbackPointer + 4 ) % CIRCULAR_BUFFER_SIZE;
    
  #ifdef DEBUG
    // Detect if we've 'caught up' to the streaming pointer (whoops - that's bad next time around - bad data!)
    if ( playbackPointer == streamPointer )
      criticalTimingError = true;
  #endif
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
  
  #ifdef DEBUG

    // Catch (and clear) any timing/overflow error signalled by the interrupt
    if ( criticalTimingError ) {
      criticalTimingError = false;
      criticalTimingCounter++;
      streamMaximum = 0;                          // reset for a better look at what happens immediately after
      Serial.print("Critical timing error #");
      Serial.println(criticalTimingCounter);
    }
  #endif
  
  // Calculate how many bytes have been played by the interrupt - we need to stream this many from the SD
  int bytesToStream = playbackPointer - streamPointer;

  // It's a circular buffer - handle the wrap-around by filling to the end and allowing the next loop to
  // fill the remainder from the start of the buffer.
  if ( bytesToStream < 0 )                                       // wrap-around situation?
    bytesToStream = CIRCULAR_BUFFER_SIZE - streamPointer;        // fill only to end of buffer and let NEXT loop do the remainder

  if ( bytesToStream > 0 ) {

    #ifdef DEBUG

      // Diagnose the largest 'gap' which needs to be filled. This shows in effect how well the SD streaming
      // is going and gives the worst-case buffer size requirement. Of course we could run out of space and that
      // would be a bit of a shame. So we probably need to use the fastest SD card we can find.
      
      if ( bytesToStream > streamMaximum ) {
        streamMaximum = bytesToStream;
        Serial.print("MAX = ");
        Serial.println(streamMaximum);

        if ( streamMaximum >= CIRCULAR_BUFFER_SIZE )
          Serial.println("CRITICAL: buffer too small!" );
        }
    #endif

    // Grab the approrpiate amount of bytes from the SD WAV file
    nbtv.read( circularAudioVideoBuffer + streamPointer, bytesToStream );
    
    // The pointer now jumps along to the next free space.
    streamPointer = ( streamPointer + bytesToStream ) % CIRCULAR_BUFFER_SIZE;
  }
}

