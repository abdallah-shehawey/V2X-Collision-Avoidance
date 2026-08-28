/**
 ******************************************************************************
 * @file    Bootloader_main.c
 * @author  Alaa Hassan
 * @brief   FOTA bootloader entry point: boot-decision/rollback logic, the
 *          UART wire-protocol session (HELLO..COMMIT), and the handoff jump
 *          into whichever application slot ends up active.
 * @ingroup app_fota
 *
 * @details
 * Runs BEFORE the application, on every single reset — it lives at
 * 0x08000000, which is where the core always starts. Two outcomes:
 *
 * - **Normal case (>99% of resets):** no update in progress, nothing shows
 *   up on the UART within @ref BOOTLOADER_RECOVERY_WINDOW_MS → jump straight
 *   to the active application slot. Fast — no flash writes at all on this path.
 * - **Update case:** something answers the `HELLO` handshake within the
 *   window (see @ref Bootloader_RunSession) → the full transfer protocol
 *   runs, and only on a successful `COMMIT` does the active slot change
 *   before jumping.
 *
 * See `../docs/FOTA.md` for the full design (flash layout §3, package format
 * §4, wire protocol §5) that this file implements, and this folder's
 * `README.md` for how to build, flash and bench-test it.
 *
 * @warning **This bootloader deliberately never touches the IWDG.** Starting
 *          it here would run into a real, verified conflict: the
 *          application's own `System_setup()` (`../Src/System.c`) has an MPU
 *          magnetometer calibration sequence that blocks for roughly 12
 *          seconds BEFORE the application arms the watchdog itself in
 *          `main()` — well past the IWDG's ~8.19 s hardware maximum timeout.
 *          A bootloader-armed IWDG that is still ticking when it hands off
 *          would false-trip the MCU mid-calibration on every single boot,
 *          since the IWDG cannot be stopped once started. Leaving the IWDG
 *          completely untouched here means the application experiences
 *          EXACTLY the same (already pre-existing, unrelated to FOTA)
 *          watchdog-free window on a bootloader handoff as it does on an
 *          ordinary power-on reset — nothing new, nothing worse. Flash
 *          erase/program operations are instead bounded by their own
 *          bus-wait timeouts inside the FLASH driver itself
 *          (`FLASH_enumWaitBusy`), not by the hardware watchdog.
 ******************************************************************************
 */

#include <stdint.h>
#include <string.h>

#include "../../Inc/Drivers/LIB/ErrTypes.h"
#include "../../Inc/Drivers/MCAL/RCC/RCC_interface.h"
#include "../../Inc/Drivers/MCAL/GPIO/GPIO_interface.h"
#include "../../Inc/Drivers/MCAL/SCB/SCB_interface.h"
#include "../../Inc/Drivers/MCAL/TIM/TIM_interface.h"
#include "../../Inc/Drivers/MCAL/USART/USART_intreface.h"
#include "../../Inc/Drivers/MCAL/FLASH/FLASH_interface.h"
#include "../../Inc/Application/FOTA/FOTA_CRC32_interface.h"
#include "../../Inc/Application/FOTA/FOTA_Metadata_interface.h"
#include "../../Inc/Application/FOTA/FOTA_Protocol_interface.h"

/*============================================================================*/
/*                                CONFIGURATION                               */
/*============================================================================*/

/**
 * @brief Which @ref FOTA_PackageHeader_t::board_id this build of the
 *        bootloader accepts an update for.
 * @note  Change to @ref FOTA_BOARD_ID_CARRIER_PCB and rebuild when flashing
 *        the custom carrier PCB instead of the Nucleo bench board — see the
 *        PA0/PA1-vs-PA2/PA3 note in `../docs/FOTA.md` §5, which is the whole
 *        reason this check exists.
 */
#define BOOTLOADER_ACCEPTED_BOARD_ID FOTA_BOARD_ID_NUCLEO

/** @brief How long, on every single reset, the bootloader listens for a `HELLO` before giving up and auto-jumping. */
#define BOOTLOADER_RECOVERY_WINDOW_MS 1000U

/** @brief How long it waits instead when the application itself asked for an update (@ref FOTA_Metadata_t::update_requested). Generous — a human may be about to start a transfer manually. */
#define BOOTLOADER_UPDATE_WINDOW_MS 30000U

/** @brief Once a session has started, how long a silence before it is abandoned (falls through to the jump, using whatever state was reached). */
#define BOOTLOADER_SESSION_IDLE_MS 5000U

