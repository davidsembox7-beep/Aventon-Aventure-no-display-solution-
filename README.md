# Aventon Aventure.2 — running the stock controller with no display

The BC280 display on an Aventon Aventure.2 is not just a screen. Without it the
controller sits there doing nothing: the throttle is dead, pedalling does
nothing, and the motor never turns, even though the motor, throttle and battery
are all perfectly fine.

This documents the protocol that display speaks, so you can replace it with an
Arduino and a phone.

**Result:** throttle works, pedal assist works with levels 0–5, lights work,
walk assist works. Controlled from a phone over Bluetooth.

---

## The protocol

**KingMeter 618U style, 9600 baud, 8N1**, on the two serial wires in the
display branch.

### Display → controller

Exactly **6 bytes**:

```
46  <flags>  20  00  00  0D
```

| flags bit | meaning |
|---|---|
| 0–3 | assist level 0–5 (**pedal assist only**) |
| 4 | walk assist — fixed slow speed, runs with no throttle |
| 7 | lights — front and rear together |

Bits 5 and 6 do nothing that we could find.

The message length is **exactly 6 bytes**. Send 7 or more and the controller
does not reply at all — we tested lengths 6 through 12, and only 6 gets a
response.

### Controller → display

**8 bytes**, sent in reply to every message:

```
46  03  <current>  <speed hi>  <speed lo>  00  <x>  <x>
```

`speed` is a **wheel period, so lower means faster**. `3341` (`0x0D0D`) means
stopped. Around `818` is walk-assist speed. Around `210–350` is real riding
speed.

`current` reads 0–2 while cruising and jumps to ~5 under hard acceleration,
which is why we think it's motor current.

---

## Wiring

8-pin Julet connector at the controller. Pinout confirmed by continuity and
measurement:

| Pin | Function |
|---|---|
| 1 | GND |
| 3 | 5 V rail |
| 4 | VCC (battery, 48 V) |
| 5 | K — ignition |
| 6 | serial |
| 7 | serial |

The display branch has 5 wires: VCC, GND, and three signal wires. VCC→pin 4,
GND→pin 1, wire 1→pin 6, wire 2→pin 7, wire 3→pin 5.

Arduino connections:

```
D0 (RX)  ->  display wire 1
D1 (TX)  ->  display wire 2
GND      ->  display GND      <-- REQUIRED, see below
```

### The K wire

The controller will not boot unless K is pulled to battery voltage. Feeding it
through a resistor does **not** work — 10 kΩ from VCC gave 2.18 V and no boot,
6 kΩ gave 2.33 V and no boot. It needs a proper connection to VCC. That's what
the display does: it switches battery positive onto K.

---

## Things that cost us days — read this part

### 1. Ground. Connect the ground.

We spent an enormous amount of time sending every known e-bike protocol at every
baud rate and getting nothing back, concluding one protocol after another was
"ruled out". **The Arduino's ground was never connected to the bike's ground.**

Every one of those negative results was meaningless. With no shared ground
reference the serial levels are undefined, and we were also chasing a phantom
"echo" between the two wires that turned out to be pure ground-loop noise — it
vanished the instant a real ground was connected.

If you take one thing from this document: connect GND first.

### 2. Assist level does nothing unless you are pedalling

We swept every byte of the message looking for the assist level and concluded
"nothing affects speed". That conclusion was wrong, and we reached it **twice**.

Assist level only affects the **pedal assist** path. If nobody is turning the
pedals, changing the assist level produces no motor response at all, so it looks
dead. The only thing that moves the wheel with no pedalling is walk assist.

To test assist levels you have to actually pedal.

### 3. The throttle is not gated by assist level

The throttle gives full power at **any** assist level, including 0, as long as
something is holding up the serial link. Aventon's "the throttle does nothing at
PAS 0" really means "the throttle does nothing when no display is talking to the
controller."

So the unlock is simply: **send valid messages continuously and the throttle
works.**

### 4. The brake light is a fault indicator

While we were sending malformed messages, the brake light came on, and it went
off again as soon as valid messages resumed. It's a useful free diagnostic for
"the controller doesn't understand you".

---

## What's in here

| file | what it is |
|---|---|
| `bike_ble/bike_ble.ino` | Arduino sketch — BLE control, passcode protected |
| `index.html` | phone web app, talks to the board over Web Bluetooth |
| `manifest.json`, `sw.js`, `icon.svg` | make the page installable and work offline |

Built and tested on an **Arduino UNO R4 WiFi**. Any board with BLE and a spare
hardware serial port should work.

---

## Setup

1. Open `bike_ble/bike_ble.ino`, change `PASSCODE` to something of your own,
   and flash it to the board.
2. Wire D0, D1 and **GND** as above.
3. Host this folder on GitHub Pages (Web Bluetooth requires HTTPS, so the page
   can't be served from the Arduino itself).
4. Open the page on an Android phone in Chrome, type your passcode, tap
   Connect.
5. Chrome menu → Add to Home screen. It then works offline as an app.

The passcode is stored on your phone only — it is never part of this source, so
publishing your copy doesn't let anyone else control your bike.

**Web Bluetooth does not work in Safari on iPhone.** Android only.

---

## Powering the Arduino

Don't take power from the brake wire. It measures about 4.75 V but delivers
essentially no current — it's a sensing input pulled up through a resistor, not
a supply.

The display's own VCC is **48 V**, far above the 6–24 V an UNO R4 accepts on
VIN, so it can't be used directly either. A USB power bank is the simplest safe
option.

---

## Status

Working: throttle, pedal assist levels 0–5, lights, walk assist, live speed
readout.

Not identified: speed limit and wheel size settings. Bytes 3 and 4 of the
message are the likely candidates and are probably live — our sweeps of them
were run without a throttle or pedalling, which is exactly the mistake described
above, so those results should not be trusted.

Front and rear lights share one output and cannot be controlled separately.

---

## Disclaimer

This modifies the control system of an electric bicycle. A mistake can cause the
motor to run when you don't expect it. Test with the wheel off the ground. You
are responsible for your own bike and your own safety.

MIT licensed — see `LICENSE`.
