# This python script to be executed as user pijuice by the setuid program pijuice_cli 
# Python 3 only
#
# -*- coding: utf-8 -*-
# pylint: disable=import-error
import datetime
import json
import os
import re
import signal
import subprocess
import time
import fcntl
import json

import urwid
from pijuice import PiJuice, pijuice_hard_functions, pijuice_sys_functions, pijuice_user_functions

BUS = 1
ADDRESS = 0x14
PID_FILE = '/tmp/pijuice_sys.pid'
LOCK_FILE = '/tmp/pijuice_gui.lock'

try:
    pijuice = PiJuice(BUS, ADDRESS)
except:
    pijuice = None

pijuiceConfigData = {}
PiJuiceConfigDataPath = '/var/lib/pijuice/pijuice_config.JSON'

#### Following taken from urwid 2.0.2 to get a FloatEdit widget ###
#
# Urwid basic widget classes
#    Copyright (C) 2004-2012  Ian Ward
#
#    This library is free software; you can redistribute it and/or
#    modify it under the terms of the GNU Lesser General Public
#    License as published by the Free Software Foundation; either
#    version 2.1 of the License, or (at your option) any later version.
#
#    This library is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#    Lesser General Public License for more details.
#
#    You should have received a copy of the GNU Lesser General Public
#    License along with this library; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# Urwid web site: http://excess.org/urwid/


from urwid import Edit
from decimal import Decimal
#import re


class NumEdit(Edit):
    """NumEdit - edit numerical types

    based on the characters in 'allowed' different numerical types
    can be edited:
      + regular int: 0123456789
      + regular float: 0123456789.
      + regular oct: 01234567
      + regular hex: 0123456789abcdef
    """
    ALLOWED = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

    def __init__(self, allowed, caption, default, trimLeadingZeros=True):
        super(NumEdit, self).__init__(caption, default)
        self._allowed = allowed
        self.trimLeadingZeros = trimLeadingZeros

    def valid_char(self, ch):
        """
        Return true for allowed characters.
        """
        return len(ch) == 1 and ch.upper() in self._allowed

    def keypress(self, size, key):
        """
        Handle editing keystrokes.  Remove leading zeros.
        """
        (maxcol,) = size
        unhandled = Edit.keypress(self, (maxcol,), key)

        if not unhandled:
            if self.trimLeadingZeros:
                # trim leading zeros
                while self.edit_pos > 0 and self.edit_text[:1] == "0":
                    self.set_edit_pos(self.edit_pos - 1)
                    self.set_edit_text(self.edit_text[1:])

        return unhandled


class FloatEdit(NumEdit):
    """Edit widget for float values."""

    def __init__(self, caption="", default=None,
                 preserveSignificance=False, decimalSeparator='.'):
        """
        caption -- caption markup
        default -- default edit value
        preserveSignificance -- return value has the same signif. as default
        decimalSeparator -- use '.' as separator by default, optionally a ','
        """
        self.significance = None
        self._decimalSeparator = decimalSeparator
        if decimalSeparator not in ['.', ',']:
            raise ValueError("invalid decimalSeparator: {}".format(
                             decimalSeparator))

        val = ""
        if default is not None and default is not "":
            if not isinstance(default, (int, str, Decimal)):
                raise ValueError("default: Only 'str', 'int', "
                                 "'long' or Decimal input allowed")

            if isinstance(default, str) and len(default):
                # check if it is a float, raises a ValueError otherwise
                float(default)
                default = Decimal(default)

            if preserveSignificance:
                self.significance = abs(default.as_tuple()[2])

            val = str(default)

        super(FloatEdit, self).__init__(self.ALLOWED[0:10] + decimalSeparator,
                                        caption, val)

####################################################################################


def version_to_str(number):
    # Convert int version to str {major}.{minor}
    return "{}.{}".format(number >> 4, number & 15)


def validate_value(text, type, min, max, old):
    if type == 'int':
        var_type = int
    elif type == 'float':
        var_type = float
    else:
        return text

    try:
        value = var_type(text)
        if min is not None:
            value = value if value >= min else min
        if max is not None:
            value = value if value <= max else max
    except ValueError:
        value = old

    return str(value)


def confirmation_dialog(text, next, nextno='', single_option=True):
    elements = [urwid.Padding(urwid.Text(text), align='center', width='pack'),
                urwid.Divider()]
    if single_option:
        elements.append(urwid.Padding(attrmap(urwid.Button("OK", on_press=next)), align='center', width=6))
    else:
        yes_btn = urwid.Button("Yes")
        no_btn  = urwid.Button("No")
        pad_yes_btn = urwid.Padding(attrmap(yes_btn), width=7)
        pad_no_btn  = urwid.Padding(attrmap(no_btn), width=7)
        urwid.connect_signal(yes_btn, 'click', next, True)
        urwid.connect_signal(no_btn, 'click', nextno, False)
        elements.extend([pad_yes_btn, pad_no_btn])

    main.original_widget = urwid.Filler(urwid.Pile(elements))

class StatusTab(object):
    def __init__(self, *args):
        self.main()

    def get_status(self):
        """
        HAT tab in GUI. Shows general status of the device.
        """
        status = pijuice.status.GetStatus().get('data', {})
        charge_level = pijuice.status.GetChargeLevel().get('data', -1)
        general_info = "%i%%" % (charge_level)

        voltage = float(pijuice.status.GetBatteryVoltage().get('data', 0))  # mV
        if voltage:
            general_info += ", %.3fV, %s" % (voltage / 1000, status['battery'])

        usb_power = status.get('powerInput', 0)

        io_voltage = float(pijuice.status.GetIoVoltage().get('data', 0))  # mV
        io_current = float(pijuice.status.GetIoCurrent().get('data', 0))  # mA
        gpio_info = "%.3fV, %.3fA, %s" % (io_voltage / 1000, io_current / 1000,
                    status['powerInput5vIo'] if not(status == {}) else 'N/A')

        fault_info = pijuice.status.GetFaultStatus()
        if fault_info['error'] == 'NO_ERROR':
            bpi = None
            if ('battery_profile_invalid' in fault_info['data']) and fault_info['data']['battery_profile_invalid']:
                bpi = 'battery profile invalid'
            cti = None
            if ('charging_temperature_fault' in fault_info['data']) and (fault_info['data']['charging_temperature_fault'] != 'NORMAL'):
                cti = 'charging temperature' + fault_info['data']['charging_temperature_fault']
            if not (bpi or cti):
                fault = None
            else:
                faults = [x for x in (bpi, cti) if x]
                fault = ', '.join(faults)
        else:
            fault = fault_info['error']
        
        sys_sw_status = pijuice.power.GetSystemPowerSwitch().get('data', None)
        return {
            "battery": general_info,
            "gpio": gpio_info,
            "usb": str(usb_power),
            "fault": str(fault),
            "sys_sw": str(sys_sw_status) + "mA"
        }

    def update_status(self, obj, text):
        status_args = self.get_status()
        text.set_text("HAT status\n\n"
                      "Battery: {battery}\nGPIO power input: {gpio}\n"
                      "USB Micro power input: {usb}\nFault: {fault}\n"
                      "System switch: {sys_sw}\n".format(**status_args))
        self.alarm_handle = loop.set_alarm_in(1, self.update_status, text)


    def main(self, *args):
        status_args = self.get_status()
        text = urwid.Text("HAT status\n\n"
                        "Battery: {battery}\nGPIO power input: {gpio}\n"
                        "USB Micro power input: {usb}\nFault: {fault}\n"
                        "System switch: {sys_sw}\n".format(**status_args))
        #refresh_btn = urwid.Padding(attrmap(urwid.Button('Refresh', on_press=self.main)), width=24)
        main_menu_btn = urwid.Padding(attrmap(urwid.Button('Back', on_press=self._goto_main_menu)), width=24)
        pwr_switch_btn = urwid.Padding(attrmap(urwid.Button('Change Power switch', on_press=self.change_power_switch)), width=24)
        main.original_widget = urwid.Filler(
            #urwid.Pile([text, urwid.Divider(), refresh_btn, pwr_switch_btn, main_menu_btn]), valign='top')
            urwid.Pile([text, urwid.Divider(), pwr_switch_btn, main_menu_btn]), valign='top')
        self.alarm_handle = loop.set_alarm_in(1, self.update_status, text)

    def change_power_switch(self, *args):
        loop.remove_alarm(self.alarm_handle)
        elements = [urwid.Text("Choose value for System Power switch"), urwid.Divider()]
        values = [0, 500, 2100]
        for value in values:
            text = str(value) + " mA" if value else "Off"
            elements.append(urwid.Padding(attrmap(urwid.Button(text, on_press=self.set_power_switch, user_data=value)), width=11))
        elements.extend([urwid.Divider(),
                        urwid.Padding(attrmap(urwid.Button("Back", on_press=self.main)), width=8)])
        main.original_widget = urwid.Filler(urwid.Pile(elements), valign='top')


    def set_power_switch(self, button, value):
        pijuice.power.SetSystemPowerSwitch(int(value))
        self.main()

    def _goto_main_menu(self, *args):
        loop.remove_alarm(self.alarm_handle)
        main_menu()