/** @brief Once a frame has begun (its START byte was seen), how long to wait for each subsequent byte. Short — a sender that started a frame is expected to keep sending. */
#define BOOTLOADER_INTRA_FRAME_TIMEOUT_MS 300U

/*============================================================================*/
/*                              LOW-LEVEL HELPERS                             */
/*============================================================================*/

/** @brief The Raspberry Pi / bench-PC link — UART4, PA0(TX)/PA1(RX), 115200 8N1, fully polled (no interrupts anywhere in this bootloader). */
static USART_Config_t Bootloader_UART = {
    .Channel = USART_CHANNEL4,
    .BaudRate = 115200,
    .WordLength = USART_WORDLENGTH_8B,
    .StopBits = USART_STOPBITS_1,
    .Parity = USART_PARITY_NONE,
    .Mode = USART_MODE_TX_RX,
    .HardwareFlowControl = UART_HWCONTROL_NONE,
    .OverSampling = USART_OVERSAMPLING_16};

/** @brief Sectors making up application Slot A — see `../docs/FOTA.md` §3. */
static const FLASH_Sector_t Bootloader_aeSlotASectors[] = {FLASH_SECTOR_3, FLASH_SECTOR_4, FLASH_SECTOR_5};
/** @brief Sectors making up application Slot B. */
static const FLASH_Sector_t Bootloader_aeSlotBSectors[] = {FLASH_SECTOR_6, FLASH_SECTOR_7};

/**
 * @brief Bring up exactly what this bootloader needs: HSI clock (already the
 *        reset default; set explicitly for clarity, matching the
 *        application's own `System_setup()`), the UART, and a free-running
 *        millisecond counter on TIM5.
 *
 * Deliberately minimal — no ultrasonics, no IMU, no LEDs, no FreeRTOS. Every
 * peripheral this touches is re-initialised from scratch by the
 * application's own `System_setup()` after a successful jump, so nothing
 * here needs to be "undone" before handing off.
 */
static void Bootloader_HW_Init(void)
{
  RCC_enumSetSysClk(RCC_HSI_CLK);

  RCC_enumAHPPerSts(RCC_AHB1, RCC_GPIOAEN, RCC_PER_ON);
  RCC_enumABPPerSts(RCC_APB1, RCC_USART4EN, RCC_PER_ON);
  RCC_enumABPPerSts(RCC_APB1, RCC_TIM5EN, RCC_PER_ON);

  GPIO_PinConfig_t Local_stU4Pins = {
      .Port = GPIO_PORTA,
      .Mode = GPIO_ALTFN,
      .Otype = GPIO_PUSH_PULL,
      .Speed = GPIO_VERY_HIGH_SPEED,
      .PullType = GPIO_NO_PULL,
      .AlternateFunction = GPIO_AF8};
  Local_stU4Pins.PinNum = GPIO_PIN0;
  GPIO_enumPinInit(&Local_stU4Pins); /* UART4 TX (PA0) */
  Local_stU4Pins.PinNum = GPIO_PIN1;
  GPIO_enumPinInit(&Local_stU4Pins); /* UART4 RX (PA1) */

  USART_Init(&Bootloader_UART);

  /* Free-running 1ms-tick 32-bit counter — same pattern as System.c's own
   * TIM5 background timestamp, just re-created here since the bootloader
   * does not share RAM/peripheral state with the application. */
  TIM_Config_t Local_stTim5Cfg = {
      .Timer = TIM_TIMER5,
      .Prescaler = 16000U - 1U, /* 16 MHz HSI -> 1 kHz tick */
      .AutoReloadValue = 0xFFFFFFFFU,
      .Mode = TIM_COUNTERMODE_UP};
  TIM_vInit(&Local_stTim5Cfg);
  TIM_vStart(TIM_TIMER5);
}

/** @brief Milliseconds since @ref Bootloader_HW_Init — wraps at ~49.7 days; every timeout here compares differences, which stay correct across one wrap. */
static uint32_t Bootloader_u32Millis(void)
{
  uint32_t Local_u32Value = 0U;
  TIM_u32GetCounterValue(TIM_TIMER5, &Local_u32Value);
  return Local_u32Value;
}

/** @brief Block until one byte arrives or @p Copy_u32TimeoutMs elapses. Pure poll — never touches an interrupt. */
static uint8_t Bootloader_u8ReadByte(uint8_t *Copy_pu8Byte, uint32_t Copy_u32TimeoutMs)
{
  uint32_t Local_u32Start = Bootloader_u32Millis();

  while ((Bootloader_u32Millis() - Local_u32Start) < Copy_u32TimeoutMs)
  {
    if (USART_u8IsRxNotEmpty(USART_CHANNEL4))
    {
      *Copy_pu8Byte = USART_ReceiveByteDirect(USART_CHANNEL4);
      return 1U;
    }
  }

  return 0U;
}

