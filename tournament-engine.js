const fs = require("fs");
const path = require("path");
const mqtt = require("mqtt");

// --- CONFIGURACIÓN ---
const MQTT_BROKER = process.env.MQTT_BROKER || "mqtt://localhost:1883";
const MQTT_USER   = process.env.MQTT_USER   || undefined;
const MQTT_PASS   = process.env.MQTT_PASS   || undefined;

const PLAYERS_FILE = path.join(__dirname, "players.txt");

const TOPIC_STATE   = "openledrace/tournament/state";
const TOPIC_COMMAND = "openledrace/tournament/command";
const TOPIC_EVENT   = "openledrace/tournament/event";
const TOPIC_MODE    = "openledrace/tournament/mode";

let tournament  = null;
let currentMode = "bracket";

// --- UTILIDADES LÓGICAS ---

function shuffle(arr) {
  const a = [...arr];
  for (let i = a.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [a[i], a[j]] = [a[j], a[i]];
  }
  return a;
}

function nextPow2(n) { 
  let p = 1; 
  while (p < n) p *= 2; 
  return p; 
}

function roundName(size) {
  const names = {
    2: "Final",
    4: "Semifinales",
    8: "Cuartos de Final",
    16: "Octavos de Final",
    32: "Dieciseisavos de Final",
    64: "Treintaidosavos de Final"
  };
  return names[size] || `Ronda de ${size}`;
}

function readPlayers() {
  if (!fs.existsSync(PLAYERS_FILE)) throw new Error("No existe players.txt");
  const players = fs.readFileSync(PLAYERS_FILE, "utf8")
    .split(/\r?\n/).map(p => p.trim()).filter(Boolean);
  const unique = [...new Set(players)];
  if (unique.length < 2) throw new Error("Necesitas al menos 2 jugadores");
  return unique;
}

// --- GESTIÓN DE TORNEO ---

function canResolveBye(t, roundIdx, matchIdx) {
  if (roundIdx === 0) return true;
  const prev = t.rounds[roundIdx - 1];
  const f1 = prev.matches[matchIdx * 2];
  const f2 = prev.matches[matchIdx * 2 + 1];
  return f1 && f1.completed && f2 && f2.completed;
}

function resolveByes(t) {
  let changed = true;
  while (changed) {
    changed = false;
    for (let r = 0; r < t.rounds.length; r++) {
      for (let m = 0; m < t.rounds[r].matches.length; m++) {
        const match = t.rounds[r].matches[m];
        if (match.completed) continue;
        if (!canResolveBye(t, r, m)) continue;
        
        const p1 = match.player1, p2 = match.player2;
        
        // Lógica de avance automático por BYE
        if (p1 === null && p2 === null) {
          match.completed = true; match.isBye = true; changed = true;
        } else if (p1 !== null && p2 === null) {
          match.winner = p1; match.completed = true; match.isBye = true;
          propagate(t, r, m, p1); changed = true;
        } else if (p1 === null && p2 !== null) {
          match.winner = p2; match.completed = true; match.isBye = true;
          propagate(t, r, m, p2); changed = true;
        }
      }
    }
  }
  checkChampion(t);
}

function propagate(t, ri, mi, winner) {
  const nr = ri + 1;
  if (nr >= t.rounds.length) { checkChampion(t); return; }
  const nm = t.rounds[nr].matches[Math.floor(mi / 2)];
  if (mi % 2 === 0) nm.player1 = winner; else nm.player2 = winner;
}

function checkChampion(t) {
  const f = t.rounds[t.rounds.length - 1].matches[0];
  if (f.completed && f.winner) { t.finished = true; t.champion = f.winner; }
}

function createTournament(players) {
  const shuffled = shuffle(players);
  const size = nextPow2(shuffled.length);
  const numMatchesFirstRound = size / 2;

  const rounds = [];
  let currentRoundSize = size;
  let rnum = 1;

  // 1. Crear la estructura de rondas vacía
  while (currentRoundSize >= 2) {
    const matchesCount = currentRoundSize / 2;
    const matches = [];
    for (let i = 0; i < matchesCount; i++) {
      matches.push({ 
        id: `r${rnum}-m${i + 1}`, round: rnum, index: i,
        player1: null, player2: null, winner: null,
        completed: false, isBye: false 
      });
    }
    rounds.push({ round: rnum, name: roundName(currentRoundSize), matches });
    currentRoundSize /= 2;
    rnum++;
  }

  // 2. Llenado inteligente de la primera ronda
  // Repartimos los jugadores para evitar BYE vs BYE
  const firstRoundMatches = rounds[0].matches;
  
  shuffled.forEach((player, index) => {
    // Si el índice es menor que el número de partidos, va a la posición local (p1)
    // Si es mayor, empieza a llenar las posiciones visitante (p2)
    if (index < numMatchesFirstRound) {
      firstRoundMatches[index].player1 = player;
    } else {
      firstRoundMatches[index - numMatchesFirstRound].player2 = player;
    }
  });

  const t = { 
    createdAt: new Date().toISOString(), finished: false, champion: null,
    players, bracketSize: size, rounds 
  };

  // 3. Resolver automáticamente los que no tienen rival
  resolveByes(t);
  return t;
}

