#!/usr/bin/env python3
#
# Author: Milan Neskovic, Pi Supply, 2021, https://github.com/mmilann

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# Usage:
# Use to read log messages from PiJuice, firmware version >= 1.6
# if there is file path as input argument it will append messages 
# to file, otherwise will only print to screen
# Usage: 
# 	Enable: python3 pijuice_log.py --enable "OTHER|5VREG_ON|5VREG_OFF|WAKEUP_EVT|ALARM_EVT|MCU_RESET"
#	Read: python3 pijuice_log.py
#	Read to file: python3 pijuice_log.py ./pijuice_log.txt
#	Disable logging: python3 pijuice_log.py --disable

from pijuice import PiJuice, PiJuiceInterface
import time, datetime, sys

LOGGING_CMD = 0xF6 #246
LOG_MSG_FRAME_SIZE = 31
LOG_READ_MSG_SIZE =	LOG_MSG_FRAME_SIZE + 1

vbat = lambda x:((x << 3) | 0x0800)/4096 * 3.3 * 137.4/100

def Parse_5VREG_ON(data):
	t = GetDateTime(data[2:])
	v5v = lambda x:(x << 4)/4096 * 3.3 * 2
	#v = vbat(ret['data'][10])#d/4096 * 3.3 * 137.4/100
	bat = ["{0:.3f}".format(vbat(b)) for b in data[11:21]]
	reg5v = ["{0:.3f}".format(v5v(b)) for b in data[21:31]]
	logStr = str(data[0]) + ' ' + LOG_MSG_DEFS[data[1]]['name'] + ' ' + str(t) + ', ' + ['SUCCESS,', 'NO ENOUGHR POWER'][data[10]&0X01] + '\n' \
	+ '	-battery: ' + str(bat) + '\n' \
	+ '	-5V GPIO: ' + str(reg5v) + '\n'
	return logStr

def Parse_5VREG_OFF(data):
	t = GetDateTime(data[2:])
	v5v = lambda x:(x << 4)/4096 * 3.3 * 2 
	curr = lambda x: ((x&0x7F)<<4)/4096*3.3*1000/50/8 if (x&0x80) else x/4096 * 2 * 3.3 * 100#(((x * 3300 * 25) >> 8)/1000) # else (( 1469 + ((2048*138)>>12) - (2048-((x&0x7F)<<4)) )*3300*10+1)>>14
	
	curr5Vgpio =  0 if (data[13] & 0x80) else (data[13] << 5)/1000#((-data[13]-256) << 5)/1000 if (data[13] & 0x80) else (data[13] << 5)/1000
	gpio5V = "{0:.3f}".format(v5v(data[14]))
	batSignal = ["{0:.3f}".format(vbat(b)) for b in data[15:23]]
	curr5vSignal = ["{0:.3f}".format(curr(b)) for b in data[23:31]]
	logStr = str(data[0]) + ' ' + LOG_MSG_DEFS[data[1]]['name'] +' '+str(t) + ', SoC:'+str((data[11]<<2)/10)+'%, ' + str(data[12])+ 'C, GPIO_5V: '+str(gpio5V)+'V, ' +str(curr5Vgpio)+'A'+ '\n' \
	+ '	-battery: ' + str(batSignal) + '\n' \
	+ '	-current: ' + str(curr5vSignal) + '\n'
	return logStr
	
def Parse_WAKEUP_EVT(data):
	t = GetDateTime(data[2:])
	
	status = GetStatus(data[11])
	
	gpio5V = "{0:.3f}".format(((data[20] << 8) | data[19])/1000)
	batVolt = "{0:.3f}".format(((data[18] << 8) | data[17])/1000)
	i = (data[22] << 8) | data[21]
	if (i & (1 << 15)):
		i = i - (1 << 16)
	curr5Vgpio = "{0:.3f}".format(i/1000)
	wkupOnChargeCfg = (data[14] << 8) | data[13]

	logStr = str(data[0]) + ' ' + LOG_MSG_DEFS[data[1]]['name'] +' '+str(t) + ', Battery: '+str((data[15]<<2)/10)+'%, ' +str(batVolt)+'V, ' + str(data[16])+ 'C, '+status['battery'] + '\n' \
	+ '	GPIO_5V: REGULATOR: ' + ('ON, ' if((data[12]&0x01)) else 'OFF, ')+str(gpio5V)+'V, ' +str(curr5Vgpio)+'A, '+ status['powerInput5vIo'] + '\n' \
	+ '	TRIGGERS: ' + ('POWER_BUTTON'if(data[10]&0x10) else '') + (' WATCHDOG'if(data[10]&0x08) else '') + (' IO'if(data[10]&0x04) else '') + (' RTC'if(data[10]&0x02) else '') + (' ON_CHARGE'if(data[10]&0x01) else '') + '\n' \
	+ '	WAKEUP_ON_CHARGE: ' + (str(wkupOnChargeCfg)if wkupOnChargeCfg!=0xFFFF else 'DISABLED') + '\n'
	
	return logStr

