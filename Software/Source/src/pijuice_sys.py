#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function

import calendar
import datetime
import getopt
import json
import logging
import os
import signal
import subprocess
import sys
import time
import re

from pijuice import PiJuice

pijuice = None
btConfig = {}

configPath = '/var/lib/pijuice/pijuice_config.JSON'  # os.getcwd() + '/pijuice_config.JSON'
configData = {'system_task': {'enabled': False}}
status = {}
sysEvEn = False
minChgEn = False
minBatVolEn = False
lowChgEn = False
lowBatVolEn = False
chargeLevel = 50
noPowEn = False
noPowCnt = 100
logger = None
dopoll = True
PID_FILE = '/var/run/pijuice.pid'
HALT_FILE = '/tmp/pijuice_halt.flag'


def _SystemHalt(event):
    if (event in ('low_charge', 'low_battery_voltage', 'no_power')
        and configData.get('system_task', {}).get('wakeup_on_charge', {}).get('enabled', False)
        and 'trigger_level' in configData['system_task']['wakeup_on_charge']):

        try:
            tl = float(configData['system_task']['wakeup_on_charge']['trigger_level'])
            pijuice.power.SetWakeUpOnCharge(tl)
        except:
            tl = None
    pijuice.status.SetLedBlink('D2', 3, [150, 0, 0], 200, [0, 100, 0], 200)
    # Setting halt flag for 'pijuice_sys.py stop'
    with open(HALT_FILE, 'w') as f:
        pass
    subprocess.call(["sudo", "halt"])


def ExecuteFunc(func, event, param):
    if func == 'SYS_FUNC_HALT':
        #logger.info(func+' '+str(event)+' '+str(param))
        _SystemHalt(event)

    elif func == 'SYS_FUNC_HALT_POW_OFF':
        pijuice.power.SetSystemPowerSwitch(0)
        pijuice.power.SetPowerOff(60)
        _SystemHalt(event)
    elif func == 'SYS_FUNC_SYS_OFF_HALT':
        pijuice.power.SetSystemPowerSwitch(0)
        _SystemHalt(event)
    elif func == 'SYS_FUNC_REBOOT':
        subprocess.call(["sudo", "reboot"])
    elif ('USER_FUNC' in func) and ('user_functions' in configData) and (func in configData['user_functions']):
        try:
            os.system(
                "{function} {event} {param}".format(
                    function=configData['user_functions'][func],
                    event=str(event),
                    param=str(param)
                )
            )
            #subprocess.call([configData['user_functions'][func], event, param])
        except:
            print('failed to execute user func')


def _EvalButtonEvents():
    btEvents = pijuice.status.GetButtonEvents()
    if btEvents['error'] == 'NO_ERROR':
        for b in pijuice.config.buttons:
            ev = btEvents['data'][b]
            if ev != 'NO_EVENT':
                if btConfig[b][ev]['function'] != 'USER_EVENT':
                    pijuice.status.AcceptButtonEvent(b)
                    if btConfig[b][ev]['function'] != 'NO_FUNC':
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
                if lowChgEn:
                    # energy is low, take action
                    ExecuteFunc(configData['system_events']['low_charge']['function'],
                                'low_charge', level)

        chargeLevel = level
        return True
    else:
        return False


def _EvalBatVoltage():
    global lowBatVolEn
    bv = pijuice.status.GetBatteryVoltage()
    if bv['error'] == 'NO_ERROR':
        v = float(bv['data']) / 1000
        try:
            th = float(configData['system_task'].get('min_bat_voltage', {}).get('threshold'))
        except ValueError:
            th = None
        if th is not None and v < th:
            if lowBatVolEn:
                # Battery voltage below thresholdw, take action
                ExecuteFunc(configData['system_events']['low_battery_voltage']['function'], 'low_battery_voltage', v)

        return True
    else:
        return False


def _EvalPowerInputs(status):
    global noPowCnt
    NO_POWER_STATUSES = ['NOT_PRESENT', 'BAD']
    if status['powerInput'] in NO_POWER_STATUSES and status['powerInput5vIo'] in NO_POWER_STATUSES:
        noPowCnt = noPowCnt + 1
    else:
        noPowCnt = 0
    if noPowCnt == 2:
        # unplugged
        ExecuteFunc(configData['system_events']['no_power']['function'],
                    'no_power', '')


def _EvalFaultFlags():
    faults = pijuice.status.GetFaultStatus()
    if faults['error'] == 'NO_ERROR':
        faults = faults['data']
        for f in (pijuice.status.faultEvents + pijuice.status.faults):
            if f in faults:
                #logger.info('fault:' + f)
                if sysEvEn and (f in configData['system_events']) and ('enabled' in configData['system_events'][f]) and configData['system_events'][f]['enabled']:
                    if configData['system_events'][f]['function'] != 'USER_EVENT':
                        pijuice.status.ResetFaultFlags([f])
                        ExecuteFunc(configData['system_events'][f]['function'],
                                    f, faults[f])
        return True
    else:
        return False


