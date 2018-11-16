#!/usr/bin/env python
from __future__ import division, print_function

import re

__version__ = "1.4"

from collections import namedtuple
import ctypes
import datetime
from enum import Enum
import sys
import threading
import time

from smbus import SMBus

pijuice_hard_functions = ['HARD_FUNC_POWER_ON', 'HARD_FUNC_POWER_OFF', 'HARD_FUNC_RESET']
pijuice_sys_functions = ['SYS_FUNC_HALT', 'SYS_FUNC_HALT_POW_OFF', 'SYS_FUNC_SYS_OFF_HALT', 'SYS_FUNC_REBOOT']
pijuice_user_functions = ['USER_EVENT'] + ['USER_FUNC' + str(i+1) for i in range(0, 15)]


class PiJuiceError(Exception):
    """Base class for PiJuice exceptions."""
    message = 'ERROR_MESSAGE'


class CommunicationError(PiJuiceError):
    """Raised when there are problems reading from an interface."""
    message = 'COMMUNICATION_ERROR'


class DataCorruptionError(PiJuiceError):
    """Raised when checksum doesn't conform with the data."""
    message = 'DATA_CORRUPTED'


class WriteMismatchError(PiJuiceError):
    """Raised when the data that's read from interface doesn't match the data read from the interface."""
    message = 'WRITE_FAILED'


class BadArgumentError(PiJuiceError):
    """Raised when the argument is invalid (e.g. wrong pin number). Probably should be inherited from ValueError too."""
    message = 'BAD_ARGUMENT'


class InvalidDutyCycleError(PiJuiceError):
    """Raised when the duty cycle argument is off limits."""
    message = 'INVALID_DUTY_CYCLE'


class UnknownDataError(PiJuiceError):
    """Raised when the configuration data received from the interface doesn't conform with current Python code."""
    message = 'UNKNOWN_DATA'


class InvalidUSBMicroCurrentLimitError(PiJuiceError):
    """Raised when USB current limit provided to the function isn't present in PiJuiceConfig class."""
    message = 'INVALID_USB_MICRO_CURRENT_LIMIT'


class InvalidUSBMicroDPMError(PiJuiceError):
    """Raised when USB DPM value provided to the function isn't present in PiJuiceConfig class."""
    message = 'INVALID_USB_MICRO_DPM'


class UnknownConfigError(PiJuiceError):
    """Raised when the configuration data received from the interface doesn't conform with current Python code."""
    message = 'UNKNOWN_CONFIG'


class InvalidConfigError(PiJuiceError):
    """Raised when the configuration data received from the interface doesn't conform with current Python code."""
    message = 'INVALID_CONFIG'


class InvalidPeriodError(PiJuiceError):
    """Raised when the period in IO configuration is incorrect."""
    message = 'INVALID_PERIOD'


class DateTimeValidationError(PiJuiceError):
    """Base exception class for date- or time-related errors."""
    message = 'INVALID_DATE_TIME'


class InvalidSubsecondError(DateTimeValidationError):
    message = 'INVALID_SUBSECOND'


class InvalidSecondError(DateTimeValidationError):
    message = 'INVALID_SECOND'


class InvalidMinuteError(DateTimeValidationError):
    message = 'INVALID_MINUTE'


class InvalidHourError(DateTimeValidationError):
    message = 'INVALID_HOUR'


class InvalidDayError(DateTimeValidationError):
    message = 'INVALID_DAY'


class InvalidMonthError(DateTimeValidationError):
    message = 'INVALID_MONTH'


class InvalidYearError(DateTimeValidationError):
    message = 'INVALID_YEAR'


class InvalidWeekdayError(DateTimeValidationError):
    message = 'INVALID_WEEKDAY'


class InvalidDayOfMonthError(DateTimeValidationError):
    message = 'INVALID_DAY_OF_MONTH'


class InvalidMinutePeriodError(DateTimeValidationError):
    message = 'INVALID_MINUTE_PERIOD'


def pijuice_error_return(func):
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except PiJuiceError as e:
            return {'error': e.message}

    return wrapper