def Parse_ALARM_EVT(data):
	t = GetDateTime(data[2:])
	
	status = GetStatus(data[13])

	batVolt = "{0:.3f}".format(((data[17] << 8) | data[16])/1000)
	alarm = GetAlarm(data[20:])
	alarmStatus = GetAlarmStatus(data[10:])

	logStr = str(data[0]) + ' ' + LOG_MSG_DEFS[data[1]]['name'] +' '+str(t) + ', Battery: '+str((data[14]<<2)/10)+'%, ' +str(batVolt)+'V, ' + str(data[15])+ 'C, '+status['battery'] + '\n' \
	+ '	GPIO_INPUT: '+ str(status['powerInput5vIo'])+', USB_MICRO_INPUT: '+ str(status['powerInput']) + '\n' \
	+ '	STATUS: '+ str(alarmStatus) + '\n' \
	+ '	CONFIG: ' + str(alarm) + '\n'
	
	return logStr
	
def Parse_MCU_RESET(data):
	t = GetDateTime(data[2:])
	
	status = GetStatus(data[11])
	
	gpio5V = "{0:.3f}".format(((data[20] << 8) | data[19])/1000)
	batVolt = "{0:.3f}".format(((data[18] << 8) | data[17])/1000)
	i = (data[22] << 8) | data[21]
	if (i & (1 << 15)):
		i = i - (1 << 16)
	curr5Vgpio = "{0:.3f}".format(i/1000)
	wkupOnChargeCfg = (data[14] << 8) | data[13]

	logStr = str(data[0]) + ' ' + LOG_MSG_DEFS[data[1]]['name'] +' '+str(t) + ', Battery: '+str((data[15]<<2)/10)+'%, ' +str(batVolt)+'V, ' + str(data[16])+ 'C, '+status['battery'] + '\n' \
	+ '	GPIO_5V: REGULATOR: ' + ('ON, ' if((data[12]&0x01)) else 'OFF, ')+str(gpio5V)+'V, ' +str(curr5Vgpio)+'A, '+ status['powerInput5vIo'] + '\n' \
	+ '	STATE: ' + ['NORMAL', 'POWER_ON', 'POWER_RESET', 'UPDATE', 'CONFIG_RESET', 'UNKNOWN'][data[10]] + '\n' \
	+ '	WAKEUP_ON_CHARGE: ' + (str(wkupOnChargeCfg)if wkupOnChargeCfg!=0xFFFF else 'DISABLED') + '\n'
	
	return logStr
	
LOG_MSG_DEFS = [{'name':'NO_LOG   ', 'parser':{}}, 
				{'name':'MESSAGE  ', 'parser':{}},
				{'name':'VALUE	  ', 'parser':{}},
				{'name':'RESERVED1', 'parser':{}},
				{'name':'5VREG_ON ', 'parser':Parse_5VREG_ON},
				{'name':'5VREG_OFF', 'parser':Parse_5VREG_OFF},
				{'name':'WAKEUP_EVT  ', 'parser':Parse_WAKEUP_EVT},
				{'name':'ALARM_EVT  ', 'parser':Parse_ALARM_EVT},
				{'name':'MCU_RESET  ', 'parser':Parse_MCU_RESET},
				{'name':'RESERVED1', 'parser':{}},
				{'name':'ALARM_WRITE  ', 'parser':Parse_ALARM_EVT}]

