![PiJuice Logo](https://user-images.githubusercontent.com/16068311/30545031-58b8fec6-9c80-11e7-8b3a-5e1f3aefd86c.png?raw=true "PiJuice Logo")
# PiJuice
Resources for the [PiJuice range](https://www.pi-supply.com/?s=pijuice&post_type=product&tags=1&limit=5&ixwps=1) (complete with our revolutionary [PiAnywhere](https://www.pi-supply.com/product-tag/pianywhere/) technology – the best way to take your Pi off the grid!). Originally funded on [Kickstarter](https://www.kickstarter.com/projects/pijuice/pijuice-a-portable-project-platform-for-every-rasp/).

## Board overview

PiJuice is a fully uninterruptable power supply that will always keep your Raspberry Pi powered. It has a real time clock (RTC) for time tracking and scheduled task when the Pi is offline. It also has an integrated microcontroller (MCU) chip which will manage soft shut down functionality, a true low power deep sleep state and intelligent start up.  

The tri-coloured RGB LEDs will allow you to keep track of the battery charge levels and other info. There are three programmable buttons to trigger events and customisable scripts aside from their predefined functions.
PiJuice only uses five of the Pi's GPIO pins (just power and I2C), the rest are free and made available via the stacking header which allows for other boards to be used along side PiJuice.
The board can be powered directly from the grid with the standard Raspberry Pi PSU, via an on board battery, via external batteries, solar panels, wind turbines and other renewable sources.

The board is has a Raspberry Pi HAT compatible layout, with onboard EEPROM. Although it has been designed for the Raspberry Pi A+, B+, 2B and 3B but it is also compatible with the Raspberry Pi Zero v1.3 and Zero W v1.1.

PiJuice is fully [CE and FCC tested](https://github.com/PiSupply/PiJuice/tree/master/Documentation/Compliance) design and [battery safety tests](https://github.com/PiSupply/PiJuice/tree/master/Documentation/Battery%20Safety) have also been carried out.

## Installing the software

For PiJuice we have created a Raspbian Package to make it extra simple to obtain and install the necessary software.
At the command line simply type
```bash
sudo apt-get install pijuice
``` 
After a restart a new icon will appear under preference.
Please refer to our [Software section](https://github.com/PiSupply/PiJuice/tree/master/Software) to find out more.

## Basic functionality

Once the battery is slot into place LED1 will start blinking in green. This doesn't mean your Raspberry Pi is yet receiving power it is just telling you that the PiJuice MCU is receiving power and is in standby.   

### Power On and Off the Pi

![Buttons_LEDs](https://user-images.githubusercontent.com/16068311/33768831-94db68b0-dc1f-11e7-99d4-a06cb65b0135.png "Buttons and LEDs")

**If you wish to start your Raspberry Pi using the PiJuice SW1 you'll have to connect the micro USB to the PiJuice directly.**

To power on the Raspberry Pi single press SW1 briefly as you would for a click of a mouse. If the micro USB is directly connected to the Raspberry Pi then it will power up immediately without you needing to press the SW1.
To power off press and keep pressed SW1 for about 10 seconds. A "halt" command will be received by the OS and the Raspberry Pi will shut down in the proper manner. 
To remove power to the Pi press and keep pressed SW1 for at least 20 seconds. This will remove power from the Raspberry Pi without issuing a "power off" command to the OS. This approach should only be used in case nothing else works or if the OS is mounted in read only mode otherwise data on SD card may be corrupted. 

### Providing power to the board

PiJuice can recharge its on board battery in several ways and via different power sources. The most common approach is to use the standard Raspberry Pi PSU or a micro USB PSU with similar characteristics but solar panels, wind mills, and other renewable power sources can be used too.
When the PiJuice is installed over the Raspberry Pi you can power both the Pi and PiJuice via either the Pi's micro USB or the PiJuice's one. That's because the power lines on the GPIO header are shared by both boards and this allows to effectively be able to charge the battery independently from which micro USB connector you use.
Other ways of providing power to the PiJuice is directly via the GPIO pin headers or one of the other connectors on board. See the [Hardware section](https://github.com/PiSupply/PiJuice/tree/master/Hardware) for more information.

### Buttons and LEDs

SW1 and LED1 have predefined default functions associated.

SW1 is the power button by default:
* Single brief press to power on.
* Long press of at least 10 seconds to halt (soft shutdown).
* Long press of at least 20 seconds to cut power (hard shutdown).

LED1 is charge status by default:
* with Pi off
    * Green blinking: Standby
    * Blue blinking: Charging
    * Red blinking: Low battery
* Led1 with Pi on
    * Green steady: Power on
    * Blue blinking: Charging

On board the PiJuice there are three buttons and two multicolour LEDs please refer to the [hardware](https://github.com/PiSupply/PiJuice/blob/master/Hardware/README.md) and the [software](https://github.com/PiSupply/PiJuice/blob/master/Software/README.md) sections to find out more.

### Power management

The PiJuice provides an onboard intelligent on/off switch allowing you to have control on when the Raspberry Pi will be switched on or off without having to plug or unplug the PSU as you would normally have to do.

*Note Turning on the Raspberry Pi via the onboard intelligent switch only works when the power is provided the micro USB on the PiJuice.* 

When the Raspberry Pi is off, for example due to a scheduled power off via the onboard RTC, the PiJuice will enter a low power deep-sleep state which guarantees only a minute fraction of the battery charge will be required to keep the circuitry on ensuring long periods of inactivity between recharges.
This is for example ideal when dealing with recyclable energy sources like solar panels. At all time the PiJuice will still be able to trigger a wake on to the Raspberry Pi via interrupt or calendar event.

The PiJuice has been designed so that it can accept power sources of different nature. It accepts power sources providing between 4.2V and 10V which can be applied on different onboard connectors.
You can find out more in the [hardware section](https://github.com/PiSupply/PiJuice/blob/master/Hardware/README.md).

#### UPS functionality

One of the main functionality provided by PiJuice is to ensure that the Raspberry Pi remains on when it needs to. It provides a hardware watchdog timer to keep your Raspberry Pi up and running for mission-critical remote applications and works as a Full Uninterrupted / Uninterruptable Power Supply solution.
The board comes with an onboard 1820 mAh "off the shelf" Lipo / LiIon battery which guarantees ~4/6 hours of up time.
PiJuice is compatible with any single cell LiPo or LiIon battery so bigger or even smaller batteries can be chosen especially depending on CPU load and connected hardware which may vary significantly the overall maxium up time of the Raspberry Pi. That's why we have provided means to support bigger battery sizes like 5000 or 10,000 mAH+ to could last up to 24 hrs +.
The batteries can be replaced without downtime as far as an alternative power is provided in the meantime. You could for example even use a battery bank whilst replacing the onboard battery or one connected to the screw terminal. Using a standard PSU will of course work too and that configuration is in fact ideal for a UPS setup.    

### GUI interface

Enhanced graphical user interface (GUI) is available via APT provides a full power management API for Raspbian and allows auto shutdown capability when running low on batteries.
It also provide a mean to attach customisable scripts to power events and report of battery status.
You can find out more in the [software section](https://github.com/PiSupply/PiJuice/blob/master/Software/README.md).

## Technical specs summary

- The EEPROM can be disabled and its I2C address changed for increased compatibility with other boards
- BP7X battery - original battery from Motorola Droid 2 (A955) - 1820mAh battery
- Microcontroller is an ST Micro STM32F030CCT6 ARM Cortex-M0, 48MHz, F64KB, R8KB, I2C, SPI, USART, 2.4-3.6V
- Charge IC - BQ24160RGET Charger IC Lithium-Ion/Polymer, 2.5A, 4.2-10V
- Fuel gauge IC - LC709203FQH-01TWG Battery Fuel Gauge, 1-Cell Li-ion, 2.8%
- EEPROM - CAT24C32WI-GT3 EEPROM, I2C, 32KBIT, 400KHZ, 1V7-5V5
- Optional spring pin - Mil-Max 0929-7-15-20-77-14-11-0
- Compatible with any 4 pin battery on board that can be used with 00-9155-004-742-006 battery contacts from AVX including the BP7X, BP6X, and any compatible batteries including the 1600mAh and 2300mAh ones from CameronSino (CS-MOA853SL and CS-MOA855XL)
- There is an on board 4 pin screw terminal block for larger off board batteries. Any single cell LiPo / LiIon is compatible. However, you use your own sourced battery at your own risk. We HIGHLY RECOMMEND using a battery with an internal protection circuit and a NTC (temp sensor)
- Optional header for offboard button - connected to same output as SW1
- 6 pin breakout header - with two GPIO from the ARM Cortex-M0, Vsys, 5v0, 3v3, GND connections
- Header for optional off board solar panel / wind turbine etc.
- Optional RF Shield attachment - Harwin S02-20150300 (can also double as an inexpensive heatsink)
- Input voltage range - 4.2V – 10V
- Output voltage - 3.3V and 5V
- Output amperage - maximum current at 5V gpio is 2.5A and at VSYS output 2.1A, but also this depends heavily on battery capacity. For BP7X have measured around 1.1A at 5V GPIO and around 1.6A at VSYS output. Obviously this also depends heavily on the current draw demanded by the Raspberry Pi / device itself. To achieve maximum of 2.5A it will need battery over 3500mAh.

## Additional content
A part from the material available on Github we are also preparing guides, tutorials and additional information on various platforms and media. This is a list of current and future resources:

* There is a "quick start guide" in the package [which you can see here](https://github.com/PiSupply/PiJuice/blob/master/Documentation/PiJuice%20Guide.pdf). As you can see it's pretty basic but really the PiJuice is quite intuitive and very easy to use. 
* There will also be more technical information going up on the PiJuice GitHub here - https://github.com/PiSupply/PiJuice
* And also a lot of picture guides and tutorials going up on our Maker Zone - https://www.pi-supply.com/make 
* We will likely have the pinouts available at http://pinout.xyz
* We will also have tutorials on Instructables and Hackster.io 

# Reviews and links
**Coming soon**