#include <Adafruit_NeoPixel.h>
#define MAXLED 120 // MAX LEDs active on strip

// Pins Maker Faire Roma 19 version
#define PIN_LED 2 // LED strip connection (Digital)
#define PIN_P1 A0 // Player 1 button (connected to GND)
#define PIN_P2 A2 // Player 2 button (connected to GND)
#define PIN_AUDIO 3 // Pin for audio (speaker)

int NPIXELS = MAXLED;
float speed1 = 0;
float speed2 = 0;
float dist1 = 0;
float dist2 = 0;
byte loop1 = 0;
  byte loop2 = 0;
  byte leader = 0;
  float ACEL = 0.04;
  float kf = 0.015;
  byte flag_sw1 = 0;
byte flag_sw2 = 0;
byte draworder = 0;
unsigned long timestamp = 0;
Adafruit_NeoPixel track = Adafruit_NeoPixel(MAXLED, PIN_LED, NEO_GRB + NEO_KHZ800);
int tdelay = 10;
unsigned long lastDebug = 0;
int numLaps = 1;

void setup() {
  track.begin();
  pinMode(PIN_P1, INPUT_PULLUP);
  pinMode(PIN_P2, INPUT_PULLUP);
  Serial.begin(9600);
  Serial.println("Iniciando Open LED Race");
}

void start_race() {
  speed1 = 0;
  speed2 = 0;
  dist1  = 0;
  dist2  = 0;
  loop1  = 0;
  loop2  = 0;
  for (int i = NPIXELS - 1; i >= 0; i--) {
    track.setPixelColor(i, track.Color(255, 0, 0));
    track.show();
    delay(20);
  }
  delay(500);
  track.clear();
  track.show();
  delay(500);
  for (int i = NPIXELS - 1; i >= 0; i--) {
    track.setPixelColor(i, track.Color(255, 165, 0));
    track.show();
    delay(20);
  }
  delay(500);
  track.clear();
  track.show();
  delay(500);
  for (int i = NPIXELS - 1; i >= 0; i--) {
    track.setPixelColor(i, track.Color(0, 255, 0));
    track.show();
    delay(20);
  }
  delay(500);
  track.clear();
  track.show();
  timestamp = 0;
}

void fin_carrera(int ganador) {
  int color1 = (ganador == 0) ? 255 : 0;
  int color2 = (ganador == 1) ? 255 : 0;
  for (int i = 0; i < NPIXELS; i++) {
    track.setPixelColor(i, track.Color(color1, color2, 0));
    track.show();
  }
  delay(2000);
  int count = 0;
  while (count < 4) {
    if (digitalRead(PIN_P1) == 0 && digitalRead(PIN_P2) == 0) {
      delay(1000);
      count++;
      for (int i = 0; i < count; i++) {
        track.setPixelColor(i, track.Color(0, 0, 255));
      }
      track.show();
    } else {
      count = 0;
      track.clear();
      track.show();
    }
  }
  loop1 = 0;
  loop2 = 0;
  dist1 = 0;
  dist2 = 0;
  speed1 = 0;
  speed2 = 0;
  timestamp = 0;
}

int carrera() {
  unsigned long startPressTime = 0;
  bool bothPressed = false;
  while (true) {
    track.clear();
    unsigned long currentMillis = millis();
    if (currentMillis - lastDebug >= 500) {
      lastDebug = currentMillis;
      Serial.print("P1: ");
      Serial.print(digitalRead(PIN_P1));
      Serial.print("   P2: ");
      Serial.println(digitalRead(PIN_P2));
    }
    if ((flag_sw1 == 1) && (digitalRead(PIN_P1) == 0)) {
      flag_sw1 = 0;
      speed1 += ACEL;
    }
    if ((flag_sw1 == 0) && (digitalRead(PIN_P1) == 1)) {
      flag_sw1 = 1;
    }
    speed1 -= speed1 * kf;
    if ((flag_sw2 == 1) && (digitalRead(PIN_P2) == 0)) {
      flag_sw2 = 0;
      speed2 += ACEL;
    }
    if ((flag_sw2 == 0) && (digitalRead(PIN_P2) == 1)) {
      flag_sw2 = 1;
    }
    speed2 -= speed2 * kf;
    dist1 += speed1;
    dist2 += speed2;
    if (dist1 > NPIXELS) {
      loop1++;
      dist1 = 0;
    }
    if (dist2 > NPIXELS) {
      loop2++;
      dist2 = 0;
    }
    if (loop1 > (numLaps - 1)) {
      return 1;
    }
    if (loop2 > (numLaps - 1)) {
      return 0;
    }

    if (digitalRead(PIN_P1) == 0 && digitalRead(PIN_P2) == 0) {
      if (!bothPressed) {
        startPressTime = millis();
        bothPressed = true;
      } else if (millis() - startPressTime >= 4000) {
        // Turn track blue
        for (int i = 0; i < NPIXELS; i++) {
          track.setPixelColor(i, track.Color(0, 0, 255));
        }
        track.show();
        delay(5000); // Give time to decide if the race should end
        if (digitalRead(PIN_P1) == 0 && digitalRead(PIN_P2) == 0) {
          return 2; // End race with blue color
        }
      }
    } else {
      bothPressed = false;
    }

    track.setPixelColor(((int)dist1 % NPIXELS), track.Color(0, 255, 0));
    track.setPixelColor(((int)dist2 % NPIXELS), track.Color(255, 0, 0));
    track.show();
    delay(tdelay);
  }
}


void set_laps() {
  track.clear();
  numLaps = 3; // Reset to 1 lap
}

void loop() {
  int ganador;
  
  track.clear();
  set_laps(); // Set the number of laps

  start_race();
  ganador = carrera();
  if (ganador == 2) {
    for (int i = 0; i < NPIXELS; i++) {
      track.setPixelColor(i, track.Color(0, 0, 255));
    }
    track.show();
  } else {
    Serial.println(ganador);
    fin_carrera(ganador);
  }
  delay(4000); // Track off for 4 seconds
}