def reload_settings(signum=None, frame=None):
    global pijuice
    global configData
    global btConfig

    with open(configPath, 'r') as outputConfig:
        config_dict = json.load(outputConfig)
        configData.update(config_dict)

    try:
        for b in pijuice.config.buttons:
            conf = pijuice.config.GetButtonConfiguration(b)
            if conf['error'] == 'NO_ERROR':
                btConfig[b] = conf['data']
    except:
        pass


def main():
    global pijuice
    global btConfig
    global configData
    global status
    global sysEvEn
    global chargeLevel
    global logger
    global minChgEn
    global minBatVolEn
    global lowChgEn
    global lowBatVolEn
    global noPowEn

    pid = str(os.getpid())
    with open(PID_FILE, 'w') as pid_f:
        pid_f.write(pid)

    if not os.path.exists(configPath):
        with open(configPath, 'w+') as conf_f:
            conf_f.write(json.dumps(configData))

    try:
        pijuice = PiJuice(1, 0x14)
    except:
        sys.exit(0)

    reload_settings()

    # Handle SIGHUP signal to reload settings
    signal.signal(signal.SIGHUP, reload_settings)

    if len(sys.argv) > 1 and str(sys.argv[1]) == 'stop':
        isHalting = False
        if os.path.exists(HALT_FILE):				# Created in _SystemHalt() called in main pijuice_sys process
            isHalting = True
            os.remove(HALT_FILE)
            
        try:
            if (('watchdog' in configData['system_task'])
                and ('enabled' in configData['system_task']['watchdog'])
                and configData['system_task']['watchdog']['enabled']
                ):
                # Disabling watchdog
                ret = pijuice.power.SetWatchdog(0)
                if ret['error'] != 'NO_ERROR':
                    time.sleep(0.05)
                    ret = pijuice.power.SetWatchdog(0)
        except:
            pass
        sysJobTargets = str(subprocess.check_output(["systemctl", "list-jobs"]))
        reboot = True if re.search('reboot.target.*start', sysJobTargets) is not None else False                      # reboot.target exists
        swStop = True if re.search('(?:halt|shutdown).target.*start', sysJobTargets) is not None else False           # shutdown | halt exists
        causePowerOff = True if (swStop and not reboot) else False
        ret = pijuice.status.GetStatus()
        if ( ret['error'] == 'NO_ERROR' 
        	and not isHalting			 					# Flag set in _SystemHalt()
        	and causePowerOff								# proper time to power down (!rebooting)
        	and configData.get('system_task',{}).get('ext_halt_power_off', {}).get('enabled',False)
        	):
        	# Set duration for when pijuice will cut power (Recommended 30+ sec, for halt to complete)
            try:
                powerOffDelay = int(configData['system_task']['ext_halt_power_off'].get('period',30))
                pijuice.power.SetPowerOff(powerOffDelay)
                print("PiJuice powering off in %s seconds" % configData['system_task']['ext_halt_power_off'].get('period',30))
            except ValueError:
                pass
        sys.exit(0)

    if (('watchdog' in configData['system_task'])
            and ('enabled' in configData['system_task']['watchdog'])
            and configData['system_task']['watchdog']['enabled']
            and ('period' in configData['system_task']['watchdog'])
        ):
        try:
            p = int(configData['system_task']['watchdog']['period'])
            ret = pijuice.power.SetWatchdog(p)
        except:
            p = None

    sysEvEn = 'system_events' in configData
    minChgEn = configData.get('system_task', {}).get('min_charge', {}).get('enabled', False)
    minBatVolEn = configData.get('system_task', {}).get('min_bat_voltage', {}).get('enabled', False)
    lowChgEn = sysEvEn and configData.get('system_events', {}).get('low_charge', {}).get('enabled', False)
    lowBatVolEn = sysEvEn and configData.get('system_events', {}).get('low_battery_voltage', {}).get('enabled', False)
    noPowEn = sysEvEn and configData.get('system_events', {}).get('no_power', {}).get('enabled', False)

    timeCnt = 5

    while dopoll:
        if configData.get('system_task', {}).get('enabled'):
            ret = pijuice.status.GetStatus()
            if ret['error'] == 'NO_ERROR':
                status = ret['data']
                if status['isButton']:
                    _EvalButtonEvents()

                timeCnt -= 1
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


if __name__ == '__main__':
    main()
