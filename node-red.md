# Interfaz Node-RED

Este proyecto usa Node-RED Dashboard para mostrar la carrera en una pantalla grande y controlar el torneo desde el movil. Los dos flows trabajan contra el mismo broker MQTT local (`localhost:1883`) y se apoyan en los topics publicados por `bridge/bridge.js` y `tournament-engine.js`.

## Requisitos

- Node-RED instalado.
- Paquete `node-red-dashboard` instalado. Los flows estan hechos para `node-red-dashboard` `3.6.6`.
- Broker MQTT local en `localhost:1883`.
- `bridge/bridge.js` publicando la telemetria de la pista.
- `tournament-engine.js` publicando el estado del torneo.

## Importar los flows

En Node-RED:

1. Abre el menu principal.
2. Entra en `Import`.
3. Pega el JSON del flow de pantalla grande o del control movil.
4. Pulsa `Import`.
5. Comprueba que el broker MQTT apunta a `localhost:1883`.
6. Pulsa `Deploy`.

El dashboard se abre normalmente en:

```text
http://localhost:1880/ui
```

Si accedes desde otro dispositivo de la red, usa la IP del ordenador donde corre Node-RED:

```text
http://IP_DEL_ORDENADOR:1880/ui
```

## Pantalla grande

Flow: `OpenLedRace - Full Screen No Header`

Es la vista pensada para un monitor, TV o proyector. Oculta la barra superior del dashboard para ocupar toda la pantalla.

### Entradas MQTT

| Nodo | Topic | Funcion |
| --- | --- | --- |
| `Estado` | `openledrace/tournament/state` | Recibe el estado completo del torneo. |
| `Players` | `openledrace/player/#` | Recibe telemetria de los dos jugadores. |

### Nodos principales

- `Store Data`: guarda en memoria de flow las vueltas y velocidad/distancia recibidas por MQTT.
- `Merge UI Data`: junta el ultimo estado del torneo con la telemetria mas reciente.
- `Pantalla Full Screen Sin Header`: renderiza toda la interfaz con un `ui_template`.

### Modo carrera

Cuando `msg.payload.mode === 'race'`, la pantalla muestra:

- Nombre del jugador del carril rojo (`player1`).
- Nombre del jugador del carril verde (`player2`).
- Vueltas de cada jugador.
- Velocidad mostrada como `km/h`.
- Barra de potencia para cada carril.
- Bloque central `VS`.
- Ronda actual del torneo, por ejemplo `Final`, `Semifinales` o `Cuartos de Final`.

La pantalla toma los nombres desde:

```text
msg.payload.currentMatch.match.player1
msg.payload.currentMatch.match.player2
```

Si no hay torneo cargado, usa valores por defecto:

```text
LOCAL
VISITANTE
```

### Modo bracket

Cuando `msg.payload.mode === 'bracket'`, la pantalla cambia a vista de torneo:

- Muestra todas las rondas del bracket.
- Oculta los partidos marcados como `isBye`.
- Resalta el ganador de cada enfrentamiento.
- Si el torneo ha terminado, muestra el campeon final.

El modo se cambia desde el control movil publicando:

```json
{"action":"set_mode","mode":"bracket"}
```

o:

```json
{"action":"set_mode","mode":"race"}
```

## Control movil

Flow: `OpenLedRace Tournament`

Es una vista compacta para usar desde el telefono durante el torneo.

### Entradas MQTT

| Nodo | Topic | Funcion |
| --- | --- | --- |
| `Estado torneo` | `openledrace/tournament/state` | Recibe el bracket y el partido actual. |
| `P1 km` | `openledrace/player/1/km` | Recibe distancia del jugador 1. |
| `P1 laps` | `openledrace/player/1/laps` | Recibe vueltas del jugador 1. |
| `P2 km` | `openledrace/player/2/km` | Recibe distancia del jugador 2. |
| `P2 laps` | `openledrace/player/2/laps` | Recibe vueltas del jugador 2. |

### Salida MQTT

El nodo `Enviar comando` publica en:

```text
openledrace/tournament/command
```

### Acciones disponibles

Marcar ganador del jugador 1:

```json
{
  "action": "set_winner",
  "matchId": "r1-m1",
  "winner": "Nombre jugador 1"
}
```

Marcar ganador del jugador 2:

```json
{
  "action": "set_winner",
  "matchId": "r1-m1",
  "winner": "Nombre jugador 2"
}
```

Mostrar bracket en la pantalla grande:

```json
{"action":"set_mode","mode":"bracket"}
```

Mostrar carrera en la pantalla grande:

```json
{"action":"set_mode","mode":"race"}
```

Reiniciar torneo:

```json
{"action":"reset_tournament"}
```

## Como encajan las piezas

El flujo completo es:

1. El Arduino ejecuta la carrera y emite eventos JSON por Serial.
2. `bridge/bridge.js` lee el Serial y publica telemetria en MQTT.
3. `tournament-engine.js` publica el estado del torneo y escucha comandos.
4. Node-RED lee los topics MQTT y actualiza la pantalla grande.
5. El movil envia comandos MQTT para cambiar modo o marcar ganadores.
6. El motor de torneo avanza el bracket y vuelve a publicar el estado actualizado.

