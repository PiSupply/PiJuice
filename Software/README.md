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

### Manual process

Copy the deb package to the pi home and install the package.

`sudo dpkg -i ./pijuice_1.0-1_all.deb`

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

### Main software menu, with no battery attached

![Main software menu, with no battery attached](https://user-images.githubusercontent.com/16068311/35161233-7cfa5fce-fd37-11e7-83ec-72a8043ee0c0.png "Main software menu, with no battery attached")

This picture is how the PiJuice Settings software looks when it loads up. This also shows some basic information about the battery charge, battery voltage, and where it is charging from....here is it showing 0% and a low voltage on the battery - because there is no battery installed! You can also see that it is charging from the Pi GPIO (meaning it is plugged in to the Pis microUSB) and it also shows the rail voltages and current draw over the GPIO pins. Below that is the PiJuice microUSB and as you can see in this screenshot that is not currently plugged in. There is a fault checker, a system switch state and also a link to a HAT config menu (more on that later! - see PiJuice HAT Configuration Menu screenshot).

### Main software menu, with battery attached

![Main software menu, with battery attached](https://user-images.githubusercontent.com/16068311/35161234-7d125174-fd37-11e7-9383-e2e80044258d.png "Main software menu, with battery attached")

This screenshot shows the same menu as in the previous screenshot, the only difference being there is now a battery installed in the PiJuice.

### Wakeup Alarm Menu

![Wakeup Alarm Menu](https://user-images.githubusercontent.com/16068311/35161225-7bd18140-fd37-11e7-8889-e6715023b334.png "Wakeup Alarm Menu")

In this screenshot we have moved over to the Wakeup alarm tab of the config menu and as you can see this is an area where you can set schedules for the Pi to automatically wake up. This is useful for remote monitoring applications.

This feature will only work if you are either plugged in to the PiJuice microUSB / running on battery. If the battery is low and you are plugged in via the Raspberry Pis GPIO the only way to enable this feature is by soldering the optional "spring pin" that comes with the PiJuice HAT.

### System Task Menu

![System Task Menu](https://user-images.githubusercontent.com/16068311/35161236-7d4e6f56-fd37-11e7-9209-7943e88a76d5.png "System Task Menu")

Here we have the system task menu tab. This enables you to set the external watchdog timer - useful for remote applications where you can't come and do a hard-reset yourself if the Pi crashes or hangs. The PiJuice essentially monitors for a "heart beat" from the software - if it does not sense it after a defined period of time it automatically resets the Raspberry Pi. You can also set here wakeup on charge levels, minimum battery levels and voltages.

The watchdog timer has a configurable time-out. It defines the time after which it will power cycle if it doesn't receive a heartbeat signal. The time step is in minutes so the minimum time-out period is one minute and the maximum is 65535 minutes. The number can be any whole number between one and 65535. If you set the time to zero the watchdog timer will be disabled.

### System Events Menu

![System Events Menu](https://user-images.githubusercontent.com/16068311/35161235-7d31d544-fd37-11e7-92b4-dc0ccab55c56.png "System Events Menu")

This is the system events menu tab. It allows you to trigger events for certain scenarios such as low charge, low voltage and more. Each paramater has a couple of preset options to choose from, and also you can select options from the "user scripts" tab which allows you to trigger your own custom scripts when certain system events occur for maximum flexibility.

### User scripts menu

![User scripts menu](https://user-images.githubusercontent.com/16068311/33845750-5a95cdf6-de9c-11e7-8840-b6dc7f079d35.png "User scripts menu")

This is the user scripts menu tab as we mentioned in the above screenshot description where you can add paths to custom scripts that you can trigger on events.

User scripts can be assigned to user functions called by system task when configured event arise. This should be non-blocking callback function that implements customized system functions or event logging.

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
* CHARGE STATUS. LED is configured to signal current charge level of battery. For level <= 15% red with configurable brightness. For level > 15% and level <=50% mix of red and green with configurable brightness. For level > 50% green with configurable brightness. When battery is charging blinking blue with configurable brightness is added to current charge level color. For full buttery state blue component is steady on.
* USER LED. When LED is configured as User LED it can be directly controlled with User software via command interface. Initial PiJuice power on User LED state is defined with R, G, and B brightness level parameters.

### PiJuice HAT Config Battery Menu

![PiJuice HAT Config Battery Menu](https://user-images.githubusercontent.com/16068311/35161226-7c026f30-fd37-11e7-897a-a470e06a4c8b.png "PiJuice HAT Config Battery Menu")

The battery menu is a very important one. It basically allows you to set charge profiles for the PiJuice charge chip in order to correctly and efficiently charge the battery, correctly monitor the charge percentages and more. We have got a number of built in presets such as the ones that will come with the PiJuice by default (the BP7X) and all of the other ones we will supply. But as promised, there is also the ability to add your own custom charge profiles and even your own battery temperature sensor in order to increase the safety and efficiency of charging your batteries.

As previously mentioned, some of these are even hard coded into the firmware on the PiJuice which enables you to actually select profiles using the PiJuices on board DIP switch.

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

### Automatic wake-up
PiJuice can be configured to automatically wake-up system in several ways: on charge level, when power source appears, by RTC Alarm.
* **Wakeup on charge**: When power source appears and battery starts charging this function can be configured to wakeup system when charge level reaches settable trigger level. Trigger level can be set in range 0%-100%. Setting trigger level to 0 means that system will wake-up as soon as power source appears and battery starts charging.
* **Wake-up Alarm**: PiJuice features real-time clock with Alarm function that can be configured to wake-up system at programmed date/time.

### JSON configuration file
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
            - SYS_FUNC_HALT_POW_OFF
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
        - threshold (%): 0..100 
    - min_charge
        - enabled: true, false
        - threshold (%): 0..100 
    - wakeup_on_charge
        - enabled: true, false
        - trigger_level (Volts): 0..10
* **user_functions**: 
    - absolute path to user defined script

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

### Battery Profiles
The default battery profile is defined by the DIP switch position. On v1.1 it is position 01 for BP7X, on v1.0 version, it might be different, you can try different positions, and power circle pijuice to get updated. You can see a guide on positioning the DIP switch [here](https://github.com/PiSupply/PiJuice/blob/master/Hardware/Batteries/Pijuice_battery_config.xlsx).

It is not possible to detect battery not present when powered through on board USB micro, so it might show 0% only.

User functions are 4 digit binary coded and have 15 combinations, code 0 is USER_EVENT meant that it will not be processed by system task, but left to user and python API to manage it. I thought it is rare case that all 15 will be needed so on gui there is 8 (it will make big window also). However if someone needs more scripts it can be manualy added by editing config json file: /var/lib/pijuice/pijuice_config.JSON. Also all other configurations can be managed manually in this file if the GUI is not available.
