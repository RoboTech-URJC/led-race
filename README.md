<img width="1600" height="533" alt="led_race_header_def" src="https://github.com/user-attachments/assets/1fd5920d-692b-4823-8bb6-7aa130bcd875" />


# RoboTech led-race

Proyecto para una pista de carreras con tira LED NeoPixel, dos pulsadores de jugador, selector de modo, telemetria por puerto serie y publicacion MQTT. Incluye tambien un motor sencillo de torneo por eliminatorias.

## Componentes del proyecto

- `main/main.ino`: firmware Arduino para controlar la carrera, la tira LED, el modo luces y la telemetria serie.
- `bridge/bridge.js`: puente Serial -> MQTT. Lee los eventos JSON del Arduino y los publica en topics MQTT.
- `tournament-engine.js`: motor de torneo por bracket. Lee `players.txt`, genera los enfrentamientos y publica el estado por MQTT.
- `players.txt`: lista de participantes, un jugador por linea.

## Hardware esperado

El sketch usa estos pines por defecto:

| Funcion | Pin |
| --- | --- |
| Tira NeoPixel | `D2` |
| Pulsador jugador 1 | `A0` |
| Pulsador jugador 2 | `A2` |
| Audio | `D3` |
| Switch de modo | `D4` |

Los pulsadores y el switch usan `INPUT_PULLUP`, por lo que se activan conectando el pin a GND.

El switch de modo funciona asi:

- Cerrado: modo luces.
- Abierto: modo carrera.

## Requisitos

- Node.js
- npm
- Broker MQTT, por ejemplo Mosquitto en `mqtt://localhost:1883`
- Arduino IDE o Arduino CLI
- Libreria Arduino `Adafruit_NeoPixel`

## Instalacion

Instala las dependencias del motor de torneo:

```bash
npm install
```

Instala las dependencias del puente serie/MQTT:

```bash
cd bridge
npm install
```

## Cargar el firmware

Abre `main/main.ino` con Arduino IDE, instala `Adafruit_NeoPixel`, selecciona tu placa y puerto, y sube el sketch.

Valores importantes configurables en el sketch:

- `MAXLED`: numero de LEDs de la pista.
- `DEFAULT_LAPS`: vueltas por defecto.
- `TRACK_KM`: kilometros por vuelta para calcular telemetria.
- `ACEL`, `kf` y `tdelay`: comportamiento de aceleracion y frenado.
- `CRASH_SPEED_THRESHOLD`: velocidad desde la que puede haber accidente.

## Ejecutar el puente Serial -> MQTT

En Linux:

```bash
cd bridge
SERIAL_PORT=/dev/ttyACM0 npm run start:linux
```

En Windows:

```bash
cd bridge
npm run start:win
```

Tambien puedes ejecutar manualmente:

```bash
cd bridge
SERIAL_PORT=/dev/ttyUSB0 BAUD_RATE=115200 MQTT_BROKER=mqtt://localhost:1883 node bridge.js
```

Variables de entorno soportadas:

- `SERIAL_PORT`: puerto serie del Arduino. Por defecto `/dev/ttyUSB0`.
- `BAUD_RATE`: velocidad serie. Por defecto `115200`.
- `MQTT_BROKER`: URL del broker MQTT. Por defecto `mqtt://localhost:1883`.
- `MQTT_USER`: usuario MQTT opcional.
- `MQTT_PASS`: password MQTT opcional.

## Telemetria MQTT de carrera

El puente publica:

| Topic | Contenido |
| --- | --- |
| `openledrace/player/1/km` | Kilometros del jugador 1 |
| `openledrace/player/1/laps` | Vueltas del jugador 1 |
| `openledrace/player/1/kmh` | Velocidad del jugador 1 |
| `openledrace/player/2/km` | Kilometros del jugador 2 |
| `openledrace/player/2/laps` | Vueltas del jugador 2 |
| `openledrace/player/2/kmh` | Velocidad del jugador 2 |
| `openledrace/race/event` | Eventos JSON de carrera |
| `openledrace/race/status` | Estado retenido de la carrera |

