#include <Adafruit_NeoPixel.h>

// ── Pines ─────────────────────────────────────────────────────────────────────
#define PIN_LED   2
#define PIN_P1    A0
#define PIN_P2    A2
#define PIN_AUDIO 3
#define PIN_SW    4    // Switch modo: CERRADO = luces, ABIERTO = carrera

// ── Configuración ─────────────────────────────────────────────────────────────
#define MAXLED        180
#define DEFAULT_LAPS  3
#define TRACK_KM      0.05f   // km por vuelta — ajusta a tu pista

#define SPEED_MAX              2.67f  // ACEL/kf — actualiza si cambias esos valores
#define CRASH_SPEED_THRESHOLD  70.0f  // km/h a partir del cual hay riesgo
#define CRASH_PROB_MAX         0.005f // prob. máxima por tick (a 100 km/h ~2s, a 80 km/h ~6s)
#define RECOVERY_DURATION      2000UL // ms de parpadeo tras crash
#define TELEMETRY_INTERVAL     100UL  // ms entre envíos de telemetría

// ── NeoPixel ──────────────────────────────────────────────────────────────────
Adafruit_NeoPixel track = Adafruit_NeoPixel(MAXLED, PIN_LED, NEO_GRB + NEO_KHZ800);

// ── Variables de carrera ──────────────────────────────────────────────────────
int   NPIXELS = MAXLED;
float ACEL    = 0.04f;
float kf      = 0.015f;
int   tdelay  = 10;
int   numLaps = DEFAULT_LAPS;

float speed1 = 0, speed2 = 0;
float dist1  = 0, dist2  = 0;
byte  loop1  = 0, loop2  = 0;
byte  flag_sw1 = 0, flag_sw2 = 0;
float km1 = 0, km2 = 0;
unsigned long lastTelemetry = 0;

// ── Struct PlayerCrash (declarado ANTES de cualquier función que lo use) ──────
struct PlayerCrash {
  bool          recovering;
  unsigned long recoveryStart;
  bool          blinkState;
  unsigned long lastBlink;
};

PlayerCrash crash1 = { false, 0, false, 0 };
PlayerCrash crash2 = { false, 0, false, 0 };

// ── Colores ───────────────────────────────────────────────────────────────────
// P1 = ROJO, P2 = VERDE
#define COLOR_P1    track.Color(255,   0,   0)
#define COLOR_P2    track.Color(  0, 255,   0)
#define COLOR_BLINK track.Color(255, 255, 255)

// ─────────────────────────────────────────────────────────────────────────────
// UTILIDADES
// ─────────────────────────────────────────────────────────────────────────────

float toKmh(float spd) {
  float k = (spd / SPEED_MAX) * 100.0f;
  return constrain(k, 0.0f, 100.0f) * 8.2;
}

bool switchClosed() {
  // INPUT_PULLUP: LOW = cerrado, HIGH = abierto
  return digitalRead(PIN_SW) == LOW;
}

// ─────────────────────────────────────────────────────────────────────────────
// SERIAL / TELEMETRÍA
// ─────────────────────────────────────────────────────────────────────────────

void serialEvt(const char* type, const String& data) {
  Serial.print(F("{\"evt\":\""));
  Serial.print(type);
  Serial.print(F("\","));
  Serial.print(data);
  Serial.println(F("}"));
}

void sendTelemetry(bool racing) {
  unsigned long now = millis();
  if (now - lastTelemetry < TELEMETRY_INTERVAL) return;
  lastTelemetry = now;

  float kmh1 = toKmh(speed1);
  float kmh2 = toKmh(speed2);

  Serial.print(F("{\"evt\":\"telemetry\",\"racing\":"));
  Serial.print(racing ? F("true") : F("false"));
  Serial.print(F(",\"p1\":{\"laps\":")); Serial.print(loop1);
  Serial.print(F(",\"km\":")  );        Serial.print(km1, 3);
  Serial.print(F(",\"kmh\":") );        Serial.print(kmh1, 1);
  Serial.print(F(",\"dist\":")); Serial.print(dist1, 1);
  Serial.print(F(",\"crash\":"));       Serial.print(crash1.recovering ? F("true") : F("false"));
  Serial.print(F("},\"p2\":{\"laps\":")); Serial.print(loop2);
  Serial.print(F(",\"km\":")  );        Serial.print(km2, 3);
  Serial.print(F(",\"kmh\":") );        Serial.print(kmh2, 1);
  Serial.print(F(",\"dist\":")); Serial.print(dist2, 1);
  Serial.print(F(",\"crash\":"));       Serial.print(crash2.recovering ? F("true") : F("false"));
  Serial.println(F("}}"));
}

// ─────────────────────────────────────────────────────────────────────────────
// CRASH / RECOVERY
// ─────────────────────────────────────────────────────────────────────────────

