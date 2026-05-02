################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/BSW_program.c \
../Src/DNPW_program.c \
../Src/DSRC.c \
../Src/EEBL_program.c \
../Src/FCW_program.c \
../Src/GPIO_prog.c \
../Src/IMA_program.c \
../Src/NVIC_program.c \
../Src/RCC_program.c \
../Src/SYSTIC_program.c \
../Src/SafetyEngine_program.c \
../Src/System.c \
../Src/USART_program.c \
../Src/main.c \
../Src/syscalls.c \
../Src/sysmem.c 

OBJS += \
./Src/BSW_program.o \
./Src/DNPW_program.o \
./Src/DSRC.o \
./Src/EEBL_program.o \
./Src/FCW_program.o \
./Src/GPIO_prog.o \
./Src/IMA_program.o \
./Src/NVIC_program.o \
./Src/RCC_program.o \
./Src/SYSTIC_program.o \
./Src/SafetyEngine_program.o \
./Src/System.o \
./Src/USART_program.o \
./Src/main.o \
./Src/syscalls.o \
./Src/sysmem.o 

C_DEPS += \
./Src/BSW_program.d \
./Src/DNPW_program.d \
./Src/DSRC.d \
./Src/EEBL_program.d \
./Src/FCW_program.d \
./Src/GPIO_prog.d \
./Src/IMA_program.d \
./Src/NVIC_program.d \
./Src/RCC_program.d \
./Src/SYSTIC_program.d \
./Src/SafetyEngine_program.d \
./Src/System.d \
./Src/USART_program.d \
./Src/main.d \
./Src/syscalls.d \
./Src/sysmem.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DSTM32 -DSTM32F4 -DSTM32F446RETx -DNUCLEO_F446RE -c -I../Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/BSW_program.cyclo ./Src/BSW_program.d ./Src/BSW_program.o ./Src/BSW_program.su ./Src/DNPW_program.cyclo ./Src/DNPW_program.d ./Src/DNPW_program.o ./Src/DNPW_program.su ./Src/DSRC.cyclo ./Src/DSRC.d ./Src/DSRC.o ./Src/DSRC.su ./Src/EEBL_program.cyclo ./Src/EEBL_program.d ./Src/EEBL_program.o ./Src/EEBL_program.su ./Src/FCW_program.cyclo ./Src/FCW_program.d ./Src/FCW_program.o ./Src/FCW_program.su ./Src/GPIO_prog.cyclo ./Src/GPIO_prog.d ./Src/GPIO_prog.o ./Src/GPIO_prog.su ./Src/IMA_program.cyclo ./Src/IMA_program.d ./Src/IMA_program.o ./Src/IMA_program.su ./Src/NVIC_program.cyclo ./Src/NVIC_program.d ./Src/NVIC_program.o ./Src/NVIC_program.su ./Src/RCC_program.cyclo ./Src/RCC_program.d ./Src/RCC_program.o ./Src/RCC_program.su ./Src/SYSTIC_program.cyclo ./Src/SYSTIC_program.d ./Src/SYSTIC_program.o ./Src/SYSTIC_program.su ./Src/SafetyEngine_program.cyclo ./Src/SafetyEngine_program.d ./Src/SafetyEngine_program.o ./Src/SafetyEngine_program.su ./Src/System.cyclo ./Src/System.d ./Src/System.o ./Src/System.su ./Src/USART_program.cyclo ./Src/USART_program.d ./Src/USART_program.o ./Src/USART_program.su ./Src/main.cyclo ./Src/main.d ./Src/main.o ./Src/main.su ./Src/syscalls.cyclo ./Src/syscalls.d ./Src/syscalls.o ./Src/syscalls.su ./Src/sysmem.cyclo ./Src/sysmem.d ./Src/sysmem.o ./Src/sysmem.su

.PHONY: clean-Src