/*============================================================================*/
/*                             FRAME PARSER / SENDER                          */
/*============================================================================*/

/** @brief One received, CRC-validated frame — see @ref FOTA_Protocol_interface.h for the wire layout. */
typedef struct
{
  uint8_t  cmd;
  uint16_t len;
  uint8_t  payload[FOTA_PROTO_MAX_PAYLOAD];
} Bootloader_Frame_t;

/**
 * @brief The CRC32 a frame's four CRC bytes must equal: computed over
 *        `cmd`, the 2-byte little-endian `len`, and the payload — never over
 *        the START/END delimiter bytes themselves.
 */
static uint32_t Bootloader_u32FrameCrc(uint8_t Copy_u8Cmd, uint16_t Copy_u16Len, const uint8_t *Copy_pu8Payload)
{
  uint8_t Local_au8LenBytes[2] = {(uint8_t)(Copy_u16Len & 0xFFU), (uint8_t)((Copy_u16Len >> 8) & 0xFFU)};
  uint32_t Local_u32Crc = FOTA_CRC32_u32Init();

  Local_u32Crc = FOTA_CRC32_u32Update(Local_u32Crc, &Copy_u8Cmd, 1U);
  Local_u32Crc = FOTA_CRC32_u32Update(Local_u32Crc, Local_au8LenBytes, 2U);
  if (Copy_u16Len > 0U)
  {
    Local_u32Crc = FOTA_CRC32_u32Update(Local_u32Crc, Copy_pu8Payload, Copy_u16Len);
  }

  return FOTA_CRC32_u32Finalize(Local_u32Crc);
}

/** @brief Send one complete frame. Blocking (polled TX, like everything else here). */
static void Bootloader_SendFrame(uint8_t Copy_u8Cmd, const uint8_t *Copy_pu8Payload, uint16_t Copy_u16Len)
{
  uint32_t Local_u32Crc = Bootloader_u32FrameCrc(Copy_u8Cmd, Copy_u16Len, Copy_pu8Payload);
  uint16_t i;

  USART_enumTransmit(&Bootloader_UART, FOTA_PROTO_START_BYTE);
  USART_enumTransmit(&Bootloader_UART, Copy_u8Cmd);
  USART_enumTransmit(&Bootloader_UART, (uint8_t)(Copy_u16Len & 0xFFU));
  USART_enumTransmit(&Bootloader_UART, (uint8_t)((Copy_u16Len >> 8) & 0xFFU));

  for (i = 0U; i < Copy_u16Len; i++)
  {
    USART_enumTransmit(&Bootloader_UART, Copy_pu8Payload[i]);
  }

  USART_enumTransmit(&Bootloader_UART, (uint8_t)(Local_u32Crc & 0xFFU));
  USART_enumTransmit(&Bootloader_UART, (uint8_t)((Local_u32Crc >> 8) & 0xFFU));
  USART_enumTransmit(&Bootloader_UART, (uint8_t)((Local_u32Crc >> 16) & 0xFFU));
  USART_enumTransmit(&Bootloader_UART, (uint8_t)((Local_u32Crc >> 24) & 0xFFU));

  USART_enumTransmit(&Bootloader_UART, FOTA_PROTO_END_BYTE);
}

/** @brief Convenience: a response with no payload (ACKs, and the frame that only needs its command byte to say everything). */
static void Bootloader_SendCmd(uint8_t Copy_u8Cmd)
{
  Bootloader_SendFrame(Copy_u8Cmd, NULL, 0U);
}

/** @brief Wait up to @p Copy_u32TimeoutMs for @ref FOTA_PROTO_START_BYTE, discarding anything else — the entry point of receiving a frame. */
static uint8_t Bootloader_u8WaitForStart(uint32_t Copy_u32TimeoutMs)
{
  uint32_t Local_u32Start = Bootloader_u32Millis();
  uint8_t Local_u8Byte;

  for (;;)
  {
    uint32_t Local_u32Elapsed = Bootloader_u32Millis() - Local_u32Start;

    if (Local_u32Elapsed >= Copy_u32TimeoutMs)
    {
      return 0U;
    }

    if (!Bootloader_u8ReadByte(&Local_u8Byte, Copy_u32TimeoutMs - Local_u32Elapsed))
    {
      return 0U; /* the remaining budget ran out inside ReadByte itself */
    }

    if (Local_u8Byte == FOTA_PROTO_START_BYTE)
    {
      return 1U;
    }
    /* anything else: keep discarding within the same window */
  }
}