bool checkCrash(float spd) {
  float kmh = toKmh(spd);
  if (kmh <= CRASH_SPEED_THRESHOLD) return false;
  float excess = (kmh - CRASH_SPEED_THRESHOLD) / (100.0f - CRASH_SPEED_THRESHOLD);
  float prob   = excess * CRASH_PROB_MAX;
  return ((float)random(0, 10000) / 10000.0f) < prob;
}

void triggerCrash(float &spd, PlayerCrash &pc) {
  spd               = 0;
  pc.recovering     = true;
  pc.recoveryStart  = millis();
  pc.blinkState     = true;
  pc.lastBlink      = millis();
}

// Devuelve true mientras sigue en recovery
bool updateRecovery(PlayerCrash &pc, unsigned long now) {
  if (!pc.recovering) return false;
  if (now - pc.recoveryStart >= RECOVERY_DURATION) {
    pc.recovering = false;
    return false;
  }
  if (now - pc.lastBlink >= 120UL) {
    pc.blinkState = !pc.blinkState;
    pc.lastBlink  = now;
  }
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ANIMACIONES
// ─────────────────────────────────────────────────────────────────────────────

void colorSweep(uint8_t r, uint8_t g, uint8_t b, int delayMs) {
  for (int i = NPIXELS - 1; i >= 0; i--) {
    track.setPixelColor(i, track.Color(r, g, b));
    track.show();
    delay(delayMs);
  }
}

void start_race() {
  speed1 = dist1 = km1 = 0;
  speed2 = dist2 = km2 = 0;
  loop1  = loop2 = 0;
  flag_sw1 = flag_sw2 = 0;
  lastTelemetry = 0;
  crash1 = { false, 0, false, 0 };
  crash2 = { false, 0, false, 0 };

  serialEvt("race_start", "\"numLaps\":" + String(numLaps));

  colorSweep(255,   0,   0, 20); delay(500);
  track.clear(); track.show();   delay(500);
  colorSweep(255, 165,   0, 20); delay(500);
  track.clear(); track.show();   delay(500);
  colorSweep(  0, 255,   0, 20); delay(500);
  track.clear(); track.show();
}

// ─────────────────────────────────────────────────────────────────────────────
// SELECTOR DE VUELTAS
// Retorna true si el usuario confirmó; false si se salió sin confirmar (timeout)
// ─────────────────────────────────────────────────────────────────────────────

void set_laps() {
  numLaps = DEFAULT_LAPS;
  serialEvt("set_laps", "\"laps\":" + String(numLaps));

  while (true) {
    track.clear();
    for (int i = 0; i < numLaps && i < NPIXELS; i++)
      track.setPixelColor(i, track.Color(0, 255, 0));
    track.show();

    bool p1 = (digitalRead(PIN_P1) == LOW);
    bool p2 = (digitalRead(PIN_P2) == LOW);

    if (p1 && p2) {
      delay(3000);
      serialEvt("laps_confirmed", "\"laps\":" + String(numLaps));
      return;
    }
    if (p1) {
      numLaps++;
      serialEvt("set_laps", "\"laps\":" + String(numLaps));
      delay(300);
    }
    if (p2 && numLaps > 1) {
      numLaps--;
      serialEvt("set_laps", "\"laps\":" + String(numLaps));
      delay(300);
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// CARRERA
// Retorna: 1=gana P1, 0=gana P2, 2=abortada
// ─────────────────────────────────────────────────────────────────────────────

int carrera() {
  unsigned long startPressTime = 0;
  bool bothPressed = false;

  while (true) {
    unsigned long now = millis();
    track.clear();

    // ── P1: física (solo si no está en recovery) ──────────────────────────────
    if (!crash1.recovering) {
      if (flag_sw1 == 1 && digitalRead(PIN_P1) == LOW) { flag_sw1 = 0; speed1 += ACEL; }
      if (flag_sw1 == 0 && digitalRead(PIN_P1) == HIGH)  flag_sw1 = 1;
      speed1 -= speed1 * kf;
      dist1  += speed1;

      if (dist1 >= NPIXELS) {
        loop1++;
        dist1 -= NPIXELS;
        km1 += TRACK_KM;
        Serial.print(F("{\"evt\":\"lap\",\"player\":1,\"lap\":"));
        Serial.print(loop1);
        Serial.print(F(",\"km\":")); Serial.print(km1, 3);
        Serial.println(F("}"));
      }
    }

    // ── P2: física ────────────────────────────────────────────────────────────
    if (!crash2.recovering) {
      if (flag_sw2 == 1 && digitalRead(PIN_P2) == LOW) { flag_sw2 = 0; speed2 += ACEL; }
      if (flag_sw2 == 0 && digitalRead(PIN_P2) == HIGH)  flag_sw2 = 1;
      speed2 -= speed2 * kf;
      dist2  += speed2;

      if (dist2 >= NPIXELS) {
        loop2++;
        dist2 -= NPIXELS;
        km2 += TRACK_KM;
        Serial.print(F("{\"evt\":\"lap\",\"player\":2,\"lap\":"));
        Serial.print(loop2);
        Serial.print(F(",\"km\":")); Serial.print(km2, 3);
        Serial.println(F("}"));
      }
    }

    // ── Condición de victoria ─────────────────────────────────────────────────
    if (loop1 >= numLaps) { serialEvt("race_end", "\"winner\":1"); return 1; }
    if (loop2 >= numLaps) { serialEvt("race_end", "\"winner\":2"); return 0; }

    // ── Check crash ───────────────────────────────────────────────────────────
    if (!crash1.recovering && checkCrash(speed1)) {
      triggerCrash(speed1, crash1);
      serialEvt("crash", "\"player\":1");
    }
    if (!crash2.recovering && checkCrash(speed2)) {
      triggerCrash(speed2, crash2);
      serialEvt("crash", "\"player\":2");
    }

    // ── Recovery (no bloqueante) ──────────────────────────────────────────────
    bool r1 = updateRecovery(crash1, now);
    bool r2 = updateRecovery(crash2, now);

    // ── Dibujar LEDs ──────────────────────────────────────────────────────────
    int px1 = (int)dist1 % NPIXELS;
    int px2 = (int)dist2 % NPIXELS;

    if (r1) { if (crash1.blinkState) track.setPixelColor(px1, COLOR_BLINK); }
    else    { track.setPixelColor(px1, COLOR_P1); }

    if (r2) { if (crash2.blinkState) track.setPixelColor(px2, COLOR_BLINK); }
    else    { track.setPixelColor(px2, COLOR_P2); }

    track.show();

    // ── Telemetría ────────────────────────────────────────────────────────────
    sendTelemetry(true);

    // ── Abortar: ambos botones 4 s ────────────────────────────────────────────
    if (digitalRead(PIN_P1) == LOW && digitalRead(PIN_P2) == LOW) {
      if (!bothPressed) { startPressTime = now; bothPressed = true; }
      else if (now - startPressTime >= 4000UL) {
        for (int i = 0; i < NPIXELS; i++)
          track.setPixelColor(i, track.Color(0, 0, 255));
        track.show();
        delay(5000);
        if (digitalRead(PIN_P1) == LOW && digitalRead(PIN_P2) == LOW) {
          serialEvt("race_abort", "\"reason\":\"both_held\"");
          return 2;
        }
      }
    } else {
      bothPressed = false;
    }

    delay(tdelay);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// FIN DE CARRERA
// ─────────────────────────────────────────────────────────────────────────────

void fin_carrera(int ganador) {
  uint8_t r = (ganador == 1) ? 255 : 0;
  uint8_t g = (ganador == 0) ? 255 : 0;
  for (int i = 0; i < NPIXELS; i++)
    track.setPixelColor(i, track.Color(r, g, 0));
  track.show();
  delay(2000);

  // Resetear y enviar telemetría final a cero
  loop1 = loop2 = 0;
  dist1 = dist2 = speed1 = speed2 = km1 = km2 = 0;
  sendTelemetry(false);

  // Esperar que ambos pulsen 4 veces seguidas para continuar
  delay(2);
}

// ─────────────────────────────────────────────────────────────────────────────
// MODO LUCES (switch cerrado)
// Juego de luces en bucle. Si ambos botones se mantienen 4 s → selector de vueltas.
// ─────────────────────────────────────────────────────────────────────────────

// Variables de estado para el juego de luces (no bloqueante)
struct LightShow {
  uint8_t       mode;          // 0=arcoíris, 1=persecución, 2=chispa
  int           step;
  unsigned long lastStep;
  unsigned long stepDelay;
  // Para persecución
  int           head;
  int           tail;
  // Para chispa aleatoria
  int           sparks[8];
};

LightShow ls = { 0, 0, 0, 30, 0, 0, {-1,-1,-1,-1,-1,-1,-1,-1} };

// Calcula color arcoíris para una posición
uint32_t wheel(byte pos) {
  pos = 255 - pos;
  if (pos < 85)  return track.Color(255 - pos * 3, 0, pos * 3);
  if (pos < 170) { pos -= 85;  return track.Color(0, pos * 3, 255 - pos * 3); }
  pos -= 170;
  return track.Color(pos * 3, 255 - pos * 3, 0);
}

void updateLightShow(unsigned long now) {
  if (now - ls.lastStep < ls.stepDelay) return;
  ls.lastStep = now;

  track.clear();

  switch (ls.mode) {

    case 0: // ── Arcoíris deslizante ──────────────────────────────────────────
      for (int i = 0; i < NPIXELS; i++)
        track.setPixelColor(i, wheel((byte)((i + ls.step) & 255)));
      ls.step = (ls.step + 1) % 256;
      ls.stepDelay = 20;
      // Cambiar modo cada 256 pasos
      if (ls.step == 0) { ls.mode = 1; ls.head = 0; ls.stepDelay = 30; }
      break;

    case 1: // ── Persecución rojo/verde ────────────────────────────────────────
    {
      // Cola que se desvanece
      for (int t = 0; t < 12; t++) {
        int pos = (ls.head - t + NPIXELS) % NPIXELS;
        uint8_t bright = 255 - t * 20;
        if (bright > 0)
          track.setPixelColor(pos, track.Color(bright, 0, bright / 3));
      }
      ls.head = (ls.head + 1) % NPIXELS;
      ls.stepDelay = 25;
      ls.step++;
      if (ls.step >= NPIXELS * 3) { ls.mode = 2; ls.step = 0; ls.stepDelay = 40; }
      break;
    }

    case 2: // ── Chispas aleatorias ────────────────────────────────────────────
    {
      // Actualizar chispas existentes (se apagan)
      for (int s = 0; s < 8; s++) {
        if (ls.sparks[s] >= 0) {
          track.setPixelColor(ls.sparks[s], track.Color(
            random(100, 255), random(50, 200), 0));
          if (random(3) == 0) ls.sparks[s] = -1; // apagar aleatoriamente
        }
      }
      // Añadir nueva chispa
      for (int s = 0; s < 8; s++) {
        if (ls.sparks[s] < 0) {
          ls.sparks[s] = random(NPIXELS);
          break;
        }
      }
      ls.step++;
      ls.stepDelay = 40;
      if (ls.step >= 80) {
        ls.mode = 0; ls.step = 0; ls.stepDelay = 20;
        for (int s = 0; s < 8; s++) ls.sparks[s] = -1;
      }
      break;
    }
  }

  track.show();
}

void runLightShow() {
  serialEvt("mode", "\"value\":\"lights\"");
  track.clear(); track.show();
  ls = { 0, 0, 0, 20, 0, 0, {-1,-1,-1,-1,-1,-1,-1,-1} };

  unsigned long pressStart = 0;
  bool bothHeld = false;

  while (switchClosed()) {
    unsigned long now = millis();

    // ── Actualizar luces ──────────────────────────────────────────────────────
    updateLightShow(now);

    // ── Detectar ambos botones 4 s → selector de vueltas ─────────────────────
    if (digitalRead(PIN_P1) == LOW && digitalRead(PIN_P2) == LOW) {
      if (!bothHeld) { pressStart = now; bothHeld = true; }
      else if (now - pressStart >= 4000UL) {
        // Confirmar 4 s con tira azul
        for (int i = 0; i < NPIXELS; i++)
          track.setPixelColor(i, track.Color(0, 0, 255));
        track.show();
        delay(1000);
        // Entrar al selector
        set_laps();
        // Volver al juego de luces
        ls = { 0, 0, millis(), 20, 0, 0, {-1,-1,-1,-1,-1,-1,-1,-1} };
        bothHeld = false;
      }
    } else {
      bothHeld = false;
    }
  }

  // Switch abierto → salir del modo luces
  track.clear(); track.show();
}

// ─────────────────────────────────────────────────────────────────────────────
// SETUP & LOOP
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  track.begin();
  track.clear();
  track.show();

  pinMode(PIN_P1, INPUT_PULLUP);
  pinMode(PIN_P2, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);

  randomSeed(analogRead(A1));

  Serial.begin(115200);
  Serial.println(F("{\"evt\":\"boot\",\"msg\":\"Open LED Race ready\"}"));
}

void loop() {
  // ── Modo luces (switch cerrado) ───────────────────────────────────────────
  if (switchClosed()) {
    runLightShow();
    return; // volver al inicio del loop para re-evaluar el switch
  }

  // ── Modo carrera (switch abierto) ────────────────────────────────────────
  serialEvt("mode", "\"value\":\"race\"");
  track.clear(); track.show();

  // numLaps ya fue fijado en set_laps() (o es DEFAULT_LAPS si no se tocó)
  start_race();
  int ganador = carrera();

  if (ganador == 2) {
    // Abortada
    for (int i = 0; i < NPIXELS; i++)
      track.setPixelColor(i, track.Color(0, 0, 255));
    track.show();
    delay(4000);
  } else {
    fin_carrera(ganador);
    delay(4000);
  }

  // Reset telemetría
  speed1 = speed2 = dist1 = dist2 = km1 = km2 = 0;
  loop1  = loop2 = 0;
  sendTelemetry(false);
}