class FirmwareTab(object):
    FIRMWARE_UPDATE_ERRORS = ['NO_ERROR', 'I2C_BUS_ACCESS_ERROR', 'INPUT_FILE_OPEN_ERROR', 'STARTING_BOOTLOADER_ERROR', 'FIRST_PAGE_ERASE_ERROR',
                              'EEPROM_ERASE_ERROR', 'INPUT_FILE_READ_ERROR', 'PAGE_WRITE_ERROR', 'PAGE_READ_ERROR', 'PAGE_VERIFY_ERROR', 'CODE_EXECUTE_ERROR']

    def __init__(self, *args):
        self.firmware_path = None
        self.show_firmware()

    def get_current_fw_version(self):
        # Returns current version as int (first 4 bits - minor, second 4 bits - major)
        status = pijuice.config.GetFirmwareVersion()

        if status['error'] == 'NO_ERROR':
            major, minor = status['data']['version'].split('.')
        else:
            major = minor = 0
        current_version = (int(major) << 4) + int(minor)
        return current_version

    def check_for_fw_updates(self):
        # Check /usr/share/pijuice/data/firmware/ for new version of firmware.
        # Returns (version, path)
        bin_file = None
        bin_dir = '/usr/share/pijuice/data/firmware/'

        try:
            files = [f for f in os.listdir(bin_dir) if os.path.isfile(os.path.join(bin_dir, f))]
        except:
            files = []
        latest_version = 0
        firmware_path = ""

        regex = re.compile(r"PiJuice-V(\d+)\.(\d+)_(\d+_\d+_\d+).elf.binary")
        for f in files:
            # 1 - major, 2 - minor, 3 - date
            match = regex.match(f)
            if match:
                major = int(match.group(1))
                minor = int(match.group(2))
                version = (major << 4) + minor
                if version >= latest_version:
                    latest_version = version
                    bin_file = bin_dir + f
                    firmware_path = bin_file

        return latest_version, firmware_path

    def get_fw_status(self):
        current_version = self.get_current_fw_version()
        latest_version, firmware_path = self.check_for_fw_updates()
        new_firmware_path = None

        if current_version and latest_version:
            if latest_version > current_version:
                firmware_status = 'New firmware (V' + version_to_str(latest_version) + ') is available'
                new_firmware_path = firmware_path
            else:
                firmware_status = 'up to date'
        elif not current_version:
            firmware_status = 'unknown'
        else:
            firmware_status = 'Missing/wrong firmware file'
        return current_version, latest_version, firmware_status, new_firmware_path

    def update_firmware_start(self, *args):
        device_status = pijuice.status.GetStatus()

        if device_status['error'] == 'NO_ERROR':
            if device_status['data']['powerInput'] != 'PRESENT' and \
                device_status['data']['powerInput5vIo'] != 'PRESENT' and \
                pijuice.status.GetChargeLevel().get('data', 0) < 20:
                # Charge level is too low
                return confirmation_dialog("Charge level is too low", next=main_menu, single_option=True)
        confirmation_dialog("Are you sure you want to update the firmware?", next=self.update_firmware,
                            nextno=main_menu, single_option=False)

    def update_firmware(self, *args):
        current_addr = pijuice.config.interface.GetAddress()
        error_status = None
        if current_addr:
            main.original_widget = urwid.Filler(urwid.LineBox(urwid.Pile([
                                      urwid.Text("Updating firmware, Wait ...", align='center'),
                                      urwid.Divider(),
                                      urwid.Text("Interrupting this process can lead to a non-functional device.", align='center')
                                                              ])))
            loop.draw_screen()
            addr = format(current_addr, 'x')
            with open('/dev/null','w') as f:    # Suppress pijuiceboot output
                result = 256 - subprocess.call(['pijuiceboot', addr, self.firmware_path], stdout=f, stderr=subprocess.STDOUT)
            if result != 256:
                error_status = self.FIRMWARE_UPDATE_ERRORS[result] if result < 11 else 'UNKNOWN'
                messages = {
                    "I2C_BUS_ACCESS_ERROR": 'Check if I2C bus is enabled.',
                    "INPUT_FILE_OPEN_ERROR": 'Firmware binary file might be missing or damaged.',
                    "STARTING_BOOTLOADER_ERROR": 'Try to start bootloader manually. Press and hold button SW3 while powering up RPI and PiJuice.',
                    "UNKNOWN_ADDRESS": "Unknown PiJuice I2C address",
                }
        else:
            error_status = "UNKNOWN_ADDRESS"

        if error_status:
            message = "Firmware update failed.\nReason: " + error_status + '. ' + messages.get(error_status, '')
        else:
            # Wait till firmware has restarted (current_version != 0)
            current_version = 0
            while current_version == 0:
                current_version = self.get_current_fw_version()
                time.sleep(0.2)
            message = "Firmware update successful"

        confirmation_dialog(message, single_option=True, next=self.show_firmware)
    
    def show_firmware(self, *args):
        current_version, latest_version, firmware_status, firmware_path = self.get_fw_status()
        current_version_txt = urwid.Text("Current version: " + version_to_str(current_version))
        status_txt = urwid.Text("Status: " + firmware_status)
        elements = [urwid.Text("Firmware"), urwid.Divider(), current_version_txt, status_txt, urwid.Divider()]
        if latest_version > current_version:
            self.firmware_path = firmware_path
            elements.append(urwid.Padding(attrmap(urwid.Button('Update', on_press=self.update_firmware_start)),width=10))
        elements.append(urwid.Padding(attrmap(urwid.Button('Back', on_press=main_menu)),width=10))
        main.original_widget = urwid.Filler(urwid.Pile(elements), valign='top')


class GeneralTab(object):
    if pijuice:
        RUN_PIN_VALUES = pijuice.config.runPinConfigs
        EEPROM_ADDRESSES = pijuice.config.idEepromAddresses
        INPUTS_PRECEDENCE = pijuice.config.powerInputs
        USB_CURRENT_LIMITS = pijuice.config.usbMicroCurrentLimits
        USB_MICRO_IN_DPMS = pijuice.config.usbMicroDPMs
        POWER_REGULATOR_MODES = pijuice.config.powerRegulatorModes

    def __init__(self, *args):
        try:
            self.current_config = self._get_device_config()
            self.main()
        except:
            confirmation_dialog("Unable to connect to device", single_option=True, next=main_menu)

    def _get_device_config(self):
        config = {}
        config['run_pin'] = self.RUN_PIN_VALUES.index(pijuice.config.GetRunPinConfig().get('data'))
        config['i2c_addr'] = int(pijuice.config.GetAddress(1).get('data'))
        config['i2c_addr_rtc'] = int(pijuice.config.GetAddress(2).get('data'))
        config['eeprom_addr'] = self.EEPROM_ADDRESSES.index(pijuice.config.GetIdEepromAddress().get('data'))
        config['eeprom_write_unprotected'] = not pijuice.config.GetIdEepromWriteProtect().get('data', False)
        result = pijuice.config.GetPowerInputsConfig()
        if result['error'] == 'NO_ERROR':
            pow_config = result['data']
            config['precedence'] = self.INPUTS_PRECEDENCE.index(pow_config['precedence'])
            config['gpio_in_enabled'] = pow_config['gpio_in_enabled']
            config['usb_micro_current_limit'] = self.USB_CURRENT_LIMITS.index(pow_config['usb_micro_current_limit'])
            config['usb_micro_dpm'] = self.USB_MICRO_IN_DPMS.index(pow_config['usb_micro_dpm'])
            config['no_battery_turn_on'] = pow_config['no_battery_turn_on']

        config['power_reg_mode'] = self.POWER_REGULATOR_MODES.index(pijuice.config.GetPowerRegulatorMode().get('data'))
        config['charging_enabled'] = pijuice.config.GetChargingConfig().get('data', {}).get('charging_enabled')
        return config

    def main(self, *args):
        elements = [urwid.Text("General settings"), urwid.Divider()]

        options_with_lists = [
            {'title': 'Run pin', 'list': self.RUN_PIN_VALUES, 'key': 'run_pin'},
            {'title': 'EEPROM address', 'list': self.EEPROM_ADDRESSES, 'key': 'eeprom_addr'},
            {'title': 'Inputs precedence', 'list': self.INPUTS_PRECEDENCE, 'key': 'precedence'},
            {'title': 'USB micro current limit', 'list': self.USB_CURRENT_LIMITS, 'key': 'usb_micro_current_limit'},
            {'title': 'USB micro IN DPM', 'list': self.USB_MICRO_IN_DPMS, 'key': 'usb_micro_dpm'},
            {'title': 'Power regulator mode', 'list': self.POWER_REGULATOR_MODES, 'key': 'power_reg_mode'},
        ]

        options_with_checkboxes = [
            {'title': "GPIO input enable", 'key': 'gpio_in_enabled'},
            {'title': "EEPROM write unprotect", 'key': 'eeprom_write_unprotected'},
            {'title': "Charging enabled", 'key': 'charging_enabled'},
            {'title': "No battery turn on", 'key': 'no_battery_turn_on'},
        ]

        # I2C address
        i2c_addr_edit = urwid.Edit("I2C address: ", edit_text=str(self.current_config['i2c_addr']))
        urwid.connect_signal(i2c_addr_edit, 'change', lambda x, text: self.current_config.update({'i2c_addr': text}))
        elements.append(attrmap(i2c_addr_edit))

        # I2C address RTC
        i2c_addr_rtc_edit = urwid.Edit("I2C address RTC: ", edit_text=str(self.current_config['i2c_addr_rtc']))
        urwid.connect_signal(i2c_addr_rtc_edit, 'change', lambda x, text: self.current_config.update({'i2c_addr_rtc': text}))
        elements.append(attrmap(i2c_addr_rtc_edit))

        for option in options_with_checkboxes:
            elements.append(attrmap(urwid.CheckBox(option['title'], state=self.current_config[option['key']],
                                           on_state_change=lambda x, state, key: self.current_config.update({key: state}),
                                           user_data=option['key'])))

        for option in options_with_lists:
            elements.append(attrmap(urwid.Button("{title}: {value}".format(title=option['title'],
                                         value=option['list'][self.current_config[option['key']]]),
                                         on_press=self._list_options, user_data=option)))

        elements.extend([urwid.Divider(),
                         urwid.Padding(attrmap(urwid.Button("Apply settings", on_press=self._apply_settings)), width=20),
                         urwid.Padding(attrmap(urwid.Button('Reset to default', on_press=lambda x: confirmation_dialog(
                             "This action will reset all settings on your device to their default values.\n"
                             "Do you want to proceed?", single_option=False, next=self._reset_settings, nextno=main_menu))), width=20),
                         urwid.Padding(attrmap(urwid.Button('Back', on_press=main_menu)), width=20)
                         ])
        main.original_widget = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(elements)), width=48)
    
    def _list_options(self, button, data):
        body = [urwid.Text(data['title']), urwid.Divider()]
        self.bgroup = []
        for choice in data['list']:
            button = urwid.RadioButton(self.bgroup, choice)
            body.append(button)
        self.bgroup[self.current_config[data['key']]].toggle_state()
        body.extend([urwid.Divider(),
                     urwid.Padding(urwid.Button('Back', on_press=self._set_option, user_data=data), width=8)])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(body))
    
    def _set_option(self, button, data):
        states = [c.state for c in self.bgroup]
        self.current_config[data['key']] = states.index(True)
        self.bgroup = []
        self.main()

    def _apply_settings(self, *args):
        device_config = self._get_device_config()
        changed = [key for key in self.current_config.keys() if self.current_config[key] != device_config[key]]

        if 'run_pin' in changed:
            pijuice.config.SetRunPinConfig(self.RUN_PIN_VALUES[self.current_config['run_pin']])

        for i, addr in enumerate(['i2c_addr', 'i2c_addr_rtc']):
            if addr in changed:
                value = device_config[addr]
                try:
                    new_value = int(str(self.current_config[addr]), 16)
                    if new_value <= 0x7F:
                        value = self.current_config[addr]
                except:
                    pass
                pijuice.config.SetAddress(i + 1, value)

        if 'eeprom_addr' in changed:
            pijuice.config.SetIdEepromAddress(self.EEPROM_ADDRESSES[self.current_config['eeprom_addr']])
        if 'eeprom_write_unprotected' in changed:
            pijuice.config.SetIdEepromWriteProtect(not self.current_config['eeprom_write_unprotected'])

        if set(['precedence', 'gpio_in_enabled', 'usb_micro_current_limit', 'usb_micro_dpm', 'no_battery_turn_on']) & set(changed):
            config = {
                'precedence': self.INPUTS_PRECEDENCE[self.current_config['precedence']],
                'gpio_in_enabled': self.current_config['gpio_in_enabled'],
                'no_battery_turn_on': self.current_config['no_battery_turn_on'],
                'usb_micro_current_limit': self.USB_CURRENT_LIMITS[self.current_config['usb_micro_current_limit']],
                'usb_micro_dpm': self.USB_MICRO_IN_DPMS[self.current_config['usb_micro_dpm']],
            }
            pijuice.config.SetPowerInputsConfig(config, True)
        
        if 'power_reg_mode' in changed:
            pijuice.config.SetPowerRegulatorMode(self.POWER_REGULATOR_MODES[self.current_config['power_reg_mode']])
        if 'charging_enabled' in changed:
            pijuice.config.SetChargingConfig({'charging_enabled': self.current_config['charging_enabled']}, True)
        
        self.current_config = self._get_device_config()
        confirmation_dialog("Settings successfully updated", single_option=True, next=self.main)

    def _reset_settings(self, button, is_confirmed):
        if is_confirmed:
            error = pijuice.config.SetDefaultConfiguration().get('error', 'NO_ERROR')
            if error == "NO_ERROR":
                confirmation_dialog("Settings have been reset to their default values", single_option=True, next=main_menu)
            else:
                confirmation_dialog("Failed to reset settings: " + error, single_option=True, next=main_menu)
        else:
            self.main()


