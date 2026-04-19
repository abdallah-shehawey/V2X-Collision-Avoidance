#include "../Inc/System.h"
#include "../Inc/DSRC.h"
#include "../Inc/ErrTypes.h"
#include "../Inc/GPIO_interface.h"
#include "../Inc/RCC_interface.h"
#include "../Inc/USART_intreface.h"
#include "../Inc/SYSTIC_interface.h"
#include "../Inc/NVIC_interface.h"

float US_Distances[US_SENSOR_COUNT];
float Host_Speed   = 0.0f;
float Host_Heading = 0.0f;

// ====== USART ======
USART_Handle_t USART_1 =
    {
        .Channel = USART_CHANNEL1,
        .BaudRate = 9600,
        .WordLength = USART_WORDLENGTH_8B,
        .StopBits = USART_STOPBITS_1,
        .Parity = USART_PARITY_NONE,
        .Mode = USART_MODE_TX_RX,
        .HardwareFlowControl = UART_HWCONTROL_NONE,
        .OverSampling = USART_OVERSAMPLING_8,
        .RXNEIE = USART_RXNEIE_EN,
        .TCIE = USART_TCIE_DIS,
        .TXEIE = USART_TXEIE_DIS,
        .IDLEIE = USART_IDLEIE_DIS,
        .PEIE = USART_PEIE_DIS,
        USART_RXCMP,
};

// ====== GPIO ======
GPIO_PinConfig_t PA9 = {
    .Port = GPIO_PORTA,
    .PinNum = GPIO_PIN9,
    .Mode = GPIO_ALTFN,
    .Otype = GPIO_PUSH_PULL,
    .Speed = GPIO_HIGH_SPEED,
    .PullType = GPIO_NO_PULL,
    .AlternateFunction = GPIO_AF7};

GPIO_PinConfig_t PA10 = {
    .Port = GPIO_PORTA,
    .PinNum = GPIO_PIN10,
    .Mode = GPIO_ALTFN,
    .Otype = GPIO_PUSH_PULL,
    .Speed = GPIO_HIGH_SPEED,
    .PullType = GPIO_NO_PULL,
    .AlternateFunction = GPIO_AF7};

// ============================================================
// System Init - init all peripherals
// ============================================================
void System_Init(void)
{
  // clock init
  SYSTIC_vInit();
  RCC_enumAHPPerSts(RCC_AHB1, RCC_GPIOAEN, RCC_PER_ON);
  RCC_enumABPPerSts(RCC_APB2, RCC_USART1EN, RCC_PER_ON);

  // GPIO init
  GPIO_enumPinInit(&PA9);
  GPIO_enumPinInit(&PA10);

  // USART init
  USART_InitIT(&USART_1);
  NVIC_vEnableIRQ(NVIC_USART1);

  // DSRC init
  DSRC_Init();
}

/* ============ Shared Direction Detection ============ */

/**
 * @brief Calculate absolute heading difference, normalized to [0, 180]
 */
static float CalcHeadingDiff(float h1, float h2)
{
  float diff = h1 - h2;

  if (diff > 180.0f)
  {
    diff -= 360.0f;
  }
  if (diff < -180.0f)
  {
    diff += 360.0f;
  }

  return (diff < 0.0f) ? -diff : diff;
}

Direction_t System_DetectDirection(float my_heading, float other_heading)
{
  float diff = CalcHeadingDiff(my_heading, other_heading);

  if (diff <= HEADING_SAME_THRESHOLD)
  {
    return DIR_SAME;
  }

  if (diff >= (180.0f - HEADING_OPPOSITE_THRESHOLD))
  {
    return DIR_OPPOSITE;
  }

  return DIR_UNKNOWN;
}

/* ============ Shared TTC & Risk Evaluation ============ */

float System_CalcTTC(float distance, float relative_speed)
{
  if (relative_speed <= 0.0f)
  {
    return -1.0f;
  }

  return distance / relative_speed;
}

RiskLevel_t System_EvaluateRisk(float ttc, float warning_ttc, float critical_ttc)
{
  if (ttc < 0.0f)
  {
    return RISK_SAFE;
  }

  if (ttc <= critical_ttc)
  {
    return RISK_CRITICAL;
  }

  if (ttc <= warning_ttc)
  {
    return RISK_WARNING;
  }

  return RISK_SAFE;
}

// ====== UART RX Interrupt ======
void USART_RXCMP(void)
{
  uint8_t byte;
  USART_enumReceive(&USART_1, &byte);
  DSRC_RxCallback(byte);
}
