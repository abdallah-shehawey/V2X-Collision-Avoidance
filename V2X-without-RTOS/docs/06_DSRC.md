# DSRC — The Communication Layer

> **Files:** [`Src/DSRC.c`](../Src/DSRC.c) · [`Inc/DSRC.h`](../Inc/DSRC.h)

DSRC (Dedicated Short-Range Communications) is the layer that **sends and receives** the V2V messages. Every safety module sits on top of it: they read the neighbor table DSRC builds, and they hand DSRC their flags to broadcast. In this project the physical link is a **UART (USART1)** — DSRC is the framing/parsing/table-management layer on top of it.

---

## 1. The Message: the `Neighbor` struct

Every car broadcasts one `Neighbor` struct describing itself. The same struct type is used to store received neighbors in the table.

```c
typedef struct {
  uint8_t  vehicle_id;
  float    speed;            // m/s
  float    heading;          // 0..360 degrees
  uint32_t last_update;
  uint8_t  fcw_headon_flag;  // head-on candidate: 0/1
  uint8_t  bsw_flag;         // sender's front side(s): bit0=LEFT, bit1=RIGHT (0/1/2/3)
  uint8_t  ima_flag;         // 0=Safe, 1=Warning, 2=Critical
} Neighbor;
```

A single message carries the car's **state** (speed, heading) plus the **cooperative
flags** other cars act on. Only the flags that another car needs are broadcast: FCW's
head-on candidate, BSW's sender side, and IMA's level. Purely local results (front
collision, EEBL, DNPW, blind-spot) stay on the car that computed them.

---

## 2. The Wire Frame (how a packet looks on the UART)

```text
┌────────────┬──────────────────────┬────────────┬──────────┐
│ START_BYTE │   Neighbor (raw)     │  CHECKSUM  │ END_BYTE │
│   0xAA     │   sizeof(Neighbor)   │  1 byte    │  0x55    │
└────────────┴──────────────────────┴────────────┴──────────┘
```

```c
#define START_BYTE 0xAA
#define END_BYTE   0x55
#define PACKET_SIZE (1 + sizeof(Neighbor) + 1 + 1)
```

- **START_BYTE / END_BYTE:** framing markers so the receiver knows where a packet begins and ends.
- **CHECKSUM:** a 1-byte XOR over all the data bytes — a cheap integrity check.

The struct is sent **as raw bytes** (`memcpy` of its memory). That works as long as both ends use the same MCU/compiler/struct layout (same padding and endianness). On a heterogeneous fleet you'd switch to explicit serialization — flagged as a portability note.

---

## 3. The Checksum

```c
static uint8_t calc_checksum(uint8_t *data, uint8_t len) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < len; i++)
    sum ^= data[i];     // XOR of all bytes
  return sum;
}
```

A simple XOR checksum. It catches single-bit and many burst errors cheaply; it is **not** a strong CRC. Fine for a short-range prototype link.

---

## 4. Sending: `DSRC_SendNeighbor()`

```c
void DSRC_SendNeighbor(Neighbor *n) {
  uint8_t raw[sizeof(Neighbor)];
  memcpy(raw, n, sizeof(Neighbor));
  uint8_t chk = calc_checksum(raw, sizeof(Neighbor));

  USART_enumTransmit(&USART_1, START_BYTE);
  for (uint8_t i = 0; i < sizeof(Neighbor); i++)
    USART_enumTransmit(&USART_1, raw[i]);
  USART_enumTransmit(&USART_1, chk);
  USART_enumTransmit(&USART_1, END_BYTE);
}
```

Straightforward: frame the struct between START/END with a checksum and push each byte out the UART. This is called once per super-loop iteration in `main()`.

---

## 5. Receiving: the Interrupt-Driven State Machine

Reception never blocks the main loop. Each incoming byte fires the USART RX interrupt → `USART_RXCMP()` (in `System.c`) → `DSRC_RxCallback(byte)`. That callback is a **4-state parser**:

```text
WAIT_START ──(byte==0xAA)──► READ_DATA ──(got sizeof(Neighbor) bytes)──►
READ_CHECKSUM ──(1 byte)──► READ_END ──(byte==0x55 & checksum ok)──► queue_push()
        └────────────────────── any path resets to WAIT_START ──────────────────────┘
```

