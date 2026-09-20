// shuttlepro-bridge.js
//
// Bridge: Contour Design ShuttlePRO v2 (Standard-USB-HID) -> OBS MultiReplay
// (via obs-websocket).
//
// Im Unterschied zum JLCooper SloMo Mini ist die ShuttlePRO v2 ein ganz
// gewoehnliches USB-HID-Geraet -- kein FTDI-Chip, kein Raw-Mode-Handshake,
// keine Baudraten-Initialisierung. node-hid liest die Reports direkt.
//
// Voraussetzungen:
//   npm install node-hid obs-websocket-js
//
// Ausfuehren:
//   node shuttlepro-bridge.js
//
// WICHTIG: Falls Contour's eigene Konfigurationssoftware (ContourShuttle /
// der Windows/Mac-Treiber-Dienst) laeuft, beansprucht sie das Geraet
// exklusiv -- vor dem Start dieser Bridge beenden, genau wie bei M|Replay
// und der SlomoMini.

const HID = require('node-hid');
const OBSWebSocket = require('obs-websocket-js').default;

// ---------------------------------------------------------------------------
// Konfiguration -- hier anpassen
// ---------------------------------------------------------------------------
const VENDOR_ID = 0x0b33;
const PRODUCT_ID = 0x0030;

const OBS_WS_URL = 'ws://10.89.64.1:4455';
const OBS_WS_PASSWORD = '';

// ---------------------------------------------------------------------------
// Tasten-Tabelle
//
// Die ShuttlePRO v2 hat 15 Tasten, aber -- anders als beim SlomoMini --
// keine bedruckten Funktionsnamen (Bit-Reihenfolge siehe processReport()
// unten: Bits 0-7 in Byte[3] = Tasten 1-8, Bits 0-6 in Byte[4] = Tasten
// 9-15). Hier erstmal generisch benannt; einfach durch eure tatsaechliche
// Panel-Beschriftung ersetzen.
// ---------------------------------------------------------------------------
const BUTTON_NAMES = [
  'button-1', 'button-2', 'button-3', 'button-4', 'button-5',
  'button-6', 'button-7', 'button-8', 'button-9', 'button-10',
  'button-11', 'button-12', 'button-13', 'button-14', 'button-15',
];

// ---------------------------------------------------------------------------
// OBS-Anbindung (identisch zur SlomoMini-Bridge)
// ---------------------------------------------------------------------------
const obs = new OBSWebSocket();

async function triggerHotkey(name) {
  try {
    await obs.call('TriggerHotkeyByName', { hotkeyName: name });
    console.log(`  -> OBS-Hotkey ausgeloest: ${name}`);
  } catch (err) {
    console.error(`  -> Fehler beim Ausloesen von ${name}:`, err.message);
  }
}

async function callVendor(requestType, requestData) {
  try {
    await obs.call('CallVendorRequest', {
      vendorName: 'multireplay',
      requestType,
      requestData,
    });
  } catch (err) {
    console.error(`  -> Vendor-Request "${requestType}" fehlgeschlagen:`, err.message);
  }
}

// ---------------------------------------------------------------------------
// Tasten-Aktionen -- noch unbelegt, hier eure Zuordnung eintragen (analog zu
// BUTTON_ACTIONS in slomo-bridge.js). 'last-cue'/'next-cue' als Beispiel
// stehen gelassen, damit step_event_selection sofort testbar ist.
// ---------------------------------------------------------------------------
const BUTTON_ACTIONS = {
  // 'button-1': () => triggerHotkey('ReplayMarkIn'),
  // 'button-2': () => triggerHotkey('ReplayMarkOut'),
  'button-14': () => callVendor('step_event_selection', { delta: -1 }), // last-cue
  'button-15': () => callVendor('step_event_selection', { delta: 1 }),  // next-cue
};

function handleButton(name, pressed) {
  const ts = new Date().toISOString().split('T')[1].replace('Z', '');
  console.log(`[${ts}] Taste ${name}: ${pressed ? 'PRESS' : 'release'}`);

  if (!pressed) return; // alles unten nur bei PRESS ausloesen

  const action = BUTTON_ACTIONS[name];
  if (action) {
    action();
  } else {
    console.log(`  -> (noch keine Aktion fuer "${name}" hinterlegt)`);
  }
}

// ---------------------------------------------------------------------------
// Shuttle-Ring (Byte[0], signiert -7..+7, federzentriert)
//
// ANNAHME (bitte pruefen, ob das eurer gewuenschten Semantik entspricht):
// set_speed kennt nur 5..200% Vorwaertsgeschwindigkeit, keine Rueckwaerts-
// wiedergabe. Deshalb hier als reine Geschwindigkeits-Wippe interpretiert:
//   Mitte (0)   -> 100% (normale Geschwindigkeit)
//   rechts (+7) -> 200% (schnellste Vorwaertswiedergabe)
//   links  (-7) ->   5% (langsamste Vorwaertswiedergabe)
// Falls ihr stattdessen "links = rueckwaerts schutteln" wollt, braucht das
// eine Erweiterung auf Plugin-Seite (z.B. negative percent-Werte oder ein
// eigener "shuttle"-Vendor-Request mit Richtung) -- hier absichtlich noch
// nicht spekulativ vorgegriffen.
// ---------------------------------------------------------------------------
let lastSpeedPct = null;

function shuttleValueToPercent(value) {
  if (value === 0) return 100;
  if (value > 0) return Math.round(100 + (value / 7) * 100); // 100..200
  return Math.round(100 + (value / 7) * 95);                 // 100..5 (value ist negativ)
}