LOG_ENABLE_LIST = ['OTHER', '5VREG_ON', '5VREG_OFF', 'WAKEUP_EVT', 'ALARM_EVT', 'MCU_RESET', 'RESERVED2']

def GetStatus(d):
	status = {}
	status['isFault'] = bool(d & 0x01)
	status['isButton'] = bool(d & 0x02)
	batStatusEnum = ['NORMAL', 'CHARGING_FROM_IN',
					'CHARGING_FROM_5V_IO', 'NOT_PRESENT']
	status['battery'] = batStatusEnum[(d >> 2) & 0x03]
	powerInStatusEnum = ['NOT_PRESENT', 'BAD', 'WEAK', 'PRESENT']
	status['powerInput'] = powerInStatusEnum[(d >> 4) & 0x03]
	status['powerInput5vIo'] = powerInStatusEnum[(d >> 6) & 0x03]
	return status
	
def GetAlarmStatus(d):
	r = {}
	if (d[0] & 0x01) and (d[0] & 0x04):
		r['alarm_wakeup_enabled'] = True
	else:
		r['alarm_wakeup_enabled'] = False
	if d[1] & 0x01:
		r['alarm_flag'] = True
	else:
		r['alarm_flag'] = False
	return r
		
def GetAlarm(d):
	alarm = {}
	if (d[0] & 0x80) == 0x00:
		alarm['second'] = ((d[0] >> 4) & 0x07) * 10 + (d[0] & 0x0F)

	if (d[1] & 0x80) == 0x00:
		alarm['minute'] = ((d[1] >> 4) & 0x07) * 10 + (d[1] & 0x0F)
	else:
		alarm['minute_period'] = d[7]

	if (d[2] & 0x80) == 0x00:
		if (d[2] & 0x40):
			# hourFormat = '12'
			ampm = 'PM' if (d[2] & 0x20) else 'AM'
			alarm['hour'] = str(((d[2] >> 4) & 0x01) * 10 + (d[2] & 0x0F)) + ' ' + ampm
		else:
			# hourFormat = '24'
			alarm['hour'] = ((d[2] >> 4) & 0x03) * 10 + (d[2] & 0x0F)
	else:
		if d[4] == 0xFF and d[5] == 0xFF and d[6] == 0xFF:
			alarm['hour'] = 'EVERY_HOUR'
		else:
			h = ''
			n = 0
			for i in range(0, 3):
				for j in range(0, 8):
					if d[i+4] & ((0x01 << j) & 0xFF):
						if (d[2] & 0x40):
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

	if (d[3] & 0x40):
		if (d[3] & 0x80) == 0x00:
			alarm['weekday'] = d[3] & 0x07
		else:
			if d[8] == 0xFF:
				alarm['weekday'] = 'EVERY_DAY'
			else:
				day = ''
				n = 0
				for j in range(1, 8):
					if d[8] & ((0x01 << j) & 0xFF):
						day = (day + ';') if n > 0 else day
						day = day + str(j)
						n = n + 1
				alarm['weekday'] = day
	else:
		if (d[3] & 0x80) == 0x00:
			alarm['day'] = ((d[3] >> 4) & 0x03) * 10 + (d[3] & 0x0F)
		else:
			alarm['day'] = 'EVERY_DAY'

	return alarm

def GetDateTime(buf):
	#ret = self.interface.ReadData(self.RTC_TIME_CMD, 9)
	#if ret['error'] != 'NO_ERROR':
	#	return ret
	d = buf#ret['data']
	dt = {}
	dt['second'] = ((d[0] >> 4) & 0x07) * 10 + (d[0] & 0x0F)

	dt['minute'] = ((d[1] >> 4) & 0x07) * 10 + (d[1] & 0x0F)

	if (d[2] & 0x40):
		# hourFormat = '12'
		ampm = 'PM' if (d[2] & 0x20) else 'AM'
		dt['hour'] = str(((d[2] >> 4) & 0x01) * 10 + (d[2] & 0x0F)) + ' ' + ampm
	else:
		# hourFormat = '24'
		dt['hour'] = ((d[2] >> 4) & 0x03) * 10 + (d[2] & 0x0F)

	dt['weekday'] = d[3] & 0x07
	dt['day'] = ((d[4] >> 4) & 0x03) * 10 + (d[4] & 0x0F)

	dt['month'] = ((d[5] >> 4) & 0x01) * 10 + (d[5] & 0x0F)

	dt['year'] = ((d[6] >> 4) & 0x0F) * 10 + (d[6] & 0x0F) + 2000

	dt['subsecond'] = d[7] / 256
		 
	try:
		ts = datetime.datetime(dt['year'], dt['month'], dt['day'], dt['hour'], dt['minute'], dt['second'], int(d[7] *1000000 / 256))
	except:
		#print('error parsing time', dt)
		ts=dt
	
	return ts
	
