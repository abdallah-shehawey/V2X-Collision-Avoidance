################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ThirdParty/Source/portable/MemMang/heap_4.c 

OBJS += \
./ThirdParty/Source/portable/MemMang/heap_4.o 

C_DEPS += \
./ThirdParty/Source/portable/MemMang/heap_4.d 


# Each subdirectory must supply rules for building sources it contributes
ThirdParty/Source/portable/MemMang/%.o ThirdParty/Source/portable/MemMang/%.su ThirdParty/Source/portable/MemMang/%.cyclo: ../ThirdParty/Source/portable/MemMang/%.c ThirdParty/Source/portable/MemMang/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F446xx -c -I../Core/Inc -I"D:/Grad_Project/APP/free/ThirdParty/Source/portable" -I"D:/Grad_Project/APP/free/ThirdParty/Source/include" -I"D:/Grad_Project/APP/free/ThirdParty/Source/portable/GCC/ARM_CM4F" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-ThirdParty-2f-Source-2f-portable-2f-MemMang

clean-ThirdParty-2f-Source-2f-portable-2f-MemMang:
	-$(RM) ./ThirdParty/Source/portable/MemMang/heap_4.cyclo ./ThirdParty/Source/portable/MemMang/heap_4.d ./ThirdParty/Source/portable/MemMang/heap_4.o ./ThirdParty/Source/portable/MemMang/heap_4.su

.PHONY: clean-ThirdParty-2f-Source-2f-portable-2f-MemMang

