import curses
import time

import pijuice as PiJuice

BUS = 1
ADDRESS = 0x14

REFRESH_INTERVAL = 5000

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
    general_info = "%i%" % (charge_level)

    voltage = float(pijuice.status.GetBatteryVoltage().get('data', 0))  # mV
    if voltage:
        general_info += ", %iV, %s" % (voltage / 1000, status['battery'])
    
    usb_power = status.get('power_input', 0)

    io_voltage = float(pijuice.status.GetIoCurrent().get('data', 0))  # mV
    io_current = float(pijuice.status.GetIoCurrent().get('data', 0))  # mA
    gpio_info = "%iV, %iA, %s" % (io_voltage / 1000, io_current / 1000, status['powerInput5vIo'])

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
            usb_power,
            fault,
            sys_sw_status)


def show_status(stdscr):
    general_info, gpio_info, usb_power, fault, sys_sw_status = get_status()
    stdscr.addstr(0, 0, "HAT status")
    stdscr.addstr(1, 0, "Battery: " + general_info)
    stdscr.addstr(2, 0, "GPIO power input: " + gpio_info)
    stdscr.addstr(3, 0, "USB Micro power input: " + str(usb_power))
    stdscr.addstr(4, 0, "Fault: " + fault)
    stdscr.addstr(5, 0, "System switch: " + sys_sw_status)

    stdscr.refresh()
    stdscr.getkey()


def main(stdscr):
    stdscr.clear()
    show_status(stdscr)


stdscr = curses.initscr()
curses.wrapper(main)
