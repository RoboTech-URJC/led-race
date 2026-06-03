/**
 * serial-mqtt-bridge.js
 * Lee JSON por Serial desde el Arduino y publica en MQTT.
 *
 * Instalar dependencias:
 *   npm install serialport mqtt
 *
 * Uso:
 *   node serial-mqtt-bridge.js
 *   SERIAL_PORT=/dev/ttyUSB0 node serial-mqtt-bridge.js   (Linux)
 *   SERIAL_PORT=COM3 node serial-mqtt-bridge.js            (Windows)
 */

const { SerialPort }    = require("serialport");
const { ReadlineParser } = require("@serialport/parser-readline");
const mqtt               = require("mqtt");

// ── Config ───────────────────────────────────────────────────────────────────

const SERIAL_PORT = process.env.SERIAL_PORT || "/dev/ttyUSB0";
const BAUD_RATE   = parseInt(process.env.BAUD_RATE || "115200");
const MQTT_BROKER = process.env.MQTT_BROKER || "mqtt://localhost:1883";
const MQTT_USER   = process.env.MQTT_USER   || undefined;
const MQTT_PASS   = process.env.MQTT_PASS   || undefined;

// Topics base
const T = {
  // Telemetría en tiempo real
  p1_km:    "openledrace/player/1/km",
  p1_laps:  "openledrace/player/1/laps",
  p1_kmh:   "openledrace/player/1/kmh",
  p2_km:    "openledrace/player/2/km",
  p2_laps:  "openledrace/player/2/laps",
  p2_kmh:   "openledrace/player/2/kmh",
  // Eventos
  event:    "openledrace/race/event",
  status:   "openledrace/race/status",
};

// ── MQTT ─────────────────────────────────────────────────────────────────────

const mqttClient = mqtt.connect(MQTT_BROKER, {
  username: MQTT_USER,
  password: MQTT_PASS,
  will: {
    topic:   T.status,
    payload: JSON.stringify({ online: false }),
    retain:  true,
  }
});

mqttClient.on("connect", () => {
  console.log("🔌 MQTT conectado:", MQTT_BROKER);
  mqttClient.publish(T.status, JSON.stringify({ online: true }), { retain: true });
});
mqttClient.on("error", err => console.error("MQTT error:", err.message));

function pub(topic, value, opts = {}) {
  mqttClient.publish(topic, String(value), opts);
}
function pubJson(topic, obj, opts = {}) {
  mqttClient.publish(topic, JSON.stringify(obj), opts);
}

// ── Serial ───────────────────────────────────────────────────────────────────

const port = new SerialPort({ path: SERIAL_PORT, baudRate: BAUD_RATE });
const parser = port.pipe(new ReadlineParser({ delimiter: "\n" }));

port.on("open", () => {
  console.log(`📡 Serial abierto: ${SERIAL_PORT} @ ${BAUD_RATE}`);
});
port.on("error", err => {
  console.error("❌ Serial error:", err.message);
  console.error("   → Comprueba SERIAL_PORT. Puertos disponibles:");
  SerialPort.list().then(ports =>
    ports.forEach(p => console.error("     ", p.path, p.manufacturer || ""))
  );
});

// ── Procesar mensajes del Arduino ─────────────────────────────────────────────

parser.on("data", (line) => {
  line = line.trim();
  if (!line.startsWith("{")) return;   // ignorar líneas no-JSON

  let msg;
  try { msg = JSON.parse(line); }
  catch (e) { console.warn("JSON inválido:", line); return; }

  const evt = msg.evt;

  // ── Telemetría periódica ──────────────────────────────────────────────────
  if (evt === "telemetry") {
    const { p1, p2, racing } = msg;

    pub(T.p1_km,   p1.km.toFixed(3));
    pub(T.p1_laps, p1.laps);
    pub(T.p1_kmh,  p1.kmh.toFixed(1));

    pub(T.p2_km,   p2.km.toFixed(3));
    pub(T.p2_laps, p2.laps);
    pub(T.p2_kmh,  p2.kmh.toFixed(1));

    // Log compacto para no spammear la consola
    if (racing) {
      process.stdout.write(
        `\r🏎  P1: ${p1.laps}v ${p1.kmh.toFixed(1)}km/h ${p1.km.toFixed(2)}km` +
        `  |  P2: ${p2.laps}v ${p2.kmh.toFixed(1)}km/h ${p2.km.toFixed(2)}km   `
      );
    }
    return;
  }

  // ── Vuelta completada ──────────────────────────────────────────────────────
  if (evt === "lap") {
    const player = msg.player;
    console.log(`\n🔁 P${player} completa vuelta ${msg.lap} | km: ${msg.km}`);
    if (player === 1) pub(T.p1_laps, msg.lap);
    if (player === 2) pub(T.p2_laps, msg.lap);
    pubJson(T.event, { type: "lap", player, lap: msg.lap, km: msg.km });
    return;
  }

  // ── Inicio de carrera ──────────────────────────────────────────────────────
  if (evt === "race_start") {
    console.log(`\n🏁 Carrera iniciada — ${msg.numLaps} vuelta(s)`);
    // Reset km y laps en MQTT
    ["p1_km","p1_laps","p1_kmh","p2_km","p2_laps","p2_kmh"].forEach(k => pub(T[k], "0"));
    pubJson(T.event, { type: "race_start", numLaps: msg.numLaps });
    pubJson(T.status, { online: true, racing: true, numLaps: msg.numLaps }, { retain: true });
    return;
  }

  // ── Fin de carrera ─────────────────────────────────────────────────────────
  if (evt === "race_end") {
    console.log(`\n🏆 Carrera terminada — Ganador: P${msg.winner}`);
    pubJson(T.event, { type: "race_end", winner: msg.winner });
    pubJson(T.status, { online: true, racing: false, lastWinner: msg.winner }, { retain: true });
    return;
  }

  // ── Carrera abortada ───────────────────────────────────────────────────────
  if (evt === "race_abort") {
    console.log(`\n⛔ Carrera abortada`);
    pubJson(T.event, { type: "race_abort" });
    pubJson(T.status, { online: true, racing: false }, { retain: true });
    return;
  }

  // ── Selección de vueltas ───────────────────────────────────────────────────
  if (evt === "set_laps" || evt === "laps_confirmed") {
    console.log(`\n⚙️  Vueltas: ${msg.laps}${evt === "laps_confirmed" ? " ✅ confirmadas" : ""}`);
    pubJson(T.event, { type: evt, laps: msg.laps });
    return;
  }

  // ── Boot ───────────────────────────────────────────────────────────────────
  if (evt === "boot") {
    console.log(`\n✅ Arduino listo: ${msg.msg || ""}`);
    pubJson(T.event, { type: "boot" });
    return;
  }

  // Cualquier otro evento lo publicamos tal cual
  console.log("\n📨 Evento:", JSON.stringify(msg));
  pubJson(T.event, msg);
});
