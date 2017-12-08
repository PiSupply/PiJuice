![PiJuice Logo](https://user-images.githubusercontent.com/16068311/30545031-58b8fec6-9c80-11e7-8b3a-5e1f3aefd86c.png?raw=true "PiJuice Logo")
# PiJuice
Resources for the [PiJuice range](https://www.pi-supply.com/?s=pijuice&post_type=product&tags=1&limit=5&ixwps=1) (complete with our revolutionary [PiAnywhere](https://www.pi-supply.com/product-tag/pianywhere/) technology – the best way to take your Pi off the grid!). Originally funded on [Kickstarter](https://www.kickstarter.com/projects/pijuice/pijuice-a-portable-project-platform-for-every-rasp/).

**Surround rethink where it should be**

## Board overview

PiJuice is a fully uninterruptable power supply that will always keep your Raspberry Pi powered. It has a real time clock (RTC) for time tracking and scheduled task when the Pi is offline. It aslo has an integrated microcontroller (MCU) chip which will manage soft shut down functionality and a true low power deep sleep state and intelligent start up.  

The tri-coloured RGB LEDs will allow you to keep track of the battery charge levels and other info. There are three programmable buttons to trigger events and customisable scripts aside from their predefined functions.
PiJuice only uses five of the Pi's GPIO pins (just power and I2C), the rest are free and made available via the stacking header which allows for other boards to be used along side PiJuice.
The board can be powered directly from the grid with the standard Raspberry Pi PSU, via an on board battery, via external batteries, solar panels, wind turbines and other renewable sources.

The board is has a Raspberry Pi HAT compatible layout, with onboard EEPROM. Although it has been designed for the Raspberry Pi A+, B+, 2B and 3B but it is also compatible with the Raspberry Pi Zero v1.3 and Zero W v1.1.

PiJuice is fully [CE and FCC tested](https://github.com/PiSupply/PiJuice/tree/master/Documentation/Compliance) design and [battery safety tests](https://github.com/PiSupply/PiJuice/tree/master/Documentation/Battery%20Safety) have also been carried out.

## Basic functionality
tbd-----------------------------

What's the difference between powering from the Pi or from Juice
Board layout description. Buttons, LEDs
Connectors
Batteries
Solar panels and other sources (wind thermal)

tbd-----------------------------

Once the battery is slot into place LED2 will start glowing green. This doesn't mean your Raspberry Pi is yet receiving power it is just telling you that the PiJuice MCU is receiving power and is operational.   

### Power On and Off the Pi
![Buttons_LEDs](https://user-images.githubusercontent.com/16068311/33768831-94db68b0-dc1f-11e7-99d4-a06cb65b0135.png "Buttons and LEDs")

To power on the Raspberry Pi press SW1 briefly (less than 5 seconds)------

### Providing power to the board

PiJuice can recharge its on board battery in several ways and via different power sources. The most common approach is to use the standard Raspberry Pi PSU or a micro USB PSU with similar characteristics.
When the PiJuice is installed over the Raspberry Pi you can power both the Pi and PiJuice via either the Pi's micro USB or the PiJuice's one. That's because the power lines on the GPIO header are shared by both boards and this allows to effectively be able to charge the battery independently from which micro USB connector you use.
Other ways of providing power to the PiJuice is directly via the GPIO pin headers or one of the other connectors on board. See the [Hardware section](https://github.com/PiSupply/PiJuice/tree/master/Hardware) for more information.

### Buttons and LEDs

LONG_PRESS1: press and hold at least 10 seconds to automatically halt (there should be red blinks on user led if successful).
LONG_PRESS2: press and hold at least 20 seconds to cut power if system is freezes and cannot be halted.

- can you tell me default operations of each button?
SW1 is power button by default:
·        Single press to power on (release in less than 800 ms),
·        Long press of at least 10 seconds to halt,
·        Long press of at least 20 seconds to cut power.
SW2 is user button by default, configured to trigger user scripts:
·        Single press in less than 400ms to invoke “USER_FUNC1”,
·        Double press within 600ms to invoke “USER_FUNC2”.
 
SW3 is user button by default, configured to trigger user scripts:
·        Press will invoke “USER_FUNC3”,
·        Release will invoke “USER_FUNC4”.
Default settings can be overridden in “Buttons” tab of Pijuice HAT configuration window.

There are fixed button functions:
·        Dual long press of SW1 and SW2 for 20 seconds will reset Pijuice HAT configuration to default (this applies to mcu configuration only).
·        Holding pressed SW3 while powering up Pijuice will initiate bootloader (used only in cases when ordinary initiation through I2C does not works because of damaged firmware).


- can you tell me default operations of each led?
D1 charge status by default
D2 user LED by default.

On board the PiJuice there are three buttons and two multicolour LEDs

Programmable multi-colored RGB led (x2) and buttons (x3) with super simple user-configurable options

### Power management
Onboard intelligent on/off switch 
Low power deep-sleep state with wake on interrupt/calendar event
Batteries can be charged from different type of sources and voltages

#### UPS functionality
Hardware watchdog timer to keep your Raspberry Pi up and running for mission-critical remote applications
A Full Uninterrupted / Uninterruptable Power Supply solution.
Onboard 1820 mAh “off the shelf” Lipo / LiIon battery for ~4 to 6 hours in constant use! (with support for larger Lipo Battery of 5000 or 10,000 mAH+ to last up to 24 hrs +)
Replace the battery without downtime. Compatible with any single cell LiPo or LiIon battery    


### GUI interface
Full power management API available to Raspberry Pi OS with auto shutdown capability when running low on batteries
Enhanced graphical user interface (GUI) available for easy install (via APT)
Customisable scripts for enhanced flexibility and full report of battery status

## Technical specs

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

## Content

We just wanted to give you an idea here, or where we will be putting all this content:

There is a "quick start guide" in the package [which you can see here](https://github.com/PiSupply/PiJuice/blob/master/Documentation/PiJuice%20Guide.pdf). As you can see it's pretty basic but really the PiJuice is quite intuitive and very easy to use. 
There will also be more technical information going up on the PiJuice GitHub here - https://github.com/PiSupply/PiJuice
And also a lot of picture guides and tutorials going up on our Maker Zone - https://www.pi-supply.com/make 
We will likely have the pinouts available at http://pinout.xyz
We will also have tutorials on Instructables and Hackster.io 

# Third party software libraries
