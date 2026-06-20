/* main.c - STM32 V2V UART Node
 * Sends speed to Raspberry Pi, receives V2P data back.
 *
 * Connections:
 *   STM32 TX (PA2)  --> RPi RX
 *   STM32 RX (PA3)  --> RPi TX
 *   Baudrate: 9600
 *
 * Setup in CubeMX:
 *   - Enable USART2, Async, 9600 8N1
 *   - Enable USART2 global interrupt
 */

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Config ──────────────────────────────────────────────── */
#define FIXED_SPEED_KMH     60.0f   /* fixed test speed */
#define SEND_INTERVAL_MS    1000    /* send every 1 second */
#define RX_BUFFER_SIZE      64

/* ── Globals ─────────────────────────────────────────────── */
extern UART_HandleTypeDef huart2;

/* RX */
static char     rx_buf[RX_BUFFER_SIZE];
static uint8_t  rx_byte;           /* single byte DMA/IT target */
static uint8_t  rx_index = 0;
static uint8_t  rx_ready = 0;      /* set to 1 when full line received */

/* Parsed V2P data (received from RPi) */
typedef struct {
    int   car_count;
    float front_cm;
    char  alert_level[16];  /* "none" | "caution" | "danger" */
    char  tl_state[8];      /* "RED"  | "GREEN"   | "YELLOW" */
} V2P_Data;

static V2P_Data v2p = {0, 0.0f, "none", "UNKNOWN"};

/* ── Prototypes ──────────────────────────────────────────── */
static void send_speed(float speed_kmh);
static void parse_v2p(char *line);
static void process_v2p(void);

/* ═══════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* Start receiving one byte at a time (interrupt mode) */
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

    uint32_t last_send = 0;

    while (1)
    {
        /* --- Send speed every SEND_INTERVAL_MS --- */
        if ((HAL_GetTick() - last_send) >= SEND_INTERVAL_MS)
        {
            last_send = HAL_GetTick();
            send_speed(FIXED_SPEED_KMH);
        }

        /* --- Process received line if ready --- */
        if (rx_ready)
        {
            rx_ready = 0;
            parse_v2p(rx_buf);
            process_v2p();
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 * Send speed to Raspberry Pi
 * Format: "SPEED:60.0\r\n"
 * ═══════════════════════════════════════════════════════════ */
static void send_speed(float speed_kmh)
{
    char msg[32];
    int  len = snprintf(msg, sizeof(msg), "SPEED:%.1f\r\n", speed_kmh);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, len, 100);
}

/* ═══════════════════════════════════════════════════════════
 * Parse line received from RPi
 * Expected format: "V2P:<cars>,<front_cm>,<alert>,<tl>\r\n"
 * Example:         "V2P:3,285.0,danger,RED"
 * ═══════════════════════════════════════════════════════════ */
static void parse_v2p(char *line)
{
    if (strncmp(line, "V2P:", 4) != 0)
        return;   /* not our message */

    char *p = line + 4;   /* skip "V2P:" */

    /* car_count */
    v2p.car_count = atoi(p);
    p = strchr(p, ','); if (!p) return; p++;

    /* front_cm */
    v2p.front_cm = strtof(p, NULL);
    p = strchr(p, ','); if (!p) return; p++;

    /* alert_level */
    char *end = strchr(p, ','); if (!end) return;
    int len = end - p;
    if (len >= (int)sizeof(v2p.alert_level)) len = sizeof(v2p.alert_level) - 1;
    strncpy(v2p.alert_level, p, len);
    v2p.alert_level[len] = '\0';
    p = end + 1;

    /* tl_state (strip \r\n) */
    strncpy(v2p.tl_state, p, sizeof(v2p.tl_state) - 1);
    v2p.tl_state[strcspn(v2p.tl_state, "\r\n")] = '\0';
}

/* ═══════════════════════════════════════════════════════════
 * Act on parsed V2P data
 * Blink LED / set outputs based on alert level
 * ═══════════════════════════════════════════════════════════ */
static void process_v2p(void)
{
    /* Send ACK back to RPi */
    const char *ack = "ACK:OK\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)ack, strlen(ack), 50);

    /* Example: turn on LED on PA5 if danger */
    if (strcmp(v2p.alert_level, "danger") == 0)
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    /* Example: turn on LED on PA6 if RED light */
    if (strcmp(v2p.tl_state, "RED") == 0)
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}

/* ═══════════════════════════════════════════════════════════
 * UART RX Interrupt Callback
 * Called after every single byte received.
 * Builds a line in rx_buf, sets rx_ready on '\n'.
 * ═══════════════════════════════════════════════════════════ */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2)
        return;

    if (rx_byte == '\n')
    {
        /* Null-terminate and signal main loop */
        if (rx_index > 0 && rx_buf[rx_index - 1] == '\r')
            rx_index--;             /* strip \r */
        rx_buf[rx_index] = '\0';
        rx_index = 0;
        rx_ready = 1;
    }
    else if (rx_index < RX_BUFFER_SIZE - 1)
    {
        rx_buf[rx_index++] = (char)rx_byte;
    }
    else
    {
        rx_index = 0;   /* overflow — reset */
    }

    /* Re-arm interrupt for next byte */
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}