ifs = PiJuiceInterface(1,0x14)

def GetPiJuiceLog(ifs):
	logStrOut = []
	#end = False
	i = 0
	while True:
		ret = ifs.ReadData(LOGGING_CMD, 31)
		if ret['error'] == 'NO_ERROR': 
			if (ret['data'][1] == 0): return {'data':logStrOut, 'error':'NO_ERROR'}
			  
			s = LOG_MSG_DEFS[ret['data'][1]]['parser'](ret['data'])
			logStrOut.insert(0,s)
			#print(s)

			time.sleep(0.01)
		else: #elif ret['error'] == 'COMMUNICATION_ERROR':
			print(ret)
			return ret

if '--enable' in sys.argv:
	ci = sys.argv.index('--enable')+1
	cfgList = []
	if len(sys.argv) > ci:
		cfgList = sys.argv[ci].split('|')
	else:
		print('Missing parameter')
		exit(-1)
	config = 0x00
	for i in range(0, len(LOG_ENABLE_LIST)):
		if (LOG_ENABLE_LIST[i] in cfgList):
			config |= 0x01<<i
	if config == 0x00:
		print('Invalid parameter')
		exit(-1)
	ifs.WriteData(LOGGING_CMD, [0x01, config])
	time.sleep(0.1)
	ret = ifs.ReadData(LOGGING_CMD, 31)
	if ret['error'] == 'NO_ERROR': 
		d = ret['data']
		if d[1] == 0 and d[2] == 0x01 and d[3] == config:
			print ('Log enable configured successfully', hex(config))
			exit(0)
		else:
			print ('Failed to configure log enable', hex(config), d)
			exit(-1)

if '--get_config' in sys.argv:
	ifs.WriteData(LOGGING_CMD, [0x02])
	time.sleep(0.1)
	ret = ifs.ReadData(LOGGING_CMD, 31)
	if ret['error'] == 'NO_ERROR' and ret['data'][1]==0 and ret['data'][2]==1: 
		msk = 0x01
		strOut = ''
		#print(ret['data'][3])
		for i in range(0, 7):
			if msk&ret['data'][3]: strOut += ('|'if strOut else '') + LOG_ENABLE_LIST[i]
			msk <<= 1
		print(strOut)
		exit(0)
	else:
		print(ret)
		exit(-1)
	exit(0)

if '--disable' in sys.argv:
	ifs.WriteData(LOGGING_CMD, [0x01, 0x00])
	time.sleep(0.1)
	ret = ifs.ReadData(LOGGING_CMD, 31)
	if ret['error'] == 'NO_ERROR':
		d = ret['data']
		if d[1] == 0 and d[2] == 0x01 and d[3] == 0x00:
			print ('Logging disabled successfully')
			exit(0)
		else:
			print ('Failed to disable logging')
			exit(-1)

ifs.WriteData(LOGGING_CMD, [0])
time.sleep(0.01)
ret = GetPiJuiceLog(ifs)
if ret['error'] != 'NO_ERROR': 
	time.sleep(0.5)
	ifs.WriteData(LOGGING_CMD, [0])
	time.sleep(0.01)
	ret = GetPiJuiceLog(ifs)
	
if ret['error'] == 'NO_ERROR': 
	if len(sys.argv)>1:
		fp = sys.argv[1]
		#print(fp)
		with open(fp, 'a') as file:
			for s in (ret['data']):
				file.write(s+'\n')
	else:
		for s in (ret['data']):
			print(s)
else:
	print ('failed to read log')
	exit(-1)
