# Update PiJuice firmware

There are two ways in which you can upgrade the firmware for the PiJuice. You can use the command line tool provided here or refer to the [Software section](https://github.com/PiSupply/PiJuice/tree/master/Software) to find out how to perform a firmware update via the GUI.

pijuiceboot needs to be made executable by running:
```bash
chmod 755 pijuiceboot
```
or
```bash
chmod a+x pijuiceboot
```
## Usage:
```text
./pijuiceboot i2c_address path_to_firmware_binary
```

### Example:
```bash
./pijuiceboot 14 PiJuice-V1.1_2018_01_15.elf.binary
```

The firmware source code is not intended to be made openly available as it requires deep understanding of the hardware design and the programming involved if you wanted to customise it. Making changes to the firmware may even harm components or damage the battery.
The majority of customisation can anyway be done by using the I2C command interface provided through the Python API that is installed together with configuration GUI. This is the best approach for end users wishing to customise further PiJuice for their needs.

pijucetest.py is an example made available to demonstrate how to communicate with PiJuice using Python.

The [script](https://github.com/PiSupply/PiJuice/blob/master/Software/Test/pijuicetest.py) can be found under /home/pi/PiJuice/Software/Test/pijuicetest.py

Make a copy into your home folder and run it to see a display a demo:
```bash
cp /home/pi/PiJuice/Software/Test/pijuicetest.py /home/pi/.
cd ~
python pijuicetest.py
```
The script should display the firmware status, it will print data read from the MCU on screen like battery voltage, charge, battery profile, etc.

*Note: For old firmwares (prior to V1.0_2017_08_15) try to use the old firmware updater with a new firmware binary, or use ST debuger to erase flash.*

## FAQ

**PiJuice bricked after successful firmware update**

To recover from this issue follow these steps:

1. Make sure that you Raspberry Pi is shutdown
2. Disconnect all power to the PiJuice or Raspberry Pi and remove the PiJuice battery
3. Hold down SW3 button on the PiJuice and connect power to the Raspberry Pi
4. Release SW3 button after few seconds to enter bootloader mode\
**NOTE: LEDs on the PiJuice will remain off**
5. Stop the pijuice service with `sudo systemctl stop pijuice.service`
6. Download the Github repository `sudo git clone https://github.com/PiSupply/PiJuice.git`
7. Enter the Firmware directory `cd PiJuice/Firmware/`
8. Run `pijuiceboot 14 PiJuice-V1.2_2018_05_02.elf.binary`
9. Wait until the Firmware has successfully flashed
10. Shutdown the Raspberry Pi and remove power
11. Connect the battery to the PiJuice and boot up as normal
