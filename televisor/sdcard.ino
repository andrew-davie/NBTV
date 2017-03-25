
#define SD_CS_PIN 4

void setupSD2() {

  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output
  // or the SD library functions will not work.

  pinMode(SS, OUTPUT);

  if (!SDX.begin(SD_CS_PIN)) {
    Serial.println("SD failed!");
    return;
  }
}


