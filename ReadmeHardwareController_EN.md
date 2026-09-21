# Hardware Controller Integration: JLCooper SloMo Mini

Integration of a JLCooper SloMo Mini replay controller with [obs-multireplay](https://github.com/ThommiTechnik2/obs-multireplay) for EVS-style replay control in a broadcast environment.

## Architecture

```
JLCooper SloMo Mini  --(Raw USB, FTDI)-->  JLCooper-bridge.js (Node.js)  --(obs-websocket)-->  OBS + obs-multireplay
```

The hardware integration runs entirely **outside** the OBS plugin: `obs-multireplay` itself contains no HID/USB code. Instead, a standalone Node.js bridge (`JLCooper.js`) talks directly to the controller over raw USB and drives OBS via obs-websocket vendor requests in the `"multireplay"` namespace.

**Why this separation?**
- USB/HID access is significantly more convenient in Node.js than in C++ (no platform-specific driver headaches inside the plugin build)
- Scales to multiple controller types (e.g. Contour ShuttlePRO v2, in progress)
- The bridge can be restarted/debugged independently, without reloading OBS

## Hardware: JLCooper SloMo Mini

- **Connection:** FTDI-based, raw USB — **not** the Sony 9-pin protocol
- **USB IDs:** VID `0x0760`, PID `0x0005`
- **FTDI init:** 38400 baud, 8O1 (odd parity), no flow control
- **Raw Mode:** activated via the byte sequence `0x8f 0xff` + register reads (see `enterRawMode()` in the bridge)
- **Keep-alive:** registers `0x0b`–`0x0f` are polled every 50ms, mirroring the behavior of the original M|Replay software

### Incoming protocol (device → bridge)

| Tag byte | Meaning |
|---|---|
| `0x80` | Button (press/release byte pair; release byte = press byte − `0x40`) |
| `0x81` | Jog wheel, 7-bit two's complement (`0x01`–`0x3F` forward, `0x40`–`0x7F` reverse) |
| `0x83` | T-bar, values 0–63 (0 = stop, 63 = play) |

### Outgoing display protocol (bridge → device)

Verified through systematic USB testing:

```
0x81  0x00  <32 bytes ASCII: line1[16] + line2[16]>  0xff
```

Both display lines (16 characters each) are written in a **single** write. Timing tests at a 200ms refresh interval showed no interference with the button/jog/T-bar polling running in parallel.

**Important:** All USB writes (keep-alive, display updates, etc.) go through a shared queue (`sendBytes`), since concurrent `transferOut` calls from independent timers otherwise cause `endpoint not found` errors.

## Button mapping

| Button (press byte) | Function |
|---|---|
| `mark-in` / `mark-out` | Replay Mark In / Mark Out |
| `w1` / `w2` | Mark In/Out 5s / 10s lookback |
| `play` | Play Last Event to Output |
| `stop` | Stop (halt playback) |
| `reverse-play` | Play backward |
| `f-fwd` | Jump to Now (back to the live edge) |
| `frame-minus1` / `frame-plus1` | Step one frame back/forward |
| `cam-a`…`cam-d` | Select camera 1–4 |
| `shift` + `cam-a`…`cam-d` | Select camera 5–8 |
| `cycle-player` | Toggle the active channel (A/B) — the linked A+B mode is deliberately kept UI-only |
| `replay` | Toggle recording start/stop |
| `last-cue` / `next-cue` | Step event selection back/forward |
| `store-cue` | Next playlist (1–20) |
| `shift` + `store-cue` | Previous playlist |
| `digit-0`…`digit-9` + `enter` | Digit entry → jump to event ID |
| `clr` | Clear the digit entry buffer |

### T-bar

Stepless replay speed via the `set_speed` vendor request: value 0 → 5%, value 63 → 100%.

### Jog wheel

Raw deltas are accumulated over a 50ms window and applied as a single jump via the `scrub_seconds` vendor request (not `step_frames`, which restarts the playback queue on every call during sustained jogging — this originally caused the "wheel of death").

## Display

Two lines, EVS-style, refreshed every 200ms:

```
Line 1: TC 00:01:23:14        (timecode)
Line 2: E042 L03 100%         (event ID, active playlist, speed)
```

**Note on the timecode:** the display is driven by the plugin's master timeline (absolute instants since session/recording start), not a clip-relative time. In live mode it effectively shows the session/recording's running time — a deliberate design decision; not currently clip-relative (could be added later by subtracting the event's in-point from the cursor).

## Extensions in the obs-multireplay fork

New vendor requests in the `"multireplay"` namespace, in addition to the existing ones (`step_frames`, `set_speed`, `scrub_seconds`, `step_event_selection`, `select_event_by_id`):

| Request | Purpose |
|---|---|
| `get_playback_status` | Returns `cursorMs`, `speedPercent`, `eventId`, `activeList` for the display |
| `toggle_active_channel` | Toggles the active channel between A and B |
| `step_list_selection` | Changes the active playlist (wraps around 1–20) |

To support these, thin public bridge wrappers around otherwise-private members were added to `multireplay-dock.hpp` (`currentSpeedPercentBridge()`, `markTimeNsBridge()`, `selectedEventIdsBridge()`, `toggleActiveChannelBridge()`), since the underlying methods and fields (`markTimeNs()`, `selectedEventIds()`, `speedPct_`) are private.

## Setup

Requirements:
```bash
npm install usb obs-websocket-js
```

```bash
node JLCooper.js
```

## Open items / possible next steps

- Clip-relative timecode display (currently: absolute master-timeline time)
- Linked A+B channel mode from the hardware (currently deliberately UI-only)
- Contour ShuttlePRO v2 (VID `0x0b33`, PID `0x0030`) as a second supported controller — HID report structure already determined (shuttle ring, jog counter, 15-button bitmask)
