#!/usr/bin/env python
# -*- coding: utf-8 -*-

from tkinter import *
from tkinter.ttk import *
from pijuice import *
import tkinter.messagebox
import copy
import os
import subprocess
import json

try:
	pijuice = PiJuice(1,0x14)
except:
	pijuice = None

pijuiceConfigData = {}
PiJuiceConfigDataPath = '/var/lib/pijuice/pijuice_config.JSON' #os.getcwd() + '/pijuice_config.JSON'

def _ValidateIntEntry(var, oldVar, min, max):
	new_value = var.get()
	try:
		if var.get() != '':
			chg = int(var.get())
			if chg > max or chg < min:
				var.set(oldVar.get())
			else:
				oldVar.set(var.get())
		else:
			oldVar.set(var.get())
	except:
		var.set(oldVar.get())  
	oldVar.set(var.get())
	
def _ValidateFloatEntry(var, oldVar, min, max):
	new_value = var.get()
	try:
		if var.get() != '':
			chg = float(var.get())
			if chg > max or chg < min:
				var.set(oldVar.get())
			else:
				oldVar.set(var.get())
		else:
			oldVar.set(var.get())
	except:
		var.set(oldVar.get())  
	oldVar.set(var.get())

class PiJuiceFirmware:
	def __init__(self, master):
		self.frame = Frame(master, name='firmware')
		self.frame.grid(row=0, column=0, sticky=W)
		self.frame.rowconfigure(10, weight=1)
		self.frame.columnconfigure((0,3), weight=1, uniform=1)
		
		Label(self.frame, text="Firmware version:").grid(row=0, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
		self.firmVer = StringVar()
		self.firmVerLbl = Label(self.frame, textvariable=self.firmVer, text="").grid(row=0, column=1, padx=(2, 2), pady=(10, 0), sticky = W)
		self.newFirmStatus = StringVar()
		self.newFirmStatusLbl = Label(self.frame, textvariable=self.newFirmStatus, text="").grid(row=1, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
		status = pijuice.config.GetFirmvareVersion()
		self.ver = None
		if status['error'] == 'NO_ERROR':
			self.ver = status['data']
			self.firmVer.set(self.ver['version'][0] + '.' + self.ver['version'][1])
		else:
			self.firmVer.set(status['error'])
		
		self.newVer = '10'
		if self.ver and self.newVer:
			if int(str(self.newVer), 16) > int(str(self.ver['version']), 16):
				self.newFirmStatus.set('New firmware available')
			else:
				self.newFirmStatus.set('Firmware is up to date')
		
		self.defaultConfigBtn = Button(self.frame, text='Update firmware', state="normal", underline=0, command= self._UpdateFirmwareCmd)
		self.defaultConfigBtn.grid(row=1, column=1, padx=(2, 2), pady=(20, 0), sticky = W)
		
		self.firmUpdateErrors = ['NO_ERROR', 'I2C_BUS_ACCESS_ERROR', 'INPUT_FILE_OPEN_ERROR', 'STARTING_BOOTLOADER_ERROR', 'FIRST_PAGE_ERASE_ERROR',
		'EEPROM_ERASE_ERROR', 'INPUT_FILE_READ_ERROR', 'PAGE_WRITE_ERROR', 'PAGE_READ_ERROR', 'PAGE_VERIFY_ERROR', 'CODE_EXECUTE_ERROR']
	def _UpdateFirmwareCmd(self):
		q = tkinter.messagebox.showwarning('Update Fimware','Warning! Interrupting firmare update may lead to non functional PiJuice HAT.', parent=self.frame)
		if q:
			print('Updating fimware')
			inputFile = '/usr/share/pijuice/data/firmware/PiJuice.elf.binary'
			curAdr = pijuice.config.interface.GetAddress()
			if curAdr:
				adr = format(curAdr, 'x')
				ret = 256 - subprocess.call(['pijuiceboot', adr, inputFile])#subprocess.call([os.getcwd() + '/stmboot', adr, inputFile])
				print('firm res', ret) 
				if ret == 256:
					tkinter.messagebox.showinfo('Firmware update', 'Finished succesfully!', parent=self.frame)
				else:
					errorStatus = self.firmUpdateErrors[ret] if ret < 11 else '	UNKNOWN'
					msg = ''
					if errorStatus == 'I2C_BUS_ACCESS_ERROR':
						msg = 'Check if I2C bus is enabled.'
					elif errorStatus == 'INPUT_FILE_OPEN_ERROR':
						msg = 'Firmware binary file might be missing or damaged.'
					elif errorStatus == 'STARTING_BOOTLOADER_ERROR':
						msg = 'Try to start bootloader manualy. Press and hold button SW3 while powering up RPI and PiJuice.'
					tkinter.messagebox.showerror('Firmware update failed', 'Reason: ' + errorStatus + '. ' + msg, parent=self.frame)
			else:
				tkinter.messagebox.showerror('Firmware update', 'Unknown pijuice I2C address', parent=self.frame)
		

class PiJuiceHatTab:
	def __init__(self, master): 
		self.frame = Frame(master, name='hat')
		self.frame.grid(row=0, column=0, sticky=W)
		self.frame.rowconfigure(10, weight=1)
		self.frame.columnconfigure((0,2), weight=1, uniform=1)
		
		if pijuice == None:
			return
		
		Label(self.frame, text="Battery:").grid(row=0, column=0, padx=(2, 2), pady=(20, 0), sticky = W)
		self.status = StringVar()
		self.statusLbl = Label(self.frame,textvariable=self.status, text='')
		self.statusLbl.grid(row=0, column=1, padx=(2, 2), pady=(20, 0), columnspan=2, sticky = W)
		
		Label(self.frame, text="GPIO power input:").grid(row=1, column=0, padx=(2, 2), pady=(20, 0), sticky = W)
		self.gpioPower = StringVar()
		self.gpioPowerLbl = Label(self.frame,textvariable=self.gpioPower, text='')
		self.gpioPowerLbl.grid(row=1, column=1, padx=(2, 2), pady=(20, 0), columnspan=2, sticky = W)
			
		Label(self.frame, text="USB Micro power input:").grid(row=2, column=0, padx=(2, 2), pady=(20, 0), sticky = W)
		self.usbPower = StringVar()
		self.usbPowerLbl = Label(self.frame,textvariable=self.usbPower, text='')
		self.usbPowerLbl.grid(row=2, column=1, padx=(2, 2), pady=(20, 0), columnspan=2, sticky = W)
		
		Label(self.frame, text="Events:").grid(row=3, column=0, padx=(2, 2), pady=(20, 0), sticky = W)
		self.fault = StringVar()
		self.faultLbl = Label(self.frame,textvariable=self.fault, text='')
		self.faultLbl.grid(row=3, column=1, padx=(2, 2), pady=(20, 0), columnspan=2, sticky = W)
		
		#self.sysSwEnable = BooleanVar()
		#self.sysSwEnableCheck = Checkbutton(self.frame, text = 'System switch state', variable = self.sysSwEnable).grid(row=4, column=0, sticky = W, padx=(2, 2), pady=(20, 0))
		#self.sysSwLimit = StringVar()
		#self.sysSwLimitEntry = Entry(self.frame,textvariable=self.sysSwLimit)
		#self.sysSwLimitEntry.bind("<Return>", self._SetSysSwitch)
		#self.sysSwLimitEntry.grid(row=4, column=1, sticky = W, padx=(2, 2), pady=(20, 0))
		
		Label(self.frame, text="System switch:").grid(row=4, column=0, padx=(2, 2), pady=(20, 0), sticky = W)
		self.sysSwLimit = IntVar()
		Radiobutton(self.frame, text="Off", variable=self.sysSwLimit, value=0).grid(row=4, column=1, padx=(2, 2), pady=(10, 0), sticky='WE')
		Radiobutton(self.frame, text="500mA", variable=self.sysSwLimit, value=500).grid(row=4, column=2, padx=(2, 2), pady=(10, 0), sticky='WE')
		Radiobutton(self.frame, text="2100mA", variable=self.sysSwLimit, value=2100).grid(row=4, column=3, padx=(2, 2), pady=(10, 0), sticky='WE')
		self.sysSwLimit.trace("w", self._SetSysSwitch)
		
		Label(self.frame, text="PiJuice board variant:").grid(row=8, column=0, padx=(2, 2), pady=(20, 0), sticky = W)
		self.pijuiceBoardType = IntVar()
		Radiobutton(self.frame, text="HAT", variable=self.pijuiceBoardType, value=0).grid(row=8, column=1, padx=(2, 2), pady=(10, 0), sticky='WE')
		Radiobutton(self.frame, text="Zero", variable=self.pijuiceBoardType, value=1).grid(row=8, column=2, padx=(2, 2), pady=(10, 0), sticky='WE')
		self.pijuiceBoardType.trace("w", self._SetBoardVariant)
		try:
			with open('board_variant.txt') as f:
				if f.read() == 'Zero':
					self.pijuiceBoardType.set(1)
				else:
					self.pijuiceBoardType.set(0)
		except:
			self.pijuiceBoardType.set(0) 
		
		self.runTestBtn = Button(self.frame, text='Run Test', state="normal", underline=0, command= self._HatConfigCmd)
		self.runTestBtn.grid(row=9, column=0, padx=(2, 2), pady=(20, 0), sticky = W)
		self.counter = 0
		#print 'hat _RefreshStatus()'
		self.frame.after(1000, self._RefreshStatus)
		#self._RefreshStatus()
		#print 'hat _RefreshStatus()  end'
		
	def _RefreshStatus(self):
		try:
			ret = pijuice.status.GetStatus()
			if ret['error'] == 'NO_ERROR':
				self.usbPower.set(ret['data']['powerInput'])
				
			chg = pijuice.status.GetChargeLevel()
			if chg['error'] == 'NO_ERROR':
				self.status.set(str(chg['data'])+'%')
			else:
				self.status.set(chg['error'])
				
			temp = pijuice.status.GetBatteryTemperature()
			if temp['error'] == 'NO_ERROR':
				temp = str(temp['data']) + '°C'
			else:
				temp = '??°C'
				
			volt = pijuice.status.GetBatteryVoltage()
			if volt['error'] == 'NO_ERROR':
				self.status.set(str(chg['data'])+'%, '+str(float(volt['data'])/1000)+'V, ' + temp + ', ' +ret['data']['battery'])
			else:
				self.status.set(volt['error'])
				
			curr = pijuice.status.GetIoCurrent()
			if curr['error'] == 'NO_ERROR':
				curr = str(float(curr['data']) / 1000) + 'A, '
			else:
				curr = '' 
				
			v5v = pijuice.status.GetIoVoltage()
			if v5v['error'] == 'NO_ERROR':
				self.gpioPower.set(str(float(v5v['data'])/1000)+'V, ' + curr + ret['data']['powerInput5vIo'])
			else:
				self.gpioPower.set(v5v['error'])
				
			fau = pijuice.status.GetFaultStatus() 
			if fau['error'] == 'NO_ERROR':
				bpi = None
				if ('battery_profile_invalid' in fau['data']) and fau['data']['battery_profile_invalid']:
					bpi = 'battery profile invalid'
				cti = None
				if ('charging_temperature_fault' in fau['data']) and (fau['data']['charging_temperature_fault'] != 'NORMAL'):
					cti = 'charging temperature' + fau['data']['charging_temperature_fault']
				if (bpi == None) and (cti == None):
					self.fault.set('no fault')
				else:
					self.fault.set(bpi + ' ' + cti)
			else:
				self.fault.set(fau['error'])
				
			ret = pijuice.power.GetSystemPowerSwitch()
			if ret['error'] == 'NO_ERROR': 
				#self.sysSwEnable.set(int(ret['data']) > 0)
				self.sysSwLimit.set(ret['data'])
				#print ret['data']
			#else:
				#self.sysSwLimit.set(ret['error'])
		except:
			pass
		self.frame.after(6000, self._RefreshStatus)
		
	def _SetSysSwitch(self, *args):
		#if self.sysSwEnable.get(): 
		pijuice.power.SetSystemPowerSwitch(self.sysSwLimit.get())
		#else:
		#	pijuice.power.SetSystemPowerSwitch(0)
	def _SetBoardVariant(self, *args):
		try:
			with open("board_variant.txt", "w") as f:
				
				if self.pijuiceBoardType.get() == 1:
					f.write("Zero")
				else:
					f.write("HAT")
		except:
			print('error writing to board variant file')
	
	def _HatConfigCmd(self):
		if pijuice != None:
		
			#tkMessageBox.showinfo('Set switch', 'Set tet switch in calibration resistor position and press ok', parent=self.frame)
			time.sleep(1)
			
			#check charger i2c and fuel gauge ic fault status
			faultStatus = pijuice.interface.ReadData(250, 1)
			if faultStatus['error'] != 'NO_ERROR':
				tkinter.messagebox.showerror('On-board test', 'Test failed, I2C communication error.', parent=self.frame)
				return
			else:
				f = faultStatus['data'][0]
				chargeFaults = ['NO_FAULT', 'THERMAL_SHUTDOWN', 'BATTERY_TEMPERATURE_FAULT', 'WATCHDOG_TIMER_EXPIRED', 'SAFETY_TIMER_EXPIRED', 'IN_SUPPLY_FAULT', 'USB_SUPPLY_FAULT', 'BATTERY_FAULT', 'UNKNOWN']
				if (f & 0x01) != 0:
					tkinter.messagebox.showerror('On-board test', 'Test failed, Charger ic I2C communication error.', parent=self.frame)
					return
				elif (f & 0x0E) != 0:
					tkinter.messagebox.showerror('On-board test', 'Test failed,' + chargeFaults[(f & 0x0E) >> 1] + '.', parent=self.frame)
					return
				elif (f & 0x10) != 0:
					tkinter.messagebox.showerror('On-board test', 'Test failed, Fuel gauge ic I2C communication error.', parent=self.frame)
					return
				elif (f & 0x20) != 0:
					tkinter.messagebox.showerror('On-board test', 'Test failed, Battery temperature sensor fault.', parent=self.frame)
					return

			# charge level test
			chg = pijuice.status.GetChargeLevel()
			if chg['error'] == 'NO_ERROR':
				chg = chg['data']
				if chg < 1 or chg > 99:
					tkinter.messagebox.showerror('On-board test', 'Test failed, invalid charge level.', parent=self.frame)
					return
			else:
				tkinter.messagebox.showerror('On-board test', 'Test failed, I2C communication error.', parent=self.frame)
				return
				
			# power button test, turn off boost regulator, and check if it turns on button press
			ret = pijuice.power.SetPowerOff(0)
			if ret['error'] != 'NO_ERROR':
				tkinter.messagebox.showerror('Power button test', 'Test failed, I2C communication error.', parent=self.frame)
				return
			time.sleep(0.2)
			v5v = pijuice.status.GetIoVoltage()
			if v5v['error'] != 'NO_ERROR':
				tkinter.messagebox.showerror('Boost regulator test', 'Boost regulator test failed, I2C communication error.', parent=self.frame)
				return
			else:
				v = v5v['data']
				if v > 500:
					print(v)
					tkinter.messagebox.showerror('Boost regulator test', 'Boost regulator test failed, cannot turn off', parent=self.frame)
					return
			tkinter.messagebox.showinfo("Power button test", "Short press switch1 and press ok", icon='warning')
			time.sleep(0.2)
			
			# test boost regulator is turned on and have good voltage
			v5v = pijuice.status.GetIoVoltage()
			if v5v['error'] != 'NO_ERROR':
				tkinter.messagebox.showerror('Boost regulator test', 'Boost regulator test failed, I2C communication error.', parent=self.frame)
				return
			else:
				v = v5v['data']
				if v < 4900 or v > 5250:
					print(v)
					tkinter.messagebox.showerror('Boost regulator test', 'Boost regulator test failed, no turned on or bad regulator voltage', parent=self.frame)
					return
					
			if self.pijuiceBoardType.get() == 0:
				# sw2 button test
				pijuice.status.AcceptButtonEvent('SW2')
				btnEvents = pijuice.status.GetButtonEvents()
				if btnEvents['error'] != 'NO_ERROR':
					tkinter.messagebox.showinfo('SW2 button test', 'Test failed, I2C communication error.', parent=self.frame)
					return
				if btnEvents['data']['SW2'] != 'NO_EVENT':
					tkinter.messagebox.showinfo('SW2 button test', 'SW2 button test failed', parent=self.frame)
					return
				tkinter.messagebox.showinfo('SW2 button test', 'Single press SW2 button and click ok', parent=self.frame)
				btnEvents = pijuice.status.GetButtonEvents()
				if btnEvents['error'] != 'NO_ERROR':
					tkinter.messagebox.showinfo('SW2 button test', 'Test failed, I2C communication error.', parent=self.frame)
					return
				if btnEvents['data']['SW2'] != 'SINGLE_PRESS':
					tkinter.messagebox.showinfo('SW2 button test', 'SW2 button test failed, false or no event: ' + btnEvents['data']['SW2'], parent=self.frame)
					return
				
				# sw3 button test
				pijuice.status.AcceptButtonEvent('SW3')
				btnEvents = pijuice.status.GetButtonEvents()
				if btnEvents['error'] != 'NO_ERROR':
					tkinter.messagebox.showinfo('SW3 button test', 'Test failed, I2C communication error.', parent=self.frame)
					return
				if btnEvents['data']['SW3'] != 'NO_EVENT':
					tkinter.messagebox.showinfo('SW2 button test', 'SW3 button test failed', parent=self.frame)
					return
				tkinter.messagebox.showinfo('SW3 button test', 'Short press SW3 button and click ok', parent=self.frame)
				btnEvents = pijuice.status.GetButtonEvents()
				if btnEvents['error'] != 'NO_ERROR':
					tkinter.messagebox.showinfo('SW3 button test', 'Test failed, I2C communication error.', parent=self.frame)
					return
				if btnEvents['data']['SW3'] != 'PRESS' and btnEvents['data']['SW3'] != 'RELEASE':
					tkinter.messagebox.showinfo('SW3 button test', 'SW3 button test failed, false or no event: ' + btnEvents['data']['SW3'], parent=self.frame)
					return
				
			# User LED test
			ret = pijuice.status.SetLedState('D2', [150,255,255])
			if ret['error'] != 'NO_ERROR':
				tkinter.messagebox.showerror('User LED test', 'Test failed, I2C communication error.', parent=self.frame)
				return
			userLedTest = tkinter.messagebox.askquestion("LED D2 test", "LED D2 is turned on white?", icon='warning', parent=self.frame)
			if userLedTest != 'yes':
				tkinter.messagebox.showerror('LED D2 test', 'Test failed.', parent=self.frame)
				return
			ret = pijuice.status.SetLedState('D2', [0,0,0])
				
			# calibrate current sense
			pijuice.interface.WriteData(248, [0x55, 0x26, 0xa0, 0x2b]) # send calibration command
			# verify 
			time.sleep(0.4)
			# test load current after calibration
			curr = pijuice.status.GetIoCurrent()
			if curr['error'] == 'NO_ERROR':
				curr = curr['data']
				if curr < 42 or curr > 59:
					# try once more
					pijuice.interface.WriteData(248, [0x55, 0x26, 0xa0, 0x2b]) # send calibration command
					# verify 
					time.sleep(0.4)
					# test load current after calibration
					curr = pijuice.status.GetIoCurrent()
					if curr['error'] == 'NO_ERROR':
						curr = curr['data']
						if curr < 42 or curr > 59:
							print(curr)
							tkinter.messagebox.showinfo('Current sense calibration', 'Current sense calibration failed.', parent=self.frame)
							return
					else:
						tkinter.messagebox.showinfo('Current sense calibration', 'Current sense calibration failed, I2C communication error.', parent=self.frame)
						return
			else:
				tkinter.messagebox.showinfo('Current sense calibration', 'Current sense calibration failed, I2C communication error.', parent=self.frame)
				return
				
			# USB micro input charging test
			tkinter.messagebox.showinfo('Plug in USB micro power', 'Plug in power to PiJuce USB micro input and press ok', parent=self.frame)
			time.sleep(1.5)
			status = pijuice.status.GetStatus()
			if status['error'] != 'NO_ERROR':
				tkinter.messagebox.showerror('USB micro input charging test', 'Test failed, I2C communication error.', parent=self.frame)
				return
			else:
				if status['data']['battery'] != 'CHARGING_FROM_IN':
					tkinter.messagebox.showerror('USB micro input charging test', 'Test failed, Battery is not charging.', parent=self.frame)
					return
					
			tkinter.messagebox.showinfo('Set switch', 'Remove USB micro input from PiJuice board, set test switch in RPI 5V position and press ok', parent=self.frame)
			time.sleep(1.5)
			# 5V gpio charging test
			status = pijuice.status.GetStatus()
			if status['error'] != 'NO_ERROR':
				tkinter.messagebox.showinfo('5V GPIO charging test', 'Test failed, I2C communication error.', parent=self.frame)
				return
			else:
				if status['data']['battery'] != 'CHARGING_FROM_5V_IO':
					tkinter.messagebox.showinfo('5V GPIO charging test', 'Test failed, Battery is not charging.', parent=self.frame)
					return
					
			chg = pijuice.status.GetChargeLevel()
			if chg['error'] == 'NO_ERROR':
				chg = chg['data']
				if chg < 1 or chg > 99:
					tkinter.messagebox.showerror('On-board test', 'Test failed, invalid charging level. ' + str(chg), parent=self.frame)
					return
			else:
				tkinter.messagebox.showerror('On-board test', 'Test failed, I2C communication error.', parent=self.frame)
				return
			if chg > 50:
				chgColor = 'green'
			elif chg > 15:
				chgColor = 'orange'
			else:
				chgColor = 'red'
				
			# Charge status LED test
			ledTest = tkinter.messagebox.askquestion("Charging status LED test", "LED is blinking blue and " + chgColor + "?", icon='warning', parent=self.frame)
			if ledTest != 'yes':
				tkinter.messagebox.showerror('Charging status LED test', 'Test failed.', parent=self.frame)
				return

			# test system power switch
			pijuice.power.SetSystemPowerSwitch(500)
			time.sleep(0.2)
			powSwTest = tkinter.messagebox.askquestion("System power switch test", "Measure system voltage is greater than 3V?", icon='warning', parent=self.frame)
			if powSwTest != 'yes':
				tkinter.messagebox.showerror('System power switch test', 'Test failed.', parent=self.frame)
				return
			
			tkinter.messagebox.showinfo('Test complete', 'Unit successfully passed test.', parent=self.frame)
			
			#self.advWindow = PiJuiceHATConfigGui()
		
class PiJuiceConfigGui(Frame): 

	def __init__(self, isapp=True, name='pijuiceConfig'):
		Frame.__init__(self, name=name)
		self.grid(row=0, column=0)#self.pack(expand=Y, fill=BOTH)
		#self.master.maxsize(width=650, height=500)
		self.master.title('PiJuice Configuration')
		self.isapp = isapp
	
		global PiJuiceConfigDataPath
		global pijuiceConfigData
		
		try:
			with open(PiJuiceConfigDataPath, 'r') as outputConfig:
				pijuiceConfigData = json.load(outputConfig)
		except:
			pijuiceConfigData = {}
			print('Failed to load ', PiJuiceConfigDataPath)
		
		self.hatTest = PiJuiceHatTab(self) 

def PiJuiceGuiOnclosing():
	#if tkMessageBox.askokcancel("Quit", "Do you want to quit?"):
	if not os.path.exists(os.path.dirname(PiJuiceConfigDataPath)):
		print(os.path.dirname(PiJuiceConfigDataPath))
		os.makedirs(os.path.dirname(PiJuiceConfigDataPath)) 
	try:
		with open(PiJuiceConfigDataPath , 'w+') as outputConfig:
			json.dump(pijuiceConfigData, outputConfig, indent=2)
	except:
		print() 
	root.destroy() 
		
if __name__ == '__main__':

	root = Tk()
	
	root.protocol("WM_DELETE_WINDOW", PiJuiceGuiOnclosing)
	if pijuice == None:
		tkinter.messagebox.showerror('PuJuice Interfacing', 'Failed to use I2C bus. Check if I2C is enabled', parent=root)
	PiJuiceConfigGui().mainloop()
