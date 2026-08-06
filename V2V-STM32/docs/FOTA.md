# 📡 FOTA — Firmware Over-The-Air Update

## Design & Roadmap for the STM32 Safety Core

> **Status: design document, not yet implemented.** Nothing in this file
> exists in the firmware today — no bootloader, no flash driver, no update
> protocol. This is the plan the team implements *against*, stage by stage
> (see [§11 Staged Roadmap](#-11-staged-roadmap)).

> 🗣️ **بالعربي بسرعة:** الملف ده مش كود شغال — ده تصميم كامل (roadmap)
> لإزاي هنضيف خاصية تحديث الفيرموير عن بعد (FOTA) لمتحكم الـ STM32 بتاع
> عربيتنا، من غير ما نحتاج نفك اللوحة ونوصلها بـ ST-Link كل مرة. هننفذه
> على مراحل، كل مرحلة قابلة للاختبار لوحدها قبل ما ننتقل للي بعدها.

---

## 📋 Table of Contents

1. [Overview](#-1-overview)
2. [Why a Bootloader + A/B Slots](#-2-why-a-bootloader--ab-slots-not-a-single-overwrite)
3. [Flash Partitioning](#-3-flash-partitioning)
4. [The Firmware Package Format (`.fpkg`)](#-4-the-firmware-package-format-fpkg)
5. [Wire Protocol — Raspberry Pi ⇄ STM32 Bootloader](#-5-wire-protocol--raspberry-pi--stm32-bootloader)
6. [New STM32 Driver Code Needed](#-6-new-stm32-driver-code-needed)
7. [Linker Script Changes](#-7-linker-script-changes)
8. [Cloud Distribution — How the Update Reaches the Raspberry Pi](#-8-cloud-distribution--how-the-update-reaches-the-raspberry-pi)
9. [Raspberry Pi Side — `RPI/FOTA/`](#-9-raspberry-pi-side--rpifota)
10. [End-to-End Flow](#-10-end-to-end-flow)
11. [Staged Roadmap](#-11-staged-roadmap)
12. [Risks & Gotchas](#-12-risks--gotchas)

---

## 🎯 1. Overview

```text
Firebase (Cloud Storage + Firestore)     ◄── developer publishes a new .fpkg
        │  HTTPS (internet)
        ▼
Raspberry Pi 5  ── RPI/FOTA/fota_agent.py
        │  UART (wired, local — NOT ESP-NOW, NOT the STM32's own radio)
        ▼
STM32F446RE  ── resident bootloader (Sectors 0-1)
        │  flashes the inactive app slot, verifies, then boots it
        ▼
New application running — reports back over the existing telemetry link
```

The STM32 has **no internet connectivity of its own** and never will —
it talks to the ESP32 only for the real-time V2V (ESP-NOW) mesh, and
that link must stay dedicated to collision-avoidance traffic, not firmware
bytes. The **Raspberry Pi** is the only node in the whole system with real
internet access today (it already runs an MQTT client to HiveMQ Cloud —
see [`RPI/V2N/Car_client.py`](../../RPI/V2N/Car_client.py)), so it is the
natural **FOTA gateway**: it fetches/downloads the new firmware from the
cloud, verifies it, and pushes it into the STM32 over a **wired UART**
link into a small bootloader that lives permanently in flash.

Three independent problems, three sections each answer:

| Problem | Answered in |
|---|---|
| How does a new build get from the developer's PC to the car's Raspberry Pi, over the internet? | [§8 Cloud Distribution](#-8-cloud-distribution--how-the-update-reaches-the-raspberry-pi) |
| How does the Raspberry Pi push that file into the STM32 safely? | [§4](#-4-the-firmware-package-format-fpkg) + [§5 Wire Protocol](#-5-wire-protocol--raspberry-pi--stm32-bootloader) |
| How does the STM32 write it to flash without bricking itself? | [§2](#-2-why-a-bootloader--ab-slots-not-a-single-overwrite) + [§3](#-3-flash-partitioning) + [§6 Driver code](#-6-new-stm32-driver-code-needed) |

> 🗣️ **بالعربي:** في 3 مراحل منفصلة تمامًا عن بعض: (1) إزاي التحديث
> بينزل من الإنترنت لجهاز الراسبيري باي (Firebase)، (2) إزاي الراسبيري
> باي بيبعت الملف ده للـ STM32 عن طريق سلك UART، (3) إزاي الـ STM32
> بيكتب التحديث في الفلاش بتاعه من غير ما "يتبلوك" (brick) لو حصل قطع
> كهرباء أو خطأ في النص. كل مرحلة موثقة في قسم منفصل تحت.

---

## 🧠 2. Why a Bootloader + A/B Slots (not a single overwrite)

The STM32F446RE has **512 KB of single-bank flash** — unlike some larger
STM32F4/F7 parts, it has **no dual-bank hardware** to swap between two
complete flash banks atomically. So "safe update" has to be built by hand
out of the flash sectors that do exist.

The naive approach — erase the running application and write the new one
in its place — fails the basic bricking test: if power drops, a UART byte
gets dropped, or the new image turns out to be bad *after* the old one is
already erased, the board has **no valid application left** and no way
back except SWD/ST-Link (i.e. someone physically opens the car).

The fix is **A/B slots**: two full copies of application flash, one
running (**active**), one idle. An update is written entirely into the
**idle** slot while the active one keeps running untouched. Only after the
new image is fully written and CRC-verified does a tiny "boot pointer" in
a separate metadata sector get flipped. If the new image then fails to
prove it's alive after booting, the bootloader flips the pointer straight
back — automatically, no Raspberry Pi or human involvement required.

> 🗣️ **بالعربي:** الشريحة عندنا فيها فلاش واحد بس (مش زي بعض الشرائح
> الكبيرة اللي فيها بنكين تبديل بينهم فورًا). فبدل ما نمسح الفيرموير
> الشغال ونكتب الجديد مكانه (لو حصل خطأ هنا هنبوظ الجهاز بالكامل)،
> بنعمل نسختين كاملتين من التطبيق جنب بعض في الفلاش: نسخة شغالة (A) ونسخة
> احتياطية (B). التحديث بيتكتب في النسخة الغير شغالة بس، وبعد ما نتأكد
> إنه سليم 100% بنخلي البووتلودر (bootloader) يشغّله. لو فشل، بيرجع
> تلقائي للنسخة القديمة اللي لسه سليمة.

---

## 🗺️ 3. Flash Partitioning

512 KB, sector-by-sector (STM32F446RE sector map, confirmed against
[`STM32F446RETX_FLASH.ld`](../STM32F446RETX_FLASH.ld)):

| Region | Sectors | Size | Address range | Purpose |
|---|---|---|---|---|
| **Bootloader** | 0–1 | 32 KB | `0x08000000`–`0x08007FFF` | Resident bootloader — always runs first on reset |
| **Metadata** | 2 | 16 KB | `0x08008000`–`0x0800BFFF` | Append-log: active slot, boot-pending flag, per-slot version/size/CRC |
| **App Slot A** | 3–5 | 208 KB | `0x0800C000`–`0x0803FFFF` | One full application image |
| **App Slot B** | 6–7 | 256 KB | `0x08040000`–`0x0807FFFF` | The other full application image |
| | | **512 KB total** | | Fully accounted for, no gaps, no overlap |

Slots A and B are **deliberately different sizes** (208 KB vs 256 KB) —
symmetry isn't a real requirement, both only need to comfortably fit the
largest application build that will ever exist. The binding constraint is
the **smaller** slot (208 KB); before committing to this map, build the
current firmware once and check its `.bin` size with
`arm-none-eabi-size` — don't take a guess on faith.

### Metadata record (lives in Sector 2, one record per state change)

```c
typedef struct __attribute__((packed)) {
    uint32_t magic;           // 0x56325846  = "V2XF"
    uint32_t seq;              // increases by 1 every record — newest wins
    uint8_t  active_slot;      // 0 = A, 1 = B
    uint8_t  boot_pending;     // 1 = jumped here, not yet confirmed alive
    uint8_t  boot_attempts;    // consecutive unconfirmed boots of active_slot
    uint8_t  _reserved0;
    struct {
        uint32_t build_no;     // monotonically increasing build counter
        uint32_t payload_len;  // real byte length of the app image
        uint32_t payload_crc32;
        uint8_t  board_id;     // rejects an image built for the wrong board
        uint8_t  valid;        // 1 once the bootloader CRC-verified this slot
        uint8_t  _reserved1[2];
    } slot[2];                 // slot[0] = A, slot[1] = B
    uint32_t record_crc32;     // CRC over everything above
} FOTA_Metadata_t;              // ~56 bytes, stored as a fixed 64-byte record
```

Sector 2 is used as an **append log** (new record written to the next free
64-byte slot, never overwritten in place) so a power loss mid-write just
leaves the previous — still CRC-valid — record as the answer. When the
16 KB sector fills (~256 records, i.e. thousands of update/boot cycles),
the bootloader compacts it: copy the newest valid record out, erase the
sector, write it back as record 0.

### Boot decision, every single reset

1. Read the newest CRC-valid metadata record.
2. If `boot_pending == 0` → jump straight to `slot[active_slot]` (this is
   the path taken on >99% of resets — fast, no flash writes).
3. Before jumping to a slot that was **just updated**, the bootloader first
   writes `boot_pending = 1`, *then* jumps.
4. If `boot_pending` is already `1` on entry (i.e. the last boot never
   confirmed itself): `boot_attempts++`.
   - `== 1` → try once more (tolerates one spurious reset, e.g. someone
     hitting NRST during bring-up).
   - `>= 2` → **roll back**: flip `active_slot` to the other one (still
     fully intact), clear `boot_pending`/`boot_attempts`, jump there.
5. The application, a few seconds after `RTOS_setup()` starts, checks that
   every task's heartbeat slot has advanced at least once (reusing the
   same liveness array [`IWDG_program.c`](../Src/IWDG_program.c) already
   drives), then calls a new `FOTA_MarkBootOK()` which clears
   `boot_pending`. This is a **separate** function from the existing
   `vTask_Watchdog` — the watchdog task's job is safety-critical liveness
   monitoring and should not also carry "confirm this was a good update."

> 🗣️ **بالعربي:** قسمنا الـ 512 كيلوبايت لأربع مناطق: بووتلودر صغير
> (32 كيلو) بيشتغل أول حاجة عند كل تشغيل، منطقة صغيرة لحفظ "بيانات
> الحالة" (مين النسخة الشغالة دلوقتي، هل آخر تحديث نجح ولا لأ)، ونسختين
> كاملتين من التطبيق (A و B). كل مرة الجهاز يشتغل، البووتلودر بيقرر:
> يكمل عادي على النسخة الشغالة، ولا لو فيه تحديث جديد ما أثبتش إنه شغال
> صح، يرجع تلقائي للنسخة القديمة.

---

## 📦 4. The Firmware Package Format (`.fpkg`)

One concrete container format, shipped end-to-end unchanged from Firebase
Storage → Raspberry Pi disk → stripped by the bootloader into the
metadata sector. It is **never** linked into the app image itself — the
app's own vector table must sit at byte 0 of its slot for VTOR relocation
to work, so the header travels *alongside* the raw `.bin`, not glued onto
its front.

### Header layout (20 bytes, packed, little-endian)

| Offset | Field | Type | Meaning |
|---|---|---|---|
| 0 | `magic` | `uint32` | `0x56325846` ("V2XF") — rejects garbage/wrong files fast |
| 4 | `header_ver` | `uint8` | Format version of this header (`1` today) |
| 5 | `board_id` | `uint8` | Target hardware (e.g. `0x01` = Nucleo bench, `0x02` = carrier PCB) |
| 6 | `_reserved` | `uint8[2]` | Padding, zero |
| 8 | `build_no` | `uint32` | Monotonic build counter (not semver — simpler "is this newer?" logic) |
| 12 | `payload_len` | `uint32` | Exact byte length of the app image that follows |
| 16 | `payload_crc32` | `uint32` | CRC32 (standard/zlib polynomial) over the payload bytes |

Followed immediately by `payload_len` bytes of the raw application
`.bin` (whichever slot's linker output — see [§7](#-7-linker-script-changes)).

```text
┌──────────────────────┬─────────────────────────────────────┐
│  20-byte .fpkg header │        raw application .bin          │
│  (magic, build_no,    │        (exactly payload_len bytes)   │
│   payload_len, CRC32) │                                       │
└──────────────────────┴─────────────────────────────────────┘
```

### Building one — `fota_packager.py`

A one-page script, run by the developer (or CI) after every STM32 build:

```python
#!/usr/bin/env python3
"""fota_packager.py — wraps a raw application .bin into a .fpkg."""
import struct, zlib, sys

MAGIC = 0x56325846      # "V2XF"
HEADER_VER = 1

def pack(bin_path: str, build_no: int, board_id: int, out_path: str) -> None:
    payload = open(bin_path, "rb").read()
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    header = struct.pack(
        "<IBBHII I".replace(" ", ""),   # little-endian, matches the table above
        MAGIC, HEADER_VER, board_id, 0,  # magic, header_ver, board_id, reserved(u16)
        build_no, len(payload), crc,
    )
    with open(out_path, "wb") as f:
        f.write(header)
        f.write(payload)
    print(f"{out_path}: build {build_no}, {len(payload)} bytes, crc32=0x{crc:08X}")

if __name__ == "__main__":
    # usage: fota_packager.py app_slotA.bin 42 1 firmware_v42.fpkg
    pack(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4])
```

> ⚠️ Match `struct.pack`'s format string to the **exact** C struct layout
> above (including any compiler padding on the STM32 side — the bootloader
> should read this with explicit byte offsets, not a blind `memcpy` onto a
> possibly-padded C struct, to guarantee both sides agree byte-for-byte).

> 🗣️ **بالعربي:** أي تحديث بيتحول لملف واحد اسمه `.fpkg`: أول 20 بايت
> "هيدر" (رقم تعريف، رقم إصدار البناء، حجم الملف، CRC للتأكد إنه مش
> متلخبط) وبعدها الملف الفعلي (`app.bin`) زي ما هو بالظبط. الهيدر ده
> بيتبعت مع الملف لحد الراسبيري باي، وبعدين البووتلودر بس هو اللي بياخد
> البيانات منه ويحطها في منطقة الـ metadata — مش بيتلحم جوه الفيرموير
> نفسه عشان ما يبوظش مكان الـ vector table.

---

## 🔌 5. Wire Protocol — Raspberry Pi ⇄ STM32 Bootloader

> ⚠️ **Open decision — confirm before wiring anything.** The project has
> two conflicting pin plans for the Raspberry Pi UART link:
> [`Inc/System/System.h`](../Inc/System/System.h) (current firmware) says
> **USART2 (PA2/PA3)** is the RPi link and UART4 (PA0/PA1) is free, while
> [`docs/PCB_BUILD_STAGES.md`](PCB_BUILD_STAGES.md) (the newer carrier-PCB
> design, Stage 8/9) wires the RPi to **UART4 (PA0/PA1)** and reserves
> USART2/PA2-PA3 for the ST-Link virtual COM port (debug only). **This must
> be confirmed against whichever board you're actually testing on before
> any FOTA wiring decision is final.** Everything below refers to "the RPi
> UART" generically for that reason — substitute whichever one is real on
> your bench.

### Frame layout

```text
┌────────┬─────┬────────────┬───────────────────────┬─────────────┬────────┐
│ START  │ CMD │  LEN (LE)  │       PAYLOAD          │  CRC32 (LE) │  END   │
│ 0xAA   │ 1 B │    2 B     │     LEN bytes           │    4 B      │  0x55  │
└────────┴─────┴────────────┴───────────────────────┴─────────────┴────────┘
```

Same structural idea as the existing `Neighbor`-frame parser in
[`DSRC.c`](../Src/DSRC.c) (explicit state machine — `WAIT_START → READ_DATA
→ READ_CRC → READ_END`, any garbled byte just falls back to
`WAIT_START`) — but **CRC32** instead of a single XOR byte (a bad flash
write is a much bigger deal than one dropped telemetry line), and
**request/response** instead of fire-and-forget broadcast, so the
bootloader's UART can be **fully polled** — no interrupts, no NVIC, no
FreeRTOS needed in the bootloader at all (see [§12](#-12-risks--gotchas)).

### Opcodes

| Opcode | Name | Direction | Payload | Meaning |
|---|---|---|---|---|
| `0x01` | `HELLO` | Pi → STM32 | — | "Are you the bootloader and listening?" |
| `0x02` | `HELLO_ACK` | STM32 → Pi | `active_slot(1B), build_no(4B)` | Bootloader confirms + reports current state |
| `0x10` | `START_UPDATE` | Pi → STM32 | the 20-byte `.fpkg` header | Announces the incoming image |
| `0x11` | `START_ACK` / `START_NAK` | STM32 → Pi | `reason(1B)` on NAK | Accepts (erases target slot) or rejects (wrong `board_id`, too big for the slot, etc.) |
| `0x20` | `DATA_CHUNK` | Pi → STM32 | `seq(2B), data(≤256B)` | One chunk of the payload |
| `0x21` | `CHUNK_ACK` / `CHUNK_NAK` | STM32 → Pi | `seq(2B)` | Confirms or requests resend of that sequence number |
| `0x30` | `END_UPDATE` | Pi → STM32 | — | "That was the last chunk." |
| `0x31` | `END_ACK` / `END_NAK` | STM32 → Pi | — | Whole-image CRC32 verified against the header, pass/fail |
| `0x40` | `COMMIT` | Pi → STM32 | — | Only sent after an explicit `END_ACK` — "make it permanent" |
| `0x41` | `COMMIT_ACK` | STM32 → Pi | — | Metadata updated (`active_slot` flipped, `boot_pending=1`); STM32 resets right after |

`CRC verify` and `commit` are **two separate steps on purpose** — the
bootloader never boots a slot it hasn't been explicitly told to commit,
even if the CRC passed, so a Raspberry Pi that crashes right after
`END_ACK` simply leaves the STM32 sitting on its still-good old slot
until the Pi comes back and finishes the handshake.

### Sequence

```text
Pi                                   STM32 bootloader
│──── HELLO ─────────────────────────►│
│◄──── HELLO_ACK (slot, build) ───────│
│──── START_UPDATE (fpkg header) ────►│  (bootloader erases target slot,
│◄──── START_ACK ──────────────────── │   kicking IWDG between sectors)
│──── DATA_CHUNK #0 ──────────────────►│
│◄──── CHUNK_ACK #0 ───────────────────│
│──── DATA_CHUNK #1 ──────────────────►│
│◄──── CHUNK_NAK #1 ────────────────── │  (bad CRC — resend #1)
│──── DATA_CHUNK #1 (retry) ──────────►│
│◄──── CHUNK_ACK #1 ────────────────── │
│              ...  ~256-byte chunks, one ACK/NAK round-trip each  ...
│──── END_UPDATE ─────────────────────►│  (whole-image CRC32 check)
│◄──── END_ACK ──────────────────────── │
│──── COMMIT ─────────────────────────►│
│◄──── COMMIT_ACK ────────────────────── │  (STM32 resets itself right after)
```

- **Chunk size: 256 bytes** — small enough that a NAK-triggered resend is
  cheap, large enough to keep framing overhead low (~800-1000 chunks for a
  ~220 KB image).
- **Timeout/resend rule**: if the Pi doesn't get an ACK/NAK within (say)
  500 ms of a chunk, treat it as a NAK and resend the same sequence
  number — never advance without a positive ACK.
- **Entering update mode**: the *running application* — not a hardware
  pin — triggers this. On receiving an "update available" command over
  its own normal protocol, it writes `update_requested=1` to metadata and
  performs a software reset (`SCB->AIRCR` `SYSRESETREQ`, via the existing
  hand-written SCB driver). The bootloader sees the flag on the next boot
  and stays resident waiting for `HELLO` instead of auto-jumping. No new
  BOOT0 GPIO wiring from the Pi is needed.
- **Recovery fallback**: regardless of the flag, the bootloader always
  waits a short fixed window (~500 ms–1 s) after *every* reset for the
  `HELLO` handshake before giving up and auto-jumping — this is the manual
  recovery path if the app is fully unresponsive and can't set the flag
  itself (connect a terminal/script, reset the board, send `HELLO` inside
  the window).

> 🗣️ **بالعربي:** الراسبيري باي والبووتلودر بيتكلموا ببروتوكول بسيط:
> "أهلاً، إنت جاهز؟" → "أيوه، ده رقم النسخة الحالية" → "هبعتلك تحديث،
> ده حجمه وبصمته" → "تمام، مسحت المكان، ابعت" → إرسال الملف على شكل
> "قطع" كل واحدة 256 بايت مع تأكيد (ACK) لكل قطعة، ولو فيه قطعة غلط
> بيطلب إعادة إرسالها بس. آخر خطوة "COMMIT" منفصلة تمامًا عن التأكد من
> الـ CRC — يعني حتى لو كل حاجة صحت، البووتلودر مش هيشغل النسخة الجديدة
> غير لو استلم أمر "COMMIT" صريح.

---

## 🛠️ 6. New STM32 Driver Code Needed

Following the existing MCAL layering
(`_interface.h` / `_private.h` / `_config.h` / `_program.c`, see the
[USART driver](../Inc/Drivers/MCAL/USART/USART_intreface.h) for the
pattern) and the register-level, magic-key-gated style already used by
[`IWDG_program.c`](../Src/IWDG_program.c):

### `Inc/Drivers/MCAL/FLASH/` + `Src/FLASH_program.c` (new)

```c
ErrorState_t FLASH_enumUnlock(void);
ErrorState_t FLASH_enumLock(void);
ErrorState_t FLASH_enumEraseSector(uint8_t sector_num);
ErrorState_t FLASH_enumProgramWord(uint32_t addr, uint32_t data);
ErrorState_t FLASH_enumProgramBuffer(uint32_t addr, const uint8_t *buf, uint32_t len);
ErrorState_t FLASH_enumWaitBusy(void);
```

Register-level operations needed (no ST HAL, matching the rest of this
codebase):

- **Unlock sequence** — `FLASH->KEYR = 0x45670123; FLASH->KEYR = 0xCDEF89AB;`
  — structurally identical to `IWDG_KEY_ENABLE/REFRESH/START`, a good
  callout in the new driver's Doxygen header.
- **Sector erase** — `FLASH->CR.SER=1`, `FLASH->CR.SNB[3:0]=sector_num`,
  `STRT=1`, poll `FLASH->SR.BSY` until clear, then clear `SER`.
- **Programming** — `FLASH->CR.PG=1`, `PSIZE=0b10` (x32/word programming —
  valid for this 3.3 V design), write the destination word, poll `BSY`,
  clear `PG`.
- **Error checks** — after every operation, check `FLASH->SR` for
  `WRPERR`/`PGAERR`/`PGPERR`/`PGSERR`, matching the defensiveness already
  present throughout this codebase's drivers.
- Always re-lock (`FLASH->CR.LOCK=1`) at the end of every public call
  rather than trusting caller discipline.

### CRC32 utility (software, not a new peripheral driver)

A small table-based CRC32 routine — **bit-for-bit identical to Python's
`zlib.crc32`** (standard reflected polynomial) — living wherever the
project's non-peripheral utility code sits
([`Inc/Drivers/LIB/`](../Inc/Drivers/LIB)). This is deliberately **not**
the STM32's hardware CRC peripheral: that unit uses a fixed,
non-reflected polynomial convention that does not match Python's
`zlib.crc32` without extra bit-reversal gymnastics on both sides — a
software CRC32 over a ~220 KB image costs tens of milliseconds at 16 MHz,
irrelevant next to the multi-second UART transfer it's checking, and
removes an entire class of "why do the two sides disagree" bugs. The
hardware CRC peripheral is a fine *optional* stretch-goal driver later,
never a FOTA dependency.

### `SCB_vSetVectorTable()` (new function on the existing SCB driver)

```c
// Inc/Drivers/MCAL/SCB/SCB_interface.h
ErrorState_t SCB_vSetVectorTable(uint32_t vtor_addr); // writes SCB->VTOR (0xE000ED08)
```

Used once, right before the bootloader jumps to an app slot (see the jump
sequence in [§7](#-7-linker-script-changes)).

### Explicitly out of scope: code signing

CRC32 + A/B rollback only — **no HMAC/signature**. The threat model here
is transmission corruption and developer mistakes on a single, physically
owned prototype, delivered over a *wired* link from a Raspberry Pi that
already has unmediated authority over the vehicle today (it drives the
motors). Adding asymmetric signing means a crypto library and private-key
custody for a threat that doesn't exist in this deployment — a real,
consciously-made scope cut, worth stating plainly rather than leaving
silently absent.

> 🗣️ **بالعربي:** محتاجين نكتب driver جديد للفلاش (فتح القفل، مسح
> sector، كتابة) بنفس أسلوب الأكواد الموجودة عندكم بالفعل (زي
> `IWDG_program.c`)، ودالة CRC32 بالسوفت وير (مش الهاردوير CRC اللي في
> الشريحة، عشان مش هيتطابق بسهولة مع اللي هيتحسب في بايثون على الراسبيري
> باي)، ودالة واحدة جديدة تضاف لملف SCB الموجود لتحريك الـ vector table.
> وقررنا **مانعملش توقيع رقمي (code signing)** للتحديث — الـ CRC32 +
> نظام الرجوع للنسخة القديمة كافيين لمشروع تخرج زي ده.

---

## 📐 7. Linker Script Changes

### New bootloader linker script — `Bootloader_FLASH.ld`

```ld
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 32K
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}
```

The bootloader can safely claim the **entire** 128 KB RAM region — there's
no leftover-RAM-state hazard, because the app's own `Reset_Handler` (in
[`startup_stm32f446retx.s`](../Startup/startup_stm32f446retx.s))
**unconditionally** re-copies `.data` from flash and zero-fills `.bss` on
every entry, no matter what the bootloader left behind. This is a
verified property of the existing startup code, not an assumption.

### Application — two build configurations, not one

Since Slot A (`0x0800C000`) and Slot B (`0x08040000`) have different base
addresses, and one static `.ld` file only encodes one `FLASH ORIGIN`, the
same application source is built **twice**:

```
STM32F446RETX_FLASH_SlotA.ld    FLASH ORIGIN = 0x0800C000   LENGTH = 208K
STM32F446RETX_FLASH_SlotB.ld    FLASH ORIGIN = 0x08040000   LENGTH = 256K
```

This is mechanical, not novel — the project already has separate
Debug/Release CubeIDE configurations to clone the pattern from. (Building
once and relocating post-link would need true position-independent code,
which is a much bigger lift than a second linker script + build config —
not worth it here.)

`.isr_vector` still lands at byte 0 of `FLASH` in both configs (now the
slot's own start address, not `0x08000000`) — exactly what makes the jump
sequence below line up correctly. `_estack` (`ORIGIN(RAM)+LENGTH(RAM)`) is
unchanged either way.

### Bootloader → application handoff sequence

```c
void Bootloader_JumpToApp(uint32_t app_base)
{
    __disable_irq();
    SCB_vSetVectorTable(app_base);                  // SCB->VTOR
    uint32_t msp    = *(uint32_t *)(app_base + 0);   // word 0: app's initial MSP
    uint32_t reset  = *(uint32_t *)(app_base + 4);   // word 1: app's Reset_Handler
    __set_MSP(msp);
    ((void (*)(void))reset)();                        // never returns
}
```

The bootloader never touches `SCB->AIRCR` priority grouping — the app
already sets this itself unconditionally on startup, so there's nothing
for the bootloader to get wrong by omission. It also doesn't need to
worry about system clock state: [`System_setup()`](../Src/System.c)
unconditionally forces `RCC_HSI_CLK` (16 MHz, no PLL) on every boot
regardless of prior state, so the bootloader is free to also just run on
default HSI.

> 🗣️ **بالعربي:** لازم نعمل linker script منفصل للبووتلودر، واتنين
> للتطبيق نفسه (نسخة لمكان A ونسخة لمكان B) لأن كل واحدة عندها عنوان
> بداية مختلف في الفلاش. البووتلودر لما يقرر يشغل نسخة معينة، بيعمل
> "قفزة" برمجية ليها: يوجه الـ vector table للمكان الصح، يظبط الـ stack
> pointer، وبعدين ينده على الـ Reset_Handler بتاع التطبيق وكأنه شغال
> جديد من الصفر.

---

## ☁️ 8. Cloud Distribution — How the Update Reaches the Raspberry Pi

This is the piece that sits **before** everything above: how does a new
`.fpkg` get from the developer's laptop onto the car's Raspberry Pi in
the first place, over the internet? Documented concretely with
**Firebase**, plus a named zero-new-infrastructure alternative using the
MQTT broker the project already runs.

### Chosen backend: Firebase (Cloud Storage + Firestore)

```text
Developer PC
   │  1. build + fota_packager.py → firmware_v42.fpkg
   ▼
Firebase Cloud Storage                    Firestore
   bucket: v2x-fota/                      doc: firmware_releases/latest
   firmware/{board_id}/42.fpkg    ◄────►  { build_no: 42,
                                             storage_path: "firmware/1/42.fpkg",
                                             crc32: "0x9F2A1B7C",
                                             released_at: "...",
                                             mandatory: false }
   │  HTTPS download                          │  Firestore read
   ▼                                          ▼
RPI/FOTA/fota_agent.py  ── polls Firestore, compares build_no to what
                            the STM32 currently reports over telemetry
```

- **Firebase Cloud Storage** holds the actual `.fpkg` bytes, under a
  simple path convention: `firmware/{board_id}/{build_no}.fpkg`.
- **Firestore** holds one small pointer document
  (`firmware_releases/latest`) with `{build_no, storage_path, crc32,
  released_at, notes, mandatory}`. The Raspberry Pi **never trusts the
  Storage object by itself** — it always cross-checks the downloaded
  `.fpkg`'s own CRC32 (recomputed locally) against the value recorded in
  this Firestore document *before* going anywhere near the STM32.
- **"Is this actually newer?"** — the agent compares Firestore's
  `build_no` against the STM32's currently-running `build_no` (already
  flowing to the Raspberry Pi over the existing telemetry link, once the
  metadata scheme in [§3](#-3-flash-partitioning) exists) — a strictly
  greater value is the only trigger for a download.
- **Auth** — a Firebase **service-account key** (JSON), used from the
  Raspberry Pi via the `firebase-admin` Python SDK (or a plain HTTPS REST
  call with a short-lived token). **Store this key as a git-ignored
  file or environment variable, never commit it** — this project already
  has one documented credential leak (the MQTT password in
  `RPI/V2N/Car_client.py`, flagged in `AI_TECHNICAL_AUDIT.md`); the
  Firebase key must not become a second instance of the same mistake.
- **Trigger model** — the Raspberry Pi **polls** Firestore on a modest
  interval (every few minutes), or on-demand from a "Check for updates"
  button on the DashBoard — not a persistent push connection. Simpler and
  reliable for a prototype vehicle that isn't always powered on.

### Named alternative: reuse the existing HiveMQ MQTT broker

The project already runs a live MQTT connection to HiveMQ Cloud
([`RPI/V2N/Car_client.py`](../../RPI/V2N/Car_client.py)). A retained
message on a new `firmware/announce` topic on that **same** broker —
`{build_no, download_url, crc32}` — is a valid substitute for the
Firestore pointer step, with **zero new external services** to stand up.
The one thing it doesn't replace: the `.fpkg` bytes themselves still need
to live somewhere HTTP-reachable for the `download_url` to point at
(Firebase Storage still works fine for just that piece, or any plain
HTTPS file host). Pick whichever backend the team already has an account
for and is comfortable operating — the rest of this document (package
format, wire protocol, bootloader) is identical either way.

> 🗣️ **بالعربي:** التحديث بيتنزل على السحابة (Firebase Storage) وبنسجل
> "آخر إصدار متاح" في قاعدة بيانات Firestore (رقم الإصدار، رابط الملف،
> الـ CRC بتاعه). الراسبيري باي بيسأل Firestore كل شوية "فيه تحديث جديد
> ولا لأ؟" (أو لما تدوس زرار "تحقق من التحديثات" في الداشبورد)، ولو لقى
> رقم إصدار أحدث من اللي شغال دلوقتي، بينزل الملف، يتأكد من الـ CRC بتاعه
> بنفسه، وبعدين يبدأ يتكلم مع الـ STM32 زي الموضح فوق. البديل الجاهز
> عندنا فعلاً هو نفس سيرفر الـ MQTT (HiveMQ) اللي المشروع بيستخدمه بالفعل
> لبيانات الإشارة الذكية — ممكن نستخدمه بدل Firestore من غير ما نضيف
> خدمة جديدة، لكن لازم لسه مكان نحط فيه ملف التحديث نفسه.

---

## 🖥️ 9. Raspberry Pi Side — `RPI/FOTA/`

Follows the existing [`RPI/hub`](../../RPI/hub) pub/sub +
[`RPI/systemd`](../../RPI/systemd) service pattern already used by every
other RPi subsystem.

```text
RPI/FOTA/
├── fota_agent.py       # long-running service — hub client (ipc_node.py pattern)
├── fota_packager.py    # dev/CI tool — see §4
├── README.md
RPI/systemd/v2x-fota.service
```

- **`fota_agent.py`** registers with the hub like every other RPi process,
  owns *both* halves of the pipeline: the Firebase fetch (§8) and the
  UART push (§5). Publishes progress to a `fota/state` topic —
  `checking` → `downloading` → `updating` → `done`/`failed`, each carrying
  the build numbers involved — for `dashboard_bridge.py` to forward into
  `data.json`, exactly like the existing `v2n_frame`/`v2p_frame` pattern.
- **Manual trigger, automatic check** — checking Firestore for a newer
  version can run unattended on a timer; **actually installing** one
  requires a human tap on a "Firmware Update" panel in the DashBoard/
  Control UI (upload confirmation or an "Install" button once a new
  version is detected) — deliberate, human-in-the-loop for a
  safety-adjacent device, not silent background self-flashing.
- **UART port sharing** — once the RPi UART is also used for FOTA, it's
  shared with the existing telemetry reader
  ([`DashBoard/server.py`](../../RPI/DashBoard/server.py)). Before a
  transfer starts, `fota_agent.py` asks (over the existing hub) for
  `server.py` to release the port, and hands it back afterward — one more
  hub message on the existing pub/sub pattern, no new transport needed.
- **Independent verification** — the agent recomputes CRC32 over the
  downloaded payload itself and compares it to the Firestore-recorded
  value **before** it ever opens the serial port — a bad/corrupt download
  is caught without ever touching the STM32.

> 🗣️ **بالعربي:** هنضيف مجلد جديد `RPI/FOTA/` فيه سكريبت (`fota_agent.py`)
> بيشتغل كخدمة دايمة زي باقي خدمات الراسبيري باي الموجودة، بيتكلم مع
> نفس نظام الـ pub/sub الموجود. هو المسؤول عن تنزيل التحديث من Firebase
> والتحقق منه، وبعدين إرساله للـ STM32. أي تحديث محتاج ضغطة تأكيد من
> المستخدم على الداشبورد قبل ما يتنفذ فعليًا — مفيش تحديث تلقائي بالكامل
> من غير علم حد، لأن الجهاز ده بيتحكم في عربية فعلية.

---

## 🔁 10. End-to-End Flow

1. Developer builds the application **twice** (Slot A config, Slot B
   config).
2. `fota_packager.py` wraps whichever `.bin` targets the currently
   *inactive* slot (the agent reports which one that is) into a `.fpkg`
   (§4).
3. Developer publishes the `.fpkg` to Firebase Storage and updates the
   Firestore `firmware_releases/latest` pointer (§8).
4. `fota_agent.py` (polling, or human-triggered "Check for updates")
   notices a newer `build_no` than what the STM32 currently reports.
5. Agent downloads the `.fpkg`, independently re-verifies its CRC32
   against the Firestore-recorded value.
6. Human confirms "Install" on the DashBoard (optional gate, per §9).
7. Agent asks `server.py` (over the hub) to release the shared UART port.
8. Agent sends the running app an "update available" command over its
   existing protocol; app writes `update_requested=1` to metadata and
   performs a software reset.
9. Bootloader boots, sees the flag, stays resident, waits for `HELLO`.
10. `HELLO` → `HELLO_ACK` (bootloader reports inactive slot + current
    build).
11. `START_UPDATE` (the `.fpkg` header) → bootloader erases the target
    slot's sectors, kicking the IWDG between every individual sector
    erase → `START_ACK`.
12. `DATA_CHUNK` × N, ACK/NAK-and-resend per chunk, streamed into the
    freshly erased slot.
13. `END_UPDATE` → bootloader computes CRC32 over the full assembled
    payload, compares to the header → `END_ACK`/`END_NAK`.
14. On `END_ACK`: agent sends `COMMIT` → bootloader writes
    `slot[target].valid=1`, `active_slot=target`, `boot_pending=1`,
    resets.
15. Bootloader (next boot) jumps to the new slot (§7's handoff sequence).
16. New app boots; once its heartbeat array shows every task alive,
    `FOTA_MarkBootOK()` clears `boot_pending`.
17. If step 16 never happens (crash/hang/hard fault/IWDG reset before
    confirmation): next reset, bootloader sees `boot_pending` still set,
    retries once, then automatically reverts `active_slot` — **no
    Raspberry Pi or human involvement required for rollback.**

> 🗣️ **بالعربي:** ده تسلسل الخطوات كاملة من أول ما المطور يبني نسخة
> جديدة لحد ما تشتغل فعليًا على العربية: بناء → تغليف (.fpkg) → رفع
> على Firebase → الراسبيري باي يكتشفه ويحمله ويتأكد منه → تأكيد المستخدم
> → إرسال أمر للـ STM32 يدخل وضع التحديث → تبادل البيانات بالبروتوكول
> في §5 → تأكيد (COMMIT) → إعادة تشغيل → تشغيل النسخة الجديدة → تأكيد
> ذاتي إنها شغالة صح. لو أي خطوة من الأخيرة دي فشلت، البووتلودر بيرجع
> تلقائيًا للنسخة القديمة.

---

## 🚧 11. Staged Roadmap

Build and validate incrementally — each stage should be fully working and
bench-tested before the next one starts.

| Stage | Scope | Key risk | Rough effort |
|---|---|---|---|
| **1** | FLASH + software CRC32 drivers, standalone bench test: erase a **scratch** sector the running app never uses, program a known pattern, read back, CRC-check. No bootloader yet. | First-time unlock/erase/program register sequence — test only against scratch sectors while learning, so a mistake can't brick the board. | 2–3 days |
| **2** | Minimal 2-stage bootloader that unconditionally jumps to a fixed app address. No A/B, no CRC gate, no metadata yet — pure proof that the VTOR/MSP/PC jump mechanics work. | Conceptually the trickiest stage despite its tiny code size — everything downstream depends on it. Highest-value stage to get right early. | 2–4 days |
| **3** | UART chunked-receive-and-flash, no Raspberry Pi involved yet — driven by a PC Python script over USB-UART (bench convenience — can use whichever UART is confirmed free right now, even if it isn't the final one). | First real erase-during-operation timing measurements; protocol state-machine correctness. | 4–6 days |
| **4** | A/B slots + metadata log + rollback + watchdog-tied `FOTA_MarkBootOK()`. | Most subtle correctness logic. **Budget real bench time for deliberate fault injection** — pull power / hit reset mid-transfer, mid-erase, mid-commit — and confirm no brick every time, not just the happy path. | 4–7 days |
| **5** | `RPI/FOTA/` agent: Firebase project setup (bucket, Firestore doc, service account) + systemd service + hub integration + UART port-sharing handshake with `server.py`. | Firebase setup can be built and tested completely independently of any STM32 work. Mostly Python plumbing — every pattern needed already exists in the repo to copy from (`ipc_node.py`, `dashboard_bridge.py`, `systemd/*.service`). | 3–5 days |
| **6** | DashBoard UI hook: progress bar, state text, "Check for updates"/"Install" buttons. | Small, additive. | 1–2 days |

Rough total: **3–4 focused weeks** for a small team already familiar with
this codebase.

> 🗣️ **بالعربي:** الجدول ده بيقسم الشغل لـ 6 مراحل، كل واحدة تُبنى
> وتُختبر لوحدها الأول: (1) اختبار الفلاش على منطقة فاضية، (2) بووتلودر
> بسيط بيقفز بس، (3) استقبال ملف عن طريق UART من كمبيوتر عادي (مش
> راسبيري باي لسه)، (4) نظام A/B الكامل مع اختبار "قطع الكهرباء عمدًا"
> للتأكد إن مفيش بريكينج, (5) خدمة الراسبيري باي و Firebase، (6) واجهة
> الداشبورد. المدة التقريبية ٣-٤ أسابيع لفريق طلاب متعود على الكود ده.

---

## ⚠️ 12. Risks & Gotchas

- **No redundancy for the bootloader itself.** Sectors 0-1 are the one
  thing this A/B design does *not* protect — there's only one bootloader
  copy. Accepted trade-off: a corrupted bootloader is recovered on the
  bench via SWD/ST-Link, which is always available during development;
  making the bootloader self-updating is a materially harder problem
  (you can brick your own updater) with no real payoff here.

- **Watchdog vs. multi-sector erase — use real numbers.** The app's
  *configured* IWDG timeout today is **2000 ms** (`WDG_TIMEOUT_MS` in
  [`main.c`](../Src/main.c)), not the peripheral's theoretical 8.19 s
  ceiling — this figure is irrelevant to the bootloader anyway, since the
  bootloader runs *before* the app (re)arms IWDG and can call
  `IWDG_voidInit()` with its own longer value for its own lifetime (the
  driver's unlock/PR/RLR sequence can be re-run later by the app to
  shorten it back). Erasing Slot A means up to 3 sector-erase calls back
  to back (sectors 3, 4, 5); **verify actual worst-case erase time per
  sector against the STM32F446 datasheet's flash timing table** rather
  than trust an approximate figure. Structural fix regardless of the
  exact number: **kick the watchdog after every individual sector erase**,
  not once before the whole multi-sector loop — this turns a
  multi-second aggregate operation into a series of individually-bounded,
  always-safe sub-operations.

- **The bootloader should not run FreeRTOS.** Bare superloop/polled, no
  RTOS. Its job (wait briefly, do a blocking UART transfer, jump) has no
  real concurrency need, and pulling in FreeRTOS adds link size and a
  second NVIC-priority correctness surface for zero benefit. Using the
  existing MCAL's **polled** `USART_Init`/`USART_enumTransmit`/
  `USART_enumReceive` (not `USART_InitIT`) means the bootloader arms
  **zero NVIC interrupts** — nothing needs disabling before the jump
  because nothing was ever armed.

- **RAM state and clock state at handoff are non-issues** (verified, not
  assumed) — see [§7](#-7-linker-script-changes).

- **UART1 (ESP32) never needs to be touched during a bootloader session.**
  The bootloader only initializes the one UART it actually uses; USART1's
  pins are simply never configured by it, so the ESP32/V2V link is
  naturally quiescent during an update and comes back up normally as part
  of the new app's own `System_setup()` after a successful boot.

- **The USART2-vs-UART4 pin-plan discrepancy is a hard blocker**, repeated
  here deliberately — confirm which document matches the board you're
  actually testing FOTA on before wiring anything (see the callout at the
  top of [§5](#-5-wire-protocol--raspberry-pi--stm32-bootloader)).

- **Firebase service-account key handling.** Same credential-leak class as
  the project's existing MQTT-password issue (`AI_TECHNICAL_AUDIT.md`) —
  don't repeat it. Git-ignored file or environment variable, never
  committed.

- **The Raspberry Pi losing internet mid-download must never touch the
  STM32.** The two phases — "fetch and verify locally" and "push to the
  STM32" — are strictly sequential and never interleaved; a failed/partial
  Firebase download simply means `fota_agent.py` retries later, with the
  STM32 completely uninvolved and unaware anything happened.

> 🗣️ **بالعربي:** أهم نقط الخطر: (1) البووتلودر نفسه معندوش نسخة
> احتياطية — لو اتبوظ محتاجين نوصل بـ ST-Link يدويًا (مقبول لمشروع
> تخرج بييجي معاه وصول فيزيائي دايمًا)، (2) لازم نطفي/نشعل الـ watchdog
> بعد كل عملية مسح sector لوحدها مش بعد كل العمليات مرة واحدة، (3)
> البووتلودر ميستخدمش FreeRTOS ولا أي مقاطعات (interrupts) خالص عشان
> نقلل نقاط الفشل، (4) لازم تتأكدوا فعليًا من الـ UART الصح قبل ما توصلوا
> أي حاجة، (5) مفتاح Firebase لازم يتخزن بشكل آمن زي أي باسورد تاني في
> المشروع.

---

## 📁 Where This Fits in the Repo (once implemented)

```text
V2V-STM32/
├── Bootloader/                    # NEW — separate CubeIDE project or build target
│   ├── Src/Bootloader_main.c
│   ├── Bootloader_FLASH.ld
│   └── ...
├── Inc/Drivers/MCAL/FLASH/        # NEW — FLASH driver (§6)
├── Src/FLASH_program.c            # NEW
├── Inc/Drivers/LIB/CRC32.h        # NEW — software CRC32 utility (§6)
├── Src/CRC32.c                    # NEW
├── STM32F446RETX_FLASH_SlotA.ld   # NEW — replaces the single .ld (§7)
├── STM32F446RETX_FLASH_SlotB.ld   # NEW
└── docs/
    └── FOTA.md                    # this file

RPI/
└── FOTA/                          # NEW (§9)
    ├── fota_agent.py
    ├── fota_packager.py
    └── README.md
```

---

## 🏁 Summary

FOTA on this project is three independently-buildable pieces glued
together: **Firebase → Raspberry Pi** (internet delivery, §8),
**Raspberry Pi → STM32 bootloader** (a small framed binary protocol over
a wired UART, §4–§5), and **the STM32 bootloader itself** (A/B flash
slots + CRC verification + automatic rollback tied to the existing
watchdog liveness mechanism, §2–§3, §6–§7). Nothing here touches the
real-time V2V (ESP-NOW) path, and every stage in the roadmap (§11) is
independently bench-testable before the next one starts — the design
favors "genuinely hard to brick" over maximal sophistication, on purpose,
for a student-built graduation project.

---
