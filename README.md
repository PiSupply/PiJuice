![Alt text](https://user-images.githubusercontent.com/16068311/30545031-58b8fec6-9c80-11e7-8b3a-5e1f3aefd86c.png?raw=true "Optional Title")
# PiJuice
Resources for the [PiJuice range](https://www.pi-supply.com/?s=pijuice&post_type=product&tags=1&limit=5&ixwps=1) (complete with our revolutionary [PiAnywhere](https://www.pi-supply.com/product-tag/pianywhere/) technology – the best way to take your Pi off the grid!). Originally funded on [Kickstarter](https://www.kickstarter.com/projects/pijuice/pijuice-a-portable-project-platform-for-every-rasp/).

## Technical FAQ
 - can you tell me purpose and pinouts for each of the tag connect cables J7 and J6 and how they can be used?
J7 is development header to connect ST-Link with mcu and download firmware or debug. Can be also used in production to write firmware. It needs Tag-connect TC2050 ARM20-10 adapter to connect ST-Link to cable.
J6 is for programming ID EEPROM in production. See signals below, there is no adapter board and needs to be wired manually to some programming tool. It is just option for programming if it is not possible to do with EEPROM programmer before assembly.

- can you tell me specification of button to connect to J5? Just momentary push button? And it connects to same as one of the other buttons SW1-3? Which one?
It needs to be simple tactile pushbutton and connects parallel to SW1.
- can you tell me specification of solar panel to connect to J3?
Requirement for on board USB micro input is to provide voltage 4.2V – 10V, as labeled on board. Also minimum current source should provide to get charging is 80mA.
 
- can you tell me specification of what can be connected to J4?
J4 is alternative to onboard USB micro input. Pad holes to solder 2.54mm header, any type that can fit and user finds useful.
 
- can you tell me the purpose and pinouts for TP1 to TP23?
TP1-TP23 are test points with possible use in production test to connect to custom bed of nails or spring pins. This should not be in user guide and purpose of each is about connections in schematic.
- can you tell me the purpose of unpopulated posts R20, R26 and D3?
R20 if place to solder resistor as additional way to configure battery profile without using software configuration additional to dip switch, where charging current and charging voltage are encoded with resistance of resistor. Also user can select predefined profile by choosing one of 16 with resistor value instead with dip switch which is limited to 4. There is table within excel document Pijuice_battery_config.xlsx  sheet “Charge settings” how to choose resistor for desired charge settings and sheet “Profile selection” how to choose resistor to select one of predefined battery profiles. R20 should be through hole 0.1% tolerance.
It is possible to override resistor settings in software. In Pijuice HAT configuration window on “Battery” tab check “Custom” and edit battery charging parameters to desired values, then click apply. You can also return to resistor settings by choosing “DEFAULT” from drop-down list.
 
Unpopulated R26 is reserved for future development use to add different possibilities with resistor configuration.

Unpopulated D3 is charger ic status LED, is on during active charging of charger ic and is used for firmware development purposes and debugging.
 
- with DIP switch, position in test document is for BP7X by default. Can you tell me, what are defaults for other 3 dip switch positions? And can these all be overridden in firmware/software? Can you explain exact process?
There is table in Pijuice_battery_config.xlsx for dip switch configuration options, how to select desired battery profile with switch code. Switch and resistor configuration can be overridden in software configuration, in Pijuice HAT configuration window on “Battery” tab choose battery profile from drop down list. You can also return to dip switch selected profile by choosing “DEFAULT” from drop-down list.
 
- can you tell me under what circumstances somebody would want to install the RF should onto clips?
In cases where it is experienced greater heating of Pijuice board, or in cases there are sensitive electronics around like radios or sensors hats sensitive to 1.5MHz and harmonics or 2.5MHz and harmonics that are frequencies of pijuice regulators.
 
- can you tell me specification of NTC and ID on battery terminals?
NTC terminal can be used to connect NTC thermistor integrated with battery and used for temperature regulation of battery during charging. Requirement is that battery uses 10KOhm NTC resistor with known thermistor B constant that can be entered as profile data in config GUI “battery” tab. There are regulation threshold data that can be entered for custom batteries like Cold, Cool, Warm and HOT temperatures that are derived from battery manufacturers specification.
ID is not used it is just reserved for possible future use, because there is ID on BP7X, BP6X so possibility to identify it automatically.
 
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
·        Dual long press of SW1 and SW2 (J5 for Pijuice Zero)  for 20 seconds will reset Pijuice HAT configuration to default (this applies to mcu configuration only).
·        Holding pressed SW3 (J5 for Pijuice Zero) while powering up Pijuice will initiate bootloader (used only in cases when ordinary initiation through I2C does not works because of damaged firmware).

- can you tell me default operations of each led?
D1 charge status by default
D2 user LED by default.
- on P3 what is vsys - battery voltage? Vcc is 5v0? What is current capacity of these pins and 3v3? And for the two IO pins, how can those be utilized and what pins on ARM cortex do they connect to?
VSYS on P3 is same as VSYS on J3 and is switchable battery voltage for system use, like connecting to Pibot power input. VSYS switch is programmable with software with “OFF”, “ON 500mA current limit” and “ON 2100mA current limit”.
 
- do we have source code for the firmware that we could share with people for custom applications?
Firmware source code is not intended to be shared with users, because it requires deep understanding of hardware and programming to do customization, and may even harm something or damage battery in case of mistakes.
There is I2C command interface provided through python API that install together with configuration GUI intended for users to do custom applications on Linux side. There is pijucetest.py that can be used as example how to do programming in python and communicate data with Pijuice.
-What are pinout designations for spring battery connector J1 on Pijuice HAT?

