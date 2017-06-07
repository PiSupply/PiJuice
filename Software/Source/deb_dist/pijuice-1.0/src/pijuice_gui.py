#!/usr/bin/env python
# -*- coding: utf-8 -*-

from Tkinter import *
from ttk import *
from pijuice import *
import tkMessageBox
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
		q = tkMessageBox.showwarning('Update Fimware','Warning! Interrupting firmare update may lead to non functional PiJuice HAT.', parent=self.frame)
		if q:
			print 'Updating fimware'
			inputFile = '/usr/share/pijuice/data/firmware/PiJuice.elf.binary'
			curAdr = pijuice.config.interface.GetAddress()
			if curAdr:
				adr = format(curAdr, 'x')
				ret = 256 - subprocess.call(['pijuiceboot', adr, inputFile])#subprocess.call([os.getcwd() + '/stmboot', adr, inputFile])
				print 'firm res', ret 
				if ret == 256:
					tkMessageBox.showinfo('Firmware update', 'Finished succesfully!', parent=self.frame)
				else:
					errorStatus = self.firmUpdateErrors[ret] if ret < 11 else '	UNKNOWN'
					msg = ''
					if errorStatus == 'I2C_BUS_ACCESS_ERROR':
						msg = 'Check if I2C bus is enabled.'
					elif errorStatus == 'INPUT_FILE_OPEN_ERROR':
						msg = 'Firmware binary file might be missing or damaged.'
					elif errorStatus == 'STARTING_BOOTLOADER_ERROR':
						msg = 'Try to start bootloader manualy. Press and hold button SW3 while powering up RPI and PiJuice.'
					tkMessageBox.showerror('Firmware update failed', 'Reason: ' + errorStatus + '. ' + msg, parent=self.frame)
			else:
				tkMessageBox.showerror('Firmware update', 'Unknown pijuice I2C address', parent=self.frame)
		
