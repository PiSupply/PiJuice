#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import re
import time

import urwid
import pijuice as PiJuice

BUS = 1
ADDRESS = 0x14
MAIN_MENU_TITLE = u'PiJuice HAT CLI'
FIRMWARE_UPDATE_ERRORS = ['NO_ERROR', 'I2C_BUS_ACCESS_ERROR', 'INPUT_FILE_OPEN_ERROR', 'STARTING_BOOTLOADER_ERROR', 'FIRST_PAGE_ERASE_ERROR',
                          'EEPROM_ERASE_ERROR', 'INPUT_FILE_READ_ERROR', 'PAGE_WRITE_ERROR', 'PAGE_READ_ERROR', 'PAGE_VERIFY_ERROR', 'CODE_EXECUTE_ERROR']

try:
    pijuice = PiJuice.PiJuice(BUS, ADDRESS)
except:
    pijuice = None


def version_to_str(number):
    # Convert int version to str {major}.{minor}
    return "{}.{}".format(number >> 4, number & 15)


def confirmation_dialog(text, next, single_option=True):
    elements = [urwid.Text(text), urwid.Divider()]
    if single_option:
        elements.append(urwid.Button("OK", on_press=next))
    else:
        yes_btn = urwid.Button("Yes")
        no_btn = urwid.Button("No")
        urwid.connect_signal(yes_btn, 'click', next, True)
        urwid.connect_signal(no_btn, 'click', next, False)
        elements.extend([yes_btn, no_btn])

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
        
        usb_power = status.get('power_input', 0)

        io_voltage = float(pijuice.status.GetIoVoltage().get('data', 0))  # mV
        io_current = float(pijuice.status.GetIoCurrent().get('data', 0))  # mA
        gpio_info = "%.3fV, %.3fA, %s" % (io_voltage / 1000, io_current / 1000, status['powerInput5vIo'])

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


    def main(self, *args):
        status_args = self.get_status()
        text = urwid.Text("HAT status\n\n"
                        "Battery: {battery}\nGPIO power input: {gpio}\n"
                        "USB Micro power input: {usb}\nFault: {fault}\n"
                        "System switch: {sys_sw}\n".format(**status_args))
        refresh_btn = urwid.Button('Refresh', on_press=self.main)
        main_menu_btn = urwid.Button('Back', on_press=main_menu)
        pwr_switch_btn = urwid.Button('Change Power switch', on_press=self.change_power_switch)
        main.original_widget = urwid.Filler(
            urwid.Pile([text, urwid.Divider(), refresh_btn, pwr_switch_btn, main_menu_btn]))


    def change_power_switch(self, *args):
        elements = [urwid.Text("Choose value for System Power switch"), urwid.Divider()]
        values = [0, 500, 2100]
        for value in values:
            text = str(value) + " mA" if value else "Off"
            elements.append(urwid.Button(text, on_press=self.set_power_switch, user_data=value))
        elements.extend([urwid.Divider(),
                        urwid.Button("Back", on_press=self.main)])
        main.original_widget = urwid.Filler(urwid.Pile(elements))


    def set_power_switch(self, button, value):
        pijuice.power.SetSystemPowerSwitch(int(value))
        self.main()


class FirmwareTab(object):
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

    def update_firmware_start(self, path):
        device_status = pijuice.status.GetStatus()

        if device_status['error'] == 'NO_ERROR':
            if device_status['data']['powerInput'] != 'PRESENT' and \
                device_status['data']['powerInput5vIo'] != 'PRESENT' and \
                pijuice.status.GetChargeLevel().get('data', 0) < 20:
                # Charge level is too low
                return confirmation_dialog("Charge level is too low", next=self.show_firmware, single_option=True)
        confirmation_dialog("Are you sure you want to update the firmware?", next=self.update_firmware)

    def update_firmware(self, button=None):
        current_addr = pijuice.config.interface.GetAddress()
        error_status = None
        if current_addr:
            main.original_widget = urwid.Filler(urwid.Pile([urwid.Text("Updating firmware. Interrupting this process can lead to non-functional device.")]))
            addr = format(current_addr, 'x')
            result = 256 - subprocess.call(['pijuiceboot', addr, path])
            if result != 256:
                error_status = FIRMWARE_UPDATE_ERRORS[result] if result < 11 else 'UNKNOWN'
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
            message = "Firmware update successful"

        confirmation_dialog(message, single_option=True, next=self.show_firmware)
    
    def show_firmware(self, button=None):
        current_version, latest_version, firmware_status, firmware_path = self.get_fw_status()
        current_version_txt = urwid.Text("Current version: " + version_to_str(current_version))
        status_txt = urwid.Text("Status: " + firmware_status)
        elements = [urwid.Text("Firmware"), urwid.Divider(), current_version_txt, status_txt, urwid.Divider()]
        if latest_version > current_version:
            self.firmware_path = firmware_path
            elements.append(urwid.Button('Update', on_press=self.update_firmware))
        elements.append(urwid.Button('Back', on_press=main_menu))
        main.original_widget = urwid.Filler(urwid.Pile(elements))