// --- COMUNICACIÓN Y ESTADO ---

function getCurrentMatch(t) {
  if (!t || t.finished) return null;
  for (const round of t.rounds)
    for (const match of round.matches)
      if (!match.completed && !match.isBye && match.player1 !== null && match.player2 !== null)
        return { match, roundName: round.name };
  return null;
}

function setWinner(matchId, winnerName) {
  if (!tournament) throw new Error("No hay torneo");
  for (let r = 0; r < tournament.rounds.length; r++)
    for (let m = 0; m < tournament.rounds[r].matches.length; m++) {
      const match = tournament.rounds[r].matches[m];
      if (match.id !== matchId) continue;
      if (match.completed) throw new Error(`Partido ${matchId} ya cerrado`);
      if (winnerName !== match.player1 && winnerName !== match.player2)
        throw new Error(`${winnerName} no está en ${matchId}`);
      
      match.winner = winnerName; match.completed = true; match.isBye = false;
      propagate(tournament, r, m, winnerName);
      resolveByes(tournament);
      return;
    }
  throw new Error(`Partido ${matchId} no existe`);
}

function statePayload() {
  const current = getCurrentMatch(tournament);
  return { 
    type: "tournament_state", 
    timestamp: new Date().toISOString(),
    mode: currentMode, 
    currentMatch: current, 
    tournament 
  };
}

function publishState(client) { if (!tournament) return; client.publish(TOPIC_STATE, JSON.stringify(statePayload()), { retain: true }); }
function publishMode(client)  { client.publish(TOPIC_MODE, JSON.stringify({ mode: currentMode, timestamp: new Date().toISOString() }), { retain: true }); }
function publishEvent(client, event, data = {}) { client.publish(TOPIC_EVENT, JSON.stringify({ event, timestamp: new Date().toISOString(), ...data })); }

function loadTournament() {
  const players = readPlayers();
  tournament    = createTournament(players);
  currentMode   = "bracket";
  
  console.log(`\n🏆 Torneo: ${tournament.rounds[0].name} (${tournament.bracketSize} slots)`);
  tournament.rounds.forEach(r => {
    console.log(`--- ${r.name} ---`);
    r.matches.forEach(m => {
      const p1 = m.player1 || "---";
      const p2 = m.player2 || "---";
      const status = m.isBye ? `>> BYE (Pasa ${m.winner})` : (m.completed ? `>> GANÓ: ${m.winner}` : "[Pendiente]");
      console.log(`  ${m.id}: ${p1} vs ${p2} ${status}`);
    });
  });
}

// --- MQTT CLIENT ---

const client = mqtt.connect(MQTT_BROKER, { username: MQTT_USER, password: MQTT_PASS });

client.on("connect", () => {
  console.log("🔌 Conectado a MQTT:", MQTT_BROKER);
  try { 
    loadTournament(); 
    publishState(client); 
    publishMode(client); 
    publishEvent(client, "tournament_created", { playerCount: tournament.players.length }); 
  } catch (err) { 
    console.error("❌ Error:", err.message); 
  }
  client.subscribe(TOPIC_COMMAND);
});

client.on("message", (topic, message) => {
  if (topic !== TOPIC_COMMAND) return;
  let cmd; 
  try { cmd = JSON.parse(message.toString()); } catch { return; }
  
  try {
    switch (cmd.action) {
      case "set_winner":
        setWinner(cmd.matchId, cmd.winner);
        console.log(`✅ Ganador: ${cmd.winner} en ${cmd.matchId}`);
        publishEvent(client, "winner_set", { matchId: cmd.matchId, winner: cmd.winner });
        publishState(client);
        break;
      case "set_mode":
        currentMode = cmd.mode; 
        publishMode(client); 
        publishState(client); 
        break;
      case "reset_tournament":
      case "reload_players":
        loadTournament(); 
        publishState(client); 
        publishMode(client); 
        publishEvent(client, "tournament_reset"); 
        break;
      case "get_state":
        publishState(client); 
        break;
    }
  } catch (err) { 
    console.error("❌", err.message); 
    publishEvent(client, "error", { message: err.message }); 
  }
});

client.on("error", err => console.error("MQTT error:", err.message));