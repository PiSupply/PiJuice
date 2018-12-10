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

from pijuice import PiJuice, FaultEvent, USER_FUNCTIONS_COUNT


class PiJuiceJSONConfig:
    SYS_EVENTS = ['low_charge', 'low_battery_voltage', 'no_power'] + [e.name for e in FaultEvent] + \
                 ['battery_profile_invalid', 'charging_temperature_fault']
    USER_FUNC_NAMES = ('USER_FUNC{}'.format(x) for x in range(1, USER_FUNCTIONS_COUNT + 1))

    def __init__(self, config_path):
        self.config_path = config_path

        # It should work with a self.reset() call, but defining attributes explicitly in the __init__ method is good.
        self.system_task_enabled = False
        self.wakeup_on_charge_enabled = False
        self.wakeup_on_charge_trigger_level = 0.0
        self.min_charge_enabled = False
        self.min_charge_threshold = 0.0
        self.min_bat_voltage_enabled = False
        self.min_bat_voltage_threshold = 0.0
        self.watchdog_enabled = False
        self.watchdog_period = 0
        self.ext_halt_power_off_enabled = False
        self.ext_halt_power_off_period = 30

        self.system_events_enabled = False
        self.system_events = {e: {'enabled': False, 'function': None} for e in self.SYS_EVENTS}
        self.user_functions = ['' for x in range(USER_FUNCTIONS_COUNT + 1)]

    def reset(self):
        self.system_task_enabled = False
        self.wakeup_on_charge_enabled = False
        self.wakeup_on_charge_trigger_level = 0.0
        self.min_charge_enabled = False
        self.min_charge_threshold = 0.0
        self.min_bat_voltage_enabled = False
        self.min_bat_voltage_threshold = 0.0
        self.watchdog_enabled = False
        self.watchdog_period = 0
        self.ext_halt_power_off_enabled = False
        self.ext_halt_power_off_period = 30

        self.system_events_enabled = False
        self.system_events = {e: {'enabled': False, 'function': None} for e in self.SYS_EVENTS}
        self.user_functions = ['' for x in range(USER_FUNCTIONS_COUNT + 1)]

    def as_dict(self):
        data = {
            'system_task': {
                'enabled': self.system_task_enabled,
                'wakeup_on_charge': {
                    'enabled': self.wakeup_on_charge_enabled,
                    'trigger_level': self.wakeup_on_charge_trigger_level
                },
                'min_charge': {
                    'enabled': self.min_charge_enabled,
                    'threshold': self.min_charge_threshold
                },
                'min_bat_voltage': {
                    'enabled': self.min_bat_voltage_enabled,
                    'threshold': self.min_bat_voltage_threshold
                },
                'watchdog': {
                    'enabled': self.watchdog_enabled,
                    'period': self.watchdog_period
                },
                'ext_halt_power_off': {
                    'enabled': self.ext_halt_power_off_enabled,
                    'period': self.ext_halt_power_off_period
                }
            },
            'user_functions': {'USER_FUNC{}'.format(x+1): self.user_functions[x] for x in range(USER_FUNCTIONS_COUNT)}
        }

        if self.system_events_enabled:
            data['system_events'] = self.system_events

        return data

    def from_dict(self, src_dict):
        """
        May cause errors on type conversion.
        :param src_dict:
        :return:
        """
        if not isinstance(src_dict, dict):
            raise TypeError

        self.reset()

        if 'system_task' in src_dict:
            if 'enabled' in src_dict['system_task']:
                self.system_task_enabled = bool(src_dict['system_task']['enabled'])
            if 'wakeup_on_charge' in src_dict['system_task']:
                if 'enabled' in src_dict['system_task']['wakeup_on_charge']:
                    self.wakeup_on_charge_enabled = bool(src_dict['system_task']['wakeup_on_charge'])
                if 'trigger_level' in src_dict['system_task']['wakeup_on_charge']:
                    self.wakeup_on_charge_trigger_level = \
                        float(src_dict['system_task']['wakeup_on_charge']['trigger_level'])
            if 'min_charge' in src_dict['system_task']:
                if 'enabled' in src_dict['system_task']['min_charge']:
                    self.min_charge_enabled = bool(src_dict['system_task']['min_charge'])
                if 'threshold' in src_dict['system_task']['min_charge']:
                    self.min_charge_threshold = float(src_dict['system_task']['min_charge']['threshold'])
            if 'min_bat_voltage' in src_dict['system_task']:
                if 'enabled' in src_dict['system_task']['min_bat_voltage']:
                    self.min_bat_voltage_enabled = bool(src_dict['system_task']['min_bat_voltage'])
                if 'threshold' in src_dict['system_task']['min_bat_voltage']:
                    self.min_bat_voltage_threshold = float(src_dict['system_task']['min_bat_voltage']['threshold'])
            if 'watchdog' in src_dict['system_task']:
                if 'enabled' in src_dict['system_task']['watchdog']:
                    self.watchdog_enabled = bool(src_dict['system_task']['watchdog'])
                if 'period' in src_dict['system_task']['watchdog']:
                    self.watchdog_period = int(src_dict['system_task']['watchdog']['period'])
            if 'ext_halt_power_off' in src_dict['system_task']:
                if 'enabled' in src_dict['system_task']['ext_halt_power_off']:
                    self.ext_halt_power_off_enabled = bool(src_dict['system_task']['ext_halt_power_off'])
                if 'period' in src_dict['system_task']['ext_halt_power_off']:
                    self.ext_halt_power_off_period = int(src_dict['system_task']['ext_halt_power_off']['period'])
        if 'system_events' in src_dict:
            if not isinstance(src_dict['system_events'], dict):
                raise TypeError

            self.system_events_enabled = True

            for event_key in src_dict['system_events'].keys():
                if event_key in self.SYS_EVENTS:
                    if not isinstance(src_dict['system_events'][event_key], dict):
                        raise TypeError

                    if 'enabled' in src_dict['system_events'][event_key]:
                        self.system_events[event_key]['enabled'] = bool(src_dict['system_events'][event_key]['enabled'])
                    if 'function' in src_dict['system_events'][event_key]:
                        self.system_events[event_key]['function'] = \
                            str(src_dict['system_events'][event_key]['function'])
        if 'user_functions' in src_dict:
            if not isinstance(src_dict['user_functions'], dict):
                raise TypeError

            for user_func in src_dict['user_functions'].keys():
                if user_func in self.USER_FUNC_NAMES:
                    user_func_index = int(''.join(i for i in user_func if i.isdigit())) - 1
                    self.user_functions[user_func_index] = str(src_dict['user_functions'][user_func])

    def write_to_file(self):
        if not os.path.exists(self.config_path):
            with open(self.config_path, 'w+') as conf_f:
                conf_f.write(json.dumps(self.as_dict()))

    def load_from_file(self):
        with open(self.config_path, 'r') as output_config:
            config_dict = json.load(output_config)
            self.from_dict(config_dict)


