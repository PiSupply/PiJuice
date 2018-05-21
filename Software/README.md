# PiJuice Software

## Software installation
### Automated process
At the command line simply type:
```bash
sudo apt-get install pijuice-gui
```
PiJuice depends on other libraries to be present, the package is designed to raise them and let apt-get resolve them.

If you wish to install just the light version of PiJuice with no GUI:
```bash
sudo apt-get install pijuice-base
```
This is particularly indicated for Raspbian Lite or an headless installation.

Note: Users using Debian Jessie must run `sudo apt-get update` & `sudo apt-get upgrade` followed by `sudo apt-get install pijuice-gui` or `sudo apt-get install pijuice-base`

### Manual process

Copy either of the deb packages to the pi home and install it.

For example for the full version with GUI:

`sudo dpkg -i ./pijuice-gui_1.1-1_all.deb`

For the light version:

`sudo dpkg -i ./pijuice-base_1.1-1_all.deb`

Should the installation complain about missing dependencies you need to sort them first and try with the installation once again.

You will need to reboot at this point so that the system tray app is refreshed.

To remove PiJuice you'll need to run:

`sudo dpkg -r pijuice`

#### Build DEB-package manually
`./pckg-pijuice.sh`

or for the light version without GUI:

`./pckg-pijuice.sh --light`

*Note: You will need python-stdeb, dh-systemd and debhelper in order to be able to build.*

## GUI Menus

We have also taken a LOT of screenshots of all the different menu options etc to show you the full software. So lets get stuck in:

