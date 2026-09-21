// slomo-bridge.js
//
// Bridge: JLCooper SloMo Mini (Raw Mode, roh per USB) -> OBS MultiReplay
// (via obs-websocket).
//
// Voraussetzungen:
//   npm install usb obs-websocket-js
//
// Ausführen:
//   node JLCooper-bridge.js
//

const { usb } = require('usb');
const OBSWebSocket = require('obs-websocket-js').default;

// ---------------------------------------------------------------------------
// Konfiguration -- hier anpassen
// ---------------------------------------------------------------------------
const VENDOR_ID = 0x0760;
const PRODUCT_ID = 0x0005;
const IN_ENDPOINT = 1;
const OUT_ENDPOINT = 2;

const OBS_WS_URL = 'ws://10.89.64.1:4455';
const OBS_WS_PASSWORD = '';

const BAUD_38400 = 0xc04e; // FTDI-Baudratencode fuer 38400 (siehe frueheres Skript)
const DISPLAY_REFRESH_MS = 200; // siehe Timing-Test: keine Interferenz mit Input-Polling

// ---------------------------------------------------------------------------
// Tasten-Tabelle (aus den Wireshark-Mitschnitten gegen M|Replay gewonnen)
// Press-Byte -> Name. Release-Byte = Press-Byte - 0x40 (empirisch bestaetigt
// bei jeder bisher getesteten Taste).
// ---------------------------------------------------------------------------
const BUTTON_NAMES = {
  0x40: 'replay',
  0x45: 'shift',
  0x46: 'cycle-player',
  0x4c: 'reverse-play',
  0x4d: 'stop',
  0x4e: 'play',
  0x4f: 'f-fwd',
  0x50: 'cam-a',
  0x51: 'cam-b',
  0x52: 'cam-c',
  0x53: 'cam-d',
  0x54: 'mark-in',
  0x55: 'w1',
  0x56: 'frame-minus1',
  0x58: 'frame-plus1',
  0x59: 'w2',
  0x5a: 'mark-out',
  0x5c: 'store-cue',
  0x5d: 'last-cue',
  0x5e: 'next-cue',
  0x5f: 'digit-0',
  0x60: 'digit-1',
  0x61: 'digit-2',
  0x62: 'digit-3',
  0x63: 'digit-4',
  0x64: 'digit-5',
  0x65: 'digit-6',
  0x66: 'digit-7',
  0x67: 'digit-8',
  0x68: 'digit-9',
  0x69: 'clr',
  0x6a: 'enter',
};

// ---------------------------------------------------------------------------
// OBS-Anbindung
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

// Vendor-Requests aus dem MultiReplay-Patch (siehe vendor-request-patch.diff):
// "step_frames" (delta: int, +vor/-rueck) und "set_speed" (percent: 5..200).
// Ein einzelner Aufruf erledigt das direkt im Plugin, ohne den Umweg ueber
// OBS' Hotkey-Dispatch -- das war der eigentliche Grund fuer das "Wheel of
// Death" bei schnellem Jog-Drehen ueber TriggerHotkeyByName.
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
// Bridge-eigener Zustand (Dinge, die das Geraet selbst nicht mitteilt)
// ---------------------------------------------------------------------------
let shiftHeld = false;
let isRecording = false;
let lastSpeedPct = null;

function selectCamera(baseNumber) {
  const cam = shiftHeld ? baseNumber + 4 : baseNumber;
  triggerHotkey(`ReplayACamera${cam}`);
}

function toggleRecording() {
  isRecording = !isRecording;
  triggerHotkey(isRecording ? 'MultiReplayStartRecording' : 'MultiReplayStopRecording');
}

function stepList(delta) {
  const actualDelta = shiftHeld ? -delta : delta;
  callVendor('step_list_selection', { delta: actualDelta });
}