class LEDTab(object):
    if pijuice:
       LED_FUNCTIONS_OPTIONS = pijuice.config.ledFunctionsOptions
       LED_NAMES = pijuice.config.leds

    def __init__(self, *args):
        self._refresh_settings()
        self.main()
    
    def main(self, *args):
        elements = [urwid.Text("LED settings"), urwid.Divider()]
        for i in range(len(self.LED_NAMES)):
            elements.append(urwid.Padding(
                            attrmap(urwid.Button(self.LED_NAMES[i], on_press=self.configure_led, user_data=i)),
                                          width=6))

        elements.extend([urwid.Divider(),
                         attrmap(urwid.Button("Apply settings", on_press=self._apply_settings)),
                         attrmap(urwid.Button("Refresh", on_press=self._refresh_settings)),
                         attrmap(urwid.Button("Back", on_press=main_menu)),
                         ])
        main.original_widget = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(elements)), width=18)
    
    def configure_led(self, button, index):
        elements = [urwid.Text("LED " + self.LED_NAMES[index]), urwid.Divider()]
        colors = ('R', 'G', 'B')
        button=attrmap(urwid.Button("Function: {value}".format(value=self.current_config[index]['function']),
                            on_press=self._list_functions, user_data=index))
        elements.append(urwid.Padding(button, width=30))
        for color in colors:
            color_edit = urwid.Edit(color + ": ", edit_text=str(self.current_config[index]['color'][colors.index(color)]))
            urwid.connect_signal(color_edit, 'change', self._set_color, {'color_index': colors.index(color), 'led_index': index})
            elements.append(attrmap(color_edit))
        elements.extend([urwid.Divider(), urwid.Padding(attrmap(urwid.Button("Back", on_press=self.main)), width=8)])
        main.original_widget = urwid.Filler(urwid.Pile(elements), valign='top')
    
    def _get_led_config(self):
        config = []
        for i in range(len(self.LED_NAMES)):
            result = pijuice.config.GetLedConfiguration(self.LED_NAMES[i])
            led_config = {}
            try:
                led_config['function'] = result['data']['function']
            except ValueError:
                led_config['function'] = self.LED_FUNCTIONS_OPTIONS[0]
            led_config['color'] = [result['data']['parameter']['r'], result['data']['parameter']['g'], result['data']['parameter']['b']]
            config.append(led_config)
        return config
    
    def _refresh_settings(self, *args):
        self.current_config = self._get_led_config()
    
    def _apply_settings(self, *args):
        device_config = self._get_led_config()
        for i in range(len(self.LED_NAMES)):
            config = {
                "function": self.current_config[i]['function'],
                "parameter": {
                    "r": self.current_config[i]['color'][0],
                    "g": self.current_config[i]['color'][1],
                    "b": self.current_config[i]['color'][2],
                }
            }
            pijuice.config.SetLedConfiguration(self.LED_NAMES[i], config)
        
        self.current_config = self._get_led_config()
        confirmation_dialog("Settings successfully updated", single_option=True, next=self.main)
    
    def _list_functions(self, button, led_index):
        body = [urwid.Text("Choose function for " + self.LED_NAMES[led_index]), urwid.Divider()]
        self.bgroup = []
        for choice in self.LED_FUNCTIONS_OPTIONS:
            button = urwid.RadioButton(self.bgroup, choice)
            body.append(attrmap(button))
        self.bgroup[self.LED_FUNCTIONS_OPTIONS.index(self.current_config[led_index]['function'])].toggle_state()
        body.extend([urwid.Divider(),
                     urwid.Padding(attrmap(urwid.Button('Back', on_press=self._set_function, user_data=led_index)),
                                   width=8)])
        main.original_widget = urwid.Filler(urwid.Pile(body), valign='top')
    
    def _set_function(self, button, led_index):
        states = [c.state for c in self.bgroup]
        self.current_config[led_index]['function'] = self.LED_FUNCTIONS_OPTIONS[states.index(True)]
        self.bgroup = []
        self.configure_led(None, led_index)

    def _set_color(self, edit, text, data):
        led_index = data['led_index']
        color_index = data['color_index']
        try:
            value = int(text)
        except ValueError:
            value = self.current_config[led_index]['color'][color_index]
        if value >= 0 and value <= 255:
            self.current_config[led_index]['color'][color_index] = value
    

class ButtonsTab(object):
    if pijuice:
        FUNCTIONS = ['NO_FUNC'] + pijuice_hard_functions + pijuice_sys_functions + pijuice_user_functions
        BUTTONS = pijuice.config.buttons
        EVENTS = pijuice.config.buttonEvents

    def __init__(self):
        self.device_config = self._get_device_config()
        self.current_config = self._get_device_config()
        self.main()

    def main(self, *args):
        elements = [urwid.Text("Buttons"), urwid.Divider()]
        for sw_id in self.BUTTONS:
            elements.append(urwid.Padding(attrmap(urwid.Button(sw_id, on_press=self.configure_sw, user_data=sw_id)), width=7))
        elements.append(urwid.Divider())
        if self.device_config != self.current_config:
            elements.append(urwid.Padding(attrmap(urwid.Button("Apply settings", on_press=self._apply_settings)), width=18))
        elements.extend([urwid.Padding(attrmap(urwid.Button("Refresh", on_press=self._refresh_settings)), width=11),
                         urwid.Padding(attrmap(urwid.Button("Back", on_press=main_menu)), width=11),
                         ])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(elements))
    
    def configure_sw(self, button, sw_id):
        elements = [urwid.Text("Settings for " + sw_id), urwid.Divider()]
        config = self.current_config[sw_id]
        for action, parameters in config.items():
            elements.append(attrmap(urwid.Button("{action}: {function}, {parameter}".format(
                action=action, function=parameters['function'], parameter=parameters['parameter']),
                on_press=self.configure_action, user_data={'sw_id': sw_id, 'action': action})))
        elements += [urwid.Divider(), urwid.Padding(attrmap(urwid.Button("Back", on_press=self.main)), width=8)]
        main.original_widget = urwid.Padding(urwid.ListBox(
            urwid.SimpleFocusListWalker(elements)), width=46)
    
    def configure_action(self, button, data):
        sw_id = data['sw_id']
        action = data['action']
        functions_btn = attrmap(urwid.Button("Function: {}".format(self.current_config[sw_id][action]['function']),
                                     on_press=self._set_function, user_data={'sw_id': sw_id, 'action': action}))
        parameter_edit = urwid.Edit("Parameter: ", edit_text=str(self.current_config[sw_id][action]['parameter']))
        urwid.connect_signal(parameter_edit, 'change', self._set_parameter, {'sw_id': sw_id, 'action': action})
        parameter_edit = attrmap(parameter_edit)
        parameter_text = urwid.Text("Parameter: " + str(self.current_config[sw_id][action]['parameter']))
        if action != 'PRESS' and action != 'RELEASE':
           paramline = parameter_edit
        else:
           paramline = parameter_text
        back_btn = urwid.Padding(attrmap(urwid.Button("Back", on_press=self.configure_sw, user_data=sw_id)), width=8)
        elements = [urwid.Text("Set function for {} on {}".format(action, sw_id)),
                    urwid.Divider(),
                    functions_btn,
                    paramline,
                    urwid.Divider(),
                    back_btn]
        main.original_widget = urwid.Padding(urwid.ListBox(
            urwid.SimpleFocusListWalker(elements)), width=37)
    
    def _refresh_settings(self, *args):
        self.device_config = self._get_device_config()
        self.current_config = self._get_device_config()
        confirmation_dialog("Settings have been refreshed", next=self.main, single_option=True)
    
    def _set_function(self, button, data):
        sw_id = data['sw_id']
        action = data['action']
        body = [urwid.Text("Choose function for {} on {}".format(action, sw_id)), urwid.Divider()]
        self.bgroup = []
        for function in self.FUNCTIONS:
            button = attrmap(urwid.RadioButton(self.bgroup, function))
            body.append(button)
        self.bgroup[self.FUNCTIONS.index(self.current_config[sw_id][action]['function'])].toggle_state()
        body.extend([urwid.Divider(),
                     urwid.Padding(attrmap(urwid.Button('Back', on_press=self._on_function_chosen, user_data=data)), width=8)])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(body))
    
    def _on_function_chosen(self, button, data):
        states = [c.state for c in self.bgroup]
        self.current_config[data['sw_id']][data['action']]['function'] = self.FUNCTIONS[states.index(True)]
        self.bgroup = []
        self.configure_action(None, data)
    
    def _set_parameter(self, edit, text, data):
        sw_id = data['sw_id']
        action = data['action']
        # 'PRESS' and 'RELEASE' take no parameter
        if action != 'PRESS' and action != 'RELEASE':
            self.current_config[sw_id][action]['parameter'] = text
    
    def _get_device_config(self):
        config = {}
        got_error = False
        for button in self.BUTTONS:
            button_config = pijuice.config.GetButtonConfiguration(button)
            if button_config.get('error', 'NO_ERROR'):
                config[button] = button_config.get('data')
            else:
                config[button] = {}
                got_error = True

        if got_error:
            confirmation_dialog("Failed to connect to PiJuice", next=main_menu, single_option=True)
        else:
            return config
    
    def _apply_settings(self, *args):
        got_error = False
        errors = []
        for button in self.BUTTONS:
            error_msg = pijuice.config.SetButtonConfiguration(button, self.current_config[button]).get('error', 'NO_ERROR')
            errors.append(error_msg)
            got_error |= error_msg != "NO_ERROR"

        self.device_config = self._get_device_config()
        self.current_config = self._get_device_config()
        if got_error:
            confirmation_dialog("Failed to apply settings: " + str(errors), next=self.main, single_option=True)
        else:
            notify_service()
            confirmation_dialog("Settings have been applied", next=self.main, single_option=True)
    

