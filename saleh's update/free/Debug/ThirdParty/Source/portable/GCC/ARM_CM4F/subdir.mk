################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ThirdParty/Source/portable/GCC/ARM_CM4F/port.c 

OBJS += \
./ThirdParty/Source/portable/GCC/ARM_CM4F/port.o 

C_DEPS += \
./ThirdParty/Source/portable/GCC/ARM_CM4F/port.d 


# Each subdirectory must supply rules for building sources it contributes
ThirdParty/Source/portable/GCC/ARM_CM4F/%.o ThirdParty/Source/portable/GCC/ARM_CM4F/%.su ThirdParty/Source/portable/GCC/ARM_CM4F/%.cyclo: ../ThirdParty/Source/portable/GCC/ARM_CM4F/%.c ThirdParty/Source/portable/GCC/ARM_CM4F/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F446xx -c -I../Core/Inc -I"D:/Grad_Project/APP/free/ThirdParty/Source/portable" -I"D:/Grad_Project/APP/free/ThirdParty/Source/include" -I"D:/Grad_Project/APP/free/ThirdParty/Source/portable/GCC/ARM_CM4F" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-ThirdParty-2f-Source-2f-portable-2f-GCC-2f-ARM_CM4F

clean-ThirdParty-2f-Source-2f-portable-2f-GCC-2f-ARM_CM4F:
	-$(RM) ./ThirdParty/Source/portable/GCC/ARM_CM4F/port.cyclo ./ThirdParty/Source/portable/GCC/ARM_CM4F/port.d ./ThirdParty/Source/portable/GCC/ARM_CM4F/port.o ./ThirdParty/Source/portable/GCC/ARM_CM4F/port.su

.PHONY: clean-ThirdParty-2f-Source-2f-portable-2f-GCC-2f-ARM_CM4F

