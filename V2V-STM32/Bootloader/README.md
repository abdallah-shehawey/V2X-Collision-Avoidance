# 🔐 FOTA Bootloader — V2V-STM32

## STM32F446RE bootloader: A/B application slots, UART chunked-transfer
protocol, CRC-verified, automatic rollback on a failed boot.

This is Stages 1-4 of the FOTA roadmap in
[`../docs/FOTA.md`](../docs/FOTA.md) — an actual, buildable bootloader you
can flash and test **today, on the bench, with just a PC and a USB-TTL
adapter — no Raspberry Pi and no Firebase involved yet.** Those come later
(Stage 5+); this is deliberately self-contained so you can validate the hard
part (does the flash-write/rollback machinery actually work) before adding
any of the cloud/Pi plumbing on top of it.

> 🗣️ **بالعربي بسرعة:** ده البووتلودر الحقيقي (كود شغال، مش تصميم بس) —
> ممكن تبنيه وتجربه دلوقتي على الطبلوهة بس، من غير ما تستنى الراسبيري باي
> أو Firebase. الملف ده فيه كل خطوات البناء والتحديث والاختبار خطوة بخطوة.

---

## ⚠️ Before you start — read this

- **This is a *separate*, plain GNU Makefile project — it does NOT touch
  your existing STM32CubeIDE `V2V-STM32` project or its Debug/Release build
  configs.** Building/flashing the bootloader is a deliberate, separate
  action; your everyday CubeIDE workflow (build app, flash via ST-Link,
  debug) is completely unaffected until *you* choose to flash the
  bootloader onto a chip.
