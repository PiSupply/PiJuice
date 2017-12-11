**Datasheets**

# Hardware
![PiJuice_Overview](https://user-images.githubusercontent.com/16068311/33771575-55e9593c-dc29-11e7-9e2c-3a0c1810c4c8.png "PiJuice Overview")

## Switches

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

### Defaults
- can you tell me specification of button to connect to J5? Just momentary push button? And it connects to same as one of the other buttons SW1-3? Which one?
It needs to be simple tactile pushbutton and connects parallel to SW1.

- with DIP switch, position in test document is for BP7X by default. Can you tell me, what are defaults for other 3 dip switch positions? And can these all be overridden in firmware/software? Can you explain exact process?
There is table in Pijuice_battery_config.xlsx for dip switch configuration options, how to select desired battery profile with switch code. Switch and resistor configuration can be overridden in software configuration, in Pijuice HAT configuration window on “Battery” tab choose battery profile from drop down list. You can also return to dip switch selected profile by choosing “DEFAULT” from drop-down list.

## LEDs

### Defaults

## Connectors
- can you tell me specification of solar panel to connect to J3?
Requirement for on board USB micro input is to provide voltage 4.2V – 10V, as labeled on board. Also minimum current source should provide to get charging is 80mA.

- can you tell me specification of what can be connected to J4?
J4 is alternative to onboard USB micro input. Pad holes to solder 2.54mm header, any type that can fit and user finds useful.

-What are pinout designations for spring battery connector J1 on Pijuice HAT?
Refer to the main picture

 - can you tell me purpose and pinouts for each of the tag connect cables J7 and J6 and how they can be used?
J7 is development header to connect ST-Link with mcu and download firmware or debug. Can be also used in production to write firmware. It needs Tag-connect TC2050 ARM20-10 adapter to connect ST-Link to cable.
J6 is for programming ID EEPROM in production. See signals below, there is no adapter board and needs to be wired manually to some programming tool. It is just option for programming if it is not possible to do with EEPROM programmer before assembly.

- TP1,2,3 Run TP1 Pi 3B, TP2 Pi Zero, TP3 Pi B+ 2B  Pogo pin----

- can you tell me the purpose and pinouts for TP4 to TP23?
TP4-TP23 are test points with possible use in production test to connect to custom bed of nails or spring pins. This should not be in user guide and purpose of each is about connections in schematic.

## Pinout
P1 pinout
```text
P1
-----------------------------------------------------------------------------------------------------------------------
| 2     4     6     8    10    12    14    16    18    20    22    24    26    28    30    32    34    36    38    40 |
|5V    5V    GND   o     o     o     GND   o     o     GND   o     o     o     +     GND   o     GND   o     o     o  |
|3V3   #     #     o     GND   o     o     o     3V3   o     o     o     GND   +     o     o     o     o     o     GND|
| 1     3     5     7     9    11    13    15    17    19    21    23    25    27    29    31    33    35    37    39 |
-----------------------------------------------------------------------------------------------------------------------

# Used
o Available
+ Can be disabled

3 I2C_SDA to MCU
5 I2C_SCL to MCU
27 I2C_SDA to HAT EEPROM
28 I2C_SDL to HAT EEPROM
```

[P3 Pinout](#P3-Pinout)
```text
P3
--------------------------------------
| 1     2     3     4     5     6    |
|VSYS  5V    GND   3V3   IO1   IO2   |
--------------------------------------
```

J2 Pinout
```text
J2
------------------------
| 1     2     3     4  |
|GND   NTC   ID    VBAT|
------------------------
```

J3 Pinout
```text
J3
------------
| 1     2  |
|GND   VSYS|
------------
```

J4 Pinout
```text
J4
------------
| 1     2  |
|VIN   GND |
------------
```

J6 Pinout
```text
J6
-----------------
| 2     4     6 |
|GND   SDA   NC |
|3V3   SCL   WP |
| 1     3     5 |
-----------------
```

- can you tell me specification of NTC and ID on battery terminals?
NTC terminal can be used to connect NTC thermistor integrated with battery and used for temperature regulation of battery during charging. Requirement is that battery uses 10KOhm NTC resistor with known thermistor B constant that can be entered as profile data in config GUI “battery” tab. There are regulation threshold data that can be entered for custom batteries like Cold, Cool, Warm and HOT temperatures that are derived from battery manufacturers specification.
ID is not used it is just reserved for possible future use, because there is ID on BP7X, BP6X so possibility to identify it automatically.

- on P3 what is vsys - battery voltage? Vcc is 5v0? What is current capacity of these pins and 3v3? And for the two IO pins, how can those be utilized and what pins on ARM cortex do they connect to?
VSYS on P3 is same as VSYS on J3 and is switchable battery voltage for system use, like connecting to Pibot power input. VSYS switch is programmable with software with “OFF”, “ON 500mA current limit” and “ON 2100mA current limit”.

## Components

### Unpopulated
- R13, R22, R51 are hardware configuration options for measuring battery temperature using NTC,
alternative to fuel gauge and this is mostly for development purposes not important for user.

- C31 for development purposes.

- R20 is place to solder resistor as additional way to configure battery profile without using software configuration additional to dip switch, where charging current and charging voltage are encoded with resistance of resistor. Also user can select predefined profile by choosing one of 16 with resistor value instead with dip switch which is limited to 4. There is table within excel document Pijuice_battery_config.xlsx  sheet “Charge settings” how to choose resistor for desired charge settings and sheet “Profile selection” how to choose resistor to select one of predefined battery profiles. R20 should be through hole 0.1% tolerance.
It is possible to override resistor settings in software. In Pijuice HAT configuration window on “Battery” tab check “Custom” and edit battery charging parameters to desired values, then click apply. You can also return to resistor settings by choosing “DEFAULT” from drop-down list.

- R26 is reserved for future development use to add different possibilities with resistor configuration.

- D3 is charger ic status LED, is on during active charging of charger ic and is used for firmware development purposes and debugging.

## Power management
- What is the difference between powering from the Pi's micro USB and the PiJuice micro USB?
Powering from on rpi input is more efficient, this is usually advantage in UPS applications, but maximum charger input from gpio input is 1.5A. Maximum current from HAT USB micro is 2.5A, but it passes two regulators to power rpi and is hence less efficient, and this input should be used if most battery powered applications and harvesting sources like solar panels.
Also on HAT input has wider input voltage range.

- What's the best way to generate new battery profiles where datasheet is unknown and for generic batteries.
Some good values for charging current that appears on internet for Li-ion batteries is 0.5 x C/h, where C is capacity.
Battery regulation voltage 4.2V typicaly. Setting lower value will reduce energy but can somewhat extend battery life 
especially in UPS applications where most of time there is power source connected.
For temperature thresholds, hot, cold, warm, cool, there are some standard as reference: JEITA compliance.

- When plugging a 2.5A PSU into the onboard microUSB port on PiJuice, it only takes 0.75A - is that due to the charge IC and charge profile? 
It is normal because pijuice has an efficient charger, based on switching not linear principle so calculation for input current when charging no load is
Iinput ~ Ibat * Vbat/Vin / k, for exmple in case Vbat = 3.7, Vin = 5V, Ibat = 0.925, k ~ 0.92 efficiency coefficient, gives around 0.75A. it depends on charging current Ibat from mentioned equation. Charging current is settable and differs for different batteries, for pkcell Ibat is set to 2.5A. Charging current is specified by battery manufacturer but general spec if no data available is around 0.5C where C is battery capacity.

### Max. current that the juice will be able to supply? 

- Maximum current at 5V gpio is 2.5A, as VSYS output 2.1A, but also depends on battery capacity.
For BP7X have measured around 1.1A at 5V GPIO and around 1.6A at VSYS output.
To achieve maximum of 2.5A it will need battery over 3500mAh.

## Batteries

- Single cell lipo - can't do cells/batteries in parallel or series.

### Battery profiles
there are two other sheets in excel file: Profile selection and Charge settings where you have 
column R [KOhm] for R20 resistance choices. It needs to download excel cannot see additional sheets 
from browser.

### Battery charge/discharge

### Battery charge level notes

This is basically a known quirk of lithium ion batteries whilst charging / discharging and the specific "fuel gauge" IC we are using.
It is effectively about the measuring principle of the fuel gauge IC, that measures battery impedance to estimate charge level. Due to parasitic impedance (mostly internal battery protection circuit - such modern phones tablets won't have, but they tightly control their manufacturer of batteries which is far harder for a low volume product) there are measuring errors especially while charging because there are big currents over 1 amp. We took the attitude that safety is a priority over charge level accuracy.
Purpose for charge level is to have estimation during discharging to know when it is near to empty... info that is useful for field applications. It is precise enough while powering Pi and discharging with no inputs. More accurate readings with this IC is to have fixed battery type without protection circuit before fuel gauge connection point - that is the usual case in phones or laptops. 
The protection method is integrated within the BP7X battery we are using, which is an older battery that as you can see was removable, that makes impedance measurement errors, it is about hardware not firmware. That is why on many older phones you do not even have charge level during charging but only blinking symbol. 
Attached is charge discharge test log, with charge level and voltage printed every minute during charging and discharging. There is initial rise during charging to over 50%, but discharge is pretty linear.
Last but not least, we will likely try to fix this in a future software update based on practical test data - but a) this is not a huge priority and b) it will need more test data than we are able to get right now (multiple boards over long period charging and discharging in different scenarios)

## Misc
 
- can you tell me under what circumstances somebody would want to install the RF should onto clips?
In cases where it is experienced greater heating of Pijuice board, or in cases there are sensitive electronics around like radios or sensors hats sensitive to 1.5MHz and harmonics or 2.5MHz and harmonics that are frequencies of pijuice regulators.