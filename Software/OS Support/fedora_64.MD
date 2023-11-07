# Installing PiJuice Software on Fedora 64-bit

## Install Fedora onto the Raspberry Pi
https://docs.fedoraproject.org/en-US/quick-docs/raspberry-pi/

## Configure firmware device tree
While Fedora 39 was in beta kernel device tree, which is the default, was not working and this project required the install be switched to firmware device tree. The issue was resolved just before the Fedora 39 release and now the default kernel device tree works.

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

4. Reboot and run PiJuice CLI or PiJuice GUI when on Desktop:
```
sudo reboot
```

## Run PiJuice Software
Once rebooted, find the tray in the upper right. Right-click to launch the settings gui. Run the PiJuice Settings application from the GNOME menu. From the command line launch:

`pijuice_cli` or `pijuice_gui`
