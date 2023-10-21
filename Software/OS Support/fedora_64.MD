# Installing PiJuice Software on Fedora 64-bit

## Install Fedora onto the Raspberry Pi
https://docs.fedoraproject.org/en-US/quick-docs/raspberry-pi/

## Configure firmware device tree
Currently Fedora is configured for kernel device tree which results in a different enumeration of the i2c bus. Also it requires poking sysfs to instantiate rtc_ds1307 module.

1. remove or rename the file /boot/dtb (it's a symlink so it's safe to delete):
```
sudo mv /boot/dtb /boot/dtb.disable
```

2. configure Fedora not to reenable kernel DT on a kernel upgrade
```
echo "FirmwareDT=True" | sudo tee -a /etc/u-boot.conf
```

3. configure the firmware DT to load the ds1307 with i2c
```
echo -e "\n# pijuice rtc\ndtoverlay=i2c-rtc,ds1307" | sudo tee -a /boot/efi/config.txt
```

## Install PiJuice Softaware
The icon-tray app requires GNOME 45 to show in the upper right of the toolbar. Fedora 39 has GNOME 45. 

Currently the software is forked and modified for Fedora here:
https://github.com/komacke/PiJuice

1. Enble the Fedora COPR repository with the PiJuice packages:
```
sudo dnf copr enable komacke/pijuice-hat
```

2. Install the pijuice-base package to get the service and cli utlities:
```
sudo dnf install pijuice-base
```

3. Install the pijuice-gui package to get the gui and tray:
```
sudo dnf install pijuice-gui
```

4. Reboot and run PiJuice CLI or PiJuice GUI when on Desktop:\
```
sudo reboot
```

## Run PiJuice Software
Once rebooted, find the tray in the upper right. Right-click to launch the settings gui. Run the PiJuice Settings application from the GNOME menu. From the command line launch:

`pijuice_cli` or `pijuice_gui`