/**
 * @brief Read the rest of a frame (everything after the START byte, which
 *        the caller has already consumed via @ref Bootloader_u8WaitForStart)
 *        and validate it.
 * @retval 1 A well-formed, CRC-valid frame is in @p Copy_pstFrame.
 * @retval 0 Timed out mid-frame, `len` was out of range, the END byte was
 *           wrong, or the CRC did not match — the caller should simply wait
 *           for the next frame (@ref Bootloader_u8WaitForStart again), not
 *           treat this as fatal.
 */
static uint8_t Bootloader_u8ReceiveFrameBody(Bootloader_Frame_t *Copy_pstFrame)
{
  uint8_t Local_u8Byte;
  uint16_t Local_u16Len;
  uint16_t Local_u16Idx;
  uint8_t Local_au8Crc[4];
  uint8_t i;
  uint32_t Local_u32Expected;
  uint32_t Local_u32Received;

  if (!Bootloader_u8ReadByte(&Local_u8Byte, BOOTLOADER_INTRA_FRAME_TIMEOUT_MS)) return 0U;
  Copy_pstFrame->cmd = Local_u8Byte;

  if (!Bootloader_u8ReadByte(&Local_u8Byte, BOOTLOADER_INTRA_FRAME_TIMEOUT_MS)) return 0U;
  Local_u16Len = Local_u8Byte;
  if (!Bootloader_u8ReadByte(&Local_u8Byte, BOOTLOADER_INTRA_FRAME_TIMEOUT_MS)) return 0U;
  Local_u16Len |= ((uint16_t)Local_u8Byte << 8);

  if (Local_u16Len > FOTA_PROTO_MAX_PAYLOAD)
  {
    return 0U; /* not a frame this bootloader speaks — resync on the next START byte */
  }
  Copy_pstFrame->len = Local_u16Len;

  for (Local_u16Idx = 0U; Local_u16Idx < Local_u16Len; Local_u16Idx++)
  {
    if (!Bootloader_u8ReadByte(&Copy_pstFrame->payload[Local_u16Idx], BOOTLOADER_INTRA_FRAME_TIMEOUT_MS)) return 0U;
  }

  for (i = 0U; i < 4U; i++)
  {
    if (!Bootloader_u8ReadByte(&Local_au8Crc[i], BOOTLOADER_INTRA_FRAME_TIMEOUT_MS)) return 0U;
  }

  if (!Bootloader_u8ReadByte(&Local_u8Byte, BOOTLOADER_INTRA_FRAME_TIMEOUT_MS)) return 0U;
  if (Local_u8Byte != FOTA_PROTO_END_BYTE)
  {
    return 0U;
  }

  Local_u32Received = (uint32_t)Local_au8Crc[0] | ((uint32_t)Local_au8Crc[1] << 8) |
                       ((uint32_t)Local_au8Crc[2] << 16) | ((uint32_t)Local_au8Crc[3] << 24);
  Local_u32Expected = Bootloader_u32FrameCrc(Copy_pstFrame->cmd, Copy_pstFrame->len, Copy_pstFrame->payload);

  return (Local_u32Received == Local_u32Expected) ? 1U : 0U;
}

/*============================================================================*/
/*                          UPDATE SESSION STATE MACHINE                      */
/*============================================================================*/

/** @brief Everything the update session tracks between frames, for the one transfer in progress (if any). */
typedef struct
{
  uint8_t  active;         /**< A START_UPDATE has been accepted; DATA_CHUNK/END_UPDATE are now meaningful. */
  uint8_t  verified;        /**< END_UPDATE's whole-image CRC check passed; COMMIT may now be accepted. */
  uint8_t  target_slot;     /**< @ref FOTA_SLOT_A or @ref FOTA_SLOT_B — always the slot NOT currently active. */
  uint32_t target_base;     /**< Flash base address of that slot. */
  uint32_t expected_len;    /**< Payload length announced by START_UPDATE. */
  uint32_t expected_crc;    /**< Payload CRC32 announced by START_UPDATE. */
  uint32_t build_no;        /**< Build number announced by START_UPDATE. */
  uint8_t  board_id;        /**< Board id announced by START_UPDATE. */
  uint32_t bytes_written;   /**< How much of the payload has been flashed so far. */
  uint32_t running_crc;     /**< Running CRC32 state (@ref FOTA_CRC32_u32Update) over the bytes written so far. */
  uint16_t expected_seq;    /**< Next DATA_CHUNK sequence number this session will accept. */
} Bootloader_Session_t;

