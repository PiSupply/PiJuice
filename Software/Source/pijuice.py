#!/usr/bin/env python
from smbus import SMBus
import time
import datetime
import calendar
import sys, getopt
from ctypes import *
from fcntl import ioctl
import struct 
import ctypes
import threading

pijuice_hard_functions = ['HARD_FUNC_POWER_ON', 'HARD_FUNC_POWER_OFF']
pijuice_sys_functions = ['SYS_FUNC_HALT', 'SYS_FUNC_HALT_POW_OFF', 'SYS_FUNC_SYS_OFF_HALT', 'SYS_FUNC_REBOOT']
pijuice_user_functions = ['USER_EVENT'] + ['USER_FUNC' + str(i+1) for i in range(0,15)]

class PiJuiceInterface():
	def __init__(self, bus=1, address=0x14):
		"""Create a new PiJuice instance.  Bus is an optional parameter that
		specifies the I2C bus number to use, for example 1 would use device
		/dev/i2c-1.  If bus is not specified then the open function should be
		called to open the bus.
		"""
		I2C_SLAVE             = 0x0703  # Use this slave address
		I2C_SLAVE_FORCE       = 0x0706  # Use this slave address, even if
                                # is already in use by a driver!
		I2C_PEC               = 0x0708  # != 0 to use PEC with SMBus
		
		self.i2cbus=SMBus(bus)
		self.addr=address
		#self._device = open('/dev/i2c-{0}'.format(bus), 'r+b', buffering=0)
		#ioctl(self._device.fileno(), I2C_SLAVE_FORCE, self.addr & 0x7F)
		self.t = None
		self.comError = False
		self.errTime = 0
	 
	def __del__(self):
		"""Clean up any resources used by the PiJuice instance."""
		self.i2cbus = None
		#if self._device is not None:
			#self._device.close()
			#self._device = None

	def __enter__(self):
		"""Context manager enter function."""
		# Just return this object so it can be used in a with statement
		return self

	def __exit__(self, exc_type, exc_val, exc_tb):
		"""Context manager exit function, ensures resources are cleaned up."""
		#if self._device is not None:
			#self._device.close()
			#self._device = None
		self.i2cbus = None
		return False  # Don't suppress exceptions.
		
	def GetAddress(self):
		return self.addr
	
	def _GetChecksum(self, data):
		fcs=0xFF
		for x in data[:]:
			fcs=fcs^x
		return fcs
				
	def _Read(self):
		#if self.comError and (time.time()-self.errTime) < 4:
		#	self.d = None
		try:
			d = self.i2cbus.read_i2c_block_data(self.addr, self.cmd, self.length)
			#self._device.write(cmdData)
			#d = [byte for byte in bytearray(self._device.read(length + 1))]
			#return d
			self.d = d
			self.comError = False
		except:# IOError:
			self.comError = True
			self.errTime = time.time()
			self.d = None
			
	def _Write(self):
		try:
			self.i2cbus.write_i2c_block_data(self.addr, self.cmd, self.d)
			#self._device.write(cmdData)
			#d = [byte for byte in bytearray(self._device.read(length + 1))]
			#return d
			self.comError = False
		except:# IOError:
			self.comError = True
			self.errTime = time.time()
			
	def _DoTransfer(self, oper):
		if (self.t != None and self.t.isAlive()) or (self.comError and (time.time()-self.errTime) < 4):
			#print self.t.isAlive()
			return False

		self.t = threading.Thread(target=oper, args=())
		self.t.start()
		
		# wait for transfer to finish or timeout
		n = 0
		while  self.t.isAlive() and n < 2:
			time.sleep(0.05)
			n = n + 1
		#print self.t.isAlive(), n
		if self.comError or self.t.isAlive():
			return False
		
		return True
		
	def ReadData(self, cmd, length):
		d = []
		#cmdData = bytearray(1)
		#cmdData[0] = cmd & 0xFF

		self.cmd = cmd
		self.length = length + 1
		if not self._DoTransfer(self._Read):
			return {'error':'COMMUNICATION_ERROR'}
		
		d = self.d
		#d = self._Read(cmd, length + 1) 
		#if not d:
			#d = self._Read(cmd, length + 1)  
			#if not d:
				#return {'error':'COMMUNICATION_ERROR'}
		#try:
			#d = self.i2cbus.read_i2c_block_data(self.addr, cmd, length + 1)
			#self._device.write(cmdData)
			#d = [byte for byte in bytearray(self._device.read(length + 1))]
		#except IOError:
			# try once more
			#try:
				#d = self.i2cbus.read_i2c_block_data(self.addr, cmd, length + 1)
				#self._device.write(cmdData)
				#d = [byte for byte in bytearray(self._device.read(length + 1))]
			#except IOError:
				#return {'error':'COMMUNICATION_ERROR'}
		
		if self._GetChecksum(d[0:-1]) != d[-1]:
			print self._GetChecksum(d), d, length
			return {'error':'DATA_CORRUPTED'}
		del d[-1]
		return {'data':d, 'error':'NO_ERROR'}
			
	def WriteData(self, cmd, data):
		fcs = self._GetChecksum(data)
		d = data[:]
		d.append(fcs)
		#d = bytearray(len(data)+2)
		#d[0] = cmd & 0xFF
		#d[1:-1] = data[0:]
		#d[-1] = fcs
		
		self.cmd = cmd
		self.d = d
		if not self._DoTransfer(self._Write):
			return {'error':'COMMUNICATION_ERROR'}
		
		#try:
			#self.i2cbus.write_i2c_block_data(self.addr, cmd, d)
			#self._device.write(d)
		#except IOError:
			# try once more
			#try:
				#self.i2cbus.write_i2c_block_data(self.addr, cmd, d)
				#self._device.write(d)
			#except IOError:
				#return {'error':'COMMUNICATION_ERROR'}
		return {'error':'NO_ERROR'}
				
	def WriteDataVerify(self, cmd, data, delay = None):
		wresult = self.WriteData(cmd, data)
		if wresult['error'] != 'NO_ERROR':
			return wresult
		else:
			if delay != None:
				try:
					time.sleep(delay*1)
				except:
					time.sleep(0.1)
			result = self.ReadData(cmd, len(data))
			if result['error'] != 'NO_ERROR':
				return result
			else:
				if (data==result['data']):
					return {'error':'NO_ERROR'}
				else:
					print 'wr', data
					print 'rd', result['data']
					# result['data']
					return {'error':'WRITE_FAILED'}

