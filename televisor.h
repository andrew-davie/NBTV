#ifndef televisor_h
#define televisor_h

/*
 * Arduino Mechanical TV by Andrew Davie
 * For a complete build history see https://www.taswegian.com/NBTV/forum/viewforum.php?f=28
 */

#define DEBUG

    // >>> Don't forget to open the serial terminal if DEBUG outputs are active! <<<

#define NEXTION     
#define SDX
#define CIRCULAR_BUFFER_SIZE 512
// buffer size must be a multiple of sample size (i.e, (video + audio) * bytes per)) = 4 for 16-bit, and 2 for 8-bit

#define SHOW_WAV_STATS

#endif