/** @brief Erase every sector of one application slot, in order. Stops at the first failure. */
static ErrorState_t Bootloader_enumEraseSlot(uint8_t Copy_u8Slot)
{
  ErrorState_t Local_u8ErrorState = OK;
  const FLASH_Sector_t *Local_paeSectors = (Copy_u8Slot == FOTA_SLOT_A) ? Bootloader_aeSlotASectors : Bootloader_aeSlotBSectors;
  uint8_t Local_u8Count = (Copy_u8Slot == FOTA_SLOT_A)
                              ? (uint8_t)(sizeof(Bootloader_aeSlotASectors) / sizeof(Bootloader_aeSlotASectors[0]))
                              : (uint8_t)(sizeof(Bootloader_aeSlotBSectors) / sizeof(Bootloader_aeSlotBSectors[0]));
  uint8_t i;

  for (i = 0U; (i < Local_u8Count) && (Local_u8ErrorState == OK); i++)
  {
    Local_u8ErrorState = FLASH_enumEraseSector(Local_paeSectors[i]);
  }

  return Local_u8ErrorState;
}

/** @brief Handle one HELLO frame. */
static void Bootloader_HandleHello(const FOTA_Metadata_t *Copy_pstMeta)
{
  FOTA_HelloAck_t Local_stAck;
  Local_stAck.active_slot = Copy_pstMeta->active_slot;
  Local_stAck.build_no = Copy_pstMeta->slot[Copy_pstMeta->active_slot].build_no;
  Bootloader_SendFrame(FOTA_CMD_HELLO_ACK, (const uint8_t *)&Local_stAck, (uint16_t)sizeof(Local_stAck));
}

/** @brief Handle one START_UPDATE frame: validate the package header, pick and erase the target slot. */
static void Bootloader_HandleStartUpdate(const FOTA_Metadata_t *Copy_pstMeta, Bootloader_Session_t *Copy_pstSession,
                                          const Bootloader_Frame_t *Copy_pstFrame)
{
  FOTA_PackageHeader_t Local_stHeader;
  uint8_t Local_u8TargetSlot;
  uint32_t Local_u32TargetBase;
  uint32_t Local_u32TargetSize;
  uint8_t Local_u8Nak = 0U;

  memset(Copy_pstSession, 0, sizeof(*Copy_pstSession));

  if (Copy_pstFrame->len != sizeof(FOTA_PackageHeader_t))
  {
    Local_u8Nak = FOTA_NAK_BAD_MAGIC;
  }
  else
  {
    memcpy(&Local_stHeader, Copy_pstFrame->payload, sizeof(Local_stHeader));

    if (Local_stHeader.magic != FOTA_PKG_MAGIC)
    {
      Local_u8Nak = FOTA_NAK_BAD_MAGIC;
    }
    else if (Local_stHeader.board_id != BOOTLOADER_ACCEPTED_BOARD_ID)
    {
      Local_u8Nak = FOTA_NAK_WRONG_BOARD;
    }
    else
    {
      Local_u8TargetSlot = (Copy_pstMeta->active_slot == FOTA_SLOT_A) ? FOTA_SLOT_B : FOTA_SLOT_A;
      Local_u32TargetBase = (Local_u8TargetSlot == FOTA_SLOT_A) ? FOTA_SLOTA_BASE : FOTA_SLOTB_BASE;
      Local_u32TargetSize = (Local_u8TargetSlot == FOTA_SLOT_A) ? FOTA_SLOTA_SIZE : FOTA_SLOTB_SIZE;

      if (Local_stHeader.payload_len > Local_u32TargetSize)
      {
        Local_u8Nak = FOTA_NAK_TOO_LARGE;
      }
      else if (Bootloader_enumEraseSlot(Local_u8TargetSlot) != OK)
      {
        Local_u8Nak = FOTA_NAK_ERASE_FAILED;
      }
      else
      {
        Copy_pstSession->active = 1U;
        Copy_pstSession->target_slot = Local_u8TargetSlot;
        Copy_pstSession->target_base = Local_u32TargetBase;
        Copy_pstSession->expected_len = Local_stHeader.payload_len;
        Copy_pstSession->expected_crc = Local_stHeader.payload_crc32;
        Copy_pstSession->build_no = Local_stHeader.build_no;
        Copy_pstSession->board_id = Local_stHeader.board_id;
        Copy_pstSession->running_crc = FOTA_CRC32_u32Init();
      }
    }
  }

  if (Local_u8Nak != 0U)
  {
    Bootloader_SendFrame(FOTA_CMD_START_NAK, &Local_u8Nak, 1U);
  }
  else
  {
    Bootloader_SendCmd(FOTA_CMD_START_ACK);
  }
}