class IOTab(object):
    if pijuice:
        IO_PINS_COUNT = 2
        IO_CONFIG_PARAMS = pijuice.config.ioConfigParams  # mode: [var_1, var_2]
        IO_SUPPORTED_MODES = pijuice.config.ioSupportedModes
        IO_PULL_OPTIONS = pijuice.config.ioPullOptions

    def __init__(self):
        self.current_config = self._get_device_config()
        self.main()
    
    def main(self, *args):
        elements = [urwid.Text("IO settings"), urwid.Divider()]
        for i in range(self.IO_PINS_COUNT):
            elements.append(urwid.Padding(attrmap(urwid.Button("IO" + str(i + 1),
                                                      on_press=self.configure_io, user_data=i)), width=7))

        elements.extend([urwid.Divider(),
                         urwid.Padding(attrmap(urwid.Button("Apply settings",
                                                            on_press=self._apply_settings, user_data=self.IO_PINS_COUNT)),
                                       width=18),
                         urwid.Padding(attrmap(urwid.Button("Back", on_press=main_menu)), width=18)
                        ])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(elements))
    
    def configure_io(self, button, pin_id):
        elements = [urwid.Text("IO{}".format(pin_id + 1)), urwid.Divider()]
        pin_config = self.current_config[pin_id]
        mode = pin_config['mode']
        pull = pin_config['pull']
        # < Mode >  
        mode_select_btn = urwid.Padding(attrmap(urwid.Button("Mode: {}".format(mode),
                                       on_press=self._select_mode, user_data=pin_id)), width=32)
        # < Pull >
        pull_select_btn = urwid.Padding(attrmap(urwid.Button("Pull: {}".format(pull),
                                       on_press=self._select_pull, user_data=pin_id)), width=32)
        elements += [mode_select_btn, pull_select_btn]
        # Edits for vars
        # XXX: Hack to pass var parameters
        if len(self.IO_CONFIG_PARAMS.get(mode, [])) > 0:
            var_config = self.IO_CONFIG_PARAMS[mode][0]
            var_name = var_config.get('name', '')
            var_unit = var_config.get('unit')
            label = "{} [{}]: ".format(
                var_name, var_unit) if var_unit else "{} [0/1]: ".format(var_name)
            var_edit_1 = urwid.Edit(label, edit_text=str(pin_config[var_name]))
            # Validate int/float
            urwid.connect_signal(var_edit_1, 'change', lambda x, text: self.current_config[pin_id].update(
                {self.IO_CONFIG_PARAMS[mode][0]['name']: validate_value(
                    text,
                    self.IO_CONFIG_PARAMS[mode][0].get('type', 'str'),
                    self.IO_CONFIG_PARAMS[mode][0].get('min'),
                    self.IO_CONFIG_PARAMS[mode][0].get('max'),
                    pin_config[var_name])}))
            elements.append(urwid.Padding(attrmap(var_edit_1), width=32))

        if len(self.IO_CONFIG_PARAMS.get(mode, [])) > 1:
            var_config = self.IO_CONFIG_PARAMS[mode][1]
            var_name = var_config.get('name', '')
            var_unit = var_config.get('unit')
            label = "{} [{}]: ".format(
                var_name, var_unit) if var_unit else "{}: ".format(var_name)
            var_edit_2 = urwid.Edit(label, edit_text=str(pin_config[var_name]))
            # Validate int/float
            urwid.connect_signal(var_edit_2, 'change', lambda x, text: self.current_config[pin_id].update(
                {self.IO_CONFIG_PARAMS[mode][1]['name']: validate_value(
                    text,
                    self.IO_CONFIG_PARAMS[mode][1].get('type', 'str'),
                    self.IO_CONFIG_PARAMS[mode][1].get('min'),
                    self.IO_CONFIG_PARAMS[mode][1].get('max'),
                    pin_config[var_name])}))
            elements.append(urwid.Padding(attrmap(var_edit_2), width=32))

        elements += [urwid.Divider(),
                     urwid.Padding(attrmap(urwid.Button("Apply", on_press=self._apply_settings, user_data=pin_id)), width=9),
                     urwid.Padding(attrmap(urwid.Button("Back", on_press=self.main)), width=9)]
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(elements))
    
    def _select_mode(self, button, pin_id):
        elements = [urwid.Text("Mode for IO{}".format(pin_id + 1)), urwid.Divider()]
        self.bgroup = []
        for choice in self.IO_SUPPORTED_MODES[pin_id + 1]:
            elements.append(urwid.Padding(attrmap(urwid.RadioButton(self.bgroup, choice)), width=26))
        # Toggle the configured state
        self.bgroup[self.IO_SUPPORTED_MODES[pin_id + 1].index(self.current_config[pin_id]['mode'])].toggle_state()

        elements.extend([urwid.Divider(),
                         urwid.Padding(attrmap(urwid.Button('Back', on_press=self._on_mode_selected, user_data=pin_id)),
                                       width=8)
                        ])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(elements))
    
    def _on_mode_selected(self, button, pin_id):
        states = [c.state for c in self.bgroup]
        mode = self.IO_SUPPORTED_MODES[pin_id + 1][states.index(True)]
        pull = self.current_config[pin_id]['pull']
        config = {
            'mode': mode, 'pull': pull
        }
        for var in self.IO_CONFIG_PARAMS.get(mode, []):
            config[var['name']] = ''
        self.current_config[pin_id] = config
        self.bgroup = []
        self.configure_io(None, pin_id)

    def _select_pull(self, button, pin_id):
        elements = [urwid.Text("Pull for IO{}".format(pin_id + 1)), urwid.Divider()]
        self.bgroup = []
        for choice in self.IO_PULL_OPTIONS:
            elements.append(urwid.Padding(attrmap(urwid.RadioButton(self.bgroup, choice)), width=13))
        # Toggle the configured state
        self.bgroup[self.IO_PULL_OPTIONS.index(self.current_config[pin_id]['pull'])].toggle_state()

        elements.extend([urwid.Divider(),
                         urwid.Padding(attrmap(urwid.Button('Back', on_press=self._on_pull_selected, user_data=pin_id)),
                                       width=8)
                        ])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(elements))
    
    def _on_pull_selected(self, button, pin_id):
        states = [c.state for c in self.bgroup]
        self.current_config[pin_id]['pull'] = self.IO_PULL_OPTIONS[states.index(True)]
        self.bgroup = []
        self.configure_io(None, pin_id)

    def _get_device_config(self, *args):
        config = []
        for i in range(self.IO_PINS_COUNT):
            result = pijuice.config.GetIoConfiguration(i + 1)
            if result['error'] != 'NO_ERROR':
                confirmation_dialog("Unable to connect to device: {}".format(result['error']), next=main_menu, single_option=True)
            else:
                config.append(result['data'])
        return config
    
    def _apply_settings(self, button, pin_id):
        if pin_id >= self.IO_PINS_COUNT:
            # Apply for all pins
            errors = []
            for i in range(self.IO_PINS_COUNT):
                error_msg = self._apply_for_pin(i)
                if error_msg != 'NO_ERROR':
                    errors.append(error_msg)
            if errors:
                confirmation_dialog("Failed to apply some settings. Error: {}".format(errors), next=self.main, single_option=True)
            else:
                confirmation_dialog("Updated settings for all pins", next=self.main, single_option=True)
        else:
            error_msg = self._apply_for_pin(pin_id)
            if error_msg != 'NO_ERROR':
                confirmation_dialog("Failed to apply settings for IO{}. Error: {}".format(pin_id + 1, error_msg), next=self.main, single_option=True)
            else:
                confirmation_dialog("Updated settings for IO{}".format(pin_id + 1), next=self.main, single_option=True)

    def _apply_for_pin(self, pin_id):
        result = pijuice.config.SetIoConfiguration(pin_id + 1, self.current_config[pin_id], True)
        return result.get('error', 'NO_ERROR')


class BatteryProfileTab(object):
    if pijuice:
        BATTERY_PROFILES = pijuice.config.batteryProfiles + ['CUSTOM', 'DEFAULT']
        TEMP_SENSE_OPTIONS = pijuice.config.batteryTempSenseOptions
        EDIT_KEYS = ['capacity', 'chargeCurrent', 'terminationCurrent', 'regulationVoltage', 'cutoffVoltage',
                     'tempCold', 'tempCool', 'tempWarm', 'tempHot', 'ntcB', 'ntcResistance']

    def __init__(self, *args):
        self.status_text = ""
        self.custom_values = False
        self.refresh()
    
    def main(self, *args):
        elements = [urwid.Text("Battery settings"),
                    urwid.Divider(),
                    urwid.Text("Status: " +  self.status_text),
                    urwid.Padding(attrmap(urwid.Button("Profile: {}".format(self.profile_name), on_press=self.select_profile)),
                                  width=25),
                    urwid.Divider(),
                    urwid.Padding(attrmap(urwid.CheckBox("Custom", state=self.custom_values,
                                                         on_state_change=self._toggle_custom_values)),
                                  width=32)
        ]

        self.param_edits = [
            urwid.IntEdit("Capacity [mAh]: ", default=self.profile_data['capacity']),
            urwid.IntEdit("Charge current [mA]: ", default=self.profile_data['chargeCurrent']),
            urwid.IntEdit("Termination current [mA]: ", default=self.profile_data['terminationCurrent']),
            urwid.IntEdit("Regulation voltage [mV]: ", default=self.profile_data['regulationVoltage']),
            urwid.IntEdit("Cutoff voltage [mV]: ", default=self.profile_data['cutoffVoltage']),
            urwid.IntEdit("Cold temperature [C]: ", default=self.profile_data['tempCold']),
            urwid.IntEdit("Cool temperature [C]: ", default=self.profile_data['tempCool']),
            urwid.IntEdit("Warm temperature [C]: ", default=self.profile_data['tempWarm']),
            urwid.IntEdit("Hot temperature [C]: ", default=self.profile_data['tempHot']),
            urwid.IntEdit("NTC B constant [1k]: ", default=self.profile_data['ntcB']),
            urwid.IntEdit("NTC resistance [ohm]: ", default=self.profile_data['ntcResistance']),
        ]

        for i, edit in enumerate(self.param_edits):
            urwid.connect_signal(edit, 'change', lambda x, text, idx: self.profile_data.update({self.EDIT_KEYS[idx]: text}), i)

        for i, edit in enumerate(self.param_edits):
            self.param_edits[i] = urwid.Padding(attrmap(edit), width=32)

        if self.custom_values:
            elements += self.param_edits
        else:
            elements += [
            urwid.Text("Capacity [mAh]: " + str(self.profile_data['capacity'])),
            urwid.Text("Charge current [mA]: " + str(self.profile_data['chargeCurrent'])),
            urwid.Text("Termination current [mA]: " + str(self.profile_data['terminationCurrent'])),
            urwid.Text("Regulation voltage [mV]: " + str(self.profile_data['regulationVoltage'])),
            urwid.Text("Cutoff voltage [mV]: " + str(self.profile_data['cutoffVoltage'])),
            urwid.Text("Cold temperature [C]: " + str(self.profile_data['tempCold'])),
            urwid.Text("Cool temperature [C]: " + str(self.profile_data['tempCool'])),
            urwid.Text("Warm temperature [C]: " + str(self.profile_data['tempWarm'])),
            urwid.Text("Hot temperature [C]: " + str(self.profile_data['tempHot'])),
            urwid.Text("NTC B constant [1k]: " + str(self.profile_data['ntcB'])),
            urwid.Text("NTC resistance [ohm]: " + str(self.profile_data['ntcResistance'])),
        ]

        elements.extend([urwid.Divider(),
                         urwid.Padding(
                             attrmap(urwid.Button("Temperature sense: {}".format(
                                 self.TEMP_SENSE_OPTIONS[self.temp_sense_profile_idx]), on_press=self.select_sense)),
                             width=34),
                         urwid.Divider(),
                         urwid.Padding(attrmap(urwid.Button("Refresh", on_press=self.refresh)), width=18),
                         urwid.Padding(attrmap(urwid.Button("Apply settings", on_press=self._apply_settings)), width=18),
                         urwid.Padding(attrmap(urwid.Button("Back", on_press=main_menu)), width=18)
                         ])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(elements))
    
    def refresh(self, *args):
        self._read_profile_status()
        self._read_profile_data()
        self._read_temp_sense()
        self.main()
    
    def _read_profile_data(self, *args):
        config = pijuice.config.GetBatteryProfile()
        if config['error'] == 'NO_ERROR':
            self.profile_data = config['data']
        else:
            confirmation_dialog("Unable to read battery data. Error: {}".format(config['error']), next=main_menu, single_option=True)
    
    def _read_profile_status(self, *args):
        self.profile_name = 'CUSTOM'
        self.status_text = ''
        status = pijuice.config.GetBatteryProfileStatus()
        if status['error'] == 'NO_ERROR':
            self.profile_status = status['data']

            if self.profile_status['validity'] == 'VALID':
                if self.profile_status['origin'] == 'PREDEFINED':
                    self.profile_name = self.profile_status['profile']
            else:
                self.status_text = 'Invalid profile'

            if self.profile_status['source'] == 'DIP_SWITCH' and self.profile_status['origin'] == 'PREDEFINED' and self.BATTERY_PROFILES.index(self.profile_name) == 1:
                self.status_text = 'Default profile'
            else:
                self.status_text = 'Custom profile by: ' if self.profile_status['origin'] == 'CUSTOM' else 'Profile selected by: '
                self.status_text += self.profile_status['source']
        else:
            confirmation_dialog("Unable to read battery data. Error: {}".format(
                status['error']), next=main_menu, single_option=True)
    
    def _read_temp_sense(self, *args):
        temp_sense_config = pijuice.config.GetBatteryTempSenseConfig()
        if temp_sense_config['error'] == 'NO_ERROR':
            self.temp_sense_profile_idx = self.TEMP_SENSE_OPTIONS.index(temp_sense_config['data'])
        else:
            confirmation_dialog("Unable to read battery data. Error: {}".format(
                temp_sense_config['error']), next=main_menu, single_option=True)

    def _clear_text_edits(self, *args):
        for edit in self.param_edits:
            edit.set_edit_text("")
    
    def _toggle_custom_values(self, *args):
        self.custom_values ^= True
        self.main()
    
    def select_profile(self, *args):
        body = [urwid.Text("Select battery profile"), urwid.Divider()]
        self.bgroup = []
        for choice in self.BATTERY_PROFILES:
            button = urwid.RadioButton(self.bgroup, choice)
            body.append(urwid.Padding(attrmap(button), width=16))
        self.bgroup[self.BATTERY_PROFILES.index(self.profile_name)].toggle_state()
        body.extend([urwid.Divider(),
                     urwid.Padding(attrmap(urwid.Button('Back', on_press=self._set_profile)), width=8)])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(body))
    
    def _set_profile(self, *args):
        states = [c.state for c in self.bgroup]
        self.profile_name = self.BATTERY_PROFILES[states.index(True)]
        self.bgroup = []
        self.main()

    def select_sense(self, *args):
        body = [urwid.Text("Select temperature sense"), urwid.Divider()]
        self.bgroup = []
        for choice in self.TEMP_SENSE_OPTIONS:
            button = urwid.RadioButton(self.bgroup, choice)
            body.append(urwid.Padding(attrmap(button), width=16))
        self.bgroup[self.temp_sense_profile_idx].toggle_state()
        body.extend([urwid.Divider(),
                     urwid.Padding(attrmap(urwid.Button('Back', on_press=self._set_sense)), width=8)])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(body))

    def _set_sense(self, *args):
        states = [c.state for c in self.bgroup]
        self.temp_sense_profile_idx = states.index(True)
        self.bgroup = []
        self.main()

    def _apply_settings(self, *args):
        status = pijuice.config.SetBatteryTempSenseConfig(self.TEMP_SENSE_OPTIONS[self.temp_sense_profile_idx])
        if status['error'] != 'NO_ERROR':
            confirmation_dialog('Failed to apply temperature sense options. Error: {}'.format(
                status['error']), next=main_menu, single_option=True)

        if self.custom_values:
            status = self.write_custom_values()
        else:
            status = pijuice.config.SetBatteryProfile(self.profile_name)

        if status['error'] != 'NO_ERROR':
            confirmation_dialog('Failed to apply profile options. Error: {}'.format(
                status['error']), next=main_menu, single_option=True)
        else:
            confirmation_dialog("Settings successfully updated", single_option=True, next=self.refresh)
    
    def write_custom_values(self, *args):
        # Keys for config are given in order of Edit widgets (self.param_edits)
        # edit_keys = ['capacity', 'chargeCurrent', 'terminationCurrent', 'regulationVoltage', 'cutoffVoltage',
        #              'tempCold', 'tempCool', 'tempWarm', 'tempHot', 'ntcB', 'ntcResistance']
        CHARGE_CURRENT_MIN = 550
        CHARGE_CURRENT_MAX = 2500
        TERMINATION_CURRENT_MIN = 50
        TERMINATION_CURRENT_MAX = 400
        REGULATION_VOLTAGE_MIN = 3500
        REGULATION_VOLTAGE_MAX = 4440

        profile = {}
        profile['capacity'] = int(self.param_edits[0].value())

        charge_current = int(self.param_edits[1].value())
        if charge_current < CHARGE_CURRENT_MIN:
            charge_current = CHARGE_CURRENT_MIN
        elif charge_current > CHARGE_CURRENT_MAX:
            charge_current = CHARGE_CURRENT_MAX
        profile['chargeCurrent'] = charge_current

        termination_current = int(self.param_edits[2].value())
        if termination_current < TERMINATION_CURRENT_MIN:
            termination_current = TERMINATION_CURRENT_MIN
        elif termination_current > TERMINATION_CURRENT_MAX:
            termination_current = TERMINATION_CURRENT_MAX
        profile['terminationCurrent'] = termination_current

        regulation_voltage = int(self.param_edits[3].value())
        if regulation_voltage < REGULATION_VOLTAGE_MIN:
            regulation_voltage = REGULATION_VOLTAGE_MIN
        elif regulation_voltage > REGULATION_VOLTAGE_MAX:
            regulation_voltage = REGULATION_VOLTAGE_MAX
        profile['regulationVoltage'] = regulation_voltage

        profile['cutoffVoltage'] = int(self.param_edits[4].value())
        profile['tempCold'] = int(self.param_edits[5].value())
        profile['tempCool'] = int(self.param_edits[6].value())
        profile['tempWarm'] = int(self.param_edits[7].value())
        profile['tempHot'] = int(self.param_edits[8].value())
        profile['ntcB'] = int(self.param_edits[9].value())
        profile['ntcResistance'] = int(self.param_edits[10].value())

        return pijuice.config.SetCustomBatteryProfile(profile)

    # def _show_current_config(self, *args):
    #     current_config = {
    #         'profile': self.profile_name,
    #         'profile_status': self.profile_status,
    #         'profile_data': self.profile_data,
    #         'status_text': self.status_text,
    #     }
    #     main.original_widget = urwid.Filler(urwid.Pile(
    #         [urwid.Text(str(current_config)), urwid.Button('Back', on_press=self.main)]))


