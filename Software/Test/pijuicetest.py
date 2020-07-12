#!/usr/bin/env python3

import time
import datetime
import calendar
import sys, getopt
from pijuice import PiJuice

pijuice = PiJuice(1,0x14)

def checkValue(val):
    val = val['data'] if val['error'] == 'NO_ERROR' else val['error']
    return val

status = checkValue(pijuice.status.GetStatus())
print('Status = ', status) 
	
fault =  checkValue(pijuice.status.GetFaultStatus())
print('Fault = ', fault)

charge = checkValue(pijuice.status.GetChargeLevel())
fault =  checkValue(pijuice.status.GetFaultStatus())
temp = checkValue( pijuice.status.GetBatteryTemperature())
vbat = checkValue(pijuice.status.GetBatteryVoltage())
ibat = checkValue(pijuice.status.GetBatteryCurrent())
vio =  checkValue(pijuice.status.GetIoVoltage())
iio = checkValue(pijuice.status.GetIoCurrent())
print('Charge =',charge,'%,', 'T =', temp, ', Vbat =',vbat, ', Ibat =',ibat, ', Vio =',vio, ', Iio =',iio)

regmode = checkValue(pijuice.config.GetPowerRegulatorMode())
print('RegulatorMode =', regmode)
runpin = checkValue((pijuice.config.GetRunPinConfig()))
print('RunPin =', runpin)
sysswitch = checkValue(pijuice.power.GetSystemPowerSwitch())
print('SystemPowerSwitch =', sysswitch)

ledconfig = checkValue(pijuice.config.GetLedConfiguration('D1'))
print('LedConfigD1 =', ledconfig)
ledblink = checkValue(pijuice.status.GetLedBlink('D2'))
print('LedBlinkD2 =', ledblink)

but3 = checkValue(pijuice.config.GetButtonConfiguration('SW3'))
print('ButtonSW3 =', but3)
wakeuponch = checkValue(pijuice.power.GetWakeUpOnCharge())
print('WakeUpOnCharge =', wakeuponch, '(127 = disabled)')
watchdog = checkValue(pijuice.power.GetWatchdog())
print('Watchdog =', watchdog)
buttonevents = checkValue(pijuice.status.GetButtonEvents())
print('ButtonEvents =', buttonevents)
batprofilestat = checkValue(pijuice.config.GetBatteryProfileStatus())
print('BatteryProfileStatus =', batprofilestat)
batprofile = checkValue(pijuice.config.GetBatteryProfile())
print('BatteryProfile =', batprofile)
batextprofile = checkValue(pijuice.config.GetBatteryExtProfile())
print('Extended BatteryProfile =', batextprofile)
tempsense = checkValue(pijuice.config.GetBatteryTempSenseConfig())
print('Battery temperature sense =', tempsense)

pjaddr = checkValue(pijuice.config.GetAddress(1))
print('PiJuice I2C address =', pjaddr, '(hex)')
eepromwrprot = checkValue(pijuice.config.GetIdEepromWriteProtect())
print('HAT eeprom write protect =', eepromwrprot)
eepromaddr = checkValue(pijuice.config.GetIdEepromAddress())
print('HAT eeprom address =', eepromaddr, '(hex)')
fwver = checkValue(pijuice.config.GetFirmwareVersion())
print('Firmware Version =', fwver)