class GeneralTab(object):
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
        elements.append(i2c_addr_edit)

        # I2C address RTC
        i2c_addr_rtc_edit = urwid.Edit("I2C address RTC: ", edit_text=str(self.current_config['i2c_addr_rtc']))
        urwid.connect_signal(i2c_addr_rtc_edit, 'change', lambda x, text: self.current_config.update({'i2c_addr_rtc': text}))
        elements.append(i2c_addr_rtc_edit)

        for option in options_with_checkboxes:
            elements.append(urwid.CheckBox(option['title'], state=self.current_config[option['key']],
                                           on_state_change=lambda x, state, key: self.current_config.update({key: state}),
                                           user_data=option['key']))

        for option in options_with_lists:
            elements.append(urwid.Button("{title}: {value}".format(title=option['title'], value=option['list'][self.current_config[option['key']]]),
                                         on_press=self._list_options, user_data=option))

        elements.extend([urwid.Divider(), urwid.Button("Apply settings", on_press=self._apply_settings),
                         urwid.Button('Back', on_press=main_menu),
                         urwid.Button("Show config", on_press=self._show_current_config),  # XXX: For debug purposes
                         ])
        main.original_widget = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(elements)), left=2, right=2)
    
    def _list_options(self, button, data):
        body = [urwid.Text(data['title']), urwid.Divider()]
        self.bgroup = []
        for choice in data['list']:
            button = urwid.RadioButton(self.bgroup, choice)
            body.append(button)
        self.bgroup[self.current_config[data['key']]].toggle_state()
        body.extend([urwid.Divider(), urwid.Button('Back', on_press=self._set_option, user_data=data)])
        main.original_widget = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(body)), left=2, right=2)
    
    def _set_option(self, button, data):
        states = [c.state for c in self.bgroup]
        self.current_config[data['key']] = states.index(True)
        self.bgroup = []
        self.main()

    def _show_current_config(self, *args):
        main.original_widget = urwid.Filler(urwid.Pile([urwid.Text(str(self.current_config)), urwid.Button('Back', on_press=self.main)]))
    
    def _apply_settings(self, *args):
        device_config = self._get_device_config()
        changed = [key for key in self.current_config.keys() if self.current_config[key] != device_config[key]]

        if 'run_pin' in changed:
            pijuice.config.SetRunPinConfig(self.current_config['run_pin'])

        # XXX: unstable
        # for i, addr in enumerate('i2c_addr', 'i2c_addr_rtc'):
        #     if addr in changed:
        #         value = self.device_config[addr]
        #         try:
        #             new_value = int(str(self.current_config[addr]), 16)
        #             if new_value <= 0x7F:
        #                 value = self.current_config[addr]
        #         except:
        #             pass
        #         pijuice.config.SetAddress(i + 1, value)

        if 'eeprom_addr' in changed:
            pijuice.config.SetIdEepromAddress(self.EEPROM_ADDRESSES[self.current_config['eeprom_addr']])
        if 'eeprom_write_unprotected' in changed:
            pijuice.config.SetIdEepromWriteProtect(not self.current_config['eeprom_write_unprotected'])

        if set(['precedence', 'gpio_in_enabled', 'usb_micro_current_limit', 'usb_micro_dpm', 'no_battery_turn_on']) & set(changed):
            config = {
                'precedence': self.current_config['precedence'],
                'gpio_in_enabled': self.current_config['gpio_in_enabled'],
                'no_battery_turn_on': self.current_config['no_battery_turn_on'],
                'usb_micro_current_limit': self.current_config['usb_micro_current_limit'],
                'usb_micro_dpm': self.current_config['usb_micro_dpm'],
            }
            pijuice.config.SetPowerInputsConfig(config, True)
        
        if 'power_reg_mode' in changed:
            pijuice.config.SetPowerRegulatorMode(self.POWER_REGULATOR_MODES[self.current_config['power_reg_mode']])
        if 'charging_enabled' in changed:
            pijuice.config.SetChargingConfig({'charging_enabled': self.current_config['charging_enabled']}, True)
        
        self.current_config = self._get_device_config()
        confirmation_dialog("Settings successfully updated", single_option=True, next=self.main)


