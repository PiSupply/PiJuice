# PiJuice Software

Default battery profile is defined by dip switch position. On v1.1 it is position 01 for BP7X, on v1.0 version, it might be different, you can try different positions, and power circle pijuice to get updated.

It is not possible to detect battery not present when powered through on board usb micro, so it might show 0% only.

User functions are 4 digit binary coded and have 15 combinations, code 0 is USER_EVENT meant that it will not be processed by system task, but left to user and python API to manage it. I thought it is rare case that all 15 will be needed so on gui there is 8 (it will make big window also). However if someone needs more scripts it can be manualy added by editing config json file: /var/lib/pijuice/pijuice_config.JSON. Also all other configurations can be managed manually in this file if gui is not available. 

There are dependencies so you got error if not installed. Package is configured to intentionally  raise those errors so you can install it before. Usually if package is added to some server and configured as repository than you can use apt-get that will automatically install dependencies, and update. I can prepare packages to add on private server, do not know for official.

It is possible that tray do not update if you open config by right click on tray, but if you open from menu it should update.

## Software manual installation

Copy the deb package to the pi home and install the package.

`sudo dpkg -i ./pijuice_1.0-1_all.deb`

You will need reboot at this point so that the system tray app is refreshed.

To remove PiJuice you'll need to run:

`sudo dpkg -r pijuice`

### Build DEB-package manually
`./pckg-pijuice.sh`

or for the light version without GUI:

`./pckg-pijuice.sh --light`

*Note: You will need python-stdeb, dh-systemd and debhelper in order to be able to build.*

## GUI Menus

We have also taken a LOT of screenshots of all the different menu options etc to show you the full software. So lets get stuck in:

