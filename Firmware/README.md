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
./pijuiceboot 14 PiJuice-V1.0_2017_10_23.elf.binary
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

*Note: For old firmwares try to use the old firmware updater with a new firmware binary, or use ST debuger to erase flash.*