## Topics usados por Node-RED

| Topic | Emisor | Consumidor | Uso |
| --- | --- | --- | --- |
| `openledrace/player/1/km` | Bridge | Node-RED | Distancia P1. |
| `openledrace/player/1/laps` | Bridge | Node-RED | Vueltas P1. |
| `openledrace/player/1/kmh` | Bridge | Node-RED pantalla grande | Velocidad P1 por coincidencia de topic. |
| `openledrace/player/2/km` | Bridge | Node-RED | Distancia P2. |
| `openledrace/player/2/laps` | Bridge | Node-RED | Vueltas P2. |
| `openledrace/player/2/kmh` | Bridge | Node-RED pantalla grande | Velocidad P2 por coincidencia de topic. |
| `openledrace/tournament/state` | Tournament engine | Node-RED | Estado completo del torneo. |
| `openledrace/tournament/command` | Node-RED movil | Tournament engine | Comandos de torneo. |

Nota: en el flow de pantalla grande se usa `openledrace/player/#` y despues se comprueba `includes('1/km')` o `includes('2/km')`. Eso tambien coincide con `openledrace/player/1/kmh` y `openledrace/player/2/kmh`, por eso la variable interna `p1km`/`p2km` acaba funcionando como velocidad en la interfaz. Si se quiere dejar mas claro, conviene renombrar esas variables a `p1kmh` y `p2kmh`.

## DERRAPE / accidente

En el firmware el derrape esta implementado como un sistema de `crash` y recuperacion. No depende de Node-RED: ocurre dentro de `main/main.ino` mientras se ejecuta la carrera.

### Donde esta configurado

```cpp
#define CRASH_SPEED_THRESHOLD  70.0f
#define CRASH_PROB_MAX         0.005f
#define RECOVERY_DURATION      2000UL
```

### Como se calcula la velocidad

Cada pulsacion aumenta la velocidad interna:

```cpp
speed += ACEL;
```

En cada ciclo tambien hay freno/friccion:

```cpp
speed -= speed * kf;
```

La velocidad interna se convierte a km/h con:

```cpp
float k = (spd / SPEED_MAX) * 100.0f;
return constrain(k, 0.0f, 100.0f) * 8.2;
```

### Cuando puede derrapar

Si la velocidad calculada esta por debajo o igual a `70 km/h`, no hay derrape.

```cpp
if (kmh <= CRASH_SPEED_THRESHOLD) return false;
```

Si supera `70 km/h`, aparece una probabilidad de accidente. Cuanto mas se pasa del umbral, mas probable es derrapar:

```cpp
float excess = (kmh - CRASH_SPEED_THRESHOLD) / (100.0f - CRASH_SPEED_THRESHOLD);
float prob = excess * CRASH_PROB_MAX;
```

Despues el firmware lanza una tirada aleatoria:

```cpp
return ((float)random(0, 10000) / 10000.0f) < prob;
```

### Que pasa cuando derrapa

Cuando un jugador derrapa:

1. Su velocidad pasa a `0`.
2. Entra en estado de recuperacion.
3. Su LED parpadea en blanco.
4. Durante la recuperacion no puede seguir acelerando.
5. Pasados `RECOVERY_DURATION` milisegundos, vuelve a la carrera.

La recuperacion dura por defecto:

```text
2000 ms = 2 segundos
```

El evento tambien sale por Serial como:

```json
{"evt":"crash","player":1}
```

o:

```json
{"evt":"crash","player":2}
```

El bridge publica ese evento en:

```text
openledrace/race/event
```

### Que se ve en la pista

- P1 normalmente es rojo.
- P2 normalmente es verde.
- Si un jugador derrapa, su posicion parpadea en blanco durante la recuperacion.

### Que se ve en Node-RED

La pantalla grande muestra la velocidad y la barra de potencia. Cuando la velocidad sube, el jugador se acerca a la zona de riesgo de derrape.

El template ya marca una clase `danger-active` cuando el valor mostrado supera `70`, aunque ahora mismo no hay una regla CSS especifica para esa clase. Se puede usar para añadir un aviso visual, por ejemplo borde rojo, parpadeo o texto de peligro.

## Recomendacion de ajuste

Si los derrapes ocurren demasiado pronto o demasiado tarde, toca estos valores en `main/main.ino`:

- Sube `CRASH_SPEED_THRESHOLD` para que sea mas dificil derrapar.
- Baja `CRASH_SPEED_THRESHOLD` para que sea mas facil derrapar.
- Sube `CRASH_PROB_MAX` para aumentar la probabilidad.
- Baja `CRASH_PROB_MAX` para hacer la carrera mas estable.
- Sube `RECOVERY_DURATION` para penalizar mas el derrape.
- Baja `RECOVERY_DURATION` para que el jugador vuelva antes a la carrera.
