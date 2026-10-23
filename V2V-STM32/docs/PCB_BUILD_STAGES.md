# V2X Car PCB — Stage-by-Stage Build Guide (Altium)

Carrier board. Nothing main is soldered: STM32 / ESP32-S3 / MPU9250 plug into
headers; L298N sits off-board wired by cable; Ultrasonics & RPi are brought out as
**parallel male-pin + female-header** for flexible mounting.

Build the schematic **one block at a time** in the order below. Each block lists:
Goal · Nets · Required libraries · How to find and download them · Notes.

---

## Where to get Altium libraries (applies to every stage)

| Source | Link | What you get from it | How |
|---|---|---|---|
| **SamacSys / Component Search Engine** | componentsearchengine.com | Any IC / module / connector | Create a free account and install the **Altium plugin** → Search part → Place (symbol + footprint + 3D) |
| **SnapEDA** | snapeda.com | Dev-boards (Nucleo / ESP / HC-SR04 / MPU) | Search → Download → Altium Designer format → import |
| **Ultra Librarian** | ultralibrarian.com | Alternative source for dev-boards | Search → Export → Altium |
| **Espressif official** | github.com/espressif (KiCad/Altium) | ESP32-S3 devkit | Download the Altium files, or convert |
| **Altium built-in** | Manufacturer Part Search panel | Passives, headers, standard connectors | Directly inside Altium |

> Fastest route: install the **SamacSys Altium plugin** (from componentsearchengine.com)
> — it lives inside Altium and pulls any part in with one click. Get the dev-boards
> from **SnapEDA**.

---

# STAGE 1 — POWER BLOCK (the foundation — start here)

### Goal

Distribute power safely, and keep motor noise from reaching the sensors.

### Nets

```
VBAT (2S LiPo ~7.4V)  ──► VMOT  (straight to the L298N — the motor domain)
VBAT  ──► [BUCK 7.4→5V] ──► +5V  ──► Nucleo 5V , HC-SR04 ×6 , ESP 5V
+3V3 (from the Nucleo) ──► MPU9250
GND  ──► STAR GROUND (a single point where every ground meets)
```

- The Raspberry Pi **takes no power from this board** — only GND and UART.

### Components

| Component | Qty | Purpose |
|---|---|---|
| Screw terminal 2-pin (5.08 mm) | 1 | Battery input, VBAT |
| Buck module LM2596 (or MP1584), ≥ 2 A | 1 | 7.4 V → 5 V |
| Slide switch SPDT | 1 | Main switch (optional) |
| Electrolytic cap 1000 µF / 16 V | 1 | On VMOT (absorbs motor inrush) |
| Electrolytic cap 470 µF / 10 V | 1 | On +5 V |
| Ceramic caps 100 nF + 10 µF | several | Decoupling |
| Fuse holder + 2–3 A fuse | 1 | Protection (optional) |

### Required libraries and how to get them

| Part | Source | Search term |
|---|---|---|
| Screw terminal 2P 5.08 | SamacSys | `screw terminal 2 5.08` (e.g. Phoenix `1935161`) |
| LM2596 module | Altium built-in, or SamacSys | Place it as a **1×4 header** (IN+ / IN- / OUT+ / OUT-) from Misc Connectors; or pull the IC `LM2596S-5.0` from SamacSys |
| Slide switch SPDT | SamacSys | `slide switch SPDT` |
| Caps / Resistors / Fuse | Altium built-in | `Miscellaneous Devices.IntLib` |

### Critical notes

- **Star ground:** one single point where the battery GND, motor GND, 5 V GND,
  logic GND and the Raspberry Pi's GND wire all meet. The motor's return current
  goes straight back to that point and never shares a path with the sensors' ground.
- Keep the motor domain (VMOT + L298N) isolated in its own corner of the board.

---

# STAGE 2 — MCU BLOCK (Nucleo-F446RE)

### Goal

A base of headers the Nucleo plugs into, so it can be removed.

### Nets

The Nucleo brings every GPIO out on the **Morpho CN7 + CN10** connectors. Place
matching female headers and route the nets from there to the other blocks. (The
full table is in the "PIN MAP" section at the end of this file.)

### Components

| Component | Qty |
|---|---|
| Female header 2×19 (2.54 mm) — Morpho | 2 |
| (Optional) Female header for the Arduino pins | as needed |

### Libraries and how to get them

| Part | Source | Search term |
|---|---|---|
| Nucleo-F446RE footprint | **SnapEDA** | `Nucleo-F446RE` or `Nucleo-64` |
| Alternative: 2×19 header | Altium built-in | `Header 19X2` from Misc Connectors |

### Notes

- **Confirm the Morpho pin count (2×19) against your board's mechanical drawing**
  before you commit to it.
