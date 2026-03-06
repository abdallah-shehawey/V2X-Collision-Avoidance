################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ThirdParty/Source/portable/MemMang/heap_4.c 

C_DEPS += \
./ThirdParty/Source/portable/MemMang/heap_4.d 

OBJS += \
./ThirdParty/Source/portable/MemMang/heap_4.o 


# Each subdirectory must supply rules for building sources it contributes
ThirdParty/Source/portable/MemMang/%.o ThirdParty/Source/portable/MemMang/%.su ThirdParty/Source/portable/MemMang/%.cyclo: ../ThirdParty/Source/portable/MemMang/%.c ThirdParty/Source/portable/MemMang/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DSTM32 -DSTM32F4 -DSTM32F446RETx -DNUCLEO_F446RE -c -I../Inc -I"D:/Grad_Project/APP/Barecpp/ThirdParty/Source/include" -I"D:/Grad_Project/APP/Barecpp/ThirdParty/Source/portable/GCC/ARM_CM4F" -I"D:/Grad_Project/APP/Barecpp/ThirdParty/Source/portable" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-ThirdParty-2f-Source-2f-portable-2f-MemMang

clean-ThirdParty-2f-Source-2f-portable-2f-MemMang:
	-$(RM) ./ThirdParty/Source/portable/MemMang/heap_4.cyclo ./ThirdParty/Source/portable/MemMang/heap_4.d ./ThirdParty/Source/portable/MemMang/heap_4.o ./ThirdParty/Source/portable/MemMang/heap_4.su

.PHONY: clean-ThirdParty-2f-Source-2f-portable-2f-MemMang

