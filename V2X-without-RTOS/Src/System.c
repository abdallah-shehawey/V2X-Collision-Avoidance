#include "../Inc/System.h"
#include "../Inc/DSRC.h"
#include "../Inc/ErrTypes.h"
#include "../Inc/GPIO_interface.h"
#include "../Inc/RCC_interface.h"
#include "../Inc/USART_intreface.h"
#include "../Inc/SYSTIC_interface.h"
#include "../Inc/NVIC_interface.h"

// ====== USART ======
USART_Handle_t USART_1 =
    {
        .Channel = USART_CHANNEL1,
        .BaudRate = 115200,
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

// ====== UART RX Interrupt ======
void USART_RXCMP(void)
{
  uint8_t byte;
  USART_enumReceive(&USART_1, &byte);
  DSRC_RxCallback(byte);
}
