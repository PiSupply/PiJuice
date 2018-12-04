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
pijuice_user_functions = ['USER_EVENT'] + ['USER_FUNC' + str(i + 1) for i in range(0, 15)]

IO_PINS_COUNT = 2
LEDS_COUNT = 2
SLAVE_COUNT = 2


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


class IOMode(Enum):
    NOT_USED = 0
    ANALOG_IN = 1
    DIGITAL_IN = 2
    DIGITAL_OUT_PUSHPULL = 3
    DIGITAL_IO_OPEN_DRAIN = 4
    PWM_OUT_PUSHPULL = 5
    PWM_OUT_OPEN_DRAIN = 6
    UNKNOWN = 7


class IOPullOption(Enum):
    NOPULL = 0
    PULLDOWN = 1
    PULLUP = 2
    UNKNOWN = 3


class LedFunction(Enum):
    # XXX: Avoid setting ON_OFF_STATUS
    NOT_USED = 0
    CHARGE_STATUS = 1
    ON_OFF_STATUS = 2
    USER_LED = 3


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


class PiJuiceHardFunction(Enum):  # Maybe unite all the functions?
    HARD_FUNC_POWER_ON = 0
    HARD_FUNC_POWER_OFF = 1
    HARD_FUNC_RESET = 1


class PiJuiceSysFunction(Enum):
    SYS_FUNC_HALT = 0
    SYS_FUNC_HALT_POW_OFF = 1
    SYS_FUNC_SYS_OFF_HALT = 2
    SYS_FUNC_REBOOT = 3


PiJuiceUserFunction = Enum('PiJuiceUserFunction', ['USER_EVENT'] + ['USER_FUNC{}'.format(x) for x in range(1, 16)])
PiJuiceNoFunction = Enum('PiJuiceNoFunction', ('NO_FUNC', 'UNKNOWN'))


class ButtonEventFunction:
    def __init__(self, event):
        self.event = event
        self._function = PiJuiceNoFunction.UNKNOWN
        self._param = 0

    @property
    def function(self):
        return self._function

    @function.setter
    def function(self, value):
        if not isinstance(value, (PiJuiceHardFunction, PiJuiceSysFunction, PiJuiceUserFunction, PiJuiceNoFunction)):
            raise BadArgumentError

        self._function = value

    @property
    def param(self):
        return self._param

    @param.setter
    def param(self, value):
        try:
            self._param = int(value)
        except (TypeError, ValueError):
            raise BadArgumentError


