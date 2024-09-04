![PiJuice Logo](https://user-images.githubusercontent.com/1878314/73881793-e1c00080-4858-11ea-8ce8-e5bd4cc6f9b6.png)
# PiJuice
Resources for the [PiJuice range](https://uk.pi-supply.com/search?type=product&q=PiJuice) (complete with our revolutionary [PiAnywhere](https://uk.pi-supply.com/search?type=product&q=PiAnywhere) technology – the best way to take your Pi off the grid!). Originally funded on [Kickstarter](https://www.kickstarter.com/projects/pijuice/pijuice-a-portable-project-platform-for-every-rasp/).

#### Where to buy a PiJuice?

Below you can find a list of our PiJuice range of products:

- [PiJuice HAT](https://uk.pi-supply.com/collections/pijuice/products/pijuice-portable-power-raspberry-pi))
- [PiJuice Zero](https://uk.pi-supply.com/products/pijuice-zero)
- [PiJuice Additional / Replacement Batteries](https://uk.pi-supply.com/search?type=product&q=PiJuice+Battery)
- [PiJuice Cases](https://uk.pi-supply.com/search?type=product&q=PiJuice+Case)
- [PiJuice Solar Panels](https://uk.pi-supply.com/search?type=product&q=PiJuice+Solar+Panel)
- [PiJuice Kits](https://uk.pi-supply.com/search?type=product&q=PiJuice+Kit)
- [Other PiJuice Accessories](https://uk.pi-supply.com/search?type=product&q=PiJuice)

We also have a variety of resellers and distributors that stock our PiJuice range. You can often find them by just typing in the product SKU (shown on each product page above the title with format PIS-0000) or product title into Google in your country, however we also have a [list of distributors](https://uk.pi-supply.com/pages/distributors) on our website for your reference. If you are struggling to find stock of any particular products, feel free to send us an email to [sales@pi-supply.com](mailto:sales@pi-supply.com) and we will do our best to help you find what you need.

## Board overview

PiJuice is a fully uninterruptable / uninterupted power supply that will always keep your Raspberry Pi powered! It has a real time clock (RTC) for time tracking and scheduled tasks when the Pi is offline (as well as remote on/off of your Pi). It also has an integrated microcontroller (MCU) chip which will manage soft shut down functionality, a true low power deep sleep state and intelligent start up.  

The tri-coloured RGB LEDs will allow you to keep track of the battery charge levels and other info (they are programmable). There are also three programmable buttons which will allow you to trigger events or customisable scripts aside from their predefined functions. PiJuice only uses five of the Pi's GPIO pins (just power and I2C), the rest are free and made available via the stacking header which allows for other boards to be used along side PiJuice. The board can be powered directly from the grid with the standard Raspberry Pi PSU, via an on board battery, via external batteries, solar panels, wind turbines and other renewable sources.

The board has a Raspberry Pi HAT or pHAT compatible layout, with onboard EEPROM (you can disable the EEPROM if you want also). It has been designed for the Raspberry Pi A+, B+, 2B, 3A+, 3B and 3B+. It is also compatible with the Raspberry Pi Zero v1.3 and Zero W v1.1 or any other Pi with a 40 way GPIO header.

PiJuice is a fully [CE and FCC tested](https://github.com/PiSupply/PiJuice/tree/master/Documentation/Compliance) design and [battery safety tests](https://github.com/PiSupply/PiJuice/tree/master/Documentation/Battery%20Safety) have also been carried out for peace of mind when using in educational or industrial settings.

## Installing the software

PiJuice will actually work straight out of the box, with no software, to power the Pi and for some other basic functionality. However to get the most out of PiJuice you really need to install the software packages. For PiJuice we have created a Raspbian / Debian Package to make it extra simple to obtain and install the necessary software. At the command line simply type:
```bash
sudo apt-get install pijuice-gui
```
After a restart a new icon "PiJuice Settings" will appear under Menu>Preferences.

![pijuice](https://raw.githubusercontent.com/PiSupply/PiJuice/master/Software/Source/data/images/pijuice.png)

If you wish to install just the light version of PiJuice with no GUI:
```bash
sudo apt-get install pijuice-base
```
This is particularly indicated for Raspbian Lite or an headless installation.

 Please refer to our [software section](https://github.com/PiSupply/PiJuice/tree/master/Software) to find out more.

## Basic functionality

**Important note**: If your PiJuice was supplied with a battery fitted, you will **need to remove the little plastic battery isolation tab**. If you don't remove that plastic tab, your PiJuice will not charge up, and will report 0% battery level.

Once the battery is slotted into place (make sure you remove the little plastic battery isolation tab too!) LED1 will start blinking in green. This doesn't mean your Raspberry Pi is yet receiving power it is just telling you that the PiJuice MCU is receiving power and is in standby.   

### Power On and Off the Pi

![Buttons_LEDs](https://user-images.githubusercontent.com/16068311/33768831-94db68b0-dc1f-11e7-99d4-a06cb65b0135.png "Buttons and LEDs")

**If you wish to start your Raspberry Pi using the PiJuice SW1 you'll have to connect the micro USB to the PiJuice directly.**

To power on the Raspberry Pi single press SW1 briefly as you would for a click of a mouse. If the micro USB is directly connected to the Raspberry Pi then it will power up immediately without you needing to press the SW1. To power off press and keep pressed SW1 for about 10 seconds. A "halt" command will be received by the OS and the Raspberry Pi will shut down in the proper manner. To remove power to the Pi press and keep pressed SW1 for at least 20 seconds. This will remove power from the Raspberry Pi without issuing a "power off" command to the OS. This approach should only be used in case nothing else works or if the OS is mounted in read only mode otherwise data on SD card may be corrupted.

### Providing power to the board

PiJuice can recharge its on board battery in several ways and via different power sources. The most common approach is to use the standard Raspberry Pi PSU or a micro USB PSU with similar characteristics but solar panels, wind mills, and other renewable power sources can be used too. When the PiJuice is installed over the Raspberry Pi you can power both the Pi and PiJuice via either the Pi's micro USB or the PiJuice's one. That's because the power lines on the GPIO header are shared by both boards and this allows to effectively be able to charge the battery independently from which micro USB connector you use. Other ways of providing power to the PiJuice is directly via the GPIO pin headers or one of the other connectors on board. See the [hardware section](https://github.com/PiSupply/PiJuice/tree/master/Hardware) for more information.

### Buttons and LEDs

SW1 and LED1 have predefined default functions associated.

* **SW1** - Power button:
  * Single brief press to power on.
  * Long press of at least 10 seconds to halt (soft shutdown).
  * Long press of at least 20 seconds to cut power (hard shutdown).

* **LED1** - Charge status:
  * with Pi off
    * ![#0000FF](http://via.placeholder.com/15x15/0000FF/000000?text=+) Blue steady: Battery fully charged
    * ![#00FF00](http://via.placeholder.com/15x15/00FF00/000000?text=+) Green blinking: Charge over 50%
    * ![#0000FF](http://via.placeholder.com/15x15/0000FF/000000?text=+) Blue blinking: Charging
    * ![#FF0000](http://via.placeholder.com/15x15/FF0000/000000?text=+) Red blinking: Low battery
  * with Pi on
    * ![#00FF00](http://via.placeholder.com/15x15/00FF00/000000?text=+) Green blinking: Power on - Battery over 50%
    * ![#0000FF](http://via.placeholder.com/15x15/0000FF/000000?text=+) Blue blinking: Charging
    * ![#FFA500](http://via.placeholder.com/15x15/FFA500/000000?text=+) Orange blinking: Low battery - level < 50%
    * ![#FF0000](http://via.placeholder.com/15x15/FF0000/000000?text=+) Red blinking: Low battery - level < 15% or battery absent

On board the PiJuice HAT there are three buttons and two multicolour LEDs and on the PiJuice Zero pHAT, 1 button and two multicolour LEDs; please refer to the [hardware](https://github.com/PiSupply/PiJuice/blob/master/Hardware/README.md) and the [software](https://github.com/PiSupply/PiJuice/blob/master/Software/README.md) sections to find out more.

### Power management

The PiJuice provides an onboard intelligent on/off switch allowing you to have control on when the Raspberry Pi will be switched on or off without having to plug or unplug the PSU as you would normally have to do.

*Note: Turning on the Raspberry Pi via the onboard intelligent switch only works when the power is provided to the micro USB on the PiJuice.*

When the Raspberry Pi is off, for example due to a scheduled power off via the onboard RTC, the PiJuice will enter a low power deep-sleep state which guarantees only a minute fraction of the battery charge will be required to keep the circuitry on ensuring long periods of inactivity between recharges.

This is for example ideal when dealing with recyclable energy sources like solar panels. At all times the PiJuice will still be able to trigger a wake-up call to the Raspberry Pi via interrupt or calendar event.

The PiJuice has been designed so that it can accept power sources of different nature. It accepts power sources providing between 4.2V and 10V which can be applied on different onboard connectors.

You can find out more in the [hardware section](https://github.com/PiSupply/PiJuice/blob/master/Hardware/README.md).

### UPS  and remote functionality

One of the main functionalities provided by PiJuice is to ensure that the Raspberry Pi always remains on when it needs to and shuts down safely when low on power. It provides a hardware watchdog timer to keep your Raspberry Pi up and running for mission-critical remote applications and works as a full uninterrupted / uninterruptible power supply solution.

The PiJuice HAT comes with an onboard 1820 mAh "off the shelf" Lipo / LiIon battery which guarantees ~4/6 hours of up time.
PiJuice is compatible with any single cell LiPo or LiIon battery so bigger (or even smaller) batteries can be chosen especially depending on CPU load and connected hardware which may cause significant variations to the overall maximum up time of the Raspberry Pi. That's why we have provided means to support bigger battery sizes like 5000 or 10,000 mAH+ to help your PiJuice last up to 24 hrs +.

The batteries can be replaced without downtime as long as an alternative power source is provided in the meantime. You could for example even use a battery bank whilst replacing either the onboard battery or one connected to the screw terminal (you shouldn't connect a battery on board and to screw terminal at same time though or you will fry your PiJuice). Using a standard PSU will of course work too and that configuration is in fact ideal for a UPS setup.

The PiJuice Zero pHAT comes with a 3-pin JST battery connector for directly connecting any single cell LiPo or LiIon. In the Pi-Supply store we stock a large range of various capacity of LiPo batteries that have been tested and are compatible with the PiJuice boards - [available batteries](https://uk.pi-supply.com/search?type=product&q=pijuice+batteries)

### Graphic and Terminal based user interface

A graphical user interface (GUI) is available via the pijuice-gui Raspbian package and provides a full power management API for the Raspbian desktop that allows auto shutdown capability when running low on batteries.

It also provides a means to attach customisable scripts to power events and reporting of the battery status.
You can find out more in the [software section](https://github.com/PiSupply/PiJuice/blob/master/Software/README.md).

For a headless installation the pijuice-base Raspbian package provides a terminal based equivalent (pijuice_cli) with the same capabilities as the GUI one.

## Technical specs summary

- The EEPROM can be disabled and its I2C address changed for increased compatibility with other boards
- BP7X battery - original battery from Motorola Droid 2 (A955) - 1820mAh battery
- Microcontroller is an ST Micro STM32F030CCT6 ARM CortexM0,48MHz,F256KB,R32KB,I2C,SPI,USART,2.4-3.6V
- Charge IC - BQ24160RGET Charger IC Lithium-Ion/Polymer, 2.5A, 4.2-10V
- Fuel gauge IC - LC709203FQH-01TWG Battery Fuel Gauge, 1-Cell Li-ion, 2.8%
- EEPROM - CAT24C32WI-GT3 EEPROM, I2C, 32KBIT, 400KHZ, 1V7-5V5
- Optional spring pin - Mil-Max 0929-7-15-20-77-14-11-0 or 0929-7-15-20-75-14-11-0
- Compatible with any 4 pin battery on board that can be used with 00-9155-004-742-006 battery contacts from AVX including the BP7X, BP6X, and any compatible batteries including the 1600mAh and 2300mAh ones from CameronSino (CS-MOA853SL and CS-MOA855XL)
- There is an on board 4 pin screw terminal block for larger off board batteries. Any single cell LiPo / LiIon is compatible. However, you use your own sourced battery at your own risk. We HIGHLY RECOMMEND using a battery with an internal protection circuit and a NTC (temp sensor)
- Optional header for offboard button - connected to same output as SW1
- 6 pin breakout header - with two GPIO from the ARM Cortex-M0, Vsys, 5v0, 3v3, GND connections
- Header for optional off board solar panel / wind turbine etc.
- Optional RF Shield attachment - Harwin S02-20150300 (can also double as an inexpensive heatsink)
- Input voltage range - 4.2V – 10V
- Output voltage - 3.3V and 5V
- Output amperage - maximum current at 5V GPIO is 2.5A and at VSYS output 2.1A, but also this depends heavily on battery capacity. For BP7X have measured around 1.1A at 5V GPIO and around 1.6A at VSYS output. Obviously this also depends heavily on the current draw demanded by the Raspberry Pi / device itself. To achieve maximum of 2.5A it will need battery over 3500mAh.

## Additional content
Apart from the material available on Github we are also preparing guides, tutorials and additional information on various platforms and media. This is a list of current and future resources:

* Check the main [PiJuice Quick Start Guide and FAQ](https://learn.pi-supply.com/make/pijuice-quick-start-guide-faq/) for a detailed assembly guide
* Our [PiJuice Battery Discharge Time Calculator](https://learn.pi-supply.com/battery-levels/)  
* There is a "quick start guide" in the package [which you can see here](https://github.com/PiSupply/PiJuice/blob/master/Documentation/PiJuice%20Guide.pdf). As you can see it's pretty basic but really the PiJuice is quite intuitive and very easy to use.
* Pinout is available at [Raspberry Pi Pinout](https://pinout.xyz/pinout/pijuice)

# Reviews and links
Here are some links to reviews and articles from around the web:

* MagPi 5 star review - [PiJuice Review: Portable Power For Raspberry Pi](https://www.raspberrypi.org/magpi/pijuice-review/)
* Electromaker 4.5 star review - [A smart portable power solution for your Raspberry Pi](https://www.electromaker.io/blog/article/a-smart-portable-power-solution-for-your-raspberry-pi-34)
* RasPi.TV technical discussion - [PiJuice – testing the software and hardware plus 6W 40W solar panels video?](http://raspi.tv/2017/pijuice-testing-the-software-and-hardware-plus-6w-40w-solar-panels-video)
* RasPi.TV unboxing - [PiJuice – unboxing, first look and why it’s 2 years late?](http://raspi.tv/2017/pijuice-unboxing-first-look-and-why-its-2-years-late)
* Michael Horne / Tim Richardson review - [PiJuice – portable battery power for your Raspberry Pi – a review](http://www.recantha.co.uk/blog/?p=17790)
* Novaspirit Tech review / video - [PiJuice HAT Review](https://www.youtube.com/watch?v=VLNguCP7kjE)
* Lots of info and images and updates available on the [PiJuice Kickstarter](https://www.kickstarter.com/projects/pijuice/pijuice-a-portable-project-platform-for-every-rasp/)

# Third party software libraries and projects
It is safe to say we have an awesome and growing community of people using PiJuice to add battery power to their projects  - both from our Kickstarter and beyond. We get a huge amount of contributions of code, some of which we can easily integrate here and others which we can't (we are only a small team). However we want to make sure that any contributions are easy to find, for anyone looking. So here is a list of other software libraries that might be useful to you (if you have one of your own, please visit the "Issues" tab above and let us know!):

* Power outage monitor (using PiJuice and PaPiRus) by Frederick Vandenbosch. [You can find pictures and code here](http://frederickvandenbosch.be/?p=2876).
* Build a wildlife camera using a Raspberry Pi and PiJuice [by Electromaker](https://www.electromaker.io/blog/article/build-a-wildlife-camera-using-a-raspberry-pi-and-pijuice).
* CarPi - a Raspberry Pi, PiJuice and a HifiBerry all living neatly in a dashboard running Android Auto! [by Rick Tonoli](http://www.ricktonoli.com/2018/06/13/car-pi-hardware/).
* PiJuice on Windows IoT Core! You can find the [code here](https://github.com/tresoft/PiJuice-Uwp).
* [PiJuice](https://github.com/networkupstools/nut/blob/master/docs/man/pijuice.txt) is now supported by [Network UPS Tools](
https://github.com/networkupstools/nut/)!
* PiJuice deployment [with balenaCloud](https://github.com/balenalabs-incubator/balena-pijuice). You can also view the blog post about it [here](https://www.balena.io/blog/how-to-protect-your-device-with-pijuice-and-balenacloud/).
* Send info from [PiJuice](https://github.com/dalehumby/PiJuice-MQTT) to MQTT
* Integrate [PiJuice](https://github.com/Racailloux/home-assistant-pijuice) in the Home Assistant software.
