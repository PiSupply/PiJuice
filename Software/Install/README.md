# Copy deb package to pi home and run install
`sudo dpkg -i pijuice_1.0-1_all.deb`

OR (for version without GUI)

`sudo dpkg -i pijuice-light_1.0-1_all.deb`

# Need reboot to start Tray app

# Configuration GUI:
Start -> Preferences -> PiJuice

# Firmware update:
Configuration GUI -> HAT Tab -> Configure HAT -> Firmware Tab

Note: If there is old firmware in unit from december 2016, it needs to be updated with new firmware before installation using firmload tool

# For old firmware try to use old firmware updater with new firmware binary, or use ST debugger to erase flash

# Edit config.txt, add rtc enable line:
`dtoverlay=i2c-rtc,ds1339,wakeup-source`

# Remove installation with:
`sudo dpkg -r pijuice`