class Button:
    BUTTON_CONFIGURATION_CMD = 0x6E
    EVENT_STATE_PARAMS = [
        [0xf0, 0xff],
        [0x0f, 0xff],
        [0xff, 0xf0]
    ]

    def __init__(self, interface, index):
        self._interface = interface
        self.index = index
        self.name = 'SW{}'.format(self.index + 1)
        self._event_state = ButtonEvent.UNKNOWN
        self._events = [ButtonEventFunction(ButtonEvent(x)) for x in range(len(ButtonEvent))]

    @property
    def event_state(self):
        return self._event_state

    @event_state.setter
    def event_state(self, value):
        self._interface.write_data(Buttons.BUTTON_EVENT_CMD, self.EVENT_STATE_PARAMS[self.index])
        self._event_state = ButtonEvent.NO_EVENT

    @property
    def press_event(self):
        return self._events[ButtonEvent.PRESS.value]

    @property
    def release_event(self):
        return self._events[ButtonEvent.RELEASE.value]

    @property
    def single_press_event(self):
        return self._events[ButtonEvent.SINGLE_PRESS.value]

    @property
    def double_press_event(self):
        return self._events[ButtonEvent.DOUBLE_PRESS.value]

    @property
    def long_press_1_event(self):
        return self._events[ButtonEvent.LONG_PRESS1.value]

    @property
    def long_press_2_event(self):
        return self._events[ButtonEvent.LONG_PRESS2.value]

    def get_config(self):
        data = self._interface.read_data(self.BUTTON_CONFIGURATION_CMD + self.index, 12)

        for event_index in range(len(ButtonEvent)):
            self._events[event_index].param = data[event_index * 2 + 1] * 100
            data_func = data[event_index * 2]

            try:
                if data_func == 0:
                    self._events[event_index].function = PiJuiceNoFunction.NO_FUNC
                elif data_func & 0xf0 == 0:
                    self._events[event_index].function = PiJuiceHardFunction((data_func & 0x0f) - 1)
                elif data_func & 0xf0 == 0x10:
                    self._events[event_index].function = PiJuiceSysFunction((data_func & 0x0f) - 1)
                elif data_func & 0xf0 == 0x20:
                    self._events[event_index].function = PiJuiceUserFunction((data_func & 0x0f) - 1)
                else:
                    self._events[event_index].function = PiJuiceNoFunction.UNKNOWN
            except ValueError:
                self._events[event_index].function = PiJuiceNoFunction.UNKNOWN

    def set_config(self):
        data = [0] * (len(ButtonEvent) * 2)

        for event_index in range(len(ButtonEvent)):
            data[event_index * 2 + 1] = (self._events[event_index].param // 100) & 0xff

            if isinstance(self._events[event_index].function, PiJuiceHardFunction):
                data[event_index * 2] = self._events[event_index].function.value + 0x01
            elif isinstance(self._events[event_index].function, PiJuiceSysFunction):
                data[event_index * 2] = self._events[event_index].function.value + 0x11
            elif isinstance(self._events[event_index].function, PiJuiceUserFunction):
                data[event_index * 2] = self._events[event_index].function.value + 0x20
            else:
                data[event_index * 2] = 0

        self._interface.write_data(self.BUTTON_CONFIGURATION_CMD + self.index, data, True, 0.4)


class Buttons:
    BUTTON_EVENT_CMD = 0x45

    def __init__(self, interface):
        self._interface = interface

        self.sw1 = Button(self._interface, 0)
        self.sw2 = Button(self._interface, 1)
        self.sw3 = Button(self._interface, 2)

    def update_button_events(self):
        data = self._interface.read_data(self.BUTTON_EVENT_CMD, 2)

        try:
            self.sw1.event_state = ButtonEvent[data[0] & 0x0F]
        except ValueError:
            self.sw1.event_state = ButtonEvent.UNKNOWN

        try:
            self.sw2.event_state = ButtonEvent[(data[0] >> 4) & 0x0F]
        except ValueError:
            self.sw2.event_state = ButtonEvent.UNKNOWN

        try:
            self.sw3.event_state = ButtonEvent[data[1] & 0x0F]
        except ValueError:
            self.sw3.event_state = ButtonEvent.UNKNOWN


class Led:
    LED_STATE_CMD = 0x66
    LED_BLINK_CMD = 0x68
    LED_CONFIGURATION_CMD = 0x6A
    # First value is named blink_count because there is a built-in method named 'count'.
    LedBlink = namedtuple('LedBlink', ('blink_count', 'rgb1', 'period1', 'rgb2', 'period2'))
    RGB = namedtuple('RGB', ('r', 'g', 'b'))

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
        self._function = LedFunction.NOT_USED
        self._parameter = self.RGB(0, 0, 0)

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

        data = [led_blink.count & 0xFF] + led_blink.rgb1 * 1 + [(led_blink.period1 // 10) & 0xFF] + \
               led_blink.rgb2 * 1 + [(led_blink.period2 // 10) & 0xFF]
        self._interface.write_data(self.LED_BLINK_CMD + self.index, data)

    @property
    def function(self):
        return self._function

    @function.setter
    def function(self, value):
        if not isinstance(value, LedFunction):
            raise BadArgumentError

        self._function = value

    @property
    def parameter(self):
        return self._parameter

    @parameter.setter
    def parameter(self, value):
        if not isinstance(value, self.RGB):
            raise BadArgumentError

        for elem in value:
            if not isinstance(elem, int) or elem < 0 or elem > 255:
                raise BadArgumentError

        self._parameter = value

    def get_config(self):
        data = self._interface.read_data(self.LED_CONFIGURATION_CMD + self.index, 4)

        try:
            self.function = LedFunction(data[0])
        except ValueError:
            raise UnknownConfigError

        self.parameter = self.RGB(data[1], data[2], data[3])

    def set_config(self):
        data = [
            self.function.value,
            self.parameter.r,
            self.parameter.g,
            self.parameter.b
        ]
        self._interface.write_data(self.LED_CONFIGURATION_CMD + self.index, data, True, 0.2)


class Pin:
    IO_PIN_ACCESS_CMD = 0x75
    IO_CONFIGURATION_CMD = 0x72

    def __init__(self, interface, pin_index):
        """

        :param interface:
        :param pin_index: 0 for pin #1, 1 for pin #2
        """
        self._interface = interface
        self.index = pin_index

        self._non_volatile = False
        self._mode = IOMode.UNKNOWN
        self._pull_option = IOPullOption.UNKNOWN
        self._value = 0
        self._period = 2
        self._duty_cycle = 0.0

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
        duty_cycle = float(duty_cycle_int) * 100 // 65534 if duty_cycle_int < 65535 else 100.0
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

    @property
    def non_volatile(self):
        return self._non_volatile

    @non_volatile.setter
    def non_volatile(self, value):
        self._non_volatile = bool(value)

    @property
    def mode(self):
        return self._mode

    @mode.setter
    def mode(self, value):
        if not isinstance(value, IOMode):
            raise BadArgumentError
        self._mode = value

    @property
    def pull_option(self):
        return self._pull_option

    @pull_option.setter
    def pull_option(self, value):
        if not isinstance(value, IOPullOption):
            raise BadArgumentError
        self._pull_option = value

    @property
    def value(self):
        return self._value

    @value.setter
    def value(self, v):
        try:
            self._value = int(v)
        except (TypeError, ValueError):
            raise BadArgumentError

    @property
    def period(self):
        return self._period

    @period.setter
    def period(self, value):
        try:
            p = int(value)

            if p < 2 or p > 65536 * 2:
                raise ValueError

            self._period = p
        except (TypeError, ValueError):
            raise BadArgumentError

    @property
    def duty_cycle(self):
        return self._duty_cycle

    @duty_cycle.setter
    def duty_cycle(self, value):
        try:
            dc = float(value)

            if dc < 0 or dc > 100:
                raise ValueError

            self._duty_cycle = dc
        except (TypeError, ValueError):
            raise BadArgumentError

    def get_io_configuration(self):
        data = self._interface.read_data(self.IO_CONFIGURATION_CMD + self.index * 5, 5)
        self.non_volatile = bool(data[0] & 0x80)
        self.mode = IOMode(data[0] & 0x0f) if (data[0] & 0x0f) < len(IOMode) else IOMode.UNKNOWN
        self.pull_option = IOPullOption((data[0] >> 4) & 0x03) if ((data[0] >> 4) & 0x03) < len(IOPullOption) \
            else IOPullOption.UNKNOWN

        if self._mode in (IOMode.DIGITAL_IO_OPEN_DRAIN, IOMode.DIGITAL_OUT_PUSHPULL):
            self.value = int(data[1])
        elif self._mode in (IOMode.PWM_OUT_OPEN_DRAIN, IOMode.PWM_OUT_PUSHPULL):
            self.period = ((data[1] | (data[2] << 8)) + 1) * 2
            duty_cycle_int = data[3] | (data[4] << 8)
            self.duty_cycle = float(duty_cycle_int) * 100 // 65534 if duty_cycle_int < 65535 else 100.0

    def set_io_configuration(self):
        data = [0x00 for x in range(5)]
        nv = 0x80 if self.non_volatile else 0x00
        data[0] = (self.mode.value & 0x0F) | ((self.pull_option.value & 0x03) << 4) | nv

        if self.mode in (IOMode.DIGITAL_IO_OPEN_DRAIN, IOMode.DIGITAL_OUT_PUSHPULL):
            data[1] = self.value
        elif self.mode in (IOMode.PWM_OUT_PUSHPULL, IOMode.PWM_OUT_OPEN_DRAIN):
            p = self.period // 2 - 1
            data[1] = p & 0xFF
            data[2] = (p >> 8) & 0xFF
            data[3] = 0xFF
            data[4] = 0xFF
            duty_cycle_int = int(self.duty_cycle * 65534 // 100)
            data[3] = duty_cycle_int & 0xff
            data[4] = (duty_cycle_int >> 8) & 0xff

        self._interface.write_data(self.IO_CONFIGURATION_CMD + self.index * 5, data, True, 0.2)


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
            'SW1': self.buttons.sw1.event_state.name,
            'SW2': self.buttons.sw2.event_state.name,
            'SW3': self.buttons.sw3.event_state.name
        }
        return {'data': event, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def AcceptButtonEvent(self, button):
        if button == 'SW1':
            self.buttons.sw1.event_state = None
        elif button == 'SW2':
            self.buttons.sw2.event_state = None
        elif button == 'SW3':
            self.buttons.sw3.event_state = None
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
                        if data[i + 4] & ((0x01 << j) & 0xFF):
                            hours.append(i * 8 + j)

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


class PiJuicePower:
    WATCHDOG_ACTIVATION_CMD = 0x61
    POWER_OFF_CMD = 0x62
    WAKEUP_ON_CHARGE_CMD = 0x63
    SYSTEM_POWER_SWITCH_CTRL_CMD = 0x64

    def __init__(self, interface):
        self._interface = interface

        self._power_off = 0
        self._wake_up_on_charge = 0
        self._is_wake_up_on_charge_enabled = False
        self._watchdog = 0
        self._system_power_switch = 0

    def __enter__(self):
        # Just return this object so it can be used in a with statement
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        return False  # Don't suppress exceptions.

    @property
    def power_off(self):
        self._power_off = self._interface.read_data(self.POWER_OFF_CMD, 1)[0]
        return self._power_off

    @power_off.setter
    def power_off(self, value):
        self._interface.write_data(self.POWER_OFF_CMD, [value])
        self._power_off = value

    @property
    def wake_up_on_charge(self):
        data = self._interface.read_data(self.WAKEUP_ON_CHARGE_CMD, 1)[0]
        self._is_wake_up_on_charge_enabled = (data != 0xff)
        self._wake_up_on_charge = data
        return data

    @wake_up_on_charge.setter
    def wake_up_on_charge(self, value):
        if not isinstance(value, int) or value > 100 or value < 0:
            raise BadArgumentError

        self._interface.write_data(self.WAKEUP_ON_CHARGE_CMD, [value], True)
        self._wake_up_on_charge = value

    @property
    def wake_up_on_charge_enabled(self):
        data = self._interface.read_data(self.WAKEUP_ON_CHARGE_CMD, 1)[0]
        self._is_wake_up_on_charge_enabled = (data != 0xff)
        return self._is_wake_up_on_charge_enabled

    def disable_wake_up_on_charge(self):
        self._interface.write_data(self.WAKEUP_ON_CHARGE_CMD, [0x7f], True)
        self._is_wake_up_on_charge_enabled = False

    @property
    def watchdog(self):
        data = self._interface.read_data(self.WATCHDOG_ACTIVATION_CMD, 2)
        self._watchdog = (data[1] << 8) | data[0]
        return self._watchdog

    @watchdog.setter
    def watchdog(self, value):
        if not isinstance(value, int) or value > 65535 or value < 0:
            raise BadArgumentError

        self._interface.write_data(self.WATCHDOG_ACTIVATION_CMD, [value & 0xFF, (value >> 8) & 0xFF])
        self._watchdog = value

    @property
    def system_power_switch(self):
        data = self._interface.read_data(self.SYSTEM_POWER_SWITCH_CTRL_CMD, 1)
        self._system_power_switch = data[0] * 100
        return self._system_power_switch

    @system_power_switch.setter
    def system_power_switch(self, value):
        if not isinstance(value, int):
            raise BadArgumentError

        data = value // 100  # milliampere -> deciampere
        self._interface.write_data(self.SYSTEM_POWER_SWITCH_CTRL_CMD, [data])
        self._system_power_switch = value

    @pijuice_error_return
    def SetPowerOff(self, delay):
        self.power_off = delay
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetPowerOff(self):
        return {'data': self.power_off, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetWakeUpOnCharge(self, arg):
        try:
            if arg == 'DISABLED':
                self.disable_wake_up_on_charge()
            elif 0 <= int(arg) <= 100:
                self.wake_up_on_charge = int(arg)
            else:
                raise ValueError
        except (TypeError, ValueError):
            raise BadArgumentError

        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetWakeUpOnCharge(self):
        data = self.wake_up_on_charge

        if not self._is_wake_up_on_charge_enabled:
            return {'data': 'DISABLED', 'error': 'NO_ERROR'}
        else:
            return {'data': data, 'error': 'NO_ERROR'}

    # input argument 1 - 65535 minutes activates watchdog, 0 disables watchdog
    @pijuice_error_return
    def SetWatchdog(self, minutes):
        try:
            d = int(minutes) & 0xFFFF
        except (TypeError, ValueError):
            raise BadArgumentError

        self.watchdog = d
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetWatchdog(self):
        return {'data': self.watchdog, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetSystemPowerSwitch(self, state):
        try:
            d = int(state)
        except (TypeError, ValueError):
            raise BadArgumentError

        self.system_power_switch = d
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetSystemPowerSwitch(self):
        return {'data': self.system_power_switch, 'error': 'NO_ERROR'}


class FirmwareVersion:
    FIRMWARE_VERSION_CMD = 0xFD

    def __init__(self, interface):
        self._interface = interface
        self.major = 0
        self.minor = 0
        self.variant = '0'

        self.update_version()

    def update_version(self):
        data = self._interface.read_data(self.FIRMWARE_VERSION_CMD, 2)
        self.major = data[0] >> 4
        self.minor = (data[0] << 4 & 0xf0) >> 4
        self.variant = format(data[1], 'x')

    def __str__(self):
        return '.'.join((str(x) for x in (self.major, self.minor)))


class IdEepromAddress(Enum):
    _50 = 0x50
    _52 = 0x52


class Slave:
    I2C_ADDRESS_CMD = 0x7C

    def __init__(self, interface, index):
        self._interface = interface
        self.index = index

    @property
    def address(self):
        return self._interface.read_data(self.I2C_ADDRESS_CMD + self.index, 1)[0]

    @address.setter
    def address(self, value):
        if not isinstance(value, int) or value < 0 or value > 0x7f:
            raise BadArgumentError

        self._interface.write_data(self.I2C_ADDRESS_CMD + self.index, [value])


class RunPinConfig(Enum):
    NOT_INSTALLED = 0
    INSTALLED = 1


class PowerRegulatorMode(Enum):
    POWER_SOURCE_DETECTION = 0
    LDO = 1
    DCDC = 2


class PowerInput(Enum):
    USB_MICRO = 0
    _5V_GPIO = 1  # Was 5V_GPIO in previous code


class PowerInputsConfig:
    POWER_INPUTS_CONFIG_CMD = 0x5E
    USB_MICRO_CURRENT_LIMITS = ('1.5A', '2.5A')
    USB_MICRO_DPMS = tuple("{0:.2f}V".format(4.2 + 0.08 * x) for x in range(8))

    def __init__(self, interface):
        self._interface = interface

        self._non_volatile = False
        self._precedence = PowerInput.USB_MICRO
        self._gpio_in_enabled = False
        self._no_battery_turn_on = False
        self._usb_micro_current_limit = self.USB_MICRO_CURRENT_LIMITS[0]
        self._usb_micro_dpm = self.USB_MICRO_DPMS[0]

    @property
    def non_volatile(self):
        return self._non_volatile

    @non_volatile.setter
    def non_volatile(self, value):
        self._non_volatile = bool(value)

    @property
    def precedence(self):
        return self._precedence

    @precedence.setter
    def precedence(self, value):
        if not isinstance(value, PowerInput):
            raise BadArgumentError

        self._precedence = value

    @property
    def gpio_in_enabled(self):
        return self._gpio_in_enabled

    @gpio_in_enabled.setter
    def gpio_in_enabled(self, value):
        self._gpio_in_enabled = bool(value)

    @property
    def no_battery_turn_on(self):
        return self._no_battery_turn_on

    @no_battery_turn_on.setter
    def no_battery_turn_on(self, value):
        self._no_battery_turn_on = bool(value)

    @property
    def usb_micro_current_limit(self):
        return self._usb_micro_current_limit

    @usb_micro_current_limit.setter
    def usb_micro_current_limit(self, value):
        if value not in self.USB_MICRO_CURRENT_LIMITS:
            raise BadArgumentError

        self._usb_micro_current_limit = value

    @property
    def usb_micro_dpm(self):
        return self._usb_micro_dpm

    @usb_micro_dpm.setter
    def usb_micro_dpm(self, value):
        if value not in self.USB_MICRO_DPMS:
            raise BadArgumentError

        self._usb_micro_dpm = value

    def get_config(self):
        data = self._interface.read_data(self.POWER_INPUTS_CONFIG_CMD, 1)[0]
        self.non_volatile = bool(data & 0x80)
        self.precedence = PowerInput(data & 0x01)
        self.gpio_in_enabled = bool(data & 0x02)
        self.no_battery_turn_on = bool(data & 0x04)
        self.usb_micro_current_limit = self.USB_MICRO_CURRENT_LIMITS[(data >> 3) & 0x01]
        self.usb_micro_dpm = self.USB_MICRO_DPMS[(data >> 4) & 0x07]

    def set_config(self):
        non_volatile = 0x80 if self.non_volatile else 0x00
        precedence = 0x01 if self.precedence == PowerInput._5V_GPIO else 0x00
        gpio_in_enabled = 0x02 if self.gpio_in_enabled else 0x00
        no_battery_on = 0x04 if self.no_battery_turn_on else 0x00
        usb_micro_limit = self.USB_MICRO_CURRENT_LIMITS.index(self.usb_micro_current_limit) << 3
        usb_dpm = (self.USB_MICRO_DPMS.index(self.usb_micro_dpm) & 0x07) << 4

        data = [non_volatile | precedence | gpio_in_enabled | no_battery_on | usb_micro_limit | usb_dpm]
        self._interface.write_data(self.POWER_INPUTS_CONFIG_CMD, data, True)


class BatteryTempSenseOption(Enum):
    NOT_USED = 0
    NTC = 1
    ON_BOARD = 2
    AUTO_DETECT = 3


class BatteryProfileId(Enum):
    BP6X = 0
    BP7X = 1
    SNN5843 = 2
    LIPO8047109 = 3
    DEFAULT = 0xFF
    CUSTOM = 0x0F
    UNKNOWN = -1


class BatteryProfileOrigin(Enum):
    PREDEFINED = 0
    CUSTOM = 1


class BatteryProfileSource(Enum):
    HOST = 0
    DIP_SWITCH = 1
    RESISTOR = 2


class BatteryProfileValidity(Enum):
    VALID = 0
    INVALID = 1
    DATA_WRITE_NOT_COMPLETED = -1


class BatteryProfile:
    BATTERY_PROFILE_ID_CMD = 0x52
    BATTERY_PROFILE_CMD = 0x53

    def __init__(self, interface):
        self._interface = interface

        self._origin = BatteryProfileOrigin.PREDEFINED
        self._source = BatteryProfileSource.HOST
        self._validity = BatteryProfileValidity.VALID
        self._profile_id = BatteryProfileId.DEFAULT

        self._capacity = 0
        self._charge_current = 0
        self._termination_current = 0
        self._regulation_voltage = 0
        self._cutoff_voltage = 0
        self._temp_cold = 0
        self._temp_cool = 0
        self._temp_warm = 0
        self._temp_hot = 0
        self._ntc_b = 0
        self._ntc_resistance = 0

        self.is_valid = True

    @property
    def origin(self):
        return self._origin

    @origin.setter
    def origin(self, value):
        if not isinstance(value, BatteryProfileOrigin):
            raise BadArgumentError
        self._origin = value

    @property
    def source(self):
        return self._source

    @source.setter
    def source(self, value):
        if not isinstance(value, BatteryProfileSource):
            raise BadArgumentError
        self._source = value

    @property
    def validity(self):
        return self._validity

    @validity.setter
    def validity(self, value):
        if not isinstance(value, BatteryProfileValidity):
            raise BadArgumentError
        self._validity = value

    @property
    def profile_id(self):
        return self._profile_id

    @profile_id.setter
    def profile_id(self, value):
        if not isinstance(value, BatteryProfileId):
            raise BadArgumentError
        self._profile_id = value

    @property
    def capacity(self):
        return self._capacity

    @capacity.setter
    def capacity(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        self._capacity = value

    @property
    def charge_current(self):
        return self._charge_current

    @charge_current.setter
    def charge_current(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        self._charge_current = value

    @property
    def termination_current(self):
        return self._termination_current

    @termination_current.setter
    def termination_current(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        self._termination_current = value

    @property
    def regulation_voltage(self):
        return self._regulation_voltage

    @regulation_voltage.setter
    def regulation_voltage(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        self._regulation_voltage = value

    @property
    def cutoff_voltage(self):
        return self._cutoff_voltage

    @cutoff_voltage.setter
    def cutoff_voltage(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        self._cutoff_voltage = value

    @property
    def temp_cold(self):
        return self._temp_cold

    @temp_cold.setter
    def temp_cold(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        self._temp_cold = value

    @property
    def temp_cool(self):
        return self._temp_cool

    @temp_cool.setter
    def temp_cool(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        self._temp_cool = value

    @property
    def temp_warm(self):
        return self._temp_warm

    @temp_warm.setter
    def temp_warm(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        self._temp_warm = value

    @property
    def temp_hot(self):
        return self._temp_hot

    @temp_hot.setter
    def temp_hot(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        self._temp_hot = value

    @property
    def ntc_b(self):
        return self._ntc_b

    @ntc_b.setter
    def ntc_b(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        self._ntc_b = value

    @property
    def ntc_resistance(self):
        return self._ntc_resistance

    @ntc_resistance.setter
    def ntc_resistance(self, value):
        if not isinstance(value, int):
            raise BadArgumentError
        self._ntc_resistance = value

    def get_status(self):
        data = self._interface.read_data(self.BATTERY_PROFILE_ID_CMD, 1)[0]

        if data == 0xf0:
            self.validity = BatteryProfileValidity.DATA_WRITE_NOT_COMPLETED
        else:
            self.source = BatteryProfileSource((data >> 4) & 0x03)
            self.validity = BatteryProfileValidity((data >> 6) & 0x01)

            try:
                self.profile_id = BatteryProfileId(data & 0x0f)
            except ValueError:
                self.profile_id = BatteryProfileId.UNKNOWN

            self.origin = BatteryProfileOrigin.CUSTOM if \
                self.profile_id == BatteryProfileId.CUSTOM else BatteryProfileOrigin.PREDEFINED

    def set_status(self):
        self._interface.write_data(self.BATTERY_PROFILE_ID_CMD, [self.profile_id.value])

    def get_custom_profile(self):
        data = self._interface.read_data(self.BATTERY_PROFILE_CMD, 14)
        self.is_valid = not all(v == 0 for v in data)
        self.capacity = (data[1] << 8) | data[0]
        self.charge_current = data[2] * 75 + 550
        self.termination_current = data[3] * 50 + 50
        self.regulation_voltage = data[4] * 20 + 3500
        self.cutoff_voltage = data[5] * 20
        self.temp_cold = ctypes.c_byte(data[6]).value
        self.temp_cool = ctypes.c_byte(data[7]).value
        self.temp_warm = ctypes.c_byte(data[8]).value
        self.temp_hot = ctypes.c_byte(data[9]).value
        self.ntc_b = (data[11] << 8) | data[10]
        self.ntc_resistance = ((data[13] << 8) | data[12]) * 10

    def set_custom_profile(self):
        data = [
            self.capacity & 0xff,
            (self.capacity >> 8) & 0xff,
            (self.charge_current - 550) // 75,
            (self.termination_current - 50) // 50,
            (self.regulation_voltage - 3500) // 20,
            self.cutoff_voltage // 20,
            ctypes.c_ubyte(self.temp_cold).value,
            ctypes.c_ubyte(self.temp_cool).value,
            ctypes.c_ubyte(self.temp_warm).value,
            ctypes.c_ubyte(self.temp_hot).value,
            self.ntc_b & 0xff,
            (self.ntc_b >> 8) & 0xff,
            (self.ntc_resistance // 10) & 0xff,
            ((self.ntc_resistance // 10) >> 8) & 0xff
        ]
        self._interface.write_data(self.BATTERY_PROFILE_CMD, data, True, 0.2)


class ChargingConfig:
    CHARGING_CONFIG_CMD = 0x51

    def __init__(self, interface):
        self._interface = interface
        self._charging_enabled = False
        self._non_volatile = False

    @property
    def charging_enabled(self):
        return self._charging_enabled

    @charging_enabled.setter
    def charging_enabled(self, value):
        self._charging_enabled = bool(value)

    @property
    def non_volatile(self):
        return self._non_volatile

    @non_volatile.setter
    def non_volatile(self, value):
        self._non_volatile = bool(value)

    def get_config(self):
        data = self._interface.read_data(self.CHARGING_CONFIG_CMD, 1)[0]
        self.charging_enabled = bool(data[0] & 0x01)
        self.non_volatile = bool(data[0] & 0x80)

    def set_config(self):
        non_volatile = 0x80 if self.non_volatile else 0x00
        self._interface.write_data(self.CHARGING_CONFIG_CMD, [non_volatile | int(self.charging_enabled)], True)


class PiJuiceConfig:
    BATTERY_TEMP_SENSE_CONFIG_CMD = 0x5D
    RUN_PIN_CONFIG_CMD = 0x5F
    POWER_REGULATOR_CONFIG_CMD = 0x60
    BUTTON_CONFIGURATION_CMD = 0x6E
    ID_EEPROM_WRITE_PROTECT_CTRL_CMD = 0x7E
    ID_EEPROM_ADDRESS_CMD = 0x7F
    RESET_TO_DEFAULT_CMD = 0xF0
    FIRMWARE_VERSION_CMD = 0xFD

    def __init__(self, interface):
        self._interface = interface
        self._battery_temp_sense = BatteryTempSenseOption.NOT_USED
        self.battery_profile = BatteryProfile(self._interface)
        self.charging_config = ChargingConfig(self._interface)
        self.firmware_version = FirmwareVersion(self._interface)
        self.power_inputs_config = PowerInputsConfig(self._interface)
        self.buttons = Buttons(self._interface)
        self.leds = [Led(self._interface, x) for x in
                     range(LEDS_COUNT)]  # TODO: don't forget to modify other files, since .leds was ['D1', 'D2'] before
        self.pins = [Pin(self._interface, x) for x in range(IO_PINS_COUNT)]
        self.slaves = [Slave(self._interface, x) for x in range(SLAVE_COUNT)]

    def __enter__(self):
        # Just return this object so it can be used in a with statement
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        return False  # Don't suppress exceptions.

    def set_default_configuration(self):
        self._interface.write_data(self.RESET_TO_DEFAULT_CMD, [0xaa, 0x55, 0x0a, 0xa3])

    def run_test_calibration(self):
        self._interface.write_data(248, [0x55, 0x26, 0xa0, 0x2b])

    @property
    def id_eeprom_address(self):
        data = self._interface.read_data(self.ID_EEPROM_ADDRESS_CMD, 1)[0]
        return IdEepromAddress(data)

    @id_eeprom_address.setter
    def id_eeprom_address(self, value):
        if not isinstance(value, IdEepromAddress):
            raise BadArgumentError

        self._interface.write_data(self.ID_EEPROM_ADDRESS_CMD, [value.value], True)

    @property
    def id_eeprom_write_protect(self):
        return bool(self._interface.read_data(self.ID_EEPROM_WRITE_PROTECT_CTRL_CMD, 1)[0])

    @id_eeprom_write_protect.setter
    def id_eeprom_write_protect(self, value):
        if not isinstance(value, bool):
            raise BadArgumentError

        self._interface.write_data(self.ID_EEPROM_WRITE_PROTECT_CTRL_CMD, [int(value)], True)

    @property
    def run_pin_config(self):
        data = self._interface.read_data(self.RUN_PIN_CONFIG_CMD, 1)[0]

        try:
            return RunPinConfig(data)
        except ValueError:
            raise UnknownDataError

    @run_pin_config.setter
    def run_pin_config(self, value):
        if not isinstance(value, RunPinConfig):
            raise BadArgumentError

        self._interface.write_data(self.RUN_PIN_CONFIG_CMD, [value.value], True)

    @property
    def power_regulator_mode(self):
        data = self._interface.read_data(self.POWER_REGULATOR_CONFIG_CMD, 1)

        try:
            return PowerRegulatorMode(data[0])
        except ValueError:
            raise UnknownDataError

    @power_regulator_mode.setter
    def power_regulator_mode(self, value):
        if not isinstance(value, PowerRegulatorMode):
            raise BadArgumentError

        self._interface.write_data(self.POWER_REGULATOR_CONFIG_CMD, [value.value], True)

    @property
    def battery_temp_sense(self):
        data = self._interface.read_data(self.BATTERY_TEMP_SENSE_CONFIG_CMD, 1)[0]

        try:
            self._battery_temp_sense = BatteryTempSenseOption(data)
            return self._battery_temp_sense
        except ValueError:
            raise UnknownDataError

    @battery_temp_sense.setter
    def battery_temp_sense(self, value):
        if not isinstance(value, BatteryTempSenseOption):
            raise BadArgumentError

        self._battery_temp_sense = value
        self._interface.write_data(self.BATTERY_TEMP_SENSE_CONFIG_CMD, [self._battery_temp_sense.value], True)

    @pijuice_error_return
    def SetChargingConfig(self, config, non_volatile=False):
        self.charging_config.non_volatile = non_volatile
        self.charging_config.charging_enabled = config['charging_enabled']
        self.charging_config.set_config()
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetChargingConfig(self):
        self.charging_config.get_config()
        return {'data': {'charging_enabled': self.charging_config.charging_enabled},
                'non_volatile': self.charging_config.non_volatile,
                'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetBatteryProfile(self, profile):
        self.battery_profile.profile_id = BatteryProfileId[profile]
        self.battery_profile.set_status()
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetBatteryProfileStatus(self):
        self.battery_profile.get_status()

        if self.battery_profile.validity == BatteryProfileValidity.DATA_WRITE_NOT_COMPLETED:
            return {'data': {'validity': self.battery_profile.validity.name}, 'error': 'NO_ERROR'}

        return {
            'data': {
                'origin': self.battery_profile.origin.name,
                'source': self.battery_profile.source.name,
                'profile': self.battery_profile.profile_id.name,
                'validity': self.battery_profile.validity.name
            },
            'error': 'NO_ERROR'
        }

    @pijuice_error_return
    def GetBatteryProfile(self):
        self.battery_profile.get_custom_profile()

        if not self.battery_profile.is_valid:
            return {'data': 'INVALID', 'error': 'NO_ERROR'}

        profile = {
            'capacity': self.battery_profile.capacity,
            'chargeCurrent': self.battery_profile.charge_current,
            'terminationCurrent': self.battery_profile.termination_current,
            'regulationVoltage': self.battery_profile.regulation_voltage,
            'cutoffVoltage': self.battery_profile.cutoff_voltage,
            'tempCold': self.battery_profile.temp_cold,
            'tempCool': self.battery_profile.temp_cool,
            'tempWarm': self.battery_profile.temp_warm,
            'tempHot': self.battery_profile.temp_hot,
            'ntcB': self.battery_profile.ntc_b,
            'ntcResistance': self.battery_profile.ntc_resistance
        }
        return {'data': profile, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetCustomBatteryProfile(self, profile):
        self.battery_profile.capacity = profile['capacity']
        self.battery_profile.ntc_b = profile['ntcB']
        self.battery_profile.ntc_resistance = profile['ntcResistance']
        self.battery_profile.charge_current = profile['chargeCurrent']
        self.battery_profile.termination_current = profile['termiationCurrent']
        self.battery_profile.regulation_voltage = profile['regulationVoltage']
        self.battery_profile.cutoff_voltage = profile['cutoffVoltage']
        self.battery_profile.temp_hot = profile['tempHot']
        self.battery_profile.temp_cold = profile['tempCold']
        self.battery_profile.temp_cool = profile['tempCool']
        self.battery_profile.temp_warm = profile['tempWarm']
        self.battery_profile.set_custom_profile()
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetBatteryTempSenseConfig(self):
        return {'data': self.battery_temp_sense.name, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetBatteryTempSenseConfig(self, config):
        try:
            self.battery_temp_sense = BatteryTempSenseOption[config]
            return {'error': 'NO_ERROR'}
        except ValueError:
            raise BadArgumentError

    @pijuice_error_return
    def SetPowerInputsConfig(self, config, non_volatile=False):
        self.power_inputs_config.non_volatile = non_volatile
        self.power_inputs_config.precedence = PowerInput._5V_GPIO if \
            config['precedence'] == '5V_GPIO' else PowerInput[config['precedence']]
        self.power_inputs_config.gpio_in_enabled = config['gpio_in_enabled']
        self.power_inputs_config.no_battery_turn_on = config['no_battery_turn_on']
        self.power_inputs_config.usb_micro_current_limit = config['usb_micro_current_limit']
        self.power_inputs_config.usb_micro_dpm = config['usb_micro_dpm']
        self.power_inputs_config.set_config()
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetPowerInputsConfig(self):
        self.power_inputs_config.get_config()
        precedence = self.power_inputs_config.precedence.name if \
            self.power_inputs_config.precedence != PowerInput._5V_GPIO else '5V_GPIO'
        config = {
            'precedence': precedence,
            'gpio_in_enabled': self.power_inputs_config.gpio_in_enabled,
            'no_battery_turn_on': self.power_inputs_config.no_battery_turn_on,
            'usb_micro_current_limit': self.power_inputs_config.usb_micro_current_limit,
            'usb_micro_dpm': self.power_inputs_config.usb_micro_dpm
        }
        return {'data': config, 'non_volatile': self.power_inputs_config.non_volatile, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetButtonConfiguration(self, button):
        if button == 'SW1':
            b = self.buttons.sw1
        elif button == 'SW2':
            b = self.buttons.sw2
        elif button == 'SW3':
            b = self.buttons.sw3
        else:
            raise BadArgumentError

        config = {}
        b.get_config()

        for index in range(len(ButtonEvent)):
            event = b._events[index]
            config[ButtonEvent[index]] = {'function': event.function.name, 'parameter': event.param}

        return {'data': config, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetButtonConfiguration(self, button, config):
        if button == 'SW1':
            b = self.buttons.sw1
        elif button == 'SW2':
            b = self.buttons.sw2
        elif button == 'SW3':
            b = self.buttons.sw3
        else:
            raise BadArgumentError

        for index in range(len(ButtonEvent)):
            b._events[index].param = int(config[ButtonEvent(index).name]['parameter'])
            func_from_cfg = config[ButtonEvent(index).name]['function']
            func = PiJuiceNoFunction.NO_FUNC

            try:
                func = PiJuiceHardFunction[func_from_cfg]
            except ValueError:
                try:
                    func = PiJuiceSysFunction[func_from_cfg]
                except ValueError:
                    try:
                        func = PiJuiceUserFunction[func_from_cfg]
                    except ValueError:
                        pass

            b._events[index].function = func

        b.set_config()
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetLedConfiguration(self, led):
        led_object = None

        for l in self.leds:
            if led == l.name:
                led_object = l
                break

        if not led_object:
            raise BadArgumentError

        led_object.get_config()
        data = {
            'function': led_object.name,
            'parameter': {
                'r': led_object.parameter.r,
                'g': led_object.parameter.g,
                'b': led_object.parameter.b
            }
        }
        return {'data': data, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetLedConfiguration(self, led, config):
        led_object = None

        for l in self.leds:
            if led == l.name:
                led_object = l
                break

        if not led_object:
            raise BadArgumentError

        led_object.function = LedFunction[config['function']]
        led_object.parameter = Led.RGB(
            int(config['parameter']['r']),
            int(config['parameter']['g']),
            int(config['parameter']['b'])
        )
        led_object.set_config()
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetPowerRegulatorMode(self):
        return {'data': self.power_regulator_mode.name, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetPowerRegulatorMode(self, mode):
        try:
            self.power_regulator_mode = PowerRegulatorMode[mode]
            return {'error': 'NO_ERROR'}
        except ValueError:
            raise BadArgumentError

    @pijuice_error_return
    def GetRunPinConfig(self):
        return {'data': self.run_pin_config, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetRunPinConfig(self, config):
        try:
            self.run_pin_config = RunPinConfig[config]
            return {'error': 'NO_ERROR'}
        except (PiJuiceError, ValueError):
            raise BadArgumentError

    @pijuice_error_return
    def SetIoConfiguration(self, io_pin, config, non_volatile=False):
        if io_pin not in range(1, IO_PINS_COUNT + 1):
            raise BadArgumentError

        try:
            pin = self.pins[io_pin - 1]
            pin.mode = IOMode[config['mode']]
            pin.pull_option = IOPullOption[config['pull']]
            pin.non_volatile = non_volatile

            if pin.mode in (IOMode.DIGITAL_IO_OPEN_DRAIN, IOMode.DIGITAL_OUT_PUSHPULL):
                pin.value = int(config['value']) & 0x01
            elif pin.mode in (IOMode.PWM_OUT_OPEN_DRAIN, IOMode.PWM_OUT_PUSHPULL):
                pin.period = config['period']
                pin.duty_cycle = config['duty_cycle']

            pin.set_io_configuration()
            return {'error': 'NO_ERROR'}
        except (PiJuiceError, KeyError, TypeError):
            raise InvalidConfigError

    @pijuice_error_return
    def GetIoConfiguration(self, io_pin):
        if io_pin not in range(1, IO_PINS_COUNT + 1):
            raise BadArgumentError

        pin = self.pins[io_pin - 1]
        pin.get_io_configuration()
        result = {
            'data': {
                'mode': pin.mode.name,
                'pull': pin.pull_option.name
            },
            'non_volatile': pin.non_volatile,
            'error': 'NO_ERROR'
        }

        if pin.mode in (IOMode.DIGITAL_OUT_PUSHPULL, IOMode.DIGITAL_IO_OPEN_DRAIN):
            result['data']['value'] = pin.value
        elif pin.mode in (IOMode.PWM_OUT_OPEN_DRAIN, IOMode.PWM_OUT_OPEN_DRAIN):
            result['data']['period'] = pin.period
            result['data']['duty_cycle'] = pin.duty_cycle

        return result

    @pijuice_error_return
    def GetAddress(self, slave):
        if slave not in range(1, SLAVE_COUNT + 1):
            raise BadArgumentError

        return {'data': format(self.slaves[slave - 1].address, 'x'), 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetAddress(self, slave, hexAddress):
        try:
            adr = int(str(hexAddress), 16)
        except (TypeError, ValueError):
            raise BadArgumentError

        if slave not in range(1, SLAVE_COUNT + 1) or adr < 0 or adr > 0x7F:
            raise BadArgumentError

        self.slaves[slave - 1].address = adr
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIdEepromWriteProtect(self):
        return {'data': self.id_eeprom_write_protect, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetIdEepromWriteProtect(self, status):
        self.id_eeprom_write_protect = status
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIdEepromAddress(self):
        return {'data': format(self.id_eeprom_address.value, 'x'), 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetIdEepromAddress(self, hexAddress):
        try:
            self.id_eeprom_address = IdEepromAddress(int(str(hexAddress), 16))
            return {'error': 'NO_ERROR'}
        except ValueError:
            raise BadArgumentError

    @pijuice_error_return
    def SetDefaultConfiguration(self):
        self.set_default_configuration()
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetFirmwareVersion(self):
        return {'data': {'version': str(self.firmware_version), 'variant': self.firmware_version.variant},
                'error': 'NO_ERROR'}

    @pijuice_error_return
    def RunTestCalibration(self):
        self.run_test_calibration()


# Create an interface object for accessing PiJuice features via I2C bus.
class PiJuice:
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
        firmware_version = str(PiJuice().config.firmware_version)
    except:
        firmware_version = None

    uname = os.uname()
    os_version = ' '.join((uname[0], uname[2], uname[3]))
    return __version__, firmware_version, os_version


if __name__ == '__main__':
    if sys.argv[1] == '--version':
        sw_version, fw_version, os_version = get_versions()
        print("Software version: %s" % sw_version)
        if fw_version is None:
            fw_version = "No connection to PiJuice"
        print("Firmware version: %s" % fw_version)
        print("OS version: %s" % os_version)
