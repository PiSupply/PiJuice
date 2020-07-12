import time
import datetime
import calendar
import sys, getopt
from pijuice import PiJuice

pijuice = PiJuice(1,0x14)

status = pijuice.status.GetStatus()
status = status['data'] if status['error'] == 'NO_ERROR' else status['error']
print('Status = ', status) 
	
fault =  pijuice.status.GetFaultStatus() 
fault = fault['data'] if fault['error'] == 'NO_ERROR' else fault['error']
print('Fault = ', fault)

charge = pijuice.status.GetChargeLevel()		
charge = charge['data'] if charge['error'] == 'NO_ERROR' else charge['error']
fault =  pijuice.status.GetFaultStatus() 
fault = fault['data'] if fault['error'] == 'NO_ERROR' else fault['error']
temp =  pijuice.status.GetBatteryTemperature()
temp = temp['data'] if temp['error'] == 'NO_ERROR' else temp['error']
vbat = pijuice.status.GetBatteryVoltage()	        
vbat = vbat['data'] if vbat['error'] == 'NO_ERROR' else vbat['error'] 
ibat = pijuice.status.GetBatteryCurrent()
ibat = ibat['data'] if ibat['error'] == 'NO_ERROR' else ibat['error']
vio =  pijuice.status.GetIoVoltage()
vio = vio['data'] if vio['error'] == 'NO_ERROR' else vio['error']
iio = pijuice.status.GetIoCurrent()
iio = iio['data'] if iio['error'] == 'NO_ERROR' else iio['error'] 
print(charge,'%,', 'T=', temp, ', Vbat=',vbat, ', Ibat=',ibat, ', Vio=',vio, ', Iio=',iio)

print(pijuice.config.GetPowerRegulatorMode())
print(pijuice.config.GetRunPinConfig())

print(pijuice.power.GetSystemPowerSwitch() )
print(pijuice.config.GetLedConfiguration('D1'))
print(pijuice.status.SetLedState('D2', [150, 0,100]))
print(pijuice.status.GetLedBlink('D2'))

print('button SW3 =', pijuice.config.GetButtonConfiguration('SW3'))
print(pijuice.power.GetWakeUpOnCharge())
print(pijuice.power.GetWatchdog() )
print(pijuice.status.GetButtonEvents())
print(pijuice.config.GetBatteryProfileStatus())
print(pijuice.config.GetBatteryProfile())
print(pijuice.config.GetAddress(1))
print(pijuice.config.GetIdEepromWriteProtect())
print(pijuice.config.GetIdEepromAddress())
print(pijuice.config.GetFirmwareVersion())
print(pijuice.GetBatteryTempSenseConfig())
