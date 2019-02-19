# Hardware
![PiJuice_Overview](https://user-images.githubusercontent.com/16068311/36384527-7d562e2c-1587-11e8-81d1-d78835ff8e67.png "PiJuice Overview")

![PiJuice_Zero](https://drive.google.com/uc?id=1l1uLXDp_rf0GQmAKm7W06QNgxjKIoULq)

The above images of the PiJuice PCB are used in the following descriptions to highlight some of the inputs / outputs and other useful hardware information.

## Switches
On board the PiJuice, highlighted in green in the above overview images, there are momentary switches and one DIP switch(PiJuice HAT). Please note that SW1 and J5 have the same function...J5 has been provided so that a separate tactile pushbutton can be used for ease of use for example on a custom case (similar to a reset or power button on a normal computer).

The following lists the default function/configuration (these can be easily overwritten in the software GUI/JSON file - see below):

### Buttons

* **SW1/J5** is power button by default:
    * Single press to power on (release in less than 800 ms)
    * Long press of at least 10 seconds to halt
    * Long press of at least 20 seconds to cut power
* **SW2** is user button by default, configured to trigger user scripts:
    * Single press in less than 400ms to invoke “USER_FUNC1”
    * Double press within 600ms to invoke “USER_FUNC2”
* **SW3** is user button by default, configured to trigger user scripts:
    * Press will invoke “USER_FUNC3”
    * Release will invoke “USER_FUNC4”

Default settings can be overridden in the "Buttons" tab of PiJuice HAT configuration window. Check the [software section](https://github.com/PiSupply/PiJuice/blob/master/Software/README.md) for more information.

#### Special functions

* Dual long press of SW1 and SW2 for 20 seconds will reset PiJuice HAT configuration to default. This applies to the MCU configuration only.
* Holding pressed SW3 while powering up PiJuice HAT will initiate the bootloader. This is used only in cases when ordinary initiation through I2C does not work because of damaged firmware.

### DIP Switch

The DIP switch is preset for the BP7X battery that we supply with every PiJuice HAT.

You can set the DIP Switch for four predefined battery profiles.

![PiJuice DIP Switch Settings](https://user-images.githubusercontent.com/16068311/34769251-25c7c3b6-f5f5-11e7-971f-e93f5d4d3cc0.jpg "PiJuice DIP Switch Settings")

```text
BP6X
-ON---CTS-
[OFF][OFF]
--1----2--

SNN5843
-ON---CTS-
[OFF][ON ]
--1----2--

BP7X (Default)
-ON---CTS-
[ON ][OFF]
--1----2--

LIP08047109
-ON---CTS-
[ON ][ON ]
--1----2--
```

We have also provided [this document](https://github.com/PiSupply/PiJuice/tree/master/Hardware/Batteries/Pijuice_battery_config.xlsx) that should help you to select desired battery profile via the DIP switch.

Whether you use the DIP switch and the resistor configuration by populating R20 you can always override the settings using the software configuration GUI. From the Pijuice HAT configuration window on the "Battery" tab choose battery profile from drop down list. You can also return to DIP switch selected profile by choosing "DEFAULT" from drop-down list.

*Note that it is not possible to detect battery not present when powered through on board USB micro, so it might show 0% only.*

## LEDs

On board of the PiJuice there are two multicolour LEDs highlighted in orange in the picture above.

* **LED1** (D1) is the charge status by default. It will blink when the MCU detects the current load to be below 50mA. The MCU will then set itself in Low Power Mode to conserve as much as possible energy.
When the Pi is on or in halt state the LED will be a steady colour with exception of the charging mode which will always be blinking as far as there is a battery charging.
    * ![#00FF00](http://via.placeholder.com/15x15/00FF00/000000?text=+) Green: Power on - Battery over 50%
    * ![#0000FF](http://via.placeholder.com/15x15/0000FF/000000?text=+) Blue: Charging
    * ![#FFA500](http://via.placeholder.com/15x15/FFA500/000000?text=+) Orange: Low battery - level < 50%
    * ![#FF0000](http://via.placeholder.com/15x15/FF0000/000000?text=+) Red: Low battery - level < 15% or battery absent
* **LED2** (D2) is the user LED by default.

You can look at the [demo script](https://github.com/PiSupply/PiJuice/blob/master/Software/Test/pijuicetest.py) provided in the code base to see how to use it in Python.

Default settings can be overridden in the "LEDs" tab of PiJuice HAT configuration window. Check the [software section ](https://github.com/PiSupply/PiJuice/blob/master/Software/README.md) for more information.

## Connectors

On board the PiJuice there are several connectors. The connectors / headers available to the end user are highlighted in blue.

### Top of the board

* **J4** is an alternative to the PiJuice microUSB micro input. Pad holes are provided to solder a 2.54mm header...any type (straight or right angle, female or male) and length can fit.
  You can plug in power sources to this connector with input voltage between 4.2V and 10V. A minimum current of 80mA will be sufficient for the battery to charge.
  *Note: Although this connector may be used for some types of solar panels be aware that we only support the use of the official panels.*

* The **screw terminal** can be used to connect an off-board / external battery instead of the provided onboard battery (the BP7X). We recommend you use a battery with an NTC thermistor integrated with the battery and used for temperature regulation of battery during charging we also recommend that the battery have onboard over charge / over current etc. protection (this is not essential, but it is far safer if you have them - lithium batteries can be dangerous so it is wise to think about safety). The requirements are that battery uses a 10KOhm NTC resistor with known thermistor B constant which can be entered as profile data in config GUI "Battery" tab. There are regulation threshold data points that can be entered for custom batteries like Cold, Cool, Warm and HOT temperatures that are derived from the battery manufacturers specification. The ID pin is not used and it is just reserved for possible future use. It could for example be used to automatically recognise which battery is connected when using BP7X or BP6X. The screw terminal is intentionally facing inwards, towards the onboard battery, for safety...**this is not a mistake!** The idea is to make it harder for you to accidentally plug in two batteries at once and fry your PiJuice. If you would like to turn the screw terminal to face the other direction you can do so by unscrewing all four screws, turning it around, and then screwing them back in again. Please note that you do this at your own risk and it will void your warranty.

* **J2** header on the **PiJuice Zero** provides the same functionality as the screw terminals on the PiJuice HAT. This header allows you to directly connect a single cell Lithium polymer battery with 3 terminals to the PiJuice Zero, providing VBAT, GND and NTC connections.

* **P3** is an expansion header which provides access to two unused GPIO pins on the ARM Cortex M0 (STM32-F0) MCU on board the PiJuice. There is also a regulated 3V3 and 5V0 pin, a GND pin and a VSYS pin which has the same function as VSYS on J3. VSYS is a switchable battery voltage for system use and can be used with boards like PiBot to provide power. VSYS output is programmable via with software with "OFF", "ON 500mA current limit" and "ON 2100mA current limit".
  The 5V pin is wired with the GPIO header and is then share amongst the Raspberry Pi's electronics and the PiJuice for battery charging. The available current that this pin can supply is around 800mA.
  As for the 3V3 a maximum of 100mA sourced by the internal LDO.

### Bottom of the board (**PiJuice HAT only**)

* **J3** provides the same VSYS as per P3. J3 is a battery output for external load connections with current limit, we use this for external boards such as PiBot. Current limit prevents power loss on the Raspberry Pi if load draws excessive current.
* **J7** is a development header to be used in conjunction with an ST-Link programmer for the MCU and a [Tag-Connect cable](http://www.tag-connect.com/TC2050-IDC). It is used to download firmware or perform debugging. It can be also used during production to write the firmware. It requires a [Tag-connect TC2050 ARM20-10](http://www.tag-connect.com/TC2050-ARM2010) adapter to connect ST-Link to cable. This connector is not intended for end users.
* **J6** is used to program the ID EEPROM during production. There is no adapter board and it needs to be wired manually to some programming tool. It requires a [Tag-Connect cable](http://www.tag-connect.com/TC2030-IDC). It provides an additional option to program the EEPROM in case it cannot be pre-programmed before assembly. This connector is not intended for end users.

## Pinout

#### P1 pinout
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
+ Can be reused

3 I2C_SDA to MCU
5 I2C_SCL to MCU
27 I2C_SDA to HAT EEPROM
28 I2C_SDL to HAT EEPROM
```

#### P3 Pinout
```text
P3
--------------------------------------
| 1     2     3     4     5     6    |
|VSYS  5V    GND   3V3   IO1   IO2   |
--------------------------------------
```

#### J1 Pinout
```text
J1
------------------------
| 1     2     3     4  |
|VBAT  ID    NTC   GND |
------------------------
```

#### J2 Pinout
```text
J2
------------------------
| 1     2     3     4  |
|GND   NTC   ID    VBAT|
------------------------
```

#### J2 Pinout PiJuice Zero
```text
J2
----------------------
|  1      2     3    |
| VBAT   GND   NTC   |
----------------------
```

#### J3 Pinout
```text
J3
------------
| 1     2  |
|GND   VSYS|
------------
```

#### J4 Pinout
```text
J4
------------
| 1     2  |
|VIN   GND |
------------
```

#### J6 Pinout
```text
J6
-----------------
| 2     4     6 |
|GND   SDA   NC |
|3V3   SCL   WP |
| 1     3     5 |
-----------------
```

#### J7 Pinout
```text
J7
-------------------------------
| 10    9     8     7     6   |
|NRST  GND   SDA1  SCL1 BOOT0 |
|3V3  SWDIO  GND SWDLCK  GND  |
| 1     2     3     4     5   |
-------------------------------
```

## Components

### Main active components

![Main ICs](https://user-images.githubusercontent.com/16068311/33900058-345d3218-df65-11e7-9335-7973c1a7a599.png "Main ICs")

![PiJuice Zero ICs](https://drive.google.com/uc?id=1ElkbDf5VM5ce5g07ztZ3CUCaMfaSJwVZ)

The images above highlight the main ICs used on PiJuice. Links to the various datasheets have been provided in line with the description.

1. **Microcontroller** is an [ST Micro STM32F030CCT6](https://github.com/PiSupply/PiJuice/tree/master/Hardware/STM32F030CCT6.pdf) ARM Cortex-M0, 48MHz, F64KB, R8KB, I2C, SPI, USART, 2.4-3.6V
2. **Charge IC** - [BQ24160RGET](https://github.com/PiSupply/PiJuice/tree/master/Hardware/BQ24160RGET.pdf) Charger IC Lithium-Ion/Polymer, 2.5A, 4.2-10V
3. **Fuel gauge IC** - [LC709203FQH-01TWG](https://github.com/PiSupply/PiJuice/tree/master/Hardware/LC709203FQH.pdf) Battery Fuel Gauge, 1-Cell Li-ion, 2.8%
4. **High side power-distribution switch** - [NCP380LMUAJAATBG](https://github.com/PiSupply/PiJuice/tree/master/Hardware/NCP380-D.pdf) Fixed/Adjustable Current‐Limiting Power‐Distribution Switch
5. **EEPROM** - [CAT24C32WI-GT3](https://github.com/PiSupply/PiJuice/tree/master/Hardware/CAT24C32-D.pdf) EEPROM, I2C, 32KBIT, 400KHZ, 1V7-5V5

### Unpopulated

You may notice that there are several components which have not be installed on your board. This section aims to explain what they are and which are for user customisation.

* **TP1, 2 and 3** should be used to install the pogo / spring pin (Mill-Max 0929-7-15-20-75-14-11-0 or 0929-7-15-20-77-14-11-0) for the wake on function. Each is located with respect to the "Run" pad on the various Raspberry Pi layouts:
    - TP1 - Pi 3B
    - TP2 - Pi Zero
    - TP3 - Pi B+ and 2B

**Note:** As of Firmware v1.3_2019_01_15 pogo pin is no longer required to wakeup the Raspberry Pi when powering from the Pi micro USB. A single press of SW1 on the PiJuice will wakeup the Pi or a short of GPIO pins 5 & 6 will also wakeup the Pi. This method has been introduced to accommodate the new Raspberry Pi boards and any future boards.

* **R20** is place to solder resistor that provides an additional way to configure battery profile without using software configuration additional to DIP switch where the charging current and the charging voltage are encoded with the resistance of the resistor. This approach also allows for a wider choice of battery profiles. You can in fact choose 16 profiles with as many resistor values as opposed to 4 via the dip switch. Please refer to [this document](https://github.com/PiSupply/PiJuice/tree/master/Hardware/Batteries/Pijuice_battery_config.xlsx). In the "Charge settings" you can find how to choose the resistor value for desired charge settings. In the "Profile selection" you can find how to choose the resistor to select one of predefined battery profiles. *Note that R20 should be through hole and with 0.1% tolerance.* It is also possible to override the resistor settings in software. In the Pijuice HAT configuration window on the "Battery" tab check "Custom" and edit battery charging parameters to desired values, then click apply. You can return to resistor settings by choosing "DEFAULT" from the drop-down list.

* **R13, R22, R51 and C31** are hardware configuration options for measuring battery temperature using NTC and an alternative to fuel gauge and these are mostly for development purposes not for end users.

* **R26** is reserved for future development and is used to add different possibilities with resistor configuration.

* **D3** is a charger IC status LED. It is on during active charging of charger IC and it is used for firmware development purposes and debugging.

* **TP4-TP23** are test points used during the production and test phase of the board. They provide a means to temporarily connect custom bed of nails or spring pins and not intended for end users.

## Power management and batteries

### USB Micro Input
PiJuice features configurable current limit at USB Micro power input. One of two settings can be selected, 1.5A and 2.5A. PiJuice will limit current draw from connected source even if it needs more to cover total demand for charging battery and supplying output Rails. This may be useful to set lower limit for weaker sources that usually will shut-down when overloaded.
Another USB Micro power input feature is Dynamic Power Management Loop (DPM). This feature limits voltage drop at power input to configured minimum. If power source is weak and cannot supply enough current to fully charge battery plus provide loading demand than PiJuice will limit current draw to level that will prevent voltage to fall below preset DPM preventing power source crashing. This is excellent feature for non-reliable sources like energy harvesting and solar panels.

### 5V Power Regulator
5V Power regulator supplies 5V power Rail with up to 2.5A of continuous current. Regulator can be set in one of 3 modes of operation:
* **DCDC Switching Mode**. In this mode 5V Rail voltage is regulated to 5V with 2.5% tolerance, typically 5.07V at mid-loaded conditions. This is the most efficient operation mode.
* **LDO Mode**. This mode regulates 5V Rail voltage to 4.79V. In this mode output voltage has lowest output ripple.
* **POWER SOURCE DETECTION Mode**. In this mode regulator switches between DCDC switching mode and LDO mode with most of time in DCDC switching mode. This is mode has high efficiency but increased voltage ripple.

### 5V GPIO Rail power source detection
Pijuice can detect when power source is connected through 5V GPIO rail. If power source at 5V GPIO Rail is detected pijuice will take power from it to charge battery. If power source is not detected pijuice will automatically supply 5V GPIO Rail with 5V regulator powered from battery. For reliable Power source detection 5v Power regulator needs to be set in POWER SOURCE DETECTION Mode or LDO Mode and power source connected to 5V GPIO Rail needs to provide voltage between 4.8V and 5.25V.

### Automatic wake-up
PiJuice can be configured to automatically wake-up system in several ways: on charge level, when power source appears, by RTC Alarm.
* **Wakeup on charge**: When power source appears and battery starts charging this function can be configured to wakeup system when charge level reaches settable trigger level. Trigger level can be set in range 0%-100%. Setting trigger level to 0 means that system will wake-up as soon as power source appears and battery starts charging.
* **Wake-up Alarm**: PiJuice features real-time clock with Alarm function that can be configured to wake-up system at programmed date/time.

### Which micro USB connector to use

PiJuice allows for multiple ways of providing power to its battery and to the Raspberry Pi. When deciding whether to use the Pi's micro USB or the PiJuice micro USB you need to take into consideration the following:
* **Powering from the Pi's micro USB**
    - Powering from the Pi's input is more efficient, this is usually an advantage in UPS applications, but the maximum charger input from the GPIO pins is 1.5A.
    - Requires installation of the spring / pogo pin to do timed wake ups of the Pi and to use SW1 to wake the Pi
* **Powering from the PiJuice's micro USB**
    - Powering via the PiJuice micro USB offers a wider input voltage range.
    - Additionally the maximum current from HAT USB micro is 2.5A, but it needs to go through two regulators to power the Raspberry Pi and is hence less efficient. This input should be used in most battery powered applications and harvesting sources like solar panels.
    - If you wish to start your Raspberry Pi using the PiJuice SW1 you'll have to connect the micro USB to the PiJuice directly (unless you use a spring pin)

When plugging a 2.5A PSU (or a solar panel or other device with more capacity) into the onboard microUSB port on PiJuice, you may notice that with the BP7X battery it only draws 0.75A. This is normal because PiJuice has an efficient charger, based on switching not linear principle so calculation for input current when charging with no load is `Iinput ~ Ibat * Vbat/Vin / k`, for exmple in case `Vbat = 3.7`, `Vin = 5V`, `Ibat = 0.925`, `k ~ 0.92` efficiency coefficient, gives around 0.75A. So the current draw depends on charging current Ibat from the mentioned equation. Charging current is configurable and differs for different batteries (the figures used in example are for the BP7X), for the 5000mAH pkcell battery that we offer Ibat is set to 2.5A. Charging current is specified by battery manufacturer but general spec if no data available is around 0.5C where C is battery capacity.

### Max. current that the PiJuice will be able to supply

The maximum current at 5V via the GPIO is 2.5A and via VSYS is 2.1A. This is also dependent on battery capacity. For BP7X battery for example we have measured around 1.1A at 5V GPIO and around 1.6A at VSYS output. To achieve the maximum of 2.5A you will need to use a battery of at least 3500mAh.

### Which batteries can be used with PiJuice

You can use any single cell lipo with the PiJuice as long as you configure the board and the software correctly. You cannot use cells/batteries in parallel or series. As mentioned above we recommend that you use a battery with an NTC temperature sensor on board as well as on board protection circuitry (overcharge / overcurrent etc). Please note that you use your own battery at your own cost and risk and we do not officially support using it in this configuration as there are far too many variables - so you are on your own if there are any issues or if you break your Pi or PiJuice or damage anything else.

We intend to create add on boards as accessories in the future which will allow use with other battery technologies but we do not have anything like this right now.

### Battery profiles

The best way to generate new battery profiles when the datasheet is unknown and for generic batteries is to use values for charging current that can be found on the Internet for Li-ion batteries. A good rule of thumb is 0.5 x C/h, where C is the battery capacity. Battery regulation voltage is 4.2V typically. Setting lower values will reduce energy but can somewhat extend battery life especially in UPS applications where most of time there is a power source connected. For temperature thresholds, hot, cold, warm, cool, there are some standards as referenced in the [JEITA compliance](http://www.ti.com/lit/an/slyt365/slyt365.pdf).

There are two other sheets in the [excel file](https://github.com/PiSupply/PiJuice/tree/master/Hardware/Batteries/Pijuice_battery_config.xlsx) Profile selection and Charge settings where you have a column R [KOhm] for R20 resistance choices.

Battery profile defines charging, discharging parameters for PiJuice Battery. Also profile defines normal working conditions according JEITA safety standard, and temperature sensor parameters if integrated with battery.
* **Capacity**. Charge capacity of battery.
* **Charge current**. [550mA – 2500mA]. Defines constant current that PiJuice battery is charged in current regulation phase of charging process.
* **Termination current**. [50mA – 400mA]. When charging current drops below termination current threshold in voltage regulation phase charging process terminates.
* **Regulation voltage**. [3500mV – 4440mV]. Defines constant voltage to which voltage over battery is regulated in voltage regulation phase of charging process.
* **Cut-off voltage**. [0mV – 5100mV]. Defines minimum voltage at which battery is fully discharged.
* **Cold temperature**. Defines temperature threshold according to JEITA standard below which charging is suspended.
* **Cool temperature**. Defines temperature threshold according to JEITA standard below which charge current is reduced to half of programmed charge current. This threshold should be set above cold temperature.
* **Warm temperature**. Defines temperature threshold according to JEITA standard above which the battery regulation voltage is reduced by 140mV from the programmed regulation voltage. This threshold should be set above cool temperature.
* **Hot temperature**.  Defines temperature threshold according to JEITA standard above which charging is suspended. This threshold should be set above warm temperature.
* **NTC  B constant**. Defines thermistor B constant of NTC temperature sensor if it is integrated with battery.
* **NTC resistance**. Defines nominal thermistor resistance at 25°C of NTC temperature sensor if it is integrated with battery.

#### Predefined Profiles

PiJuice has set of predefined profiles for set of tested compatible batteries that can be selected if one of batteries is used.
* (0) BP6X
  - Capacity: 1400 mAh,
  - Charge current: 850 mA,
  - Termination current: 50 mA,
  - Regulation voltage: 4180 mV,
  - Cut-off voltage: 3000 mV,
  - Cold temperature: 1 °C,
  - Cool temperature: 10 °C,
  - Warm temperature: 45 °C,
  - Hot temperature: 59 °C,
  - NTC  B constant: 3380 K,
  - NTC resistance 10000 Ω.

* (1) BP7X
  - Capacity: 1820 mAh,
  - Charge current: 925 mA,
  - Termination current: 50 mA,
  - Regulation voltage: 4180 mV,
  - Cut-off voltage: 3000 mV,
  - Cold temperature: 1 °C,
  - Cool temperature: 10 °C,
  - Warm temperature: 45 °C,
  - Hot temperature: 59 °C,
  - NTC  B constant: 3380 K,
  - NTC resistance 10000 Ω.

* (2) SNN5843
  - Capacity: 2300 mAh,
  - Charge current: 1150 mA,
  - Termination current: 100 mA,
  - Regulation voltage: 4180 mV,
  - Cut-off voltage: 3000 mV,
  - Cold temperature: 1 °C,
  - Cool temperature: 10 °C,
  - Warm temperature: 45 °C,
  - Hot temperature: 59 °C,
  - NTC  B constant: 3380 K,
  - NTC resistance 10000 Ω.

* (3) LIPO8047109
  - Capacity: 5000 mAh,
  - Charge current: 2500 mA,
  - Termination current: 100 mA,
  - Regulation voltage: 4180 mV,
  - Cut-off voltage: 3000 mV,
  - Cold temperature: 1 °C,
  - Cool temperature: 2 °C,
  - Warm temperature: 45 °C,
  - Hot temperature: 50 °C,
  - NTC  B constant: 3380 K,
  - NTC resistance 10000 Ω.

### Battery charge/discharge

#### Battery uptime calculations

To get a good approximation of how long your PiJuice setup will run with your chosen battery you can use the formula below in which you need to provide the capacity of the battery in mAh and the overall load of your setup in mA:

`((Battery Size * 3.7) / (mA * 5)) * 0.75`

*((Battery mAh * Battery Voltage) / (Pi current draw * Pi voltage)) * Estimated efficiency of system*

For example an idling Raspberry Pi 3B consuming 230mA running with our default battery BP7X with a capacity of 1820mAh and a voltage of 3.7V should be up for about 4 hours and 40 minutes.

`((1820mAh * 3.7V) / (230mA * 5V)) * 0.75 = 4.39h`

We have created the [PiJuice Battery Discharge Time Calculator](https://www.pi-supply.com/battery-levels/) and a [spreadsheet](https://github.com/PiSupply/PiJuice/blob/master/Hardware/Batteries/PiJuice%20Battery%20Discharge%20Levels.xlsx) which gives some typical battery capacities and usage scenarios ([with thanks to Alex from RasPi.TV for his testing efforts](http://raspi.tv/2017/how-much-power-does-pi-zero-w-use)) on all the versions of the Raspberry Pi and how they then relate to approximate uptime of the Pi when using the PiJuice. Obviously these are pretty basic theoretical estimations and if the uptime is "mission-critical" then you should definitely perform real-life testing before relying on these figures but these should give you a good idea.

**NOTE:** When PiJuice is in stand-by mode and 5V power has been removed, total current draw is 0.5mA from the battery

#### Battery charge level notes

There is a known quirk for lithium ion batteries whilst charging/discharging and the specific "fuel gauge" IC we are using on PiJuice. It is effectively about the measuring principle of the fuel gauge IC, that measures battery impedance to estimate charge level. Due to parasitic impedance (mostly due to the internal battery protection circuit - modern phones and tablets won't have this, but they tightly control their manufacturer of batteries which is far harder for a low volume product) there are measuring errors especially while charging because there are big currents over 1 Amp. We took the attitude that safety is a priority over charge level accuracy.

The purpose for the charge level is to have estimation during discharging to know when it is near to empty... info that is useful for field applications. It is precise enough while powering Pi and discharging with no inputs. More accurate readings with this IC can be achieve by having a fixed battery type without protection circuit before fuel gauge connection point - that is the usual case in phones or laptops. The protection method is integrated within the BP7X battery we are using, which is an older battery that as you can see was removable. This introduces impedance measurement errors, it is about hardware not firmware. That is why on many older phones you do not even have charge level during charging but only blinking symbol.

We have provided a sample [charge / discharge test log](https://github.com/PiSupply/PiJuice/blob/master/Hardware/Batteries/pijuice_charge.log), with charge level and voltage printed every minute during charging and discharging. There is an initial rise during charging to over 50%, but discharge is pretty linear.

Last but not least, we will likely try to fix this in a future software update based on practical test data - but a) this is not a huge priority and b) it will need more test data than we are able to get right now (multiple boards over long period charging and discharging in different scenarios)

## Misc

### RF shield clips (M1-M4)

You can use the optional RF Shield - Harwin S02-20150300 under the following circumnstances:
* You experience greater heating of Pijuice board
* When there is sensitive electronics around the board like radios or sensor hats sensitive to 1.5MHz and harmonics or 2.5MHz and harmonics that are frequencies of PiJuice regulators

### Battery Label Sticker

We have created a label you can stick to your battery to remind you of some of the common features of PiJuice. You can [find the sticker under the documentation section of our GitHub](https://github.com/PiSupply/PiJuice/blob/master/Documentation/Battery-Sticker.pdf). As shown below, you need to print it 43mm wide by 40mm wide for it to be the correct size to stick onto your battery.

![image](https://user-images.githubusercontent.com/3359418/37776742-e1fd155c-2ddd-11e8-85a7-460377883496.png)  

### 3D Design Files

We have had a number of people request 3D design files for the HAT (so they can design their own cases etc) as well as for the surround (so they can modify the surround and/or print their own at home). You can find those files on out GitHub at the following locations:

* [HAT 3D Files](https://github.com/PiSupply/PiJuice/tree/master/Hardware/3D/HAT)
* [Surround 3D Files](https://github.com/PiSupply/PiJuice/tree/master/Hardware/3D/Surround)