Ejemplo para escuchar eventos:

```bash
mosquitto_sub -h localhost -t 'openledrace/#' -v
```

## Usar el modo carrera

1. Pon el switch abierto para entrar en modo carrera.
2. El Arduino muestra la salida con una secuencia rojo, naranja y verde.
3. Cada jugador pulsa su boton para acelerar.
4. Si un jugador supera el umbral de velocidad, puede sufrir un accidente y queda unos segundos en recuperacion.
5. Gana quien completa primero el numero de vueltas configurado.

Para abortar una carrera, manten ambos botones pulsados durante 4 segundos.

## Modo luces y seleccion de vueltas

Con el switch cerrado, la pista entra en modo luces.

Desde el modo luces:

1. Manten ambos botones pulsados durante 4 segundos.
2. Entra el selector de vueltas.
3. Pulsa P1 para subir vueltas.
4. Pulsa P2 para bajar vueltas.
5. Pulsa ambos botones durante 3 segundos para confirmar.

## Torneo

Edita `players.txt` con un participante por linea:

```txt
Jugador 1
Jugador 2
Jugador 3
Jugador 4
```

Arranca el motor de torneo:

```bash
MQTT_BROKER=mqtt://localhost:1883 node tournament-engine.js
```

El motor crea un bracket de potencia de 2, resuelve automaticamente los BYE y publica el estado por MQTT.

Topics usados por el torneo:

| Topic | Uso |
| --- | --- |
| `openledrace/tournament/state` | Estado completo del torneo, retenido |
| `openledrace/tournament/command` | Comandos JSON de entrada |
| `openledrace/tournament/event` | Eventos del torneo |
| `openledrace/tournament/mode` | Modo actual, retenido |

## Comandos MQTT de torneo

Marcar ganador:

```bash
mosquitto_pub -h localhost -t openledrace/tournament/command -m '{"action":"set_winner","matchId":"r1-m1","winner":"Jugador 1"}'
```

Cambiar modo:

```bash
mosquitto_pub -h localhost -t openledrace/tournament/command -m '{"action":"set_mode","mode":"race"}'
```

Pedir estado:

```bash
mosquitto_pub -h localhost -t openledrace/tournament/command -m '{"action":"get_state"}'
```

Reiniciar torneo o recargar jugadores:

```bash
mosquitto_pub -h localhost -t openledrace/tournament/command -m '{"action":"reset_tournament"}'
mosquitto_pub -h localhost -t openledrace/tournament/command -m '{"action":"reload_players"}'
```

## Flujo recomendado

1. Arranca el broker MQTT.
2. Carga `main/main.ino` en el Arduino.
3. Ejecuta `bridge/bridge.js` con el puerto serie correcto.
4. Edita `players.txt`.
5. Ejecuta `tournament-engine.js`.
6. Escucha `openledrace/#` para integrar pantalla, dashboard o automatizaciones.

## Problemas comunes

- Si el puente no abre el puerto serie, revisa `SERIAL_PORT` y permisos del dispositivo.
- En Linux puede hacer falta pertenecer al grupo `dialout` para leer `/dev/ttyUSB*` o `/dev/ttyACM*`.
- Si no llegan mensajes MQTT, confirma que el broker esta arrancado y que `MQTT_BROKER` coincide en el puente y el torneo.
- Si el torneo no arranca, revisa que `players.txt` exista y tenga al menos 2 jugadores.
- Si la tira LED no coincide con la longitud real, ajusta `MAXLED` en `main/main.ino`.

  ## Team
---
- Adrían Manzanares~ [Amanza17](https://github.com/amanza17)
- Adrían Madinabeitia ~ [madport](https://github.com/madport)
- Iván Porras ~ [Ivan-Porras](https://github.com/Ivan-Porras)
- Óscar Martínez ~ [OscarMrZ](https://github.com/OscarMrZ)

