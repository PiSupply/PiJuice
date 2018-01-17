#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import datetime
import calendar
import sys, getopt
import os
import subprocess
from pijuice import *
import logging
import json
import signal

pijuice = None
btConfig = {}

configPath = '/var/lib/pijuice/pijuice_config.JSON'#os.getcwd() + '/pijuice_config.JSON'
configData = {'system_task':{'enabled':False}}
status = {}
sysEvEn = False
minChgEn = False
minBatVolEn = False
lowChgEn = False
lowBatVolEn = False
chargeLevel = 50
timeCnt = 5
noPowEn = False
noPowCnt = 100
logger = None
dopoll = True 

def _SystemHalt(event):
	if (
		(event == 'low_charge' or event == 'low_battery_voltage' or event == 'no_power')
		and ('wakeup_on_charge' in configData['system_task']) 
		and ('enabled' in configData['system_task']['wakeup_on_charge']) 
		and configData['system_task']['wakeup_on_charge']['enabled']
		and 'trigger_level' in configData['system_task']['wakeup_on_charge']):
		try:
			tl = float(configData['system_task']['wakeup_on_charge']['trigger_level'])
			print 'wakeup on charge', tl
			print pijuice.power.SetWakeUpOnCharge(tl)
		except:
			tl = None 
	pijuice.status.SetLedBlink('D2', 3, [150, 0,0], 200, [0, 100,0], 200)
	print 'halt command'
	#os.system("sudo halt")
	subprocess.call(["sudo", "halt"])
		 
def ExecuteFunc(func, event, param):
	print func, event, param
	if func == 'SYS_FUNC_HALT':
		print 'SYS_FUNC_HALT'
		#logger.info(func+' '+str(event)+' '+str(param))
		_SystemHalt(event)
		 
	elif func == 'SYS_FUNC_HALT_POW_OFF':
		print 'SYS_FUNC_HALT_POW_OFF'
		#logger.info(func+' '+str(event)+' '+str(param))
		#os.system("sudo halt")
		pijuice.power.SetSystemPowerSwitch(0)
		pijuice.power.SetPowerOff(60)
		_SystemHalt(event)
	elif func == 'SYS_FUNC_SYS_OFF_HALT':
		print 'SYS_FUNC_SYS_OFF_HALT'
		pijuice.power.SetSystemPowerSwitch(0)
		_SystemHalt(event)
	elif func == 'SYS_FUNC_REBOOT':
		print 'reboot, SYS_FUNC_REBOOT'
		#logger.info(func+' '+str(event)+' '+str(param))
		#os.system("sudo reboot")
		subprocess.call(["sudo", "reboot"])
	elif ('USER_FUNC' in func) and ('user_functions' in configData) and (func in configData['user_functions']):
		#ind = int(func.split("USER_FUNC",1)[1])
		print 'execute user func', configData['user_functions'][func]
		try:
			os.system(configData['user_functions'][func] + ' '+ str(event)+ ' ' + str(param))
			#subprocess.call([configData['user_functions'][func], event, param])
		except:
			print 'failed to execute user func'
					
def _EvalButtonEvents():
	print pijuice
	btEvents = pijuice.status.GetButtonEvents()
	if btEvents['error'] == 'NO_ERROR':
		for b in pijuice.config.buttons:
			ev = btEvents['data'][b]
			if ev != 'NO_EVENT': 
				if btConfig[b][ev]['function'] != 'USER_EVENT':
					pijuice.status.AcceptButtonEvent(b)
					if btConfig[b][ev]['function'] != 'NO_FUNC':
						print 'batton event', b, ev
						ExecuteFunc(btConfig[b][ev]['function'], ev, b)
		return True
	else:
		return False
	
def _EvalCharge():
	global lowChgEn
	charge = pijuice.status.GetChargeLevel()
	if charge['error'] == 'NO_ERROR':
		level = float(charge['data'])
		global chargeLevel
		if ('threshold' in configData['system_task']['min_charge']):
			th = float(configData['system_task']['min_charge']['threshold'])
			if level == 0 or ((level < th) and ((chargeLevel-level) >= 0 and (chargeLevel-level) < 3)):
				#print 'level, th', level, th, chargeLevel, (level < th)
				#global lowChgEn
				#print 'lowChgEn' , lowChgEn
				if lowChgEn:
					# energy is low, take action
					#print 'ExecuteFunc'
					ExecuteFunc(configData['system_events']['low_charge']['function'], 'low_charge', level)
			
		chargeLevel = level
		#print level, '%'
		#logger.info(str(level)) 
		return True
	else:
		return False
		
def _EvalBatVoltage():
	global lowBatVolEn
	bv = pijuice.status.GetBatteryVoltage()
	if bv['error'] == 'NO_ERROR':
		v = float(bv['data']) / 1000
		if ('threshold' in configData['system_task']['min_bat_voltage']):
			try:
				th = float(configData['system_task']['min_bat_voltage']['threshold'])
			except:
				th = None
			#print 'v, th',v,th,'bv=',bv
			if th != None and v < th:
				if lowBatVolEn:
					# Battery voltage below thresholdw, take action
					ExecuteFunc(configData['system_events']['low_battery_voltage']['function'], 'low_battery_voltage', v)
			
		#logger.info(str(level))
		return True
	else:
		return False