function handleShuttle(value) {
  const pct = shuttleValueToPercent(value);
  if (pct !== lastSpeedPct) {
    lastSpeedPct = pct;
    console.log(`Shuttle: Wert ${value} -> ${pct}%`);
    callVendor('set_speed', { percent: pct });
  }
}

// ---------------------------------------------------------------------------
// Jog-Wheel (Byte[1])
//
// Anders als beim SlomoMini (das direkt ein vorzeichenbehaftetes Delta pro
// Tick liefert) meldet die ShuttlePRO v2 eine ABSOLUTE, umlaufende 8-Bit-
// Position (0..255, wrapt bei Ueber-/Unterlauf). Das Delta muss hier selbst
// gebildet werden -- auf dem kuerzesten Weg auf dem 256er-Ring, damit ein
// Sprung von 255 auf 0 als +1 (nicht als -255) gezaehlt wird.
//
// Die allererste Messung hat keinen Vorwert; wie in der linuxcnc-shuttle-
// Doku vermerkt, geht deren erste Bewegung dadurch unvermeidlich verloren
// (der erste Report dient nur der Baseline-Initialisierung).
//
// Ab hier identisches Sammel-/Flush-Prinzip wie beim SlomoMini-Jog: rohe
// Deltas ueber ein kurzes Zeitfenster aufsummieren und dann in EINEM
// scrub_seconds-Aufruf umsetzen, statt bei jedem Tick einzeln step_frames
// zu rufen (siehe die ausfuehrliche Begruendung in slomo-bridge.js --
// "wheel of death").
// ---------------------------------------------------------------------------
const JOG_FLUSH_INTERVAL_MS = 50;
const JOG_FPS_ASSUMPTION = 25; // ggf. an euer Projekt anpassen

let prevJog = null;
let jogAccumulator = 0;
let jogFlushTimer = null;

function jogDelta(current, previous) {
  let diff = current - previous;
  if (diff > 128) diff -= 256;
  else if (diff < -128) diff += 256;
  return diff;
}

function flushJog() {
  jogFlushTimer = null;
  if (jogAccumulator === 0) return;
  const deltaTicks = jogAccumulator;
  jogAccumulator = 0;
  const seconds = deltaTicks / JOG_FPS_ASSUMPTION;
  console.log(`Jog: gesammeltes Delta=${deltaTicks} -> ${seconds.toFixed(3)}s`);
  callVendor('scrub_seconds', { seconds });
}

function handleJog(value) {
  if (prevJog === null) {
    prevJog = value; // nur Baseline setzen, keine Bewegung auswerten
    return;
  }
  const delta = jogDelta(value, prevJog);
  prevJog = value;
  if (delta === 0) return;

  jogAccumulator += delta;
  if (!jogFlushTimer) {
    jogFlushTimer = setTimeout(flushJog, JOG_FLUSH_INTERVAL_MS);
  }
}

// ---------------------------------------------------------------------------
// Report parsen
//
// 5 Bytes, ungepolstert:
//   [0] Shuttle (int8, -7..+7, 0 = Mitte)
//   [1] Jog (uint8, umlaufender Zaehler)
//   [2] unbenutzt
//   [3] Tasten 1-8  (Bit 0 = Taste 1 .. Bit 7 = Taste 8)
//   [4] Tasten 9-15 (Bit 0 = Taste 9 .. Bit 6 = Taste 15)
// ---------------------------------------------------------------------------
let prevButtonsLow = 0;
let prevButtonsHigh = 0;

function toInt8(byte) {
  return byte > 127 ? byte - 256 : byte;
}

function processButtonByte(current, previous, bitOffset) {
  const changed = current ^ previous;
  for (let bit = 0; bit < 8; bit++) {
    if (!((changed >> bit) & 1)) continue;
    const index = bitOffset + bit; // 0-basiert
    if (index >= BUTTON_NAMES.length) continue;
    const pressed = !!((current >> bit) & 1);
    handleButton(BUTTON_NAMES[index], pressed);
  }
}

function processReport(data) {
  if (data.length < 5) return;

  const shuttle = toInt8(data[0]);
  const jog = data[1];
  const buttonsLow = data[3];
  const buttonsHigh = data[4];

  handleShuttle(shuttle);
  handleJog(jog);

  processButtonByte(buttonsLow, prevButtonsLow, 0);
  processButtonByte(buttonsHigh, prevButtonsHigh, 8);
  prevButtonsLow = buttonsLow;
  prevButtonsHigh = buttonsHigh;
}

// ---------------------------------------------------------------------------
// Hauptprogramm
// ---------------------------------------------------------------------------
async function main() {
  console.log('Verbinde mit OBS...');
  await obs.connect(OBS_WS_URL, OBS_WS_PASSWORD);
  console.log('✅ Mit OBS verbunden.');

  console.log('Suche ShuttlePRO v2...');
  let device;
  try {
    device = new HID.HID(VENDOR_ID, PRODUCT_ID);
  } catch (err) {
    console.error('❌ Geraet nicht gefunden oder bereits von einem anderen Prozess');
    console.error('   (z.B. Contours eigener Treiber/Konfigurationssoftware) geoeffnet:', err.message);
    process.exit(1);
  }
  console.log('✅ HID-Geraet geoeffnet.');

  device.on('data', (data) => {
    processReport(data);
  });

  device.on('error', (err) => {
    console.error('HID-Fehler:', err.message);
  });

  console.log('\nBereit. Tasten/Jog/Shuttle am Geraet bedienen...\n');
}

main().catch((err) => {
  console.error('Unerwarteter Fehler:', err);
  process.exit(1);
});