![Raspberry Pi Menu Entry for PiJuice Settings Software](https://user-images.githubusercontent.com/16068311/35343402-7a362126-0122-11e8-9961-0b7013453f3f.png "Raspberry Pi Menu Entry for PiJuice Settings Software")

We have compiled the source code into a .deb Debian package file so it is super easy to install. Once installation is complete the software appears in the system menu under Menu -> Preferences -> PiJuice Settings as you can see in the above image.

### System Tray

![System Tray](https://user-images.githubusercontent.com/16068311/33845725-43e1d0e6-de9c-11e7-921e-64c9cceb2c96.png "System Tray")

Once you load the software, you will see the PiJuice icon appear in the system tray, as above. This icon shows you the status of the PiJuice - charging from Pi, charging from PiJuice, running on battery as you have in a normal laptop computer. Additionally you can hover over it to tell you the charge level of the battery.

*Note that it is not possible to detect battery not present when powered through on board USB micro, so it might show 0% only.*

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

## PiJuice Settings

### Main software menu

![Main software menu, with battery attached](https://user-images.githubusercontent.com/16068311/35161234-7d125174-fd37-11e7-9383-e2e80044258d.png "Main software menu, with battery attached")

This picture is how the PiJuice Settings software looks when it loads up. This also shows some basic information about the battery charge, battery voltage, and where it is charging from....here is it showing 97% and a high voltage on the battery. You can also see that it is charging from the Pi GPIO (meaning it is plugged in to the Pis microUSB) and it also shows the rail voltages and current draw over the GPIO pins. Below that is the PiJuice microUSB and as you can see in this screenshot that is plugged in. There is a fault checker, a system switch state and also a link to a HAT config menu (more on that later! - see PiJuice HAT Configuration Menu screenshot).

- **Battery:**
  - Battery Charge - 0-100%
  - Battery Voltage - Current battery voltage level
  - Battery Status:
    - **NOT_PRESENT** - Battery is not detected or not installed
    - **CHARGING_FROM_IN** - Battery is charging from PiJuice USB power connector
    - **CHARGING_FROM_5V_IO** - Battery is charging from GPIO pin (Will occur if powered through Raspberry Pi power connector)
    - **NORMAL** - Battery is present but not charging
- **GPIO power input:**
  - Voltage provided/received from the GPIO pins
  - Amperage provided/received from the GPIO pins
  - GPIO Input Status (Powered from Raspberry Pi)
    - **NOT_PRESENT** - No power supply connected to GPIO pins (i.e Raspberry Pi)
    - **BAD** - GPIO power is bad, find an alternative power supply with a higher rating
    - **WEAK** - GPIO power is weak i.e. power supply cannot charge the PiJuice and provide power to the Raspberry Pi
    - **PRESENT** - GPIO power is good
- **USB Micro power input:** - PiJuice Micro USB input status
  - **NOT_PRESENT** - Power supply is not connected to the PiJuice micro USB connector
  - **BAD** - Power supply is connected but is not providing enough power
  - **WEAK** - Power supply is connected but is weak
  - **PRESENT** - Power supply is connected and is providing good power to the PiJuice
  - **FAULT:** - Displays any faults
- **System Switch:** for use with VSYS on J3 to provide power to external devices
  - **off** - VSYS pin is off
  - **500mA** - VSYS pin provides up to 500mA of power
  - **2100mA** - VSYS pin provides up to 2100mA of power

### Wakeup Alarm Menu

![Wakeup Alarm Menu](https://user-images.githubusercontent.com/16068311/35161225-7bd18140-fd37-11e7-8889-e6715023b334.png "Wakeup Alarm Menu")

In this screenshot we have moved over to the Wakeup alarm tab of the config menu and as you can see this is an area where you can set schedules for the Pi to automatically wake up. This is useful for remote monitoring applications.

This feature will only work if you are either plugged in to the PiJuice microUSB / running on battery. If the battery is low and you are plugged in via the Raspberry Pis GPIO the only way to enable this feature is by soldering the optional "spring pin" that comes with the PiJuice HAT (See the [hardware section](https://github.com/PiSupply/PiJuice/tree/master/Hardware#unpopulated) for further details).

### System Task Menu

![System Task Menu](https://user-images.githubusercontent.com/16068311/35161236-7d4e6f56-fd37-11e7-9209-7943e88a76d5.png "System Task Menu")

Here we have the system task menu tab. This enables you to set the external watchdog timer - useful for remote applications where you can't come and do a hard-reset yourself if the Pi crashes or hangs. The PiJuice essentially monitors for a "heart beat" from the software - if it does not sense it after a defined period of time it automatically resets the Raspberry Pi. You can also set here wakeup on charge levels, minimum battery levels and voltages.

The watchdog timer has a configurable time-out. It defines the time after which it will power cycle if it doesn't receive a heartbeat signal. The time step is in minutes so the minimum time-out period is one minute and the maximum is 65535 minutes. The number can be any whole number between one and 65535. If you set the time to zero the watchdog timer will be disabled.

* **System task enabled** - Check this box to enable one or more of the following options:
* **Watchdog** - You can set a delay here in minutes (maximum of 65535) as to when to power cycle the Raspberry Pi when a heartbeat signal is not detected anymore. Usually this would occur when the system has crashed.
* **Wakeup on charge** - Set a percentage battery value when to wakeup the Raspberry Pi whilst on charge. Usually this value would be high, between 90-100%. This is usually used in-conjunction with "Minimum charge".
* **Minimum charge** - Set a minimum battery percentage level to safely shutdown the Raspberry Pi when the battery is below this value. Low values should be typically between 5-10%. NOTE: The type of system shutdown can be set under "System Events" under "Low charge" menu.
* **Minimum battery voltage** - Set a minimum battery voltage level to safely shutdown the Raspberry Pi when below the set level. Note: The type of system shutdown can be set under "System Events" under "Low battery voltage" menu.

### System Events Menu

![System Events Menu](https://user-images.githubusercontent.com/16068311/35161235-7d31d544-fd37-11e7-92b4-dc0ccab55c56.png "System Events Menu")

This is the system events menu tab. It allows you to trigger events for certain scenarios such as low charge, low voltage and more. Each parameter has a couple of preset options to choose from, and also you can select options from the "user scripts" tab which allows you to trigger your own custom scripts when certain system events occur for maximum flexibility.

* **Low charge**. System task monitors charge level and generates this event when charge drops below configurable threshold as set in "System Task"
* **Low battery voltage**. This event is generated when battery voltage drops below configurable threshold as set in "System Task"
* **No power**. System task generates this event when power source disappears and system is powered only with energy from battery.
* **Watchdog reset**. If watchdog reset happened for some reason PiJuice raises watchdog reset fault flag that system task can detect immediately after boot.
* **Button power off**. This event is raised after boot if there was power off triggered by button press.
* **Forced power off**. If there was forced power off caused by loss of energy (battery voltage approached cut-off threshold), PiJuice raises forced power off fault flag that system task can detect immediately after boot.
* **Forced sys power off**. This event is raised if there was forced system switch turn off caused by loss of energy.

#### System Functions

* **NO_FUNC** - Do nothing when system event is triggered.
* **SYS_FUNC_HALT** - System is halted
* **SYS_FUNC_HALT_POW_OFF** - System halts and 5V power regulator and system switch are set to off
* **SYS_FUNC_SYS_OFF_HALT** - System is halted and system switch is set to off and system halts
* **SYS_FUNC_REBOOT** - System reboots
* **USER_EVENT** - Script will not be processed by system task
* **USER_FUNCX** - Run a custom script as set in "User Scripts"

**NOTE:** SYS_FUNC_HALT_POW_OFF still provides power to the Raspberry Pi for a further 60 seconds after shutdown

### User Scripts menu

![User scripts menu](https://user-images.githubusercontent.com/16068311/35161237-7d653b8c-fd37-11e7-9a1e-be8b71e27a27.png "User scripts menu")

This is the user scripts menu tab as we mentioned in the above screenshot description where you can add paths to custom scripts that you can trigger on events.

User scripts can be assigned to user functions called by system task when configured event arise. This should be non-blocking callback function that implements customized system functions or event logging.

User functions are 4 digit binary coded and have 15 combinations, code 0 is USER_EVENT meant that it will not be processed by system task, but left to user and python API to manage it. We thought that it should be a rare case that all 15 combinations would be needed on the GUI so we only provided 8. However if someone needs more scripts they can be manually added by editing config json file: /var/lib/pijuice/pijuice_config.JSON as explained in the [JSON file Section](https://github.com/PiSupply/PiJuice/blob/master/Software/README.md#adding-user_func-from-9-to-15)

**NOTE:** In order for your user script to run you must make sure that it is executable and that system task is enabled in the **System Task** menu. If you are also assigning a user function to a button then you must also assign the user function under the **Buttons** tab in **PiJuice HAT Configuration**. To make your script executable you can do so from the command line with the following command:

```bash
chmod +x user_script.py
```
**NOTE:** Scripts executed by the pijuice.service will run as root not pi

Alternatively you can simply add the following in the User Scripts tab under the function:

```text
python user_script.py
```


## PiJuice Configuration

### PiJuice HAT General Config Menu

![PiJuice HAT General Config Menu](https://user-images.githubusercontent.com/16068311/35161230-7caa54d4-fd37-11e7-88cb-a76b2891af4d.png "PiJuice HAT General Config Menu")

In the first config menu screenshot, we mentioned a button in the image that said "Configure HAT" - if you were to click on that button it would bring you to this PiJuice HAT general configuration menu. It allows you to configure a lot of hardware settings on the PiJuice HAT itself (as opposed to the previous menus which were actually configuring the software - hopefully that is not too confusing!)

This is the general tab, which allows you to select whether you have installed the spring pin / run pin and also the I2C addresses of the HAT and the RTC as well as changing the write protect on the eeprom and changing the actual physical I2C address of the eeprom. These eeprom features can be very useful if you want to stack another HAT on top of the PiJuice but still have that other HAT auto-configure itself.

* **Inputs precedence**: Selects what power input will have precedence for charging and supplying VSYS output when both are present, HAT USB Micro Input, GPIO 5V Input. 5V_GPIO selected by default.
* **GPIO Input Enabled**: Enables/disables powering HAT from 5V GPIO Input. Enabled by default.
* **USB Micro current limit**: Selects maximum current that HAT can take from USB Micro connected power source. 2.5A selected by default.
* **USB Micro IN DPM**: Selects minimum voltage at USB Micro power input for Dynamic Power Management Loop. 4.2V set by default.
* **No battery turn on**: If enabled pijuice will automatically power on 5V rail and trigger wake-up as soon as power appears at USB Micro Input and there is no battery. Disabled by default.
* **Power regulator mode**: Selects power regulator mode. POWER_SOURCE_DETECTION by default.


*Note: Using the "Reset to default configuration" will restore the board to its default settings and for a short while the GUI will report "COMMUNICATION_ERROR"*

### PiJuice HAT Config Buttons Menu

![PiJuice HAT Config Buttons Menu](https://user-images.githubusercontent.com/16068311/35161227-7c2194be-fd37-11e7-90ee-521a7d65813f.png "PiJuice HAT Config Buttons Menu")

Next we have the buttons menu - this configures the actions of the buttons on the PiJuice HAT (there are three surface mount buttons, one of which also has a 2 pin 2.54mm header so you can break out a button on a cable to the edge of a case or wherever you fancy).

There are a number of preset behaviours for the buttons - startup/shutdown etc and this menu also ties in to the "User Scripts" menu shown above meaning you can actually trigger your own custom scripts and events based on the press of one of these buttons very easily.

You can even trigger different events for a press, release, single press, double press and two lengths of long press - you can even configure the length of time these long presses would take before triggering the event. As you can see the first button is already configured for system power functionality and we would highly recommend that at least one of the buttons is configured to these settings or you may have issues turning your PiJuice on and off :-)

#### Button events:
* **PRESS**. Triggered immediately after button is pressed
* **RELEASE**: Triggered immediately after button is released
* **SINGLE PRESS**: Triggered if button is released in time less than configurable timeout after button press.
* **DOUBLE PRESS**: Triggered if button is double pressed in time less than configurable timeout.
* **LONG PRESS 1**: Triggered if button is hold pressed hold for configurable time period 1.
* **LONG PRESS 2**: Triggered if button is hold pressed hold for configurable time period 2.

Button events can be configured to trigger predefined or user functions.

#### Hardware functions
* **POWER ON**: This function will wake-up system. 5V regulator (5V GPIO rail) will be turned on if was off.
* **POWER OFF**: 5V regulator (5V GPIO rail) turns off.
* **RESET**: If run pin is installed then reset is triggered by run signal activation. If run pin is not installed rest is done by power circle at 5V GPIO rail if power source is not present.


### PiJuice HAT Config LEDs Menu

![PiJuice HAT Config LEDs Menu](https://user-images.githubusercontent.com/16068311/35161232-7cdfe4aa-fd37-11e7-9249-f02f89ea2587.png "PiJuice HAT Config LEDs Menu")

Perhaps our favourite options menu is the LEDs menu - as with the buttons we have made these super versatile. They can have standard functions as displayed above, they can have preset functions or you can define custom ways for them to behave.

Each LED can be assigned to predefined predefined function or configured for user software control as User LED.
* **CHARGE STATUS**. LED is configured to signal current charge level of battery. For level <= 15% red with configurable brightness. For level > 15% and level <=50% mix of red and green with configurable brightness. For level > 50% green with configurable brightness. When battery is charging blinking blue with configurable brightness is added to current charge level color. For full buttery state blue component is steady on.

**Red** - R parameter defines color component level of red below 15%
**Green** - G parameter defines color component charge level over 50%
**Blue** - B parameter defines color component for charging (blink) and fully charged states (constant)

**NOTE:** Red LED and Green LED will show the charge status between 15% - 50%

* **USER LED**. When LED is configured as User LED it can be directly controlled with User software via command interface. Initial PiJuice power on User LED state is defined with R, G, and B brightness level parameters.

### PiJuice HAT Config Battery Menu

![PiJuice HAT Config Battery Menu](https://user-images.githubusercontent.com/16068311/35161226-7c026f30-fd37-11e7-897a-a470e06a4c8b.png "PiJuice HAT Config Battery Menu")

The battery menu is a very important one. It basically allows you to set charge profiles for the PiJuice charge chip in order to correctly and efficiently charge the battery, correctly monitor the charge percentages and more. We have got a number of built in presets such as the ones that will come with the PiJuice by default (the BP7X) and all of the other ones we will supply. But as promised, there is also the ability to add your own custom charge profiles and even your own battery temperature sensor in order to increase the safety and efficiency of charging your batteries.

As previously mentioned, some of these are even hard coded into the firmware on the PiJuice which enables you to actually select profiles using the PiJuices on board DIP switch.

More information on the default profiles and how to created additional ones can be found in the [Hardware Section](https://github.com/PiSupply/PiJuice/tree/master/Hardware#battery-profiles)

### PiJuice HAT Config IO Menu
![PiJuice HAT Config IO Menu](https://user-images.githubusercontent.com/16068311/35161231-7cc2820c-fd37-11e7-875b-b80b18c3a6ab.png "PiJuice HAT Config IO Menu")

This Tab provides configuration of two pins IO port provided from HAC microcontroller at P3 Header.
Modes selection box provides to program IO pin to one of predefined modes:
* **NOT_USED**: Set IO pin in neutral configuration (passive input).
* **ANALOG_IN**: Set IO pin in analog to digital converter mode. In this mode Value can be read with status function GetIoAnalogInput(). Pull has no effect in this mode.
* **DIGITAL_IN**: Set IO pin in digital input mode. Pull in this mode cen be set to NO_PULL, PULLDOWN or PULLUP. Use status function SetIoDigitalOutput() to read input value dynamically.
* **DIGITAL_OUT_PUSHPULL**: Set IO pin in digital output mode with push-pull driver topology. Pull in this mode should be set to NO_PULL. Initial value can be set to 0 or 1. Use status function SetIoDigitalOutput() to control output value dynamically.
* **DIGITAL_IO_OPEN_DRAIN**: Set IO pin in digital output mode with open-drain driver topology. Pull in this mode can be set to NO_PULL, PULLDOWN or PULLUP. Initial value can be set to 0 or 1. Use status function SetIoDigitalOutput() to control output value dynamically.
* **PWM_OUT_PUSHPULL**: Set IO pin to PWM output mode with push-pull driver topology. Pull in this mode should be set to NO_PULL. Period [us] box sets period in microseconds in range [2, 131072] with 2us resolution. Set initial duty_circle in range [0, 100]. Use status function SetIoPWM() to control duty circle dynamically.
* **PWM_OUT_OPEN_DRAIN**: Set IO pin to PWM output mode with open-drain driver topology. Pull in this mode can be set to NO_PULL, PULLDOWN or PULLUP. Period [us] box sets period in microseconds in range [2, 131072] with 2us resolution. Set initial duty_circle in range [0, 100]. Use status function SetIoPWM() to control duty circle dynamically.

Click Apply button to save new settings.


### PiJuice HAT Config Firmware Menu

![PiJuice HAT Config Firmware Menu](https://user-images.githubusercontent.com/16068311/35274166-0879d4a0-0033-11e8-8d49-628c27d727f8.png "PiJuice HAT Config Firmware Menu")

Last but very much not least is the firmware menu. This allows you to update the firmware on the PiJuice MCU chip as and when necessary meaning we can actively improve the firmware and any updates or improvements we make in the future can be retrospectively applied to all PiJuice HATs!

*Note that the PiJuice package you installed comes with a default firmware located at the path below:*
```text
/usr/share/pijuice/data/firmware/
```

the filename would look like `PiJuice-V1.1-2018_01_15.elf.binary`

If you want to use the GUI to update the firmware to a more recent version you will have to override this file with the new one that you can download from our [Firmware section](https://github.com/PiSupply/PiJuice/tree/master/Firmware).

*Remember though that the firmware we provide in the software package you've obtained from either APT or Github is generally the only one you should ever use for that specific version of Software release, therefore only update the firmware if the GUI reports that the firmware is not up to date or if we instruct you to do so.*

During the update the window may become unresponsive. **Wait until the update is finished** before you continue with anything else.

## JSON configuration file
Changes made on tabs "System Task", "System Events" and "User Scripts" on the main windows will be saved on a JSON file.

`/var/lib/pijuice/pijuice_config.JSON`

here is an example of a configuration.

```text
{
  "system_events": {
    "low_battery_voltage": {
      "function": "SYS_FUNC_HALT",
      "enabled": true
    },
    "low_charge": {
      "function": "NO_FUNC",
      "enabled": true
    },
    "button_power_off": {
      "function": "USER_FUNC1",
      "enabled": true
    },
    "forced_power_off": {
      "function": "USER_FUNC2",
      "enabled": true
    },
    "no_power": {
      "function": "SYS_FUNC_HALT_POW_OFF",
      "enabled": true
    },
    "forced_sys_power_off": {
      "function": "USER_FUNC3",
      "enabled": true
    },
    "watchdog_reset": {
      "function": "USER_EVENT",
      "enabled": true
    }
  },
  "user_functions": {
    "USER_FUNC8": "",
    "USER_FUNC1": "/home/pi/user-script.sh",
    "USER_FUNC2": "",
    "USER_FUNC3": "",
    "USER_FUNC4": "",
    "USER_FUNC5": "",
    "USER_FUNC6": "",
    "USER_FUNC7": ""
  },
  "system_task": {
    "watchdog": {
      "enabled": true,
      "period": "60"
    },
    "min_bat_voltage": {
      "threshold": "1",
      "enabled": true
    },
    "min_charge": {
      "threshold": "1",
      "enabled": true
    },
    "enabled": true,
    "wakeup_on_charge": {
      "enabled": true,
      "trigger_level": "1"
    }
  }
}
```

For the light version of PiJuice changes can be done directly on the JSON file.
Here is a list of accepted values for the various fields above.
* **system_events**:
    - low_battery_voltage low_charge no_power:
        * enabled: true, false
        * function:
            - NO_FUNC
            - SYS_FUNC_HALT
            - SYS_FUNC_HALT_POW_OFF
            - SYS_FUNC_SYS_OFF_HALT
            - SYS_FUNC_REBOOT
            - USER_EVENT
            - USER_FUNC1 .. USER_FUNC15
    - button_power_off, forced_power_off, forced_sys_power_off, watchdog_reset
        * enabled: true, false
        * function:
            - NO_FUNC
            - USER_EVENT
            - USER_FUNC1 .. USER_FUNC15
* **system_task**:
    - enabled: true, false
    - watchdog
        - enabled: true, false
        - period (minutes): 1..65535
    - min_bat_voltage
        - enabled: true, false
        - threshold (V): 0..10
    - min_charge
        - enabled: true, false
        - threshold (%): 0..100
    - wakeup_on_charge
        - enabled: true, false
        - trigger_level (%): 0..100
* **user_functions**:
    - absolute path to user defined script

### Adding USER_FUNC from 9 to 15

The user functions section of the JSON file looks like the following. To add USER_FUNC from 9 to 15 simply append them to the existing ones.

```text
 "user_functions": {    
    "USER_FUNC1": "",
    "USER_FUNC2": "",
    "USER_FUNC3": "",
    "USER_FUNC4": "",
    "USER_FUNC5": "",
    "USER_FUNC6": "",
    "USER_FUNC7": "",
    "USER_FUNC8": "",
    "USER_FUNC9": "",
    ...
    "USER_FUNC15": ""
  },

```

## I2C Command API
PiJuice HAT provides control, status and configuration of supported features through I2C Command API. Read/write commands are based on I2C block read/write transfers where messages carrying data are exchanged with Master. Message starts with one byte command code, followed by data payload and with checksum byte at the end of message. Checksum is 8-bit XOR calculated over all data payload bytes.

### Command Abstraction Layer
In order to facilitate communication with PiJuice HAT using I2C Command API there is abstraction layer hat encapsulates commands into more intuitive interface to configure, control and retrieve status of PiJuice features. This layer is implemented as python script module pijuice.py. Different types of interface function are encapsulated in next set of classes:
* **PiJuiceInterface**. Functions for low level message exchange end error checking through I2C bus.
* **PiJuiceStatus** Functions for dynamically controlling and reading status of PiJuice features.
* **PiJuiceRtcAlarm** Functions for setting-up real time clock and wake-up alarm.
* **PiJuicePower** Power management functions.
* **PiJuiceConfig** Functions for static configuration that mostly involves non-volatile data that saves in EEPROM.
All the function classes are encapsulated in top level object PiJuice(bus, address), where bus presents I2C bus identifier and address presents PiJuice HAT I2C slave address.
Usage example:
```python
from pijuice import PiJuice # Import pijuice module
pijuice = PiJuice(1, 0x14) # Instantiate PiJuice interface object
print pijuice.status.GetStatus() # Read PiJuice staus.
```
Commands are encapsulated with two type of functions, Setters that writes configuration and control data to PiJuice and Getters that reads status or current configuration/control data.
Every function returns object of dictionary type containing communication error status:
```python
{
'error':error_status
}
```
Where error_staus value can be NO_ERROR in case data are exchanged with no communication errors or value that describers error in cases where communication fails. In case of Getter functions additions additional data object is returned in case of successful data read with value that presents returned data:
```python
{
'error':error_status,
'data':data
}
```
#### PiJuiceStatus functions
**GetStatus()**

Gets basic PiJuice status information about power inputs, battery and events.
Returns:
'data':{
'isFault':is_fault,
'isButton':is_button,
'battery':battery_status,
'powerInput':power_input_status,
'powerInput5vIo':5v_power_input_status
}
Where:
* is_fault is True if there faults or fault events waiting to be read or False if there is no faults and no fault events.
* is_button is True if there are button events, False if not.
* battery_status is string constant that describes current battery status, one of four: 'NORMAL', 'CHARGING_FROM_IN', 'CHARGING_FROM_5V_IO', 'NOT_PRESENT'.
* power_input_status is string constant that describes current status of USB Micro power input, one of four: 'NOT_PRESENT', 'BAD', 'WEAK', 'PRESENT'.
* 5v_power_input_status: is string constant that describes current status of 5V GPIO power input, one of four: 'NOT_PRESENT', 'BAD', 'WEAK', 'PRESENT'.
Example:
```python
print pijuice.status.GetStatus()
```
Returns:
`{'data': {'battery': 'CHARGING_FROM_5V_IO', 'powerInput5vIo': 'PRESENT', 'isFault': False, 'isButton': False, 'powerInput': 'NOT_PRESENT'}, 'error': 'NO_ERROR'}`

**GetChargeLevel()**

Gets current charge level percentage.
Returns:
'data':charge_level
Where charge_level is percentage of charge, [0 - 100]%.
Example:
```python
print pijuice.status.GetChargeLevel()
```
Returns:
`{'data': 57, 'error': 'NO_ERROR'}`

**GetButtonEvents()**
Gets events generated by PiJuice buttons presses.
Returns:
'data': {
'SW1':event,
'SW2':event,
'SW3':event
}
where event is detected event name for corresponding button and can be one of: 'PRESS', 'RELEASE', 'SINGLE_PRESS', 'DOUBLE_PRESS', 'LONG_PRESS1', 'LONG_PRESS2' if event is generated or 'NO_EVENT' if event is absent.
Example:
```python
print pijuice.status.GetButtonEvents()
```
Returns:
`{'data': {'SW1': 'NO_EVENT', 'SW3': ' SINGLE_PRESS', 'SW2': 'NO_EVENT'}, 'error': 'NO_ERROR'}`

**AcceptButtonEvent(button)**
Clears generated button event.
Arguments:
button: button designator, one of: 'SW1', 'SW2', 'SW3'.
Example:
```python
print pijuice.status. AcceptButtonEvent ('SW2')
```

**SetLedState(led, rgb)**
Sets red, green and blue brightness levels for LED configured as “User LED”.
Arguments:
led: LED designator, one of: 'D1', 'D2'.
rgb:[r, g, b] - array of brightness levels of LED components, where r, g and b, are in range [0 – 255].
Example:
```python
print pijuice.status.SetLedState(‘D2’, [127, 0, 200])
```

**GetLedState(led)**
Gets current brightness levels for LED configured as “User LED”.
Arguments:
led: LED designator, one of: 'D1', 'D2'.
Returns:
'data':[r, g, b]
where [r, g, b] is array of brightness levels of LED components, where r, g and b, are in range [0 – 255].
Example:
```python
print pijuice.status.GetLedState('D1')
```
Returns:
`{'data': [127, 0, 200], 'error': 'NO_ERROR'}`

**SetLedBlink(led, count, rgb1, period1, rgb2, period2)**
Plays blink pattern on LED configured as “User LED”.
Arguments:
led: LED designator, one of: 'D1', 'D2'.
count: number of blinks for count in range [1 - 254], blink indefinite number of times for count = 255.
rgb1: [r, g, b] is array of brightness levels of LED components in first period of blink, where r, g and b, are in range [0 – 255].
period1: duration of first blink period in range [10 – 2550] miliseconds.
rgb2: [r, g, b] is array of brightness levels of LED components in second period of blink, where r, g and b, are in range [0 – 255].
Period2: duration of second blink period in range [10 – 2550] miliseconds.
Example:
```python
pijuice.status.SetLedBlink('D2', 10, [0,200,100], 1000, [100, 0, 0], 500)
```

Note: SetLedBlink statement sends the command to the microprocessor on the PiJuice board, which then executes it. Therefore, you have to wait in your Python program before you send the next SetLedBlink.

i.e.
```python
pijuice.status.SetLedBlink('D2', 1,(244, 66, 104), 0.3, (0, 0, 52), 0.1)
pijuice.status.SetLedBlink('D2', 1,(255, 33, 52), 0.5, (0, 0, 0), 0.75)
```
must be written as:

```python
pijuice.status.SetLedBlink('D2', 1,[244, 66, 104], 300, [0, 0, 52], 100)
sleep(0.4)
pijuice.status.SetLedBlink('D2', 1,[255, 33, 52], 500, [0, 0, 0], 750)
sleep(1.25)
```

## I2C Command API
PiJuice HAT provides control, status and configuration of supported features through I2C Command API. Read/write commands are based on I2C block read/write transfers where messages carrying data are exchanged with Master. Message starts with one byte command code, followed by data payload and with checksum byte at the end of message. Checksum is 8-bit XOR calculated over all data payload bytes.

### Command Abstraction Layer
In order to facilitate communication with PiJuice HAT using I2C Command API there is abstraction layer hat encapsulates commands into more intuitive interface to configure, control and retrieve status of PiJuice features. This layer is implemented as python script module pijuice.py. Different types of interface function are encapsulated in next set of classes:
* **PiJuiceInterface**. Functions for low level message exchange end error checking through I2C bus.
* **PiJuiceStatus** Functions for dynamically controlling and reading status of PiJuice features.
* **PiJuiceRtcAlarm** Functions for setting-up real time clock and wake-up alarm.
* **PiJuicePower** Power management functions.
* **PiJuiceConfig** Functions for static configuration that mostly involves non-volatile data that saves in EEPROM.
All the function classes are encapsulated in top level object PiJuice(bus, address), where bus presents I2C bus identifier and address presents PiJuice HAT I2C slave address.
Usage example:
```python
from pijuice import PiJuice # Import pijuice module
pijuice = PiJuice(1, 0x14) # Instantiate PiJuice interface object
print pijuice.status.GetStatus() # Read PiJuice staus.
```
Commands are encapsulated with two type of functions, Setters that writes configuration and control data to PiJuice and Getters that reads status or current configuration/control data.
Every function returns object of dictionary type containing communication error status:
```python
{
'error':error_status
}
```
Where error_staus value can be NO_ERROR in case data are exchanged with no communication errors or value that describers error in cases where communication fails. In case of Getter functions additions additional data object is returned in case of successful data read with value that presents returned data:
```python
{
'error':error_status,
'data':data
}
```
#### PiJuiceStatus functions
**GetStatus()**

Gets basic PiJuice status information about power inputs, battery and events.
Returns:
'data':{
'isFault':is_fault,
'isButton':is_button,
'battery':battery_status,
'powerInput':power_input_status,
'powerInput5vIo':5v_power_input_status
}
Where:
* is_fault is True if there faults or fault events waiting to be read or False if there is no faults and no fault events.
* is_button is True if there are button events, False if not.
* battery_status is string constant that describes current battery status, one of four: 'NORMAL', 'CHARGING_FROM_IN', 'CHARGING_FROM_5V_IO', 'NOT_PRESENT'.
* power_input_status is string constant that describes current status of USB Micro power input, one of four: 'NOT_PRESENT', 'BAD', 'WEAK', 'PRESENT'.
* 5v_power_input_status: is string constant that describes current status of 5V GPIO power input, one of four: 'NOT_PRESENT', 'BAD', 'WEAK', 'PRESENT'.
Example:
```python
print pijuice.status.GetStatus()
```
Returns:
`{'data': {'battery': 'CHARGING_FROM_5V_IO', 'powerInput5vIo': 'PRESENT', 'isFault': False, 'isButton': False, 'powerInput': 'NOT_PRESENT'}, 'error': 'NO_ERROR'}`

**GetChargeLevel()**

Gets current charge level percentage.
Returns:
'data':charge_level
Where charge_level is percentage of charge, [0 - 100]%.
Example:
```python
print pijuice.status.GetChargeLevel()
```
Returns:
`{'data': 57, 'error': 'NO_ERROR'}`

**GetButtonEvents()**
Gets events generated by PiJuice buttons presses.
Returns:
'data': {
'SW1':event,
'SW2':event,
'SW3':event
}
where event is detected event name for corresponding button and can be one of: 'PRESS', 'RELEASE', 'SINGLE_PRESS', 'DOUBLE_PRESS', 'LONG_PRESS1', 'LONG_PRESS2' if event is generated or 'NO_EVENT' if event is absent.
Example:
```python
print pijuice.status.GetButtonEvents()
```
Returns:
`{'data': {'SW1': 'NO_EVENT', 'SW3': ' SINGLE_PRESS', 'SW2': 'NO_EVENT'}, 'error': 'NO_ERROR'}`

**AcceptButtonEvent(button)**
Clears generated button event.
Arguments:
button: button designator, one of: 'SW1', 'SW2', 'SW3'.
Example:
```python
print pijuice.status. AcceptButtonEvent ('SW2')
```

**SetLedState(led, rgb)**
Sets red, green and blue brightness levels for LED configured as “User LED”.
Arguments:
led: LED designator, one of: 'D1', 'D2'.
rgb:[r, g, b] - array of brightness levels of LED components, where r, g and b, are in range [0 – 255].
Example:
```python
print pijuice.status.SetLedState(‘D2’, [127, 0, 200])
```

**GetLedState(led)**
Gets current brightness levels for LED configured as “User LED”.
Arguments:
led: LED designator, one of: 'D1', 'D2'.
Returns:
'data':[r, g, b]
where [r, g, b] is array of brightness levels of LED components, where r, g and b, are in range [0 – 255].
Example:
```python
print pijuice.status.GetLedState('D1')
```
Returns:
`{'data': [127, 0, 200], 'error': 'NO_ERROR'}`

**SetLedBlink(led, count, rgb1, period1, rgb2, period2)**
Plays blink pattern on LED configured as “User LED”.
Arguments:
led: LED designator, one of: 'D1', 'D2'.
count: number of blinks for count in range [1 - 254], blink indefinite number of times for count = 255.
rgb1: [r, g, b] is array of brightness levels of LED components in first period of blink, where r, g and b, are in range [0 – 255].
period1: duration of first blink period in range [10 – 2550] miliseconds.
rgb2: [r, g, b] is array of brightness levels of LED components in second period of blink, where r, g and b, are in range [0 – 255].
Period2: duration of second blink period in range [10 – 2550] miliseconds.
Example:
```python
pijuice.status.SetLedBlink('D2', 10, [0,200,100], 1000, [100, 0, 0], 500)
```

**GetLedBlink(led)**
Gets current settings of blink pattern for LED configured as “User LED”.
Arguments:
led: LED designator, one of: 'D1', 'D2'.
Returns:
'data': {
'count':count,
'rgb1':rgb1,
'period1':period1,
'rgb2':rgb2,
'period2':period2
}
Example:
```python
print pijuice.status.GetLedBlink('D2')
```
Returns:
`{'data': {'count': 10, 'period2': 500, 'rgb2': [100, 0, 0], 'rgb1': [0, 200, 100], 'period1': 1000}, 'error': 'NO_ERROR'}`

**GetIoDigitalInput(pin)**
Gets state at IO pin configured as digital input.
Arguments:
pin: IO pin designator, 1 for IO1, 2 for IO2.
Returns:
'data':input_state
Where input_state is 0 for low input state, 1 for high.
Example:
```python
print pijuice.status.GetIoDigitalInput(1)
```
Returns:
`{'data': 0, 'error': 'NO_ERROR'}`

**SetIoDigitalOutput(pin, value)**
Sets state at IO pin configured as digital output.
Arguments:
pin: IO pin designator, 1 for IO1, 2 for IO2.
value: output state to set, 0 for low output state, 1 for high.
Example:
```python
print pijuice.status.SetIoDigitalOutput(1, 1)
```

**GetIoDigitalOutput(pin)**
Gets current output state at IO pin configured as digital output.
Arguments:
pin: IO pin designator, 1 for IO1, 2 for IO2.
Returns:
'data':output_state
Where output_state is 0 for low output state, 1 for high.
Example:
```python
print pijuice.status.GetIoDigitalOutput(1)
```
Returns:
`{'data': 1, 'error': 'NO_ERROR'}`

**GetIoAnalogInput(pin)**
Gets voltage in millivolts at IO pin configured as analog input.
Arguments:
pin: IO pin designator, 1 for IO1, 2 for IO2.
Returns:
'data':analog_value
where analog_value is voltage in millivolts measured at analog input.
Example:
```python
print pijuice.status.GetIoAnalogInput(1)
```
Returns:
`{'data': 2222, 'error': 'NO_ERROR'}`

**SetIoPWM(pin, dutyCircle)**
Sets PWM duty circle at IO pin configured as PWM output.
Arguments:
pin: IO pin designator, 1 for IO1, 2 for IO2.
dutyCircle: pulse width as percentage of period, [0 - 100]%
Example:
```python
print pijuice.status.SetIoPWM(2, 35.6)
```

**GetIoPWM(pin)**
Gets current PWM duty circle at IO pin configured as PWM output.
Arguments:
pin: IO pin designator, 1 for IO1, 2 for IO2.
Returns:
'data':duty_circle
where duty_circle is pulse width as percentage of period.
Example:
```python
print pijuice.status.GetIoPWM(2)
```
Returns:
`{'data': 35.59984130375072, 'error': 'NO_ERROR'}`

#### PiJuicePower Functions

**SetSystemPowerSwitch(state)**
Sets state of System switch.
Arguments:
' state':state
where state is desired current limit in milliampere (two options available, 500 and 2100), or switch off if 0.
Example:
```python
print pijuice.power.SetSystemPowerSwitch(500)
```

**GetSystemPowerSwitch()**
Gets current state of System switch.
Returns:
'data': state
where state is current limit in milliampere or 0 if switch is off.
Example:
```python
print pijuice.power.GetSystemPowerSwitch()
```
Returns:
`{'data': 500, 'error': 'NO_ERROR'}`