class PiJuiceStatus():

	STATUS_CMD = 0X40
	FAULT_EVENT_CMD = 0X44
	CHARGE_LEVEL_CMD = 0X41
	BUTTON_EVENT_CMD = 0X45
	BATTERY_TEMPERATURE_CMD = 0X47
	BATTERY_VOLTAGE_CMD = 0X49
	BATTERY_CURRENT_CMD = 0X4b
	IO_VOLTAGE_CMD = 0X4d
	IO_CURRENT_CMD = 0X4f
	LED_STATE_CMD = 0x66
	LED_BLINK_CMD = 0x68
	
	def __init__(self, interface):
		self.interface = interface
		
	def __enter__(self):
		# Just return this object so it can be used in a with statement
		return self

	def __exit__(self, exc_type, exc_val, exc_tb):
		return False  # Don't suppress exceptions.
			
	def GetStatus(self):
		result = self.interface.ReadData(self.STATUS_CMD, 1)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data'][0]
			status = {} 
			status['isFault'] = bool(d & 0x01)
			status['isButton'] = bool(d & 0x02) 
			batStatusEnum = ['NORMAL', 'CHARGING_FROM_IN', 'CHARGING_FROM_5V_IO', 'NOT_PRESENT']
			status['battery'] = batStatusEnum[(d >> 2) & 0x03]
			powerInStatusEnum = ['NOT_PRESENT', 'BAD', 'WEAK', 'PRESENT']
			status['powerInput'] = powerInStatusEnum[(d >> 4) & 0x03]
			status['powerInput5vIo'] = powerInStatusEnum[(d >> 6) & 0x03]
			return {'data':status, 'error':'NO_ERROR'}

	def GetChargeLevel(self):
		result = self.interface.ReadData(self.CHARGE_LEVEL_CMD, 1)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			return {'data':result['data'][0], 'error':'NO_ERROR'}
		
	faultEvents = ['button_power_off', 'forced_power_off', 'forced_sys_power_off', 'watchdog_reset']
	faults = ['battery_profile_invalid', 'charging_temperature_fault']
	def GetFaultStatus(self):
		result = self.interface.ReadData(self.FAULT_EVENT_CMD, 1)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data'][0]
			fault = {} 
			if d & 0x01:
				fault['button_power_off'] = True
			if d & 0x02: 
				fault['forced_power_off'] = True #bool(d & 0x02)
			if d & 0x04:
				fault['forced_sys_power_off'] = True
			if d & 0x08:
				fault['watchdog_reset'] = True
			if d & 0x20:
				fault['battery_profile_invalid'] = True
			batChargingTempEnum = ['NORMAL', 'SUSPEND', 'COOL', 'WARM']
			if (d >> 6) & 0x03:
				fault['charging_temperature_fault'] = batChargingTempEnum[(d >> 6) & 0x03]
			return {'data':fault, 'error':'NO_ERROR'}
		
	def ResetFaultFlags(self, flags):
		d = 0xFF
		for ev in flags: 
			try:
				d = d & ~(0x01<<self.faultEvents.index(ev))
			except:
				ev
		self.interface.WriteData(self.FAULT_EVENT_CMD, [d]) # clear fault events
			
	buttonEvents = ['NO_EVENT', 'PRESS', 'RELEASE', 'SINGLE_PRESS', 'DOUBLE_PRESS', 'LONG_PRESS1', 'LONG_PRESS2']
	def GetButtonEvents(self):
		result = self.interface.ReadData(self.BUTTON_EVENT_CMD, 2)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data']
			event = {}
			try:
				event['SW1'] = self.buttonEvents[d[0] & 0x0F]
			except:
				event['SW1'] = 'UNKNOWN'
			try: 
				event['SW2'] = self.buttonEvents[(d[0] >> 4) & 0x0F]
			except:
				event['SW2'] = 'UNKNOWN'
			try: 
				event['SW3'] = self.buttonEvents[d[1] & 0x0F]
			except:
				event['SW3'] = 'UNKNOWN'
				#event['SW4'] = self.buttonEvents[(d[1] >> 4) & 0x0F]
			return {'data':event, 'error':'NO_ERROR'}
			
	buttons = ['SW' + str(i+1) for i in range(0,3)]
	def AcceptButtonEvent(self, button):
		b = None
		try: 
			b = self.buttons.index(button)
		except ValueError:
			return {'error':'BAD_ARGUMENT'}
		d = [0xF0, 0xFF] if b == 0 else [0x0F, 0xFF] if b == 1 else [0xFF, 0xF0]
		self.interface.WriteData(self.BUTTON_EVENT_CMD, d) # clear button events
		
	def GetBatteryTemperature(self):
		result = self.interface.ReadData(self.BATTERY_TEMPERATURE_CMD, 2)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data']
			temp = d[1]
			temp = temp << 8;
			temp = temp | d[0]
			return {'data':temp, 'error':'NO_ERROR'}
		
	def GetBatteryVoltage(self):
		result = self.interface.ReadData(self.BATTERY_VOLTAGE_CMD, 2)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data']
			return {'data':(d[1] << 8) | d[0], 'error':'NO_ERROR'}
		
	def GetBatteryCurrent(self):
		result = self.interface.ReadData(self.BATTERY_CURRENT_CMD, 2)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data']
			i = (d[1] << 8) | d[0]
			if (i & (1<<15)):
				i = i - (1<<16)
			return {'data':i, 'error':'NO_ERROR'}
		
	def GetIoVoltage(self):
		result = self.interface.ReadData(self.IO_VOLTAGE_CMD, 2)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data']
			return {'data':(d[1] << 8) | d[0], 'error':'NO_ERROR'}

	def GetIoCurrent(self):
		result = self.interface.ReadData(self.IO_CURRENT_CMD, 2)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data']
			i = (d[1] << 8) | d[0]
			if (i & (1<<15)):
				i = i - (1<<16)
			return {'data':i, 'error':'NO_ERROR'}
			
	leds = ['D1', 'D2']
	def SetLedState(self, led, rgb):
		i = None
		try:
			i = self.leds.index(led)
		except:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteData(self.LED_STATE_CMD + i, rgb)
		
	def GetLedState(self, led):
		i = None
		try:
			i = self.leds.index(led)
		except:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.ReadData(self.LED_STATE_CMD + i, 3)
		
	def SetLedBlink(self, led, count, rgb1, period1, rgb2, period2):
		i = None
		d = None
		try:
			i = self.leds.index(led)
			d = [count&0xFF] + rgb1*1 + [(period1/10)&0xFF] + rgb2*1 + [(period2/10)&0xFF]
		except:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteData(self.LED_BLINK_CMD + i, d)	
		
	def GetLedBlink(self, led):
		i = None
		try:
			i = self.leds.index(led)
		except:
			return {'error':'BAD_ARGUMENT'}
		ret = self.interface.ReadData(self.LED_BLINK_CMD + i, 9)
		if ret['error'] != 'NO_ERROR':
			return ret
		else:
			d = ret['data']
			return {'data':{'count':d[0], 'rgb1':d[1:4], 'period1':d[4]*10, 'rgb2':d[5:8], 'period2':d[8]*10}, 'error':'NO_ERROR'}
	
