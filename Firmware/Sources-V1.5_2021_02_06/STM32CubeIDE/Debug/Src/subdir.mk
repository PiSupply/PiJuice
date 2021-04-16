################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/adc.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/analog.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/ave_filter.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/battery.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/button.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/charger_bq2416x.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/command_server.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/config_switch_resistor.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/crc.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/e2.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/eeprom.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/fuel_gauge_lc709203f.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/hostcomms.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/i2cdrv.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/io_control.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/iodrv.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/led.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/load_current_sense.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/nv.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/osloop.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/power_management.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/power_source.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/rtc_ds1339_emu.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/taskman.c \
C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/util.c 

OBJS += \
./Src/adc.o \
./Src/analog.o \
./Src/ave_filter.o \
./Src/battery.o \
./Src/button.o \
./Src/charger_bq2416x.o \
./Src/command_server.o \
./Src/config_switch_resistor.o \
./Src/crc.o \
./Src/e2.o \
./Src/eeprom.o \
./Src/fuel_gauge_lc709203f.o \
./Src/hostcomms.o \
./Src/i2cdrv.o \
./Src/io_control.o \
./Src/iodrv.o \
./Src/led.o \
./Src/load_current_sense.o \
./Src/nv.o \
./Src/osloop.o \
./Src/power_management.o \
./Src/power_source.o \
./Src/rtc_ds1339_emu.o \
./Src/taskman.o \
./Src/util.o 

C_DEPS += \
./Src/adc.d \
./Src/analog.d \
./Src/ave_filter.d \
./Src/battery.d \
./Src/button.d \
./Src/charger_bq2416x.d \
./Src/command_server.d \
./Src/config_switch_resistor.d \
./Src/crc.d \
./Src/e2.d \
./Src/eeprom.d \
./Src/fuel_gauge_lc709203f.d \
./Src/hostcomms.d \
./Src/i2cdrv.d \
./Src/io_control.d \
./Src/iodrv.d \
./Src/led.d \
./Src/load_current_sense.d \
./Src/nv.d \
./Src/osloop.d \
./Src/power_management.d \
./Src/power_source.d \
./Src/rtc_ds1339_emu.d \
./Src/taskman.d \
./Src/util.d 


# Each subdirectory must supply rules for building sources it contributes
Src/adc.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/adc.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/adc.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/analog.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/analog.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/analog.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/ave_filter.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/ave_filter.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/ave_filter.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/battery.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/battery.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/battery.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/button.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/button.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/button.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/charger_bq2416x.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/charger_bq2416x.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/charger_bq2416x.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/command_server.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/command_server.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/command_server.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/config_switch_resistor.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/config_switch_resistor.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/config_switch_resistor.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/crc.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/crc.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/crc.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/e2.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/e2.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/e2.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/eeprom.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/eeprom.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/eeprom.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/fuel_gauge_lc709203f.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/fuel_gauge_lc709203f.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/fuel_gauge_lc709203f.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/hostcomms.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/hostcomms.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/hostcomms.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/i2cdrv.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/i2cdrv.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/i2cdrv.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/io_control.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/io_control.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/io_control.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/iodrv.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/iodrv.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/iodrv.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/led.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/led.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/led.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/load_current_sense.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/load_current_sense.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/load_current_sense.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/nv.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/nv.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/nv.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/osloop.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/osloop.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/osloop.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/power_management.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/power_management.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/power_management.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/power_source.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/power_source.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/power_source.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/rtc_ds1339_emu.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/rtc_ds1339_emu.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/rtc_ds1339_emu.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/taskman.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/taskman.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/taskman.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Src/util.o: C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Src/util.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F030xC -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"C:/Repository/PiJuice/Firmware/Sources-V2.0_2021_04_16/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Src/util.d" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

