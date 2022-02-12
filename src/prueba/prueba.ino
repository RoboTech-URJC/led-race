#include <Adafruit_NeoPixel.h>
#define MAXLED         50 // MAX LEDs actives on strip

#define PIN_LED        3

int NPIXELS = MAXLED; // leds on track

Adafruit_NeoPixel track = Adafruit_NeoPixel(MAXLED, PIN_LED, NEO_BRG + NEO_KHZ800);

void setup() {
  track.begin();
}

void ledon(int led) {
  for (int i = 1; i < led; i++) {
    track.setPixelColor(i, track.Color(0, 0, 0));
    delay(20);
    track.show();
  }
  track.setPixelColor(led, track.Color(255, 0, 0));
  delay(20);
  track.show();

}

void loop() {
  // all_leds();
  track.clear();
  track.show();
  track.setPixelColor(14, track.Color(0, 200, 0));
  track.show();
  delay(1000000);
  

}
