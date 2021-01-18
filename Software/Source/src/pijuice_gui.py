# This python script to be executed as user pijuice by the setuid program pijuice_gui
# Python 3 only
#
# -*- coding: utf-8 -*-

import calendar
import copy
import datetime
import json
import os
import re
import signal
from subprocess import Popen, PIPE
from threading import Thread
import fcntl
import sys
import time
from signal import SIGUSR1, SIGUSR2

from tkinter import Button as tkButton
from tkinter import (Tk, BooleanVar, DoubleVar, IntVar, StringVar, Toplevel,
                     N, W, S, E, X, Y, BOTH, RIGHT, HORIZONTAL, END)
from tkinter.ttk import (Button, Checkbutton, Combobox, Entry, Frame, Label,
                         Notebook, Progressbar, Radiobutton, Style)
from tkinter.colorchooser import askcolor
from tkinter.filedialog import askopenfilename
import tkinter.messagebox as MessageBox
from queue import Queue, Empty

from pijuice import (PiJuice, pijuice_hard_functions, pijuice_sys_functions,
                     pijuice_user_functions)

I2C_ADDRESS_DEFAULT = 0x14
I2C_BUS_DEFAULT = 1
PID_FILE = '/tmp/pijuice_sys.pid'
TRAY_PID_FILE = '/tmp/pijuice_tray.pid'
LOCK_FILE = '/tmp/pijuice_gui.lock'
USER_FUNCS_TOTAL = 15
USER_FUNCS_MINI = 8
pijuiceConfigData = {}
PiJuiceConfigDataPath = '/var/lib/pijuice/pijuice_config.JSON'
FWVER = 0xFF

pijuice = None

def _InitPiJuiceInterface():
    global pijuice
    global root
    try:
        addr = I2C_ADDRESS_DEFAULT
        bus = I2C_BUS_DEFAULT
        global pijuiceConfigData
        if 'board' in pijuiceConfigData and 'general' in pijuiceConfigData['board']:
            if 'i2c_addr' in pijuiceConfigData['board']['general']:
                addr = int(pijuiceConfigData['board']['general']['i2c_addr'], 16)
            if 'i2c_bus' in pijuiceConfigData['board']['general']:
                bus = pijuiceConfigData['board']['general']['i2c_bus']
        pijuice = PiJuice(bus, addr)
    except:
        pijuice = None
        root.withdraw()
        MessageBox.showerror('PiJuice Settings', 'Failed to use I2C bus. Check if I2C is enabled', parent=root)
        sys.exit()

def _ValidateIntEntry(var, min, max):
    new_value = var.get()
    try:
        if new_value != '':
            chg = int(new_value)
            if chg > max :
                new_value = str(max)
            elif chg < min :
                new_value = str(min)
        else:
            new_value = str(min)
    except:
        new_value = str(min)
    var.set(new_value)

def _ValidateFloatEntry(var, min, max):
    new_value = var.get()
    try:
        if new_value != '':
            chg = float(new_value)
            if chg > max :
                new_value = str(max)
            elif chg < min :
                new_value = str(min)
        else:
            new_value = str(min)
    except:
        new_value = str(min)
    var.set(new_value)

def _Iter_except(function, exception):
    """Works like builtin 2-argument `iter()`, but stops on `exception`."""
    try:
        while True:
            yield function()
    except exception:
        return

class PiJuiceFirmwareWait(object):
    # Tab to show only during firmware update
    def __init__(self, master):
        self.frame = Frame(master, name='fwwait')
        self.frame.grid(row=0, column=0, sticky=W)
        self.progressvalue=DoubleVar()

        l = Label(self.frame, text="Update in progress. Please wait", font='TkDefaultFont 22', foreground='red')
        l.place(relx=0.5, rely=0.25, anchor='center')

        self.progressbar = Progressbar(self.frame, style='green.Horizontal.TProgressbar', orient=HORIZONTAL, length=500, mode='determinate', variable=self.progressvalue)
        self.progressbar.place(relx=0.5, rely=0.5, anchor='center')

        self.waitrestart = Label(self.frame, text="Waiting for firmware restart ...", font='TkDefaultFont 18')
        self.waitrestart.place(relx=0.5, rely=0.75, anchor='center')
        self.waitrestart.pi = self.waitrestart.place_info()

