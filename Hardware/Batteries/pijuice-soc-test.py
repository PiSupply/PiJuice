# Author: Milan Neskovic, Pi Supply, 2018

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# Usage:
# Use to measure battery open circuit voltage and resistance by discharging battery using resistor load attached at PiJuice Vsys connection.
# Power supply should be connected to Raspberry Pi only. Program will output to screen and text file measured battery resistance-Rbat and open circuit voltage-OCV. 
# For getting ocv10,ocv50,ocv90,r10,r50,r90 profile configuration parameters, test can be started at battery full and finished when battery discharges to zero.
# Approximate battery resistance for purpose of profile calibration can be evaulated after few measurement points in short program run from arbitrary battery level.
# Output file name: battery_soc_test.txt
# Battery capacity depends on conditions like load current and temperature, so nominal capacity may not correspond to real test capacity. To get better test accuracy 
# it is needed to normalize final dSOC after full discharge to 100%, real_soc=100+dSOC*100/dSOCfinal.
# Program prints charge level estimated by pijuice as pj charge, and can be used for accuracy test.

# Parameters:
c0=1820 #[mAh] battery capacity
Rload = 10 #[Ohm] Resistance of test load resistor connected to Vsys. 10Ohm/2W resistor is good example. Set to 0 if load resistor is not connected.
Igpio=0 #[mA] Current draw from GPIO 5V, this can be measured current consumed by Raspberry pi, Rpi3b 480mA typical.
onTime = 1.0 # Length of time interval resistor load is on
offTime = 0.5 # Length of time interval resistor load is off
endVolt = 3100 #[mV], end test when battery voltage drops below this level
k=0.93 # PiJuice regulator efficiency.

import sys
import time, datetime, calendar 
from time import sleep
from array import array
from fcntl import ioctl
from ctypes import *
import signal

_device = open('/dev/i2c-{0}'.format(1), 'r+b', buffering=0)
I2C_SLAVE_FORCE = 0x0706
ioctl(_device.fileno(), I2C_SLAVE_FORCE, 0x14 & 0x7F)

def _GetChecksum(data):
	fcs=0xFF
	for x in data[:]:
		fcs=fcs^x
	return fcs
		
def _Read(length, cmd):
	global _device
	cmdData = bytearray(1)
	cmdData[0] = cmd & 0xFF
	_device.write(cmdData)
	sleep(0.002)
	d = [byte for byte in bytearray(_device.read(length+1))]
	if _GetChecksum(d[0:-1]) != d[-1]:
		return {'error':'DATA_CORRUPTED'}
	del d[-1]
	return {'data':d, 'error':'NO_ERROR'}

	
def signal_handler(sig, frame):
	cd = bytearray(3)
	cd[0] = 0X64
	cd[1] = 0
	cd[2] = 0xFF^cd[1]
	_device.write(cd)
	print(' exiting')
	sys.exit(0)
		
signal.signal(signal.SIGINT, signal_handler)

volt_on=0
volt_off=0
chargeLvl=0
temp=0
initLvl=10000
onOff = 0
dSOC=0
dSOCp=0.0
dSocDiff=0
start_time = time.time()
adv_time = 0

if Rload==0: Rload=10000000
cf = bytearray([0x5E, 0x09, 0xFF^0x09])
_device.write(cf) # turn off GPIO input to prevent charging
cd = bytearray(3)
cd[0] = 0X64
cd[1] = 0
cd[2] = 0xFF^cd[1]
_device.write(cd)
sleep(0.2)

while 1:	
	elapsed_time = time.time() - start_time
	if elapsed_time > adv_time: #+ delTime:
		#sleep(0.2)
		ret=_Read(2, 0X49)
		if ret['error'] == 'NO_ERROR':
			d=ret['data']
			volt=(d[1] << 8) | d[0]
			if onOff:
				volt_on=volt
			else:
				volt_off=volt
		else:
			print("volt read ERROR")
		if onOff:
			Ibat1=volt_on/(Rload+0.06) #Vsys load current. Addition of sys switch resistance.
			Ibat2=(Igpio*5100)/k/(volt_on+0.000001) # Battery current produced by 5V GPIO loading.
			if (Ibat1+Ibat2)<5000:
				Ibat=Ibat1+Ibat2 
				dSOC=dSOC-(Ibat1*onTime+Ibat2*(onTime+offTime))/3600 # state of charge drop counted from program start
			Rbat=(volt_off-volt_on)/(Ibat+0x0000000001)
			dSOCp=dSOC/c0*100
			sleep(0.002)
			ret=_Read(2, 66)
			if ret['error'] == 'NO_ERROR':
				d=ret['data']
				chargeLvl = ((d[1] << 8) | d[0])/10.0
				if initLvl==10000: 
					initLvl=chargeLvl
				else:
					dSocDiff=initLvl-chargeLvl+dSOCp
			else:
				print("charge level read ERROR")
			ret=_Read(2, 71) # Temperature read
			if ret['error'] == 'NO_ERROR':
				d=ret['data']
				temp = int(d[0])#((d[1] << 8) | d[0])
				if temp > 128: 
					temp = -(256-temp)
			else:
				print("temperature read ERROR")
			out="%08.2f"%elapsed_time+'sec, Vbon:%04d'%volt_on+ 'mV, OCV:%04d'%volt_off+ 'mV, Ibat:%06.1f'%Ibat+'mA, Rbat:%07.5f'%Rbat+ 'Ohm, dSOC:%07.2f'%dSOCp+'%,'+' pj charge:%05.1f'%chargeLvl+'%,'+' diff:%05.2f'%dSocDiff+', T:%dC'%temp
			print(out)
			output_file = open('battery_soc_test.txt', 'ab')
			output_file.write((out+'\n').encode())
			output_file.close()
		if (volt_off < endVolt) and (volt_off > 0):
			cd[1] = 0
			cd[2] = 0xFF^cd[1]
			_device.write(cd)
			print('end voltage reached, terminating')
			print('dSOC90: %07.2f'%(dSOCp*0.1)+' %') # Read ocv90 and r90 from line where dSOC matches dSOC90 value
			print('dSOC50: %07.2f'%(dSOCp*0.5)+' %') # Read ocv50 and r50 from line where dSOC matches dSOC50 value
			print('dSOC10: %07.2f'%(dSOCp*0.9)+' %') # Read ocv10 and r10 from line where dSOC matches dSOC10 value
			break
	
		onOff = not onOff
		cd[1] = 21 if onOff else 0
		cd[2] = 0xFF^cd[1]
		_device.write(cd)
		adv_time = adv_time + (onTime if onOff else offTime)
