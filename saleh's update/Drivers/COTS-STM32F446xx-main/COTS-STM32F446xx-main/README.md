# STM32F446RE Nucleo Board

## Overview

The STM32F446RE Nucleo board is a low-cost and easy-to-use development platform from STMicroelectronics. This board features the STM32F446RE microcontroller based on the high-performance ARM® Cortex®-M4 32-bit RISC core operating at up to 180 MHz.

## Key Features

- STM32F446RE microcontroller in LQFP64 package
- ARM® Cortex®-M4 core with FPU
- 180 MHz maximum frequency
- 512 KB Flash memory
- 512 KB SRAM
- Various interfaces and peripherals:
  - USB OTG FS
  - 3x SPI
  - 3x I2C
  - 4x USART
  - 2x UART
  - 3x ADC (16-channel)
  - 2x DAC channels
  - 17 Timers

## Development Environment

- Supported IDEs:
  - STM32CubeIDE
  - Keil MDK-ARM
  - IAR EWARM
- Programming/Debugging:
  - Integrated ST-LINK/V2-1 debugger/programmer
  - SWD interface

## Hardware Requirements

- STM32F446RE Nucleo Board
- USB Type-A to Mini-B cable
- Power supply: through USB bus or external 7V-12V

## Getting Started

1. **Install Development Environment**

   - Download and install your preferred IDE
   - Install STM32CubeMX for project initialization
   - Install necessary drivers

2. **Project Setup**

   - Create a new project in your IDE
   - Configure clock settings (max 180 MHz)
   - Configure desired peripherals
   - Generate initialization code

3. **Building and Flashing**
   - Build your project
   - Connect the board via USB
   - Flash the program using ST-LINK

## Pin Configuration

- User LED (LD2): PA5
- User Button (B1): PC13
- USART2:
  - TX: PA2
  - RX: PA3

## Documentation

For more detailed information, refer to:

- [STM32F446RE Datasheet](https://www.st.com/resource/en/datasheet/stm32f446re.pdf)
- [User Manual](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)
- [Reference Manual](https://www.st.com/resource/en/reference_manual/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)

## License

This project is released under the MIT License.