class PiJuiceInterface:
    ERROR_TIMEOUT = 4
    TRANSFER_ATTEMPTS = 2
    TRANSFER_TIMEOUT = 0.05

    def __init__(self, bus=1, address=0x14):
        """Create a new PiJuice instance.  Bus is an optional parameter that
        specifies the I2C bus number to use, for example 1 would use device
        /dev/i2c-1.  If bus is not specified then the open function should be
        called to open the bus.
        """
        self._command = None
        self._data = None
        self._data_length = 0
        self._error = False
        self._last_error_time = 0
        self._i2c_bus = SMBus(bus)
        self._thread = None
        self._lock = threading.Lock()

        self.address = address

    def __del__(self):
        """Clean up any resources used by the PiJuice instance."""
        self._i2c_bus = None

    def __enter__(self):
        """Context manager enter function."""
        # Just return this object so it can be used in a with statement
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit function, ensures resources are cleaned up."""
        self._i2c_bus = None
        return False  # Don't suppress exceptions

    def _get_checksum(self, data):
        checksum = 0xFF
        for x in data[:]:
            checksum = checksum ^ x
        return checksum

    def _read(self):
        try:
            self._data = self._i2c_bus.read_i2c_block_data(self.address, self._command, self._data_length)
            self._error = False
        except:  # IOError:
            self._error = True
            self._last_error_time = time.time()
            self._data = None

    def _write(self):
        try:
            self._i2c_bus.write_i2c_block_data(self.address, self._command, self._data)
            self._error = False
        except:  # IOError:
            self._error = True
            self._last_error_time = time.time()

    def _do_transfer(self, operation):
        if (self._thread and self._thread.is_alive()) or \
                (self._error and (time.time() - self._last_error_time) < self.ERROR_TIMEOUT):
            return False

        self._thread = threading.Thread(target=operation, args=())
        self._thread.start()

        # wait for transfer to finish or timeout
        n = 0
        while self._thread.is_alive() and n < self.TRANSFER_ATTEMPTS:
            time.sleep(self.TRANSFER_TIMEOUT)
            n = n + 1
        if self._error or self._thread.is_alive():
            return False

        return True

    def read_data(self, command, data_length):
        self._command = command
        self._data_length = data_length + 1

        if not self._do_transfer(self._read):
            raise CommunicationError

        d = self._data
        checksum_received = d[:-1]
        checksum_calculated = self._get_checksum(d[0:-1])

        if checksum_calculated != checksum_received:
            raise DataCorruptionError

        return d[:-1]

    def write_data(self, command, data, verify=False, verify_delay=0):
        checksum = self._get_checksum(data)
        d = data[:]
        d.append(checksum)

        self._command = command
        self._data = d
        if not self._do_transfer(self._write):
            raise CommunicationError

        if verify:
            if verify_delay:
                time.sleep(verify_delay)

            result = self.read_data(command, len(data))

            if data != result:
                print('wr {}\n rd {}'.format(data, result))
                raise WriteMismatchError


class FaultEvent(Enum):
    button_power_off = 0
    forced_power_off = 1
    forced_sys_power_off = 2
    watchdog_reset = 3


class BatteryChargingTemperature(Enum):
    NORMAL = 0
    SUSPEND = 1
    COOL = 2
    WARM = 3


class ButtonEvent(Enum):
    UNKNOWN = -1
    NO_EVENT = 0
    PRESS = 1
    RELEASE = 2
    SINGLE_PRESS = 3
    DOUBLE_PRESS = 4
    LONG_PRESS1 = 5
    LONG_PRESS2 = 6


class BatteryStatus(Enum):
    NORMAL = 0
    CHARGING_FROM_IN = 1
    CHARGING_FROM_5V_IO = 2
    NOT_PRESENT = 4


class PowerInputStatus(Enum):
    NOT_PRESENT = 0
    BAD = 1
    WEAK = 2
    PRESENT = 3


class Faults:
    FAULT_EVENT_CMD = 0x44

    def __init__(self, interface):
        self._interface = interface

        self._button_power_off = False
        self._forced_power_off = False
        self._forced_sys_power_off = False
        self._watchdog_reset = False
        self.battery_profile_invalid = False
        self.charging_temperature_fault = BatteryChargingTemperature.NORMAL

    def update_fault_flags(self):
        data = self._interface.read_data(self.FAULT_EVENT_CMD, 1)[0]

        if data & 0x01:
            self._button_power_off = True
        if data & 0x02:
            self._forced_power_off = True
        if data & 0x04:
            self._forced_sys_power_off = True
        if data & 0x08:
            self._watchdog_reset = True
        if data & 0x20:
            self.battery_profile_invalid = True
        self.charging_temperature_fault = BatteryChargingTemperature((data >> 6) & 0x03)

    def reset_fault_flags(self, flags):
        """

        :param flags: iterable of integers
        :return:
        """
        data = 0xff
        for flag in flags:
            try:
                FaultEvent(flag)
            except ValueError:
                raise BadArgumentError

            data = data & ~(0x01 << flag)
        self._interface.write_data(self.FAULT_EVENT_CMD, [data])

    @property
    def button_power_off(self):
        return self._button_power_off

    @button_power_off.setter
    def button_power_off(self, value):
        self.reset_fault_flags((FaultEvent.button_power_off.value,))
        self._button_power_off = False

    @property
    def forced_power_off(self):
        return self._forced_power_off

    @forced_power_off.setter
    def forced_power_off(self, value):
        self.reset_fault_flags((FaultEvent.forced_power_off.value,))
        self._forced_power_off = False

    @property
    def forced_sys_power_off(self):
        return self._forced_sys_power_off

    @forced_sys_power_off.setter
    def forced_sys_power_off(self, value):
        self.reset_fault_flags((FaultEvent.forced_sys_power_off.value,))
        self._forced_sys_power_off = False

    @property
    def watchdog_reset(self):
        return self._watchdog_reset

    @watchdog_reset.setter
    def watchdog_reset(self, value):
        self.reset_fault_flags((FaultEvent.watchdog_reset.value,))
        self._watchdog_reset = False


class Buttons:
    BUTTON_EVENT_CMD = 0x45

    def __init__(self, interface):
        self._interface = interface
        self._sw1_event = ButtonEvent.UNKNOWN
        self._sw2_event = ButtonEvent.UNKNOWN
        self._sw3_event = ButtonEvent.UNKNOWN

    def update_button_events(self):
        data = self._interface.read_data(self.BUTTON_EVENT_CMD, 2)

        sw1_event_index = data[0] & 0x0F
        if sw1_event_index < len(ButtonEvent) + 1:
            sw1_event_index = -1

        sw2_event_index = (data[0] >> 4) & 0x0F
        if sw2_event_index < len(ButtonEvent) + 1:
            sw2_event_index = -1

        sw3_event_index = data[1] & 0x0F
        if sw3_event_index < len(ButtonEvent) + 1:
            sw3_event_index = -1

        self._sw1_event = ButtonEvent(sw1_event_index)
        self._sw2_event = ButtonEvent(sw2_event_index)
        self._sw3_event = ButtonEvent(sw3_event_index)

    @property
    def sw1(self):
        return self._sw1_event

    @sw1.setter
    def sw1(self, value):
        self._interface.write_data(self.BUTTON_EVENT_CMD, [0xf0, 0xff])
        self._sw1_event = ButtonEvent.NO_EVENT

    @property
    def sw2(self):
        return self._sw2_event

    @sw2.setter
    def sw2(self, value):
        self._interface.write_data(self.BUTTON_EVENT_CMD, [0x0f, 0xff])
        self._sw2_event = ButtonEvent.NO_EVENT

    @property
    def sw3(self):
        return self._sw3_event

    @sw3.setter
    def sw3(self, value):
        self._interface.write_data(self.BUTTON_EVENT_CMD, [0xff, 0xf0])
        self._sw3_event = ButtonEvent.NO_EVENT


class Led:
    LED_STATE_CMD = 0x66
    LED_BLINK_CMD = 0x68
    # First value is named blink_count because there is a built-in method named 'count'.
    LedBlink = namedtuple('LedBlink', ('blink_count', 'rgb1', 'period1', 'rgb2', 'period2'))

    def __init__(self, interface, led_index):
        """

        :param interface:
        :param led_index: 0 for D1, 1 for D2
        """
        self._interface = interface
        self.index = led_index
        self.name = 'D{}'.format(self.index + 1)
        self._state = None
        self._blink = None

    @property
    def state(self):
        self._state = self._interface.read_data(self.LED_STATE_CMD + self.index, 3)
        return self._state

    @state.setter
    def state(self, rgb):
        try:
            if len(rgb) != 3:
                raise ValueError
            for color in rgb:
                if not isinstance(color, int):
                    raise TypeError
                if color < 0 or color > 255:
                    raise ValueError
        except (TypeError, ValueError):
            raise BadArgumentError

        self._interface.write_data(self.LED_STATE_CMD + self.index, rgb)

    @property
    def blink(self):
        data = self._interface.read_data(self.LED_BLINK_CMD + self.index, 9)
        blink_data = self.LedBlink(data[0], data[1:4], data[4] * 10, data[5:8], data[8] * 10)
        return blink_data

    @blink.setter
    def blink(self, led_blink):
        if not isinstance(led_blink, self.LedBlink):
            raise BadArgumentError

        data = [led_blink.count & 0xFF] + led_blink.rgb1*1 + [(led_blink.period1//10) & 0xFF] +\
               led_blink.rgb2*1 + [(led_blink.period2//10) & 0xFF]
        self._interface.write_data(self.LED_BLINK_CMD + self.index, data)


class Pin:
    IO_PIN_ACCESS_CMD = 0x75

    def __init__(self, interface, pin_index):
        """

        :param interface:
        :param pin_index: 0 for pin #1, 1 for pin #2
        """
        self._interface = interface
        self.index = pin_index

    @property
    def io_digital_input(self):
        data = self._interface.read_data(self.IO_PIN_ACCESS_CMD + self.index * 5, 2)
        return 1 if data[0] == 1 else 0

    @property
    def io_digital_output(self):
        data = self._interface.read_data(self.IO_PIN_ACCESS_CMD + self.index * 5, 2)
        return 1 if data[1] == 1 else 0

    @io_digital_output.setter
    def io_digital_output(self, value):
        data = [0, 0 if value == 0 else 1]
        self._interface.write_data(self.IO_PIN_ACCESS_CMD + self.index * 5, data)

    @property
    def io_analog_input(self):
        data = self._interface.read_data(self.IO_PIN_ACCESS_CMD + self.index * 5, 2)
        return (data[1] << 8) | data[0]

    @property
    def io_pwm(self):
        data = self._interface.read_data(self.IO_PIN_ACCESS_CMD + self.index * 5, 2)
        duty_cycle_int = data[0] | (data[1] << 8)
        duty_cycle = float(duty_cycle_int) * 100 // 65534 if duty_cycle_int < 65535 else 100
        return duty_cycle

    @io_pwm.setter
    def io_pwm(self, duty_cycle):
        try:
            if duty_cycle < 0 or duty_cycle > 100:
                raise BadArgumentError
        except TypeError:
            raise BadArgumentError

        duty_cycle_int = int(round(duty_cycle * 65534 // 100))
        data = [duty_cycle_int & 0xFF, (duty_cycle_int >> 8) & 0xFF]
        self._interface.write_data(self.IO_PIN_ACCESS_CMD + self.index * 5, data)


class PiJuiceStatus:
    STATUS_CMD = 0x40
    CHARGE_LEVEL_CMD = 0x41
    BUTTON_EVENT_CMD = 0x45
    BATTERY_TEMPERATURE_CMD = 0x47
    BATTERY_VOLTAGE_CMD = 0x49
    BATTERY_CURRENT_CMD = 0x4b
    IO_VOLTAGE_CMD = 0x4d
    IO_CURRENT_CMD = 0x4f

    def __init__(self, interface):
        self._interface = interface

        self._charge_level = 0
        self._is_fault = False
        self._is_button = False
        self._battery_status = BatteryStatus.NORMAL
        self._power_input = PowerInputStatus.NOT_PRESENT
        self._power_input_5v_io = PowerInputStatus.NOT_PRESENT

        self.faults = Faults(self._interface)
        self.buttons = Buttons(self._interface)
        self.d1 = Led(self._interface, 0)
        self.d2 = Led(self._interface, 1)
        self.pin1 = Pin(self._interface, 0)
        self.pin2 = Pin(self._interface, 1)

    def __enter__(self):
        # Just return this object so it can be used in a with statement
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        return False  # Don't suppress exceptions.

    @pijuice_error_return
    def GetStatus(self):
        self.update_status()
        status = {
            'isFault': self.is_fault,
            'isButton': self.is_button,
            'battery': self.battery_status.name,
            'powerInput': self.power_input.name,
            'powerInput5vIo': self.power_input_5v_io.name
        }
        return {'data': status, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetChargeLevel(self):
        return {'data': self.charge_level, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetFaultStatus(self):
        self.faults.update_fault_flags()
        fault = {}

        if self.faults.button_power_off:
            fault['button_power_off'] = True
        if self.faults.forced_power_off:
            fault['forced_power_off'] = True
        if self.faults.forced_sys_power_off:
            fault['forced_sys_power_off'] = True
        if self.faults.watchdog_reset:
            fault['watchdog_reset'] = True
        if self.faults.battery_profile_invalid:
            fault['battery_profile_invalid'] = True
        if self.faults.charging_temperature_fault.value:
            fault['charging_temperature_fault'] = BatteryChargingTemperature(self.faults.charging_temperature_fault)
        return {'data': fault, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def ResetFaultFlags(self, flags):
        flags_int = []

        try:
            for flag in flags:
                flags_int.append(FaultEvent[flag])
        except ValueError:
            raise BadArgumentError

        self.faults.reset_fault_flags(flags_int)

    @pijuice_error_return
    def GetButtonEvents(self):
        self.buttons.update_button_events()
        event = {
            'SW1': self.buttons.sw1.name,
            'SW2': self.buttons.sw2.name,
            'SW3': self.buttons.sw3.name
        }
        return {'data': event, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def AcceptButtonEvent(self, button):
        if button == 'SW1':
            self.buttons.sw1 = None
        elif button == 'SW2':
            self.buttons.sw2 = None
        elif button == 'SW3':
            self.buttons.sw3 = None
        else:
            raise BadArgumentError

    @pijuice_error_return
    def GetBatteryTemperature(self):
        return {'data': self.battery_temperature, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetBatteryVoltage(self):
        return {'data': self.battery_voltage, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetBatteryCurrent(self):
        return {'data': self.battery_current, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIoVoltage(self):
        return {'data': self.io_voltage, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIoCurrent(self):
        return {'data': self.io_current, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetLedState(self, led, rgb):
        if led == 'D1':
            self.d1.state = rgb
        elif led == 'D2':
            self.d2.state = rgb
        else:
            raise BadArgumentError

        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetLedState(self, led):
        if led == 'D1':
            return self.d1.state
        elif led == 'D2':
            return self.d2.state
        else:
            raise BadArgumentError

    @pijuice_error_return
    def SetLedBlink(self, led, count, rgb1, period1, rgb2, period2):
        data = Led.LedBlink(count, rgb1, period1, rgb2, period2)

        if led == 'D1':
            self.d1.blink = data
        elif led == 'D2':
            self.d2.blink = data
        else:
            raise BadArgumentError

        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetLedBlink(self, led):
        if led == 'D1':
            data = self.d1.blink
        elif led == 'D2':
            data = self.d2.blink
        else:
            raise BadArgumentError

        return {
            'data': {
                'count': data.blink_count,
                'rgb1': data.rgb1,
                'period1': data.period1,
                'rgb2': data.rgb2,
                'period2': data.period2
                },
            'error': 'NO_ERROR'
        }

    @pijuice_error_return
    def GetIoDigitalInput(self, pin):
        if pin == 1:
            data = self.pin1.io_digital_input
        elif pin == 2:
            data = self.pin2.io_digital_input
        else:
            raise BadArgumentError

        return {'data': data, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetIoDigitalOutput(self, pin, value):
        if pin == 1:
            self.pin1.io_digital_output = value
        elif pin == 2:
            self.pin2.io_digital_output = value
        else:
            raise BadArgumentError

        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIoDigitalOutput(self, pin):
        if pin == 1:
            data = self.pin1.io_digital_output
        elif pin == 2:
            data = self.pin2.io_digital_output
        else:
            raise BadArgumentError

        return {'data': data, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIoAnalogInput(self, pin):
        if pin == 1:
            data = self.pin1.io_analog_input
        elif pin == 2:
            data = self.pin2.io_analog_input
        else:
            raise BadArgumentError

        return {'data': data, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetIoPWM(self, pin, dutyCycle):
        if pin == 1:
            self.pin1.io_pwm = dutyCycle
        elif pin == 2:
            self.pin2.io_pwm = dutyCycle
        else:
            raise BadArgumentError

        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIoPWM(self, pin):
        if pin == 1:
            data = self.pin1.io_pwm
        elif pin == 2:
            data = self.pin2.io_pwm
        else:
            raise BadArgumentError

        return {'data': data, 'error': 'NO_ERROR'}

    def update_status(self):
        result = self._interface.read_data(self.STATUS_CMD, 1)[0]

        self._is_fault = bool(result & 0x01)
        self._is_button = bool(result & 0x02)
        self._battery_status = BatteryStatus((result >> 2) & 0x03)
        self._power_input = PowerInputStatus((result >> 4) & 0x03)
        self._power_input_5v_io = PowerInputStatus((result >> 6) & 0x03)

    @property
    def charge_level(self):
        self._charge_level = self._interface.read_data(self.CHARGE_LEVEL_CMD, 1)[0]
        return self._charge_level

    @property
    def battery_status(self):
        return self._battery_status

    @property
    def is_fault(self):
        return self._is_fault

    @property
    def is_button(self):
        return self._is_button

    @property
    def power_input(self):
        return self._power_input

    @property
    def power_input_5v_io(self):
        return self._power_input_5v_io

    @property
    def battery_temperature(self):
        temperature_data = self._interface.read_data(self.BATTERY_TEMPERATURE_CMD, 2)
        return (temperature_data[1] << 8) | temperature_data[0]

    @property
    def battery_voltage(self):
        voltage_data = self._interface.read_data(self.BATTERY_VOLTAGE_CMD, 2)
        return (voltage_data[1] << 8) | voltage_data[0]

    @property
    def battery_current(self):
        current_data = self._interface.read_data(self.BATTERY_CURRENT_CMD, 2)
        i = (current_data[1] << 8) | current_data[0]
        if i & (1 << 15):
            i = i - (1 << 16)  # Getting negative values from integer
        return i

    @property
    def io_voltage(self):
        voltage_data = self._interface.read_data(self.IO_VOLTAGE_CMD, 2)
        return (voltage_data[1] << 8) | voltage_data[0]

    @property
    def io_current(self):
        current_data = self._interface.read_data(self.IO_CURRENT_CMD, 2)
        i = (current_data[1] << 8) | current_data[0]
        if i & (1 << 15):
            i = i - (1 << 16)
        return i


class DaylightSavingMode(Enum):
    NONE = 0
    ADD1H = 1
    SUB1H = 2


def double_digit_to_hex(value, low_digit_limit=0x0f, high_digit_limit=0x0f):
    # 21 will turn to 0x21
    low_digit = (value % 10) & low_digit_limit
    high_digit = (value // 10) & high_digit_limit
    return (high_digit << 4) | low_digit


def hex_to_double_digit(value, low_digit_limit=0x0f, high_digit_limit=0x0f):
    low_digit = value & low_digit_limit
    high_digit = (value >> 4) & high_digit_limit
    return high_digit * 10 + low_digit


class Alarm:
    RTC_ALARM_CMD = 0xB9
    RTC_CTRL_STATUS_CMD = 0xC2

    def __init__(self, interface):
        self._interface = interface

        self._alarm_wake_up_enabled = False
        self._alarm_flag = False

        self._second = 0
        self._minute = 0
        self._minute_period = 0
        self._use_minute_period = False
        self._hour = 0
        self._hours = ()
        self._use_multiple_hours = False
        self._every_hour = False
        self._hour_format = 24
        self._day = 1
        self._weekday = 1
        self._weekdays = ()
        self._use_weekday = False
        self._use_multiple_weekdays = False
        self._every_day = False

    @property
    def second(self):
        return self._second

    @second.setter
    def second(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        elif value < 0 or value > 60:
            raise BadArgumentError
        self._second = value

    @property
    def minute(self):
        return self._minute

    @minute.setter
    def minute(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        elif value < 0 or value > 60:
            raise BadArgumentError
        self._minute = value
        self._use_minute_period = False
        self._minute_period = 0

    @property
    def minute_period(self):
        return self._minute_period

    @minute_period.setter
    def minute_period(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        elif value < 1 or value > 60:
            raise BadArgumentError
        self._minute_period = value
        self._use_minute_period = True
        self._minute = 0

    @property
    def hour(self):
        return self._hour

    @hour.setter
    def hour(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        elif value < 0 or value > 24:
            raise BadArgumentError
        self._hour = value
        self._use_multiple_hours = False
        self._hours = ()
        self._every_hour = False

    @property
    def hours(self):
        return self._hours

    @hours.setter
    def hours(self, value):
        if not isinstance(value, (tuple, list, set)):
            raise BadArgumentError
        for elem in value:
            if not isinstance(elem, int):
                raise BadArgumentError
            elif elem < 0 or elem > 24:
                raise BadArgumentError
        self._hours = tuple(value)
        self._use_multiple_hours = True
        self._hour = 0
        self._every_hour = False

    @property
    def every_hour(self):
        return self._every_hour

    @every_hour.setter
    def every_hour(self, value):
        if value:
            self._every_hour = True
            self._hour = 0
            self._hours = ()
            self._use_multiple_hours = True
        else:
            self._every_hour = False
            self._hour = 0
            self._hours = ()
            self._use_multiple_hours = False

    @property
    def hour_format(self):
        return self._hour_format

    @hour_format.setter
    def hour_format(self, value):
        if value not in (12, 24):
            raise BadArgumentError
        self._hour_format = value

    @property
    def day(self):
        return self._day

    @day.setter
    def day(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        elif value < 1 or value > 31:  # What will happen if there is no 31st day?
            raise BadArgumentError
        self._day = value
        self._weekday = 1
        self._use_weekday = False
        self._use_multiple_weekdays = False
        self._weekdays = ()

    @property
    def weekday(self):
        return self._weekday

    @weekday.setter
    def weekday(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        elif value < 1 or value > 7:
            raise BadArgumentError
        self._weekday = value
        self._day = 1
        self._use_weekday = True
        self._use_multiple_weekdays = False
        self._weekdays = ()

    @property
    def weekdays(self):
        return self._weekdays

    @weekdays.setter
    def weekdays(self, value):
        if not isinstance(value, (tuple, list, set)):
            raise BadArgumentError
        for elem in value:
            if not isinstance(elem, int):
                raise BadArgumentError
            elif elem < 1 or elem > 7:
                raise BadArgumentError
        self._weekday = 1
        self._day = 1
        self._use_weekday = True
        self._use_multiple_weekdays = True
        self._weekdays = tuple(value)

    @property
    def every_day(self):
        return self._every_day

    @every_day.setter
    def every_day(self, value):
        self._every_day = bool(value)

    def update_control_status(self):
        data = self._interface.read_data(self.RTC_CTRL_STATUS_CMD, 2)

        self._alarm_wake_up_enabled = (data[0] & 0x01) and (data[0] & 0x04)
        self._alarm_flag = True if data[1] & 0x01 else False

    @property
    def alarm_wake_up_enabled(self):
        return self._alarm_wake_up_enabled

    @alarm_wake_up_enabled.setter
    def alarm_wake_up_enabled(self, value):
        data = self._interface.read_data(self.RTC_CTRL_STATUS_CMD, 2)
        is_enabled = (data[0] & 0x01) and (data[0] & 0x04)

        if (is_enabled and value) or (not is_enabled and not value):
            return  # do nothing
        elif is_enabled and not value:
            data[0] = data[0] & 0xFE  # disable
        elif not is_enabled and value:
            data[0] = data[0] | 0x01 | 0x04  # enable

        self._interface.write_data(self.RTC_CTRL_STATUS_CMD, data, True)
        self._alarm_wake_up_enabled = bool(value)

    @property
    def alarm_flag(self):
        return self._alarm_flag

    @alarm_flag.setter
    def alarm_flag(self, value):
        data = self._interface.read_data(self.RTC_CTRL_STATUS_CMD, 2)
        self._alarm_flag = True if data[1] & 0x01 else False

        if not value and self._alarm_flag:
            data[1] &= 0xFE
            self._interface.write_data(self.RTC_CTRL_STATUS_CMD, data, True)
            self._alarm_flag = False

    def get_alarm(self):
        data = self._interface.read_data(self.RTC_ALARM_CMD, 9)

        if data[0] & 0x80:
            self._second = 0
        else:
            self._second = hex_to_double_digit(data[0], high_digit_limit=0x07)

        if data[1] & 0x80:
            self._use_minute_period = True
            self._minute = 0
            self._minute_period = data[7]
        else:
            self._use_minute_period = False
            self._minute = hex_to_double_digit(data[1], high_digit_limit=0x07)
            self._minute_period = 0

        if data[2] & 0x40:
            self._hour_format = 12
        else:
            self._hour_format = 24

        if data[2] & 0x80:
            self._use_multiple_hours = True

            if data[4] == 0xFF and data[5] == 0xFF and data[6] == 0xFF:
                self._every_hour = True
            else:
                self._every_hour = False
                hours = []
                for i in range(0, 3):
                    for j in range(0, 8):
                        if data[i+4] & ((0x01 << j) & 0xFF):
                            hours.append(i*8+j)

                self._hours = tuple(hours)
        else:
            self._use_multiple_hours = False
            self._every_hour = False

            if self._hour_format == 12:
                pm_shift = 12 if data[2] & 0x20 else 0
                hour_12 = hex_to_double_digit(data[2], high_digit_limit=1)
                self._hour = (0 if hour_12 == 12 else hour_12) + pm_shift
            else:
                self._hour = hex_to_double_digit(data[2], high_digit_limit=0x03)

        if data[3] & 0x40:
            self._use_weekday = True

            if data[3] & 0x80:
                self._use_multiple_weekdays = True
                if data[8] == 0xff:
                    self._every_day = True
                else:
                    self._every_day = False
                    weekdays = []

                    for j in range(1, 8):
                        if data[8] & ((0x01 << j) & 0xFF):
                            weekdays.append(j)

                    self._weekdays = tuple(weekdays)
            else:
                self._every_day = False
                self._use_multiple_weekdays = False
                self._weekday = data[3] & 0x07
        else:
            self._use_weekday = False

            if data[3] & 0x80:
                self._every_day = True
            else:
                self._every_day = False
                self._day = hex_to_double_digit(data[3], high_digit_limit=0x03)

    def set_alarm(self, disable=False):
        data = [0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF]
        if disable:
            self._interface.write_data(self.RTC_ALARM_CMD, data, True, 0.2)

        data[0] = double_digit_to_hex(self._second)

        if self._use_minute_period:
            data[1] = 0x80
            data[7] = self._minute_period
        else:
            data[1] = double_digit_to_hex(self._minute)

        if self._every_hour:
            data[2] = 0x80
        elif not self._use_multiple_hours:
            if self._hour_format == 24:
                data[2] = double_digit_to_hex(self._hour, high_digit_limit=0x03)
            else:
                if self._hour >= 12:  # PM
                    hour_ampm = self._hour - 12 if self._hour != 12 else 12
                    data[2] = double_digit_to_hex(hour_ampm) | 0x20 | 0x40
                else:  # AM
                    hour_ampm = self._hour if self._hour != 0 else 12
                    data[2] = double_digit_to_hex(hour_ampm) | 0x40
        else:
            data[2] = 0x80
            hs = 0x00000000

            for hour in self._hours:
                hs = hs | (0x00000001 << hour)

            data[4] = hs & 0x000000FF
            hs = hs >> 8
            data[5] = hs & 0x000000FF
            hs = hs >> 8
            data[6] = hs & 0x000000FF

        if self._use_weekday:
            data[3] = 0x40

            if self._every_day or self._use_multiple_weekdays:
                data[3] |= 0x80
            else:
                data[3] |= self._weekday

            if self._use_multiple_weekdays:
                ds = 0x00
                for day in self._weekdays:
                    ds |= (0x01 << day)
                data[8] = ds
        else:
            if self._every_day:
                data[3] |= 0x80
            else:
                data[3] = double_digit_to_hex(self._day, high_digit_limit=0x03)

        self._interface.write_data(self.RTC_ALARM_CMD, data)
        time.sleep(0.2)
        ret = self._interface.read_data(self.RTC_ALARM_CMD, 9)

        if data != ret:
            if ret[2] & 0x40:
                pm_shift = 12 if ret[2] & 0x20 else 0
                hour_12 = hex_to_double_digit(ret[2], high_digit_limit=1)
                ret_hour = (0 if hour_12 == 12 else hour_12) + pm_shift
            else:
                ret_hour = hex_to_double_digit(ret[2], high_digit_limit=0x03)

            if self._hour != ret_hour:
                raise WriteMismatchError


class Time:
    RTC_TIME_CMD = 0xB0

    def __init__(self, interface):
        self._interface = interface

        self._date_time = datetime.datetime.now()
        self._hour_format = 24
        self.subsecond = 0
        self._dst_mode = DaylightSavingMode.NONE
        self._store_operation = False

    def get_date_time(self):
        data = self._interface.read_data(self.RTC_TIME_CMD, 9)
        second = hex_to_double_digit(data[0], high_digit_limit=7)
        minute = hex_to_double_digit(data[1], high_digit_limit=7)

        if data[2] & 0x40:
            self._hour_format = 12
            hour_12 = hex_to_double_digit(data[2], high_digit_limit=1)
            pm_shift = 12 if (data[2] & 0x20) else 0
            hour = (0 if hour_12 == 12 else hour_12) + pm_shift
        else:
            self._hour_format = 24
            hour = hex_to_double_digit(data[2], high_digit_limit=2)

        day = hex_to_double_digit(data[4], high_digit_limit=3)
        month = hex_to_double_digit(data[5], high_digit_limit=1)
        year = hex_to_double_digit(data[6]) + 2000

        try:
            self._date_time = datetime.datetime(year, month, day, hour, minute, second)
        except ValueError:
            raise DateTimeValidationError

        self._dst_mode = DaylightSavingMode(data[8] & 0x03)
        self._store_operation = bool(data[8] & 0x04)

    def set_date_time(self, date_time, hour_format, dst_mode, store_operation):
        if not isinstance(date_time, datetime.datetime):
            raise BadArgumentError
        if date_time.year < 2000 or date_time.year > 2099:
            raise BadArgumentError
        if hour_format not in (12, 24):
            raise BadArgumentError
        if not isinstance(dst_mode, DaylightSavingMode):
            raise BadArgumentError

        if hour_format == 12:
            if date_time.hour >= 12:  # PM
                hour_ampm = date_time.hour - 12 if date_time.hour != 12 else 12
                hour = double_digit_to_hex(hour_ampm) | 0x20 | 0x40
            else:  # AM
                hour_ampm = date_time.hour if date_time.hour != 0 else 12
                hour = double_digit_to_hex(hour_ampm) | 0x40
        else:
            hour = double_digit_to_hex(date_time.hour)

        data = [
            double_digit_to_hex(date_time.second),
            double_digit_to_hex(date_time.minute), hour,
            date_time.isoweekday(),
            double_digit_to_hex(date_time.day),
            double_digit_to_hex(date_time.month),
            double_digit_to_hex(date_time.year - 2000),
            self.subsecond,
            dst_mode.value | 0x04 if store_operation else dst_mode.value
        ]

        self._interface.write_data(self.RTC_TIME_CMD, data)
        time.sleep(0.2)
        ret = self._interface.read_data(self.RTC_TIME_CMD, 9)

        if data != ret and abs(ret[0] - data[0]) > 2:
            raise WriteMismatchError

        self._date_time = date_time
        self._hour_format = hour_format
        self._dst_mode = dst_mode
        self._store_operation = store_operation

    @property
    def date_time(self):
        return self._date_time

    @property
    def hour_format(self):
        return self._hour_format

    @property
    def dst_mode(self):
        return self._dst_mode

    @property
    def store_operation(self):
        return self._store_operation


class PiJuiceRtcAlarm:
    def __init__(self, interface):
        self._interface = interface
        self.alarm = Alarm(self._interface)
        self.time = Time(self._interface)

    def __enter__(self):
        # Just return this object so it can be used in a with statement
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        return False  # Don't suppress exceptions.

    @pijuice_error_return
    def GetControlStatus(self):
        self.alarm.update_control_status()
        return {
            'data': {
                'alarm_wakeup_enabled': self.alarm.alarm_wake_up_enabled,
                'alarm_flag': self.alarm.alarm_flag
            },
            'error': 'NO_ERROR'
        }

    @pijuice_error_return
    def ClearAlarmFlag(self):
        self.alarm.alarm_flag = False
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetWakeupEnabled(self, status):
        self.alarm.alarm_wake_up_enabled = status
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetTime(self):
        self.time.get_date_time()

        if self.time.hour_format == 12:
            pm_shift = 12 if self.time.date_time.hour >= 12 else 0
            hour = (self.time.date_time.hour - pm_shift) if (self.time.date_time.hour - pm_shift) else 12
            hour = '{}{}'.format(hour, 'PM' if pm_shift == 12 else 'AM')
        else:
            hour = self.time.date_time.hour

        result = {
            'second': self.time.date_time.second,
            'minute': self.time.date_time.minute,
            'hour': hour,
            'weekday': self.time.date_time.isoweekday(),
            'day': self.time.date_time.day,
            'month': self.time.date_time.month,
            'year': self.time.date_time.year,
            'subsecond': self.time.subsecond,
            'daylightsaving': self.time.dst_mode.name,
            'storeoperation': self.time.store_operation
        }

        return {'data': result, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetTime(self, dateTime):
        try:
            second = int(dateTime['second'])
        except (KeyError, TypeError, ValueError):
            raise InvalidSecondError

        try:
            minute = int(dateTime['minute'])
        except (KeyError, TypeError, ValueError):
            raise InvalidMinuteError

        try:
            hour_src = dateTime['hour']
            if isinstance(hour_src, str):
                # 12-hour format, PM
                if re.match('^(0[1-9]|1[0-2]|[1-9])[pP][mM]$', hour_src):
                    hour = int(re.match('^(0[1-9]|1[0-2]|[1-9])', hour_src)[0])
                    hour_format = 12
                    hour = 12 + hour if hour != 12 else 12
                # 12-hour format, AM
                elif re.match('^(0[1-9]|1[0-2]|[1-9])[aA][mM]$', hour_src):
                    hour = int(re.match('^(0[1-9]|1[0-2]|[1-9])', hour_src)[0])
                    hour_format = 12
                    hour = hour if hour != 12 else 0
                # 24-hour format is the same for both strings and integers
                elif re.match('^(0[0-9]|1[0-9]|2[0-3]|[0-9])$', hour_src):
                    hour = int(hour_src)
                else:
                    raise ValueError

            if isinstance(hour_src, int):
                hour_format = 24
                hour = hour_src
            else:
                raise ValueError
        except (KeyError, TypeError, ValueError):
            raise InvalidHourError

        try:
            day = int(dateTime['day'])
        except (KeyError, TypeError, ValueError):
            raise InvalidDayError

        try:
            month = int(dateTime['month'])
        except (KeyError, TypeError, ValueError):
            raise InvalidMonthError

        try:
            year = int(dateTime['year'])
        except (KeyError, TypeError, ValueError):
            raise InvalidYearError

        try:
            date_time = datetime.datetime(year, month, day, hour, minute, second)
        except ValueError:
            raise DateTimeValidationError

        if 'daylightsaving' in dateTime:
            try:
                dst_mode = DaylightSavingMode[dateTime['daylightsaving']]
            except KeyError:
                raise DateTimeValidationError
        else:
            dst_mode = DaylightSavingMode.NONE

        store_operation = ('storeoperation' in dateTime)
        self.time.set_date_time(date_time, hour_format, dst_mode, store_operation)

    @pijuice_error_return
    def GetAlarm(self):
        self.alarm.get_alarm()
        alarm = {'second': self.alarm._second}

        if self.alarm._use_minute_period:
            alarm['minute_period'] = self.alarm._minute_period
        else:
            alarm['minute'] = self.alarm._minute

        if self.alarm._every_hour:
            alarm['hour'] = 'EVERY_HOUR'
        elif not self.alarm._use_multiple_hours:
            if self.alarm._hour_format == 24:
                alarm['hour'] = self.alarm._hour
            else:
                pm_shift = 12 if self.alarm._hour >= 12 else 0
                hour = (self.alarm._hour - pm_shift) if (self.alarm._hour - pm_shift) else 12
                alarm['hour'] = '{}{}'.format(hour, 'PM' if pm_shift == 12 else 'AM')
        else:
            if self.alarm._hour_format == 24:
                alarm['hour'] = ';'.join((str(hour) for hour in self.alarm._hours))
            else:
                hours_12 = []
                for hour in self.alarm._hours:
                    pm_shift = 12 if hour >= 12 else 0
                    hour_12 = (hour - pm_shift) if (hour - pm_shift) else 12
                    hours_12.append('{}{}'.format(hour_12, 'PM' if pm_shift == 12 else 'AM'))

                alarm['hour'] = ';'.join(hours_12)

        if self.alarm._use_weekday:
            if self.alarm._every_day:
                alarm['weekday'] = 'EVERY_DAY'
            elif not self.alarm._use_multiple_weekdays:
                alarm['weekday'] = self.alarm._weekday
            else:
                alarm['weekday'] = ';'.join((str(weekday) for weekday in self.alarm._weekdays))
        else:
            if self.alarm._every_day:
                alarm['day'] = 'EVERY_DAY'
            else:
                alarm['day'] = self.alarm._day

        return {'data': alarm, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetAlarm(self, alarm):
        if not alarm:
            self.alarm.set_alarm(False)

        if 'second' in alarm:
            try:
                self.alarm.second = int(alarm['second'])
            except (TypeError, ValueError, BadArgumentError):
                raise InvalidSecondError
        else:
            self.alarm.second = 0

        if 'minute' in alarm:
            try:
                self.alarm.minute = int(alarm['minute'])
            except (TypeError, ValueError, BadArgumentError):
                raise InvalidMinuteError
        elif 'minute_period' in alarm:
            try:
                self.alarm.minute_period = int(alarm['minute_period'])
            except (TypeError, ValueError, BadArgumentError):
                raise InvalidMinutePeriodError
        else:
            self.alarm.minute_period = 1  # every minute

        if 'hour' in alarm:
            hour = alarm['hour']
            if hour == 'EVERY_HOUR':
                self.alarm.every_hour = True
            elif isinstance(hour, int):
                try:
                    self.alarm.hour = hour
                except BadArgumentError:
                    raise InvalidHourError

                self.alarm.hour_format = 24
            elif isinstance(hour, str):
                # 12-hour format, PM
                if re.match('^(0[1-9]|1[0-2]|[1-9])[pP][mM]$', hour):
                    hour = int(re.match('^(0[1-9]|1[0-2]|[1-9])', hour)[0])
                    self.alarm.hour_format = 12
                    self.alarm.hour = 12 + hour if hour != 12 else 12
                # 12-hour format, AM
                elif re.match('^(0[1-9]|1[0-2]|[1-9])[aA][mM]$', hour):
                    hour = int(re.match('^(0[1-9]|1[0-2]|[1-9])', hour)[0])
                    self.alarm.hour_format = 12
                    self.alarm.hour = hour if hour != 12 else 0
                # 24-hour format is the same for both strings and integers
                elif re.match('^(0[0-9]|1[0-9]|2[0-3]|[0-9])$', hour):
                    self.alarm.hour_format = 24
                    self.alarm.hour = int(hour)
                elif ';' in hour:
                    hours = hour.rstrip(';').split(';')
                    if hours:
                        self.alarm.hour_format = 24
                        out_hours = []
                        for elem in hours:
                            if re.match('^(0[1-9]|1[0-2]|[1-9])[pP][mM]$', elem):
                                h = int(re.match('^(0[1-9]|1[0-2]|[1-9])', elem)[0])
                                out_hours.append(12 + h if h != 12 else 12)
                            # 12-hour format, AM
                            elif re.match('^(0[1-9]|1[0-2]|[1-9])[aA][mM]$', elem):
                                h = int(re.match('^(0[1-9]|1[0-2]|[1-9])', elem)[0])
                                out_hours.append(h if h != 12 else 12)
                            # 24-hour format is the same for both strings and integers
                            elif re.match('^(0[0-9]|1[0-9]|2[0-3]|[0-9])$', hour):
                                out_hours.append(int(hour))
                            else:
                                raise InvalidHourError
                        self.alarm.hours = out_hours
                    else:
                        raise InvalidHourError
                else:
                    raise InvalidHourError
        else:
            self.alarm.every_hour = True

        if 'weekday' in alarm:
            if alarm['weekday'] == 'EVERY_DAY':
                self.alarm.every_day = True
            elif ';' not in alarm['weekday']:
                try:
                    self.alarm.weekday = int(alarm['weekday'])
                except (TypeError, ValueError, BadArgumentError):
                    raise InvalidWeekdayError
            else:
                weekdays = alarm['weekday'].rstrip(';').split(';')
                if weekdays:
                    weekdays_out = []
                    try:
                        for wd in weekdays:
                            weekdays_out.append(int(wd))
                        self.alarm.weekdays = weekdays_out
                    except (TypeError, ValueError, BadArgumentError):
                        raise InvalidDayError
                else:
                    raise InvalidWeekdayError
        elif 'day' in alarm:
            if alarm['day'] == 'EVERY_DAY':
                self.alarm.every_day = True
            else:
                try:
                    self.alarm.day = int(alarm['day'])
                except (TypeError, ValueError, BadArgumentError):
                    raise InvalidDayError
        else:
            self.alarm.every_day = True

        self.alarm.set_alarm()


class PiJuicePower(object):

    WATCHDOG_ACTIVATION_CMD = 0x61
    POWER_OFF_CMD = 0x62
    WAKEUP_ON_CHARGE_CMD = 0x63
    SYSTEM_POWER_SWITCH_CTRL_CMD = 0x64

    def __init__(self, interface):
        self.interface = interface

    def __enter__(self):
        # Just return this object so it can be used in a with statement
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        return False  # Don't suppress exceptions.

    @pijuice_error_return
    def SetPowerOff(self, delay):
        self.interface.write_data(self.POWER_OFF_CMD, [delay])
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetPowerOff(self):
        return self.interface.read_data(self.POWER_OFF_CMD, 1)

    @pijuice_error_return
    def SetWakeUpOnCharge(self, arg):
        try:
            if arg == 'DISABLED':
                d = 0x7F
            elif 0 <= int(arg) <= 100:
                d = int(arg)
            else:
                raise ValueError
        except (TypeError, ValueError):
            raise BadArgumentError

        self.interface.write_data(self.WAKEUP_ON_CHARGE_CMD, [d], True)
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetWakeUpOnCharge(self):
        ret = self.interface.read_data(self.WAKEUP_ON_CHARGE_CMD, 1)
        d = ret[0]

        if d == 0xFF:
            return {'data': 'DISABLED', 'error': 'NO_ERROR'}
        else:
            return {'data': d, 'error': 'NO_ERROR'}

    # input argument 1 - 65535 minutes activates watchdog, 0 disables watchdog
    @pijuice_error_return
    def SetWatchdog(self, minutes):
        try:
            d = int(minutes) & 0xFFFF
        except:
            raise BadArgumentError

        self.interface.write_data(self.WATCHDOG_ACTIVATION_CMD, [d & 0xFF, (d >> 8) & 0xFF])
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetWatchdog(self):
        d = self.interface.read_data(self.WATCHDOG_ACTIVATION_CMD, 2)
        return {'data': (d[1] << 8) | d[0], 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetSystemPowerSwitch(self, state):
        try:
            d = int(state) // 100
        except:
            raise BadArgumentError
        self.interface.write_data(self.SYSTEM_POWER_SWITCH_CTRL_CMD, [d])
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetSystemPowerSwitch(self):
        ret = self.interface.read_data(self.SYSTEM_POWER_SWITCH_CTRL_CMD, 1)
        return {'data': ret[0] * 100, 'error': 'NO_ERROR'}


class PiJuiceConfig(object):

    CHARGING_CONFIG_CMD = 0x51
    BATTERY_PROFILE_ID_CMD = 0x52
    BATTERY_PROFILE_CMD = 0x53
    BATTERY_TEMP_SENSE_CONFIG_CMD = 0x5D
    POWER_INPUTS_CONFIG_CMD = 0x5E
    RUN_PIN_CONFIG_CMD = 0x5F
    POWER_REGULATOR_CONFIG_CMD = 0x60
    LED_CONFIGURATION_CMD = 0x6A
    BUTTON_CONFIGURATION_CMD = 0x6E
    IO_CONFIGURATION_CMD = 0x72
    I2C_ADDRESS_CMD = 0x7C
    ID_EEPROM_WRITE_PROTECT_CTRL_CMD = 0x7E
    ID_EEPROM_ADDRESS_CMD = 0x7F
    RESET_TO_DEFAULT_CMD = 0xF0
    FIRMWARE_VERSION_CMD = 0xFD

    def __init__(self, interface):
        self.interface = interface

    def __enter__(self):
        # Just return this object so it can be used in a with statement
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        return False  # Don't suppress exceptions.

    @pijuice_error_return
    def SetChargingConfig(self, config, non_volatile=False):
        if not isinstance(config['charging_enabled'], bool):
            raise BadArgumentError

        nv = 0x80 if non_volatile else 0x00
        d = [nv | int(config['charging_enabled'])]
        self.interface.write_data(self.CHARGING_CONFIG_CMD, d, True)
        return {'error': 'NO_ERROR'}
    
    @pijuice_error_return
    def GetChargingConfig(self):
        ret = self.interface.read_data(self.CHARGING_CONFIG_CMD, 1)
        return {'data': {'charging_enabled': bool(ret[0] & 0x01)},
                'non_volatile': bool(ret['data'][0] & 0x80),
                'error': 'NO_ERROR'}

    batteryProfiles = ['BP6X', 'BP7X', 'SNN5843', 'LIPO8047109']

    @pijuice_error_return
    def SetBatteryProfile(self, profile):
        if profile == 'DEFAULT':
            profile_id = 0xFF
        elif profile == 'CUSTOM':
            profile_id = 0x0F
        elif profile in self.batteryProfiles:
            profile_id = self.batteryProfiles.index(profile)
        else:
            raise BadArgumentError
        self.interface.write_data(self.BATTERY_PROFILE_ID_CMD, [profile_id])
        return {'error': 'NO_ERROR'}

    batteryProfileSources = ['HOST', 'DIP_SWITCH', 'RESISTOR']
    batteryProfileValidity = ['VALID', 'INVALID']

    @pijuice_error_return
    def GetBatteryProfileStatus(self):
        ret = self.interface.read_data(self.BATTERY_PROFILE_ID_CMD, 1)
        profile_id = ret[0]
        if profile_id == 0xF0:
            return {'data': {'validity': 'DATA_WRITE_NOT_COMPLETED'}, 'error': 'NO_ERROR'}
        origin = 'CUSTOM' if (profile_id & 0x0F) == 0x0F else 'PREDEFINED'
        source = self.batteryProfileSources[(profile_id >> 4) & 0x03]
        validity = self.batteryProfileValidity[(profile_id >> 6) & 0x01]
        try:
            profile = self.batteryProfiles[(profile_id & 0x0F)]
        except:
            profile = 'UNKNOWN'
        return {'data': {'validity': validity, 'source': source, 'origin': origin, 'profile': profile},
                'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetBatteryProfile(self):
        d = self.interface.read_data(self.BATTERY_PROFILE_CMD, 14)
        if all(v == 0 for v in d):
            return {'data': 'INVALID', 'error': 'NO_ERROR'}
        profile = {
            'capacity': (d[1] << 8) | d[0],
            'chargeCurrent': d[2] * 75 + 550,
            'terminationCurrent': d[3] * 50 + 50,
            'regulationVoltage': d[4] * 20 + 3500,
            'cutoffVoltage': d[5] * 20,
            'tempCold': ctypes.c_byte(d[6]).value,
            'tempCool': ctypes.c_byte(d[7]).value,
            'tempWarm': ctypes.c_byte(d[8]).value,
            'tempHot': ctypes.c_byte(d[9]).value,
            'ntcB': (d[11] << 8) | d[10],
            'ntcResistance': ((d[13] << 8) | d[12]) * 10
        }
        return {'data': profile, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetCustomBatteryProfile(self, profile):
        try:
            c = profile['capacity']
            B = profile['ntcB']
            R = profile['ntcResistance'] // 10
            d = [
                c & 0xFF,
                (c >> 8) & 0xFF,
                int(round((profile['chargeCurrent'] - 550) // 75)),
                int(round((profile['terminationCurrent'] - 50) // 50)),
                int(round((profile['regulationVoltage'] - 3500) // 20)),
                int(round(profile['cutoffVoltage'] // 20)),
                ctypes.c_ubyte(profile['tempCold']).value,
                ctypes.c_ubyte(profile['tempCool']).value,
                ctypes.c_ubyte(profile['tempWarm']).value,
                ctypes.c_ubyte(profile['tempHot']).value,
                B & 0xFF,
                (B >> 8) & 0xFF,
                R & 0xFF,
                (R >> 8) & 0xFF
            ]
        except:
            raise BadArgumentError
        print(d)
        self.interface.write_data(self.BATTERY_PROFILE_CMD, d, True, 0.2)
        return {'error': 'NO_ERROR'}

    batteryTempSenseOptions = ['NOT_USED', 'NTC', 'ON_BOARD', 'AUTO_DETECT']

    @pijuice_error_return
    def GetBatteryTempSenseConfig(self):
        d = self.interface.read_data(self.BATTERY_TEMP_SENSE_CONFIG_CMD, 1)
        if d[0] < len(self.batteryTempSenseOptions):
            return {'data': self.batteryTempSenseOptions[d[0]], 'error': 'NO_ERROR'}
        else:
            raise UnknownDataError

    @pijuice_error_return
    def SetBatteryTempSenseConfig(self, config):
        try:
            ind = self.batteryTempSenseOptions.index(config)
        except ValueError:
            raise BadArgumentError

        self.interface.write_data(self.BATTERY_TEMP_SENSE_CONFIG_CMD, [ind], True)
        return {'error': 'NO_ERROR'}

    powerInputs = ['USB_MICRO', '5V_GPIO']
    usbMicroCurrentLimits = ['1.5A', '2.5A']
    usbMicroDPMs = list("{0:.2f}".format(4.2+0.08*x)+'V' for x in range(0, 8))

    @pijuice_error_return
    def SetPowerInputsConfig(self, config, non_volatile=False):
        try:
            nv = 0x80 if non_volatile else 0x00
            prec = 0x01 if (config['precedence'] == '5V_GPIO') else 0x00
            gpioInEn = 0x02 if config['gpio_in_enabled'] else 0x00
            noBatOn = 0x04 if config['no_battery_turn_on'] else 0x00
        except ValueError:
            raise BadArgumentError

        try:
            ind = self.usbMicroCurrentLimits.index(config['usb_micro_current_limit'])
            usbMicroLimit = int(ind) << 3
        except ValueError:
            raise InvalidUSBMicroCurrentLimitError

        try:
            ind = self.usbMicroDPMs.index(config['usb_micro_dpm'])
            dpm = (int(ind) & 0x07) << 4
        except ValueError:
            raise InvalidUSBMicroDPMError

        d = [nv | prec | gpioInEn | noBatOn | usbMicroLimit | dpm]
        self.interface.write_data(self.POWER_INPUTS_CONFIG_CMD, d, True)
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetPowerInputsConfig(self):
        d = self.interface.read_data(self.POWER_INPUTS_CONFIG_CMD, 1)
        config = {
            'precedence': self.powerInputs[d & 0x01],
            'gpio_in_enabled': bool(d & 0x02),
            'no_battery_turn_on': bool(d & 0x04),
            'usb_micro_current_limit': self.usbMicroCurrentLimits[(d >> 3) & 0x01],
            'usb_micro_dpm': self.usbMicroDPMs[(d >> 4) & 0x07]
        }
        return {'data': config, 'non_volatile': bool(d & 0x80), 'error': 'NO_ERROR'}

    buttons = ['SW' + str(i+1) for i in range(0, 3)]
    buttonEvents = ['PRESS', 'RELEASE', 'SINGLE_PRESS', 'DOUBLE_PRESS', 'LONG_PRESS1', 'LONG_PRESS2']

    @pijuice_error_return
    def GetButtonConfiguration(self, button):
        try:
            b = self.buttons.index(button)
        except ValueError:
            raise BadArgumentError

        d = self.interface.read_data(self.BUTTON_CONFIGURATION_CMD + b, 12)
        config = {}
        for i in range(0, len(self.buttonEvents)):
            config[self.buttonEvents[i]] = {}
            try:
                if d[i*2] == 0:
                    config[self.buttonEvents[i]]['function'] = 'NO_FUNC'
                elif d[i*2] & 0xF0 == 0:
                    config[self.buttonEvents[i]]['function'] = pijuice_hard_functions[(d[i*2] & 0x0F)-1]
                elif d[i*2] & 0xF0 == 0x10:
                    config[self.buttonEvents[i]]['function'] = pijuice_sys_functions[(d[i*2] & 0x0F)-1]
                elif d[i*2] & 0xF0 == 0x20:
                    config[self.buttonEvents[i]]['function'] = pijuice_user_functions[d[i*2] & 0x0F]
                else:
                    config[self.buttonEvents[i]]['function'] = 'UNKNOWN'
            except IndexError:
                config[self.buttonEvents[i]]['function'] = 'UNKNOWN'
            config[self.buttonEvents[i]]['parameter'] = d[i*2+1] * 100
        return {'data': config, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetButtonConfiguration(self, button, config):
        try:
            b = self.buttons.index(button)
        except ValueError:
            raise BadArgumentError

        data = [0x00] * (len(self.buttonEvents) * 2)
        for i in range(0, len(self.buttonEvents)):
            try:
                data[i*2] = pijuice_hard_functions.index(
                    config[self.buttonEvents[i]]['function']) + 0x01
            except ValueError:
                try:
                    data[i*2] = pijuice_sys_functions.index(
                        config[self.buttonEvents[i]]['function']) + 0x11
                except:
                    try:
                        data[i*2] = pijuice_user_functions.index(
                            config[self.buttonEvents[i]]['function']) + 0x20
                    except:
                        data[i*2] = 0
            data[i*2+1] = (int(config[self.buttonEvents[i]]['parameter']) // 100) & 0xff

        self.interface.write_data(self.BUTTON_CONFIGURATION_CMD + b, data, True, 0.4)
        return {'error': 'NO_ERROR'}

    leds = ['D1', 'D2']
    # XXX: Avoid setting ON_OFF_STATUS
    ledFunctionsOptions = ['NOT_USED', 'CHARGE_STATUS', 'USER_LED']
    ledFunctions = ['NOT_USED', 'CHARGE_STATUS', 'ON_OFF_STATUS', 'USER_LED']

    @pijuice_error_return
    def GetLedConfiguration(self, led):
        try:
            i = self.leds.index(led)
        except ValueError:
            raise BadArgumentError

        ret = self.interface.read_data(self.LED_CONFIGURATION_CMD + i, 4)
        config = {}

        try:
            config['function'] = self.ledFunctions[ret[0]]
        except (IndexError, KeyError):
            raise UnknownConfigError

        config['parameter'] = {
            'r': ret[1],
            'g': ret[2],
            'b': ret[3]
        }
        return {'data': config, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetLedConfiguration(self, led, config):
        try:
            i = self.leds.index(led)
            d = [
                self.ledFunctions.index(config['function']),
                int(config['parameter']['r']),
                int(config['parameter']['g']),
                int(config['parameter']['b'])
            ]
        except ValueError:
            raise BadArgumentError

        self.interface.write_data(self.LED_CONFIGURATION_CMD + i, d, True, 0.2)
        return {'error': 'NO_ERROR'}

    powerRegulatorModes = ['POWER_SOURCE_DETECTION', 'LDO', 'DCDC']

    @pijuice_error_return
    def GetPowerRegulatorMode(self):
        d = self.interface.read_data(self.POWER_REGULATOR_CONFIG_CMD, 1)

        if d[0] < len(self.powerRegulatorModes):
            return {'data': self.powerRegulatorModes[d[0]], 'error': 'NO_ERROR'}
        else:
            raise UnknownDataError

    @pijuice_error_return
    def SetPowerRegulatorMode(self, mode):
        try:
            ind = self.powerRegulatorModes.index(mode)
        except:
            raise BadArgumentError

        self.interface.write_data(self.POWER_REGULATOR_CONFIG_CMD, [ind], True)
        return {'error': 'NO_ERROR'}

    runPinConfigs = ['NOT_INSTALLED', 'INSTALLED']

    @pijuice_error_return
    def GetRunPinConfig(self):
        d = self.interface.read_data(self.RUN_PIN_CONFIG_CMD, 1)

        if d[0] < len(self.runPinConfigs):
            return {'data': self.runPinConfigs[d[0]], 'error': 'NO_ERROR'}
        else:
            raise UnknownDataError

    @pijuice_error_return
    def SetRunPinConfig(self, config):
        try:
            ind = self.runPinConfigs.index(config)
        except:
            raise BadArgumentError
        self.interface.write_data(self.RUN_PIN_CONFIG_CMD, [ind], True)
        return {'error': 'NO_ERROR'}

    ioModes = ['NOT_USED', 'ANALOG_IN', 'DIGITAL_IN', 'DIGITAL_OUT_PUSHPULL',
               'DIGITAL_IO_OPEN_DRAIN', 'PWM_OUT_PUSHPULL', 'PWM_OUT_OPEN_DRAIN']
    ioSupportedModes = {
            1: ['NOT_USED', 'ANALOG_IN', 'DIGITAL_IN', 'DIGITAL_OUT_PUSHPULL',
                'DIGITAL_IO_OPEN_DRAIN', 'PWM_OUT_PUSHPULL', 'PWM_OUT_OPEN_DRAIN'],

            2: ['NOT_USED', 'DIGITAL_IN', 'DIGITAL_OUT_PUSHPULL',
                'DIGITAL_IO_OPEN_DRAIN', 'PWM_OUT_PUSHPULL', 'PWM_OUT_OPEN_DRAIN']
        }
    ioPullOptions = ['NOPULL', 'PULLDOWN', 'PULLUP']
    ioConfigParams = {
        'DIGITAL_OUT_PUSHPULL': [{'name': 'value', 'type': 'int', 'min': 0, 'max': 1}],
        'DIGITAL_IO_OPEN_DRAIN': [{'name': 'value', 'type': 'int', 'min': 0, 'max': 1}],
        'PWM_OUT_PUSHPULL': [{'name': 'period', 'unit': 'us', 'type': 'int', 'min': 2, 'max': 65536 * 2},
                             {'name': 'duty_cycle', 'unit': '%', 'type': 'float', 'min': 0, 'max': 100}],
        'PWM_OUT_OPEN_DRAIN': [{'name': 'period', 'unit': 'us', 'type': 'int', 'min': 2, 'max': 65536 * 2},
                               {'name': 'duty_cycle', 'unit': '%', 'type': 'float', 'min': 0, 'max': 100}]
    }

    @pijuice_error_return
    def SetIoConfiguration(self, io_pin, config, non_volatile=False):
        d = [0x00, 0x00, 0x00, 0x00, 0x00]
        try:
            mode = self.ioModes.index(config['mode'])
            pull = self.ioPullOptions.index(config['pull'])
            nv = 0x80 if non_volatile else 0x00
            d[0] = (mode & 0x0F) | ((pull & 0x03) << 4) | nv

            if config['mode'] == 'DIGITAL_OUT_PUSHPULL' or config['mode'] == 'DIGITAL_IO_OPEN_DRAIN':
                d[1] = int(config['value']) & 0x01  # output value
            elif config['mode'] == 'PWM_OUT_PUSHPULL' or config['mode'] == 'PWM_OUT_OPEN_DRAIN':
                p = int(config['period'])
                if p >= 2:
                    p = p // 2 - 1
                else:
                    raise InvalidPeriodError
                d[1] = p & 0xFF
                d[2] = (p >> 8) & 0xFF
                d[3] = 0xFF
                d[4] = 0xFF
                dc = float(config['duty_cycle'])
                if dc < 0 or dc > 100:
                    raise InvalidConfigError
                elif dc < 100:
                    dci = int(dc*65534//100)
                    d[3] = dci & 0xFF
                    d[4] = (dci >> 8) & 0xFF
        except:
            raise InvalidConfigError
        self.interface.write_data(self.IO_CONFIGURATION_CMD + (io_pin - 1) * 5, d, True, 0.2)
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIoConfiguration(self, io_pin):
        d = self.interface.read_data(self.IO_CONFIGURATION_CMD + (io_pin - 1) * 5, 5)
        nv = bool(d[0] & 0x80)
        mode = self.ioModes[d[0] & 0x0F] if ((d[0] & 0x0F) < len(self.ioModes)) else 'UNKNOWN'
        pull = self.ioPullOptions[(d[0] >> 4) & 0x03] if (((d[0] >> 4) & 0x03) < len(self.ioPullOptions)) else 'UNKNOWN'
        if mode == 'DIGITAL_OUT_PUSHPULL' or mode == 'DIGITAL_IO_OPEN_DRAIN':
            return {'data': {'mode': mode, 'pull': pull, 'value': int(d[1])},
                    'non_volatile': nv, 'error': 'NO_ERROR'}
        elif mode == 'PWM_OUT_PUSHPULL' or mode == 'PWM_OUT_OPEN_DRAIN':
            per = ((d[1] | (d[2] << 8)) + 1) * 2
            dci = d[3] | (d[4] << 8)
            dc = float(dci) * 100 // 65534 if dci < 65535 else 100
            return {'data': {'mode': mode, 'pull': pull, 'period': per, 'duty_cycle': dc},
                    'non_volatile': nv, 'error': 'NO_ERROR'}
        else:
            return {'data': {'mode': mode, 'pull': pull},
                    'non_volatile': nv, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetAddress(self, slave):
        if slave not in (1, 2):
            raise BadArgumentError

        result = self.interface.read_data(self.I2C_ADDRESS_CMD + slave - 1, 1)
        return {'data': format(result[0], 'x'), 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetAddress(self, slave, hexAddress):
        try:
            adr = int(str(hexAddress), 16)
        except (TypeError, ValueError):
            raise BadArgumentError

        if slave not in (1, 2) or adr < 0 or adr > 0x7F:
            raise BadArgumentError

        self.interface.write_data(self.I2C_ADDRESS_CMD + slave - 1, [adr])
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIdEepromWriteProtect(self):
        ret = self.interface.read_data(self.ID_EEPROM_WRITE_PROTECT_CTRL_CMD, 1)
        status = True if ret[0] == 1 else False
        return {'data': status, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetIdEepromWriteProtect(self, status):
        if not isinstance(status, bool):
            raise BadArgumentError

        self.interface.write_data(self.ID_EEPROM_WRITE_PROTECT_CTRL_CMD, [int(status)], True)
        return {'error': 'NO_ERROR'}

    idEepromAddresses = ['50', '52']

    @pijuice_error_return
    def GetIdEepromAddress(self):
        ret = self.interface.read_data(self.ID_EEPROM_ADDRESS_CMD, 1)
        return {'data': format(ret[0], 'x'), 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetIdEepromAddress(self, hexAddress):
        if str(hexAddress) not in self.idEepromAddresses:
            raise BadArgumentError

        adr = int(str(hexAddress), 16)
        self.interface.write_data(self.ID_EEPROM_ADDRESS_CMD, [adr], True)
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetDefaultConfiguration(self):
        self.interface.write_data(self.RESET_TO_DEFAULT_CMD, [0xaa, 0x55, 0x0a, 0xa3])
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetFirmwareVersion(self):
        ret = self.interface.read_data(self.FIRMWARE_VERSION_CMD, 2)
        major_version = ret[0] >> 4
        minor_version = (ret[0] << 4 & 0xf0) >> 4
        version_str = '%i.%i' % (major_version, minor_version)
        return {'data': {'version': version_str, 'variant': format(ret[1], 'x')},
                'error': 'NO_ERROR'}

    @pijuice_error_return
    def RunTestCalibration(self):
        self.interface.write_data(248, [0x55, 0x26, 0xa0, 0x2b])


# Create an interface object for accessing PiJuice features via I2C bus.
class PiJuice(object):

    def __init__(self, bus=1, address=0x14):
        self.interface = PiJuiceInterface(bus, address)
        self.status = PiJuiceStatus(self.interface)
        self.config = PiJuiceConfig(self.interface)
        self.power = PiJuicePower(self.interface)
        self.rtcAlarm = PiJuiceRtcAlarm(self.interface)

    def __enter__(self):
        # Just return this object so it can be used in a with statement
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        return False  # Don't suppress exceptions.


def get_versions():
    import os
    try:
        p = PiJuice()
        firmware_version_dict = p.config.GetFirmwareVersion()
    except:
        firmware_version_dict = {}
    uname = os.uname()
    os_version = ' '.join((uname[0], uname[2], uname[3]))
    firmware_version = firmware_version_dict.get('data', {}).get('version')
    return __version__, firmware_version, os_version


if __name__ == '__main__':
    if sys.argv[1] == '--version':
        sw_version, fw_version, os_version = get_versions()
        print("Software version: %s" % sw_version)
        if fw_version is None:
            fw_version = "No connection to PiJuice"
        print("Firmware version: %s" % fw_version)
        print("OS version: %s" % os_version)