![Raspberry Pi Menu Entry for PiJuice Configuration Software](https://user-images.githubusercontent.com/16068311/33845719-3eab8388-de9c-11e7-8499-149e52e66b71.png "Raspberry Pi Menu Entry for PiJuice Configuration Software")

Raspberry Pi Menu Entry for PiJuice Configuration Software

As said in the above video, we have compiled the source code into a .deb Debian package file so it is super easy to install (instructions on how on the GitHub page linked below). Eventually, we will get this included in the official Raspbian package repositories so you will be able to install it very easily using the command "sudo apt-get install pijuice" just like you do with other software! Once installation is complete the software appears in the system menu under Menu -> Preferences -> PiJuice Configuration as you can see in the above image.

### System Tray

![System Tray](https://user-images.githubusercontent.com/16068311/33845725-43e1d0e6-de9c-11e7-921e-64c9cceb2c96.png "System Tray")

Once you load the software, you will see the PiJuice icon appear in the system tray, as above. This icon shows you the status of the PiJuice - charging from Pi, charging from PiJuice, running on battery as you have in a normal laptop computer. Additionally you can hover over it to tell you the charge level of the battery.

![bat-100](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/bat-90.png)
![bat-50](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/bat-50.png)
![bat-0](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/bat-0.png)

![bat-in-100](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/bat-in-90.png)
![bat-in-50](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/bat-in-50.png)
![bat-in-0](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/bat-in-0.png)

![bat-rpi-90](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/bat-rpi-90.png)
![bat-rpi-50](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/bat-rpi-50.png)
![bat-rpi-0](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/bat-rpi-0.png)

![no-bat-in-0](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/no-bat-in-0.png)
![no-bat-rpi-0](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/no-bat-rpi-0.png)

![connection-rror](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/connection-error.png)

You can also right click on this icon to load the configuration menu, instead of having to go to the menu as in the previous image.

## Software Configuration

### Main software menu, with no battery attached

![Main software menu, with no battery attached](https://user-images.githubusercontent.com/16068311/33845737-4caea1ae-de9c-11e7-848f-56b58cea3f23.png "Main software menu, with no battery attached")

This picture is how the PiJuice Configuration software looks when it loads up. This also shows some basic information about the battery charge, battery voltage, and where it is charging from....here is it showing 0% and a low voltage on the battery - because there is no battery installed! You can also see that it is charging from the Pi GPIO (meaning it is plugged in to the Pis microUSB) and it also shows the rail voltages and current draw over the GPIO pins. Below that is the PiJuice microUSB and as you can see in this screenshot that is not currently plugged in. There is a fault checker, a system switch state and also a link to a HAT config menu (more on that later! - see PiJuice HAT Configuration Menu screenshot).

### Main software menu, with battery attached

![Main software menu, with battery attached](https://user-images.githubusercontent.com/16068311/33845730-46f3ef4e-de9c-11e7-8a79-8bf64380da6f.png "Main software menu, with battery attached")

This screenshot shows the same menu as in the previous screenshot, the only difference being there is now a battery installed in the PiJuice.

### Wakeup Alarm Menu

![Wakeup Alarm Menu](https://user-images.githubusercontent.com/16068311/33845741-502a4252-de9c-11e7-9ec5-ddef3ec95d91.png "Wakeup Alarm Menu")

In this screenshot we have moved over to the Wakeup alarm tab of the config menu and as you can see this is an area where you can set schedules for the Pi to automatically wake up. This is useful for remote monitoring applications.

This feature will only work if you are either plugged in to the PiJuice microUSB / running on battery. If the battery is low and you are plugged in via the Raspberry Pis GPIO the only way to enable this feature is by soldering the optional "spring pin" that comes with the PiJuice HAT.

### System Task Menu

![System Task Menu](https://user-images.githubusercontent.com/16068311/33845745-53cb645e-de9c-11e7-836f-9601881a3fa9.png "System Task Menu")

Here we have the system task menu tab. This enables you to set the external watchdog timer - useful for remote applications where you can't come and do a hard-reset yourself if the Pi crashes or hangs. The PiJuice essentially monitors for a "heart beat" from the software - if it does not sense it after a defined period of time it automatically resets the Raspberry Pi. You can also set here wakeup on charge levels, minimum battery levels and voltages.

### System Events Menu

![System Events Menu](https://user-images.githubusercontent.com/16068311/33845747-56fca07a-de9c-11e7-8026-8de2b847f330.png "System Events Menu")

This is the system events menu tab. It allows you to trigger events for certain scenarios such as low charge, low voltage and more. Each paramater has a couple of preset options to choose from, and also you can select options from the "user scripts" tab which allows you to trigger your own custom scripts when certain system events occur for maximum flexibility.

### User scripts menu

![User scripts menu](https://user-images.githubusercontent.com/16068311/33845750-5a95cdf6-de9c-11e7-8840-b6dc7f079d35.png "User scripts menu")

This is the user scripts menu tab as we mentioned in the above screenshot description where you can add paths to custom scripts that you can trigger on events.

## PiJuice Configuration

### PiJuice HAT General Config Menu

![PiJuice HAT General Config Menu](https://user-images.githubusercontent.com/16068311/33845754-5eada1e8-de9c-11e7-8102-b933247a18ca.png "PiJuice HAT General Config Menu")

In the first config menu screenshot, we mentioned a button in the image that said "Configure HAT" - if you were to click on that button it would bring you to this PiJuice HAT general configuration menu. It allows you to configure a lot of hardware settings on the PiJuice HAT itself (as opposed to the previous menus which were actually configuring the software - hopefully that is not too confusing!)

This is the general tab, which allows you to select whether you have installed the spring pin / run pin and also the I2C addresses of the HAT and the RTC as well as changing the write protect on the eeprom and changing the actual physical I2C address of the eeprom. These eeprom features can be very useful if you want to stack another HAT on top of the PiJuice but still have that other HAT auto-configure itself.

### PiJuice HAT Config Buttons Menu

![PiJuice HAT Config Buttons Menu](https://user-images.githubusercontent.com/16068311/33845760-620ac668-de9c-11e7-8b88-7d514f925068.png "PiJuice HAT Config Buttons Menu")

Next we have the buttons menu - this configures the actions of the buttons on the PiJuice HAT (there are three surface mount buttons, one of which also has a 2 pin 2.54mm header so you can break out a button on a cable to the edge of a case or wherever you fancy).

There are a number of preset behaviours for the buttons - startup/shutdown etc and this menu also ties in to the "User Scripts" menu shown above meaning you can actually trigger your own custom scripts and events based on the press of one of these buttons very easily.

You can even trigger different events for a press, release, single press, double press and two lengths of long press - you can even configure the length of time these long presses would take before triggering the event. As you can see the first button is already configured for system power functionality and we would highly recommend that at least one of the buttons is configured to these settings or you may have issues turning your PiJuice on and off :-)

### PiJuice HAT Config LEDs Menu

![PiJuice HAT Config LEDs Menu](https://user-images.githubusercontent.com/16068311/33845764-650c1dee-de9c-11e7-9135-f489c402f59a.png "PiJuice HAT Config LEDs Menu")

Perhaps our favourite options menu is the LEDs menu - as with the buttons we have made these super versatile. They can have standard functions as displayed above, they can have preset functions or you can define custom ways for them to behave.

Who doesn't love blinkenlights!

### PiJuice HAT Config Battery Menu

![PiJuice HAT Config Battery Menu](https://user-images.githubusercontent.com/16068311/33845770-682d4354-de9c-11e7-8ad0-9149058f2266.png "PiJuice HAT Config Battery Menu")

The battery menu is a very important one. It basically allows you to set charge profiles for the PiJuice charge chip in order to correctly and efficiently charge the battery, correctly monitor the charge percentages and more. We have got a number of built in presets such as the ones that will come with the PiJuice by default (the BP7X) and all of the other ones we will supply. But as promised, there is also the ability to add your own custom charge profiles and even your own battery temperature sensor in order to increase the safety and efficiency of charging your batteries.

As previously mentioned, some of these are even hard coded into the firmware on the PiJuice which enables you to actually select profiles using the PiJuices on board DIP switch.

### PiJuice HAT Config Firmware Menu

![PiJuice HAT Config Firmware Menu](https://user-images.githubusercontent.com/16068311/33845771-6b55373a-de9c-11e7-9475-09b8dc8086dc.png "PiJuice HAT Config Firmware Menu")

Last but very much not least is the firmware menu. This allows you to update the firmware on the PiJuice MCU chip as and when necessary meaning we can actively improve the firmware and any updates or improvements we make in the future can be retrospectively applied to all PiJuice HATs!

*Note that the path to the binary file is fixed and it points to*
```text
/usr/share/pijuice/data/firmware/PiJuice.elf.binary
```
If you want to use the GUI to update the firmware you will have to override this file with the new one and make sure the filename is the same i.e. PiJuice.elf.binary
