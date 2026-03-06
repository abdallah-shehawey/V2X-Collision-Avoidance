################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/BSW_program.c \
../Src/BUZ_program.c \
../Src/DNPW_program.c \
../Src/EEBL_program.c \
../Src/EXTI_program.c \
../Src/FCW_program.c \
../Src/GPIO_prog.c \
../Src/IMA_program.c \
../Src/LED_prog.c \
../Src/MPU9250_program.c \
../Src/NVIC_program.c \
../Src/RCC_program.c \
../Src/SCB_program.c \
../Src/SDW_program.c \
../Src/SPI_program.c \
../Src/SYSCFG_program.c \
../Src/SYSTIC_program.c \
../Src/System.c \
../Src/TIM_program.c \
../Src/USART_program.c \
../Src/US_prog.c \
../Src/main.c \
../Src/syscalls.c 

C_DEPS += \
./Src/BSW_program.d \
./Src/BUZ_program.d \
./Src/DNPW_program.d \
./Src/EEBL_program.d \
./Src/EXTI_program.d \
./Src/FCW_program.d \
./Src/GPIO_prog.d \
./Src/IMA_program.d \
./Src/LED_prog.d \
./Src/MPU9250_program.d \
./Src/NVIC_program.d \
./Src/RCC_program.d \
./Src/SCB_program.d \
./Src/SDW_program.d \
./Src/SPI_program.d \
./Src/SYSCFG_program.d \
./Src/SYSTIC_program.d \
./Src/System.d \
./Src/TIM_program.d \
./Src/USART_program.d \
./Src/US_prog.d \
./Src/main.d \
./Src/syscalls.d 

OBJS += \
./Src/BSW_program.o \
./Src/BUZ_program.o \
./Src/DNPW_program.o \
./Src/EEBL_program.o \
./Src/EXTI_program.o \
./Src/FCW_program.o \
./Src/GPIO_prog.o \
./Src/IMA_program.o \
./Src/LED_prog.o \
./Src/MPU9250_program.o \
./Src/NVIC_program.o \
./Src/RCC_program.o \
./Src/SCB_program.o \
./Src/SDW_program.o \
./Src/SPI_program.o \
./Src/SYSCFG_program.o \
./Src/SYSTIC_program.o \
./Src/System.o \
./Src/TIM_program.o \
./Src/USART_program.o \
./Src/US_prog.o \
./Src/main.o \
./Src/syscalls.o 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DSTM32 -DSTM32F4 -DSTM32F446RETx -DNUCLEO_F446RE -c -I../Inc -I"D:/Grad_Project/APP/Barecpp/ThirdParty/Source/include" -I"D:/Grad_Project/APP/Barecpp/ThirdParty/Source/portable/GCC/ARM_CM4F" -I"D:/Grad_Project/APP/Barecpp/ThirdParty/Source/portable" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/BSW_program.cyclo ./Src/BSW_program.d ./Src/BSW_program.o ./Src/BSW_program.su ./Src/BUZ_program.cyclo ./Src/BUZ_program.d ./Src/BUZ_program.o ./Src/BUZ_program.su ./Src/DNPW_program.cyclo ./Src/DNPW_program.d ./Src/DNPW_program.o ./Src/DNPW_program.su ./Src/EEBL_program.cyclo ./Src/EEBL_program.d ./Src/EEBL_program.o ./Src/EEBL_program.su ./Src/EXTI_program.cyclo ./Src/EXTI_program.d ./Src/EXTI_program.o ./Src/EXTI_program.su ./Src/FCW_program.cyclo ./Src/FCW_program.d ./Src/FCW_program.o ./Src/FCW_program.su ./Src/GPIO_prog.cyclo ./Src/GPIO_prog.d ./Src/GPIO_prog.o ./Src/GPIO_prog.su ./Src/IMA_program.cyclo ./Src/IMA_program.d ./Src/IMA_program.o ./Src/IMA_program.su ./Src/LED_prog.cyclo ./Src/LED_prog.d ./Src/LED_prog.o ./Src/LED_prog.su ./Src/MPU9250_program.cyclo ./Src/MPU9250_program.d ./Src/MPU9250_program.o ./Src/MPU9250_program.su ./Src/NVIC_program.cyclo ./Src/NVIC_program.d ./Src/NVIC_program.o ./Src/NVIC_program.su ./Src/RCC_program.cyclo ./Src/RCC_program.d ./Src/RCC_program.o ./Src/RCC_program.su ./Src/SCB_program.cyclo ./Src/SCB_program.d ./Src/SCB_program.o ./Src/SCB_program.su ./Src/SDW_program.cyclo ./Src/SDW_program.d ./Src/SDW_program.o ./Src/SDW_program.su ./Src/SPI_program.cyclo ./Src/SPI_program.d ./Src/SPI_program.o ./Src/SPI_program.su ./Src/SYSCFG_program.cyclo ./Src/SYSCFG_program.d ./Src/SYSCFG_program.o ./Src/SYSCFG_program.su ./Src/SYSTIC_program.cyclo ./Src/SYSTIC_program.d ./Src/SYSTIC_program.o ./Src/SYSTIC_program.su ./Src/System.cyclo ./Src/System.d ./Src/System.o ./Src/System.su ./Src/TIM_program.cyclo ./Src/TIM_program.d ./Src/TIM_program.o ./Src/TIM_program.su ./Src/USART_program.cyclo ./Src/USART_program.d ./Src/USART_program.o ./Src/USART_program.su ./Src/US_prog.cyclo ./Src/US_prog.d ./Src/US_prog.o ./Src/US_prog.su ./Src/main.cyclo ./Src/main.d ./Src/main.o ./Src/main.su ./Src/syscalls.cyclo ./Src/syscalls.d ./Src/syscalls.o ./Src/syscalls.su

.PHONY: clean-Src