// Tasten, die einen direkten OBS-Hotkey ausloesen (nur bei PRESS, nicht Release)
const BUTTON_ACTIONS = {
  'mark-in': () => triggerHotkey('ReplayMarkIn'),
  'mark-out': () => triggerHotkey('ReplayMarkOut'),
  'w1': () => triggerHotkey('ReplayMarkInOut5'),
  'w2': () => triggerHotkey('ReplayMarkInOut10'),
  'play': () => triggerHotkey('ReplayPlayLastEventToOutput'),
  'f-fwd': () => triggerHotkey('ReplayJumpToNow'),
  'frame-minus1': () => triggerHotkey('ReplayStepBackward'),
  'frame-plus1': () => triggerHotkey('ReplayStepForward'),
  'cam-a': () => selectCamera(1),
  'cam-b': () => selectCamera(2),
  'cam-c': () => selectCamera(3),
  'cam-d': () => selectCamera(4),
  'replay': () => toggleRecording(),
  'stop': () => triggerHotkey('ReplayStopEvents'),
  'reverse-play': () => triggerHotkey('ReplayPlayReverse'),
  'cycle-player': () => callVendor('toggle_active_channel', {}),
  'store-cue': () => stepList(1),
  'last-cue': () => callVendor('step_event_selection', { delta: -1 }),
  'next-cue': () => callVendor('step_event_selection', { delta: 1 }),
  // store-cue: noch nicht zugeordnet
};

// Dreistellige Zifferneingabe: Ziffern sammeln, CLR leert den Puffer, Enter
// interpretiert den Puffer als Event-ID und springt per "select_event_by_id"
// direkt zu diesem Clip.
let digitBuffer = '';
const DIGIT_MAP = {
  'digit-0': '0', 'digit-1': '1', 'digit-2': '2', 'digit-3': '3', 'digit-4': '4',
  'digit-5': '5', 'digit-6': '6', 'digit-7': '7', 'digit-8': '8', 'digit-9': '9',
};

function handleButton(name, pressed) {
  const ts = new Date().toISOString().split('T')[1].replace('Z', '');
  console.log(`[${ts}] Taste ${name}: ${pressed ? 'PRESS' : 'release'}`);

  if (name === 'shift') {
    shiftHeld = pressed;
    return;
  }
  if (!pressed) return; // alles andere nur bei PRESS ausloesen

  if (DIGIT_MAP[name]) {
    digitBuffer = (digitBuffer + DIGIT_MAP[name]).slice(-3);
    console.log(`  -> Zifferneingabe: "${digitBuffer}"`);
    return;
  }
  if (name === 'clr') {
    digitBuffer = '';
    console.log('  -> Zifferneingabe geloescht');
    return;
  }
  if (name === 'enter') {
    if (digitBuffer !== '') {
      const id = parseInt(digitBuffer, 10);
      console.log(`  -> Springe zu Event-ID ${id}`);
      callVendor('select_event_by_id', { id });
      digitBuffer = '';
    }
    return;
  }

  const action = BUTTON_ACTIONS[name];
  if (action) {
    action();
  } else {
    console.log(`  -> (noch keine Aktion fuer "${name}" hinterlegt)`);
  }
}

// ---------------------------------------------------------------------------
// T-Bar (Kanal 0x83, Wertebereich 0-63) -> Replay-Geschwindigkeit
//
// Jetzt echtes Stufenlos ueber den "set_speed"-Vendor-Request (siehe
// vendor-request-patch.diff): 0 -> 5% (Minimum, das applyReplaySpeed noch
// zulaesst), 63 -> 100% ("Play Speed", das obere Ende der T-Bar-Kalibrierung
// von letzter Woche). Kein Snapping auf die vier UI-Presets mehr noetig.
// ---------------------------------------------------------------------------
function tbarValueToPercent(value) {
  return Math.max(5, Math.round((value / 63) * 100));
}

function handleTBar(value) {
  const pct = tbarValueToPercent(value);
  if (pct !== lastSpeedPct) {
    lastSpeedPct = pct;
    console.log(`T-Bar: Wert ${value}/63 -> ${pct}%`);
    callVendor('set_speed', { percent: pct });
  }
}

