# Aventon Aventure — running the controller without a display

The Aventon Aventure ships with a BC280 display. Remove it and the bike is
dead: the throttle does nothing, pedalling does nothing, and the motor never
turns, even with a healthy motor, throttle and battery.

The display isn't just a screen — the controller waits for it to talk before it
will do anything. This documents that conversation, so an Arduino can replace
it.

Working: throttle, pedal assist levels 0–5, lights, walk assist, live speed
readout, controlled from a phone over Bluetooth.

## Protocol

KingMeter 618U style, **9600 baud, 8N1**, on the two serial wires of the display
branch.

### Display → controller

Exactly 6 bytes:

```
46  <flags>  20  00  00  0D
```

| flags bit | meaning |
|---|---|
| 0–3 | assist level 0–5 (pedal assist only) |
| 4 | walk assist — fixed slow speed, runs without throttle |
| 7 | lights — front and rear share one output |

Bits 5 and 6 have no observed effect.

Length must be exactly 6. Lengths 7 through 12 receive no reply.

### Controller → display

8 bytes, returned to every message:

```
46  03  <current>  <speed hi>  <speed lo>  00  <x>  <x>
```

`speed` is a wheel period, so **lower is faster**. `0x0D0D` (3341) means
stopped; roughly 818 is walk-assist speed and 210–350 is riding speed.

`current` sits at 0–2 while cruising and rises to about 5 under acceleration.

## Wiring

8-pin Julet connector at the controller:

| Pin | Function |
|---|---|
| 1 | GND |
| 3 | 5 V |
| 4 | VCC (48 V) |
| 5 | K — ignition |
| 6 | serial |
| 7 | serial |

The display branch carries VCC, GND and three signal wires: wire 1 → pin 6,
wire 2 → pin 7, wire 3 → pin 5.

Arduino:

```
D0 (RX)  ->  display wire 1
D1 (TX)  ->  display wire 2
GND      ->  display GND
```

**K must be connected directly to VCC.** The display switches battery positive
onto it. Feeding K through a resistor does not boot the controller — 10 kΩ gives
2.18 V and 6 kΩ gives 2.33 V, neither is enough.

## Notes

**Ground is mandatory.** Without the Arduino ground tied to the bike ground the
serial levels are undefined, the controller never replies, and the two signal
wires show a phantom echo that looks like real traffic. It is ground-loop noise
and disappears once ground is connected.

**Assist level only affects pedalling.** With nobody turning the pedals,
changing the level produces no motor response, so the byte looks dead. Test it
while pedalling.

**The throttle ignores assist level.** It delivers full power at any level,
including 0, as long as messages are being sent. The unlock is simply holding
the link up.

**The brake light indicates a malformed message.** It comes on while the
controller receives frames it can't parse and goes off when valid ones resume.

## Contents

| file | purpose |
|---|---|
| `bike_ble/bike_ble.ino` | Arduino sketch — Bluetooth control, passcode protected |
| `index.html` | phone app, connects over Web Bluetooth |
| `manifest.json`, `sw.js`, `icon.svg` | make the page installable and work offline |

Built on an Arduino UNO R4 WiFi. Any board with BLE and a spare hardware serial
port should work.

## Setup

1. Set `PASSCODE` in `bike_ble/bike_ble.ino` and flash the board.
2. Wire D0, D1 and GND as above.
3. Host this folder on GitHub Pages. Web Bluetooth requires HTTPS, so the page
   cannot be served from the Arduino.
4. Open it in Chrome on Android, enter the passcode, tap Connect.
5. Chrome menu → Add to Home screen. It then runs offline as an app.

The passcode is stored on the phone, never in the source, so a published copy
gives no one else control of the bike.

Web Bluetooth is unavailable in Safari, so iOS is not supported.

## Unidentified

Speed limit and wheel size. Bytes 3 and 4 are the likely candidates but were
only tested without a throttle or pedalling, so those results prove nothing.

## Disclaimer

This modifies the control system of an electric bicycle and can make the motor
run unexpectedly. Test with the wheel off the ground. MIT licensed, no warranty
— see `LICENSE`.
