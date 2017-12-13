# Hardware
![PiJuice_Overview](https://user-images.githubusercontent.com/16068311/33771575-55e9593c-dc29-11e7-9e2c-3a0c1810c4c8.png "PiJuice Overview")

## Switches
On board of the PiJuice, highlighted in green, there are three switches and one dip switch. Please note that SW1 and J5 have the same function. J5 has been provided so that a separate tactile pushbutton can be used for ease of use for example on a custom case.
The following lists the default function/configuration:

### Buttons
* SW1/J5 is power button by default:
    * Single press to power on (release in less than 800 ms)
    * Long press of at least 10 seconds to halt
    * Long press of at least 20 seconds to cut power
* SW2 is user button by default, configured to trigger user scripts:
    * Single press in less than 400ms to invoke “USER_FUNC1”
    * Double press within 600ms to invoke “USER_FUNC2”
* SW3 is user button by default, configured to trigger user scripts:
    * Press will invoke “USER_FUNC3”
    * Release will invoke “USER_FUNC4”

Default settings can be overridden in "Buttons" tab of PiJuice HAT configuration window. Check the [software section ](https://github.com/PiSupply/PiJuice/blob/master/Software/README.md) for more information.
#### Special functions
* Dual long press of SW1 and SW2 for 20 seconds will reset PiJuice HAT configuration to default. This applies to the MCU configuration only.
* Holding pressed SW3 while powering up PiJuice will initiate the bootloader. This used only in cases when ordinary initiation through I2C does not works because of a damaged firmware.

### Dip Switch

The dip switch is preset for the BP7X battery.
We have also provided [this document](https://github.com/PiSupply/PiJuice/tree/master/Hardware/Batteries/Pijuice_battery_config.xlsx) that should help you to select desired battery profile via the dip switch.
Whether you use the dip switch and the resistor configuration by populating R20 you can always override the settings using the software configuration GUI. From the Pijuice HAT configuration window on the "Battery" tab choose battery profile from drop down list.
You can also return to dip switch selected profile by choosing "DEFAULT" from drop-down list.

## LEDs
On board of the PiJuice there are two multicolour LEDs highlighted in orange in the picture above.

**Coming soon**

## Connectors

On board of the PiJuice there are several connectors. The end user ones are highlighted in blue.

### Top of the board

- J4 is alternative to the onboard USB micro input. Pad holes are provided to solder 2.54mm header, any type (straight or right angle, female or male) and length can fit.

- The screw terminal can be used to connect NTC thermistor integrated with battery and used for temperature regulation of battery during charging.
The requirements are that battery uses 10KOhm NTC resistor with known thermistor B constant which can be entered as profile data in config GUI "Battery" tab.
There are regulation threshold data that can be entered for custom batteries like Cold, Cool, Warm and HOT temperatures that are derived from battery manufacturers specification.
The ID pin is not used and it is just reserved for possible future use. It could for example be used to automatically recognise which battery is connected when using BP7X or BP6X.

- P3 provides a VSYS pin which has the same function as VSYS on J3. It is a switchable battery voltage for system use and can be used with boards like Pibot.
VSYS switch is programmable via with software with "OFF", "ON 500mA current limit" and "ON 2100mA current limit".

### Bottom of the board

- J3 is alternative to the USB micro input and requires an input voltage between 4.2V and 10V. The minimum current source should provide to get charging is 80mA.
- J7 is a development header to be used in conjunction with ST-Link for the MCU. It is used to download firmware or perform debugging.
It can be also used during production to write the firmware. It requires a Tag-connect TC2050 ARM20-10 adapter to connect ST-Link to cable. This connector is not intended for end users.
- J6 is used to program the ID EEPROM during production. There is no adapter board and it needs to be wired manually to some programming tool.
It provides an additional option to program the EEPROM in case it cannot be pre-programmed before assembly. This connector is not intended for end users.

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

P3 Pinout
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

## Components

### Main active components

![Main ICs](https://user-images.githubusercontent.com/16068311/33900058-345d3218-df65-11e7-9335-7973c1a7a599.png "Main ICs")

The picture above highlights the main ICs used on PiJuice. Links to the various datasheets have been provided in line with the description.

1. Microcontroller is an [ST Micro STM32F030CCT6](https://github.com/PiSupply/PiJuice/tree/master/Hardware/STM32F030CCT6.pdf) ARM Cortex-M0, 48MHz, F64KB, R8KB, I2C, SPI, USART, 2.4-3.6V
2. Charge IC - [BQ24160RGET](https://github.com/PiSupply/PiJuice/tree/master/Hardware/BQ24160RGET.pdf) Charger IC Lithium-Ion/Polymer, 2.5A, 4.2-10V
3. Fuel gauge IC - [LC709203FQH-01TWG](https://github.com/PiSupply/PiJuice/tree/master/Hardware/LC709203FQH.pdf) Battery Fuel Gauge, 1-Cell Li-ion, 2.8%
4. High side power-distribution switch - [NCP380LMUAJAATBG](https://github.com/PiSupply/PiJuice/tree/master/Hardware/NCP380-D.pdf) Fixed/Adjustable Current‐Limiting Power‐Distribution Switch
5. EEPROM - [CAT24C32WI-GT3](https://github.com/PiSupply/PiJuice/tree/master/Hardware/CAT24C32-D.pdf) EEPROM, I2C, 32KBIT, 400KHZ, 1V7-5V5

### Unpopulated

You may notice that there are several components which have not be installed on your board. This section aims to explain what they are and which are for user customisation.

- TP1, 2 and 3 should be used to install the pogo pin for the wake on function. Each is located with respect to the "Run" pad on the various Raspberry Pi layouts.
    - TP1 - Pi 3B
    - TP2 - Pi Zero
    - TP3 - Pi B+ and 2B

- R20 is place to solder resistor that provides an additional way to configure battery profile without using software configuration additional to dip switch where the charging current and the charging voltage are encoded with resistance of resistor.
This approach also allows for a wider choice of battery profiles. You can in fact choose 16 profiles with as many resistor values as opposed to 4 via the dip switch. Please refer to [this document](https://github.com/PiSupply/PiJuice/tree/master/Hardware/Batteries/Pijuice_battery_config.xlsx).
In the "Charge settings" you can find how to choose the resistor value for desired charge settings.
In the "Profile selection" you can find how to choose the resistor to select one of predefined battery profiles.
*Note that R20 should be through hole and with 0.1% tolerance.*
It is also possible to override the resistor settings in software. In Pijuice HAT configuration window on the "Battery" tab check "Custom" and edit battery charging parameters to desired values, then click apply. You can return to resistor settings by choosing "DEFAULT" from the drop-down list.

- R13, R22, R51 and C31 are hardware configuration options for measuring battery temperature using NTC, alternative to fuel gauge and this is mostly for development purposes not for end users.

- R26 is reserved for future development use to add different possibilities with resistor configuration.

- D3 is a charger IC status LED. It is on during active charging of charger IC and it is used for firmware development purposes and debugging.

- TP4-TP23 are test points used during the production and test phase of the board. They provide a mean to temporarily connect custom bed of nails or spring pins and not intended for end users.

## Power management and batteries

### Which micro USB connector to use

PiJuice allows for multiple ways of providing power to its battery and to the Raspberry Pi. When deciding if to use the Pi's micro USB or the PiJuice micro USB you need to take into consideration the following:
- Powering from the Pi's micro USB
    - Powering from the Pi's input is more efficient, this is usually an advantage in UPS applications, but the maximum charger input from the GPIO input is 1.5A.
- Powering from the PiJuice's micro USB
    - Powering via the PiJuice micro USB offers a wider input voltage range. 
    - Additionally the maximum current from HAT USB micro is 2.5A, but it needs to go through two regulators to power the Raspberry Pi and is hence less efficient.
This input should be used in most battery powered applications and harvesting sources like solar panels.
    - If you wish to start your Raspberry Pi using the PiJuice SW1 you'll have to connect the micro USB to the PiJuice directly.

- When plugging a 2.5A PSU into the onboard microUSB port on PiJuice, it only takes 0.75A - is that due to the charge IC and charge profile? 
It is normal because pijuice has an efficient charger, based on switching not linear principle so calculation for input current when charging no load is
Iinput ~ Ibat * Vbat/Vin / k, for exmple in case Vbat = 3.7, Vin = 5V, Ibat = 0.925, k ~ 0.92 efficiency coefficient, gives around 0.75A. it depends on charging current Ibat from mentioned equation. Charging current is settable and differs for different batteries, for pkcell Ibat is set to 2.5A. Charging current is specified by battery manufacturer but general spec if no data available is around 0.5C where C is battery capacity.

### Max. current that the PiJuice will be able to supply 

The maximum current at 5V via the GPIO is 2.5A and via VSYS is 2.1A. This is also depending on battery capacity.
For BP7X for example we have measured around 1.1A at 5V GPIO and around 1.6A at VSYS output.
To achieve the maximum of 2.5A you will need to use a battery of at least 3500mAh.

### Which batteries can be used with PiJuice

You can use any single cell lipo with PiJuice as far as you configure the board and the software correctly. You cannot use cells/batteries in parallel or series.

### Battery profiles

The best way to generate new battery profiles when the datasheet is unknown and for generic batteries is to use values for charging current that can be found on the Internet for Li-ion batteries.
A rule of thumb is 0.5 x C/h, where C is the battery capacity.
Battery regulation voltage 4.2V typically. Setting lower value will reduce energy but can somewhat extend battery life especially in UPS applications where most of time there is power source connected.
For temperature thresholds, hot, cold, warm, cool, there are some standard as referenced in the JEITA compliance.
There are two other sheets in the [excel file](https://github.com/PiSupply/PiJuice/tree/master/Hardware/Batteries/Pijuice_battery_config.xlsx)
Profile selection and Charge settings where you have a column R [KOhm] for R20 resistance choices.

### Battery charge/discharge

**Coming soon**

### Battery charge level notes

There is a known quirk for Lithium ion batteries whilst charging/discharging and the specific "fuel gauge" IC we are using on PiJuice.
It is effectively about the measuring principle of the fuel gauge IC, that measures battery impedance to estimate charge level. Due to parasitic impedance (mostly internal battery protection circuit - such modern phones tablets won't have, but they tightly control their manufacturer of batteries which is far harder for a low volume product) there are measuring errors especially while charging because there are big currents over 1 amp. We took the attitude that safety is a priority over charge level accuracy.
Purpose for charge level is to have estimation during discharging to know when it is near to empty... info that is useful for field applications. It is precise enough while powering Pi and discharging with no inputs. More accurate readings with this IC is to have fixed battery type without protection circuit before fuel gauge connection point - that is the usual case in phones or laptops. 
The protection method is integrated within the BP7X battery we are using, which is an older battery that as you can see was removable, that makes impedance measurement errors, it is about hardware not firmware. That is why on many older phones you do not even have charge level during charging but only blinking symbol. 
Attached is charge discharge test log, with charge level and voltage printed every minute during charging and discharging. There is initial rise during charging to over 50%, but discharge is pretty linear.
Last but not least, we will likely try to fix this in a future software update based on practical test data - but a) this is not a huge priority and b) it will need more test data than we are able to get right now (multiple boards over long period charging and discharging in different scenarios)

## Misc

- RF clips (M1-M4): You can use the optional RF Shield - Harwin S02-20150300 under the following circumnstances:
    - You experience greater heating of Pijuice board
    - When there is sensitive electronics around the board like radios or sensors hats sensitive to 1.5MHz and harmonics or 2.5MHz and harmonics that are frequencies of PiJuice regulators.