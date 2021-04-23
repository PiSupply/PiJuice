#/bin/bash

if [ "$#" -ne 2 ]; then
        echo "Usage flash.sh i2caddr binaryfile\nexample: flash.sh 0x14 PiJuice.bin"
        exit 1
fi

if ! [ -e "$2" ]; then
        echo "$2 not found!"
        exit 2
fi

# Attempt to enter bootloader mode
echo "\nAttempt to enter bootloader using address $1"
i2cset -y 1 $1 0xFE 0xFE01 w &> /dev/null

# Give it a while
sleep 1

# Grab eeprom data
echo "\nSaving eeprom data\n"
stm32flash -a 0x41 -S 0x0803B000 -r pj_eeprom_r.bin /dev/i2c-1

if [ $? -ne 0 ]; then
    echo "Could not enter booloader"
    exit ret
fi

# Burn the flash with new firmware, will erase entire chip
echo "\nWriting new firmware\n"
stm32flash -a 0x41 -w $2 /dev/i2c-1

# Burn eeprom data but do not erase flash
echo "\nRestoring eeprom data\n"
stm32flash -a 0x41 -e 0 -S 0x0803B000 -Rw pj_eeprom_r.bin /dev/i2c-1

echo "\nResetting PiJuice"

rm pj_eeprom_r.bin