#!/usr/bin/python3
# -*- coding: utf-8 -*-
import grp
import json
import os
import pwd
import signal
import stat
import subprocess
import sys
import time
import re

from pijuice import PiJuice


class PiJuiceService:
    CONFIG_PATH = '/var/lib/pijuice/pijuice_config.JSON'  # os.getcwd() + '/pijuice_config.JSON'
    PID_FILE = '/tmp/pijuice_sys.pid'
    HALT_FILE = '/tmp/pijuice_halt.flag'
    NO_POWER_STATUSES = ['NOT_PRESENT', 'BAD']

    def __init__(self):
        self.pj = None
        self.button_config = {}
        self.config_data = {'system_task': {'enabled': False}}
        self.status = {}
        self.system_events_enabled = False
        self.min_charge_enabled = False
        self.min_battery_voltage_enabled = False
        self.low_charge_enabled = False
        self.low_battery_voltage_enabled = False
        self.charge_level = 50
        self.no_power_enabled = False
        self.no_power_count = 100
        self.do_poll = True
        self.poll_count = 5

    def _system_halt(self, event):
        if (event in ('low_charge', 'low_battery_voltage', 'no_power')
            and self.config_data.get('system_task', {}).get('wakeup_on_charge', {}).get('enabled', False)
                and 'trigger_level' in self.config_data['system_task']['wakeup_on_charge']):

            try:
                trigger_level = float(self.config_data['system_task']['wakeup_on_charge']['trigger_level'])
                self.pj.power.SetWakeUpOnCharge(trigger_level)
            except:
                pass
        self.pj.status.SetLedBlink('D2', 3, [150, 0, 0], 200, [0, 100, 0], 200)
        # Setting halt flag for 'pijuice_sys.py stop'
        with open(self.HALT_FILE, 'w') as f:
            pass
        subprocess.call(["sudo", "halt"])

    def execute_function(self, func, event, param):
        if func == 'SYS_FUNC_HALT':
            self._system_halt(event)
        elif func == 'SYS_FUNC_HALT_POW_OFF':
            self.pj.power.SetSystemPowerSwitch(0)
            self.pj.power.SetPowerOff(60)
            self._system_halt(event)
        elif func == 'SYS_FUNC_SYS_OFF_HALT':
            self.pj.power.SetSystemPowerSwitch(0)
            self._system_halt(event)
        elif func == 'SYS_FUNC_REBOOT':
            subprocess.call(["sudo", "reboot"])
        elif ('USER_FUNC' in func) and ('user_functions' in self.config_data) and \
                (func in self.config_data['user_functions']):
            function_ = self.config_data['user_functions'][func]
            # Check function is defined
            if function_ == "":
                return
            # Remove possible argumemts
            cmd = function_.split()[0]

            # Check cmd is an executable file and the file owner belongs
            # to the pijuice group.
            # If so, execute the command as the file owner

            try:
                statinfo = os.stat(cmd)
            except:
                # File not found
                return
            # Get owner and ownergroup names
            owner = pwd.getpwuid(statinfo.st_uid).pw_name
            ownergroup = grp.getgrgid(statinfo.st_gid).gr_name
            # Do not allow programs owned by root
            if owner == 'root':
                print("root owned {} not allowed".format(cmd))
                return
            # Check cmd has executable permission
            if statinfo.st_mode & stat.S_IXUSR == 0:
                print("{} is not executable".format(cmd))
                return
            # Owner of cmd must belong to mygroup ('pijuice')
            mygroup = grp.getgrgid(os.getegid()).gr_name
            # Find all groups owner belongs too
            groups = [g.gr_name for g in grp.getgrall() if owner in g.gr_mem]
            groups.append(ownergroup) # append primary group
            # Does owner belong to mygroup?
            found = 0
            for g in groups:
                if g == mygroup:
                    found = 1
                    break
            if found == 0:
                print("{} owner ('{}') does not belong to '{}'".format(cmd, owner, mygroup))
                return
            # All checks passed
            cmd = "sudo -u {} {} {} {}".format(owner, cmd, event, param)
            try:
                os.system(cmd)
            except:
                print('Failed to execute user func')

    def _eval_button_events(self):
        button_events = self.pj.status.GetButtonEvents()
        if button_events['error'] == 'NO_ERROR':
            for button in self.pj.config.buttons:
                event = button_events['data'][button]
                if event != 'NO_EVENT':
                    if self.button_config[button][event]['function'] != 'USER_EVENT':
                        self.pj.status.AcceptButtonEvent(button)
                        if self.button_config[button][event]['function'] != 'NO_FUNC':
                            self.execute_function(self.button_config[button][event]['function'], event, button)
            return True
        else:
            return False

    def _eval_charge(self):
        charge = self.pj.status.GetChargeLevel()
        if charge['error'] == 'NO_ERROR':
            level = float(charge['data'])
            if 'threshold' in self.config_data['system_task']['min_charge']:
                threshold = float(self.config_data['system_task']['min_charge']['threshold'])
                if level == 0 or ((level < threshold) and (0 <= (self.charge_level - level) < 3)):
                    if self.low_charge_enabled:
                        # energy is low, take action
                        self.execute_function(self.config_data['system_events']['low_charge']['function'],
                                    'low_charge', level)

            self.charge_level = level
            return True
        else:
            return False

    def _eval_battery_voltage(self):
        battery_voltage = self.pj.status.GetBatteryVoltage()
        if battery_voltage['error'] == 'NO_ERROR':
            voltage = float(battery_voltage['data']) / 1000
            try:
                threshold = float(self.config_data['system_task'].get('min_bat_voltage', {}).get('threshold'))
            except ValueError:
                threshold = None
            if threshold is not None and voltage < threshold:
                if self.low_battery_voltage_enabled:
                    # Battery voltage below threshold, take action
                    self.execute_function(self.config_data['system_events']['low_battery_voltage']['function'],
                                          'low_battery_voltage', voltage)
            return True
        else:
            return False

    def _eval_power_inputs(self):
        if self.status['powerInput'] in self.NO_POWER_STATUSES and \
                self.status['powerInput5vIo'] in self.NO_POWER_STATUSES:
            self.no_power_count += 1
        else:
            self.no_power_count = 0

        if self.no_power_count == 2:
            # unplugged
            self.execute_function(self.config_data['system_events']['no_power']['function'], 'no_power', '')

    def _eval_fault_flags(self):
        faults = self.pj.status.GetFaultStatus()
        if faults['error'] == 'NO_ERROR':
            faults = faults['data']
            for fault in (self.pj.status.faultEvents + self.pj.status.faults):
                if fault in faults:
                    if self.system_events_enabled and \
                            (fault in self.config_data['system_events']) and \
                            ('enabled' in self.config_data['system_events'][fault]) and \
                            self.config_data['system_events'][fault]['enabled']:
                        if self.config_data['system_events'][fault]['function'] != 'USER_EVENT':
                            self.pj.status.ResetFaultFlags([fault])
                            self.execute_function(self.config_data['system_events'][fault]['function'],
                                                  fault, faults[fault])
            return True
        else:
            return False

    def reload_settings(self, signum=None, frame=None):
        with open(self.CONFIG_PATH, 'r') as output_config:
            config_dict = json.load(output_config)
            self.config_data.update(config_dict)

        try:
            for button in self.pj.config.buttons:
                conf = self.pj.config.GetButtonConfiguration(button)
                if conf['error'] == 'NO_ERROR':
                    self.button_config[button] = conf['data']
        except:
            pass

        self.system_events_enabled = 'system_events' in self.config_data
        self.min_charge_enabled = self.config_data.get('system_task', {}).get('min_charge', {}).get('enabled', False)
        self.min_battery_voltage_enabled = self.config_data.get('system_task', {}).get('min_bat_voltage', {}).\
            get('enabled', False)
        self.low_charge_enabled = self.system_events_enabled and \
                                  self.config_data.get('system_events', {}).get('low_charge', {}).get('enabled', False)
        self.low_battery_voltage_enabled = self.system_events_enabled and \
                                           self.config_data.get('system_events', {}).get('low_battery_voltage', {}).\
                                               get('enabled', False)
        self.no_power_enabled = self.system_events_enabled and \
                                self.config_data.get('system_events', {}).get('no_power', {}).get('enabled', False)

        if ('watchdog' in self.config_data['system_task']) \
                and ('enabled' in self.config_data['system_task']['watchdog']) \
                and self.config_data['system_task']['watchdog']['enabled'] \
                and ('period' in self.config_data['system_task']['watchdog']):
            try:
                p = int(self.config_data['system_task']['watchdog']['period'])
                self.pj.power.SetWatchdog(p)
            except:
                pass

    def poll(self):
        if self.config_data.get('system_task', {}).get('enabled'):
            ret = self.pj.status.GetStatus()
            if ret['error'] == 'NO_ERROR':
                status = ret['data']
                if status['isButton']:
                    self._eval_button_events()

                self.poll_count -= 1
                if self.poll_count == 0:
                    self.poll_count = 5
                    if ('isFault' in status) and status['isFault']:
                        self._eval_fault_flags()
                    if self.min_charge_enabled:
                        self._eval_charge()
                    if self.min_battery_voltage_enabled:
                        self._eval_battery_voltage()
                    if self.no_power_enabled:
                        self._eval_power_inputs()

        time.sleep(1)

    def run(self):
        pid = str(os.getpid())
        with open(self.PID_FILE, 'w') as pid_f:
            pid_f.write(pid)

        if not os.path.exists(self.CONFIG_PATH):
            with open(self.CONFIG_PATH, 'w+') as conf_f:
                conf_f.write(json.dumps(self.config_data))

        try:
            self.pj = PiJuice(1, 0x14)
        except:
            sys.exit(0)

        self.reload_settings()

        # Handle SIGHUP signal to reload settings
        signal.signal(signal.SIGHUP, self.reload_settings)

        if len(sys.argv) > 1 and str(sys.argv[1]) == 'stop':
            self.stop()

        self.poll_count = 5

        while self.do_poll:
            self.poll()

    def stop(self):
        is_halting = False
        if os.path.exists(self.HALT_FILE):   # Created in _SystemHalt() called in main pijuice_sys process
            is_halting = True
            os.remove(self.HALT_FILE)

        try:
            if (('watchdog' in self.config_data['system_task'])
                    and ('enabled' in self.config_data['system_task']['watchdog'])
                    and self.config_data['system_task']['watchdog']['enabled']):
                # Disabling watchdog
                ret = self.pj.power.SetWatchdog(0)
                if ret['error'] != 'NO_ERROR':
                    time.sleep(0.05)
                    self.pj.power.SetWatchdog(0)
        except:
            pass
        sys_job_targets = subprocess.check_output(["sudo", "systemctl", "list-jobs"]).decode('utf-8')
        # reboot = True if reboot.target exists
        reboot = True if re.search('reboot.target.*start', sys_job_targets) is not None else False
        # sw_stop = True if halt|shutdown exists
        sw_stop = True if re.search('(?:halt|shutdown).target.*start', sys_job_targets) is not None else False
        cause_power_off = True if (sw_stop and not reboot) else False
        ret = self.pj.status.GetStatus()
        if (ret['error'] == 'NO_ERROR'
            and not is_halting
            and cause_power_off                                # proper time to power down (!rebooting)
            and self.config_data.get('system_task', {}).get('ext_halt_power_off', {}).get('enabled', False)
            ):
            # Set duration for when pijuice will cut power (Recommended 30+ sec, for halt to complete)
            try:
                power_off_delay = int(self.config_data['system_task']['ext_halt_power_off'].get('period', 30))
                self.pj.power.SetPowerOff(power_off_delay)
            except ValueError:
                pass
        sys.exit(0)


if __name__ == '__main__':
    PiJuiceService().run()