- Bring SWD (PA13 / PA14) and PA2 / PA3 (USART2 debug) out onto a small header.

---

# STAGE 3 — FEEDBACK BLOCK (5 LEDs + buzzer) ⭐ start testing with this one

### Goal

The simplest block — visual and audible outputs. Easy to confirm it works.

### Nets (→ STM32)

```
LED_FR  → PC0      LED_FL  → PC1
LED_BR  → PC2      LED_BL  → PC3
LED_INT → PC7      BUZZER  → PC4
```

Each LED: STM32 pin → 330 Ω → LED → GND (active-high).
Buzzer: if it draws more than 8 mA, drive it through an NPN transistor with a
1 kΩ base resistor.

### Components

| Component | Qty |
|---|---|
| LED (3/5 mm or 0805) | 5 |
| Resistor 330 Ω | 5 |
| Active buzzer | 1 |
| NPN (2N2222 / BC547) + 1 kΩ + diode | 1 (only if the buzzer is a strong one) |

### Libraries and how to get them

| Part | Source | Search term |
|---|---|---|
| LED / Resistor | Altium built-in | `Miscellaneous Devices.IntLib` |
| Active buzzer | SamacSys | `magnetic buzzer`, or your buzzer's exact model |
| NPN transistor | Altium / SamacSys | `2N2222` / `BC547` |

---

# STAGE 4 — ULTRASONICS BLOCK (×6) — the most important one

### Goal

Six sensors, each with a **parallel pin + header** and an optional divider.

### Nets (→ STM32)

| Sensor | Position | TRIG | ECHO |
|---|---|---|---|
| US1 | Front-Left   | PB0  | PA15 |
| US2 | Front-Center | PB1  | PB3 |
| US3 | Front-Right  | PB2  | PB4 |
| US4 | Back-Left    | PB12 | PB5 |
| US5 | Back-Center  | PB13 | PC8 |
| US6 | Back-Right   | PB14 | PC9 |

**Dual access per sensor:** the same four nets (VCC / TRIG / ECHO / GND) go to both a
**1×4 male header** *and* a **1×4 female header** beside it — so you can either plug the
sensor straight onto the male pins, or run a cable from the female header if you need
to raise or angle it.

**Echo level (5 V → 3.3 V):** the echo pins (PA15, PB3, PB4, PB5, PC8, PC9) are all
**5 V-tolerant** on the F446 and do work directly on the Nucleo — but on the PCB, place
an **optional divider footprint** anyway: R1 = 1 kΩ (echo → pin) and R2 = 2 kΩ (pin → GND).
Fit R1 = 0 Ω and leave R2 unpopulated if you want the direct connection.

### Components

| Component | Qty |
|---|---|
| HC-SR04 | 6 |
| Male header 1×4 | 6 |
| Female header 1×4 | 6 |
| Divider resistors 1k / 2k (0603) | 6 pairs |
| 100 nF cap on each sensor's VCC | 6 |

### Libraries and how to get them

| Part | Source | Search term |
|---|---|---|
| HC-SR04 | **SnapEDA** | `HC-SR04` |
| Headers 1×4 | Altium built-in | `Header 4` / `Receptacle 4` |
| R / C | Altium built-in | Misc Devices |

### Notes

- Keep the echo traces short and well away from the motor traces — noise here shows
  up as a constant 400 cm reading.
- The 100 nF cap on each sensor's VCC is what suppresses the glitches.

---

# STAGE 5 — IMU BLOCK (MPU9250 / SPI1)

### Goal

A header the MPU9250 plugs into — fixed, but removable.

### Nets (→ STM32)

```
MPU_SCK  → PA5      MPU_MOSI → PA7
MPU_MISO → PA6      MPU_CS   → PA4
VCC → +3V3          GND → GND
```

| Module pin | Net | STM32 |
|---|---|---|
| SCL / SCK | MPU_SCK | PA5 |
| SDA / SDI | MPU_MOSI | PA7 |
| ADO / SDO | MPU_MISO | PA6 |
| NCS | MPU_CS | PA4 |

### Components

| Component | Qty |
|---|---|
| Female header 1×8 | 1 |
| Cap 100 nF + 10 µF | 1 of each |

### Libraries and how to get them

| Part | Source | Search term |
|---|---|---|
| MPU9250 / GY-91 module | **SnapEDA** | `MPU9250` / `GY-91` |
| Alternative: 1×8 header | Altium built-in | `Header 8` |

### Notes

- 3.3 V only — **not** 5 V.
- The INT pin is optional; you can wire it to a spare GPIO for future use.

---

# STAGE 6 — ESP32-S3 BLOCK (V2X / USART1)

### Goal

A header base for the ESP32-S3, plus the UART link to the STM32.