```c
void DSRC_RxCallback(uint8_t byte) {
  switch (parse_state) {
  case WAIT_START:
    if (byte == START_BYTE) { rx_idx = 0; parse_state = READ_DATA; }
    break;
  case READ_DATA:
    rx_buf[rx_idx++] = byte;
    if (rx_idx >= sizeof(Neighbor)) parse_state = READ_CHECKSUM;
    break;
  case READ_CHECKSUM:
    rx_checksum = byte; parse_state = READ_END;
    break;
  case READ_END:
    if (byte == END_BYTE) {
      if (calc_checksum(rx_buf, sizeof(Neighbor)) == rx_checksum) {
        Neighbor n;
        memcpy(&n, rx_buf, sizeof(Neighbor));
        queue_push(&n);          // valid packet → enqueue
      }
    }
    parse_state = WAIT_START;     // always resync after a full frame
    break;
  }
}
```

> **Why a state machine + queue?** The ISR must be short and must not touch the neighbor table directly (that table is read by the main loop — touching it from an ISR would be a race). So the ISR only parses bytes and drops finished packets into a queue. The main loop drains the queue.

---

## 6. The RX Queue (ISR → main-loop handoff)

A small **ring buffer** decouples the interrupt from the main loop:

```c
static Neighbor rx_queue[QUEUE_SIZE];          // QUEUE_SIZE = 10
static volatile uint8_t queue_head = 0;        // volatile: shared with ISR
static volatile uint8_t queue_tail = 0;
```

- **`queue_push()`** (called from the ISR): adds a packet if not full; silently drops it if full (overflow protection).
- **`queue_pop()`** (called from the main loop): removes the oldest packet; resets head/tail to 0 when empty.

`head`/`tail` are `volatile` because they are written in the ISR and read in the main loop.

---

## 7. The Neighbor Table

```c
static Neighbor neighbor_table[MAX_NEIGHBORS];  // MAX_NEIGHBORS = 20
static uint8_t  neighbor_count = 0;
```

`update_neighbor()` (called from `DSRC_Update`) has three cases:

1. **Already in the table** (same `vehicle_id`) → overwrite it with the fresh data.
2. **Not present and table not full** → append it.
3. **Table full** → replace the **oldest** entry (smallest `last_update`).

This keeps one row per vehicle and gracefully handles more than 20 cars.

---

## 8. Public API & the Main-Loop Flow

| Function | Role |
|----------|------|
| `DSRC_Init()` | clear table + queue, reset parser |
| `DSRC_RxCallback(byte)` | called from the USART ISR — parse one byte |
| `DSRC_Update()` | drain the queue into the neighbor table (call in main loop) |
| `DSRC_SendNeighbor(n)` | broadcast my own struct |
| `DSRC_GetTable()` / `DSRC_GetCount()` | how the safety modules read the neighbors |
| `DSRC_RemoveStale(now)` | drop neighbors older than `NEIGHBOR_TIMEOUT` (2000) |

```text
USART RX ISR ──► DSRC_RxCallback ──► [queue]
                                        │
main loop:  DSRC_Update() ──drains──────┘──► neighbor_table
            SafetyEngine reads the table via DSRC_GetTable()/DSRC_GetCount()
            DSRC_SendNeighbor(self) ──► broadcasts my state+flags
```

> **Open point:** `DSRC_RemoveStale()` exists but is **not called** in the current main loop, so stale neighbors persist. To use it you need `last_update` populated with real time (SysTick tick) on each received/updated entry. Listed as a follow-up for the real integration.

---

## 9. Quick Summary

| Point | Takeaway |
|-------|----------|
| Transport | UART (USART1) at 115200 baud |
| Frame | `0xAA` + raw struct + XOR checksum + `0x55` |
| RX path | interrupt → 4-state parser → ring queue → main loop drains |
| Table | up to 20 neighbors, one row per `vehicle_id`, oldest evicted |
| Integrity | 1-byte XOR checksum (prototype-grade) |
| Thread-safety | `volatile` ring buffer keeps the ISR off the neighbor table |