def _EvalPowerInputs(status):
	global noPowCnt
	if ((status['powerInput'] == 'NOT_PRESENT') or (status['powerInput'] == 'BAD')) and ((status['powerInput5vIo'] == 'NOT_PRESENT') or (status['powerInput5vIo'] == 'BAD')):
		noPowCnt = noPowCnt + 1
	else:
		noPowCnt = 0
	if noPowCnt == 2:
		#print 'unplugged'
		ExecuteFunc(configData['system_events']['no_power']['function'], 'no_power', '')

def _EvalFaultFlags():
	faults = pijuice.status.GetFaultStatus()
	if faults['error'] == 'NO_ERROR': 
		faults = faults['data']
		print faults
		for f in (pijuice.status.faultEvents + pijuice.status.faults):
			if f in faults:
				#logger.info('fault:' + f)
				if sysEvEn and (f in configData['system_events']) and ('enabled' in configData['system_events'][f]) and configData['system_events'][f]['enabled']:
					if configData['system_events'][f]['function'] != 'USER_EVENT':
						pijuice.status.ResetFaultFlags([f])
						ExecuteFunc(configData['system_events'][f]['function'], f, faults[f])
		return True
	else:
		return False

def main():

	global pijuice
	global btConfig
	global configData 
	global status
	global sysEvEn
	global chargeLevel 
	global timeCnt
	global logger
	global minChgEn
	global minBatVolEn
	global lowChgEn
	global lowBatVolEn
	global noPowEn

	try:
		pijuice = PiJuice(1,0x14)
	except:
		print 'Failed to establish interface to PiJuce'
		sys.exit(0)
		
	try:
		with open(configPath, 'r') as outputConfig:
			configData = json.load(outputConfig)
	except:
		print 'Failed to load ', configPath
		#logger.info('Failed to load config') 

	try:
		for b in pijuice.config.buttons:
			conf = pijuice.config.GetButtonConfiguration(b)
			if conf['error'] == 'NO_ERROR':
				btConfig[b] = conf['data']
	except:
		powButton = None

	#logger = logging.getLogger('pijuice')
	#hdlr = logging.FileHandler('/home/pi/pijuice.log')
	#formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
	#hdlr.setFormatter(formatter)
	#logger.addHandler(hdlr) 
	#logger.setLevel(logging.INFO)
	
	if len(sys.argv) > 1 and str(sys.argv[1]) == 'stop':
		try:
			if (('watchdog' in configData['system_task']) 
				and ('enabled' in configData['system_task']['watchdog']) 
				and configData['system_task']['watchdog']['enabled'] 
				):

				print 'disabling watchdog'
				#ret = {'error':''}
				ret = pijuice.power.SetWatchdog(0)
				if ret['error'] != 'NO_ERROR':
					time.sleep(0.05)
					ret = pijuice.power.SetWatchdog(0)
					print ret
				#logger.info('watchdog disabled') 
				print 'watchdog disabled'
		except:
			print 'Error evaluating watchdog'
		sys.exit(0)

	if (('watchdog' in configData['system_task']) 
		and ('enabled' in configData['system_task']['watchdog']) 
		and configData['system_task']['watchdog']['enabled'] 
		and ('period' in configData['system_task']['watchdog']) 
		):
		try:
			p = int(configData['system_task']['watchdog']['period'])
			ret = pijuice.power.SetWatchdog(p)
			print 'watchdog enabled', p
			#logger.info('watchdog enabled') 
		except:
			p = None
			#logger.info('cannot enable watchdg') 
			print 'cannot enable watchdg', p

	sysEvEn = 'system_events' in configData
	minChgEn = ('min_charge' in configData['system_task']) and ('enabled' in configData['system_task']['min_charge']) and configData['system_task']['min_charge']['enabled']
	minBatVolEn = ('min_bat_voltage' in configData['system_task']) and ('enabled' in configData['system_task']['min_bat_voltage']) and configData['system_task']['min_bat_voltage']['enabled']
	lowChgEn = sysEvEn and ('low_charge' in configData['system_events']) and ('enabled' in configData['system_events']['low_charge']) and configData['system_events']['low_charge']['enabled']
	lowBatVolEn = sysEvEn and ('low_battery_voltage' in configData['system_events']) and ('enabled' in configData['system_events']['low_battery_voltage']) and configData['system_events']['low_battery_voltage']['enabled']
	noPowEn = sysEvEn and ('no_power' in configData['system_events']) and ('enabled' in configData['system_events']['no_power']) and configData['system_events']['no_power']['enabled']

	if configData['system_task']['enabled']:
		print 'starting status poll'
		while dopoll:
			ret = pijuice.status.GetStatus()
			if ret['error'] == 'NO_ERROR':
				status = ret['data']
				if status['isButton']: 
					_EvalButtonEvents()
			
				timeCnt = timeCnt - 1
				if timeCnt == 0:
					timeCnt = 5
					if ('isFault' in status) and status['isFault']:
						_EvalFaultFlags()
					if minChgEn:
						_EvalCharge()
					if minBatVolEn:
						_EvalBatVoltage()
					if noPowEn:
						_EvalPowerInputs(status)

			time.sleep(1)
	else:
		print 'Task is disabled'
			
if __name__ == '__main__':
	main()