class WakeupAlarmTab(object):
    def __init__(self, *args):
        try:
            self.current_config = self._get_alarm()
        except Exception as e:
            confirmation_dialog("Unable to connect to device: {}".format(
                str(e)), next=main_menu, single_option=True)
        else:
            self.status = 'OK'
            self.device_time = self._get_device_time()
            self.main()

    def _get_alarm(self, *args):
        status = {unit: {} for unit in ('day', 'hour', 'minute', 'second')}
        # Empty by default
        for unit in ('day', 'hour', 'minute', 'second'):
            status[unit]['value'] = ''

        ctr = pijuice.rtcAlarm.GetControlStatus()
        if ctr['error'] != 'NO_ERROR':
            raise Exception(ctr['error'])
        status['enabled'] = ctr['data']['alarm_wakeup_enabled']

        alarm = pijuice.rtcAlarm.GetAlarm()
        if alarm['error'] != 'NO_ERROR':
            raise Exception(alarm['error'])

        alarm = alarm['data']

        if 'day' in alarm:
            status['day']['type'] = 0  # Day number
            if alarm['day'] == 'EVERY_DAY':
                status['day']['every_day'] = True
            else:
                status['day']['every_day'] = False
                status['day']['value'] = alarm['day']
        elif 'weekday' in alarm:
            status['day']['type'] = 1  # Day of week number
            if alarm['weekday'] == 'EVERY_DAY':
                status['day']['every_day'] = True
            else:
                status['day']['every_day'] = False
                status['day']['value'] = alarm['weekday']

        if 'hour' in alarm:
            if alarm['hour'] == 'EVERY_HOUR':
                status['hour']['every_hour'] = True
            else:
                status['hour']['every_hour'] = False
                status['hour']['value'] = alarm['hour']

        if 'minute' in alarm:
            status['minute']['type'] = 0  # Minute
            status['minute']['value'] = alarm['minute']
        elif 'minute_period' in alarm:
            status['minute']['type'] = 1  # Minute period
            status['minute']['value'] = alarm['minute_period']

        if 'second' in alarm:
            status['second']['value'] = alarm['second']

        return status

    def _get_device_time(self, *args):
        device_time = ''
        t = pijuice.rtcAlarm.GetTime()
        if t['error'] == 'NO_ERROR':
            t = t['data']
            dt = datetime.datetime(t['year'], t['month'], t['day'], t['hour'], t['minute'], t['second'])
            dt_fmt = "%a %Y-%m-%d %H:%M:%S"
            device_time = dt.strftime(dt_fmt)
        else:
            device_time = t['error']

        s = pijuice.rtcAlarm.GetControlStatus()
        if s['error'] == 'NO_ERROR' and s['data']['alarm_flag']:
            self.status = 'Last: {}:{}:{}'.format(str(t['hour']).rjust(2, '0'),
                                                  str(t['minute']).rjust(2, '0'),
                                                  str(t['second']).rjust(2, '0'))
            pijuice.rtcAlarm.ClearAlarmFlag()
        return device_time

    def _update_time(self, *args):
        self.device_time = self._get_device_time()
        self.time_text.set_text("UTC Time: " + self.device_time)
        self.status_text.set_text("Status: " + self.status)
        self.alarm_handle = loop.set_alarm_in(1, self._update_time)

    def _set_alarm(self, *args):
        loop.remove_alarm(self.alarm_handle)
        alarm = {}
        if self.current_config['second'].get('value'):
            alarm['second'] = self.current_config['second']['value']

        if self.current_config['minute'].get('value'):
            if self.current_config['minute']['type'] == 0:
                alarm['minute'] = self.current_config['minute']['value']
            elif self.current_config['minute']['type'] == 1:
                alarm['minute_period'] = self.current_config['minute']['value']

        if self.current_config['hour'].get('value') or self.current_config['hour']['every_hour']:
            alarm['hour'] = 'EVERY_HOUR' if self.current_config['hour']['every_hour'] else self.current_config['hour']['value']

        if self.current_config['day'].get('value') or self.current_config['day']['every_day']:
            if self.current_config['day']['type'] == 0:
                alarm['day'] = 'EVERY_DAY' if self.current_config['day']['every_day'] else self.current_config['day']['value']
            elif self.current_config['day']['type'] == 1:
                alarm['weekday'] = 'EVERY_DAY' if self.current_config['day']['every_day'] else self.current_config['day']['value']

        status = pijuice.rtcAlarm.SetAlarm(alarm)

        if status['error'] != 'NO_ERROR':
            confirmation_dialog('Failed to apply wakeup alarm options. Error: {}'.format(
                status['error']), next=main_menu, single_option=True)
        else:
            confirmation_dialog('Wakeup alarm has been set successfully.', next=self.main, single_option=True)

    def _toggle_wakeup(self, checkbox, state, *args):
        self.current_config['enabled'] = state
        ret = pijuice.rtcAlarm.SetWakeupEnabled(state)
        if ret['error'] != 'NO_ERROR':
            self.current_config['enabled'] = not state
            checkbox.set_state(False, do_callback=False)
            confirmation_dialog('Failed to toggle alarm. Error: {}'.format(ret['error']), next=main_menu, single_option=True)

    def _set_time(self, *args):
        loop.remove_alarm(self.alarm_handle)
        t = datetime.datetime.utcnow()
        pijuice.rtcAlarm.SetTime({
            'second': t.second,
            'minute': t.minute,
            'hour': t.hour,
            'weekday': t.weekday() + 1,
            'day': t.day,
            'month': t.month,
            'year': t.year,
            'subsecond': t.microsecond // 1000000
        })
        self._update_time()

    def _set_day_type(self, rb, state, period_type):
        if state:
            self.current_config['day']['type'] = period_type

    def _set_minute_type(self, rb, state, period_type):
        if state:
            self.current_config['minute']['type'] = period_type

    def main(self, *args):
        self.time_text = urwid.Text("UTC Time: " + self._get_device_time())
        self.status_text = urwid.Text("Status: " + self.status)
        wakeup_cbox = urwid.Padding(attrmap(urwid.CheckBox("Wakeup enabled", state=self.current_config['enabled'],
                                     on_state_change=self._toggle_wakeup)), width=19)
        elements = [urwid.Text("Wakeup Alarm"),
                    urwid.Divider(),
                    self.status_text,
                    self.time_text,
                    wakeup_cbox,
                    urwid.Padding(attrmap(urwid.Button("Set RTC time", on_press=self._set_time)), width=19),
                    urwid.Divider(),
        ]
        self.day_bgroup = []
        day_radio = attrmap(urwid.RadioButton(self.day_bgroup, "Day", on_state_change=self._set_day_type, user_data=0))
        weekday_radio = attrmap(urwid.RadioButton(self.day_bgroup, "Weekday", on_state_change=self._set_day_type, user_data=1))
        self.day_bgroup[self.current_config['day']['type']].set_state(True, do_callback=False)
        day_type_row = urwid.Columns([urwid.Padding(day_radio, width=10), urwid.Padding(weekday_radio, width=15)])

        day_edit = urwid.Edit("Day: ", edit_text=str(self.current_config['day']['value']))
        urwid.connect_signal(day_edit, 'change', lambda x, text: self.current_config['day'].update({'value': text}))
        day_checkbox = urwid.CheckBox("Every day", state=self.current_config['day']['every_day'],
                                      on_state_change=lambda x, state: self.current_config['day'].update({'every_day': state}))
        day_value_row = urwid.Columns([urwid.Padding(attrmap(day_edit), width=10), urwid.Padding(attrmap(day_checkbox), width=15)])

        hour_edit = urwid.Edit("Hour: ", edit_text=str(self.current_config['hour']['value']))
        urwid.connect_signal(hour_edit, 'change', lambda x, text: self.current_config['hour'].update({'value': text}))
        hour_checkbox = urwid.CheckBox("Every hour", state=self.current_config['hour']['every_hour'],
                                      on_state_change=lambda x, state: self.current_config['hour'].update({'every_hour': state}))
        hour_value_row = urwid.Columns([urwid.Padding(attrmap(hour_edit), width=10), urwid.Padding(attrmap(hour_checkbox), width=15)])

        self.minute_bgroup = []
        minute_radio = attrmap(urwid.RadioButton(self.minute_bgroup, "Minute", on_state_change=self._set_minute_type, user_data=0))
        minute_period_radio = attrmap(urwid.RadioButton(
            self.minute_bgroup, "Minutes period", on_state_change=self._set_minute_type, user_data=1))
        self.minute_bgroup[self.current_config['minute']['type']].toggle_state()
        minute_type_row = urwid.Columns([urwid.Padding(minute_radio, width=11), urwid.Padding(minute_period_radio, width=19)])

        minute_edit = urwid.Edit("Minute: ", edit_text=str(self.current_config['minute']['value']))
        urwid.connect_signal(minute_edit, 'change', lambda x, text: self.current_config['minute'].update({'value': text}))

        second_edit = urwid.Edit("Second: ", edit_text=str(self.current_config['second']['value']))
        urwid.connect_signal(second_edit, 'change', lambda x, text: self.current_config['second'].update({'value': text}))

        elements += [
            day_type_row,
            day_value_row,
            hour_value_row,
            urwid.Divider(),
            minute_type_row,
            urwid.Padding(attrmap(minute_edit), width=11),
            urwid.Padding(attrmap(second_edit), width=11),
            urwid.Divider(),
            urwid.Padding(attrmap(urwid.Button("Set alarm", on_press=self._set_alarm)), width=13),
            urwid.Padding(attrmap(urwid.Button("Back", on_press=self._goto_main_menu)), width=13)
        ]
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(elements))
        self.alarm_handle = loop.set_alarm_in(1, self._update_time)

    def _goto_main_menu(self, *args):
        loop.remove_alarm(self.alarm_handle)
        main_menu()

