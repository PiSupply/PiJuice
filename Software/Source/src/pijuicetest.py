#import smbus
import time
import datetime
import calendar
import sys, getopt
from pijuice import PiJuice

#pijuiceAdr=0x14 
#i2cbus = smbus.SMBus(1)
pijuice = PiJuice(1,0x14)
 
print pijuice.status.ResetFaultFlags(['powerOffFlag', 'sysPowerOffFlag'])
print pijuice.power.SetSystemPowerSwitch(0)
print pijuice.config.RunTestCalibration()
#pijuice.power.SetPowerOff(30)
while True: 
	#pijuice.status.SetPowerOff(1) 
	#print pijuice.config.SetPowerRegulatorMode('POWER_SOURCE_DETECTION')
	print pijuice.config.GetPowerRegulatorMode()
	#print pijuice.config.SetRunPinConfig('NOT_INSTALLED')
	print pijuice.config.GetRunPinConfig()
	
	print pijuice.power.GetSystemPowerSwitch() 
	#print pijuice.config.SetLedConfiguration('D1', 'CHARGE_STATUS')
	print pijuice.config.GetLedConfiguration('D1')
	print pijuice.status.SetLedState('D2', [150, 0,100])
	print pijuice.status.SetLedBlink('D2', 10, [150, 80,200], 100, [0, 250,0], 50)
	print pijuice.status.GetLedBlink('D2')
	#print pijuice.status.GetLedState('D2')

	#while True:
	status = pijuice.status.GetStatus()
	status = status['data'] if status['error'] == 'NO_ERROR' else status['error']
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

	print status 
	print fault
	print charge,'%,', 'T=', temp, ', Vbat=',vbat, ', Ibat=',ibat, ', Vio=',vio, ', Iio=',iio

	buttonConfig = {'DOUBLE_PRESS': {'function': 'USER_EVENT2', 'parameter': 600}, 'LONG_PRESS1': {'function': 'USER_EVENT1', 'parameter': 10000}, 'RELEASE': {'function': 'NO_FUNC', 'parameter': 0}, 'LONG_PRESS2': {'function': 'NO_FUNC', 'parameter': 20000}, 'SINGLE_PRESS': {'function': 'NO_FUNC', 'parameter': 0}, 'PRESS': {'function': 'NO_FUNC', 'parameter': 0}}
	#print pijuice.config.SetButtonConfiguration(buttonConfig, 'SW3')
	print 'button=', pijuice.config.GetButtonConfiguration('SW3')
	#print pijuice.power.SetWakeUpOnCharge(8)
	print pijuice.power.GetWakeUpOnCharge()
	print pijuice.power.SetWatchdog(0)  
	print pijuice.power.GetWatchdog() 
	print pijuice.status.GetButtonEvents()
	print pijuice.status.AcceptButtonEvent('SW2')
	#print pijuice.config.SetBatteryProfile('DEFAULT')
	time.sleep(0.1)
	print pijuice.config.GetBatteryProfileStatus()
	batteryProfile = {'capacity': 4800, 'tempCold': -2, 'tempCool': 3, 'regulationVoltage': 4180, 'ntcB': 3380, 'ntcResistance': 10000, 'terminationCurrent': 100, 'tempWarm': 44, 'cutoffVoltage': 3100, 'tempHot': 52, 'chargeCurrent': 2400}
	#print pijuice.config.SetCustomBatteryProfile(batteryProfile)
	print pijuice.config.GetBatteryProfile()
	#print pijuice.config.SetAddress(1, 14)
	print pijuice.config.GetAddress(1)
	#print pijuice.config.SetIdEepromWriteProtect('WRITE_PROTECTED')
	print pijuice.config.GetIdEepromWriteProtect()  
	#print pijuice.config.SetIdEepromAddress(52)
	print pijuice.config.GetIdEepromAddress()
	print pijuice.config.GetFirmwareVersion()
#time.sleep(0.12)
#pijuice.status.SetPowerOff(30)
#pijuice.SetBatteryTempSenseConfig('AUTO_DETECT')
#print pijuice.GetBatteryTempSenseConfig()