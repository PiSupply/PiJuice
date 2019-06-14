#!/usr/bin/env bash

# Disables Raspberry Pi LEDs. Determines is it is a Zero board or not
revision=`cat /proc/cpuinfo | grep 'Revision' | awk '{print $3}'`
# Is it a Zero?
if [[ "9000" == ${revision:0:4} ]]; then
    echo 1 | sudo tee /sys/class/leds/led0/brightness
else
    echo 0 | sudo tee /sys/class/leds/led0/brightness
    echo 0 | sudo tee /sys/class/leds/led1/brightness
fi

# Disables the HDMI port
/opt/vc/bin/tvservice -off

# Runs the naturejuice script
/usr/bin/python3 /usr/local/bin/naturejuice.py -f -l

