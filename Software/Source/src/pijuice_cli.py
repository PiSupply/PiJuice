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


def get_status():
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
    
    return (general_info,
            gpio_info,
            str(usb_power),
            str(fault),
            str(sys_sw_status) + "mA")


def show_status(button=None):
    general_info, gpio_info, usb_power, fault, sys_sw_status = get_status()
    status_args = {
        "battery": general_info,
        "gpio": gpio_info,
        "usb": usb_power,
        "fault": fault,
        "sys_sw": sys_sw_status
    }
    text = urwid.Text("HAT status\n\n"
                      "Battery: {battery}\nGPIO power input: {gpio}\n"
                      "USB Micro power input: {usb}\nFault: {fault}\n"
                      "System switch: {sys_sw}\n".format(**status_args))
    refresh_btn = urwid.Button('Refresh', on_press=show_status)
    main_menu_btn = urwid.Button('Back', on_press=main_menu)
    pwr_switch_btn = urwid.Button('Change Power switch', on_press=change_power_switch)
    main.original_widget = urwid.Filler(
        urwid.Pile([text, urwid.Divider(), refresh_btn, pwr_switch_btn, main_menu_btn]))


def change_power_switch(button=None):
    elements = [urwid.Text("Choose value for System Power switch"), urwid.Divider()]
    values = [0, 500, 2100]
    for value in values:
        text = str(value) + " mA" if value else "Off"
        btn = urwid.Button(text)
        urwid.connect_signal(btn, 'click', set_power_switch, value)
        elements.append(btn)
    elements.extend([urwid.Divider(),
                     urwid.Button("Back", on_press=show_status)])
    main.original_widget = urwid.Filler(urwid.Pile(elements))


def set_power_switch(button, value):
    pijuice.power.SetSystemPowerSwitch(int(value))
    show_status()


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
    "Status": show_status,
    "General": not_implemented_yet,
    "Buttons": not_implemented_yet,
    "LEDs": not_implemented_yet,
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
    align='center', width=('relative', 60),
    valign='middle', height=('relative', 60),
    min_width=20, min_height=9)
urwid.MainLoop(top, palette=[('reversed', 'standout', '')]).run()