/** @brief Handle one DATA_CHUNK frame: write it to the target slot if its sequence number is the one expected. */
static void Bootloader_HandleDataChunk(Bootloader_Session_t *Copy_pstSession, const Bootloader_Frame_t *Copy_pstFrame)
{
  uint16_t Local_u16Seq;
  uint16_t Local_u16DataLen;
  const uint8_t *Local_pu8Data;
  uint8_t Local_au8SeqBytes[2];

  if ((!Copy_pstSession->active) || (Copy_pstFrame->len < sizeof(FOTA_ChunkHeader_t)))
  {
    return; /* nothing sane to NAK — sender is out of sequence with reality; it will time out and can restart cleanly with a fresh HELLO */
  }

  Local_u16Seq = (uint16_t)Copy_pstFrame->payload[0] | ((uint16_t)Copy_pstFrame->payload[1] << 8);
  Local_u16DataLen = (uint16_t)(Copy_pstFrame->len - sizeof(FOTA_ChunkHeader_t));
  Local_pu8Data = &Copy_pstFrame->payload[sizeof(FOTA_ChunkHeader_t)];

  Local_au8SeqBytes[0] = (uint8_t)(Local_u16Seq & 0xFFU);
  Local_au8SeqBytes[1] = (uint8_t)((Local_u16Seq >> 8) & 0xFFU);

  if ((Local_u16Seq != Copy_pstSession->expected_seq) ||
      ((Copy_pstSession->bytes_written + Local_u16DataLen) > Copy_pstSession->expected_len))
  {
    /* Wrong chunk, or one that would overrun the announced length — tell the
     * sender exactly which sequence number we are still waiting for. */
    uint8_t Local_au8ExpectedSeqBytes[2] = {(uint8_t)(Copy_pstSession->expected_seq & 0xFFU),
                                             (uint8_t)((Copy_pstSession->expected_seq >> 8) & 0xFFU)};
    Bootloader_SendFrame(FOTA_CMD_CHUNK_NAK, Local_au8ExpectedSeqBytes, 2U);
    return;
  }

  if (FLASH_enumProgramBuffer(Copy_pstSession->target_base + Copy_pstSession->bytes_written, Local_pu8Data, Local_u16DataLen) != OK)
  {
    Bootloader_SendFrame(FOTA_CMD_CHUNK_NAK, Local_au8SeqBytes, 2U);
    return;
  }

  Copy_pstSession->running_crc = FOTA_CRC32_u32Update(Copy_pstSession->running_crc, Local_pu8Data, Local_u16DataLen);
  Copy_pstSession->bytes_written += Local_u16DataLen;
  Copy_pstSession->expected_seq++;

  Bootloader_SendFrame(FOTA_CMD_CHUNK_ACK, Local_au8SeqBytes, 2U);
}

/** @brief Handle one END_UPDATE frame: the whole-image CRC gate. */
static void Bootloader_HandleEndUpdate(Bootloader_Session_t *Copy_pstSession)
{
  uint32_t Local_u32FinalCrc;

  if (!Copy_pstSession->active)
  {
    Bootloader_SendCmd(FOTA_CMD_END_NAK);
    return;
  }

  Local_u32FinalCrc = FOTA_CRC32_u32Finalize(Copy_pstSession->running_crc);

  if ((Copy_pstSession->bytes_written != Copy_pstSession->expected_len) || (Local_u32FinalCrc != Copy_pstSession->expected_crc))
  {
    Copy_pstSession->active = 0U; /* abort — COMMIT will be refused regardless, but stop DATA_CHUNK from doing anything more too */
    Bootloader_SendCmd(FOTA_CMD_END_NAK);
    return;
  }

  Copy_pstSession->verified = 1U;
  Bootloader_SendCmd(FOTA_CMD_END_ACK);
}

/**
 * @brief Handle one COMMIT frame: only ever accepted after a verified
 *        END_UPDATE. Persists the new slot as active and returns 1 to tell
 *        the caller the session is over and it is time to jump.
 */
