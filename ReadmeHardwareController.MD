# Hardware-Controller-Integration: JLCooper SloMo Mini

Anbindung eines JLCooper SloMo Mini Replay-Controllers an [obs-multireplay](https://github.com/ThommiTechnik2/obs-multireplay) für EVS-Style Replay-Steuerung in einer Broadcast-Umgebung.

## Architektur

```
JLCooper SloMo Mini  --(Raw USB, FTDI)-->  slomo-bridge.js (Node.js)  --(obs-websocket)-->  OBS + obs-multireplay
```

Die Hardware-Anbindung läuft komplett **außerhalb** des OBS-Plugins: `obs-multireplay` selbst enthält keinen HID-/USB-Code. Stattdessen kommuniziert eine eigenständige Node.js-Bridge (`slomo-bridge.js`) direkt mit dem Controller über Raw-USB und steuert OBS über obs-websocket-Vendor-Requests im Namespace `"multireplay"`.

**Warum diese Trennung?**
- USB/HID-Zugriff ist in Node.js deutlich komfortabler als in C++ (keine plattformspezifischen Treiber-Themen im Plugin-Build)
- Skaliert auf mehrere Controllertypen (z. B. Contour ShuttlePRO v2, in Vorbereitung)
- Bridge lässt sich unabhängig neu starten/debuggen, ohne OBS neu zu laden

## Hardware: JLCooper SloMo Mini

- **Verbindung:** FTDI-basiert, Raw-USB — **kein** Sony-9-Pin-Protokoll
- **USB-IDs:** VID `0x0760`, PID `0x0005`
- **FTDI-Init:** 38400 Baud, 8O1 (Odd Parity), keine Flow Control
- **Raw Mode:** wird per Byte-Sequenz `0x8f 0xff` + Register-Reads aktiviert (siehe `enterRawMode()` in der Bridge)
- **Keep-Alive:** Register `0x0b`–`0x0f` werden alle 50ms abgefragt, analog zum Verhalten der Original-Software M|Replay

### Eingehendes Protokoll (Gerät → Bridge)

| Tag-Byte | Bedeutung |
|---|---|
| `0x80` | Taste (Press/Release-Bytepaar; Release-Byte = Press-Byte − `0x40`) |
| `0x81` | Jog-Rad, 7-Bit Zweierkomplement (`0x01`–`0x3F` vorwärts, `0x40`–`0x7F` rückwärts) |
| `0x83` | T-Bar, Werte 0–63 (0 = Stop, 63 = Play) |

### Ausgehendes Display-Protokoll (Bridge → Gerät)

Verifiziert per systematischem USB-Test:

```
0x81  0x00  <32 Bytes ASCII: Zeile1[16] + Zeile2[16]>  0xff
```

Beide Displayzeilen (je 16 Zeichen) werden in **einem** Write geschrieben. Timing-Tests bei 200ms Refresh-Intervall zeigten keinerlei Interferenz mit dem parallel laufenden Tasten-/Jog-/T-Bar-Polling.

**Wichtig:** Alle USB-Writes (Keep-Alive, Display-Updates, etc.) laufen über eine gemeinsame Warteschlange (`sendBytes`), da gleichzeitige `transferOut`-Aufrufe aus unabhängigen Timern sonst zu `endpoint not found`-Fehlern führen.

## Tastenbelegung

| Taste (Press-Byte) | Funktion |
|---|---|
| `mark-in` / `mark-out` | Replay Mark In / Mark Out |
| `w1` / `w2` | Mark In/Out 5s / 10s rückwirkend |
| `play` | Play Last Event to Output |
| `stop` | Stop (Playback anhalten) |
| `reverse-play` | Wiedergabe rückwärts |
| `f-fwd` | Jump to Now (zurück zum Live-Rand) |
| `frame-minus1` / `frame-plus1` | Einzelbild vor/zurück |
| `cam-a`…`cam-d` | Kamera 1–4 wählen |
| `shift` + `cam-a`…`cam-d` | Kamera 5–8 wählen |
| `cycle-player` | Aktiven Kanal (A/B) togglen — gelinkter A+B-Modus bleibt bewusst UI-only |
| `replay` | Recording Start/Stop togglen |
| `last-cue` / `next-cue` | Event-Auswahl vor/zurück |
| `store-cue` | Nächste Playlist (1–20) |
| `shift` + `store-cue` | Vorherige Playlist |
| `digit-0`…`digit-9` + `enter` | Zifferneingabe → Sprung zu Event-ID |
| `clr` | Zifferneingabe-Puffer löschen |

### T-Bar

Stufenlose Replay-Geschwindigkeit über `set_speed`-Vendor-Request: Wert 0 → 5 %, Wert 63 → 100 %.

### Jog-Rad

Rohe Deltas werden über ein 50ms-Zeitfenster gesammelt und per `scrub_seconds`-Vendor-Request als ein einziger Sprung umgesetzt (nicht `step_frames`, da das bei sustained Jog die Wiedergabe-Queue bei jedem Aufruf neu startet — führte ursprünglich zum "Wheel of Death").

## Display-Anzeige

Zwei Zeilen, EVS-Style, Refresh alle 200ms:

```
Zeile 1: TC 00:01:23:14        (Timecode)
Zeile 2: E042 L03 100%         (Event-ID, aktive Playlist, Speed)
```

**Hinweis zum Timecode:** Die Anzeige basiert auf der Master-Timeline des Plugins (absolute Instants seit Session-/Recording-Start), nicht auf einer Clip-relativen Zeit. Im Live-Modus zeigt sie faktisch die Laufzeit der Session/Aufnahme — bewusste Design-Entscheidung, aktuell nicht clip-relativ (könnte später ergänzt werden, indem die Event-In-Zeit vom Cursor subtrahiert wird).

## Erweiterungen im obs-multireplay-Fork

Neue Vendor-Requests im Namespace `"multireplay"`, zusätzlich zu den bereits bestehenden (`step_frames`, `set_speed`, `scrub_seconds`, `step_event_selection`, `select_event_by_id`):

| Request | Zweck |
|---|---|
| `get_playback_status` | Liefert `cursorMs`, `speedPercent`, `eventId`, `activeList` für die Display-Anzeige |
| `toggle_active_channel` | Togglet den aktiven Kanal zwischen A und B |
| `step_list_selection` | Wechselt die aktive Playlist (mit Wrap-Around 1–20) |

Dafür wurden in `multireplay-dock.hpp` dünne öffentliche Bridge-Wrapper um sonst private Member ergänzt (`currentSpeedPercentBridge()`, `markTimeNsBridge()`, `selectedEventIdsBridge()`, `toggleActiveChannelBridge()`), da die zugrundeliegenden Methoden und Felder (`markTimeNs()`, `selectedEventIds()`, `speedPct_`) private sind.

## Setup

Voraussetzungen:
```bash
npm install usb obs-websocket-js
```

Wichtig: M|Replay (oder ein anderes Programm, das das Gerät bereits benutzt) darf nicht gleichzeitig laufen — nur ein Prozess kann das USB-Interface exklusiv beanspruchen.

```bash
node slomo-bridge.js
```

## Offene Punkte / mögliche nächste Schritte

- Clip-relative Timecode-Anzeige (aktuell: absolute Master-Timeline-Zeit)
- Gelinkter A+B-Kanalmodus über Hardware (aktuell bewusst UI-only)
- Contour ShuttlePRO v2 (VID `0x0b33`, PID `0x0030`) als zweiter unterstützter Controller — HID-Report-Struktur bereits ermittelt (Shuttle-Ring, Jog-Zähler, 15 Tasten-Bitmask)
