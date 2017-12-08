# Update PiJuice firmware

# usage:
./pijuiceboot i2c_address path_to_firmware_binary

# example:
./pijuiceboot 14 /home/pi/PiJuice.elf.binary


- do we have source code for the firmware that we could share with people for custom applications?
Firmware source code is not intended to be shared with users, because it requires deep understanding of hardware and programming to do customization, and may even harm something or damage battery in case of mistakes.
There is I2C command interface provided through python API that install together with configuration GUI intended for users to do custom applications on Linux side. There is pijucetest.py that can be used as example how to do programming in python and communicate data with Pijuice.

In this directory there is python script called pijuicetest.py:
https://github.com/PiSupply/PiJuice/tree/master/Software/Test

copy it to /home/pi and run:
pijuicetest.py

if firmware is ok it will print data read from mcu on screen, like battery voltage, charge, battery profile...
try it to test firmware