// ---------------------------------------------------------------------------
// Jog-Rad (Kanal 0x81)
//
// Der rohe Wert ist eine 7-Bit-Zweierkomplement-Zahl, die das Geraet SELBST
// mit der Drehgeschwindigkeit skaliert: 0x01..0x3F = +1..+63 (vorwaerts),
// 0x40..0x7F = -64..-1 (rueckwaerts). Bestaetigt durch einen kontrollierten
// Test: langsames Drehen in eine Richtung zeigte sauber steigende Werte
// 01,02,03...07, die andere Richtung sauber fallende 7f,7e,7d...7a.
//
// WICHTIG: Jog/Shuttle nutzt "scrub_seconds" (EIN direkter Sprung im Plugin,
// via scrubBySeconds/seekToFraction), NICHT "step_frames". step_frames ruft
// intern stepFrameForward()/Backward() auf, die bei JEDEM Aufruf die
// Wiedergabe-Queue stoppen/neu starten -- fuer die einzelnen Frame-+-1/-1-
// Tasten genau richtig, aber bei sustained schnellem Shutteln (~34
// Ereignisse/Sekunde) voellig ueberlastet: MultiReplay lief dann so weit
// hinterher, dass selbst voellig unabhaengige Anfragen (z.B. Mark In)
// minutenlang aufgestaut blieben. scrub_seconds macht denselben Sprung in
// EINER Operation, unabhaengig von der Sprunggroesse.
//
// Rohe Deltas werden trotzdem ueber ein kurzes Zeitfenster gesammelt (statt
// bei jedem einzelnen Ereignis sofort zu senden), damit sich gegenlaeufige
// Rausch-Ausrutscher (+1 mitten in einer -8-Serie) von selbst aufheben.
//
// JOG_FPS_ASSUMPTION: die rohen Delta-Werte sind "frame-artig" skaliert;
// zur Umrechnung in Sekunden fuer scrubBySeconds nehmen wir eine Framerate
// an. Passt sie nicht zu eurem Projekt, hier anpassen.
// ---------------------------------------------------------------------------
const JOG_FLUSH_INTERVAL_MS = 50;
const JOG_FPS_ASSUMPTION = 25;

let jogAccumulator = 0;
let jogFlushTimer = null;

function flushJog() {
  jogFlushTimer = null;
  if (jogAccumulator === 0) return;
  const deltaFrames = jogAccumulator;
  jogAccumulator = 0;
  const seconds = deltaFrames / JOG_FPS_ASSUMPTION;
  console.log(`Jog: gesammeltes Delta=${deltaFrames} Frames -> ${seconds.toFixed(3)}s`);
  callVendor('scrub_seconds', { seconds });
}

function handleJog(value) {
  if (value === 0x00) return;

  const delta = value < 0x40 ? value : value - 128;
  jogAccumulator += delta;

  if (!jogFlushTimer) {
    jogFlushTimer = setTimeout(flushJog, JOG_FLUSH_INTERVAL_MS);
  }
}

// ---------------------------------------------------------------------------
// USB / FTDI-Low-Level
// ---------------------------------------------------------------------------
async function ftdiControlOut(device, request, value, index = 0) {
  await device.controlTransferOut(
    { requestType: 'vendor', recipient: 'device', request, value, index },
    new Uint8Array(0)
  );
}

async function initFtdi(device) {
  const SIO_DATA_8O1 = 0x0008 | (0x1 << 8); // 8 Datenbits, Odd Parity, 1 Stopbit
  await ftdiControlOut(device, 0x00, 0x00); // Reset
  await ftdiControlOut(device, 0x00, 0x01); // Purge RX
  await ftdiControlOut(device, 0x00, 0x02); // Purge TX
  await ftdiControlOut(device, 0x04, SIO_DATA_8O1);
  await ftdiControlOut(device, 0x02, 0x0000); // kein Flow Control
  await ftdiControlOut(device, 0x03, BAUD_38400);
  await ftdiControlOut(device, 0x09, 1); // Latency Timer 1ms
}

