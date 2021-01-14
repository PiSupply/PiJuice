# How to re-program the PiJuice EEPROM

From the [EEPROM datasheet](https://github.com/PiSupply/PiJuice/blob/master/Hardware/Datasheets/CAT24C32-D.PDF) you will see that the EEPROM has a write protect pin:

<img src="https://user-images.githubusercontent.com/3359418/104513836-35e66f80-55e8-11eb-96a1-e7ee70b6a2ee.png" width="50%" />

We have tried to make it as easy as possible to manage the programming of the EEPROM should you need to change or fix the contents, so we have made it so you can manage the write protect pin from the GUI or CLI interface. Basically, you need to remove the write protect on the EEPROM, so you can then flash it with the Raspberry Pi EEPROM tools. For that you can go to the [HAT config menu](https://github.com/PiSupply/PiJuice/blob/master/Software/README.md#pijuice-hat-general-config-menu), and tick the "ID EEPROM Write unprotect" box:

<img src="https://user-images.githubusercontent.com/16068311/35161230-7caa54d4-fd37-11e7-88cb-a76b2891af4d.png" width="50%" />

Then you can use the [Raspberry Pi EEPROM utils](https://github.com/raspberrypi/hats/tree/master/eepromutils) to program the EEPROM. You can also find a guide [here from the Raspberry Pi forum](https://www.raspberrypi.org/forums/viewtopic.php?t=108134).

You can use these commands:
```
# Clone Raspberry Pi HATs repo:
git clone https://github.com/raspberrypi/hats.git

# Fetch dtc tools and install
wget -c https://raw.githubusercontent.com/RobertCNelson/tools/master/pkgs/dtc.sh
chmod +x dtc.sh
./dtc.sh

# Enter eepromutils directory
cd hat/eepromutils

# Create the tools
make && sudo make install

# Create a blank eeprom file
dd if=/dev/zero ibs=1k count=4 of=blank.eep

# Compile the DT
wget -c https://github.com/PiSupply/PiJuice/blob/master/Firmware/EEPROM/pijuice.dts
sudo dtc -@ -I dts -O dtb -o pijuice.dtb pijuice.dts ; sudo chown pi:pi pijuice.dtb

# Get the settings file and make the .eep file
wget -c https://github.com/PiSupply/PiJuice/blob/master/Firmware/EEPROM/settings.txt
./eepmake settings.txt pijuice.eep pijuice.dtb

# Blank the EEPROM and then flash it
sudo ./eepflash -w -f=blank.eep -t=24c32
sudo ./eepflash -w -f=pijuice.eep -t=24c32
```