class SystemTaskTab(object):
    def __init__(self, *args):
        global pijuiceConfigData
        if pijuiceConfigData == None:
            pijuiceConfigData = loadPiJuiceConfig()
        self.main()

    def main(self, *args):
        global pijuiceConfigData
        elements = [urwid.Text("System Task"),
                    urwid.Divider()
        ]

        ## System Task ##
        if not ('system_task' in pijuiceConfigData):
            pijuiceConfigData['system_task'] = {}
        if not ('enabled' in pijuiceConfigData['system_task']):
            pijuiceConfigData['system_task']['enabled'] = False
        elements.extend([attrmap(urwid.CheckBox('System task enabled',
                                  state=pijuiceConfigData['system_task']['enabled'],
                                  on_state_change = lambda x, state: pijuiceConfigData['system_task'].update({'enabled':state}))),
                         urwid.Divider()
                        ])

        ## Watchdog ##
        if not('watchdog' in pijuiceConfigData['system_task']):
            pijuiceConfigData['system_task']['watchdog'] = {}
        if not('enabled' in pijuiceConfigData['system_task']['watchdog']):
            pijuiceConfigData['system_task']['watchdog']['enabled'] = False
        self.wdenabled = pijuiceConfigData['system_task']['watchdog']['enabled']
        if not('period' in pijuiceConfigData['system_task']['watchdog']):
            pijuiceConfigData['system_task']['watchdog']['period'] = 4
        wdCheckBox = attrmap(urwid.CheckBox('Watchdog', state=self.wdenabled,
                             on_state_change=self._toggle_wdenabled))
        wdperiodEdit = urwid.IntEdit("Expire period [minutes]: ", default = pijuiceConfigData['system_task']['watchdog']['period']) 
        urwid.connect_signal(wdperiodEdit, 'change', self.validate_wdperiod)
        wdperiodEditItem = attrmap(wdperiodEdit)
        wdperiodTextItem = attrmap(urwid.Text("Expire period [minutes]: " + str(pijuiceConfigData['system_task']['watchdog']['period'])))
        wdperiodItem = wdperiodEditItem if self.wdenabled else wdperiodTextItem 
        wdperiodRow = urwid.Columns([urwid.Padding(wdCheckBox, width = 24), urwid.Padding(wdperiodItem, width=37)])
        elements.extend([wdperiodRow,
                        urwid.Divider()])

        ## Wakeup on charge ##
        if not('wakeup_on_charge' in pijuiceConfigData['system_task']):
            pijuiceConfigData['system_task']['wakeup_on_charge'] = {}
        if not('enabled' in pijuiceConfigData['system_task']['wakeup_on_charge']):
            pijuiceConfigData['system_task']['wakeup_on_charge']['enabled'] = False
        self.wkupenabled = pijuiceConfigData['system_task']['wakeup_on_charge']['enabled']
        if not('trigger_level' in pijuiceConfigData['system_task']['wakeup_on_charge']):
            pijuiceConfigData['system_task']['wakeup_on_charge']['trigger_level'] = 50
        wkupCheckBox = attrmap(urwid.CheckBox('Wakeup on charge', state=self.wkupenabled,
                          on_state_change=self._toggle_wkupenabled))
        wkuplevelEdit = urwid.IntEdit("Trigger level [%]: ", default = pijuiceConfigData['system_task']['wakeup_on_charge']['trigger_level'])
        urwid.connect_signal(wkuplevelEdit, 'change', self.validate_wkuplevel)
        wkuplevelEditItem = attrmap(wkuplevelEdit)
        wkuplevelTextItem = attrmap(urwid.Text("Trigger level [%]: " + str(pijuiceConfigData['system_task']['wakeup_on_charge']['trigger_level'])))
        wkuplevelItem = wkuplevelEditItem if self.wkupenabled else wkuplevelTextItem
        wkuplevelRow = urwid.Columns([urwid.Padding(wkupCheckBox, width = 24), urwid.Padding(wkuplevelItem, width=37)])
        elements.extend([wkuplevelRow,
                         urwid.Divider()])

        ## Minimum charge ##
        if not('min_charge' in pijuiceConfigData['system_task']):
            pijuiceConfigData['system_task']['min_charge'] = {}
        if not('enabled' in pijuiceConfigData['system_task']['min_charge']):
            pijuiceConfigData['system_task']['min_charge']['enabled'] = False
        self.minchgenabled = pijuiceConfigData['system_task']['min_charge']['enabled']
        if not('threshold' in pijuiceConfigData['system_task']['min_charge']):
            pijuiceConfigData['system_task']['min_charge']['threshold'] = 10
        minchgCheckBox = attrmap(urwid.CheckBox('Min charge', state=self.minchgenabled,
                          on_state_change=self._toggle_minchgenabled))
        thresholdEdit = urwid.IntEdit("Threshold [%]: ", default = pijuiceConfigData['system_task']['min_charge']['threshold'])
        urwid.connect_signal(thresholdEdit, 'change', self.validate_minchglevel)
        thresholdEditItem = attrmap(thresholdEdit)
        thresholdTextItem = attrmap(urwid.Text("Threshold [%]: " + str(pijuiceConfigData['system_task']['min_charge']['threshold'])))
        thresholdItem = thresholdEditItem if self.minchgenabled else thresholdTextItem
        thresholdRow = urwid.Columns([urwid.Padding(minchgCheckBox, width = 24), urwid.Padding(thresholdItem, width=37)])
        elements.extend([thresholdRow,
                         urwid.Divider()])

        ## Min Battery voltage ##
        if not('min_bat_voltage' in pijuiceConfigData['system_task']):
            pijuiceConfigData['system_task']['min_bat_voltage'] = {}
        if not('enabled' in pijuiceConfigData['system_task']['min_bat_voltage']):
            pijuiceConfigData['system_task']['min_bat_voltage']['enabled'] = False
        self.minbatvenabled = pijuiceConfigData['system_task']['min_bat_voltage']['enabled']
        if not('threshold' in pijuiceConfigData['system_task']['min_bat_voltage']):
            pijuiceConfigData['system_task']['min_bat_voltage']['threshold'] = 3.3
        minbatvCheckBox = attrmap(urwid.CheckBox('Min battery voltage', state=self.minbatvenabled,
                           on_state_change=self._toggle_minbatvenabled))
        vthresholdEdit = FloatEdit(default = str(pijuiceConfigData['system_task']['min_bat_voltage']['threshold']))
        urwid.connect_signal(vthresholdEdit, 'change', self.validate_minbatvlevel)
        vthresholdEditItem = attrmap(vthresholdEdit)
        vthresholdTextItem = attrmap(urwid.Text(str(pijuiceConfigData['system_task']['min_bat_voltage']['threshold'])))
        vthresholdItem = vthresholdEditItem if self.minbatvenabled else vthresholdTextItem
        vthresholdRow = urwid.Columns([urwid.Padding(minbatvCheckBox, width = 24), urwid.Padding(vthresholdItem, width=37)])
        elements.extend([vthresholdRow,
                         urwid.Divider()])

        ## Software Halt Power Off ##
        if not('ext_halt_power_off' in pijuiceConfigData['system_task']):
            pijuiceConfigData['system_task']['ext_halt_power_off'] = {}
        if not('enabled' in pijuiceConfigData['system_task']['ext_halt_power_off']):
            pijuiceConfigData['system_task']['ext_halt_power_off']['enabled'] = False
        self.exthaltenabled = pijuiceConfigData['system_task']['ext_halt_power_off']['enabled']
        if not('period' in pijuiceConfigData['system_task']['ext_halt_power_off']):
            pijuiceConfigData['system_task']['ext_halt_power_off']['period'] = 10
        exthaltCheckBox = attrmap(urwid.CheckBox('Software Halt Power Off', state=self.exthaltenabled,
                          on_state_change=self._toggle_exthaltenabled))
        periodEdit = urwid.IntEdit("Delay period [seconds]: ", default = pijuiceConfigData['system_task']['ext_halt_power_off']['period'])
        urwid.connect_signal(periodEdit, 'change', self.validate_exthaltdelay)
        periodEditItem = attrmap(periodEdit)
        periodTextItem = attrmap(urwid.Text("Delay period [seconds]: " + str(pijuiceConfigData['system_task']['ext_halt_power_off']['period'])))
        periodItem = periodEditItem if self.exthaltenabled else periodTextItem
        periodRow = urwid.Columns([urwid.Padding(exthaltCheckBox, width = 24), urwid.Padding(periodItem, width=37)])
        elements.extend([periodRow,
                         urwid.Divider()])

        ## Footer ##
        elements.extend([urwid.Padding(attrmap(urwid.Button("Refresh", on_press=self.refresh)), width=18),
                         urwid.Padding(attrmap(urwid.Button("Apply settings", on_press=savePiJuiceConfig)), width=18),
                         urwid.Padding(attrmap(urwid.Button("Back", on_press=main_menu)), width=18)
                         ])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(elements))

    def refresh(self, *args):
        global pijuiceConfigData
        pijuiceConfigData = loadPiJuiceConfig()
        self.main()

    def _toggle_wdenabled(self, *args):
        global pijuiceConfigData
        self.wdenabled ^= True
        pijuiceConfigData['system_task']['watchdog']['enabled'] = self.wdenabled
        self.main()

    def _toggle_wkupenabled(self, *args):
        global pijuiceConfigData
        self.wkupenabled ^= True
        pijuiceConfigData['system_task']['wakeup_on_charge']['enabled'] = self.wkupenabled
        self.main()

    def _toggle_minchgenabled(self, *args):
        global pijuiceConfigData
        self.minchgenabled ^= True
        pijuiceConfigData['system_task']['min_charge']['enabled'] = self.minchgenabled
        self.main()

    def _toggle_minbatvenabled(self, *args):
        global pijuiceConfigData
        self.minbatvenabled ^= True
        pijuiceConfigData['system_task']['min_bat_voltage']['enabled'] = self.minbatvenabled
        self.main()

    def _toggle_exthaltenabled(self, *args):
        global pijuiceConfigData
        self.exthaltenabled ^= True
        pijuiceConfigData['system_task']['ext_halt_power_off']['enabled'] = self.exthaltenabled
        self.main()

    def validate_wdperiod(self, widget, newtext):
        if newtext == '':
            newtext = '0'
        text = validate_value(newtext, 'int', 1, 65535,
                              pijuiceConfigData['system_task']['watchdog']['period'])
        pijuiceConfigData['system_task']['watchdog']['period'] = text
        if text != newtext:
            self.main()

    def validate_wkuplevel(self, widget, newtext):
        if newtext == "":
            newtext = "0"
        text = validate_value(newtext, 'int', 1, 100,
                              pijuiceConfigData['system_task']['wakeup_on_charge']['trigger_level'])
        pijuiceConfigData['system_task']['wakeup_on_charge']['trigger_level'] = text
        if text != newtext:
            self.main()

    def validate_minchglevel(self, widget, newtext):
        if newtext == "":
            newtext = "0"
        text = validate_value(newtext, 'int', 1, 100,
                              pijuiceConfigData['system_task']['min_charge']['threshold'])
        pijuiceConfigData['system_task']['min_charge']['threshold'] = text
        if text != newtext:
            self.main()

    def validate_minbatvlevel(self, widget, newtext):
        if newtext == "":
            newtext = "0.0"
        text = validate_value(newtext, 'float', 0.01, 10.0,
                              pijuiceConfigData['system_task']['min_bat_voltage']['threshold'])
        pijuiceConfigData['system_task']['min_bat_voltage']['threshold'] = text
        if float(text) != float(newtext):
            self.main()

    def validate_exthaltdelay(self, widget, newtext):
        if newtext == "":
            newtext = "0"
        text = validate_value(newtext, 'int', 20, 65535,
                              pijuiceConfigData['system_task']['ext_halt_power_off']['period'])
        pijuiceConfigData['system_task']['ext_halt_power_off']['period'] = text
        if text != newtext:
            self.main()