class LEDTab(object):
    LED_FUNCTIONS_OPTIONS = pijuice.config.ledFunctionsOptions
    LED_NAMES = pijuice.config.leds

    def __init__(self, *args):
        self._refresh_settings()
        self.main()
    
    def main(self, *args):
        elements = [urwid.Text("LED settings"), urwid.Divider()]
        for i in range(len(self.LED_NAMES)):
            elements.append(urwid.Button(self.LED_NAMES[i], on_press=self.configure_led, user_data=i))

        elements.extend([urwid.Divider(),
                         urwid.Button("Apply settings", on_press=self._apply_settings),
                         urwid.Button("Refresh", on_press=self._refresh_settings),
                         urwid.Button("Back", on_press=main_menu),
                         urwid.Button("Show config", on_press=self._show_current_config),  # XXX: For debug purposes
                         ])
        main.original_widget = urwid.Padding(urwid.ListBox(urwid.SimpleFocusListWalker(elements)), left=2, right=2)
    
    def configure_led(self, button, index):
        elements = [urwid.Text("LED " + self.LED_NAMES[index]), urwid.Divider()]
        colors = ('R', 'G', 'B')
        elements.append(urwid.Button("Function: {value}".format(value=self.current_config[index]['function']),
                                         on_press=self._list_functions, user_data=index))
        for color in colors:
            color_edit = urwid.Edit(color + ": ", edit_text=str(self.current_config[index]['color'][colors.index(color)]))
            urwid.connect_signal(color_edit, 'change', self._set_color, {'color_index': colors.index(color), 'led_index': index})
            elements.append(color_edit)
        elements.extend([urwid.Divider(), urwid.Button("Back", on_press=self.main)])
        main.original_widget = urwid.Filler(urwid.Pile(elements))
    
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
            changed = [key for key in device_config[i].keys() if device_config[i][key] != self.current_config[i][key]]
            if changed:
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
            body.append(button)
        self.bgroup[self.LED_FUNCTIONS_OPTIONS.index(self.current_config[led_index]['function'])].toggle_state()
        body.extend([urwid.Divider(), urwid.Button('Back', on_press=self._set_function, user_data=led_index)])
        main.original_widget = urwid.Filler(urwid.Pile(body))
    
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
    
    def _show_current_config(self, *args):
        main.original_widget = urwid.Filler(urwid.Pile([urwid.Text(str(self.current_config)), urwid.Button('Back', on_press=self.main)]))


def menu(title, choices):
    body = [urwid.Text(title), urwid.Divider()]
    for c in choices:
        button = urwid.Button(c)
        urwid.connect_signal(button, 'click', item_chosen, c)
        body.append(urwid.AttrMap(button, None, focus_map='reversed'))
    return urwid.ListBox(urwid.SimpleFocusListWalker(body))


def item_chosen(button, choice):
    callback = menu_mapping[choice]
    callback()


def not_implemented_yet(*args):
    main_menu_btn = urwid.Button('Back', on_press=main_menu)
    main.original_widget = urwid.Filler(
        urwid.Pile([urwid.Text("Not implemented yet"), urwid.Divider(), main_menu_btn]))


def main_menu(button=None):
    main.original_widget = urwid.Padding(menu(MAIN_MENU_TITLE, choices), left=2, right=2)


def exit_program(button=None):
    raise urwid.ExitMainLoop()


menu_mapping = {
    "Status": StatusTab,
    "General": GeneralTab,
    "Buttons": not_implemented_yet,
    "LEDs": LEDTab,
    "Battery profile": not_implemented_yet,
    "IO": not_implemented_yet,
    "Wakeup Alarm": not_implemented_yet,
    "Firmware": FirmwareTab,
    "Exit": exit_program
}

# Use list of entries to set order
choices = ["Status", "General", "Buttons", "LEDs", "Battery profile",
           "IO", "Wakeup Alarm", "Firmware", "Exit"]

main = urwid.Padding(menu(MAIN_MENU_TITLE, choices), left=2, right=2)
top = urwid.Overlay(main, urwid.SolidFill(u'\N{MEDIUM SHADE}'),
    align='center', width=('relative', 80),
    valign='middle', height=('relative', 80),
    min_width=20, min_height=9)
urwid.MainLoop(top, palette=[('reversed', 'standout', '')]).run()