static uint8_t Bootloader_u8HandleCommit(FOTA_Metadata_t *Copy_pstMeta, Bootloader_Session_t *Copy_pstSession)
{
  FOTA_Metadata_t Local_stMeta;

  if ((!Copy_pstSession->active) || (!Copy_pstSession->verified))
  {
    Bootloader_SendCmd(FOTA_CMD_COMMIT_NAK);
    return 0U;
  }

  Local_stMeta = *Copy_pstMeta;
  Local_stMeta.slot[Copy_pstSession->target_slot].build_no = Copy_pstSession->build_no;
  Local_stMeta.slot[Copy_pstSession->target_slot].payload_len = Copy_pstSession->expected_len;
  Local_stMeta.slot[Copy_pstSession->target_slot].payload_crc32 = Copy_pstSession->expected_crc;
  Local_stMeta.slot[Copy_pstSession->target_slot].board_id = Copy_pstSession->board_id;
  Local_stMeta.slot[Copy_pstSession->target_slot].valid = 1U;
  Local_stMeta.active_slot = Copy_pstSession->target_slot;
  /* This slot has never booted yet — arm the confirmation check. Also
   * consumes the single-shot update_requested flag, since this save is
   * about to become the new authoritative state regardless of how that
   * flag got here. */
  Local_stMeta.boot_pending = 1U;
  Local_stMeta.boot_attempts = 0U;
  Local_stMeta.update_requested = 0U;

  if (FOTA_Metadata_enumSave(&Local_stMeta) != OK)
  {
    Bootloader_SendCmd(FOTA_CMD_COMMIT_NAK);
    return 0U;
  }

  *Copy_pstMeta = Local_stMeta;
  Copy_pstSession->active = 0U;
  Bootloader_SendCmd(FOTA_CMD_COMMIT_ACK);
  return 1U;
}

/**
 * @brief Run the update session to completion: HELLO/START_UPDATE/DATA_CHUNK*
 *        /END_UPDATE/COMMIT, in any order a well-behaved sender would use,
 *        until either a successful COMMIT happens or the link goes quiet for
 *        @ref BOOTLOADER_SESSION_IDLE_MS.
 *
 * @param[in,out] Copy_pstMeta   The metadata state — read for HELLO/START_UPDATE
 *                               decisions, updated in place by a successful COMMIT.
 * @param[in]     Copy_u32FirstWaitMs How long to wait for the FIRST frame
 *                               (the recovery/update window); every frame
 *                               after that uses @ref BOOTLOADER_SESSION_IDLE_MS.
 * @retval 1 A COMMIT succeeded — @p Copy_pstMeta now reflects the new active slot.
 * @retval 0 No session happened, or one happened but never committed
 *          (aborted, timed out, or the sender simply disconnected after
 *          HELLO/START_UPDATE without finishing) — @p Copy_pstMeta is
 *          unchanged from what the caller passed in.
 */
static uint8_t Bootloader_u8RunSession(FOTA_Metadata_t *Copy_pstMeta, uint32_t Copy_u32FirstWaitMs)
{
  Bootloader_Session_t Local_stSession;
  uint32_t Local_u32WaitMs = Copy_u32FirstWaitMs;

  memset(&Local_stSession, 0, sizeof(Local_stSession));

  for (;;)
  {
    Bootloader_Frame_t Local_stFrame;

    if (!Bootloader_u8WaitForStart(Local_u32WaitMs))
    {
      return 0U; /* silence for the whole window — session over, nothing committed */
    }

    /* Every frame after the first uses the shorter session-idle timeout, win
     * or lose on this one. */
    Local_u32WaitMs = BOOTLOADER_SESSION_IDLE_MS;

    if (!Bootloader_u8ReceiveFrameBody(&Local_stFrame))
    {
      continue; /* malformed frame — wait for the next attempt */
    }

    switch (Local_stFrame.cmd)
    {
      case FOTA_CMD_HELLO:
        Bootloader_HandleHello(Copy_pstMeta);
        break;

      case FOTA_CMD_START_UPDATE:
        Bootloader_HandleStartUpdate(Copy_pstMeta, &Local_stSession, &Local_stFrame);
        break;

      case FOTA_CMD_DATA_CHUNK:
        Bootloader_HandleDataChunk(&Local_stSession, &Local_stFrame);
        break;

      case FOTA_CMD_END_UPDATE:
        Bootloader_HandleEndUpdate(&Local_stSession);
        break;

      case FOTA_CMD_COMMIT:
        if (Bootloader_u8HandleCommit(Copy_pstMeta, &Local_stSession))
        {
          return 1U;
        }
        break;

      default:
        /* Unknown command — ignore it. A well-behaved sender never sends one;
         * this is not a reason to abandon an otherwise fine session. */
        break;
    }
  }
}

/*============================================================================*/
/*                          BOOTLOADER -> APPLICATION HANDOFF                 */
/*============================================================================*/