### Nets (cross-over!)

```
STM PA9  (ESP_TXD) ──► ESP RX0     ← crossed
STM PA10 (ESP_RXD) ◄── ESP TX0
+5V → ESP 5V        GND → GND
```

### Components

| Component | Qty |
|---|---|
| Female header matching the DevKitC-1 (≈ 2×22) | 2 |
| Cap 100 nF | 1–2 |

### Libraries and how to get them

| Part | Source | Search term |
|---|---|---|
| ESP32-S3-DevKitC-1 | **Espressif official** or **SnapEDA** | `ESP32-S3-DevKitC-1` |

### Notes

- **Confirm your devkit's pin count** — it varies between variants.
- TX ↔ RX must be crossed, and a shared GND is mandatory.
- The baud rate must be 115200 at both ends, to match the firmware.

---

# STAGE 7 — MOTOR DRIVER BLOCK (L298N — a base, wired by cable)

### Goal

The L298N sits on a header base and is connected by cables, not soldered down.

### Nets (→ STM32)

```
MOT_R_EN  → PA8     MOT_R_IN1 → PC5    MOT_R_IN2 → PC6
MOT_L_EN  → PA11    MOT_L_IN3 → PB10   MOT_L_IN4 → PB15
VMOT → battery+     GND → star ground
```

### Components

| Component | Qty |
|---|---|
| Male / female header 1×6 (control) | 1 |
| Screw terminals for the motor / power | as needed |
| Cap 1000 µF on VMOT | 1 |

### Libraries and how to get them

| Part | Source | Search term |
|---|---|---|
| L298N module | SamacSys / SnapEDA | `L298N module` (or just make a simple 1×6 header) |
| Screw terminals | SamacSys | `screw terminal` |

### Notes

- The motor is a noise domain — keep it far from the sensors, and return its GND to
  the star point.

---

# STAGE 8 — RASPBERRY PI 5 INTERFACE (pin + header)

### Goal

Only three wires to the Raspberry Pi: TX, RX, GND. (The Pi has its own power.)

### Nets

```
STM PA0 (RPI_TXD) ──► RPi GPIO15/RXD (pin 10)   ← crossed
STM PA1 (RPI_RXD) ◄── RPi GPIO14/TXD (pin 8)
GND ──► RPi GND (pin 6)
```

- Both sides are 3.3 V → **no level shifting needed**.
- Bring it out as **parallel male pin + female header**, the same way as the sensors.

### Components and libraries

| Part | Source | Search term |
|---|---|---|
| Male + female header 1×3 | Altium built-in | `Header 3` / `Receptacle 3` |

### Notes

- **Never** share 5 V with the Raspberry Pi — neither give it power nor take power
  from it.
- A shared GND is mandatory, or the UART will not work.

---

# STAGE 9 — DEBUG / MISC

| Signal | Pin | Purpose |
|---|---|---|
| SWDIO / SWCLK | PA13 / PA14 | Programming and debug |
| USART2 (VCP) | PA2 / PA3 | Serial debug |

Bring these out on a small 1×4-to-1×5 header. (Altium built-in `Header`.)

---

## Suggested build order (schematic first, then layout)

```
1. POWER  → 2. MCU  → 3. FEEDBACK  → 4. ULTRASONICS
→ 5. IMU  → 6. ESP  → 7. MOTOR  → 8. RPi  → 9. DEBUG
```

For each stage: draw the schematic block → name the nets exactly as above → annotate.

Final step: lay the board out along the same block boundaries — motor in one corner,
sensors far away from it.

---

## Full PIN MAP (reference — from the firmware, `Src/System.c`)

| Net | STM32 | Net | STM32 |
|---|---|---|---|
| MPU_SCK | PA5 | US1_TRIG/ECHO | PB0 / PA15 |
| MPU_MISO | PA6 | US2_TRIG/ECHO | PB1 / PB3 |
| MPU_MOSI | PA7 | US3_TRIG/ECHO | PB2 / PB4 |
| MPU_CS | PA4 | US4_TRIG/ECHO | PB12 / PB5 |
| ESP_TXD | PA9 | US5_TRIG/ECHO | PB13 / PC8 |
| ESP_RXD | PA10 | US6_TRIG/ECHO | PB14 / PC9 |
| RPI_TXD | PA0 | LED_FR/FL | PC0 / PC1 |
| RPI_RXD | PA1 | LED_BR/BL | PC2 / PC3 |
| MOT_R_EN | PA8 | LED_INT | PC7 |
| MOT_R_IN1/IN2 | PC5 / PC6 | BUZZER | PC4 |
| MOT_L_EN | PA11 | MOT_L_IN3/IN4 | PB10 / PB15 |

*If you change any pin in the firmware, update this table to match.*