class SystemEventsTab(object):
    EVENTS = ['low_charge', 'low_battery_voltage', 'no_power', 'watchdog_reset', 'button_power_off', 'forced_power_off',
              'forced_sys_power_off']
    EVTTXT = ['Low charge', 'Low battery voltage', 'No power', 'Watchdog reset', 'Button power off', 'Forced power off',
              'Forced sys power off']
    FUNCTIONS = ['NO_FUNC'] + pijuice_sys_functions + pijuice_user_functions

    def __init__(self, *args):
        global pijuiceConfigData
        if pijuiceConfigData == None:
            pijuiceConfigData = loadPiJuiceConfig()
        if not ('system_events' in pijuiceConfigData):
            pijuiceConfigData['system_events'] = {}
        for event in self.EVENTS:
            if not(event in pijuiceConfigData['system_events']):
                pijuiceConfigData['system_events'][event] = {}
            if not('enabled' in pijuiceConfigData['system_events'][event]):
                pijuiceConfigData['system_events'][event]['enabled'] = False
            if not('function' in pijuiceConfigData['system_events'][event]):
                pijuiceConfigData['system_events'][event]['function'] = 'NO_FUNC'
        self.main()

    def main(self, *args):
        global pijuiceConfigData
        elements = [urwid.Text('System Events'),
                    urwid.Divider()
        ]

        for i, event in enumerate(self.EVENTS):
            eventchkbox = urwid.CheckBox(self.EVTTXT[i]+':', state=pijuiceConfigData['system_events'][event]['enabled'],
                             on_state_change=self._toggle_eventenabled, user_data=event)
            eventitem = attrmap(eventchkbox)
            func = pijuiceConfigData['system_events'][event]['function']
            fbutton = attrmap(urwid.Button(func, on_press = self.set_function, user_data = [i, func]))
            ftext   = attrmap(urwid.Text('  ' + func))
            funcitem = fbutton if eventchkbox.state else ftext
            row = urwid.Columns([urwid.Padding(eventitem, width=25), urwid.Padding(funcitem, width = 25)])
            elements.append(row)
        elements.append(urwid.Divider())

        ## Footer ##
        elements.extend([urwid.Padding(attrmap(urwid.Button('Refresh', on_press=self.refresh)), width=18),
                         urwid.Padding(attrmap(urwid.Button('Apply settings', on_press=savePiJuiceConfig)), width=18),
                         urwid.Padding(attrmap(urwid.Button('Back', on_press=main_menu)), width=18)
                         ])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(elements))

    def refresh(self, *args):
        global pijuiceConfigData
        pijuiceConfigData = loadPiJuiceConfig()
        self.main()

    def _toggle_eventenabled(self, widget, state, event):
        pijuiceConfigData['system_events'][event]['enabled'] = state
        self.main()

    def set_function(self, button, data):
        global pijuiceConfigData
        index = data[0]
        func = data[1]
        elements = [urwid.Text("Select function for '"+self.EVTTXT[index]+"'"),
                    urwid.Divider()]
        self.bgroup = []
        for function in  self.FUNCTIONS:
            button = attrmap(urwid.RadioButton(self.bgroup, function))
            elements.append(button)
        self.bgroup[self.FUNCTIONS.index(func)].toggle_state()
        elements.extend([urwid.Divider(),
                         urwid.Padding(attrmap(urwid.Button('Back', on_press=self._on_function_chosen, user_data=index)), width=8)
                        ])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(elements))

    def _on_function_chosen(self, button, index):
        states = [c.state for c in self.bgroup]
        pijuiceConfigData['system_events'][self.EVENTS[index]]['function']=self.FUNCTIONS[states.index(True)]
        self.bgroup=[]
        self.main()

USER_FUNCS_TOTAL=15
class UserScriptsTab(object):
    def __init__(self, *args):
        global pijuiceConfigData
        if pijuiceConfigData == None:
            pijuiceConfigData = loadPiJuiceConfig()
        if not ('user_functions' in pijuiceConfigData):
            pijuiceConfigData['user_functions'] = {}
        for i in range(USER_FUNCS_TOTAL):
            fkey = 'USER_FUNC' + str(i+1)
            if not (fkey in pijuiceConfigData['user_functions']):
                pijuiceConfigData['user_functions'][fkey] = ''
        self.main()

    def main(self, *args):
        global pijuiceConfigData
        elements = [urwid.Text("User Scripts"),
                    urwid.Divider()
        ]

        for i in range(USER_FUNCS_TOTAL):
            flabel = 'USER FUNC' + str(i+1) + ': '
            fkey = 'USER_FUNC' + str(i+1)
            edititem = urwid.Edit(flabel, edit_text=pijuiceConfigData['user_functions'][fkey])
            urwid.connect_signal(edititem, 'change', self.updatetext, user_args = [fkey,])
            elements.append(attrmap(edititem))
        elements.append(urwid.Divider())

        ## Footer ##
        elements.extend([urwid.Padding(attrmap(urwid.Button("Refresh", on_press=self.refresh)), width=18),
                         urwid.Padding(attrmap(urwid.Button("Apply settings", on_press=savePiJuiceConfig)), width=18),
                         urwid.Padding(attrmap(urwid.Button("Back", on_press=main_menu)), width=18)
                         ])
        main.original_widget = urwid.ListBox(urwid.SimpleFocusListWalker(elements))

    def updatetext(self,  key, widget, text):
        global pijuiceConfigData
        pijuiceConfigData['user_functions'][key] = text

    def refresh(self, *args):
        global pijuiceConfigData
        pijuiceConfigData = loadPiJuiceConfig()
        self.main()

