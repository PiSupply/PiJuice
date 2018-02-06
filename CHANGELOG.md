## Version 1.2
Added packages to both Raspbian Jessie and Stretch

### Software
pijuice-gui (1.2-1) unstable; urgency=low

* src/pijuice_gui.py:
     - Fix layout for parameters labels on IO tab

## Version 1.1

### Software
pijuice-base (1.1-1) unstable; urgency=low

* pijuice.py:
    - Function for getting versions (OS, Firmware, Software)

* src/pijuice_sys.py:
    - Refactored GetFirmvareVersion to GetFirmwareVersion #34

pijuice-gui (1.1-1) unstable; urgency=low

* data/images/:
    - New icon for desktop menu

* src/pijuice_gui.py:
    - Use "clam" theme for GUI
    - Apply button in main window for saving settings
    - Adjust minimal window sizes
    - Change title for main settings window
    - Fix typo "Temerature sense" in Battery configuration tab
    - "Apply" button now applies settings from fields that use Enter key to update value
    - Various layout fixes for values to fit their elements

* src/pijuice_tray.py:
    - Add versions info to About menu in tray
    - Make tray menu entry Settings launch pijuice_gui in separate process

### Firmware
  
* data/firmware/PiJuice-V1.1_2018_01_15.elf.binary:
    - Wakeup on charge updated to be activated only if power source is present.
    - Further this enables wakeup after plugged if this parameter is set to 0.
    - Button wakeup functions power off and power off can now be assigned to arbitrary button events. Removed constrain to be assigned only to long_press2 for power off, and single_press for power on.
    - Added reset function that can be assigned to some of buttons and button events.
    - Added no battery turn on configuration.
    - Now it can be set whether or not user wants to turn on 5V rail as soon as power input is connected and there is no battery.
    - Added configuration for 2 IO ports. They can be set to analog input, digital input, digital output and pwm output.

## Version 1.0
pijuice (1.0) initial release