class PiJuiceHATConfig:
    def __init__(self, master):
		self.frame = Frame(master, name='hat')
		self.frame.grid(row=0, column=0, sticky=W)
		self.frame.rowconfigure(10, weight=1)
		self.frame.columnconfigure((0,3), weight=1, uniform=1)
		
		Label(self.frame, text="Run pin:").grid(row=0, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
		self.runPinConfig = StringVar()
		self.runPinConfigSel = Combobox(self.frame, textvariable=self.runPinConfig, state='readonly')
		self.runPinConfigSel['values'] = pijuice.config.runPinConfigs
		self.runPinConfigSel.grid(column=1, row=0, padx=(2, 2), pady=(10, 0), sticky = W)
		self.runPinConfigSel.bind("<<ComboboxSelected>>", self._RunPinConfigSelected)
		config = pijuice.config.GetRunPinConfig()
		if config['error'] != 'NO_ERROR':
			self.runPinConfig.set(config['error'])
		else:
			self.runPinConfigSel.current(pijuice.config.runPinConfigs.index(config['data']))
			
		self.slaveAddr = [{}, {}]
		self.slaveAddrEntry = [{}, {}]
		self.slaveAddrconfig = [{}, {}]
		self.oldAdr = ['', '']
		
		Label(self.frame, text="I2C Address1:").grid(row=1, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
		Label(self.frame, text="I2C Address2 (RTC):").grid(row=2, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
		for i in range(0, 2):
			self.slaveAddr[i] = StringVar()
			self.slaveAddr[i].trace("w", lambda name, index, mode, var=self.slaveAddr[i], id = i: self._ValidateSlaveAdr(var, id))
			self.slaveAddrEntry[i] = Entry(self.frame,textvariable=self.slaveAddr[i])
			self.slaveAddrEntry[i].grid(row=1+i, column=1, padx=(2, 2), pady=(10, 0), sticky=W)
			self.slaveAddrEntry[i].bind("<Return>", lambda x, id=i: self._WriteSlaveAddress(id))
			self.slaveAddrconfig[i] = pijuice.config.GetAddress(i+1)
			if self.slaveAddrconfig[i]['error'] != 'NO_ERROR':
				self.slaveAddr[i].set(self.slaveAddrconfig[i]['error'])
			else:
				self.oldAdr[i] = self.slaveAddrconfig[i]['data']
				self.slaveAddr[i].set(self.slaveAddrconfig[i]['data'])

		Label(self.frame, text="ID EEPROM Address:").grid(row=3, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
		self.idEepromAddr = StringVar()
		self.idEepromAddrSel = Combobox(self.frame, textvariable=self.idEepromAddr, state='readonly')
		self.idEepromAddrSel['values'] = pijuice.config.idEepromAddresses
		self.idEepromAddrSel.grid(column=1, row=3, padx=(2, 2), pady=(10, 0), sticky = W)
		self.idEepromAddrSel.bind("<<ComboboxSelected>>", self._IdEepromAddrSelected)
		config = pijuice.config.GetIdEepromAddress()
		if config['error'] != 'NO_ERROR':
			self.idEepromAddr.set(config['error'])
		else:
			self.idEepromAddrSel.current(pijuice.config.idEepromAddresses.index(config['data']))
			
		self.idEepromWpDisable = IntVar()
		self.idEepromWpDisableCheck = Checkbutton(self.frame, text = "ID EEPROM Write unprotect", variable = self.idEepromWpDisable).grid(row=4, column=0, sticky = W, padx=(2, 2), pady=(10, 0))
		self.idEepromWpDisable.trace("w", self._IdEepromWpDisableCheck)
		config = pijuice.config.GetIdEepromWriteProtect()
		if config['error'] != 'NO_ERROR':
			self.idEepromWpDisable.set(0)
		else:
			self.idEepromWpDisable.set(config['data'])
			
		Label(self.frame, text="Power regulator mode:").grid(row=5, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
		self.powerRegMode = StringVar()
		self.powerRegModeSel = Combobox(self.frame, textvariable=self.powerRegMode, state='readonly')
		self.powerRegModeSel['values'] = pijuice.config.powerRegulatorModes
		self.powerRegModeSel.grid(column=1, row=5, padx=(2, 2), pady=(10, 0), sticky = W)
		self.powerRegModeSel.bind("<<ComboboxSelected>>", self._PowerRegModeSelected)
		config = pijuice.config.GetPowerRegulatorMode()
		if config['error'] != 'NO_ERROR':
			self.powerRegMode.set(config['error'])
		else:
			self.powerRegModeSel.current(pijuice.config.powerRegulatorModes.index(config['data']))
	
		self.defaultConfigBtn = Button(self.frame, text='Reset to default configuration', state="normal", underline=0, command= self._ResetToDefaultConfigCmd)
		self.defaultConfigBtn.grid(row=6, column=0, padx=(2, 2), pady=(20, 0), sticky = W)

    def _ResetToDefaultConfigCmd(self):
		q = tkMessageBox.askokcancel('Reset Configuration','Warning! This action will reset PiJuice HAT configuration to default settings.', parent=self.frame)
		if q:
			status = pijuice.config.SetDefaultConfiguration()
			if status['error'] != 'NO_ERROR':
				tkMessageBox.showerror('Reset to default configuration', status['error'], parent=self.frame)
		
    def _WriteSlaveAddress(self, id):
		q = tkMessageBox.askquestion('Address update','Are you sure you want to change address?', parent=self.frame)
		if q == 'yes':
			status = pijuice.config.SetAddress(id+1, self.slaveAddr[id].get())
			if status['error'] != 'NO_ERROR':
				#self.slaveAddr[id].set(status['error'])
				tkMessageBox.showerror('Address update', status['error'], parent=self.frame)
			else:
				tkMessageBox.showinfo('Address update', "Success!", parent=self.frame)
		
    def _RunPinConfigSelected(self, event):
		status = pijuice.config.SetRunPinConfig(self.runPinConfig.get())
		if status['error'] != 'NO_ERROR':
			self.runPinConfig.set(status['error'])
			tkMessageBox.showerror('Run pin config', status['error'], parent=self.frame)
			
    def _IdEepromAddrSelected(self, event):
		status = pijuice.config.SetIdEepromAddress(self.idEepromAddr.get())
		if status['error'] != 'NO_ERROR':
			self.idEepromAddr.set(status['error'])
			tkMessageBox.showerror('ID EEPROM Address select', status['error'], parent=self.frame)
			
    def _IdEepromWpDisableCheck(self, *args):
		status = pijuice.config.SetIdEepromWriteProtect(self.idEepromWpDisable.get())
		if status['error'] != 'NO_ERROR':
			self.idEepromWpDisable.set(not self.idEepromWpDisable.get())
			tkMessageBox.showerror('ID EEPROM write protect', status['error'], parent=self.frame)
			
    def _PowerRegModeSelected(self, event):
		status = pijuice.config.SetPowerRegulatorMode(self.powerRegMode.get())
		if status['error'] != 'NO_ERROR':
			self.powerRegMode.set(status['error'])
			tkMessageBox.showerror('ID EEPROM Address select', status['error'], parent=self.frame)
			
    def _ValidateSlaveAdr(self, var, id):
		new_value = var.get()
		try:
			if var.get() != '':
				adr = int(str(var.get()), 16)
				if adr > 0x7F:
					var.set(self.oldAdr[id])
				else:
					self.oldAdr[id] = var.get()
			else:
				self.oldAdr[id] = var.get()
		except:
			var.set(self.oldAdr[id])  
		self.oldAdr[id] = var.get()
		
class PiJuiceButtonsConfig:
    def __init__(self, master):
		# frame to hold contentx
		self.frame = Frame(master, name='buttons')
		self.frame.grid(row=0, column=0, sticky=W)
		self.evFuncSelList = []
		self.evFuncList = []
		self.evParamEntryList = []
		self.evParamList = []
		self.configs = []
		self.errorState = False
		self.eventFunctions = ['NO_FUNC'] + copy.deepcopy(pijuice_hard_functions) + pijuice_sys_functions + pijuice_user_functions
		
		self.refreshConfig = StringVar()
		self.refreshConfigBtn = Button(self.frame, text='Refresh', underline=0, command=lambda v=self.refreshConfig: self.Refresh(v))
		self.refreshConfigBtn.grid(row=0, column=3)
		
		self.apply = StringVar()
		self.applyBtn = Button(self.frame, text='Apply', state="disabled", underline=0, command=lambda v=self.apply: self._ApplyNewConfig(v))
		self.applyBtn.grid(row=(len(pijuice.config.buttonEvents)+1)*len(pijuice.config.buttons)-1, column=3)
		
		self.errorStatus = StringVar()
		self.errorStatusLbl = Label(self.frame, text='', textvariable=self.errorStatus).grid(row=1, column=3)
		
		for i in range(0, len(pijuice.config.buttons)):
			Label(self.frame, text=pijuice.config.buttons[i]+":").grid(row=i*7, column=0, columnspan=2, padx=(5, 5),pady=(4, 0), sticky=W)
			Label(self.frame, text="Function:").grid(row=i*7, column=1, columnspan=2, padx=(2, 2),pady=(4, 0), sticky=W)
			Label(self.frame, text="Parameter:").grid(row=i*7, column=2, columnspan=2, padx=(2, 2),pady=(4, 0), sticky=W)

			for j in range(0, len(pijuice.config.buttonEvents)):
				r = i * (len(pijuice.config.buttonEvents) + 1) + j + 1
				ind = i * len(pijuice.config.buttonEvents) + j
				Label(self.frame, text=pijuice.config.buttonEvents[j]+":").grid(row=r, column=0, padx=(5, 5), sticky=W)

				func = StringVar()
				self.evFuncList.append(func)
				combo = Combobox(self.frame, textvariable=func, state='readonly')
				combo.grid(row=r, column=1, padx=(2, 2), sticky=W)
				combo['values'] = self.eventFunctions
				self.evFuncSelList.append(combo)
				param = StringVar()
				self.evParamList.append(param)
				ent = Entry(self.frame,textvariable=self.evParamList[ind])
				ent.grid(row=r, column=2, padx=(2, 2), sticky=W)
				self.evParamEntryList.append(ent)
					
				self.evFuncSelList[ind].bind("<<ComboboxSelected>>", self._ConfigEdited)
				self.evParamList[ind].trace("w", lambda name, index, mode, sv=param: self._ConfigEdited(sv))
				
			self.configs.append({})
			self.ReadConfig(i)
			
		self.frame.rowconfigure((len(pijuice.config.buttonEvents)+1)*len(pijuice.config.buttons), weight=1)
		self.frame.columnconfigure((0,3), weight=1, uniform=1)
		self.applyBtn.configure(state="disabled")

    def _ApplyNewConfig(self, v):
		for bind in range(0, len(pijuice.config.buttons)):
			config = {}
			for j in range(0, len(pijuice.config.buttonEvents)):
				ind = bind * len(pijuice.config.buttonEvents) + j
				config[pijuice.config.buttonEvents[j]] = {'function':self.evFuncList[ind].get(),'parameter':int(self.evParamList[ind].get())}
			if self.configs[bind]['error'] == 'NO_ERROR':
				if self.configs[bind]['data'] != config:
					status = pijuice.config.SetButtonConfiguration(pijuice.config.buttons[bind], config)
					if status['error'] != 'NO_ERROR':
						time.sleep(0.4)
						self.ReadConfig(bind)
					else:
						self.configs[bind]['data'] = config
						self.errorStatus.set('')
						
		self.applyBtn.configure(state="disabled")

    def Refresh(self, v):
		isError = False
		for i in range(0, len(pijuice.config.buttons)):
			if self.ReadConfig(i)['error'] != 'NO_ERROR':
				isError = True
		if not isError:
			self.errorStatus.set('')
		self.applyBtn.configure(state="disabled")
			
    def ReadConfig(self, bind):
		config = pijuice.config.GetButtonConfiguration(pijuice.config.buttons[bind])
		for j in range(0, len(pijuice.config.buttonEvents)):
			r = bind * (len(pijuice.config.buttonEvents) + 1) + j + 1
			ind = bind * len(pijuice.config.buttonEvents) + j
			if config['error'] == 'NO_ERROR':
				try:
					self.evFuncList[ind].current(self.eventFunctions.index(config['data'][pijuice.config.buttonEvents[j]]['function']))
				except:
					self.evFuncList[ind].set(config['data'][pijuice.config.buttonEvents[j]]['function'])
					
				self.evFuncSelList[ind].configure(state="readonly")
				self.evParamList[ind].set(config['data'][pijuice.config.buttonEvents[j]]['parameter'])
				self.evParamEntryList[ind].configure(state="normal")
			else:
				self.evFuncList[ind].set('error')
				self.evFuncSelList[ind].configure(state="disabled")
				self.evParamList[ind].set('error')
				self.evParamEntryList[ind].configure(state="readonly")
				self.errorState = True
				self.errorStatus.set(config['error'])
		self.configs[bind] = config
		return config
		
    def _ConfigEdited(self, sv):
		self.applyBtn.configure(state="normal")
		
    def _ConfigFuncSelected(self, event):
		self.applyBtn.configure(state="normal")

class PiJuiceLedConfig:
    def __init__(self, master):
		
		# frame to hold contentx
		self.frame = Frame(master, name='led')
		
		self.ledConfigsSel = []
		self.ledConfigs = []
		self.paramList = []
		self.paramEntryList = []
		self.configs = []
		
		for i in range(0, len(pijuice.config.leds)):
			Label(self.frame, text=pijuice.config.leds[i]+" function:").grid(row=i*5, column=0, padx=(5, 5), pady=(20, 0), sticky = W)
			ledConfig = StringVar()
			self.ledConfigs.append(ledConfig)
			ledConfigSel = Combobox(self.frame, textvariable=ledConfig, state='readonly')
			ledConfigSel['values'] = pijuice.config.ledFunctions
			#ledConfigSel.set('') 
			ledConfigSel.grid(column=1, row=i*5, padx=(5, 5), pady=(20, 0), sticky = W)
			self.ledConfigsSel.append(ledConfigSel)
			
			Label(self.frame, text=pijuice.config.leds[i]+" parameter:").grid(row=i*5+1, column=0, padx=(5, 5), pady=(10, 0), sticky = W)
			
			Label(self.frame, text="R:").grid(row=i*5+2, column=0, padx=(5, 5), pady=(2, 2), sticky = W)
			paramR = StringVar()
			self.paramList.append(paramR)
			ind = i * 3
			ent = Entry(self.frame,textvariable=self.paramList[ind])
			ent.grid(row=i*5+2, column=1, padx=(2, 2), pady=(2, 2), sticky='W')
			self.paramEntryList.append(ent)
				
			Label(self.frame, text="G:").grid(row=i*5+3, column=0, padx=(5, 5), pady=(2, 2), sticky = W)
			paramG = StringVar()
			self.paramList.append(paramG)
			ind = i * 3 + 1
			ent = Entry(self.frame,textvariable=self.paramList[ind])
			ent.grid(row=i*5+3, column=1, padx=(2, 2), pady=(2, 2), sticky='W')
			self.paramEntryList.append(ent)
			
			Label(self.frame, text="B:").grid(row=i*5+4, column=0, padx=(5, 5), pady=(2, 2), sticky = W)
			paramB = StringVar()
			self.paramList.append(paramB)
			ind = i * 3 + 2
			ent = Entry(self.frame,textvariable=self.paramList[ind])
			ent.grid(row=i*5+4, column=1, padx=(2, 2), pady=(2, 2), sticky='W')
			self.paramEntryList.append(ent)
			
		
			config = pijuice.config.GetLedConfiguration(pijuice.config.leds[i])
			self.configs.append({})
			if config['error'] == 'NO_ERROR':
				self.ledConfigsSel[i].current(pijuice.config.ledFunctions.index(config['data']['function']))
				paramR.set(config['data']['parameter']['r'])
				paramG.set(config['data']['parameter']['g'])
				paramB.set(config['data']['parameter']['b'])
				self.configs[i] = config['data']
			else:
				self.ledConfigsSel[i].set(config['error'])
				paramR.set('')
				paramG.set('')
				paramB.set('')
				self.configs[i] = None
				
			self.ledConfigsSel[i].bind("<<ComboboxSelected>>", lambda event, idx=i: self._NewConfigSelected(event, idx))
			paramR.trace("w", lambda name, index, mode, sv=paramR: self._ConfigEdited(sv))
			paramG.trace("w", lambda name, index, mode, sv=paramG: self._ConfigEdited(sv))
			paramB.trace("w", lambda name, index, mode, sv=paramB: self._ConfigEdited(sv))
		
		self.apply = StringVar()
		self.applyBtn = Button(self.frame, text='Apply', state="disabled", underline=0, command=lambda v=self.apply: self._ApplyNewConfig(v))
		self.applyBtn.grid(row=10, column=2)
			
		self.frame.rowconfigure(14, weight=1)
		self.frame.columnconfigure((0,2), weight=1, uniform=1)

    def _NewConfigSelected(self, event, i):
		self.applyBtn.configure(state="normal")
		
    def _ConfigEdited(self, sv):
		#_ValidateIntEntry(sv, self.oldParamVal, 0, 255)	
		self.applyBtn.configure(state="normal")
		
    def _ApplyNewConfig(self, v):
		for i in range(0, len(pijuice.config.leds)):
			ledConfig = {'function':self.ledConfigsSel[i].get(), 'parameter':{'r':self.paramList[i*3].get(), 'g':self.paramList[i*3+1].get(), 'b':self.paramList[i*3+2].get()}}
			if ( self.configs[i] == None
				or ledConfig['function'] != self.configs[i]['function']
				or ledConfig['parameter']['r'] != self.configs[i]['parameter']['r']
				or ledConfig['parameter']['g'] != self.configs[i]['parameter']['g']
				or ledConfig['parameter']['b'] != self.configs[i]['parameter']['b']
				):
				status = pijuice.config.SetLedConfiguration(pijuice.config.leds[i], ledConfig)
				#event.widget.set('')
				if status['error'] == 'NO_ERROR':
					self.applyBtn.configure(state="disabled")
					#time.sleep(0.2)
					#config = pijuice.config.GetLedConfiguration(pijuice.config.leds[i])
					#if config['error'] == 'NO_ERROR':
					#	event.widget.current(pijuice.config.ledFunctions.index(config['data']['function']))
					#else:
					#	event.widget.set(config['error'])
				else:
					tkMessageBox.showerror('Apply LED Configuration', status['error'], parent=self.frame)
					#event.widget.set(status['error'])
	
class PiJuiceBatteryConfig:
    def __init__(self, master):
		# frame to hold contentx
		self.frame = Frame(master, name='battery')
			
		# widgets to be displayed on 'Description' tab
		
		self.profileId = StringVar()
		self.profileSel = Combobox(self.frame, textvariable=self.profileId, state='readonly')
		vals = copy.deepcopy(pijuice.config.batteryProfiles)
		vals.append('CUSTOM')
		vals.append('DEFAULT')
		self.profileSel['values'] = vals
		self.profileSel.set('')
		self.profileSel.grid(column=0, row=1, sticky = W, pady=(0, 2))
		self.profileSel.bind("<<ComboboxSelected>>", self._NewProfileSelection)
		self.customCheck = IntVar()
		self.checkbutton = Checkbutton(self.frame, text = "Custom", variable = self.customCheck).grid(row=1, column=1, sticky = W, pady=(0, 2))
		self.customCheck.trace("w", self._CustomCheckEvent)
		#self.checkbutton.select()
		
		Label(self.frame, text="Profile:").grid(row=0, column=0, sticky = W, pady=(8, 4))
		self.prfStatus = StringVar()
		self.statusLbl = Label(self.frame, text="" ,textvariable=self.prfStatus).grid(row=0, column=1, sticky = W, pady=(8, 4))
		
		Label(self.frame, text="Capacity [mAh]:").grid(row=2, column=0, sticky = W)
		self.capacity = StringVar()
		self.capacity.trace("w", self._ProfileEdited)
		self.capacityEntry = Entry(self.frame,textvariable=self.capacity)
		self.capacityEntry.grid(row=2, column=1, sticky = W)
		Label(self.frame, text="Charge current [mA]:").grid(row=3, column=0, sticky = W)
		self.chgCurrent = StringVar()
		self.chgCurrent.trace("w", self._ProfileEdited)
		self.chgCurrentEntry = Entry(self.frame, textvariable=self.chgCurrent)
		self.chgCurrentEntry.grid(row=3, column=1, sticky = W)
		Label(self.frame, text="Termination current [mA]:").grid(row=4, column=0, sticky = W)
		self.termCurrent = StringVar()
		self.termCurrent.trace("w", self._ProfileEdited)
		self.termCurrentEntry = Entry(self.frame, textvariable=self.termCurrent)
		self.termCurrentEntry.grid(row=4, column=1, sticky = W)
		Label(self.frame, text="Regulation voltage [mV]:").grid(row=5, column=0, sticky = W)
		self.regVoltage = StringVar()
		self.regVoltage.trace("w", self._ProfileEdited)
		self.regVoltageEntry = Entry(self.frame,textvariable=self.regVoltage)
		self.regVoltageEntry.grid(row=5, column=1, sticky = W)
		Label(self.frame, text="Cutoff voltage [mV]:").grid(row=6, column=0, sticky = W)
		self.cutoffVoltage = StringVar()
		self.cutoffVoltage.trace("w", self._ProfileEdited)
		self.cutoffVoltageEntry = Entry(self.frame,textvariable=self.cutoffVoltage)
		self.cutoffVoltageEntry.grid(row=6, column=1, sticky = W)
		Label(self.frame, text="Cold temperature [C]:").grid(row=7, column=0, sticky = W)
		self.tempCold = StringVar()
		self.tempCold.trace("w", self._ProfileEdited)
		self.tempColdEntry = Entry(self.frame,textvariable=self.tempCold)
		self.tempColdEntry.grid(row=7, column=1, sticky = W)
		Label(self.frame, text="Cool temperature [C]:").grid(row=8, column=0, sticky = W)
		self.tempCool = StringVar()
		self.tempCool.trace("w", self._ProfileEdited)
		self.tempCoolEntry = Entry(self.frame,textvariable=self.tempCool)
		self.tempCoolEntry.grid(row=8, column=1, sticky = W)
		Label(self.frame, text="Warm temperature [C]:").grid(row=9, column=0, sticky = W)
		self.tempWarm = StringVar()
		self.tempWarm.trace("w", self._ProfileEdited)
		self.tempWarmEntry = Entry(self.frame,textvariable=self.tempWarm)
		self.tempWarmEntry.grid(row=9, column=1, sticky = W)
		Label(self.frame, text="Hot temperature [C]:").grid(row=10, column=0, sticky = W)
		self.tempHot = StringVar()
		self.tempHot.trace("w", self._ProfileEdited)
		self.tempHotEntry = Entry(self.frame,textvariable=self.tempHot)
		self.tempHotEntry.grid(row=10, column=1, sticky = W)
		Label(self.frame, text="NTC B constant [1k]:").grid(row=11, column=0, sticky = W)
		self.ntcB = StringVar()
		self.ntcB.trace("w", self._ProfileEdited)
		self.ntcBEntry = Entry(self.frame,textvariable=self.ntcB)
		self.ntcBEntry.grid(row=11, column=1, sticky = W)
		Label(self.frame, text="NTC resistance [ohm]:").grid(row=12, column=0, sticky = W)
		self.ntcResistance = StringVar()
		self.ntcResistance.trace("w", self._ProfileEdited)
		self.ntcResistanceEntry = Entry(self.frame,textvariable=self.ntcResistance)
		self.ntcResistanceEntry.grid(row=12, column=1, sticky = W)
		
		self.apply = StringVar()
		self.applyBtn = Button(self.frame, text='Apply', state="disabled", underline=0, command=lambda v=self.apply: self._ApplyNewProfile(v))
		self.applyBtn.grid(row=13, column=1, pady=(4,2), sticky = E)
		
		Label(self.frame, text="Temerature sense:").grid(row=2, column=2, padx=(5, 5), sticky = W)
		self.tempSense = StringVar()
		self.tempSenseSel = Combobox(self.frame, textvariable=self.tempSense, state='readonly')
		self.tempSenseSel['values'] = pijuice.config.batteryTempSenseOptions
		self.tempSenseSel.set('')
		self.tempSenseSel.grid(column=2, row=3, padx=(5, 5), pady=(0,2))
		self.tempSenseSel.bind("<<ComboboxSelected>>", self._NewTempSenseConfigSel)
		
		self.refreshConfig = StringVar()
		self.refreshConfigBtn = Button(self.frame, text='Refresh', underline=0, command=lambda v=self.refreshConfig: self.Refresh(v))
		self.refreshConfigBtn.grid(row=0, column=2, pady=(4,2), sticky = E)
		
		#self.closeConfig = StringVar()
		#self.closeBtn = Button(self.frame, text='Apply', state="disabled", underline=0, command=lambda v=self.closeConfig: self._Close(v))
		#self.closeBtn.grid(row=13, column=2, pady=(4,2), sticky = E)
		
		# position and set resize behaviour
		#lbl.grid(row=1, column=2, columnspan=2, sticky='new', pady=5)
		self.frame.rowconfigure(14, weight=1)
		self.frame.columnconfigure((0,2), weight=1, uniform=1)

		self.Refresh(self.refreshConfig)
		
    def Refresh(self, v):
		self.ReadProfileStatus()
		self.ReadProfileData()
		self._CustomEditEnable(False)
		self.applyBtn.configure(state="disabled")
		self.profileSel.configure(state='readonly')
		self.customCheck.set(0)
		
		tempSenseConfig = pijuice.config.GetBatteryTempSenseConfig()
		if tempSenseConfig['error'] == 'NO_ERROR':
			self.tempSenseSel.current(pijuice.config.batteryTempSenseOptions.index(tempSenseConfig['data']))
			
    def _NewProfileSelection(self, event):
		self._ClearProfileEditParams()
		self.applyBtn.configure(state="normal")
		
    def _ProfileEdited(self, *args):
		self.applyBtn.configure(state="normal")

    def _CustomCheckEvent(self, *args):
		self.ReadProfileStatus()
		self.ReadProfileData()
		self._CustomEditEnable(self.customCheck.get())
		if self.customCheck.get():
			#self.profileSel.set('')
			self.profileSel.configure(state='disabled')
		else:
			#self._ClearProfileEditParams()
			#self.ReadProfileStatus()
			self.profileSel.configure(state='readonly')
		self.applyBtn.configure(state="disabled")
		
    def _CustomEditEnable(self, en):
		newState = 'normal' if en == True else 'disabled'
		self.capacityEntry.configure(state=newState)
		self.chgCurrentEntry.configure(state=newState)
		self.termCurrentEntry.configure(state=newState)
		self.regVoltageEntry.configure(state=newState)
		self.cutoffVoltageEntry.configure(state=newState)
		self.tempColdEntry.configure(state=newState)
		self.tempCoolEntry.configure(state=newState)
		self.tempWarmEntry.configure(state=newState)
		self.tempHotEntry.configure(state=newState)
		self.ntcBEntry.configure(state=newState)
		self.ntcResistanceEntry.configure(state=newState)
		
    def _ClearProfileEditParams(self):
		self.capacity.set('')
		self.chgCurrent.set('')
		self.termCurrent.set('')
		self.regVoltage.set('')
		self.cutoffVoltage.set('')
		self.tempCold.set('')
		self.tempCool.set('')
		self.tempWarm.set('')
		self.tempHot.set('')
		self.ntcB.set('')
		self.ntcResistance.set('')
		
    def _ApplyNewProfile(self, v):
		if self.customCheck.get():
			self.WriteCustomProfileData()
		else:
			#print self.profileSel.get()
			status = pijuice.config.SetBatteryProfile(self.profileSel.get())
			if status['error'] == 'NO_ERROR':
				time.sleep(0.2)
				self.ReadProfileStatus()
				self.ReadProfileData()
				self.applyBtn.configure(state="disabled")
			#print status
			
    def _NewTempSenseConfigSel(self, event):
		status = pijuice.config.SetBatteryTempSenseConfig(self.tempSense.get())
		self.tempSenseSel.set('')
		if status['error'] == 'NO_ERROR':
			time.sleep(0.2)
			config = pijuice.config.GetBatteryTempSenseConfig()
			if config['error'] == 'NO_ERROR':
				self.tempSenseSel.current(pijuice.config.batteryTempSenseOptions.index(config['data']))
		
    def ReadProfileStatus(self):
		self.profileId = None
		#self.profileSel.configure(state="disabled")
		self.profileSel.set('')
		status = pijuice.config.GetBatteryProfileStatus()
		if status['error'] == 'NO_ERROR':
			self.status = status['data']
			if self.status['validity'] == 'VALID':
				if self.status['origin'] == 'PREDEFINED':
					#self.profileSel.configure(state="readonly")
					self.profileId = self.status['profile']
					self.profileSel.current(pijuice.config.batteryProfiles.index(self.profileId))
				else:
					self.profileSel.set('')
			else:
				self.profileSel.set('INVALID')
			if self.status['source'] == 'DIP_SWITCH' and self.status['origin'] == 'PREDEFINED' and pijuice.config.batteryProfiles.index(self.profileId) == 1:
				self.prfStatus.set('Default profile')
			else:
				if self.status['origin'] == 'CUSTOM':
					self.prfStatus.set('Custom profile by: ' + self.status['source'])
				else:
					self.prfStatus.set('Profile selected by: ' + self.status['source'])
		else:
			self.profileSel.set('')
			self._ClearProfileEditParams()
			self.prfStatus.set(status['error'])
			#print self.status
		
    def ReadProfileData(self):
		config = pijuice.config.GetBatteryProfile()
		if config['error'] == 'NO_ERROR':
			self.config = config['data'] 
			self.capacity.set(self.config['capacity'])
			self.chgCurrent.set(self.config['chargeCurrent'])
			self.termCurrent.set(self.config['terminationCurrent'])
			self.regVoltage.set(self.config['regulationVoltage'])
			self.cutoffVoltage.set(self.config['cutoffVoltage'])
			self.tempCold.set(self.config['tempCold'])
			self.tempCool.set(self.config['tempCool'])
			self.tempWarm.set(self.config['tempWarm'])
			self.tempHot.set(self.config['tempHot'])
			self.ntcB.set(self.config['ntcB'])
			self.ntcResistance.set(self.config['ntcResistance'])
			#print self.config
		else:
			self.applyBtn.configure(state="disabled")
			
    def WriteCustomProfileData(self):
		profile = {}
		profile['capacity'] = int(self.capacity.get())
		profile['chargeCurrent'] = int(self.chgCurrent.get())
		profile['terminationCurrent'] = int(self.termCurrent.get())
		profile['regulationVoltage'] = int(self.regVoltage.get())
		profile['cutoffVoltage'] = int(self.cutoffVoltage.get())
		profile['tempCold'] = int(self.tempCold.get())
		profile['tempCool'] = int(self.tempCool.get())
		profile['tempWarm'] = int(self.tempWarm.get())
		profile['tempHot'] = int(self.tempHot.get())
		profile['ntcB'] = int(self.ntcB.get())
		profile['ntcResistance'] = int(self.ntcResistance.get())
		status = pijuice.config.SetCustomBatteryProfile(profile)
		if status['error'] == 'NO_ERROR':
			time.sleep(0.2)
			self.ReadProfileData()
			self.applyBtn.configure(state="disabled")
		
class PiJuiceHATConfigGui():

    def __init__(self, isapp=True, name='pijuiceConfig'):
		#Frame.__init__(self, name=name)
		#self.grid(row=0, column=0)#self.pack(expand=Y, fill=BOTH)
		#self.master.maxsize(width=650, height=500)
		#self.master.title('PiJuice Advanced Configuration')
		#self.isapp = isapp
		
		t = Toplevel()
		t.wm_title('PiJuice HAT Configuration')
		
		# create the notebook
		nb = Notebook(t, name='notebook')

		# extend bindings to top level window allowing
		#   CTRL+TAB - cycles thru tabs
		#   SHIFT+CTRL+TAB - previous tab
		#   ALT+K - select tab using mnemonic (K = underlined letter)
		nb.enable_traversal()

		nb.grid(row=0, column=0)#nb.pack(fill=BOTH, expand=Y, padx=2, pady=3)
		
		self.hatConfig = PiJuiceHATConfig(nb)
		nb.add(self.hatConfig.frame, text='General', underline=0, padding=2)
		
		self.butonsConfig = PiJuiceButtonsConfig(nb)
		nb.add(self.butonsConfig.frame, text='Buttons', underline=0, padding=2)
		
		self.ledConfig = PiJuiceLedConfig(nb)
		nb.add(self.ledConfig.frame, text='LEDs', underline=0, padding=2)
		
		self.batConfig = PiJuiceBatteryConfig(nb)
		nb.add(self.batConfig.frame, text='Battery', underline=0, padding=2)
		
		self.firmware = PiJuiceFirmware(nb)
		nb.add(self.firmware.frame, text='Firmware', underline=0, padding=2)

class PiJuiceUserScriptConfig:
    def __init__(self, master):
		self.frame = Frame(master, name='userscript')
		self.frame.grid(row=0, column=0, sticky=W)
		self.frame.rowconfigure(10, weight=1)
		self.frame.columnconfigure((0,3), weight=1, uniform=1)
		
		global pijuiceConfigData
		self.pathEdits = []
		self.paths = []
		for i in range(0, 8):
			Label(self.frame, text='USER FUNC'+str(i+1)+':').grid(row=i, column=0, padx=(2, 2), pady=(4, 0), sticky = W)
			self.paths.append(StringVar())
			self.pathEdits.append(Entry(self.frame,textvariable=self.paths[i]))
			self.pathEdits[i].grid(row=i, column=1, padx=(2, 2), pady=(4, 0), columnspan = 3, sticky='WE')
			self.pathEdits[i].bind("<Return>", lambda x, id=i: self._UpdatePath(id))
			if ('user_functions' in pijuiceConfigData) and (('USER_FUNC'+str(i+1)) in pijuiceConfigData['user_functions']):
				self.paths[i].set(pijuiceConfigData['user_functions']['USER_FUNC'+str(i+1)])
			self.paths[i].trace("w", lambda name, index, mode, id = i: self._UpdatePath(id))
		
    def _UpdatePath(self, id):
		if not 'user_functions' in pijuiceConfigData:
			pijuiceConfigData['user_functions'] = {}
		pijuiceConfigData['user_functions']['USER_FUNC'+str(id+1)] = self.paths[id].get()
	
class PiJuiceWakeupConfig:
    def __init__(self, master):
		self.frame = Frame(master, name='wakeup')
		self.frame.grid(row=0, column=0, sticky=W)
		self.frame.rowconfigure(10, weight=1)
		self.frame.columnconfigure((0,3), weight=1, uniform=1)
		
		if pijuice == None:
			return
		
		Label(self.frame, text="UTC Time:").grid(row=0, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
		self.currTime = StringVar()
		self.currTimeLbl = Label(self.frame, textvariable=self.currTime, text="", font = "Verdana 10 bold").grid(row=1, column=0, padx=(2, 2), pady=(2, 0), columnspan = 2, sticky='W')
		
		self.setTime = StringVar()
		self.setTimeBtn = Button(self.frame, text='Set system time', underline=0, command=lambda v=self.setTime: self._SetTime(v))
		self.setTimeBtn.grid(row=1, column=2, padx=(10, 2), pady=(2,0), sticky = W)
		
		self.aDayOrWeekDay = IntVar()
		Radiobutton(self.frame, text="Day", variable=self.aDayOrWeekDay, value=1).grid(row=2, column=0, padx=(2, 2), pady=(10, 0), sticky='WE')
		Radiobutton(self.frame, text="Weekday", variable=self.aDayOrWeekDay, value=2).grid(row=2, column=1, padx=(2, 2), pady=(10, 0), sticky='WE')
			
		self.aDay = StringVar()
		self.aDayEntry = Entry(self.frame,textvariable=self.aDay)
		self.aDayEntry.grid(row=3, column=0, padx=(2, 2), pady=(2, 0), columnspan = 2, sticky='WE')
		
		self.aEveryDay = BooleanVar()
		self.aEveryDayCheck = Checkbutton(self.frame, text = "Every day", variable = self.aEveryDay)
		self.aEveryDayCheck.grid(row=3, column=2, padx=(5, 5), pady=(2, 0), sticky=W)
		
		Label(self.frame, text="Hour:").grid(row=4, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
		self.aHour = StringVar()
		#self.aDay.trace("w", self._ProfileEdited)
		self.aHourEntry = Entry(self.frame,textvariable=self.aHour)
		self.aHourEntry.grid(row=5, column=0, padx=(2, 2), pady=(2, 0), columnspan = 2, sticky='WE')
		
		self.aEveryHour = BooleanVar()
		self.aHourCheck = Checkbutton(self.frame, text = "Every hour", variable = self.aEveryHour)
		self.aHourCheck.grid(row=5, column=2, padx=(5, 5), pady=(2, 0), sticky=W)
		
		self.aMinuteOrPeriod = IntVar()
		Radiobutton(self.frame, text="Minute", variable=self.aMinuteOrPeriod, value=1).grid(row=6, column=0, padx=(2, 2), pady=(10, 0), sticky='WE')
		Radiobutton(self.frame, text="Minutes period", variable=self.aMinuteOrPeriod, value=2).grid(row=6, column=1, padx=(2, 2), pady=(10, 0), sticky='WE')
			
		self.aMinute = IntVar()
		#self.aDay.trace("w", self._ProfileEdited)
		self.aMinuteEntry = Entry(self.frame,textvariable=self.aMinute)
		self.aMinuteEntry.grid(row=7, column=0, padx=(2, 2), pady=(2, 0), columnspan = 2, sticky='WE')
		
		Label(self.frame, text="Second:").grid(row=8, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
		self.aSecond = IntVar()
		#self.aDay.trace("w", self._ProfileEdited)
		self.aSecondEntry = Entry(self.frame,textvariable=self.aSecond)
		self.aSecondEntry.grid(row=9, column=0, padx=(2, 2), pady=(2, 0), columnspan = 2, sticky='WE')
		
		self.setAlarm = StringVar()
		self.setAlarmBtn = Button(self.frame, text='Set Alarm', underline=0, command=lambda v=self.setAlarm: self._SetAlarm(v))
		self.setAlarmBtn.grid(row=10, column=0, padx=(2, 2), pady=(10,0), sticky = W)
		
		self.wakeupEnabled = BooleanVar()
		self.wakeupEnabledCheck = Checkbutton(self.frame, text = "Wakeup enabled", variable = self.wakeupEnabled)
		self.wakeupEnabledCheck.grid(row=10, column=1, padx=(2, 2), pady=(6, 0), sticky='W')
		
		self.status = StringVar()
		self.statusLbl = Label(self.frame, textvariable=self.status).grid(row=10, column=2, padx=(4, 2), pady=(6, 0), sticky = 'W')
		
		#print pijuice.rtcAlarm.SetAlarm({'second':10, 'minute':45, 'hour':'11PM', 'day':'2'})
		#print pijuice.rtcAlarm.SetAlarm({}) #disable alarm
		
		ctr = pijuice.rtcAlarm.GetControlStatus()
		if ctr['error'] == 'NO_ERROR':
			self.wakeupEnabled.set(ctr['data']['alarm_wakeup_enabled'])
		else:
			self.status.set(ctr['error'])
		
		a = pijuice.rtcAlarm.GetAlarm()
		if a['error'] == 'NO_ERROR':
			a = a['data']
			if 'day' in a:
				self.aDayOrWeekDay.set(1)
				if a['day'] == 'EVERY_DAY':
					self.aEveryDay.set(True)
				else:
					self.aDay.set(a['day'])
			elif 'weekday' in a:
				self.aDayOrWeekDay.set(2)
				if a['weekday'] == 'EVERY_DAY':
					self.aEveryDay.set(True)
				else:
					self.aDay.set(a['weekday'])
					
			if 'hour' in a:
				if a['hour'] == 'EVERY_HOUR':
					self.aEveryHour.set(True)
				else:
					self.aHour.set(a['hour'])
					
			if 'minute_period' in a:
				self.aMinuteOrPeriod.set(2)
				self.aMinute.set(a['minute_period'])
			elif 'minute' in a:
				self.aMinuteOrPeriod.set(1)
				self.aMinute.set(a['minute'])
					
			if 'second' in a:
				self.aSecond.set(a['second'])
				
		self.wakeupEnabled.trace("w", self._WakeupEnableChecked)
					 
		self._RefreshTime()
		
    def _RefreshTime(self):
		self.frame.after(1000, self._RefreshTime)
		t = pijuice.rtcAlarm.GetTime()
		if t['error'] == 'NO_ERROR':
			t = t['data']
		
			self.currTime.set(calendar.day_abbr[t['weekday']-1]+'  '+str(t['year'])+'-'+str(t['month']).rjust(2, '0')+'-'+str(t['day']).rjust(2, '0')+'  '+str(t['hour']).rjust(2, '0')+':'+str(t['minute']).rjust(2, '0')+':'+str(t['second']).rjust(2, '0')+'.'+str(t['subsecond']).rjust(2, '0'))
		s = pijuice.rtcAlarm.GetControlStatus()
		if s['error'] == 'NO_ERROR' and s['data']['alarm_flag']:
			pijuice.rtcAlarm.ClearAlarmFlag()
			self.status.set('Last: '+str(t['hour']).rjust(2, '0')+':'+str(t['minute']).rjust(2, '0')+':'+str(t['second']).rjust(2, '0'))
			
    def _SetTime(self, v):
		t = datetime.datetime.utcnow()
		print pijuice.rtcAlarm.SetTime({'second':t.second, 'minute':t.minute, 'hour':t.hour, 'weekday':t.weekday()+1, 'day':t.day,  'month':t.month, 'year':t.year, 'subsecond':t.microsecond/1000000})
		
    def _WakeupEnableChecked(self, *args):
		ret = pijuice.rtcAlarm.SetWakeupEnabled(self.wakeupEnabled.get())
		if ret['error'] != 'NO_ERROR':
			tkMessageBox.showerror('Alarm set', 'Failed to enable wakeup: ' + ret['error'], parent=self.frame)
			self.status.set(ret['error'])
			self.wakeupEnabled.set(not self.wakeupEnabled.get())
		else:
			self.status.set('')
			
    def _SetAlarm(self, v):
		a = {}
		a['second'] = self.aSecond.get()
		
		if self.aMinuteOrPeriod.get() == 2:
			a['minute_period'] = self.aMinute.get()
			print a['minute_period']
		elif self.aMinuteOrPeriod.get() == 1:
			a['minute'] = self.aMinute.get()
			
		if self.aEveryHour.get():
			a['hour'] = 'EVERY_HOUR'
		else:
			a['hour'] = self.aHour.get()
			
		if self.aDayOrWeekDay.get() == 1:
			if self.aEveryDay.get():
				a['day'] = 'EVERY_DAY'
			else:
				a['day'] = self.aDay.get()
		elif self.aDayOrWeekDay.get() == 2:
			if self.aEveryDay.get():
				a['weekday'] = 'EVERY_DAY'
			else:
				a['weekday'] = self.aDay.get()
				
		ret = pijuice.rtcAlarm.SetAlarm(a) 
		if ret['error'] != 'NO_ERROR':
			tkMessageBox.showerror('Alarm set', 'Reason: ' + ret['error'], parent=self.frame)
			self.status.set(ret['error'])
		else:
			self.status.set('')
			
		print pijuice.rtcAlarm.GetAlarm() 
		
class PiJuiceSysEventConfig:
    def __init__(self, master):
		self.frame = Frame(master, name='system_events')
		self.frame.grid(row=0, column=0, sticky=W)
		self.frame.rowconfigure(10, weight=1) 
		self.frame.columnconfigure((0,3), weight=1, uniform=1)
			
		self.eventFunctions = ['NO_FUNC'] + pijuice_sys_functions + pijuice_user_functions
		self.sysEvents = [{'id':'low_charge', 'name':'Low charge', 'funcList':self.eventFunctions}, 
		{'id':'low_battery_voltage', 'name':'Low battery voltage', 'funcList':self.eventFunctions}, 
		{'id':'watchdog_reset', 'name':'Watchdog reset', 'funcList':(['NO_FUNC']+pijuice_user_functions)}, 
		{'id':'button_power_off', 'name':'Button power off', 'funcList':(['NO_FUNC']+pijuice_user_functions)}, 
		{'id':'forced_power_off', 'name':'Forced power off', 'funcList':(['NO_FUNC']+pijuice_user_functions)}, 
		{'id':'forced_sys_power_off', 'name':'Forced sys power off', 'funcList':(['NO_FUNC']+pijuice_user_functions)}
		]
		global pijuiceConfigData
		self.sysEventEnable = [] 
		self.sysEventEnableCheck = []
		self.funcConfigsSel = []
		self.funcConfigs = []
		self.paramsEntry = []
		self.params = []
		self.oldParams = []
		for i in range(0, len(self.sysEvents)):
			self.sysEventEnable.append(BooleanVar())
			self.sysEventEnableCheck.append(Checkbutton(self.frame, text = self.sysEvents[i]['name']+":", variable = self.sysEventEnable[i]))
			self.sysEventEnableCheck[i].grid(row=i+1, column=0, sticky = W, padx=(5, 5))
			#self.params.append(StringVar())
			#self.paramsEntry.append(Entry(self.frame,textvariable=self.params[i]))
			#self.oldParams.append(StringVar())
			#self.paramsEntry[i].grid(row=1+i, column=1, sticky = W, padx=(2, 2), pady=(2, 0))
			#if 'param' in self.sysEvents[i]:
			#	unitText = (self.sysEvents[i]['param']['unit']+":")
			#else:
			#	self.paramsEntry[i].configure(state="disabled")
			#	unitText = ''
			#Label(self.frame, text=unitText).grid(row=1+i, column=2, padx=(5, 5), sticky = W)
			funcConfig = StringVar()
			self.funcConfigs.append(funcConfig)
			self.funcConfigsSel.append(Combobox(self.frame, textvariable=self.funcConfigs[i], state='readonly'))
			self.funcConfigsSel[i]['values'] = self.sysEvents[i]['funcList']#self.eventFunctions
			self.funcConfigsSel[i].current(0)
			self.funcConfigsSel[i].grid(column=1, row=1+i, padx=(5, 5), sticky = W)

			#self.params[i].set('')
			#self.oldParams[i].set('')
			if 'system_events' in pijuiceConfigData and self.sysEvents[i]['id'] in pijuiceConfigData['system_events']:
				if 'enabled' in pijuiceConfigData['system_events'][self.sysEvents[i]['id']]:
					if pijuiceConfigData['system_events'][self.sysEvents[i]['id']]['enabled'] != True:
						self.sysEventEnable[i].set(False)
						#self.paramsEntry[i].configure(state="disabled")
						self.funcConfigsSel[i].configure(state="disabled")
					else:
						self.sysEventEnable[i].set(True)
				else:
					self.sysEventEnable[i].set(False)
					#self.paramsEntry[i].configure(state="disabled")
					self.funcConfigsSel[i].configure(state="disabled")
				#if 'param' in pijuiceConfigData['system_events'][self.sysEvents[i]['id']]:
					#self.params[i].set(pijuiceConfigData['system_events'][self.sysEvents[i]['id']]['param'])
					#self.oldParams[i].set(self.params[i].get())
				if 'function' in pijuiceConfigData['system_events'][self.sysEvents[i]['id']]:
					self.funcConfigsSel[i].current(self.sysEvents[i]['funcList'].index(pijuiceConfigData['system_events'][self.sysEvents[i]['id']]['function']))
			else:
				self.sysEventEnable[i].set(False)
				#self.paramsEntry[i].configure(state="disabled")
				self.funcConfigsSel[i].configure(state="disabled")
			self.sysEventEnable[i].trace("w", lambda name, index, mode, var=self.sysEventEnable[i], id = i: self._SysEventEnableChecked(id))
			#self.params[i].trace("w",  lambda name, index, mode, var=self.params[i], id = i: self._ParamEdited(id))
			#self.paramsEntry[i].bind("<Return>", lambda x, id=i: self._WriteParam(id))
			self.funcConfigsSel[i].bind("<<ComboboxSelected>>", lambda event, idx=i: self._NewConfigSelected(event, idx))
			
    def _SysEventEnableChecked(self, i):
		if not ('system_events' in pijuiceConfigData):  
			pijuiceConfigData['system_events'] = {}
		if not (self.sysEvents[i]['id'] in pijuiceConfigData['system_events']):
			pijuiceConfigData['system_events'][self.sysEvents[i]['id']] = {}
		pijuiceConfigData['system_events'][self.sysEvents[i]['id']]['enabled'] = self.sysEventEnable[i].get()
		if self.sysEventEnable[i].get():
			self.funcConfigsSel[i].configure(state="readonly")
			#if 'param' in self.sysEvents[i]:
				#self.paramsEntry[i].configure(state="normal")
		else:
			#self.paramsEntry[i].configure(state="disabled")
			self.funcConfigsSel[i].configure(state="disabled")
		
    def _ParamEdited(self, i):
		if 'param' in self.sysEvents[i] and 'validate' in self.sysEvents[i]['param']:
			self.sysEvents[i]['param']['validate'](self.params[i], self.oldParams[i], self.sysEvents[i]['param']['min'], self.sysEvents[i]['param']['max'])

    def _WriteParam(self, i):
		if not ('system_events' in pijuiceConfigData):  
			pijuiceConfigData['system_events'] = {}
		if not (self.sysEvents[i]['id'] in pijuiceConfigData['system_events']):
			pijuiceConfigData['system_events'][self.sysEvents[i]['id']] = {}
		#if not ('param' in pijuiceConfigData['system_events'][self.sysEvents[i]['name']]):
		pijuiceConfigData['system_events'][self.sysEvents[i]['id']]['param'] = self.params[i].get()
		
    def _NewConfigSelected(self, event, i):
		if not ('system_events' in pijuiceConfigData):  
			pijuiceConfigData['system_events'] = {}
		if not (self.sysEvents[i]['id'] in pijuiceConfigData['system_events']):
			pijuiceConfigData['system_events'][self.sysEvents[i]['id']] = {}
		pijuiceConfigData['system_events'][self.sysEvents[i]['id']]['function'] = self.funcConfigsSel[i].get()

class PiJuiceConfigParamEdit:
    def __init__(self, master, r, config, name, paramDes, id, paramId, type, min, max): 
		self.frame = master
		self.config = config
		self.id = id
		self.paramId = paramId
		self.min = min
		self.max = max
		self.type = type
		#Label(self.frame, text="Watchdog").grid(row=1, column=0, padx=(2, 2), pady=(20, 0), sticky = W)
		Label(self.frame, text=paramDes).grid(row=r, column=1, padx=(2, 2), pady=(8, 0), sticky = W)
		self.paramEnable = IntVar()
		self.paramEnableCheck = Checkbutton(self.frame, text = name, variable = self.paramEnable).grid(row=r+1, column=0, sticky = W, padx=(2, 2), pady=(2, 0))
		self.param = StringVar()
		self.oldParamVal = StringVar()
		self.paramEntry = Entry(self.frame,textvariable=self.param)
		self.paramEntry.bind("<Return>", self._WriteParam)
		self.paramEntry.grid(row=r+1, column=1, sticky = W, padx=(2, 2), pady=(2, 0))
		
		if id in config:
			if paramId in config[id]:
				self.param.set(config[id][paramId])
				self.oldParamVal.set(config[id][paramId])
			else:
				self.param.set('')
				self.oldParamVal.set('')
			if ('enabled' in config[id]) and (config[id]['enabled'] == True):
				self.paramEnable.set(True)
			else:
				self.paramEntry.configure(state="disabled")
				self.paramEnable.set(False)
		else:
			self.param.set('')
			self.oldParamVal.set('')
			self.paramEntry.configure(state="disabled")
			self.paramEnable.set(False)
		
		self.paramEnable.trace("w", self._ParamEnableChecked)
		self.param.trace("w", self._ParamEdited)
		
    def _ParamEnableChecked(self, *args):
		if not (self.id in self.config):
			self.config[self.id] = {}
		if self.paramEnable.get():
			self.config[self.id]['enabled'] = True
			self.paramEntry.configure(state="normal")
		else: 
			self.config[self.id]['enabled'] = False
			self.paramEntry.configure(state="disabled")
		
    def _ParamEdited(self, *args):
		if type == 'int':
			_ValidateIntEntry(self.param, self.oldParamVal, self.min, self.max)
		elif type == 'float':
			_ValidateFloatEntry(self.param, self.oldParamVal, self.min, self.max)
		
    def _WriteParam(self, v):
		if not (self.id in self.config):
			self.config[self.id] = {}
		self.config[self.id][self.paramId] = self.param.get()
		
class PiJuiceSysTaskTab:
    def __init__(self, master): 
		self.frame = Frame(master, name='sys_task')
		self.frame.grid(row=0, column=0, sticky=W)
		self.frame.rowconfigure(10, weight=1)
		self.frame.columnconfigure((0,3), weight=1, uniform=1)
		
		if not ('system_task' in pijuiceConfigData):
			pijuiceConfigData['system_task'] = {}
		
		self.sysTaskEnable = IntVar()
		self.sysTaskEnableCheck = Checkbutton(self.frame, text = "System task enabled", variable = self.sysTaskEnable).grid(row=1, column=0, sticky = W, padx=(2, 2), pady=(20, 0))
		
		self.watchdogParam = PiJuiceConfigParamEdit(self.frame, 2, pijuiceConfigData['system_task'], "Watchdog", "Expire period [minutes]:", 'watchdog', 'period', 'int', 1, 65535)
		self.wakeupChargeParam = PiJuiceConfigParamEdit(self.frame, 4, pijuiceConfigData['system_task'], "Wakeup on charge", "Trigger level [%]:", 'wakeup_on_charge', 'trigger_level', 'int', 0, 100)
		self.minChargeParam = PiJuiceConfigParamEdit(self.frame, 6, pijuiceConfigData['system_task'], "Minimum charge", "Threshold [%]:", 'min_charge', 'threshold', 'int', 0, 100)
		self.minVoltageParam = PiJuiceConfigParamEdit(self.frame, 8, pijuiceConfigData['system_task'], "Minimum battery voltage", "Threshold [V]:", 'min_bat_voltage', 'threshold', 'float', 0, 10)
		
		if ('enabled' in pijuiceConfigData['system_task']) and (pijuiceConfigData['system_task']['enabled'] == True):
			self.sysTaskEnable.set(True)
		else:
			self.sysTaskEnable.set(False)
		
		self.sysTaskEnable.trace("w", self._SysTaskEnableChecked)
		
    def _SysTaskEnableChecked(self, *args):
		if not ('system_task' in pijuiceConfigData):
			pijuiceConfigData['system_task'] = {}
		if self.sysTaskEnable.get():
			pijuiceConfigData['system_task']['enabled'] = True
		else:
			pijuiceConfigData['system_task']['enabled'] = False

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
		
		Label(self.frame, text="Fault:").grid(row=3, column=0, padx=(2, 2), pady=(20, 0), sticky = W)
		self.fault = StringVar()
		self.faultLbl = Label(self.frame,textvariable=self.fault, text='')
		self.faultLbl.grid(row=3, column=1, padx=(2, 2), pady=(20, 0), columnspan=2, sticky = W)
		
		self.sysSwEnable = BooleanVar()
		self.sysSwEnableCheck = Checkbutton(self.frame, text = 'System switch state', variable = self.sysSwEnable).grid(row=4, column=0, sticky = W, padx=(2, 2), pady=(20, 0))
		self.sysSwLimit = StringVar()
		self.sysSwLimitEntry = Entry(self.frame,textvariable=self.sysSwLimit)
		#self.sysSwLimitEntry.bind("<Return>", self._SetSysSwitch)
		self.sysSwLimitEntry.grid(row=4, column=1, sticky = W, padx=(2, 2), pady=(20, 0))
		self.sysSwEnable.trace("w", self._SetSysSwitch)
		
		ret = pijuice.power.GetSystemPowerSwitch()
		if ret['error'] == 'NO_ERROR':
			self.sysSwEnable.set(int(ret['data']) > 0)
			self.sysSwLimit.set(ret['data'])
		else:
			self.sysSwLimit = ret['error']
		
		self.hatConfigBtn = Button(self.frame, text='Configure HAT', state="normal", underline=0, command= self._HatConfigCmd)
		self.hatConfigBtn.grid(row=9, column=0, padx=(2, 2), pady=(20, 0), sticky = W)
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
				
			volt = pijuice.status.GetBatteryVoltage()
			if volt['error'] == 'NO_ERROR':
				self.status.set(str(chg['data'])+'%, '+str(float(volt['data'])/1000)+'V, '+ret['data']['battery'])
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
		except:
			pass
		self.frame.after(6000, self._RefreshStatus)
		
    def _SetSysSwitch(self, *args):
		if self.sysSwEnable.get(): 
			pijuice.power.SetSystemPowerSwitch(self.sysSwLimit.get())
		else:
			pijuice.power.SetSystemPowerSwitch(0)
    def _HatConfigCmd(self):
		if pijuice != None:
			self.advWindow = PiJuiceHATConfigGui()
		
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
			print 'Failed to load ', PiJuiceConfigDataPath

		# create the notebook
		nb = Notebook(self, name='notebook')

		# extend bindings to top level window allowing
		#   CTRL+TAB - cycles thru tabs
		#   SHIFT+CTRL+TAB - previous tab
		#   ALT+K - select tab using mnemonic (K = underlined letter)
		nb.enable_traversal()

		nb.grid(row=0, column=0)#nb.pack(fill=BOTH, expand=Y, padx=2, pady=3)
		
		self.hatTab = PiJuiceHatTab(nb) 
		nb.add(self.hatTab.frame, text='HAT', underline=0, padding=2)
		#print 'PiJuiceWakeupConfig'
		self.wakeupTab = PiJuiceWakeupConfig(nb) 
		nb.add(self.wakeupTab.frame, text='Wakeup Alarm', underline=0, padding=2)
		#print 'PiJuiceSysTaskTab'
		self.sysTaskConfig = PiJuiceSysTaskTab(nb)
		nb.add(self.sysTaskConfig.frame, text='System Task', underline=0, padding=2)
		#print 'PiJuiceSysEventConfig'
		self.sysEventConfig = PiJuiceSysEventConfig(nb)
		nb.add(self.sysEventConfig.frame, text='System Events', underline=0, padding=2)
		#print 'PiJuiceUserScriptConfig'
		self.userScriptTab = PiJuiceUserScriptConfig(nb)
		nb.add(self.userScriptTab.frame, text='User Scripts', underline=0, padding=2)
		#print 'gui initialized'

def PiJuiceGuiOnclosing():
	#if tkMessageBox.askokcancel("Quit", "Do you want to quit?"):
	if not os.path.exists(os.path.dirname(PiJuiceConfigDataPath)):
		print os.path.dirname(PiJuiceConfigDataPath)
		os.makedirs(os.path.dirname(PiJuiceConfigDataPath)) 
	#try:
	with open(PiJuiceConfigDataPath , 'w+') as outputConfig:
		json.dump(pijuiceConfigData, outputConfig, indent=2)
	#except:
		#print 
	root.destroy() 
		
if __name__ == '__main__':

	root = Tk()
	
	root.protocol("WM_DELETE_WINDOW", PiJuiceGuiOnclosing)
	if pijuice == None:
		tkMessageBox.showerror('PuJuice Interfacing', 'Failed to use I2C bus. Check if I2C is enabled', parent=root)
	PiJuiceConfigGui().mainloop()