/** @brief `CPSID i` — mask interrupts. Written by hand rather than pulled from CMSIS, matching this project's from-scratch style. */
static inline void Bootloader_DisableIRQ(void)
{
  __asm volatile ("cpsid i" ::: "memory");
}

/** @brief `MSR msp, r0` — set the Main Stack Pointer to the application's own initial value. */
static inline void Bootloader_SetMSP(uint32_t Copy_u32TopOfStack)
{
  __asm volatile ("MSR msp, %0" : : "r" (Copy_u32TopOfStack) : );
}

/**
 * @brief Hand off to whichever application slot is now active: relocate the
 *        vector table, set the stack pointer, and branch to the application's
 *        own `Reset_Handler` — exactly as if the core had reset directly
 *        into it. Never returns.
 *
 * @param[in] Copy_u32AppBase Base address of the slot to boot
 *                            (@ref FOTA_SLOTA_BASE or @ref FOTA_SLOTB_BASE).
 */
static void Bootloader_JumpToApp(uint32_t Copy_u32AppBase)
{
  uint32_t Local_u32AppMsp = *(volatile uint32_t *)(Copy_u32AppBase + 0U);
  uint32_t Local_u32AppReset = *(volatile uint32_t *)(Copy_u32AppBase + 4U);
  void (*Local_pfnAppEntry)(void);

  Bootloader_DisableIRQ();

  /* Never armed in this bootloader (see the file header on IWDG), so there is
   * no NVIC interrupt enable to undo here either — nothing was ever armed
   * for the application to inherit. */

  (void)SCB_vSetVectorTable(Copy_u32AppBase);
  Bootloader_SetMSP(Local_u32AppMsp);

  Local_pfnAppEntry = (void (*)(void))Local_u32AppReset;
  Local_pfnAppEntry(); /* never returns */

  for (;;)
  {
    /* unreachable */
  }
}

/*============================================================================*/
/*                                    MAIN                                    */
/*============================================================================*/

int main(void)
{
  FOTA_Metadata_t Local_stMeta;
  uint32_t Local_u32Window;
  uint32_t Local_u32AppBase;
  uint8_t Local_u8Committed;

  Bootloader_HW_Init();
  FOTA_Metadata_voidInit();

  Local_stMeta = *FOTA_Metadata_pstGet();

  /* A previous jump never confirmed itself (FOTA_MarkBootOK was never
   * reached by the application) — retry once, then roll back. Persisted
   * BEFORE the listen window below, so a crash during that window still
   * leaves the attempt counted rather than being silently lost. This is the
   * only flash write an ordinary "boot_pending was already 0" reset ever
   * causes — ZERO on a board that has never been through an update at all. */
  if (Local_stMeta.boot_pending)
  {
    Local_stMeta.boot_attempts++;

    if (Local_stMeta.boot_attempts >= FOTA_MAX_BOOT_ATTEMPTS)
    {
      Local_stMeta.active_slot = (Local_stMeta.active_slot == FOTA_SLOT_A) ? FOTA_SLOT_B : FOTA_SLOT_A;
      Local_stMeta.boot_pending = 0U;
      Local_stMeta.boot_attempts = 0U;
    }

    (void)FOTA_Metadata_enumSave(&Local_stMeta);
  }

  Local_u32Window = Local_stMeta.update_requested ? BOOTLOADER_UPDATE_WINDOW_MS : BOOTLOADER_RECOVERY_WINDOW_MS;

  Local_u8Committed = Bootloader_u8RunSession(&Local_stMeta, Local_u32Window);

  /* Re-read the authoritative copy: a successful COMMIT already wrote its
   * own complete, correct record (new active_slot, boot_pending=1,
   * update_requested cleared) inside Bootloader_u8HandleCommit — nothing
   * left to do here in that case. */
  Local_stMeta = *FOTA_Metadata_pstGet();

  if ((!Local_u8Committed) && Local_stMeta.update_requested)
  {
    /* update_requested is single-shot: this listen window is what it bought,
     * win or lose. Only write flash here if it was actually set — the
     * ordinary "nothing showed up in the recovery window" path (the
     * overwhelming majority of every boot) needs no save at all. */
    Local_stMeta.update_requested = 0U;
    (void)FOTA_Metadata_enumSave(&Local_stMeta);
  }

  Local_u32AppBase = (Local_stMeta.active_slot == FOTA_SLOT_A) ? FOTA_SLOTA_BASE : FOTA_SLOTB_BASE;
  Bootloader_JumpToApp(Local_u32AppBase);

  for (;;)
  {
    /* unreachable */
  }
}