- **Flashing the bootloader overwrites whatever is at `0x08000000` today**
  — i.e. it replaces your current, full application. After flashing the
  bootloader alone (32 KB), the board will **not** run the V2V application
  anymore until you also flash an application build into Slot A
  (`0x0800C000`) — see Step 3 below. Do this on a board/chip you're happy to
  reflash, and know that ST-Link/SWD is always your way back to a clean
  slate if anything goes sideways (see [Recovery](#-recovery-if-something-goes-wrong)).
- The **UART4 (PA0/PA1)** pin plan is confirmed correct for the current
  firmware — `../Src/System.c` already wires the Raspberry Pi link to
  `UART4`/`PA0`(TX)/`PA1`(RX), matching `../docs/PCB_BUILD_STAGES.md`. This
  resolves the "which UART" open question from `../docs/FOTA.md` §5: **it's
  UART4**, confirmed from the actual running code, not just the docs. (The
  stale comment in `../Inc/System/System.h` saying USART2 is out of date —
  worth fixing separately, not touched here.)

---

## 📋 Prerequisites

| Tool | Why | Where to get it |
|---|---|---|
| `arm-none-eabi-gcc` (GNU Arm Embedded toolchain) | Builds the bootloader | Already inside your STM32CubeIDE install — see below — or [developer.arm.com](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) |
| `STM32_Programmer_CLI` | Flashes over ST-Link from the command line (`make flash`) | Comes with STM32CubeIDE / STM32CubeProgrammer |
| Python 3 + `pyserial` | Runs the bench-test script | `pip install pyserial` |
| A USB-TTL (USB-UART) adapter, 3.3V | Talks to the board's UART4 from your PC | Any FTDI/CP2102/CH340-based adapter |

**Finding the toolchain inside an existing STM32CubeIDE install** (Linux example — adjust for your OS):
```bash
find ~/st -iname "arm-none-eabi-gcc" 2>/dev/null
# typically: ~/st/stm32cubeide_.../plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.../tools/bin/
```
Add that `bin/` directory to your `PATH` (or pass `CC=/full/path/to/arm-none-eabi-gcc` to `make`) before continuing.

---

## 🔌 Wiring for bench testing

Same three wires as the normal Raspberry Pi link (`../docs/PCB_BUILD_STAGES.md`, Stage 8) — you're just putting a USB-TTL adapter where the Pi normally sits, temporarily:

```text
USB-TTL adapter          STM32 (Nucleo header)
─────────────────        ──────────────────────
   TX  ───────────────►  PA1  (UART4 RX)
   RX  ◄───────────────  PA0  (UART4 TX)
   GND ───────────────►  GND
```
Both sides are 3.3V — no level shifting needed. **Never connect the adapter's 5V/VCC pin.**

---

## 🛠️ Step 1 — Build the bootloader

```bash
cd V2V-STM32/Bootloader
make
```
Expect output ending in something like:
```text
   text    data     bss     dec     hex filename
   xxxx      xx     xxx    xxxx    xxxx build/bootloader.elf
```
This produces `build/bootloader.elf`, `build/bootloader.bin` and
`build/bootloader.hex`. **The `.bin` must be well under 32 KB** (Sectors 0-1
— see `../docs/FOTA.md` §3); the `Wall -Wextra` build will tell you about
any warnings worth looking at, but size is the number to actually check:
```bash
arm-none-eabi-size build/bootloader.elf
```

---

## 🛠️ Step 2 — Flash the bootloader

```bash
make flash
```
(runs `STM32_Programmer_CLI -c port=SWD -d build/bootloader.bin 0x08000000 -v -rst`)

Or, if you'd rather use the GUI: open **STM32CubeProgrammer**, connect over
ST-Link, and flash `build/bootloader.bin` at address `0x08000000`.

**At this point the board has a bootloader but no application in either
slot yet** — it will sit in its ~1 second recovery window on every reset,
find nothing, and jump to `0x0800C000` (Slot A) anyway, which is currently
blank/erased flash — it will hard-fault or run garbage. That's expected and
harmless; Step 3 fixes it.

---

## 🛠️ Step 3 — Build the application for Slot A, and flash it once over SWD

The application needs to be linked to run from `0x0800C000` instead of
`0x08000000` — this needs **one new STM32CubeIDE build configuration**
(your existing `Debug`/`Release` configs are untouched and still build the
old, whole-chip layout for whenever you want to go back to that):

1. In STM32CubeIDE, right-click the `V2V-STM32` project → **Build
   Configurations → Manage...** → **New...**
2. Name it `SlotA`, copy settings from `Release` (or `Debug`).
3. Open the new configuration's **Properties → C/C++ Build → Settings →
   MCU GCC Linker → General → Script file**, and point it at
   `STM32F446RETX_FLASH_SlotA.ld` (browse to the project root — this file
   already exists, added alongside the original `STM32F446RETX_FLASH.ld`).
4. **Build** that configuration. Its output `.bin` (in
   `SlotA/V2V-STM32.bin` or similar, depending on your build config's output
   folder) is the Slot A image.
5. Flash **that** `.bin` at address `0x0800C000` (not `0x08000000`!) via
   STM32CubeProgrammer, or:
   ```bash
   STM32_Programmer_CLI -c port=SWD -d path/to/SlotA/V2V-STM32.bin 0x0800C000 -v -rst
   ```

Power-cycle the board. It should now boot completely normally (LEDs,
telemetry, everything) — the bootloader jumped straight to Slot A in under
a second, exactly like before, just with an extra ~1s pause on every reset
you may or may not even notice.

> Repeat steps 3-5 with a **`SlotB`** configuration pointed at
> `STM32F446RETX_FLASH_SlotB.ld` whenever you want to prepare a Slot B build
> too — you won't need to flash Slot B manually, though: that's the whole
> point of Step 5 below.

---

## 🛠️ Step 4 — Package a build as a `.fpkg`

```bash
cd V2V-STM32/Bootloader/tools
python3 fota_packager.py path/to/SlotB/V2V-STM32.bin 1 1 firmware_v1.fpkg
#                        ^binary                     ^build_no ^board_id (1 = Nucleo)
```

Build a **Slot B** image for this (the bootloader always targets whichever
slot is *not* currently active — since Slot A is active after Step 3, the
first update you test should be a Slot B build). Bump `build_no` on each
new package you make (it's what lets the board tell "is this newer").

---

## 🛠️ Step 5 — Push it in and watch it install

```bash
python3 fota_bench_test.py --port /dev/ttyUSB0 --file firmware_v1.fpkg
```

Then **reset the board** (NRST button, or power-cycle) and run the command
above **within about a second** — that's the bootloader's recovery window.
If you keep missing the window, either:
- get faster (run the command, then immediately hit reset — the script's
  first `HELLO` retries are short, but starting the script a beat before
  resetting the board works fine too, it just waits on an empty serial
  buffer), or
- temporarily widen `BOOTLOADER_RECOVERY_WINDOW_MS` in
  `Src/Bootloader_main.c` (e.g. to `5000`), rebuild, and reflash the
  bootloader — purely a bench-testing convenience, not something to ship.

Expected output:
```text
Package: build=1 board=0x01 header_ver=1 len=123456 crc32=0x9F2A1B7C
-> HELLO  (reset the board now if it isn't already in its listen window)
<- HELLO_ACK: active_slot=A build_no=0
-> START_UPDATE
<- START_ACK (target slot erased, ready for chunks)
  chunk 20/483  (5120/123456 bytes)
  ...
  chunk 483/483  (123456/123456 bytes)
Transfer done in 9.8s (12.3 KB/s)
-> END_UPDATE
<- END_ACK (whole-image CRC32 verified against the header)
-> COMMIT
<- COMMIT_ACK — the board is switching to the new slot and resetting now.
```

The board resets itself right after `COMMIT_ACK`. It should come up running
the **new** image from Slot B — same normal boot behaviour (LEDs, telemetry
on the same UART4, once the application's own `System_setup()` reclaims it)
as any other boot.

---

## ✅ Verifying the safety mechanisms actually work

Don't just test the happy path — this is the part of FOTA that exists
specifically to survive things going wrong. With physical access to the
board (which you have, on the bench), it costs nothing to check:

- **Bad CRC / truncated transfer:** interrupt `fota_bench_test.py` (Ctrl-C)
  partway through the chunk loop, then power-cycle the board. It should
  boot straight back into the still-untouched Slot A — the interrupted Slot
  B write was never committed, so it's simply ignored.
- **A build that never confirms itself:** flash something to Slot B that
  compiles but never gets through `FOTA_MarkBootOK()` in `main.c` — e.g.
  temporarily comment out the `vTask_Watchdog` creation, or insert an
  infinite loop before the scheduler starts. Commit it via the bench script.
  Watch it boot, fail to confirm, and — after one retry — automatically
  **roll back to Slot A on its own**, no script or human involvement.
- **Power loss mid-erase:** physically cut power to the board partway
  through a `START_UPDATE`'s sector erase (right after `-> START_UPDATE`,
  before `START_ACK`). Power back on: the bootloader should boot Slot A
  normally (Slot A was never touched; the half-erased Slot B is just inert,
  not-yet-valid).

If all three of those come back to a working board with no SWD intervention
needed, the core safety design is doing its job.

---

## 🩹 Recovery, if something goes wrong

Physical access + ST-Link/SWD always works, regardless of what's on the
chip — that is the ultimate fallback this whole scheme is designed around
(see `../docs/FOTA.md` §12, "no redundancy for the bootloader itself, and
that's an accepted trade-off"). Worst case, reflash from scratch:
```bash
# bootloader
STM32_Programmer_CLI -c port=SWD -d Bootloader/build/bootloader.bin 0x08000000 -v
# application, Slot A
STM32_Programmer_CLI -c port=SWD -d path/to/SlotA/V2V-STM32.bin 0x0800C000 -v -rst
```
Or just go back to flashing the original whole-chip `Debug`/`Release`
build at `0x08000000` the way you always have — nothing about this bootloader
work removes that option.

---

## 📁 What's in this folder

```text
Bootloader/
├── Src/Bootloader_main.c   # the whole bootloader: HW init, protocol, jump
├── Bootloader_FLASH.ld     # linker script — 32 KB at 0x08000000
├── Makefile                # plain GNU Makefile build (see prerequisites above)
├── tools/
│   ├── fota_packager.py    # raw .bin -> .fpkg
│   └── fota_bench_test.py  # PC-side test client for the wire protocol
└── README.md                # this file
```

Reused directly from the main firmware (not copied — one implementation,
shared by both the bootloader and the application): `../Src/RCC_program.c`,
`GPIO_prog.c`, `SCB_program.c`, `TIM_program.c`, `USART_program.c`,
`FLASH_program.c`, `FOTA_CRC32_program.c`, `FOTA_Metadata_program.c`, and
`../Startup/startup_stm32f446retx.s`.

---

## ⏭️ What's next

This covers Stages 1-4 of the roadmap in
[`../docs/FOTA.md`](../docs/FOTA.md#-11-staged-roadmap). Stage 5
(`RPI/FOTA/fota_agent.py` — the Raspberry Pi agent that fetches from
Firebase and drives this exact same wire protocol automatically) and Stage 6
(DashBoard UI hook) build on top of this unchanged — the bootloader does not
care whether the bytes on the other end of UART4 came from this Python
script or from the eventual Raspberry Pi agent.