class PiJuiceService:
    CONFIG_PATH = '/var/lib/pijuice/pijuice_config.JSON'  # os.getcwd() + '/pijuice_config.JSON'
    PID_FILE = '/tmp/pijuice_sys.pid'
    HALT_FILE = '/tmp/pijuice_halt.flag'
    NO_POWER_STATUSES = ['NOT_PRESENT', 'BAD']

    def __init__(self):
        self.pj = None
        self.button_config = {}
        self.config = PiJuiceJSONConfig(self.CONFIG_PATH)
        self.status = {}
        self.charge_level = 50
        self.no_power_count = 100
        self.do_poll = True
        self.poll_count = 5

    def _system_halt(self, event):
        if event in ('low_charge', 'low_battery_voltage', 'no_power') \
                and self.config.wakeup_on_charge_enabled and self.config.wakeup_on_charge_trigger_level:

            try:
                trigger_level = self.config.wakeup_on_charge_trigger_level
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
        elif func in self.config.USER_FUNC_NAMES:
            function_index = list(self.config.USER_FUNC_NAMES).index(func)
            function_ = self.config.user_functions[function_index]
            # Check function is defined
            if not function_:
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
            if self.config.min_charge_enabled:
                threshold = float(self.config.min_charge_threshold)
                if level == 0 or ((level < threshold) and (0 <= (self.charge_level - level) < 3)):
                    event = 'low_charge'
                    if self.config.system_events[event]['enabled']:
                        # energy is low, take action
                        self.execute_function(self.config.system_events[event]['function'], event, level)

            self.charge_level = level
            return True
        else:
            return False

    def _eval_battery_voltage(self):
        battery_voltage = self.pj.status.GetBatteryVoltage()
        if battery_voltage['error'] == 'NO_ERROR':
            voltage = float(battery_voltage['data']) / 1000
            try:
                threshold = float(self.config.min_bat_voltage_threshold)
            except ValueError:
                threshold = None
            if threshold is not None and voltage < threshold:
                event = 'low_battery_voltage'
                if self.config.system_events[event]['enabled']:
                    # Battery voltage below threshold, take action
                    self.execute_function(self.config.system_events[event]['function'], event, voltage)
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
            self.execute_function(self.config.system_events['no_power']['function'], 'no_power', '')

    def _eval_fault_flags(self):
        faults = self.pj.status.GetFaultStatus()
        if faults['error'] == 'NO_ERROR':
            faults = faults['data']
            for fault in (self.pj.status.faultEvents + self.pj.status.faults):
                if fault in faults:
                    if self.config.system_events_enabled and \
                            (fault in self.config.system_events) and \
                            ('enabled' in self.config.system_events[fault]) and \
                            self.config.system_events[fault]['enabled']:
                        if self.config.system_events[fault]['function'] != 'USER_EVENT':
                            self.pj.status.ResetFaultFlags([fault])
                            self.execute_function(self.config.system_events[fault]['function'],
                                                  fault, faults[fault])
            return True
        else:
            return False

    def reload_settings(self, signum=None, frame=None):
        self.config.load_from_file()

        try:
            for button in self.pj.config.buttons:
                conf = self.pj.config.GetButtonConfiguration(button)
                if conf['error'] == 'NO_ERROR':
                    self.button_config[button] = conf['data']
        except:
            pass

        if self.config.watchdog_enabled:
            try:
                p = self.config.watchdog_period
                self.pj.power.SetWatchdog(p)
            except:
                pass

    def poll(self):
        if self.config.system_task_enabled:
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
                    if self.config.system_events['low_charge']['enabled']:
                        self._eval_charge()
                    if self.config.system_events['low_battery_voltage']['enabled']:
                        self._eval_battery_voltage()
                    if self.config.system_events['no_power']['enabled']:
                        self._eval_power_inputs()

        time.sleep(1)

    def run(self):
        pid = str(os.getpid())
        with open(self.PID_FILE, 'w') as pid_f:
            pid_f.write(pid)

        self.config.write_to_file()

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
            if self.config.watchdog_enabled:
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
            and self.config.ext_halt_power_off_enabled):
            # Set duration for when pijuice will cut power (Recommended 30+ sec, for halt to complete)
            try:
                power_off_delay = self.config.ext_halt_power_off_period
                self.pj.power.SetPowerOff(power_off_delay)
            except ValueError:
                pass
        sys.exit(0)


if __name__ == '__main__':
    PiJuiceService().run()