class SystemSettingsTab(object):
    def __init__(self):
        self.current_config = self.read_config_file()
        self.main()

    def read_config_file(self, *args):
        try:
            with open(CONFIG_PATH, 'r') as config_file:
                config = json.load(config_file)
            return config
        except IOError as e:  # No file or insufficient permissions
            confirmation_dialog("Unable to read system configuration file. Reason: {}".format(e.strerror), next=main_menu)
            return {}

    def apply_config(self, *args):
        try:
            if not os.path.exists(os.path.dirname(CONFIG_PATH)):
                os.makedirs(os.path.dirname(CONFIG_PATH))
            with open(CONFIG_PATH, 'w+') as output_config_file:
                json.dump(self.current_config, output_config_file, indent=2)
        except IOError as e:  # Insufficient permissions
            confirmation_dialog("Unable to write to system configuration file. Reason: {}".format(e.strerror), next=main_menu)
            return

        # Notify service about changes
        try:
            pid = int(open(PID_FILE, 'r').read())
            os.kill(pid, signal.SIGHUP)
        except OSError:
            os.system("sudo kill -s SIGHUP %i" % pid)
        except:
            confirmation_dialog("Failed to communicate with PiJuice service.", next=main_menu)

    def main(self, *args):
        elements = [urwid.Text("System Settings"),
                    urwid.Divider(),
                    urwid.Button("System Task", on_press=self.system_task),
                    urwid.Button("System Events", on_press=self.system_events),
                    urwid.Button("User Scripts", on_press=self.user_scripts),
                    urwid.Divider(),
                    urwid.Button("Apply Settings", on_press=self.apply_config),
                    urwid.Button("Show current config", on_press=self._show_current_config),
                    urwid.Button("Back", on_press=main_menu),
                    ]
        main.original_widget = urwid.Padding(urwid.ListBox(
            urwid.SimpleFocusListWalker(elements)), left=2, right=2)

    def system_task(self, btn=None, focus=1):
        elements = [urwid.Text("System Task")]
        sys_task_enabled_cbox = urwid.CheckBox(
            "System task enabled", state=self.current_config['system_task'].get('enabled', False),
            on_state_change=lambda x, state: self.current_config['system_task'].update({'enabled': state}))
        elements += [sys_task_enabled_cbox, urwid.Divider()]

        # Key, Title, Value key, Label
        config_entries = (('watchdog', "Watchdog", 'period', "Expire period [minutes]: "),
                          ('wakeup_on_charge', "Wakeup on charge", 'trigger_level', "Trigger level [%]: "),
                          ('min_charge', "Minimum charge", 'threshold', "Threshold [%]: "),
                          ('min_bat_voltage', "Minimum battery voltage", 'threshold', "Threshold [V]: "))

        for option_key, title, value_key, edit_label in config_entries:
            cbox = urwid.CheckBox(title, state=self.current_config['system_task'].get(option_key, {}).get('enabled', False),
                                  on_state_change=self._toggle_system_task, user_data=[str(option_key), str(value_key)])
            elements.append(cbox)
            if self.current_config['system_task'].get(option_key, {}).get('enabled'):
                value_widget = urwid.Edit(edit_label, edit_text=self.current_config['system_task'].get(option_key, {}).get(value_key, ''))
                urwid.connect_signal(value_widget, 'change', self._update_sys_task_value, [str(option_key), str(value_key)])
                elements += [value_widget, urwid.Divider()]

        elements += [urwid.Divider(),
                     urwid.Button("Apply Settings",
                                  on_press=self.apply_config),
                     urwid.Button("Back", on_press=self.main)]

        self.list_walker = urwid.SimpleFocusListWalker(elements)
        self.list_walker.set_focus(focus)
        main.original_widget = urwid.Padding(urwid.ListBox(self.list_walker), left=2, right=2)

    def _toggle_system_task(self, element, state, keys):
        option_key = keys[0]
        value_key = keys[1]
        if option_key not in self.current_config['system_task'].keys():
            # Option had not been set
            self.current_config['system_task'][option_key] = {'enabled': state, value_key: ''}
        else:
            self.current_config['system_task'][option_key]['enabled'] = state
            if value_key not in self.current_config['system_task'][option_key].keys():
                self.current_config['system_task'][option_key][value_key] = ''

        # Keep focus on clicked element
        self.system_task(focus=self.list_walker.get_focus()[1])

    def _update_sys_task_value(self, element, text, keys):
        option_key = keys[0]
        value_key = keys[1]
        self.current_config['system_task'][option_key][value_key] = text

    def system_events(self, btn=None, focus=1):
        ALL_FUNCTIONS = ['NO_FUNC'] + pijuice_sys_functions + pijuice_user_functions  # battery_voltage, low_charge, no_power
        USER_FUNCTIONS = ['NO_FUNC'] + pijuice_user_functions  # button_power_off, forced_power_off, forced_sys_power_off, watchdog_reset 

        elements = [urwid.Text("System Events"), urwid.Divider()]

        # Key, Label, Value type (0 - all, 1 - user)
        config_entries = (('low_charge', "Low charge", 0),
                          ('battery_voltage', "Low battery voltage", 0),
                          ('no_power', "No power", 0),
                          ('watchdog_reset', "Watchdog reset", 1),
                          ('button_power_off', "Button power off", 1),
                          ('forced_power_off', "Forced power off", 1),
                          ('forced_sys_power_off', "Forced sys power off", 1))

        system_events_config = self.current_config.get('system_events', {})
        for key, label, value_type in config_entries:
            options = list(ALL_FUNCTIONS) if value_type == 0 else list(USER_FUNCTIONS)
            event_enabled = system_events_config.get(key, {}).get('enabled', False)
            event_function = system_events_config.get(key, {}).get('function', 'NO_FUNC')
            cbox = urwid.CheckBox(label, state=event_enabled, on_state_change=self._toggle_system_event, user_data=key)
            if event_enabled:
                value_widget = urwid.Button(event_function, on_press=self._select_event_function, user_data={'options': options, 'event': key})
            else:
                value_widget = urwid.Text(event_function)
            row = urwid.Columns([cbox, value_widget])
            elements.append(row)

        elements += [urwid.Divider(),
                     urwid.Button("Apply Settings",
                                  on_press=self.apply_config),
                     urwid.Button("Back", on_press=self.main)]

        self.list_walker = urwid.SimpleFocusListWalker(elements)
        self.list_walker.set_focus(focus)
        main.original_widget = urwid.Padding(urwid.ListBox(self.list_walker), left=2, right=2)

    def _toggle_system_event(self, element, state, key):
        if 'system_events' not in self.current_config.keys():
            self.current_config['system_events'] = {}

        if key not in self.current_config['system_events'].keys():
            self.current_config['system_events'][key] = {}
        self.current_config['system_events'][key]['enabled'] = state

        if not self.current_config['system_events'][key].get('function'):
            self.current_config['system_events'][key]['function'] = 'NO_FUNC'

        self.system_events(focus=self.list_walker.get_focus()[1])

    def _select_event_function(self, btn, user_data):
        options = user_data['options']
        self.event = user_data['event']
        elements = [urwid.Text("Choose function for {}".format(self.event)), urwid.Divider()]

        self.bgroup = []
        for option in options:
            func_btn = urwid.RadioButton(self.bgroup, option, on_state_change=self._set_event_function)
            elements.append(func_btn)

        selected_function = self.current_config.get('system_events', {}).get(self.event, {}).get('function', 'NO_FUNC')
        self.bgroup[options.index(selected_function)].toggle_state()

        elements += [urwid.Divider(), urwid.Button('Back', on_press=self.system_events)]

        main.original_widget = urwid.Padding(urwid.ListBox(
            urwid.SimpleFocusListWalker(elements)), left=2, right=2)

    def _set_event_function(self, rb, state):
        if state:
            if 'system_events' not in self.current_config.keys():
                self.current_config['system_events'] = {}

            if self.event not in self.current_config['system_events'].keys():
                self.current_config['system_events'][self.event] = {'enabled': True, 'function': 'NO_FUNC'}
            self.current_config['system_events'][self.event]['function'] = rb.get_label()

            del self.event
            del self.bgroup

    def user_scripts(self, *args):
        elements = [urwid.Text("User Scripts"), urwid.Divider()]

        for i in range(1, 16):
            func_name = "USER_FUNC" + str(i)
            func_edit = urwid.Edit(func_name + ": ", edit_text=self.current_config.get('user_functions', {}).get(func_name, ''))
            elements.append(func_edit)
            urwid.connect_signal(func_edit, 'change', self._set_user_function, func_name)

        elements += [urwid.Divider(),
                     urwid.Button("Apply Settings",
                                  on_press=self.apply_config),
                     urwid.Button("Back", on_press=self.main)]
        main.original_widget = urwid.Padding(urwid.ListBox(
            urwid.SimpleFocusListWalker(elements)), left=2, right=2)

    def _set_user_function(self, edit, text, func_name):
        if self.current_config.get('user_functions') is None:
            self.current_config['user_functions'] = {}
        self.current_config['user_functions'][func_name] = text

    def _show_current_config(self, *args):
        main.original_widget = urwid.Filler(urwid.Pile(
            [urwid.Text(json.dumps(self.current_config, indent=2)), urwid.Button('Back', on_press=self.main)]))


def menu(title, choices):
    body = [urwid.Text(title), urwid.Divider()]
    for c in choices:
        if c != "":
            button = urwid.Button(c)
            urwid.connect_signal(button, 'click', item_chosen, c)
            #wrapped_button = urwid.Padding(urwid.AttrMap(button, None, focus_map='reversed'), width=20)
            wrapped_button = urwid.Padding(attrmap(button), width=20)
            body.append(wrapped_button)
        else:
            body.append(urwid.Divider())
    return urwid.ListBox(urwid.SimpleFocusListWalker(body))


def item_chosen(button, choice):
    callback = menu_mapping[choice]
    callback()


def not_implemented_yet(*args):
    main_menu_btn = urwid.Button('Back', on_press=main_menu)
    main.original_widget = urwid.Filler(
        urwid.Pile([urwid.Text("Not implemented yet"), urwid.Divider(), main_menu_btn]))


def main_menu(*args):
    main.original_widget = menu("PiJuice HAT Configuration", choices)


def exit_program(button=None):
    raise urwid.ExitMainLoop()

def attrmap(w):
    return urwid.AttrMap(w, None, focus_map='reversed')

def loadPiJuiceConfig():
    try:
        with open(PiJuiceConfigDataPath, 'r') as outputConfig:
            pijuiceConfigData = json.load(outputConfig)
    except:
        pijuiceConfigData = {}
    return pijuiceConfigData

def savePiJuiceConfig(*args):
    try:
        with open(PiJuiceConfigDataPath, 'w+') as outputConfig:
            json.dump(pijuiceConfigData, outputConfig, indent=2)
        ret = notify_service(*args)
        text = "Settings saved"
        if ret != 0:
            text += ("\n\nFailed to communicate with PiJuice service.\n"
                    "See system logs and 'systemctl status pijuice.service' for details.")
        confirmation_dialog(text, next=main_menu)
    except:
        confirmation_dialog("Failed to save settings to " + PiJuiceConfigDataPath + "\n"
                            "Check permissions of " + PiJuiceConfigDataPath, next=main_menu)

def notify_service(*args):
    ret = -1
    try:
        pid = int(open(PID_FILE, 'r').read())
        ret = os.system("sudo kill -SIGHUP " + str(pid) + " > /dev/null 2>&1")
    except:
        pass
    return ret

menu_mapping = {
    "Status": StatusTab,
    "General": GeneralTab,
    "Buttons": ButtonsTab,
    "LEDs": LEDTab,
    "Battery profile": BatteryProfileTab,
    "IO": IOTab,
    "Wakeup Alarm": WakeupAlarmTab,
    "Firmware": FirmwareTab,
    "System Task": SystemTaskTab,
    "System Events": SystemEventsTab,
    "User Scripts": UserScriptsTab,
    "Exit": exit_program
}

# Use list of entries to set order
choices = ["Status", "General", "Buttons", "LEDs", "Battery profile", "IO", "Wakeup Alarm",
           "Firmware", "", "System Task", "System Events", "User Scripts", "", "Exit"]

nolock = False
lock_file = open(LOCK_FILE, 'w')
try:
    fcntl.flock(lock_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
except IOError:
    nolock = True

def exit_cli(*args):
    raise urwid.ExitMainLoop()

if pijuice is None:
    elements = [urwid.Padding(urwid.Text('No PiJuice found or Failed to use I2C bus.\nCheck if I2C is enabled',
                                         align='center')),
                urwid.Divider()]
    elements.append(urwid.Padding(attrmap(urwid.Button("OK", on_press=exit_cli)), width=6, align='center'))
    main = urwid.Filler(urwid.Pile(elements))
elif nolock:
    elements = [urwid.Padding(urwid.Text('Another instance of PiJuice Settings is already running',
                                         align='center')),
                urwid.Divider()]
    elements.append(urwid.Padding(attrmap(urwid.Button("OK", on_press=exit_cli)), width=6, align='center'))
    main = urwid.Filler(urwid.Pile(elements))
else:
    pijuiceConfigData = None
    main = urwid.Padding(menu("PiJuice HAT Configuration", choices), left=2, right=2)

top = urwid.Overlay(urwid.LineBox(main, title='PiJuice CLI'), urwid.SolidFill(u'\N{LIGHT SHADE}'),
                    align='center', width=64,
                    valign='middle', height=20)
loop = urwid.MainLoop(top, palette=[('reversed', 'standout', '')])
loop.run()