async function sendBytes(device, bytes) {
  await device.transferOut(OUT_ENDPOINT, new Uint8Array(bytes));
}

// ---------------------------------------------------------------------------
// Display: Timecode (Zeile 1) + Event-ID/Bank/Speed (Zeile 2), EVS-Style
// Protokoll verifiziert per test-display.js: 0x81 0x00 <32 Bytes> 0xff,
// beide Zeilen in EINEM Write.
// ---------------------------------------------------------------------------
function formatTimecode(ms, fps = 25) {
  if (ms < 0 || isNaN(ms)) return '00:00:00:00';
  const totalSeconds = Math.floor(ms / 1000);
  const frames = Math.floor(((ms % 1000) / 1000) * fps);
  const seconds = totalSeconds % 60;
  const minutes = Math.floor(totalSeconds / 60) % 60;
  const hours = Math.floor(totalSeconds / 3600);
  const pad = (n, len = 2) => String(n).padStart(len, '0');
  return `${pad(hours)}:${pad(minutes)}:${pad(seconds)}:${pad(frames)}`;
}

let deviceRef = null; // wird in main() nach claimInterface gesetzt

function writeToJLCooperDisplay(line1, line2) {
  if (!deviceRef) return;
  const pad16 = (s) => s.padEnd(16, ' ').slice(0, 16);
  const text32 = pad16(line1) + pad16(line2);
  const textBytes = Array.from(Buffer.from(text32, 'ascii'));
  sendBytes(deviceRef, [0x81, 0x00, ...textBytes, 0xff]).catch((err) => {
    console.error('Fehler beim Senden an das Display:', err.message);
  });
}

async function fetchPlaybackStatus() {
  try {
    const res = await obs.call('CallVendorRequest', {
      vendorName: 'multireplay',
      requestType: 'get_playback_status',
      requestData: {},
    });
    return res.responseData; // { success, cursorMs, speedPercent, eventId }
  } catch (err) {
    console.error('get_playback_status fehlgeschlagen:', err.message);
    return null;
  }
}

let displayTimer = null;

function startDisplayLoop() {
  if (displayTimer) clearInterval(displayTimer);
  displayTimer = setInterval(async () => {
    const status = await fetchPlaybackStatus();
    if (!status || !status.success) return;

    const tc = formatTimecode(status.cursorMs, JOG_FPS_ASSUMPTION);
    const bankLabel = 'L' + String(status.activeList).padStart(2, '0');
    const eventStr = String(status.eventId).padStart(3, '0');
    const speedStr = String(status.speedPercent).padStart(3, ' ');

    const line1 = `TC ${tc}`;
    const line2 = `E${eventStr} ${bankLabel} ${speedStr}%`;

    writeToJLCooperDisplay(line1, line2);
  }, DISPLAY_REFRESH_MS);
}

// Exakte Byte-Sequenz aus dem Wireshark-Mitschnitt von M|Replay beim Start,
// die das Geraet in den Raw Mode versetzt (inkl. der zwei LCD-Zeilen, die
// dabei mit Leerzeichen ueberschrieben werden).
async function enterRawMode(device) {
  const blank32 = new Array(32).fill(0x20);

  await sendBytes(device, [0x8f, 0xff]);
  for (let i = 0; i < 3; i++) {
    await sendBytes(device, [0x80, 0x10]);
    await sendBytes(device, [0x80, 0x11]);
    await sendBytes(device, [0x80, 0x12]);
    await sendBytes(device, [0x80, 0x13]);
  }
  await sendBytes(device, [0x80, 0x15]);
  await sendBytes(device, [0x80, 0x19]);
  await sendBytes(device, [0x81, 0x00, ...blank32, 0xff]);
  await sendBytes(device, [0x80, 0x15]);
  await sendBytes(device, [0x80, 0x19]);
  await sendBytes(device, [0x81, 0x00, ...blank32, 0xff]);

  console.log('✅ Raw-Mode-Sequenz gesendet.');
}