class PiJuiceFirmware(object):
    def __init__(self, master, toplevelpath, waitwidgetclass):
        self.frame = Frame(master, name='firmware')
        self.frame.grid(row=0, column=0, sticky=W)
        self.frame.columnconfigure(0, weight=2)
        self.frame.columnconfigure(1, weight=1)
        self.nb = master
        self.toplevelpath = toplevelpath
        self.waitwidgetclass = waitwidgetclass

        self.firmwareFilePath = StringVar()

        Label(self.frame, text="Firmware version:").grid(row=0, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        self.firmVer = StringVar()
        self.firmVerLbl = Label(self.frame, textvariable=self.firmVer, text="").grid(row=0, column=1, padx=(2, 2), pady=(10, 0), sticky = W)
        self.newFirmStatus = StringVar()
        self.newFirmStatusLbl = Label(self.frame, textvariable=self.newFirmStatus, text="").grid(row=1, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        status = pijuice.config.GetFirmwareVersion()

        self.defaultConfigBtn = Button(self.frame, text='Update firmware',state="disabled", underline=0, command= self._UpdateFirmwareCmd)
        self.defaultConfigBtn.grid(row=1, column=1, padx=(2, 2), pady=(20, 0), sticky = W)

        Label(self.frame, text="Firmware file path:").grid(row=2, column=0, padx=2, pady=(10, 0), sticky=W)
        self.pathLabel = Label(self.frame, textvariable=self.firmwareFilePath, text="").grid(row=3, column=0, padx=2, pady=(10, 0), sticky=W)

        self.ver = None
        self.verStr = None
        if status['error'] == 'NO_ERROR':
            self.verStr = status['data']['version']
            major, minor = self.verStr.split('.')
            self.ver = (int(major) << 4) + int(minor)
            self.firmVer.set(self.verStr)
        else:
            self.firmVer.set(status['error'])

        from os import listdir
        from os.path import isfile, join

        self.newVer = None
        self.binFile = None
        binDir = '/usr/share/pijuice/data/firmware/'
        try:
            files = [f for f in listdir(binDir) if isfile(join(binDir, f))]
            files = sorted(files)
        except:
            files = []
        self.newVerStr = ''
        maxVer = 0
        self.firmwareFilePath.set("No firmware files found")

        regex = re.compile(r"PiJuice-V(\d+)\.(\d+)_(\d+_\d+_\d+).elf.binary")
        for f in files:
            # 1 - major, 2 - minor, 3 - date
            match = regex.match(f)
            if match:
                major_version = int(match.group(1))
                minor_version = int(match.group(2))
                verNum = (major_version << 4) + minor_version
                if verNum >= maxVer:
                    maxVer = verNum
                    self.newVerStr = "%i.%i" % (major_version, minor_version)
                    self.newVer = verNum
                    self.binFile = binDir + f
                    self.firmwareFilePath.set(self.binFile)

        if self.ver and self.newVer:
            if self.newVer > self.ver:
                self.newFirmStatus.set('New firmware V' + self.newVerStr + ' is available')
                self.defaultConfigBtn.configure(state="normal")
            else:
                self.newFirmStatus.set('Firmware is up to date')
        elif not self.ver:
            self.newFirmStatus.set('Firmware status unknown')
        else:
            self.newFirmStatus.set('Missing/wrong firmware file')

        self.firmUpdateErrors = ['NO_ERROR', 'I2C_BUS_ACCESS_ERROR', 'INPUT_FILE_OPEN_ERROR', 'STARTING_BOOTLOADER_ERROR', 'FIRST_PAGE_ERASE_ERROR',
        'EEPROM_ERASE_ERROR', 'INPUT_FILE_READ_ERROR', 'PAGE_WRITE_ERROR', 'PAGE_READ_ERROR', 'PAGE_VERIFY_ERROR', 'CODE_EXECUTE_ERROR']
    
    def _UpdateFirmwareCmd(self):
        global FWVER
        ret = pijuice.status.GetStatus()
        if ret['error'] == 'NO_ERROR':
            if ret['data']['powerInput'] != 'PRESENT' and ret['data']['powerInput5vIo'] != 'PRESENT':
                ret = pijuice.status.GetChargeLevel()
                if ret['error'] == 'NO_ERROR' and float(ret['data']) < 20:
                    MessageBox.showerror('Update Firmware','Cannot update, charge level is too low', parent=self.frame)
                    return

        q = MessageBox.askyesno('Update Firmware','Warning! Interrupting firmware update may lead to non functional PiJuice HAT. Continue?', icon=MessageBox.WARNING, parent=self.frame)
        if q:
            curAdr = pijuice.config.interface.GetAddress()
            if curAdr:

                # Display only the 'Please Wait' Tab during update
                self.nb.add(self.toplevelpath + '.notebook.fwwait')
                self.nb.select(self.toplevelpath + '.notebook.fwwait')
                self.nb.hide(self.toplevelpath + '.notebook.hat')
                self.nb.hide(self.toplevelpath + '.notebook.buttons')
                self.nb.hide(self.toplevelpath + '.notebook.led')
                self.nb.hide(self.toplevelpath + '.notebook.battery')
                self.nb.hide(self.toplevelpath + '.notebook.io')
                self.nb.hide(self.toplevelpath + '.notebook.firmware')

                # Hide the "Waiting for firmware restart" until upload finished
                self.waitwidgetclass.waitrestart.place_forget()
                self.nb.update()

                self.uploadFinished = False
                self.maxvar = 0
                adr = format(curAdr, 'x')
                # Redirect pijuiceboot output to a pipe. Read from the pipe
                # in a separate thread to not block the main gui thread.
                # The read thread puts the output in a queue.
                # The main gui thread empties the queue and updates the progressbar.
                self.process = Popen(['stdbuf', '-o0', 'pijuiceboot', adr, self.binFile], stdout=PIPE)
                q = Queue(maxsize=512)
                t = Thread(target = self.reader_thread, args = [q])
                t.daemon = True
                t.start()
                self.update(q)
                # Wait till upload finished while updating the progressbar
                while self.uploadFinished == False:
                    self.nb.update()
                ret = 256 - self.returnCode

                if ret == 256:
                    # Now show "Wait for firmware restart"
                    self.waitwidgetclass.waitrestart.place(self.waitwidgetclass.waitrestart.pi)
                    self.nb.update()
                    version = '0'
                    # Firmware has restarted if we can read the firmware version
                    while version == '0':
                        status = pijuice.config.GetFirmwareVersion()
                        if status['error'] == 'NO_ERROR':
                            version = status['data']['version']
                            major, minor = version.split('.')
                            FWVER = (int(major) << 4) + int(minor) 
                    # and hide the "Wait for firmware restart" again
                    self.waitwidgetclass.waitrestart.lower()

                # Restore tabs, make firmware tab current and hide 'Please Wait' tab
                self.nb.add(self.toplevelpath + '.notebook.hat')
                self.nb.add(self.toplevelpath + '.notebook.buttons')
                self.nb.add(self.toplevelpath + '.notebook.led')
                # Recreate battery config tab with possibly added/changed parameters
                self.nb.forget(self.toplevelpath + '.notebook.battery')
                self.batConfig = PiJuiceBatteryConfig(self.nb)
                self.nb.insert(3, self.batConfig.frame, text='Battery', underline=0, padding=2)
                # Recreate io config tab with possibly added/changed parameters
                self.nb.forget(self.toplevelpath + '.notebook.io')
                self.ioConfig = PiJuiceIoConfig(self.nb)
                self.nb.insert(4, self.ioConfig.frame, text='IO', underline=0, padding=2)
                self.nb.add(self.toplevelpath + '.notebook.firmware')
                self.nb.hide(self.toplevelpath + '.notebook.fwwait')
                self.nb.select(self.toplevelpath + '.notebook.firmware')

                if ret == 256:
                    self.newFirmStatus.set('Firmware is up to date')
                    self.firmVer.set(self.newVerStr)
                    self.defaultConfigBtn.configure(state="disabled")
                    MessageBox.showinfo('Firmware update', 'Update finished successfully!', parent=self.frame)
                else:
                    errorStatus = self.firmUpdateErrors[ret] if ret < 11 else '	UNKNOWN'
                    msg = ''
                    if errorStatus == 'I2C_BUS_ACCESS_ERROR':
                        msg = 'Check if I2C bus is enabled.'
                    elif errorStatus == 'INPUT_FILE_OPEN_ERROR':
                        msg = 'Firmware binary file might be missing or damaged.'
                    elif errorStatus == 'STARTING_BOOTLOADER_ERROR':
                        msg = 'Try to start bootloader manually. Press and hold button SW3 while powering up RPI and PiJuice.'
                    MessageBox.showerror('Firmware update failed', 'Reason: ' + errorStatus + '. ' + msg, parent=self.frame)
            else:
                MessageBox.showerror('Firmware update', 'Unknown PiJuice I2C address', parent=self.frame)

    def reader_thread(self, q):
        try:
            with self.process.stdout as pipe:
                for line in iter(pipe.readline, b''):
                    q.put(line)
        finally:
            q.put(None)

    def update(self, q):
        # Use pijuiceboot output to compute progressvalue (0..100)
        for line in _Iter_except(q.get_nowait, Empty):
            if line is None:
                self.uploadFinished = True
                self.returnCode = self.process.wait()
                return
            else:
                m = re.search('^page count (\d+)$', line.decode('utf-8'))
                if m:
                    self.maxvar = int(m.group(1))
                if self.maxvar > 0:
                    m = re.search('^Page (\d+) programmed successfully$', line.decode('utf-8'))
                    if m:
                        var = int(m.group(1))
                        progress = (self.maxvar - var) / self.maxvar * 100
                        self.waitwidgetclass.progressvalue.set(progress)
                break
        self.nb.after(10, self.update, q)

class PiJuiceHATConfig(object):
    def __init__(self, master):
        self.frame = Frame(master, name='hat')
        self.frame.grid(row=0, column=0, sticky=W)
        self.frame.columnconfigure(0, weight=0, minsize=200)
        self.frame.columnconfigure(3, weight=5, uniform=1)
        self.frame.rowconfigure(14, weight=1)

        Label(self.frame, text="Run pin:").grid(row=0, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        self.runPinConfig = StringVar()
        self.runPinConfigSel = Combobox(self.frame, textvariable=self.runPinConfig, state='readonly')
        self.runPinConfigSel['values'] = pijuice.config.runPinConfigs
        self.runPinConfigSel.grid(column=1, row=0, columnspan=3, padx=(2, 2), pady=(10, 0), sticky = W+E)
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
            self.slaveAddrEntry[i].grid(row=1+i, column=1, padx=(2, 2), pady=(10, 0), columnspan=3, sticky=W+E)
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
        self.idEepromAddrSel.grid(column=1, row=3, columnspan=3, padx=(2, 2), pady=(10, 0), sticky = W+E)
        self.idEepromAddrSel.bind("<<ComboboxSelected>>", self._IdEepromAddrSelected)
        config = pijuice.config.GetIdEepromAddress()
        if config['error'] != 'NO_ERROR':
            self.idEepromAddr.set(config['error'])
        else:
            self.idEepromAddrSel.current(pijuice.config.idEepromAddresses.index(config['data']))

        self.idEepromWpDisable = IntVar()
        Label(self.frame, text="ID EEPROM Write unprotect").grid(row=4, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        self.idEepromWpDisableCheck = Checkbutton(self.frame, variable = self.idEepromWpDisable).grid(row=4, column=1, sticky = W, padx=(2, 2), pady=(10, 0))
        self.idEepromWpDisable.trace("w", self._IdEepromWpDisableCheck)
        config = pijuice.config.GetIdEepromWriteProtect()
        if config['error'] != 'NO_ERROR':
            self.idEepromWpDisable.set(1)
        else:
            self.idEepromWpDisable.set(not config['data'])

        Label(self.frame, text="Inputs precedence:").grid(row=5, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        self.inputsPrecedence = StringVar()
        Radiobutton(self.frame, text=pijuice.config.powerInputs[0], variable=self.inputsPrecedence, value=pijuice.config.powerInputs[0]).grid(row=5, column=1, padx=(2, 2), pady=(10, 0), sticky= W)
        Radiobutton(self.frame, text=pijuice.config.powerInputs[1], variable=self.inputsPrecedence, value=pijuice.config.powerInputs[1]).grid(row=5, column=2, padx=(2, 2), pady=(10, 0), sticky= W)

        self.gpioInputEnabled = BooleanVar()
        Label(self.frame, text="GPIO Input Enabled").grid(row=7, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        self.gpioInputEnabledCheck = Checkbutton(self.frame, variable = self.gpioInputEnabled).grid(row=7, column=1, sticky = W, padx=(2, 2), pady=(10, 0))

        Label(self.frame, text="USB Micro current limit:").grid(row=8, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        self.usbMicroCurrentLimit = StringVar()
        Radiobutton(self.frame, text=pijuice.config.usbMicroCurrentLimits[0], variable=self.usbMicroCurrentLimit, value=pijuice.config.usbMicroCurrentLimits[0]).grid(row=8, column=1, padx=(2, 2), pady=(10, 0), sticky= W)
        Radiobutton(self.frame, text=pijuice.config.usbMicroCurrentLimits[1], variable=self.usbMicroCurrentLimit, value=pijuice.config.usbMicroCurrentLimits[1]).grid(row=8, column=2, padx=(2, 2), pady=(10, 0), sticky= W)

        Label(self.frame, text="USB Micro IN DPM:").grid(row=10, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        self.usbMicroInDpm = StringVar()
        self.usbMicroInDpmSel = Combobox(self.frame, textvariable=self.usbMicroInDpm, state='readonly')
        self.usbMicroInDpmSel['values'] = pijuice.config.usbMicroDPMs
        self.usbMicroInDpmSel.grid(column=1, row=10, columnspan=3, padx=(2, 2), pady=(10, 0), sticky = W+E)
        self.usbMicroInDpmSel.current(0)

        self.noBatTurnOnEnabled = BooleanVar()
        Label(self.frame, text="No battery turn on").grid(row=11, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        self.noBatTurnOnEnabledCheck = Checkbutton(self.frame, variable = self.noBatTurnOnEnabled).grid(row=11, column=1, sticky = W, padx=(2, 2), pady=(10, 0))

        ret = pijuice.config.GetPowerInputsConfig()
        if ret['error'] == 'NO_ERROR':
            powInCfg = ret['data']
            self.inputsPrecedence.set(powInCfg['precedence'])
            self.gpioInputEnabled.set(powInCfg['gpio_in_enabled'])
            self.noBatTurnOnEnabled.set(powInCfg['no_battery_turn_on'])
            self.usbMicroCurrentLimit.set(powInCfg['usb_micro_current_limit'])
            self.usbMicroInDpm.set(powInCfg['usb_micro_dpm'])

        self.usbMicroInDpmSel.bind("<<ComboboxSelected>>", self._UpdatePowerInputsConfig)
        self.usbMicroCurrentLimit.trace("w", self._UpdatePowerInputsConfig)
        self.gpioInputEnabled.trace("w", self._UpdatePowerInputsConfig)
        self.inputsPrecedence.trace("w", self._UpdatePowerInputsConfig)
        self.noBatTurnOnEnabled.trace("w", self._UpdatePowerInputsConfig)

        Label(self.frame, text="Power regulator mode:").grid(row=12, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        self.powerRegMode = StringVar()
        self.powerRegModeSel = Combobox(self.frame, textvariable=self.powerRegMode, state='readonly', width=len(max(pijuice.config.powerRegulatorModes, key=len)) + 1)
        self.powerRegModeSel['values'] = pijuice.config.powerRegulatorModes
        self.powerRegModeSel.grid(column=1, row=12, columnspan=3, padx=(2, 2), pady=(10, 0), sticky = W+E)
        self.powerRegModeSel.bind("<<ComboboxSelected>>", self._PowerRegModeSelected)
        config = pijuice.config.GetPowerRegulatorMode()
        if config['error'] != 'NO_ERROR':
            self.powerRegMode.set(config['error'])
        else:
            self.powerRegModeSel.current(pijuice.config.powerRegulatorModes.index(config['data']))

        self.chargingEnabled = BooleanVar()
        Label(self.frame, text="Charging Enabled").grid(row=13, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        self.chargingEnabledCheck = Checkbutton(self.frame, variable = self.chargingEnabled).grid(row=13, column=1, sticky = W, padx=(2, 2), pady=(10, 0))
        config = pijuice.config.GetChargingConfig()
        if config['error'] == 'NO_ERROR':
            self.chargingEnabled.set(config['data']['charging_enabled'])
        self.chargingEnabled.trace("w", self._UpdateChargingConfig)

        self.defaultConfigBtn = Button(self.frame, text='Reset to default configuration', state="normal", underline=0, command= self._ResetToDefaultConfigCmd)
        self.defaultConfigBtn.grid(row=14, column=0, padx=(2, 2), pady=(20, 0), sticky = S+W)

        Label(self.frame, text="Changes on this tab apply instantly.").grid(row=14, columnspan=3, column=1, sticky=S+E)

    def _ResetToDefaultConfigCmd(self):
        q = MessageBox.askokcancel('Reset Configuration','Warning! This action will reset PiJuice HAT configuration to default settings.', parent=self.frame)
        if q:
            status = pijuice.config.SetDefaultConfiguration()
            if status['error'] != 'NO_ERROR':
                MessageBox.showerror('Reset to default configuration', status['error'], parent=self.frame)

    def _WriteSlaveAddress(self, id):
        adr = int(self.slaveAddr[id].get(), 16)
        if adr < 8 or adr > 0x77:
            # Illegal 7-bit I2C address
            MessageBox.showerror('I2C Address', 'I2C address has to be\nbetween 0x08 and 0x77', parent=self.frame)
            # Restore original address
            self.slaveAddrEntry[id].delete(0,END)
            self.slaveAddrEntry[id].insert(END, self.slaveAddrconfig[id].get('data'))
            return

        q = MessageBox.askquestion('Address update','Are you sure you want to change address?', parent=self.frame)
        if q == 'yes':
            status = pijuice.config.SetAddress(id+1, self.slaveAddr[id].get())
            if status['error'] != 'NO_ERROR':
                #self.slaveAddr[id].set(status['error'])
                MessageBox.showerror('Address update', status['error'], parent=self.frame)
            else:
                # Update changed address
                MessageBox.showinfo('Address update', "Success!", parent=self.frame)
                self.slaveAddrconfig[id]['data'] = self.slaveAddr[id].get()
                pijuiceConfigData.setdefault('board', {}).setdefault('general', {})['i2c_addr'+['','_rtc'][id]] = self.slaveAddrconfig[id]['data']
                if (id==0): _InitPiJuiceInterface()
        else:
            # Restore original address
            self.slaveAddrEntry[id].delete(0,END)
            self.slaveAddrEntry[id].insert(END, self.slaveAddrconfig[id].get('data'))

    def _RunPinConfigSelected(self, event):
        status = pijuice.config.SetRunPinConfig(self.runPinConfig.get())
        if status['error'] != 'NO_ERROR':
            self.runPinConfig.set(status['error'])
            MessageBox.showerror('Run pin config', status['error'], parent=self.frame)

    def _IdEepromAddrSelected(self, event):
        status = pijuice.config.SetIdEepromAddress(self.idEepromAddr.get())
        if status['error'] != 'NO_ERROR':
            self.idEepromAddr.set(status['error'])
            MessageBox.showerror('ID EEPROM Address select', status['error'], parent=self.frame)

    def _IdEepromWpDisableCheck(self, *args):
        status = pijuice.config.SetIdEepromWriteProtect(not self.idEepromWpDisable.get())
        if status['error'] != 'NO_ERROR':
            self.idEepromWpDisable.set(not self.idEepromWpDisable.get())
            MessageBox.showerror('ID EEPROM write protect', status['error'], parent=self.frame)

    def _UpdatePowerInputsConfig(self, *args):
        config = {}
        config['precedence'] = self.inputsPrecedence.get()
        config['gpio_in_enabled'] = self.gpioInputEnabled.get()
        config['no_battery_turn_on'] = self.noBatTurnOnEnabled.get()
        config['usb_micro_current_limit'] = self.usbMicroCurrentLimit.get()
        config['usb_micro_dpm'] = self.usbMicroInDpm.get()
        status = pijuice.config.SetPowerInputsConfig(config, True)
        if status['error'] != 'NO_ERROR':
            MessageBox.showerror('Power Inputs Configuration', status['error'], parent=self.frame)

    def _PowerRegModeSelected(self, event):
        status = pijuice.config.SetPowerRegulatorMode(self.powerRegMode.get())
        if status['error'] != 'NO_ERROR':
            self.powerRegMode.set(status['error'])
            MessageBox.showerror('ID EEPROM Address select', status['error'], parent=self.frame)

    def _UpdateChargingConfig(self, *args):
        config = {'charging_enabled':self.chargingEnabled.get()}
        status = pijuice.config.SetChargingConfig(config, True)
        if status['error'] != 'NO_ERROR':
            MessageBox.showerror('Charging configuration', status['error'], parent=self.frame)

    def _ValidateSlaveAdr(self, var, id):
        new_value = var.get()
        try:
            if new_value != '':
                adr = int(str(new_value), 16)
                if adr > 0x7F:
                    var.set(self.oldAdr[id])
                else:
                    self.oldAdr[id] = new_value
            else:
                self.oldAdr[id] = new_value
        except:
            var.set(self.oldAdr[id])
        self.oldAdr[id] = new_value


class PiJuiceButtonsConfig(object):
    def __init__(self, master):
        self.frame = Frame(master, name='buttons')
        self.frame.grid(row=0, column=0, sticky=N+W+S+E)
        self.frame.columnconfigure(0, weight=0, minsize=120)
        self.frame.columnconfigure((1, 4), weight=1, uniform=1)

        apply_btn_row = (len(pijuice.config.buttonEvents)+1)*len(pijuice.config.buttons)

        self.frame.rowconfigure(apply_btn_row, weight=1)

        self.evFuncSelList = []
        self.evFuncList = []
        self.evParamEntryList = []
        self.evParamList = []
        self.configs = []
        self.errorState = False
        self.eventFunctions = ['NO_FUNC'] + copy.deepcopy(pijuice_hard_functions) + pijuice_sys_functions + pijuice_user_functions

        self.refreshConfigBtn = Button(self.frame, text='Refresh', underline=0, command=self.Refresh)
        self.refreshConfigBtn.grid(row=0, column=4, padx=2, pady=2, sticky=N+E)

        self.applyBtn = Button(self.frame, text='Apply', state="disabled", underline=0, command=self._ApplyNewConfig)
        self.applyBtn.grid(row=apply_btn_row, column=4, padx=2, pady=(5, 2), sticky=N+E)

        self.errorStatus = StringVar()
        self.errorStatusLbl = Label(self.frame, text='', textvariable=self.errorStatus).grid(row=apply_btn_row, column=0, padx=2, pady=2, sticky=N+W)

        combobox_length = len(max(self.eventFunctions, key=len)) + 1

        for i in range(0, len(pijuice.config.buttons)):
            Label(self.frame, text=pijuice.config.buttons[i]+":").grid(row=i*7, column=0, columnspan=2, padx=(5, 5),pady=(4, 0), sticky=W)
            Label(self.frame, text="Function:").grid(row=i*7, column=1, columnspan=2, padx=(2, 2),pady=(4, 0), sticky=W)
            Label(self.frame, text="Parameter:").grid(row=i*7, column=3, columnspan=2, padx=(2, 2),pady=(4, 0), sticky=W)

            for j in range(0, len(pijuice.config.buttonEvents)):
                r = i * (len(pijuice.config.buttonEvents) + 1) + j + 1
                ind = i * len(pijuice.config.buttonEvents) + j
                Label(self.frame, text=pijuice.config.buttonEvents[j]+":").grid(row=r, column=0, padx=(5, 5), sticky=W)

                func = StringVar()
                self.evFuncList.append(func)
                combo = Combobox(self.frame, textvariable=func, state='readonly', width=combobox_length)
                combo.grid(row=r, column=1, padx=(2, 2), columnspan=2, sticky=W+E)
                combo['values'] = self.eventFunctions
                self.evFuncSelList.append(combo)
                param = StringVar()
                self.evParamList.append(param)
                ent = Entry(self.frame,textvariable=self.evParamList[ind])
                ent.grid(row=r, column=3, padx=(2, 2), columnspan=2, sticky=W+E)
                self.evParamEntryList.append(ent)

                self.evFuncSelList[ind].bind("<<ComboboxSelected>>", self._ConfigEdited)
                self.evParamList[ind].trace("w", self._ConfigEdited)

            self.configs.append({})
            self.ReadConfig(i)

        self.applyBtn.configure(state="disabled")

    def _ApplyNewConfig(self):
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
                        notify_service()

        self.applyBtn.configure(state="disabled")

    def Refresh(self):
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
            # r = bind * (len(pijuice.config.buttonEvents) + 1) + j + 1
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

    def _ConfigEdited(self, *args):
        self.applyBtn.configure(state="normal")

    def _ConfigFuncSelected(self, event):
        self.applyBtn.configure(state="normal")


class PiJuiceLedConfig(object):
    def __init__(self, master):
        self.frame = Frame(master, name='led')
        self.frame.columnconfigure(0, minsize=100, weight=0)
        self.frame.columnconfigure(2, weight=1)

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
            ledConfigSel['values'] = pijuice.config.ledFunctionsOptions
            ledConfigSel.grid(column=1, row=i*5, padx=5, pady=(20, 0), sticky = W)
            self.ledConfigsSel.append(ledConfigSel)

            Label(self.frame, text=pijuice.config.leds[i]+" parameter:").grid(row=i*5+1, column=0, padx=5, pady=(10, 0), sticky = W)

            Label(self.frame, text="R:").grid(row=i*5+2, column=0, padx=5, pady=2, sticky = W)
            paramR = StringVar()
            self.paramList.append(paramR)
            ind = i * 3
            ent = Entry(self.frame,textvariable=self.paramList[ind])
            ent.grid(row=i*5+2, column=1, padx=(2, 2), pady=2, sticky='W')
            self.paramEntryList.append(ent)

            Label(self.frame, text="G:").grid(row=i*5+3, column=0, padx=5, pady=2, sticky = W)
            paramG = StringVar()
            self.paramList.append(paramG)
            ind = i * 3 + 1
            ent = Entry(self.frame,textvariable=self.paramList[ind])
            ent.grid(row=i*5+3, column=1, padx=(2, 2), pady=2, sticky='W')
            self.paramEntryList.append(ent)

            Label(self.frame, text="B:").grid(row=i*5+4, column=0, padx=5, pady=2, sticky = W)
            paramB = StringVar()
            self.paramList.append(paramB)
            ind = i * 3 + 2
            ent = Entry(self.frame,textvariable=self.paramList[ind])
            ent.grid(row=i*5+4, column=1, padx=(2, 2), pady=2, sticky='W')
            self.paramEntryList.append(ent)

            # Color picker button
            Button(self.frame, text='Choose color', command=lambda idx=i * 3: self._pickColor(idx)).grid(
                row=i*5+1, column=1, padx=2, pady=5, sticky=E)

            config = pijuice.config.GetLedConfiguration(pijuice.config.leds[i])
            self.configs.append({})
            if config['error'] == 'NO_ERROR':
                # XXX: Avoid setting ON_OFF_STATUS
                try:
                    self.ledConfigsSel[i].current(pijuice.config.ledFunctionsOptions.index(config['data']['function']))
                except ValueError:
                    self.ledConfigsSel[i].current(0)  # Set NOT_USED as default
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

            self.ledConfigsSel[i].bind("<<ComboboxSelected>>", self._NewConfigSelected)
            paramR.trace("w", self._ConfigEdited)
            paramG.trace("w", self._ConfigEdited)
            paramB.trace("w", self._ConfigEdited)

        self.applyBtn = Button(self.frame, text='Apply', state="disabled", underline=0, command=self._ApplyNewConfig)
        self.applyBtn.grid(row=10, column=2, padx=5, sticky=E)

    def _pickColor(self, idx):
        init_color = [param.get() for param in self.paramList[idx:idx + 3]]
        for i in range(3):
            try:
                init_color[i] = int(init_color[i])
            except ValueError:
                init_color[i] = 0
        init_color = "#%02x%02x%02x" % tuple(init_color)
        params, _ = askcolor(init_color, title="Color for D%i" % (idx // 3 + 1), parent=self.frame)
        if params:
            for i in range(3):
                # Converting to int implicitly because askcolor returns floats in Python3
                self.paramList[idx + i].set(str(int(params[i])))

    def _NewConfigSelected(self, *args):
        self.applyBtn.configure(state="normal")

    def _ConfigEdited(self, *args):
        self.applyBtn.configure(state="normal")

    def _ApplyNewConfig(self):
        for i in range(0, len(pijuice.config.leds)):
            ledConfig = {'function':self.ledConfigsSel[i].get(), 'parameter':{'r':self.paramList[i*3].get(), 'g':self.paramList[i*3+1].get(), 'b':self.paramList[i*3+2].get()}}
            # Validate values
            for color in ('r', 'g', 'b'):
                try:
                    if int(ledConfig['parameter'][color]) < 0:
                        ledConfig['parameter'][color] = '0'
                    if int(ledConfig['parameter'][color]) > 255:
                        ledConfig['parameter'][color] = '255'
                    self.paramList[i * 3 + ('r', 'g', 'b').index(color)].set(
                        str(ledConfig['parameter'][color]))
                except ValueError:
                    # If not int, set old value
                    ledConfig['parameter'][color] = self.configs[i]['parameter'][color]
                    self.paramList[i * 3 + ('r', 'g', 'b').index(color)].set(
                        str(ledConfig['parameter'][color]))

            if ( self.configs[i] == None
                or ledConfig['function'] != self.configs[i]['function']
                or ledConfig['parameter']['r'] != self.configs[i]['parameter']['r']
                or ledConfig['parameter']['g'] != self.configs[i]['parameter']['g']
                or ledConfig['parameter']['b'] != self.configs[i]['parameter']['b']
                ):
                status = pijuice.config.SetLedConfiguration(pijuice.config.leds[i], ledConfig)
                if status['error'] == 'NO_ERROR':
                    self.applyBtn.configure(state="disabled")
                else:
                    MessageBox.showerror('Apply LED Configuration', status['error'], parent=self.frame)

class PiJuiceBatteryConfig(object):
    def __init__(self, master):
        global FWVER
        self.frame = Frame(master, name='battery')
        self.frame.columnconfigure(0, weight=0, uniform=1)

        # Position and set resize behaviour
        self.frame.rowconfigure(21, weight=1)
        self.frame.columnconfigure((1,2), weight=1, uniform=1)

        # Widgets to be displayed on 'Description' tab
        self.profileId = StringVar()
        self.profileSel = Combobox(self.frame, textvariable=self.profileId, state='readonly')
        pijuice.config.SelectBatteryProfiles(FWVER)
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

        Label(self.frame, text="Profile:").grid(row=0, column=0, sticky = W, pady=(8, 4))
        self.prfStatus = StringVar()
        self.statusLbl = Label(self.frame, text="" ,textvariable=self.prfStatus).grid(row=0, column=1, sticky = W+E, pady=(8, 4))

        self.applyBtn = Button(self.frame, text='Apply', state="disabled", underline=0, command=self._ApplyNewProfile)
        self.applyBtn.grid(row=20, column=2, pady=(4,2), sticky = E)

        rownr = 1
        if FWVER >= 0x13:
            rownr = rownr + 1
            Label(self.frame, text="Chemistry:").grid(row=rownr, column=0, sticky = W)
            self.chemistry = StringVar()
            self.chemistry.trace("w", self._ProfileEdited)
            self.chemistrySel = Combobox(self.frame, textvariable=self.chemistry, state='readonly')
            self.chemistrySel['values'] = pijuice.config.batteryChemistries
            self.chemistrySel.set('')
            self.chemistrySel.grid(row=2, column=1, sticky = W+E)

        rownr = rownr + 1
        Label(self.frame, text="Capacity [mAh]:").grid(row=rownr, column=0, sticky = W)
        self.capacity = StringVar()
        self.capacity.trace("w", self._ProfileEdited)
        self.capacityEntry = Entry(self.frame,textvariable=self.capacity)
        self.capacityEntry.grid(row=rownr, column=1, sticky = W+E)

        rownr = rownr + 1
        Label(self.frame, text="Charge current [mA]:").grid(row=rownr, column=0, sticky = W)
        self.chgCurrent = StringVar()
        self.chgCurrent.trace("w", self._ProfileEdited)
        self.chgCurrentEntry = Entry(self.frame, textvariable=self.chgCurrent)
        self.chgCurrentEntry.grid(row=rownr, column=1, sticky = W+E)

        rownr = rownr + 1
        Label(self.frame, text="Termination current [mA]:").grid(row=rownr, column=0, sticky = W)
        self.termCurrent = StringVar()
        self.termCurrent.trace("w", self._ProfileEdited)
        self.termCurrentEntry = Entry(self.frame, textvariable=self.termCurrent)
        self.termCurrentEntry.grid(row=rownr, column=1, sticky = W+E)

        rownr = rownr + 1
        Label(self.frame, text="Regulation voltage [mV]:").grid(row=rownr, column=0, sticky = W)
        self.regVoltage = StringVar()
        self.regVoltage.trace("w", self._ProfileEdited)
        self.regVoltageEntry = Entry(self.frame,textvariable=self.regVoltage)
        self.regVoltageEntry.grid(row=rownr, column=1, sticky = W+E)

        rownr = rownr + 1
        Label(self.frame, text="Cutoff voltage [mV]:").grid(row=rownr, column=0, sticky = W)
        self.cutoffVoltage = StringVar()
        self.cutoffVoltage.trace("w", self._ProfileEdited)
        self.cutoffVoltageEntry = Entry(self.frame,textvariable=self.cutoffVoltage)
        self.cutoffVoltageEntry.grid(row=rownr, column=1, sticky = W+E)

        rownr = rownr + 1
        Label(self.frame, text="Cold temperature [C]:").grid(row=rownr, column=0, sticky = W)
        self.tempCold = StringVar()
        self.tempCold.trace("w", self._ProfileEdited)
        self.tempColdEntry = Entry(self.frame,textvariable=self.tempCold)
        self.tempColdEntry.grid(row=rownr, column=1, sticky = W+E)

        rownr = rownr + 1
        Label(self.frame, text="Cool temperature [C]:").grid(row=rownr, column=0, sticky = W)
        self.tempCool = StringVar()
        self.tempCool.trace("w", self._ProfileEdited)
        self.tempCoolEntry = Entry(self.frame,textvariable=self.tempCool)
        self.tempCoolEntry.grid(row=rownr, column=1, sticky = W+E)

        rownr = rownr + 1
        Label(self.frame, text="Warm temperature [C]:").grid(row=rownr, column=0, sticky = W)
        self.tempWarm = StringVar()
        self.tempWarm.trace("w", self._ProfileEdited)
        self.tempWarmEntry = Entry(self.frame,textvariable=self.tempWarm)
        self.tempWarmEntry.grid(row=rownr, column=1, sticky = W+E)

        rownr = rownr + 1
        Label(self.frame, text="Hot temperature [C]:").grid(row=rownr, column=0, sticky = W)
        self.tempHot = StringVar()
        self.tempHot.trace("w", self._ProfileEdited)
        self.tempHotEntry = Entry(self.frame,textvariable=self.tempHot)
        self.tempHotEntry.grid(row=rownr, column=1, sticky = W+E)

        rownr = rownr + 1
        Label(self.frame, text="NTC B constant [1k]:").grid(row=rownr, column=0, sticky = W)
        self.ntcB = StringVar()
        self.ntcB.trace("w", self._ProfileEdited)
        self.ntcBEntry = Entry(self.frame,textvariable=self.ntcB)
        self.ntcBEntry.grid(row=rownr, column=1, sticky = W+E)

        rownr = rownr + 1
        Label(self.frame, text="NTC resistance [ohm]:").grid(row=rownr, column=0, sticky = W)
        self.ntcResistance = StringVar()
        self.ntcResistance.trace("w", self._ProfileEdited)
        self.ntcResistanceEntry = Entry(self.frame,textvariable=self.ntcResistance)
        self.ntcResistanceEntry.grid(row=rownr, column=1, sticky = W+E)

        if FWVER >= 0x13:
            rownr = rownr + 1
            Label(self.frame, text="OCV10 [mV]:").grid(row=rownr, column=0, sticky = W)
            self.ocv10 = StringVar()
            self.ocv10.trace("w", self._ProfileEdited)
            self.ocv10Entry = Entry(self.frame,textvariable=self.ocv10)
            self.ocv10Entry.grid(row=rownr, column=1, sticky = W+E)

            rownr = rownr + 1
            Label(self.frame, text="OCV50 [mV]:").grid(row=rownr, column=0, sticky = W)
            self.ocv50 = StringVar()
            self.ocv50.trace("w", self._ProfileEdited)
            self.ocv50Entry = Entry(self.frame,textvariable=self.ocv50)
            self.ocv50Entry.grid(row=rownr, column=1, sticky = W+E)

            rownr = rownr + 1
            Label(self.frame, text="OCV90 [mV]:").grid(row=rownr, column=0, sticky = W)
            self.ocv90 = StringVar()
            self.ocv90.trace("w", self._ProfileEdited)
            self.ocv90Entry = Entry(self.frame,textvariable=self.ocv90)
            self.ocv90Entry.grid(row=rownr, column=1, sticky = W+E)

            rownr = rownr + 1
            Label(self.frame, text="R10 [mOhm]:").grid(row=rownr, column=0, sticky = W)
            self.r10 = StringVar()
            self.r10.trace("w", self._ProfileEdited)
            self.r10Entry = Entry(self.frame,textvariable=self.r10)
            self.r10Entry.grid(row=rownr, column=1, sticky = W+E)

            rownr = rownr + 1
            Label(self.frame, text="R50 [mOhm]:").grid(row=rownr, column=0, sticky = W)
            self.r50 = StringVar()
            self.r50.trace("w", self._ProfileEdited)
            self.r50Entry = Entry(self.frame,textvariable=self.r50)
            self.r50Entry.grid(row=rownr, column=1, sticky = W+E)

            rownr = rownr + 1
            Label(self.frame, text="R90 [mOhm]:").grid(row=rownr, column=0, sticky = W)
            self.r90 = StringVar()
            self.r90.trace("w", self._ProfileEdited)
            self.r90Entry = Entry(self.frame,textvariable=self.r90)
            self.r90Entry.grid(row=rownr, column=1, sticky = W+E)

        Label(self.frame, text="Temperature sense:").grid(row=2, column=2, padx=(5, 5), sticky = (W, E))
        self.tempSense = StringVar()
        self.tempSenseSel = Combobox(self.frame, textvariable=self.tempSense, state='readonly')
        self.tempSenseSel['values'] = pijuice.config.batteryTempSenseOptions
        self.tempSenseSel.set('')
        self.tempSenseSel.grid(column=2, row=3, padx=(5, 5), pady=(0,2), sticky = W)
        self.tempSenseSel.bind("<<ComboboxSelected>>", self._NewTempSenseConfigSel)

        if FWVER >= 0x13:
            Label(self.frame, text="RSoC estimation:").grid(row=5, column=2, padx=(5, 5), sticky = (W, E))
            self.rsocEst = StringVar()
            self.rsocEstSel = Combobox(self.frame, textvariable=self.rsocEst, state='readonly')
            self.rsocEstSel['values'] = pijuice.config.rsocEstimationOptions
            self.rsocEstSel.set('')
            self.rsocEstSel.grid(column=2, row=6, padx=(5, 5), pady=(0,2), sticky = W)
            self.rsocEstSel.bind("<<ComboboxSelected>>", self._NewRSocEstConfigSel)

        self.refreshConfigBtn = Button(self.frame, text='Refresh', underline=0, command=self.Refresh)
        self.refreshConfigBtn.grid(row=0, column=2, pady=(4,2), sticky = E)

        self.Refresh()

    def Refresh(self):
        global FWVER
        self.ReadProfileStatus()
        self.ReadProfileData()
        self._CustomEditEnable(False)
        self.applyBtn.configure(state="disabled")
        self.profileSel.configure(state='readonly')
        self.customCheck.set(0)

        tempSenseConfig = pijuice.config.GetBatteryTempSenseConfig()
        if tempSenseConfig['error'] == 'NO_ERROR':
            self.tempSenseSel.current(pijuice.config.batteryTempSenseOptions.index(tempSenseConfig['data']))
        if FWVER >= 0x13:
            ret = pijuice.config.GetRsocEstimationConfig()
            if ret['error'] == 'NO_ERROR':
                self.rsocEstSel.current(pijuice.config.rsocEstimationOptions.index(ret['data']))

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
            self.profileSel.configure(state='disabled')
        else:
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
        if FWVER >= 0x13:
            self.chemistrySel.configure(state=newState)
            self.ocv10Entry.configure(state=newState)
            self.ocv50Entry.configure(state=newState)
            self.ocv90Entry.configure(state=newState)
            self.r10Entry.configure(state=newState)
            self.r50Entry.configure(state=newState)
            self.r90Entry.configure(state=newState)

    def _ClearProfileEditParams(self):
        global FWVER
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
        if FWVER >= 0x13:
            self.chemistry.set('')
            self.ocv10.set('')
            self.ocv50.set('')
            self.ocv90.set('')
            self.r10.set('')
            self.r50.set('')
            self.r90.set('')

    def _ApplyNewProfile(self):
        if self.customCheck.get():
            self.WriteCustomProfileData()
        else:
            status = pijuice.config.SetBatteryProfile(self.profileSel.get())
            if status['error'] == 'NO_ERROR':
                time.sleep(0.2)
                self.ReadProfileStatus()
                self.ReadProfileData()
                self.applyBtn.configure(state="disabled")

    def _NewTempSenseConfigSel(self, *args):
        status = pijuice.config.SetBatteryTempSenseConfig(self.tempSense.get())
        self.tempSenseSel.set('')
        if status['error'] == 'NO_ERROR':
            time.sleep(0.2)
            config = pijuice.config.GetBatteryTempSenseConfig()
            if config['error'] == 'NO_ERROR':
                self.tempSenseSel.current(pijuice.config.batteryTempSenseOptions.index(config['data']))

    def _NewRSocEstConfigSel(self, *args):
        status = pijuice.config.SetRsocEstimationConfig(self.rsocEst.get())
        self.rsocEstSel.set('')
        if status['error'] == 'NO_ERROR':
            time.sleep(0.2)
            config = pijuice.config.GetRsocEstimationConfig()
            if config['error'] == 'NO_ERROR':
                self.rsocEstSel.current(pijuice.config.rsocEstimationOptions.index(config['data']))

    def ReadProfileStatus(self):
        self.profileId = None
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
                    self.profileSel.set('CUSTOM')
                else:
                    self.prfStatus.set('Profile selected by: ' + self.status['source'])
        else:
            self.profileSel.set('')
            self._ClearProfileEditParams()
            self.prfStatus.set(status['error'])

    def ReadProfileData(self):
        global FWVER
        config = pijuice.config.GetBatteryProfile()
        if config['error'] == 'NO_ERROR' and config['data'] != 'INVALID':
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
        else:
            self.applyBtn.configure(state="disabled")
        if FWVER >= 0x13:
            extconf = pijuice.config.GetBatteryExtProfile()
            if extconf['error'] == 'NO_ERROR':
                self.extconf = extconf['data']
                self.chemistry.set(self.extconf['chemistry'])
                self.ocv10.set(self.extconf['ocv10'])
                self.ocv50.set(self.extconf['ocv50'])
                self.ocv90.set(self.extconf['ocv90'])
                self.r10.set(self.extconf['r10'])
                self.r50.set(self.extconf['r50'])
                self.r90.set(self.extconf['r90'])
            else:
                self.applyBtn.configure(state="disabled")

    def WriteCustomProfileData(self):
        global FWVER
        profile = {}
        try:
            profile['capacity'] = int(self.capacity.get())
            chc = int(self.chgCurrent.get())
            if chc < 550:
                chc = 550
            elif chc > 2500:
                chc = 2500
            profile['chargeCurrent'] = chc
            tc = int(self.termCurrent.get())
            if tc < 50:
                tc = 50
            elif tc > 400:
                tc = 400
            profile['terminationCurrent'] = tc
            rv = int(self.regVoltage.get())
            if rv < 3500:
                rv = 3500
            elif rv > 4440:
                rv = 4440
            profile['regulationVoltage'] = rv
            profile['cutoffVoltage'] = int(self.cutoffVoltage.get())
            profile['tempCold'] = int(self.tempCold.get())
            profile['tempCool'] = int(self.tempCool.get())
            profile['tempWarm'] = int(self.tempWarm.get())
            profile['tempHot'] = int(self.tempHot.get())
            profile['ntcB'] = int(self.ntcB.get())
            profile['ntcResistance'] = int(self.ntcResistance.get())
        except:
            MessageBox.showerror('Error in custom profile', 'Parameters have to be numbers', parent=self.frame)
            return
        status = pijuice.config.SetCustomBatteryProfile(profile)
        if status['error'] != 'NO_ERROR':
            return

        if FWVER >= 0x13:
            extprf = {
                'chemistry': self.chemistry.get(),
                'ocv10': int(self.ocv10.get()),
                'ocv50': int(self.ocv50.get()),
                'ocv90': int(self.ocv90.get()), 
                'r10': float(self.r10.get()), 
                'r50': float(self.r50.get()),   
                'r90': float(self.r90.get())
            }
            time.sleep(0.2)
            status = pijuice.config.SetCustomBatteryExtProfile(extprf)
            if status['error'] == 'NO_ERROR':
                time.sleep(0.2)
                self.Refresh()
            else:
                MessageBox.showerror('Error writing custom profile', 'Reason: ' + status['error'], parent=self.frame)


class PiJuiceIoConfig(object):
    def __init__(self, master):
        global FWVER
        self.frame = Frame(master, name='io')
        self.frame.columnconfigure((1, 2), weight=1)

        self.config = [{}, {}]
        self.mode = [{}, {}]
        self.modeSel = [{}, {}]
        self.pull = [{}, {}]
        self.param1 = [{}, {}]
        self.paramName1 = [{}, {}]
        self.paramEntry1 = [{}, {}]
        self.paramName2 = [{}, {}]
        self.param2 = [{}, {}]
        self.paramEntry2 = [{}, {}]
        self.paramConfig1 =[None, None]
        self.paramConfig2 =[None, None]

        for i in range(0, 2):
            Label(self.frame, text="IO"+str(i+1)+":").grid(row=1+i*4, column=0, padx=(2, 2), pady=(2, 0), sticky = W)
            Label(self.frame, text="mode:").grid(row=0+i*4, column=1, padx=5, pady=(10, 0), sticky = W)
            self.mode[i] = StringVar()
            self.modeSel[i] = Combobox(self.frame, textvariable=self.mode[i], state='readonly')
            self.modeSel[i]['values'] = pijuice.config.ioSupportedModes[i+1]
            self.modeSel[i].grid(column=1, row=1+i*4, padx=5, pady=(2, 0), sticky = W+E)

            Label(self.frame, text="pull:").grid(row=0+i*4, column=2, padx=5, pady=(10, 0), sticky = W)
            self.pull[i] = StringVar()
            self.pullSel = Combobox(self.frame, textvariable=self.pull[i], state='readonly')
            self.pullSel.grid(column=2, row=1+i*4, padx=5, pady=(2, 0), sticky = W+E)
            self.pullSel['values'] = pijuice.config.ioPullOptions

            self.paramName1[i] = StringVar()
            self.paramNameLabel1 = Label(self.frame, textvariable=self.paramName1[i], text="param1:").grid(row=2+i*4, column=1, padx=(2, 2), pady=(5, 0), sticky = W)
            self.param1[i] = StringVar()
            self.paramEntry1[i] = Entry(self.frame,textvariable=self.param1[i])
            self.paramEntry1[i].grid(row=3+i*4, column=1, padx=5, pady=5, sticky=W+E)

            self.paramName2[i] = StringVar()
            self.paramNameLabel2 = Label(self.frame, textvariable=self.paramName2[i], text="param2:").grid(row=2+i*4, column=2, padx=(2, 2), pady=(5, 0), sticky = W)
            self.param2[i] = StringVar()
            self.paramEntry2[i] = Entry(self.frame,textvariable=self.param2[i])
            self.paramEntry2[i].grid(row=3+i*4, column=2, padx=5, pady=5, sticky=W+E)

            ret = pijuice.config.GetIoConfiguration(i+1)
            if ret['error'] != 'NO_ERROR':
                self.mode[i].set(ret['error'])
            else:
                self.config[i] = ret['data']
                self.mode[i].set(self.config[i]['mode'])
                self.pull[i].set(self.config[i]['pull'])

            self._ModeSelected(None, i)

            if self.paramConfig1[i]:
                if self.paramConfig1[i]['type'] != 'enum' or (i == 1 and FWVER >= 0x13):
                    self.param1[i].set(self.config[i][self.paramConfig1[i]['name']])
            if self.paramConfig2[i]:
                self.param2[i].set(self.config[i][self.paramConfig2[i]['name']])

            self.modeSel[i].bind("<<ComboboxSelected>>", lambda event, idx=i: self._ModeSelected(event, idx))
            self.param1[i].trace("w", lambda name, index, mode, idx=i: self._ParamEdited1(idx))
            self.param2[i].trace("w", lambda name, index, mode, idx=i: self._ParamEdited2(idx))

        self.applyBtn = Button(self.frame, text='Apply', state="normal", underline=0, command=self._ApplyNewConfig)
        self.applyBtn.grid(row=8, column=2, padx=(2, 2), pady=(20, 0), sticky=E)

    def _ModeSelected(self, event, i):
        global FWVER
        try:
            self.paramConfig1[i] = pijuice.config.ioConfigParams[self.mode[i].get()][0]
        except:
            self.paramConfig1[i] = None
        if self.paramConfig1[i]:
            if self.paramConfig1[i]['type'] == 'enum':
                # Wakeup option only on IO2 (i=1) for firmware >= 1.3
                if i == 0 or FWVER < 0x13:
                    self.param1[i].set('')
                    self.paramEntry1[i].configure(state="disabled")
                    self.paramName1[i].set('')
                else:
                    self.paramEntry1[i] = Combobox(self.frame, textvariable=self.param1[i], state='readonly')
                    self.paramEntry1[i]['values'] = self.paramConfig1[i]['options']
                    self.paramName1[i].set(self.paramConfig1[i]['name'])
                    if self.config[i]['mode'] == self.mode[i].get():
                        self.param1[i].set(self.config[i][self.paramConfig1[i]['name']])
                    else:
                        self.param1[i].set(self.paramConfig1[i]['options'][0])
            else:
                self.paramEntry1[i] = Entry(self.frame,textvariable=self.param1[i], state="normal")
                vname = self.paramConfig1[i]['name']
                vmin  = self.paramConfig1[i]['min']
                vmax  = self.paramConfig1[i]['max']
                vunit = self.paramConfig1[i]['unit'] if 'unit' in self.paramConfig1[i] else ''
                self.paramName1[i].set(vname +' [' + str(vmin) +'-' + str(vmax) + ((' ' + vunit) if vunit else '') +']:') 
                if self.config[i]['mode'] == self.mode[i].get():
                    self.param1[i].set(self.config[i][vname])
                else:
                    self.param1[i].set(vmin)
            self.paramEntry1[i].grid(row=3+i*4, column=1, padx=5, pady=5, sticky=W+E)
        else:
            self.paramEntry1[i].configure(state="disabled")
            self.paramName1[i].set('')
            self.param1[i].set('')

        try:
            self.paramConfig2[i] = pijuice.config.ioConfigParams[self.mode[i].get()][1]
        except:
            self.paramConfig2[i] = None
        if self.paramConfig2[i]:
            self.paramEntry2[i].configure(state="normal")
            vname = self.paramConfig2[i]['name']
            vmin  = self.paramConfig2[i]['min']
            vmax  = self.paramConfig2[i]['max']
            vunit = self.paramConfig2[i]['unit'] if 'unit' in self.paramConfig2[i] else ''
            self.paramName2[i].set(vname +' [' + str(vmin) +'-' + str(vmax) + ((' ' + vunit) if vunit else '') +']:')
            if self.config[i]['mode'] == self.mode[i].get():
                self.param2[i].set(self.config[i][vname])
            else:
                self.param2[i].set(vmin)
        else:
            self.paramEntry2[i].configure(state="disabled")
            self.paramName2[i].set('')
            self.param2[i].set('')

    def _ParamEdited1(self, i):
        if self.paramConfig1[i] and (self.paramConfig1[i]['type'] == 'int' or  self.paramConfig1[i]['type'] == 'float'):
            min = 0 if self.paramConfig1[i]['min'] < 0 else self.paramConfig1[i]['min']
            if self.paramConfig1[i]['type'] == 'int':
                _ValidateIntEntry(self.param1[i], min, self.paramConfig1[i]['max'])
            elif self.paramConfig1[i]['type'] == 'float':
                _ValidateFloatEntry(self.param1[i], min, self.paramConfig1[i]['max'])

    def _ParamEdited2(self, i):
        if self.paramConfig2[i] and (self.paramConfig2[i]['type'] == 'int' or  self.paramConfig2[i]['type'] == 'float'):
            min = 0 if self.paramConfig2[i]['min'] > 0 else self.paramConfig2[i]['min']
            if self.paramConfig2[i]['type'] == 'int':
                _ValidateIntEntry(self.param2[i], min, self.paramConfig2[i]['max'])
            elif self.paramConfig2[i]['type'] == 'float':
                _ValidateFloatEntry(self.param2[i], min, self.paramConfig2[i]['max'])

    def _ApplyNewConfig(self):
        for i in range(0, 2):
            newCfg = {
                'mode':self.mode[i].get(),
                'pull':self.pull[i].get()
                }
            if self.paramConfig1[i]:
                newCfg[self.paramConfig1[i]['name']] = self.param1[i].get()
            if self.paramConfig2[i]:
                newCfg[self.paramConfig2[i]['name']] = self.param2[i].get()

            ret = pijuice.config.SetIoConfiguration(i+1, newCfg, True)
            if ret['error'] != 'NO_ERROR':
                MessageBox.showerror('IO' + str(i+1) + ' Configuration', 'Reason: ' + ret['error'], parent=self.frame)
            else:
                self.config[i] = newCfg


class PiJuiceHATConfigGui(object):
    def __init__(self, cfg_hat_button):
        self.cfg_hat_button = cfg_hat_button
        self.cfg_hat_button.state(["disabled"])

        t = Toplevel()
        t.wm_title('PiJuice HAT Configuration')
        t.protocol("WM_DELETE_WINDOW", lambda x=self.cfg_hat_button, y=t: close_hat_config(x, y))

        # Create the notebook
        nb = Notebook(t, name='notebook')

        # Extend bindings to top level window allowing
        #   CTRL+TAB - cycles thru tabs
        #   SHIFT+CTRL+TAB - previous tab
        #   ALT+K - select tab using mnemonic (K = underlined letter)
        nb.enable_traversal()

        nb.pack(fill=BOTH, expand=Y, padx=2, pady=3)

        self.hatConfig = PiJuiceHATConfig(nb)
        nb.add(self.hatConfig.frame, text='General', underline=0, padding=2)

        self.butonsConfig = PiJuiceButtonsConfig(nb)
        nb.add(self.butonsConfig.frame, text='Buttons', underline=0, padding=2)

        self.ledConfig = PiJuiceLedConfig(nb)
        nb.add(self.ledConfig.frame, text='LEDs', underline=0, padding=2)

        self.batConfig = PiJuiceBatteryConfig(nb)
        nb.add(self.batConfig.frame, text='Battery', underline=0, padding=2)

        self.ioConfig = PiJuiceIoConfig(nb)
        nb.add(self.ioConfig.frame, text='IO', underline=0, padding=2)

        self.wait = PiJuiceFirmwareWait(nb)
        nb.add(self.wait.frame, text=' ... Wait ... ', underline=5, padding=2)
        nb.hide(self.wait.frame)

        self.firmware = PiJuiceFirmware(nb, str(t), self.wait)
        nb.add(self.firmware.frame, text='Firmware', underline=0, padding=2)

        t.update()
        t.minsize(t.winfo_width(), t.winfo_height())

def close_hat_config(button, toplevel):
    button.state(["!disabled"])
    toplevel.destroy()

class PiJuiceUserScriptConfig(object):
    def __init__(self, master):
        self.frame = Frame(master, name='userscript')
        self.frame.grid(row=0, column=0, sticky=W)
        self.frame.columnconfigure(1, weight=1, uniform=1)
        self.frame.columnconfigure(0, weight=0)

        global pijuiceConfigData
        self.pathEdits = []
        self.pathLabels = []
        self.paths = []
        self.browseButtons = []
        self.displayAll = False

        self.visibilityToggleButton = Button(self.frame, text='Show more', command=self._ToggleFuncsDisplay)

        for i in range(USER_FUNCS_TOTAL):
            self.pathLabels.append(Label(self.frame, text='USER FUNC'+str(i+1)+':'))
            self.paths.append(StringVar())
            self.pathEdits.append(Entry(self.frame,textvariable=self.paths[i]))
            self.pathEdits[i].bind("<Return>", lambda x, id=i: self._UpdatePath(id))
            if ('user_functions' in pijuiceConfigData) and (('USER_FUNC'+str(i+1)) in pijuiceConfigData['user_functions']):
                self.paths[i].set(pijuiceConfigData['user_functions']['USER_FUNC'+str(i+1)])
            self.paths[i].trace("w", lambda name, index, mode, id = i: self._UpdatePath(id))
            self.browseButtons.append(tkButton(self.frame, text="…", bd=0, command=lambda id=i: self._BrowseScript(id)))
        
        for i in range(USER_FUNCS_MINI):
            self.pathLabels[i].grid(row=i, column=0, padx=(2, 20), pady=(4, 0), sticky = W)
            self.pathEdits[i].grid(row=i, column=1, padx=2, pady=(4, 0), sticky='WE')
            self.browseButtons[i].grid(row=i, column=2, padx=2, pady=(4, 0), sticky='WE')
        
        self.visibilityToggleButton.grid(row=USER_FUNCS_MINI, column=0, padx=2, pady=2)

    def _ToggleFuncsDisplay(self):
        self.displayAll ^= True
        self.visibilityToggleButton.grid_forget()
        if self.displayAll:
            self.visibilityToggleButton.grid(row=USER_FUNCS_TOTAL, column=0, padx=2, pady=2)
            self.visibilityToggleButton.config(text='Show less')
            for i in range(USER_FUNCS_MINI, USER_FUNCS_TOTAL):
                self.pathLabels[i].grid(row=i, column=0, padx=(2, 20), pady=(4, 0), sticky = W)
                self.pathEdits[i].grid(row=i, column=1, padx=2, pady=(4, 0), sticky='WE')
                self.browseButtons[i].grid(row=i, column=2, padx=2, pady=(4, 0), sticky='WE')
        else:
            self.visibilityToggleButton.grid(row=USER_FUNCS_MINI, column=0, padx=2, pady=2)
            self.visibilityToggleButton.config(text='Show more')
            for i in range(USER_FUNCS_MINI, USER_FUNCS_TOTAL):
                self.pathLabels[i].grid_forget()
                self.pathEdits[i].grid_forget()
                self.browseButtons[i].grid_forget()
    
    def _BrowseScript(self, id):
        new_file = askopenfilename(parent=self.frame, title='Select script file for USER_FUNC ' + str(id+1))
        if new_file:
            self.paths[id].set(new_file)

    def _UpdatePath(self, id):
        if not 'user_functions' in pijuiceConfigData:
            pijuiceConfigData['user_functions'] = {}
        pijuiceConfigData['user_functions']['USER_FUNC'+str(id+1)] = self.paths[id].get()


class PiJuiceWakeupConfig(object):
    def __init__(self, master):
        self.frame = Frame(master, name='wakeup')
        self.frame.grid(row=0, column=0, sticky=W)
        self.frame.rowconfigure(10, weight=1)
        self.frame.columnconfigure((0,3), weight=1, uniform=1)
        self.frame.columnconfigure(0, minsize=100)

        if pijuice == None:
            return

        Label(self.frame, text="UTC Time:").grid(row=0, column=0, padx=(2, 2), pady=(10, 0), sticky = W)
        self.currTime = StringVar()
        self.currTimeLbl = Label(self.frame, textvariable=self.currTime, text="", font = "Verdana 10 bold").grid(row=1, column=0, padx=(2, 2), pady=(2, 0), columnspan = 2, sticky='W')

        self.setTime = StringVar()
        self.setTimeBtn = Button(self.frame, text='Set RTC time', underline=0, command=lambda v=self.setTime: self._SetTime(v))
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

            self.currTime.set(calendar.day_abbr[(t['weekday']+5) % 7]+'  '+str(t['year'])+'-'+str(t['month']).rjust(2, '0')+'-'+str(t['day']).rjust(2, '0')+'  '+str(t['hour']).rjust(2, '0')+':'+str(t['minute']).rjust(2, '0')+':'+str(t['second']).rjust(2, '0')+'.'+str(t['subsecond']).rjust(2, '0'))
        s = pijuice.rtcAlarm.GetControlStatus()
        if s['error'] == 'NO_ERROR' and s['data']['alarm_flag']:
            pijuice.rtcAlarm.ClearAlarmFlag()
            self.status.set('Last: '+str(t['hour']).rjust(2, '0')+':'+str(t['minute']).rjust(2, '0')+':'+str(t['second']).rjust(2, '0'))

    def _SetTime(self, v):
        t = datetime.datetime.utcnow()
        pijuice.rtcAlarm.SetTime({'second':t.second, 'minute':t.minute, 'hour':t.hour, 'weekday':(t.weekday()+1) % 7 + 1,
                                  'day':t.day, 'month':t.month, 'year':t.year, 'subsecond':t.microsecond//1000000})

    def _WakeupEnableChecked(self, *args):
        ret = pijuice.rtcAlarm.SetWakeupEnabled(self.wakeupEnabled.get())
        if ret['error'] != 'NO_ERROR':
            MessageBox.showerror('Alarm set', 'Failed to enable wakeup: ' + ret['error'], parent=self.frame)
            self.status.set(ret['error'])
            self.wakeupEnabled.set(not self.wakeupEnabled.get())
        else:
            self.status.set('')

    def _SetAlarm(self, v):
        a = {}
        a['second'] = self.aSecond.get()

        if self.aMinuteOrPeriod.get() == 2:
            a['minute_period'] = self.aMinute.get()
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
            MessageBox.showerror('Alarm set', 'Reason: ' + ret['error'], parent=self.frame)
            self.status.set(ret['error'])
        else:
            self.status.set('')



class PiJuiceSysEventConfig(object):
    def __init__(self, master):
        self.frame = Frame(master, name='system_events')
        self.frame.grid(row=0, column=0, sticky=W)
        self.frame.rowconfigure(10, weight=1)
        self.frame.columnconfigure(0, weight=0, minsize=175)
        self.frame.columnconfigure(1, weight=10, uniform=1)
        self.frame.columnconfigure(2, weight=1, uniform=1)

        self.eventFunctions = ['NO_FUNC'] + pijuice_sys_functions + pijuice_user_functions

        self.sysEvents = [{'id':'low_charge', 'name':'Low charge', 'funcList':self.eventFunctions},
        {'id':'low_battery_voltage', 'name':'Low battery voltage', 'funcList':self.eventFunctions},
        {'id':'no_power', 'name':'No power', 'funcList':self.eventFunctions},
        {'id':'watchdog_reset', 'name':'Watchdog reset', 'funcList':(['NO_FUNC']+pijuice_user_functions)},
        {'id':'button_power_off', 'name':'Button power off', 'funcList':(['NO_FUNC']+pijuice_user_functions)},
        {'id':'forced_power_off', 'name':'Forced power off', 'funcList':(['NO_FUNC']+pijuice_user_functions)},
        {'id':'forced_sys_power_off', 'name':'Forced sys power off', 'funcList':(['NO_FUNC']+pijuice_user_functions)},
        {'id':'sys_start', 'name':'System start', 'funcList':(['NO_FUNC']+pijuice_user_functions)},
        {'id':'sys_stop', 'name':'System stop', 'funcList':(['NO_FUNC']+pijuice_user_functions)}
        ]
        global pijuiceConfigData
        self.sysEventEnable = []
        self.sysEventEnableCheck = []
        self.funcConfigsSel = []
        self.funcConfigs = []
        self.paramsEntry = []
        self.params = []
        combobox_length = len(max(self.eventFunctions, key=len)) + 1
        for i in range(0, len(self.sysEvents)):
            self.sysEventEnable.append(BooleanVar())
            self.sysEventEnableCheck.append(Checkbutton(self.frame, text = self.sysEvents[i]['name']+":", variable = self.sysEventEnable[i]))
            self.sysEventEnableCheck[i].grid(row=i+1, column=0, sticky = W, padx=(5, 5))
            funcConfig = StringVar()
            self.funcConfigs.append(funcConfig)
            self.funcConfigsSel.append(Combobox(self.frame, textvariable=self.funcConfigs[i], state='readonly', width=combobox_length))
            self.funcConfigsSel[i]['values'] = self.sysEvents[i]['funcList']#self.eventFunctions
            self.funcConfigsSel[i].current(0)
            self.funcConfigsSel[i].grid(column=1, row=1+i, padx=(5, 5), pady=(0, 5), sticky = W+E)

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
                if 'function' in pijuiceConfigData['system_events'][self.sysEvents[i]['id']]:
                    self.funcConfigsSel[i].current(self.sysEvents[i]['funcList'].index(pijuiceConfigData['system_events'][self.sysEvents[i]['id']]['function']))
            else:
                self.sysEventEnable[i].set(False)
                self.funcConfigsSel[i].configure(state="disabled")
            self.sysEventEnable[i].trace("w", lambda name, index, mode, var=self.sysEventEnable[i], id = i: self._SysEventEnableChecked(id))
            self.funcConfigsSel[i].bind("<<ComboboxSelected>>", lambda event, idx=i: self._NewConfigSelected(event, idx))

    def _SysEventEnableChecked(self, i):
        if not ('system_events' in pijuiceConfigData):
            pijuiceConfigData['system_events'] = {}
        if not (self.sysEvents[i]['id'] in pijuiceConfigData['system_events']):
            pijuiceConfigData['system_events'][self.sysEvents[i]['id']] = {}
        pijuiceConfigData['system_events'][self.sysEvents[i]['id']]['enabled'] = self.sysEventEnable[i].get()
        if self.sysEventEnable[i].get():
            self.funcConfigsSel[i].configure(state="readonly")
        else:
            self.funcConfigsSel[i].configure(state="disabled")

    def _NewConfigSelected(self, event, i):
        if not ('system_events' in pijuiceConfigData):
            pijuiceConfigData['system_events'] = {}
        if not (self.sysEvents[i]['id'] in pijuiceConfigData['system_events']):
            pijuiceConfigData['system_events'][self.sysEvents[i]['id']] = {}
        pijuiceConfigData['system_events'][self.sysEvents[i]['id']]['function'] = self.funcConfigsSel[i].get()


class PiJuiceConfigParamEdit(object):
    def __init__(self, master, r, config, name, paramDes, id, paramId, type, min, max, restore = {}):
        self.frame = master
        self.config = config
        self.id = id
        self.paramId = paramId
        self.min = min
        self.max = max
        self.type = type
        self.restore = restore
        Label(self.frame, text=paramDes).grid(row=r, column=1, padx=(2, 2), pady=(8, 0), sticky = W)
        self.paramEnable = IntVar()
        self.paramEnableCheck = Checkbutton(self.frame, text = name, variable = self.paramEnable).grid(row=r+1, column=0, sticky = W, padx=(2, 2), pady=(2, 0))
        self.param = StringVar()
        self.paramEntry = Entry(self.frame,textvariable=self.param)
        self.paramEntry.bind("<Return>", self._WriteParam)
        self.paramEntry.grid(row=r+1, column=1, sticky = W+E, padx=(2, 2), pady=(2, 0))
        if restore:
            self.paramRestore = IntVar()
            self.paramRestoreCheck = Checkbutton(self.frame, text = "Restore", variable = self.paramRestore).grid(row=r+1, column=2, sticky = E, padx=(2, 2), pady=(2, 0))
            self.paramRestore.set(restore['init'])
            self.paramRestore.trace("w", self._ParamRestoreChecked)

        if id in config:
            if paramId in config[id]:
                self.param.set(config[id][paramId])
            else:
                self.param.set('')
            if ('enabled' in config[id]) and (config[id]['enabled'] == True):
                self.paramEnable.set(True)
            else:
                self.paramEntry.configure(state="disabled")
                self.paramEnable.set(False)
        else:
            self.param.set('')
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
            #if self.restore and self.restore['disable']: self.paramRestoreCheck.config(state=DISABLED)
        else:
            self.config[self.id]['enabled'] = False
            if self.restore and not self.restore['disable']:
                self.paramEntry.configure(state="normal")
            else:
                self.paramEntry.configure(state="disabled")
            #if self.restore and self.restore['disable']: self.paramRestoreCheck.config(state=DISABLED)

    def _ParamRestoreChecked(self, *args):
        if self.paramRestore.get()==1 and self.config[self.id]['enabled'] == False and self.restore['disable'] == True and self.param.get():
            self.paramRestore.set(0)
            MessageBox.showinfo('Restore', 'Setting Restore requires to enable '+self.id+'!', parent=self.frame)
        #else:
        #    self.restore["callback"](self.paramRestore.get(), self.param.get())

    def _ParamEdited(self, *args):
        if self.type == 'int':
            _ValidateIntEntry(self.param, self.min, self.max)
        elif self.type == 'float':
            _ValidateFloatEntry(self.param, self.min, self.max)

    def _WriteParam(self, v):
        if not (self.id in self.config):
            self.config[self.id] = {}
        self.config[self.id][self.paramId] = self.param.get()
        if self.restore: self.restore["callback"](self.paramRestore.get(), self.param.get(), self.paramEnable.get())


class PiJuiceSysTaskTab(object):
    def __init__(self, master):
        global FWVER
        self.frame = Frame(master, name='sys_task')
        self.frame.grid(row=0, column=0, sticky=W)
        self.frame.rowconfigure(12, weight=1)
        self.frame.columnconfigure(0, weight=0, minsize=190)
        self.frame.columnconfigure(1, weight=10, uniform=1)
        self.frame.columnconfigure(2, weight=1, minsize=80)

        if not ('system_task' in pijuiceConfigData):
            pijuiceConfigData['system_task'] = {}

        self.sysTaskEnable = IntVar()
        self.sysTaskEnableCheck = Checkbutton(self.frame, text = "System task enabled", variable = self.sysTaskEnable).grid(row=1, column=0, sticky = W, padx=(2, 2), pady=(20, 0))

        ret = pijuice.power.GetWatchdog()
        watchdogRestoreInit = False
        if ret['error'] == 'NO_ERROR': watchdogRestoreInit = ret['non_volatile']

        ret = pijuice.power.GetWakeUpOnCharge()
        wakeUpOnChargeRestoreInit = False
        if ret['error'] == 'NO_ERROR': wakeUpOnChargeRestoreInit = ret['non_volatile']

        self.watchdogParam = PiJuiceConfigParamEdit(self.frame, 2, pijuiceConfigData['system_task'], "Watchdog", "Expire period [minutes]:", 'watchdog', 'period', 'int', 1, 65535, restore = {"callback":self._SysTaskWatchdogRestoreChecked, "init":watchdogRestoreInit, "disable":True} if FWVER >= 0x15 else {})
        self.wakeupChargeParam = PiJuiceConfigParamEdit(self.frame, 4, pijuiceConfigData['system_task'], "Wakeup on charge", "Trigger level [%]:", 'wakeup_on_charge', 'trigger_level', 'int', 0, 100, restore = {"callback":self._SysTaskWakeupChRestoreChecked, "init":wakeUpOnChargeRestoreInit, "disable":False} if FWVER >= 0x15 else {})
        self.minChargeParam = PiJuiceConfigParamEdit(self.frame, 6, pijuiceConfigData['system_task'], "Minimum charge", "Threshold [%]:", 'min_charge', 'threshold', 'int', 0, 100)
        self.minVoltageParam = PiJuiceConfigParamEdit(self.frame, 8, pijuiceConfigData['system_task'], "Minimum battery voltage", "Threshold [V]:", 'min_bat_voltage', 'threshold', 'float', 0, 10)
        self.extHaltParam = PiJuiceConfigParamEdit(self.frame, 10, pijuiceConfigData['system_task'], "Software Halt Power Off", "Delay period [seconds]:", 'ext_halt_power_off', 'period', 'int', 20, 65535)

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

    def _SysTaskWakeupChRestoreChecked(self, state, param, enabled):
        if state:
            ret = pijuice.power.SetWakeUpOnCharge(param, True)
            #if ret['error'] != 'NO_ERROR':
            #    MessageBox.showerror('WakeupOnCharge Restore failed', ret['error'], parent=self.frame)
            #    return
            #ret = pijuice.power.GetWakeUpOnCharge()
            #if ret['error'] != 'NO_ERROR':
            #    MessageBox.showerror('WakeupOnCharge Restore failed', ret['error'], parent=self.frame)
            #    return
            #if ret['data'] != int(param) or ret['restore'] != True:
            #    MessageBox.showerror('WakeupOnCharge Restore failed', 'Try resetting this config.', parent=self.frame)
        else:
            ret = pijuice.power.SetWakeUpOnCharge('DISABLED', True)
            #if ret['error'] != 'NO_ERROR':
            #    MessageBox.showerror('WakeupOnCharge Restore failed', ret['error'], parent=self.frame)
            #    return
            #ret = pijuice.power.GetWakeUpOnCharge()
            #if ret['error'] != 'NO_ERROR':
            #    MessageBox.showerror('WakeupOnCharge Restore failed', ret['error'], parent=self.frame)
            #    return
            #if ret['data'] != 'DISABLED' or ret['restore'] != False:
            #    MessageBox.showerror('WakeupOnCharge Restore failed', 'Try resetting this config.', parent=self.frame)

    def _SysTaskWatchdogRestoreChecked(self, state, param, enabled):
        if state:
            if enabled:
                ret = pijuice.power.SetWatchdog(param, True)
                ret = pijuice.power.GetWatchdog()
            #if ret['error'] != 'NO_ERROR':
            #    MessageBox.showerror('Watchdog Restore failed', ret['error'], parent=self.frame)
            #    return
            #ret = pijuice.power.GetWatchdog()
            #if ret['error'] != 'NO_ERROR':
            #    MessageBox.showerror('Watchdog Restore failed', ret['error'], parent=self.frame)
            #    return
            #if ret['data'] != int(param) or ret['restore'] != True:
            #    MessageBox.showerror('Watchdog Restore failed', 'Try resetting this config.', parent=self.frame)
        else:
            ret = pijuice.power.SetWatchdog(0, True)
            #if ret['error'] != 'NO_ERROR':
            #    MessageBox.showerror('Watchdog Restore failed', ret['error'], parent=self.frame)
            #    return
            #ret = pijuice.power.GetWatchdog()
            #if ret['error'] != 'NO_ERROR':
            #    MessageBox.showerror('Watchdog Restore failed', ret['error'], parent=self.frame)
            #    return
            #if ret['data'] != 0 or ret['restore'] != False:
            #    MessageBox.showerror('Watchdog Restore failed', 'Try resetting this config.', parent=self.frame)

class PiJuiceHatTab(object):
    def __init__(self, master):
        self.frame = Frame(master, name='hat')
        self.frame.grid(row=0, column=0, sticky=W)
        self.frame.rowconfigure(10, weight=1)
        self.frame.columnconfigure(0, minsize=150)

        if pijuice == None:
            return

        Label(self.frame, text="Battery:").grid(row=0, column=0, padx=(2, 10), pady=(20, 0), sticky = W)
        self.status = StringVar()
        self.statusLbl = Label(self.frame,textvariable=self.status, text='')
        self.statusLbl.grid(row=0, column=1, padx=(2, 2), pady=(20, 0), columnspan=3, sticky = W)

        Label(self.frame, text="GPIO power input:").grid(row=1, column=0, padx=(2, 10), pady=(20, 0), sticky = W)
        self.gpioPower = StringVar()
        self.gpioPowerLbl = Label(self.frame,textvariable=self.gpioPower, text='')
        self.gpioPowerLbl.grid(row=1, column=1, padx=(2, 2), pady=(20, 0), columnspan=3, sticky = W)

        Label(self.frame, text="USB Micro power input:").grid(row=2, column=0, padx=(2, 10), pady=(20, 0), sticky = W)
        self.usbPower = StringVar()
        self.usbPowerLbl = Label(self.frame,textvariable=self.usbPower, text='')
        self.usbPowerLbl.grid(row=2, column=1, padx=(2, 2), pady=(20, 0), columnspan=3, sticky = W)

        Label(self.frame, text="Fault:").grid(row=3, column=0, padx=(2, 10), pady=(20, 0), sticky = W)
        self.fault = StringVar()
        self.faultLbl = Label(self.frame,textvariable=self.fault, text='')
        self.faultLbl.grid(row=3, column=1, padx=(2, 2), pady=(20, 0), columnspan=3, sticky = W)

        # Put the system switch buttons in their own frame so they do not move when column 1 changes width.
        self.sysSwFrame = Frame(self.frame)
        self.sysSwFrame.grid(row=4, column=0, columnspan=4, sticky = W+E)
        Label(self.sysSwFrame, text="System switch: ").grid(row=1, column=0, padx=(2, 55), pady=(20, 0), sticky = W)
        self.sysSwLimit = IntVar()
        Radiobutton(self.sysSwFrame, text="Off", variable=self.sysSwLimit, value=0).grid(row=1, column=1, padx=(2, 2), pady=(20, 0), sticky = W)
        Radiobutton(self.sysSwFrame, text="500mA", variable=self.sysSwLimit, value=500).grid(row=1, column=2, padx=(2, 2), pady=(20, 0), sticky = W+E)
        Radiobutton(self.sysSwFrame, text="2100mA", variable=self.sysSwLimit, value=2100).grid(row=1, column=3, padx=(2, 2), pady=(20, 0), sticky = W)
        self.sysSwLimit.trace("w", self._SetSysSwitch)

        self.hatConfigBtn = Button(self.frame, text='Configure HAT', state="normal", underline=0, command= self._HatConfigCmd)
        self.hatConfigBtn.grid(row=9, column=0, padx=(2, 2), pady=(20, 0), sticky = W)
        self.counter = 0
        self.frame.after(1000, self._RefreshStatus)

    def _RefreshStatus(self):
        try:
            ret = pijuice.status.GetStatus()
            if ret['error'] == 'NO_ERROR':
                self.usbPower.set(ret['data']['powerInput'])

            batstat = ''
            chg = pijuice.status.GetChargeLevel()
            if chg['error'] == 'NO_ERROR':
                batstat = str(chg['data'])+'%, '
                volt = pijuice.status.GetBatteryVoltage()
                if volt['error'] == 'NO_ERROR':
                    batstat = batstat + str(float(volt['data']) / 1000)+'V, '
                    temp = pijuice.status.GetBatteryTemperature()
                    if temp['error'] == 'NO_ERROR':
                        batstat = batstat + str(temp['data'])+'°C, ' + ret['data']['battery']
                        self.status.set(batstat)
                    else:
                        self.status.set(chg['error'])
                else:
                    self.status.set(volt['error'])
            else:
                self.status.set(chg['error'])
             
            curr = pijuice.status.GetIoCurrent()
            if curr['error'] == 'NO_ERROR':
                curr = str("{0:.1f}".format(float(curr['data']) / 1000)) + 'A, '
            else:
                curr = ''

            v5v = pijuice.status.GetIoVoltage()
            if v5v['error'] == 'NO_ERROR':
                self.gpioPower.set(str(float(v5v['data']) / 1000)+'V, ' + curr + ret['data']['powerInput5vIo'])
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
                self.sysSwLimit.set(ret['data'])
        except:
            pass
        #self.frame.after(6000, self._RefreshStatus)
        self.frame.after(6000, self._RefreshStatus)

    def _SetSysSwitch(self, *args):
        pijuice.power.SetSystemPowerSwitch(self.sysSwLimit.get())

    def _HatConfigCmd(self):
        if pijuice != None:
            self.advWindow = PiJuiceHATConfigGui(self.hatConfigBtn)


class PiJuiceConfigGui(Frame):
    def __init__(self, isapp=True, name='pijuiceConfig'):
        Frame.__init__(self, name=name)
        self.pack(expand=True, fill=BOTH)
        
        self.master.title('PiJuice Settings')
        self.isapp = isapp

        # Create the notebook
        nb = Notebook(self, name='notebook')

        # Extend bindings to top level window allowing
        #   CTRL+TAB - cycles thru tabs
        #   SHIFT+CTRL+TAB - previous tab
        #   ALT+K - select tab using mnemonic (K = underlined letter)
        nb.enable_traversal()

        nb.pack(fill=BOTH, expand=Y, padx=2, pady=3)

        self.hatTab = PiJuiceHatTab(nb)
        nb.add(self.hatTab.frame, text='HAT', underline=0, padding=2)

        self.wakeupTab = PiJuiceWakeupConfig(nb)
        nb.add(self.wakeupTab.frame, text='Wakeup Alarm', underline=0, padding=2)
        self.sysTaskConfig = PiJuiceSysTaskTab(nb)
        nb.add(self.sysTaskConfig.frame, text='System Task', underline=0, padding=2)
        self.sysEventConfig = PiJuiceSysEventConfig(nb)
        nb.add(self.sysEventConfig.frame, text='System Events', underline=0, padding=2)
        self.userScriptTab = PiJuiceUserScriptConfig(nb)
        nb.add(self.userScriptTab.frame, text='User Scripts', underline=0, padding=2)

        apply_button = Button(self, text="Apply")
        apply_button.event_add("<<ApplySettings>>", "<Button-1>", "<Return>", "<space>")  # XXX: Using separate actions can be unnecessary
        apply_button.bind("<<ApplySettings>>", self.apply_settings)
        apply_button.pack(side=RIGHT, padx=5, pady=5)

    def apply_settings(self, event=None):
        # Apply user scripts paths
        for i in range(0, 15):
            self.userScriptTab._UpdatePath(i)
        # Apply system task params
        for param in (self.sysTaskConfig.watchdogParam, self.sysTaskConfig.wakeupChargeParam, self.sysTaskConfig.minChargeParam, self.sysTaskConfig.minVoltageParam, self.sysTaskConfig.extHaltParam):
            param._WriteParam(None)
        save_config()


def save_config():
    if not os.path.exists(os.path.dirname(PiJuiceConfigDataPath)):
        os.makedirs(os.path.dirname(PiJuiceConfigDataPath))
    with open(PiJuiceConfigDataPath , 'w+') as outputConfig:
        json.dump(pijuiceConfigData, outputConfig, indent=2)
    notify_service()


def notify_service():
    global root
    try:
        pid = int(open(PID_FILE, 'r').read())
        os.system("sudo kill -SIGHUP " + str(pid))
    except:
        MessageBox.showerror('PiJuice Service', "Failed to communicate with PiJuice service.\n"
            "See system logs and 'systemctl status pijuice.service' for details.", parent=root)


def PiJuiceGuiOnclosing(gui):
    global pid, root

    gui.apply_settings()

    # Send signal to pijuice_tray to enable the 'Settings' menuitem again
    if pid != -1:
        # See comment on pid -1 in start_app() below
        try:
            os.system("sudo kill -SIGUSR2 " + str (pid))
        except:
            pass

    root.destroy()

def configure_style(style):
    DISABLED_BG_COLOR = 'gray80'
    DISABLED_FONT_COLOR = 'gray20'
    ENABLED_BG_COLOR = 'white'
    ENABLED_FONT_COLOR = 'black'
    style.map('TCombobox', fieldbackground=[('disabled', DISABLED_BG_COLOR), ('readonly', ENABLED_BG_COLOR)],
                            foreground=[('disabled', DISABLED_FONT_COLOR), ('readonly', ENABLED_FONT_COLOR)])

    style.map('TEntry', fieldbackground=[('disabled', DISABLED_BG_COLOR), ('readonly', ENABLED_BG_COLOR)],
                            foreground=[('disabled', DISABLED_FONT_COLOR), ('readonly', ENABLED_FONT_COLOR)])
    style.configure('green.Horizontal.TProgressbar', foreground='green', background='green')


def start_app():
    global root, pid, FWVER
    root = Tk()
    s = Style()
    theme_name = 'clam'
    if theme_name in s.theme_names():
        s.theme_use(theme_name)
        configure_style(s)

    # Acquire lock on lock file
    lock_file = open(LOCK_FILE, 'w')
    try:
        fcntl.flock(lock_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except IOError:
        root.withdraw()
        MessageBox.showerror('PiJuice Settings', 'Another instance of PiJuice Settings is already running')
        sys.exit()

    # Send signal to pijuice_tray to disable the 'Settings' menuitem
    try:
        with open(TRAY_PID_FILE, 'r') as f:
            pid = int(f.read())
    except:
        pid = -1
    if pid != -1:
        # Do not send SIGUSR1 to pid -1, this will kill all the processes of the user
        # and thus terminate the login session
        try:
            os.system("sudo kill -SIGUSR1 " + str(pid))
        except:
            pass
            
    global PiJuiceConfigDataPath
    global pijuiceConfigData

    try:
        with open(PiJuiceConfigDataPath, 'r') as outputConfig:
            pijuiceConfigData = json.load(outputConfig)
    except:
        pijuiceConfigData = {}
            
    global pijuice
    
    _InitPiJuiceInterface()
        
    status = pijuice.config.GetFirmwareVersion()
    if status['error'] == 'NO_ERROR':
        verStr = status['data']['version']
        major, minor = verStr.split('.')
        FWVER = (int(major) << 4) + int(minor)
    
    root.update()
    root.minsize(400, 400)
    gui = PiJuiceConfigGui()
    root.protocol("WM_DELETE_WINDOW", lambda x=gui: PiJuiceGuiOnclosing(gui))
    gui.mainloop()

if __name__ == '__main__':
    start_app()
