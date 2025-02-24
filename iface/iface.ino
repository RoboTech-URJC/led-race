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
bool interrupted = false;

void setup() {
  track.begin();
  pinMode(PIN_P1, INPUT_PULLUP);
  pinMode(PIN_P2, INPUT_PULLUP);
  Serial.begin(9600);
  Serial.println("Iniciando Open LED Race");
  
}

void start_race() {
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
    
    // Check for serial input
    if (Serial.available() > 0) {
        parseSerialInput();
    }
  }
}

void simulateRace() {
  float tempSpeed1 = 0;
  float tempSpeed2 = 0;
  float tempDist1 = 0;
  float tempDist2 = 0;

  for (int i = 0; i < NPIXELS; i++) {
    tempSpeed1 += ACEL ;
    tempSpeed2 += ACEL  * 0.9; // Velocidad ligeramente menor para el jugador 2

    tempDist1 += tempSpeed1;
    tempDist2 += tempSpeed2;

    track.clear();
    track.setPixelColor(((int)tempDist1 % NPIXELS), track.Color(0, 255, 0)); // Jugador 1 en verde
    track.setPixelColor(((int)tempDist2 % NPIXELS), track.Color(255, 0, 0)); // Jugador 2 en rojo
    track.show();
    delay(45);

    // Reducción de velocidad para simular fricción
    tempSpeed1 -= tempSpeed1 * kf;
    tempSpeed2 -= tempSpeed2 * kf;
  }

  // Mostrar ganador
  int ganador = (tempDist1 > tempDist2) ? 2 : 1;
  int color1 = (ganador == 1) ? 255 : 0;
  int color2 = (ganador == 2) ? 255 : 0;
  for (int i = 0; i < NPIXELS; i++) {
    track.setPixelColor(i, track.Color(color1, color2, 0));
    track.show();
  }
  delay(2000); // Mostrar ganador durante 2 segundos
}

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return track.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return track.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return track.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void coolLightShow() {
  while (!checkInterrupt()) {
    rainbowWave();
     if (checkInterrupt()) break;
    explosionEffect();
     if (checkInterrupt()) break;
    meteorEffect();
     if (checkInterrupt()) break;
    sparkleEffect();
     if (checkInterrupt()) break;
    energyPulse();
     if (checkInterrupt()) break;
  }
  return;
}

// 🌈 1. Ciclo Arcoíris en Olas 🌊
void rainbowWave() {
  for (int j = 0; j < 256; j++) {
    for (int i = 0; i < NPIXELS; i++) {
      track.setPixelColor(i, Wheel((i + j) & 255));
    }
    track.show();
    delay(20);
    if (checkInterrupt()) return;
  }
}

// 💥 2. Explosión de Color 🔥
void explosionEffect() {
  for (int radius = 0; radius <= NPIXELS / 2; radius++) {
    track.clear();
    for (int i = (NPIXELS / 2) - radius; i <= (NPIXELS / 2) + radius; i++) {
      if (i >= 0 && i < NPIXELS) {
        track.setPixelColor(i, track.Color(255, 50 + radius * 5, 0)); 
      }
    }
    track.show();
    delay(30);
    if (checkInterrupt()) return;
  }
}

// ☄️ 3. Efecto Meteorito 🌠
void meteorEffect() {
  int meteorSize = 10;
  for (int i = 0; i < NPIXELS + meteorSize; i++) {
    track.clear();
    for (int j = 0; j < meteorSize; j++) {
      if ((i - j) >= 0 && (i - j) < NPIXELS) {
        int brightness = (255 / meteorSize) * (meteorSize - j);
        track.setPixelColor(i - j, track.Color(brightness, brightness / 2, 255));
      }
    }
    track.show();
    delay(30);
    if (checkInterrupt()) return;
  }
}

// ✨ 4. Destellos Aleatorios Inteligentes ⚡
void sparkleEffect() {
  track.clear();
  for (int i = 0; i < 100; i++) {
    int pixel = random(NPIXELS);
    track.setPixelColor(pixel, track.Color(random(100, 255), random(100, 255), random(100, 255)));
    track.show();
    delay(30);
    track.setPixelColor(pixel, track.Color(0, 0, 0));
    track.show();
    if (checkInterrupt()) return;
  }
}

// 🔥 5. Pulso de Energía 🚀
void energyPulse() {
  for (int brightness = 0; brightness < 255; brightness += 5) {
    for (int i = 0; i < NPIXELS; i++) {
      track.setPixelColor(i, track.Color(brightness, 0, 255 - brightness));
    }
    track.show();
    delay(15);
    if (checkInterrupt()) return;
  }
  for (int brightness = 255; brightness >= 0; brightness -= 5) {
    for (int i = 0; i < NPIXELS; i++) {
      track.setPixelColor(i, track.Color(brightness, 0, 255 - brightness));
    }
    track.show();
    delay(15);
    if (checkInterrupt()) return;
  }
}

// 🚨 Interrupción por Serial
bool checkInterrupt() {
  if (interrupted) return true;  // Si ya se interrumpió, siempre devuelve true
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    if (!input.startsWith("START_Programa 3")) {
      interrupted = true; // Activamos la bandera de interrupción
      return true;
    }
  }
  return false;
}




void parseSerialInput() {
  int ganador;

  String input = Serial.readStringUntil('\n');

  if (input.startsWith("START_")) {
    // Manejar el inicio de diferentes programas
    String program = input.substring(6);
    if (program == "Programa 1") {
      ganador = 0;
      track.clear();
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
    } else if (program == "Programa 2") {
     simulateRace();
    } else if (program == "Programa 3") {
        interrupted = false;  // Reiniciamos la bandera antes de iniciar el show

      coolLightShow();
    }
  } else if (input.startsWith("ROUNDS_")) {
    numLaps = input.substring(7).toInt();
  } else if (input.startsWith("COLOR_")) {
    // Enviar color a la tira LED
    String color = input.substring(6);
    if (color == "Rojo") {
      setStripColor(255, 0, 0);
    } else if (color == "Verde") {
      setStripColor(0, 255, 0);
    } else if (color == "Azul") {
      setStripColor(0, 0, 255);
    }else if (color == "Off") {
      setStripColor(0, 0, 0);
    }
    
  }
  return;
}

void setStripColor(int r, int g, int b) {
  for (int i = 0; i < NPIXELS; i++) {
    track.setPixelColor(i, track.Color(r, g, b));
  }
  track.show();
}



void loop() {

  // Espera entrada serial para demo race o ajustes
  while (true) {
    if (Serial.available() > 0) {
      parseSerialInput();
    }
    //demo_race(); // Iniciar la demo race si no hay entrada serial
  }
}