// M|Replay fragt im Betrieb periodisch die Register 0x0b-0x0f ab. Unklar, ob
// das fuer den Verbindungserhalt noetig ist -- wir machen es sicherheitshalber
// nach, bis das Gegenteil bewiesen ist.
function startKeepAlive(device) {
  setInterval(async () => {
    try {
      await sendBytes(device, [0x80, 0x0b]);
      await sendBytes(device, [0x80, 0x0c]);
      await sendBytes(device, [0x80, 0x0d]);
      await sendBytes(device, [0x80, 0x0e]);
      await sendBytes(device, [0x80, 0x0f]);
    } catch (err) {
      console.error('Keep-Alive-Fehler:', err.message);
    }
  }, 50);
}

// ---------------------------------------------------------------------------
// Eingehende Ereignisse parsen
// ---------------------------------------------------------------------------
function processBuffer(buffer) {
  let offset = 0;
  while (offset < buffer.length) {
    const tag = buffer[offset];

    if ((tag === 0x80 || tag === 0x81 || tag === 0x83) && offset + 1 < buffer.length) {
      const value = buffer[offset + 1];
      offset += 2;

      if (tag === 0x80) {
        const name = BUTTON_NAMES[value];
        const releaseName = BUTTON_NAMES[value + 0x40];
        if (name) {
          handleButton(name, true);
        } else if (releaseName) {
          handleButton(releaseName, false);
        } else {
          console.log(`Unbekannter Tastencode: 0x${value.toString(16)}`);
        }
      } else if (tag === 0x81) {
        handleJog(value);
      } else if (tag === 0x83) {
        handleTBar(value);
      }
    } else {
      // Kein bekanntes Tag-Byte -- ein Byte verwerfen und weiter versuchen
      offset += 1;
    }
  }
}

// ---------------------------------------------------------------------------
// Hauptprogramm
// ---------------------------------------------------------------------------
async function main() {
  console.log('Verbinde mit OBS...');
  await obs.connect(OBS_WS_URL, OBS_WS_PASSWORD);
  console.log('✅ Mit OBS verbunden.');

  console.log('Suche SloMo Mini...');
  const device = await usb.findDeviceByIds(VENDOR_ID, PRODUCT_ID);
  if (!device) {
    console.error('❌ Geraet nicht gefunden.');
    process.exit(1);
  }

  await device.open();
  if (!device.configuration && device.configurations.length > 0) {
    await device.selectConfiguration(device.configurations[0].configurationValue);
  }
  const ifaceNumber = device.configuration.interfaces[0].interfaceNumber;
  await device.claimInterface(ifaceNumber);
  console.log('✅ USB-Interface beansprucht.');

  deviceRef = device;
  await initFtdi(device);
  await enterRawMode(device);
  startKeepAlive(device);
  startDisplayLoop();

  console.log('\nBereit. Tasten/Jog/T-Bar am Geraet bedienen...\n');

  let recvBuffer = Buffer.alloc(0);
  for (;;) {
    let result;
    try {
      result = await device.transferIn(IN_ENDPOINT, 64);
    } catch (err) {
      console.error('Lesefehler:', err.message);
      break;
    }
    if (!result || !result.data || result.data.byteLength <= 2) continue;

    const raw = new Uint8Array(result.data.buffer, result.data.byteOffset, result.data.byteLength);
    const payload = Buffer.from(raw.slice(2)); // FTDI-Statusbytes abziehen
    recvBuffer = Buffer.concat([recvBuffer, payload]);

    processBuffer(recvBuffer);
    // Puffer nach Verarbeitung leeren -- processBuffer konsumiert linear von
    // vorne, Reste (z.B. ein halbes Paket am Pufferende) muessten fuer echte
    // Robustheit erhalten bleiben; fuer den ersten Entwurf bewusst einfach
    // gehalten.
    recvBuffer = Buffer.alloc(0);
  }

  await device.releaseInterface(ifaceNumber);
  await device.close();
  await obs.disconnect();
}

main().catch((err) => {
  console.error('Unerwarteter Fehler:', err);
  process.exit(1);
});