class PiJuiceRtcAlarm():

	RTC_ALARM_CMD = 0xB9
	RTC_TIME_CMD = 0xB0
	RTC_CTRL_STATUS_CMD = 0xC2
	
	def __init__(self, interface):
		self.interface = interface
		
	def __enter__(self):
		# Just return this object so it can be used in a with statement
		return self

	def __exit__(self, exc_type, exc_val, exc_tb):
		return False  # Don't suppress exceptions.
		
	def GetControlStatus(self):
		ret = self.interface.ReadData(self.RTC_CTRL_STATUS_CMD, 2)
		if ret['error'] != 'NO_ERROR':
			return ret
		d = ret['data']
		r = {}
		if (d[0]&0x01) and (d[0]&0x04):
			r['alarm_wakeup_enabled'] = True
		else:
			r['alarm_wakeup_enabled'] = False
		if d[1]&0x01:
			r['alarm_flag'] = True
		else:
			r['alarm_flag'] = False
		return {'data':r, 'error':'NO_ERROR'}
		
	def ClearAlarmFlag(self):
		ret = self.interface.ReadData(self.RTC_CTRL_STATUS_CMD, 2)
		if ret['error'] != 'NO_ERROR':
			return ret
		d = ret['data']
		if d[1]&0x01:
			d[1] = d[1]&0xFE
			return self.interface.WriteDataVerify(self.RTC_CTRL_STATUS_CMD, d)
		else:
			return {'error':'NO_ERROR'}
			
	def SetWakeupEnabled(self, status):
		ret = self.interface.ReadData(self.RTC_CTRL_STATUS_CMD, 2)
		if ret['error'] != 'NO_ERROR':
			return ret
		d = ret['data']
		if (d[0]&0x01) and (d[0]&0x04):
			if status == True:
				return {'error':'NO_ERROR'}
			else:
				d[0] = d[0]&0xFE
		else:
			if status == False:
				return {'error':'NO_ERROR'}
			else:
				d[0] = d[0] | 0x01 | 0x04
		return self.interface.WriteDataVerify(self.RTC_CTRL_STATUS_CMD, d)
			
	def GetTime(self):
		ret = self.interface.ReadData(self.RTC_TIME_CMD, 9)
		if ret['error'] != 'NO_ERROR':
			return ret
		d = ret['data']
		dt = {}
		dt['second'] = ((d[0]>>4)&0x07) * 10 + (d[0]&0x0F)
		
		dt['minute'] = ((d[1]>>4)&0x07) * 10 + (d[1]&0x0F)
				
		if (d[2]&0x40):
			hourFormat = '12'
			ampm = 'PM' if (d[2]&0x20) else 'AM'
			dt['hour'] = str(((d[2]>>4)&0x01) * 10 + (d[2]&0x0F)) + ' ' + ampm
		else:
			hourFormat = '24'
			dt['hour'] = ((d[2]>>4)&0x03) * 10 + (d[2]&0x0F)
			
		dt['weekday'] = d[3]&0x07
		dt['day'] = ((d[4]>>4)&0x03) * 10 + (d[4]&0x0F)
		
		dt['month'] = ((d[5]>>4)&0x01) * 10 + (d[5]&0x0F)
		
		dt['year'] = ((d[6]>>4)&0x0F) * 10 + (d[6]&0x0F) + 2000
		
		dt['subsecond'] = d[7] / 256
		
		if (d[8]&0x03) == 2:
			dt['daylightsaving'] = 'SUB1H'
		elif (d[8]&0x03) == 1:
			dt['daylightsaving'] = 'ADD1H'
		else:
			dt['daylightsaving'] = 'NONE'
			
		if d[8]&0x04:
			dt['storeoperation'] = True
		else:
			dt['storeoperation'] = False
			
		return {'data':dt, 'error':'NO_ERROR'}
		
	def SetTime(self, dateTime):
		d = [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
		if dateTime == None or dateTime == {}:
			dt = {}
		else:
			dt = dateTime
			
		if 'second' in dt:
			try:
				s = int(dt['second'])
			except:
				return {'error':'INVALID_SECOND'}
			if s < 0 or s > 60:
				return {'error':'INVALID_SECOND'}
			d[0] = ((s / 10)&0x0F) << 4
			d[0] = d[0] | ((s % 10)&0x0F)
			
		if 'minute' in dt:
			try:
				m = int(dt['minute'])
			except:
				return {'error':'INVALID_MINUTE'}
			if m < 0 or m > 60:
				return {'error':'INVALID_MINUTE'}
			d[1] = ((m / 10)&0x0F) << 4
			d[1] = d[1] | ((m % 10)&0x0F)
			
		if 'hour' in dt:
			try:
				h = dt['hour']
				if isinstance(h, str):
					if (h.find('AM') > -1) or (h.find('PM') > -1):
						if (h.find('PM') > -1):
							hi = int(h.split('PM')[0])
							if hi < 1 or hi > 12:
								return {'error':'INVALID_HOUR'}
							d[2] = (((hi / 10)&0x03) << 4)
							d[2] = d[2] | ((hi % 10)&0x0F)
							d[2] = d[2] | 0x20 | 0x40
						else:
							hi = int(h.split('AM')[0])
							if hi < 1 or hi > 12:
								return {'error':'INVALID_HOUR'}
							d[2] = (((hi / 10)&0x03) << 4)
							d[2] = d[2] | ((hi % 10)&0x0F)
							d[2] = d[2] | 0x40
					else:
						h = int(h)
						if h < 0 or h > 23:
							return {'error':'INVALID_HOUR'}
						d[2] = (((h / 10)&0x03) << 4)
						d[2] = d[2] | ((h % 10)&0x0F)
						
				elif isinstance(h, int):
					#assume 24 hour format
					if h < 0 or h > 23:
						return {'error':'INVALID_HOUR'}
					d[2] = (((int(h) / 10)&0x03) << 4)
					d[2] = d[2] | ((int(h) % 10)&0x0F)
			except:
				return {'error':'INVALID_HOUR'}
			
		if 'weekday' in dt:
			try:
				day = int(dt['weekday'])
			except:
				return {'error':'INVALID_WEEKDAY'}
			if day < 1 or day > 7:
				return {'error':'INVALID_WEEKDAY'}
			d[3] = day&0x07
			
		if 'day' in dt:
			try:
				da = int(dt['day'])
			except:
				return {'error':'INVALID_DAY'}
			if da < 1 or da > 31:
				return {'error':'INVALID_DAY'}
			d[4] = ((da / 10)&0x03) << 4
			d[4] = d[4] | ((da % 10)&0x0F)
			
		if 'month' in dt:
			try:
				m = int(dt['month'])
			except:
				return {'error':'INVALID_MONTH'}
			if m < 1 or m > 12:
				return {'error':'INVALID_MONTH'}
			d[5] = ((m / 10)&0x01) << 4
			d[5] = d[5] | ((m % 10)&0x0F)
			
		if 'year' in dt:
			try:
				y = int(dt['year']) - 2000
			except:
				return {'error':'INVALID_YEAR'}
			if y < 0 or y > 99:
				return {'error':'INVALID_YEAR'}
			d[6] = ((y / 10)&0x0F) << 4
			d[6] = d[6] | ((y % 10)&0x0F)
			
		if 'subsecond' in dt:
			try:
				s = int(dt['subsecond']) * 256
			except:
				return {'error':'INVALID_SUBSECOND'}
			if s < 0 or s > 255:
				return {'error':'INVALID_SUBSECOND'}
			d[7] = s
			
		if 'daylightsaving' in dt:
			if dt['daylightsaving'] == 'SUB1H':
				d[8] |= 2
			elif dt['daylightsaving'] == 'SUB1H':
				d[8] |= 1
				
		if 'storeoperation' in dt and dt['storeoperation'] == True:
			d[8] |= 0x04
			
		ret = self.interface.WriteData(self.RTC_TIME_CMD, d)
		if ret['error'] != 'NO_ERROR':
			return ret
		# verify
		time.sleep(0.2)
		ret = self.interface.ReadData(self.RTC_TIME_CMD, 9)
		if ret['error'] != 'NO_ERROR':
			return ret
		if (d==ret['data']):
			return {'error':'NO_ERROR'}
		else:
			if abs(ret['data'][0] - d[0]) < 2:
				ret['data'][0] = d[0]
				ret['data'][7] = d[7]
				if (d==ret['data']):
					return {'error':'NO_ERROR'}
				else:
					return {'error':'WRITE_FAILED'}
			else:
				return {'error':'WRITE_FAILED'}
				
	def GetAlarm(self):
		ret = self.interface.ReadData(self.RTC_ALARM_CMD, 9)
		if ret['error'] != 'NO_ERROR':
			return ret
		d = ret['data']
		alarm = {}
		if (d[0]&0x80) == 0x00:
			alarm['second'] = ((d[0]>>4)&0x07) * 10 + (d[0]&0x0F)
		
		if (d[1]&0x80) == 0x00:
			alarm['minute'] = ((d[1]>>4)&0x07) * 10 + (d[1]&0x0F)
		else:
			alarm['minute_period'] = d[7]
				
		if (d[2]&0x80) == 0x00:
			if (d[2]&0x40):
				hourFormat = '12'
				ampm = 'PM' if (d[2]&0x20) else 'AM'
				alarm['hour'] = str(((d[2]>>4)&0x01) * 10 + (d[2]&0x0F)) + ' ' + ampm
			else:
				hourFormat = '24'
				alarm['hour'] = ((d[2]>>4)&0x03) * 10 + (d[2]&0x0F)
		else:
			if d[4] == 0xFF and d[5] == 0xFF and d[6] == 0xFF:
				alarm['hour'] = 'EVERY_HOUR'
			else:
				h = ''
				n = 0
				for i in range(0, 3):
					for j in range(0, 8):
						if d[i+4]&((0x01<<j)&0xFF):
							if (d[2]&0x40):
								if (i*8+j) == 0:
									h12 = '12AM'
								elif (i*8+j) == 12:
									h12 = '12PM'
								else:
									h12 = (str(i*8+j+1)+'AM') if ((i*8+j) < 12) else (str(i*8+j-11)+'PM')
								h = (h + ';') if n > 0 else h
								h = h + h12
							else:
								h = (h + ';') if n > 0 else h
								h = h + str(i*8+j)
							n = n + 1
				alarm['hour'] = h	
			
		if (d[3]&0x40):
			if (d[3]&0x80) == 0x00:
				alarm['weekday'] = d[3]&0x07
			else:
				if d[8] == 0xFF:
					alarm['weekday'] = 'EVERY_DAY'
				else:
					day = ''
					n = 0
					for j in range(1, 8):
						if d[8]&((0x01<<j)&0xFF):
							day = (day + ';') if n > 0 else day
							day = day + str(j)
							n = n + 1
					alarm['weekday'] = day
		else:
			if (d[3]&0x80) == 0x00:
				alarm['day'] = ((d[3]>>4)&0x03) * 10 + (d[3]&0x0F)
			else:
				alarm['day'] = 'EVERY_DAY'
	
		return {'data':alarm, 'error':'NO_ERROR'}
		
	def SetAlarm(self, alarm):
		d = [0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF]
		if alarm == None or alarm == {}:
			#disable alarm
			return self.interface.WriteDataVerify(self.RTC_ALARM_CMD, d, 0.2)
			
		if 'second' in alarm:
			try:
				s = int(alarm['second'])
			except:
				return {'error':'INVALID_SECOND'}
			if s < 0 or s > 60:
				return {'error':'INVALID_SECOND'}
			d[0] = ((s / 10)&0x0F) << 4
			d[0] = d[0] | ((s % 10)&0x0F)
			
		if 'minute' in alarm:
			try:
				m = int(alarm['minute'])
			except:
				return {'error':'INVALID_MINUTE'}
			if m < 0 or m > 60:
				return {'error':'INVALID_MINUTE'}
			d[1] = ((m / 10)&0x0F) << 4
			d[1] = d[1] | ((m % 10)&0x0F)
		else:
			d[1] = d[1] | 0x80 #every minute
			#d[1] = d[1] | (0x80 if alarm['mask']['minutes'] else 0x00)
			
		if 'minute_period' in alarm:
			d[1] = d[1] | 0x80
			try:
				s = int(alarm['minute_period'])
			except:
				return {'error':'INVALID_MINUTE_PERIOD'}
			if s < 1 or s > 60:
				return {'error':'INVALID_MINUTE_PERIOD'}
			d[7] = s

		d[4] = 0xFF
		d[5] = 0xFF
		d[6] = 0xFF 
		if 'hour' in alarm:
			try:
				h = alarm['hour']
				if h == 'EVERY_HOUR':
					d[2] = 0x80
					
				elif isinstance(h, str) and h.find(';')<0:
					if (h.find('AM') > -1) or (h.find('PM') > -1):
						if (h.find('PM') > -1):
							hi = int(h.split('PM')[0])
							d[2] = (((hi / 10)&0x03) << 4)
							d[2] = d[2] | ((hi % 10)&0x0F)
							d[2] = d[2] | 0x20 | 0x40
						else:
							hi = int(h.split('AM')[0])
							d[2] = (((hi / 10)&0x03) << 4)
							d[2] = d[2] | ((hi % 10)&0x0F)
							d[2] = d[2] | 0x40
					else:
						d[2] = (((int(h) / 10)&0x03) << 4)
						d[2] = d[2] | ((int(h) % 10)&0x0F)
						
				elif isinstance(h, int):
					#assume 24 hour format
					d[2] = (((int(h) / 10)&0x03) << 4)
					d[2] = d[2] | ((int(h) % 10)&0x0F)
					
				elif (isinstance(h, str) and h.find(';')>=0):
					hs = 0x00000000
					hFormat = ''
					hl = h.split(';')
					hl = hl[0:-1] if (not bool(hl[-1].strip())) else hl # remove ending empty string if there is ; at the end of list
					for i in hl:
						if (i.find('AM') > -1) or (i.find('PM') > -1):
							if i.find('AM') > -1:
								ham = int(i.split('AM')[0])
								if ham < 12:
									hs = hs | (0x00000001<<(ham))
								else:
									hs = hs | 0x00000001
							else:
								hpm = int(i.split('PM')[0])
								if hpm < 12:
									hs = hs | (0x00000001<<(hpm+12))
								else:
									hs = hs | (0x00000001<<(12))
						else:
							hs = hs | (0x00000001<<int(i))
					#d[2] = d[2] | (0x40 if hFormat == '12' else 0x00)
					d[2] = 0x80
					d[4] = hs&0x000000FF;
					hs = hs >> 8;
					d[5] = hs&0x000000FF;
					hs = hs >> 8;
					d[6] = hs&0x000000FF;
			except:
				return {'error':'INVALID_HOUR'}
		else:
			d[2] = 0x80 #every hour
		
		d[8] = 0xFF
		if 'weekday' in alarm:
			try:
				day = alarm['weekday']
				if day == 'EVERY_DAY':
					d[3] = 0x80 | 0x40
					
				elif isinstance(day, str) and day.find(';')<0:
					dw = int(day)
					d[3] = d[3] | (dw&0x0F) | 0x40
						
				elif isinstance(day, int):
					dw = int(day)
					d[3] = d[3] | (dw&0x0F) | 0x40
					
				elif (isinstance(day, str) and day.find(';')>=0):
					d[3] = 0x40 | 0x80
					ds = 0x00
					dl = day.split(';')
					dl = dl[0:-1] if (not bool(dl[-1].strip())) else dl
					for i in dl:
						ds = ds | (0x01<<int(i))
					d[8] = ds
			except:
				return {'error':'INVALID_WEEKDAY'}
		elif 'day' in alarm:
			try:
				day = alarm['day']
				if day == 'EVERY_DAY':
					d[3] = 0x80
					
				else:
					dm = int(day)
					d[3] = (((dm / 10)&0x03) << 4)
					d[3] = d[3] | ((dm % 10)&0x0F)
			except:
				return {'error':'INVALID_DAY_OF_MONTH'}
		else:
			d[3] = 0x80 #every day
			
		ret = self.interface.WriteData(self.RTC_ALARM_CMD, d)
		if ret['error'] != 'NO_ERROR':
			return ret
		# verify
		time.sleep(0.2)
		ret = self.interface.ReadData(self.RTC_ALARM_CMD, 9)
		if ret['error'] != 'NO_ERROR':
			return ret
		if (d==ret['data']):
			return {'error':'NO_ERROR'}
		else:
			h1 = d[2]
			h2 = ret['data'][2]
			if (h1&0x40): # convert to 24 hour format
				h1Bin = ((h1>>4)&0x01) * 10 + (h1&0x0F)
				h1Bin = h1Bin if h1Bin < 12 else 0
				h1Bin = h1Bin + (12 if h1&0x20 else 0)
			else:
				h1Bin = ((h1>>4)&0x03) * 10 + (h1&0x0F)
			if (h2&0x40): # convert to 24 hour format
				h2Bin = ((h2>>4)&0x01) * 10 + (h2&0x0F)
				h2Bin = h2Bin if h2Bin < 12 else 0
				h2Bin = h2Bin + (12 if h2&0x20 else 0)
			else:
				h2Bin = ((h2>>4)&0x03) * 10 + (h2&0x0F)
			d[2] = h1Bin | (d[2]&0x80)
			ret['data'][2] = h2Bin | (ret['data'][2]&0x80)
			if (d==ret['data']):
				return {'error':'NO_ERROR'}
			else:
				#print 'wr', d
				#print 'rd', ret['data']
				return {'error':'WRITE_FAILED'}

class PiJuicePower():

	WATCHDOG_ACTIVATION_CMD = 0X61
	POWER_OFF_CMD = 0x62
	WAKEUP_ON_CHARGE_CMD = 0x63
	SYSTEM_POWER_SWITCH_CTRL_CMD = 0X64
	
	def __init__(self, interface):
		self.interface = interface
	
	def __enter__(self):
		# Just return this object so it can be used in a with statement
		return self

	def __exit__(self, exc_type, exc_val, exc_tb):
		return False  # Don't suppress exceptions.
		
	def SetPowerOff(self, delay):
		return self.interface.WriteData(self.POWER_OFF_CMD, [delay])
		
	def GetPowerOff(self):
		return self.interface.ReadData(self.POWER_OFF_CMD, 1)
		
	def SetWakeUpOnCharge(self, arg):
		try:
			if arg == 'DISABLED':
				d = 0XFF
			elif int(arg) >= 0 and int(arg) <= 100:
				d = int(arg)
		except:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteData(self.WAKEUP_ON_CHARGE_CMD, [d])
		
	def GetWakeUpOnCharge(self):
		ret = self.interface.ReadData(self.WAKEUP_ON_CHARGE_CMD, 1)
		if ret['error'] != 'NO_ERROR':
			return ret
		else:
			d = ret['data'][0]
			if d == 0xFF:
				return {'data':'DISABLED', 'error':'NO_ERROR'}
			else:
				return {'data':d, 'error':'NO_ERROR'}
	
	# input argument 1 - 65535 minutes activates watchdog, 0 disables watchdog
	def SetWatchdog(self, minutes):
		try:
			d = int(minutes) & 0XFFFF
		except:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteData(self.WATCHDOG_ACTIVATION_CMD, [d&0xFF, (d >> 8)&0xFF]) 
		
	def GetWatchdog(self):
		ret = self.interface.ReadData(self.WATCHDOG_ACTIVATION_CMD, 2)
		if ret['error'] != 'NO_ERROR':
			return ret
		else:
			d = ret['data']
			return {'data':(d[1] << 8) | d[0], 'error':'NO_ERROR'}
			
	def SetSystemPowerSwitch(self, state):
		try:
			d = int(state)/100
		except:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteData(self.SYSTEM_POWER_SWITCH_CTRL_CMD, [d])
		
	def GetSystemPowerSwitch(self):
		ret = self.interface.ReadData(self.SYSTEM_POWER_SWITCH_CTRL_CMD, 1)
		if ret['error'] != 'NO_ERROR':
			return ret
		else:
			return {'data':ret['data'][0] * 100, 'error':'NO_ERROR'}
		
class PiJuiceConfig():

	BATTERY_PROFILE_ID_CMD = 0x52
	BATTERY_PROFILE_CMD = 0x53
	BATTERY_TEMP_SENSE_CONFIG_CMD = 0x5D
	RUN_PIN_CONFIG_CMD = 0X5F
	POWER_REGULATOR_CONFIG_CMD = 0X60
	LED_CONFIGURATION_CMD = 0x6A
	BUTTON_CONFIGURATION_CMD = 0x6E
	I2C_ADDRESS_CMD = 0x7C
	ID_EEPROM_WRITE_PROTECT_CTRL_CMD = 0x7E
	ID_EEPROM_ADDRESS_CMD = 0x7F
	RESET_TO_DEFAULT_CMD = 0xF0
	FIRMWARE_VERSION_CMD = 0XFD 
	
	def __init__(self, interface):
		self.interface = interface
	
	def __enter__(self):
		# Just return this object so it can be used in a with statement
		return self

	def __exit__(self, exc_type, exc_val, exc_tb):
		return False  # Don't suppress exceptions.
		
	batteryProfiles = ['BP6X', 'BP7X', 'SNN5843', 'LIPO8047109']
	def SetBatteryProfile(self, profile):
		id = None
		if profile == 'DEFAULT':
			id = 0xFF
		elif profile == 'CUSTOM':
			id = 0x0F
		else:
			try:
				id = self.batteryProfiles.index(profile)
			except:
				return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteData(self.BATTERY_PROFILE_ID_CMD, [id])
	
	batteryProfileSources = ['HOST', 'DIP_SWITCH', 'RESISTOR']
	batteryProfileValidity = ['VALID', 'INVALID']
	def GetBatteryProfileStatus(self):  
		ret = self.interface.ReadData(self.BATTERY_PROFILE_ID_CMD, 1)
		if ret['error'] != 'NO_ERROR':
			return ret
		else:
			id = ret['data'][0]
			if id == 0xF0:
				return {'data':{'validity':'DATA_WRITE_NOT_COMPLETED'}, 'error':'NO_ERROR'}
			origin = 'CUSTOM' if (id&0x0F) == 0x0F else 'PREDEFINED'
			source = self.batteryProfileSources[(id>>4)&0x03]
			validity = self.batteryProfileValidity[(id>>6)&0x01]
			profile = None
			try:
				profile = self.batteryProfiles[(id&0x0F)]
			except:
				profile = 'UNKNOWN'
			return {'data':{'validity':validity, 'source':source, 'origin':origin, 'profile':profile}, 'error':'NO_ERROR'}
		
	def GetBatteryProfile(self):
		ret = self.interface.ReadData(self.BATTERY_PROFILE_CMD, 14)
		if ret['error'] != 'NO_ERROR':
			return ret
		else:
			d = ret['data']
			if all(v == 0 for v in d):
				return {'data':'INVALID', 'error':'NO_ERROR'} 
			profile = {}
			profile['capacity'] = (d[1] << 8) | d[0]
			profile['chargeCurrent'] = d[2] * 75 + 550
			profile['terminationCurrent'] = d[3] * 50 + 50
			profile['regulationVoltage'] = d[4] * 20 + 3500
			profile['cutoffVoltage'] = d[5] * 20
			profile['tempCold'] = ctypes.c_byte(d[6]).value
			profile['tempCool'] = ctypes.c_byte(d[7]).value
			profile['tempWarm'] = ctypes.c_byte(d[8]).value
			profile['tempHot'] = ctypes.c_byte(d[9]).value
			profile['ntcB'] = (d[11] << 8) | d[10]
			profile['ntcResistance'] = ((d[13] << 8) | d[12]) * 10
			return {'data':profile, 'error':'NO_ERROR'}

	def SetCustomBatteryProfile(self, profile):
		d = [0x00] * 14
		try:
			c = profile['capacity']
			d[0] = c&0xFF
			d[1] = (c >> 8)&0xFF
			d[2] = int(round((profile['chargeCurrent'] - 550) / 75))
			d[3] = int(round((profile['terminationCurrent'] - 50) / 50))
			d[4] = int(round((profile['regulationVoltage'] - 3500) / 20))
			d[5] = int(round(profile['cutoffVoltage'] / 20))
			d[6] = ctypes.c_ubyte(profile['tempCold']).value
			d[7] = ctypes.c_ubyte(profile['tempCool']).value
			d[8] = ctypes.c_ubyte(profile['tempWarm']).value
			d[9] = ctypes.c_ubyte(profile['tempHot']).value
			B = profile['ntcB']
			d[10] = B&0xFF
			d[11] = (B >> 8)&0xFF
			R = profile['ntcResistance'] / 10
			d[12] = R&0xFF
			d[13] = (R >> 8)&0xFF
		except:
			return {'error':'BAD_ARGUMENT'}
		print d
		return self.interface.WriteDataVerify(self.BATTERY_PROFILE_CMD, d, 0.2)
	
	batteryTempSenseOptions = ['NOT_USED', 'NTC', 'ON_BOARD', 'AUTO_DETECT']
	def GetBatteryTempSenseConfig(self):
		result = self.interface.ReadData(self.BATTERY_TEMP_SENSE_CONFIG_CMD, 1)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data']
			if d[0] < len(self.batteryTempSenseOptions):
				return {'data':self.batteryTempSenseOptions[d[0]], 'error':'NO_ERROR'} 
			else:
				return {'error':'UNKNOWN_DATA'}
			
	def SetBatteryTempSenseConfig(self, config):
		ind = self.batteryTempSenseOptions.index(config)
		if ind == None:
			return {'error':'BAD_ARGUMENT'}
		data = [ind]
		return self.interface.WriteDataVerify(self.BATTERY_TEMP_SENSE_CONFIG_CMD, data)
	
	buttons = ['SW' + str(i+1) for i in range(0,3)]
	buttonEvents = ['PRESS', 'RELEASE', 'SINGLE_PRESS', 'DOUBLE_PRESS', 'LONG_PRESS1', 'LONG_PRESS2']
	#functionTypes = ['HARDWARE', 'SYSTEM', 'USER']
	def GetButtonConfiguration(self, button):
		returnObj = {}
		b = None
		try: 
			b = self.buttons.index(button)
		except ValueError:
			return {'error':'BAD_ARGUMENT'}
		result = self.interface.ReadData(self.BUTTON_CONFIGURATION_CMD + b, 12)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data']
			config = {}
			for i in range(0, len(self.buttonEvents)):
				config[self.buttonEvents[i]] = {} 
				try:
					if d[i*2] == 0:
						config[self.buttonEvents[i]]['function'] = 'NO_FUNC'
					elif d[i*2]&0xF0 == 0:
						config[self.buttonEvents[i]]['function'] = pijuice_hard_functions[(d[i*2]&0x0F)-1]
					elif d[i*2]&0xF0 == 0x10:
						config[self.buttonEvents[i]]['function'] = pijuice_sys_functions[(d[i*2]&0x0F)-1]
					elif d[i*2]&0xF0 == 0x20:
						config[self.buttonEvents[i]]['function'] = pijuice_user_functions[d[i*2]&0x0F]#'USER_FUNC' + str(d[i*2]&0x0F)
					else:
						config[self.buttonEvents[i]]['function'] = 'UNKNOWN'
				except IndexError:
					config[self.buttonEvents[i]]['function'] = 'UNKNOWN'
				config[self.buttonEvents[i]]['parameter'] = d[i*2+1] * 100
			return {'data':config, 'error':'NO_ERROR'}
			
	def SetButtonConfiguration(self, button, config):
		b = None
		try:
			b = self.buttons.index(button)
		except ValueError:
			return {'error':'BAD_ARGUMENT'}
		data = [0x00] * (len(self.buttonEvents) * 2)
		for i in range(0, len(self.buttonEvents)):
			try:
				data[i*2] = pijuice_hard_functions.index(config[self.buttonEvents[i]]['function']) + 0x01
			except ValueError:
				try:
					data[i*2] = pijuice_sys_functions.index(config[self.buttonEvents[i]]['function']) + 0x11
				except:
					try:
						data[i*2] = pijuice_user_functions.index(config[self.buttonEvents[i]]['function']) + 0x20
					except:
						data[i*2] = 0
			data[i*2+1] = (config[self.buttonEvents[i]]['parameter'] / 100) & 0xff
		return self.interface.WriteDataVerify(self.BUTTON_CONFIGURATION_CMD + b, data, 0.4)
		
	leds = ['D1', 'D2']
	ledFunctions = ['NOT_USED', 'CHARGE_STATUS', 'ON_OFF_STATUS', 'USER_LED']
	def GetLedConfiguration(self, led):
		i = None
		try:
			i = self.leds.index(led)
		except ValueError:
			return {'error':'BAD_ARGUMENT'}
		ret = self.interface.ReadData(self.LED_CONFIGURATION_CMD + i, 4)
		if ret['error'] != 'NO_ERROR':
			return ret
		else:
			config = {}
			try:
				config['function'] = self.ledFunctions[ret['data'][0]]
			except:
				return {'error':'UNKNOWN_CONFIG'}
			config['parameter'] = {'r':ret['data'][1], 'g':ret['data'][2], 'b':ret['data'][3]}
			return {'data':config, 'error':'NO_ERROR'}
			 
	def SetLedConfiguration(self, led, config):
		i = None
		d = [0x00, 0x00, 0x00, 0x00]
		try:
			i = self.leds.index(led)
			d[0] = self.ledFunctions.index(config['function'])
			d[1] = int(config['parameter']['r'])
			d[2] = int(config['parameter']['g'])
			d[3] = int(config['parameter']['b'])
		except ValueError:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteDataVerify(self.LED_CONFIGURATION_CMD + i, d, 0.2)
		
	powerRegulatorModes = ['POWER_SOURCE_DETECTION', 'LDO', 'DCDC']
	def GetPowerRegulatorMode(self):
		result = self.interface.ReadData(self.POWER_REGULATOR_CONFIG_CMD, 1)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data']
			if d[0] < len(self.powerRegulatorModes): 
				return {'data':self.powerRegulatorModes[d[0]], 'error':'NO_ERROR'} 
			else:
				return {'error':'UNKNOWN_DATA'}
				
	def SetPowerRegulatorMode(self, mode):
		try:
			ind = self.powerRegulatorModes.index(mode)
		except:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteDataVerify(self.POWER_REGULATOR_CONFIG_CMD, [ind])
		
	runPinConfigs = ['NOT_INSTALLED', 'INSTALLED']
	def GetRunPinConfig(self):
		result = self.interface.ReadData(self.RUN_PIN_CONFIG_CMD, 1)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			d = result['data']
			if d[0] < len(self.runPinConfigs):
				return {'data':self.runPinConfigs[d[0]], 'error':'NO_ERROR'} 
			else:
				return {'error':'UNKNOWN_DATA'}
				
	def SetRunPinConfig(self, config):
		try:
			ind = self.runPinConfigs.index(config)
		except:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteDataVerify(self.RUN_PIN_CONFIG_CMD, [ind]) 
		
	def GetAddress(self, slave):
		if slave != 1 and slave != 2:
			return {'error':'BAD_ARGUMENT'}
		result = self.interface.ReadData(self.I2C_ADDRESS_CMD + slave - 1, 1)
		if result['error'] != 'NO_ERROR':
			return result
		else:
			return {'data':format(result['data'][0], 'x'), 'error':'NO_ERROR'} 
			
	def SetAddress(self, slave, hexAddress):
		adr = None
		try:
			adr = int(str(hexAddress), 16)
		except:
			return {'error':'BAD_ARGUMENT'}
		if (slave != 1 and slave != 2) or adr < 0 or adr > 0x7F:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteData(self.I2C_ADDRESS_CMD + slave - 1, [adr])
		
	def GetIdEepromWriteProtect(self):
		ret = self.interface.ReadData(self.ID_EEPROM_WRITE_PROTECT_CTRL_CMD, 1)
		if ret['error'] != 'NO_ERROR':
			return ret
		else:
			status = True if ret['data'][0] == 1 else False
			return {'data':status, 'error':'NO_ERROR'}
			
	def SetIdEepromWriteProtect(self, status):
		d = None
		if status == True:
			d = 1
		elif status == False:
			d = 0
		else:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteDataVerify(self.ID_EEPROM_WRITE_PROTECT_CTRL_CMD, [d])
			
	idEepromAddresses = ['50', '52']
	def GetIdEepromAddress(self):
		ret = self.interface.ReadData(self.ID_EEPROM_ADDRESS_CMD, 1)
		if ret['error'] != 'NO_ERROR':
			return ret
		else:
			return {'data':format(ret['data'][0], 'x'), 'error':'NO_ERROR'} 
			
	def SetIdEepromAddress(self, hexAddress):
		if str(hexAddress) in self.idEepromAddresses:
			adr = int(str(hexAddress), 16)
		else:
			return {'error':'BAD_ARGUMENT'}
		return self.interface.WriteDataVerify(self.ID_EEPROM_ADDRESS_CMD, [adr])
		
	def SetDefaultConfiguration(self):
		return self.interface.WriteData(self.RESET_TO_DEFAULT_CMD, [0xaa, 0x55, 0x0a, 0xa3])		
		
	def GetFirmvareVersion(self):	
		ret = self.interface.ReadData(self.FIRMWARE_VERSION_CMD, 2)
		if ret['error'] != 'NO_ERROR':
			return ret
		else:
			return {'data':{'version':format(ret['data'][0], 'x'), 'variant':format(ret['data'][1], 'x')}, 'error':'NO_ERROR'}
			
	def RunTestCalibration(self):
		self.interface.WriteData(248, [0x55, 0x26, 0xa0, 0x2b])
# Create an interface object for accessing PiJuice features via I2C bus.
class PiJuice(object):

	def __init__(self, bus=1, address=0x14):
		self.interface = PiJuiceInterface(bus, address)
		self.status = PiJuiceStatus(self.interface)
		self.config = PiJuiceConfig(self.interface)
		self.power = PiJuicePower(self.interface)
		self.rtcAlarm = PiJuiceRtcAlarm(self.interface)
		
	def __enter__(self):
		# Just return this object so it can be used in a with statement
		return self

	def __exit__(self, exc_type, exc_val, exc_tb):
		return False  # Don't suppress exceptions.
		