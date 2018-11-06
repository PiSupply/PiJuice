#!/usr/bin/env python
from __future__ import division, print_function
__version__ = "1.4"

import ctypes
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


class PiJuiceStatus(object):
    STATUS_CMD = 0x40
    FAULT_EVENT_CMD = 0x44
    CHARGE_LEVEL_CMD = 0x41
    BUTTON_EVENT_CMD = 0x45
    BATTERY_TEMPERATURE_CMD = 0x47
    BATTERY_VOLTAGE_CMD = 0x49
    BATTERY_CURRENT_CMD = 0x4b
    IO_VOLTAGE_CMD = 0x4d
    IO_CURRENT_CMD = 0x4f
    LED_STATE_CMD = 0x66
    LED_BLINK_CMD = 0x68
    IO_PIN_ACCESS_CMD = 0x75

    def __init__(self, interface):
        self.interface = interface

    def __enter__(self):
        # Just return this object so it can be used in a with statement
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        return False  # Don't suppress exceptions.

    @pijuice_error_return
    def GetStatus(self):
        result = self.interface.read_data(self.STATUS_CMD, 1)
        d = result[0]
        batStatusEnum = ['NORMAL', 'CHARGING_FROM_IN', 'CHARGING_FROM_5V_IO', 'NOT_PRESENT']
        powerInStatusEnum = ['NOT_PRESENT', 'BAD', 'WEAK', 'PRESENT']
        status = {
            'isFault': bool(d & 0x01),
            'isButton': bool(d & 0x02),
            'battery': batStatusEnum[(d >> 2) & 0x03],
            'powerInput': powerInStatusEnum[(d >> 4) & 0x03],
            'powerInput5vIo': powerInStatusEnum[(d >> 6) & 0x03]
        }
        return {'data': status, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetChargeLevel(self):
        result = self.interface.read_data(self.CHARGE_LEVEL_CMD, 1)
        return {'data': result[0], 'error': 'NO_ERROR'}

    faultEvents = ['button_power_off', 'forced_power_off', 'forced_sys_power_off', 'watchdog_reset']
    faults = ['battery_profile_invalid', 'charging_temperature_fault']

    @pijuice_error_return
    def GetFaultStatus(self):
        result = self.interface.read_data(self.FAULT_EVENT_CMD, 1)
        d = result[0]
        fault = {}

        if d & 0x01:
            fault['button_power_off'] = True
        if d & 0x02:
            fault['forced_power_off'] = True  # bool(d & 0x02)
        if d & 0x04:
            fault['forced_sys_power_off'] = True
        if d & 0x08:
            fault['watchdog_reset'] = True
        if d & 0x20:
            fault['battery_profile_invalid'] = True
        batChargingTempEnum = ['NORMAL', 'SUSPEND', 'COOL', 'WARM']
        if (d >> 6) & 0x03:
            fault['charging_temperature_fault'] = batChargingTempEnum[(d >> 6) & 0x03]
        return {'data': fault, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def ResetFaultFlags(self, flags):
        d = 0xFF
        for ev in flags:
            try:
                d = d & ~(0x01 << self.faultEvents.index(ev))
            except:
                pass  # TODO: decide what should be done in case of exception
        self.interface.write_data(self.FAULT_EVENT_CMD, [d])  # clear fault events

    buttonEvents = ['NO_EVENT', 'PRESS', 'RELEASE', 'SINGLE_PRESS', 'DOUBLE_PRESS', 'LONG_PRESS1', 'LONG_PRESS2']

    @pijuice_error_return
    def GetButtonEvents(self):
        d = self.interface.read_data(self.BUTTON_EVENT_CMD, 2)
        event = {}

        try:
            event['SW1'] = self.buttonEvents[d[0] & 0x0F]
        except:
            event['SW1'] = 'UNKNOWN'
        try:
            event['SW2'] = self.buttonEvents[(d[0] >> 4) & 0x0F]
        except:
            event['SW2'] = 'UNKNOWN'
        try:
            event['SW3'] = self.buttonEvents[d[1] & 0x0F]
        except:
            event['SW3'] = 'UNKNOWN'
        return {'data': event, 'error': 'NO_ERROR'}

    buttons = ['SW' + str(i+1) for i in range(0, 3)]

    @pijuice_error_return
    def AcceptButtonEvent(self, button):
        try:
            b = self.buttons.index(button)
        except ValueError:
            raise BadArgumentError

        d = [0xF0, 0xFF] if b == 0 else [0x0F, 0xFF] if b == 1 else [0xFF, 0xF0]
        self.interface.write_data(self.BUTTON_EVENT_CMD, d)  # clear button events

    @pijuice_error_return
    def GetBatteryTemperature(self):
        d = self.interface.read_data(self.BATTERY_TEMPERATURE_CMD, 2)
        temp = d[1]
        temp = temp << 8
        temp = temp | d[0]
        return {'data': temp, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetBatteryVoltage(self):
        d = self.interface.read_data(self.BATTERY_VOLTAGE_CMD, 2)
        return {'data': (d[1] << 8) | d[0], 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetBatteryCurrent(self):
        d = self.interface.read_data(self.BATTERY_CURRENT_CMD, 2)
        i = (d[1] << 8) | d[0]
        if i & (1 << 15):
            i = i - (1 << 16)
        return {'data': i, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIoVoltage(self):
        d = self.interface.read_data(self.IO_VOLTAGE_CMD, 2)
        return {'data': (d[1] << 8) | d[0], 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIoCurrent(self):
        d = self.interface.read_data(self.IO_CURRENT_CMD, 2)
        i = (d[1] << 8) | d[0]
        if i & (1 << 15):
            i = i - (1 << 16)
        return {'data': i, 'error': 'NO_ERROR'}

    leds = ['D1', 'D2']

    @pijuice_error_return
    def SetLedState(self, led, rgb):
        try:
            i = self.leds.index(led)
        except ValueError:
            raise BadArgumentError

        self.interface.write_data(self.LED_STATE_CMD + i, rgb)
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetLedState(self, led):
        try:
            i = self.leds.index(led)
        except ValueError:
            raise BadArgumentError

        return self.interface.read_data(self.LED_STATE_CMD + i, 3)

    @pijuice_error_return
    def SetLedBlink(self, led, count, rgb1, period1, rgb2, period2):
        try:
            i = self.leds.index(led)
            d = [count & 0xFF] + rgb1*1 + [(period1//10) & 0xFF] + rgb2*1 + [(period2//10) & 0xFF]
        except (ValueError, TypeError):
            raise BadArgumentError

        self.interface.write_data(self.LED_BLINK_CMD + i, d)
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetLedBlink(self, led):
        try:
            i = self.leds.index(led)
        except ValueError:
            raise BadArgumentError

        d = self.interface.read_data(self.LED_BLINK_CMD + i, 9)
        return {
            'data': {
                'count': d[0],
                'rgb1': d[1:4],
                'period1': d[4] * 10,
                'rgb2': d[5:8],
                'period2': d[8] * 10
                },
            'error': 'NO_ERROR'
        }

    @pijuice_error_return
    def GetIoDigitalInput(self, pin):
        if pin not in (1, 2):
            raise BadArgumentError

        d = self.interface.read_data(self.IO_PIN_ACCESS_CMD + (pin - 1) * 5, 2)
        b = 1 if d[0] == 0x01 else 0
        return {'data': b, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetIoDigitalOutput(self, pin, value):
        if pin not in (1, 2):
            raise BadArgumentError

        d = [0x00, 0x00]
        d[1] = 0x00 if value == 0 else 0x01
        self.interface.write_data(self.IO_PIN_ACCESS_CMD + (pin - 1) * 5, d)
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIoDigitalOutput(self, pin):
        if pin not in (1, 2):
            raise BadArgumentError

        d = self.interface.read_data(self.IO_PIN_ACCESS_CMD + (pin - 1) * 5, 2)
        b = 1 if d[1] == 0x01 else 0
        return {'data': b, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIoAnalogInput(self, pin):
        if pin not in (1, 2):
            raise BadArgumentError

        d = self.interface.read_data(self.IO_PIN_ACCESS_CMD + (pin - 1) * 5, 2)
        return {'data': (d[1] << 8) | d[0], 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetIoPWM(self, pin, dutyCycle):
        if pin not in (1, 2):
            raise BadArgumentError

        try:
            dc = float(dutyCycle)

            if dc < 0 or dc > 100:
                raise InvalidDutyCycleError  # Why not BadArgumentError?
        except (ValueError, TypeError):
            raise BadArgumentError

        dci = int(round(dc * 65534 // 100))
        d = [dci & 0xFF, (dci >> 8) & 0xFF]
        self.interface.write_data(self.IO_PIN_ACCESS_CMD + (pin - 1) * 5, d)
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetIoPWM(self, pin):
        if pin not in (1, 2):
            raise BadArgumentError

        d = self.interface.read_data(self.IO_PIN_ACCESS_CMD + (pin - 1) * 5, 2)
        dci = d[0] | (d[1] << 8)
        dc = float(dci) * 100 // 65534 if dci < 65535 else 100
        return {'data': dc, 'error': 'NO_ERROR'}


class PiJuiceRtcAlarm(object):

    RTC_ALARM_CMD = 0xB9
    RTC_TIME_CMD = 0xB0
    RTC_CTRL_STATUS_CMD = 0xC2

    def __init__(self, interface):
        self.interface = interface

    def __enter__(self):
        # Just return this object so it can be used in a with statement
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        return False  # Don't suppress exceptions.

    @pijuice_error_return
    def GetControlStatus(self):
        d = self.interface.read_data(self.RTC_CTRL_STATUS_CMD, 2)
        r = {}
        if (d[0] & 0x01) and (d[0] & 0x04):
            r['alarm_wakeup_enabled'] = True
        else:
            r['alarm_wakeup_enabled'] = False
        if d[1] & 0x01:
            r['alarm_flag'] = True
        else:
            r['alarm_flag'] = False
        return {'data': r, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def ClearAlarmFlag(self):
        d = self.interface.read_data(self.RTC_CTRL_STATUS_CMD, 2)

        if d[1] & 0x01:
            d[1] = d[1] & 0xFE
            self.interface.write_data(self.RTC_CTRL_STATUS_CMD, d, True)

        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetWakeupEnabled(self, status):
        d = self.interface.read_data(self.RTC_CTRL_STATUS_CMD, 2)
        is_enabled = (d[0] & 0x01) and (d[0] & 0x04)

        if (is_enabled and status) or (not is_enabled and not status):
            return {'error': 'NO_ERROR'}  # do nothing
        elif is_enabled and not status:
            d[0] = d[0] & 0xFE  # disable
        elif not is_enabled and status:
            d[0] = d[0] | 0x01 | 0x04  # enable

        self.interface.write_data(self.RTC_CTRL_STATUS_CMD, d, True)
        return {'error': 'NO_ERROR'}

    @pijuice_error_return
    def GetTime(self):
        d = self.interface.read_data(self.RTC_TIME_CMD, 9)
        dt = {
            'second': ((d[0] >> 4) & 0x07) * 10 + (d[0] & 0x0F),
            'minute': ((d[1] >> 4) & 0x07) * 10 + (d[1] & 0x0F)
        }

        if d[2] & 0x40:
            # hourFormat = '12'
            ampm = 'PM' if (d[2] & 0x20) else 'AM'
            dt['hour'] = str(((d[2] >> 4) & 0x01) * 10 + (d[2] & 0x0F)) + ' ' + ampm
        else:
            # hourFormat = '24'
            dt['hour'] = ((d[2] >> 4) & 0x03) * 10 + (d[2] & 0x0F)

        dt['weekday'] = d[3] & 0x07
        dt['day'] = ((d[4] >> 4) & 0x03) * 10 + (d[4] & 0x0F)
        dt['month'] = ((d[5] >> 4) & 0x01) * 10 + (d[5] & 0x0F)
        dt['year'] = ((d[6] >> 4) & 0x0F) * 10 + (d[6] & 0x0F) + 2000
        dt['subsecond'] = d[7] // 256

        if (d[8] & 0x03) == 2:
            dt['daylightsaving'] = 'SUB1H'
        elif (d[8] & 0x03) == 1:
            dt['daylightsaving'] = 'ADD1H'
        else:
            dt['daylightsaving'] = 'NONE'

        if d[8] & 0x04:
            dt['storeoperation'] = True
        else:
            dt['storeoperation'] = False

        return {'data': dt, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetTime(self, dateTime):
        d = [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
        if not dateTime:
            dt = {}
        else:
            dt = dateTime

        if 'second' in dt:
            try:
                s = int(dt['second'])
            except:
                raise InvalidSecondError
            if s < 0 or s > 60:
                raise InvalidSecondError
            d[0] = ((s // 10) & 0x0F) << 4
            d[0] = d[0] | ((s % 10) & 0x0F)

        if 'minute' in dt:
            try:
                m = int(dt['minute'])
            except:
                raise InvalidMinuteError
            if m < 0 or m > 60:
                raise InvalidMinuteError
            d[1] = ((m // 10) & 0x0F) << 4
            d[1] = d[1] | ((m % 10) & 0x0F)

        if 'hour' in dt:
            try:
                h = dt['hour']
                if isinstance(h, str):
                    if (h.find('AM') > -1) or (h.find('PM') > -1):
                        if h.find('PM') > -1:
                            hi = int(h.split('PM')[0])
                            if hi < 1 or hi > 12:
                                raise InvalidHourError
                            d[2] = (((hi // 10) & 0x03) << 4)
                            d[2] = d[2] | ((hi % 10) & 0x0F)
                            d[2] = d[2] | 0x20 | 0x40
                        else:
                            hi = int(h.split('AM')[0])
                            if hi < 1 or hi > 12:
                                raise InvalidHourError
                            d[2] = (((hi // 10) & 0x03) << 4)
                            d[2] = d[2] | ((hi % 10) & 0x0F)
                            d[2] = d[2] | 0x40
                    else:
                        h = int(h)
                        if h < 0 or h > 23:
                            raise InvalidHourError
                        d[2] = (((h // 10) & 0x03) << 4)
                        d[2] = d[2] | ((h % 10) & 0x0F)

                elif isinstance(h, int):
                    # assume 24 hour format
                    if h < 0 or h > 23:
                        raise InvalidHourError
                    d[2] = (((int(h) // 10) & 0x03) << 4)
                    d[2] = d[2] | ((int(h) % 10) & 0x0F)
            except:
                raise InvalidHourError

        if 'weekday' in dt:
            try:
                day = int(dt['weekday'])
            except:
                raise InvalidWeekdayError
            if day < 1 or day > 7:
                raise InvalidWeekdayError
            d[3] = day & 0x07

        if 'day' in dt:
            try:
                da = int(dt['day'])
            except:
                raise InvalidDayError
            if da < 1 or da > 31:
                raise InvalidDayError
            d[4] = ((da // 10) & 0x03) << 4
            d[4] = d[4] | ((da % 10) & 0x0F)

        if 'month' in dt:
            try:
                m = int(dt['month'])
            except:
                raise InvalidMonthError
            if m < 1 or m > 12:
                raise InvalidMonthError
            d[5] = ((m // 10) & 0x01) << 4
            d[5] = d[5] | ((m % 10) & 0x0F)

        if 'year' in dt:
            try:
                y = int(dt['year']) - 2000
            except:
                raise InvalidYearError
            if y < 0 or y > 99:
                raise InvalidYearError
            d[6] = ((y // 10) & 0x0F) << 4
            d[6] = d[6] | ((y % 10) & 0x0F)

        if 'subsecond' in dt:
            try:
                s = int(dt['subsecond']) * 256
            except:
                raise InvalidSubsecondError
            if s < 0 or s > 255:
                raise InvalidSubsecondError
            d[7] = s

        if 'daylightsaving' in dt:
            if dt['daylightsaving'] == 'SUB1H':
                d[8] |= 2
            elif dt['daylightsaving'] == 'ADD1H':
                d[8] |= 1

        if dt.get('storeoperation'):
            d[8] |= 0x04

        self.interface.write_data(self.RTC_TIME_CMD, d)

        # verify
        time.sleep(0.2)
        ret = self.interface.read_data(self.RTC_TIME_CMD, 9)

        if d == ret:
            return {'error': 'NO_ERROR'}
        else:
            if abs(ret[0] - d[0]) < 2:
                ret[0] = d[0]
                ret[7] = d[7]
                if d == ret:
                    return {'error': 'NO_ERROR'}
                else:
                    raise WriteMismatchError
            else:
                raise WriteMismatchError

    @pijuice_error_return
    def GetAlarm(self):
        d = self.interface.read_data(self.RTC_ALARM_CMD, 9)
        alarm = {}
        if (d[0] & 0x80) == 0x00:
            alarm['second'] = ((d[0] >> 4) & 0x07) * 10 + (d[0] & 0x0F)

        if (d[1] & 0x80) == 0x00:
            alarm['minute'] = ((d[1] >> 4) & 0x07) * 10 + (d[1] & 0x0F)
        else:
            alarm['minute_period'] = d[7]

        if (d[2] & 0x80) == 0x00:
            if d[2] & 0x40:
                # hourFormat = '12'
                ampm = 'PM' if (d[2] & 0x20) else 'AM'
                alarm['hour'] = str(((d[2] >> 4) & 0x01) * 10 + (d[2] & 0x0F)) + ' ' + ampm
            else:
                # hourFormat = '24'
                alarm['hour'] = ((d[2] >> 4) & 0x03) * 10 + (d[2] & 0x0F)
        else:
            if d[4] == 0xFF and d[5] == 0xFF and d[6] == 0xFF:
                alarm['hour'] = 'EVERY_HOUR'
            else:
                h = ''
                n = 0
                for i in range(0, 3):
                    for j in range(0, 8):
                        if d[i+4] & ((0x01 << j) & 0xFF):
                            if d[2] & 0x40:
                                if (i*8+j) == 0:
                                    h12 = '12AM'
                                elif (i*8+j) == 12:
                                    h12 = '12PM'
                                else:
                                    h12 = (str(i*8+j+1)+'AM') if ((i*8+j) < 12) else (str(i*8+j-11)+'PM')
                                h = (h + ';') if n > 0 else h
                                h = h + h12
                            else:
                                h = (h + ';') if n > 0 else h
                                h = h + str(i*8+j)
                            n = n + 1
                alarm['hour'] = h

        if d[3] & 0x40:
            if (d[3] & 0x80) == 0x00:
                alarm['weekday'] = d[3] & 0x07
            else:
                if d[8] == 0xFF:
                    alarm['weekday'] = 'EVERY_DAY'
                else:
                    day = ''
                    n = 0
                    for j in range(1, 8):
                        if d[8] & ((0x01 << j) & 0xFF):
                            day = (day + ';') if n > 0 else day
                            day = day + str(j)
                            n = n + 1
                    alarm['weekday'] = day
        else:
            if (d[3] & 0x80) == 0x00:
                alarm['day'] = ((d[3] >> 4) & 0x03) * 10 + (d[3] & 0x0F)
            else:
                alarm['day'] = 'EVERY_DAY'

        return {'data': alarm, 'error': 'NO_ERROR'}

    @pijuice_error_return
    def SetAlarm(self, alarm):
        d = [0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF]
        if not alarm:
            # disable alarm
            self.interface.write_data(self.RTC_ALARM_CMD, d, True, 0.2)
            return {'error': 'NO_ERROR'}

        if 'second' in alarm:
            try:
                s = int(alarm['second'])
            except:
                raise InvalidSecondError
            if s < 0 or s > 60:
                raise InvalidSecondError
            d[0] = ((s // 10) & 0x0F) << 4
            d[0] = d[0] | ((s % 10) & 0x0F)

        if 'minute' in alarm:
            try:
                m = int(alarm['minute'])
            except:
                raise InvalidMinuteError
            if m < 0 or m > 60:
                raise InvalidMinuteError
            d[1] = ((m // 10) & 0x0F) << 4
            d[1] = d[1] | ((m % 10) & 0x0F)
        else:
            d[1] = d[1] | 0x80  # every minute

        if 'minute_period' in alarm:
            d[1] = d[1] | 0x80
            try:
                s = int(alarm['minute_period'])
            except:
                raise InvalidMinutePeriodError
            if s < 1 or s > 60:
                raise InvalidMinutePeriodError
            d[7] = s

        d[4] = 0xFF
        d[5] = 0xFF
        d[6] = 0xFF
        if 'hour' in alarm:
            try:
                h = alarm['hour']
                if h == 'EVERY_HOUR':
                    d[2] = 0x80

                elif isinstance(h, str) and h.find(';') < 0:
                    if (h.find('AM') > -1) or (h.find('PM') > -1):
                        if h.find('PM') > -1:
                            hi = int(h.split('PM')[0])
                            d[2] = (((hi // 10) & 0x03) << 4)
                            d[2] = d[2] | ((hi % 10) & 0x0F)
                            d[2] = d[2] | 0x20 | 0x40
                        else:
                            hi = int(h.split('AM')[0])
                            d[2] = (((hi // 10) & 0x03) << 4)
                            d[2] = d[2] | ((hi % 10) & 0x0F)
                            d[2] = d[2] | 0x40
                    else:
                        d[2] = (((int(h) // 10) & 0x03) << 4)
                        d[2] = d[2] | ((int(h) % 10) & 0x0F)

                elif isinstance(h, int):
                    # assume 24 hour format
                    d[2] = (((int(h) // 10) & 0x03) << 4)
                    d[2] = d[2] | ((int(h) % 10) & 0x0F)

                elif isinstance(h, str) and h.find(';') >= 0:
                    hs = 0x00000000
                    # hFormat = ''
                    hl = h.split(';')
                    # remove ending empty string if there is ; at the end of list
                    hl = hl[0:-1] if (not bool(hl[-1].strip())) else hl
                    for i in hl:
                        if (i.find('AM') > -1) or (i.find('PM') > -1):
                            if i.find('AM') > -1:
                                ham = int(i.split('AM')[0])
                                if ham < 12:
                                    hs = hs | (0x00000001 << ham)
                                else:
                                    hs = hs | 0x00000001
                            else:
                                hpm = int(i.split('PM')[0])
                                if hpm < 12:
                                    hs = hs | (0x00000001 << (hpm+12))
                                else:
                                    hs = hs | (0x00000001 << 12)
                        else:
                            hs = hs | (0x00000001 << int(i))
                    d[2] = 0x80
                    d[4] = hs & 0x000000FF
                    hs = hs >> 8
                    d[5] = hs & 0x000000FF
                    hs = hs >> 8
                    d[6] = hs & 0x000000FF
            except:
                raise InvalidHourError
        else:
            d[2] = 0x80  # every hour

        d[8] = 0xFF
        if 'weekday' in alarm:
            try:
                day = alarm['weekday']
                if day == 'EVERY_DAY':
                    d[3] = 0x80 | 0x40

                elif isinstance(day, str) and day.find(';') < 0:
                    dw = int(day)
                    d[3] = d[3] | (dw & 0x0F) | 0x40

                elif isinstance(day, int):
                    dw = int(day)
                    d[3] = d[3] | (dw & 0x0F) | 0x40

                elif isinstance(day, str) and day.find(';') >= 0:
                    d[3] = 0x40 | 0x80
                    ds = 0x00
                    dl = day.split(';')
                    dl = dl[0:-1] if (not bool(dl[-1].strip())) else dl
                    for i in dl:
                        ds = ds | (0x01 << int(i))
                    d[8] = ds
            except:
                raise InvalidWeekdayError
        elif 'day' in alarm:
            try:
                day = alarm['day']
                if day == 'EVERY_DAY':
                    d[3] = 0x80

                else:
                    dm = int(day)
                    d[3] = (((dm // 10) & 0x03) << 4)
                    d[3] = d[3] | ((dm % 10) & 0x0F)
            except:
                raise InvalidDayOfMonthError
        else:
            d[3] = 0x80  # every day

        self.interface.write_data(self.RTC_ALARM_CMD, d)
        # verify
        time.sleep(0.2)
        ret = self.interface.read_data(self.RTC_ALARM_CMD, 9)
        if d == ret:
            return {'error': 'NO_ERROR'}
        else:
            h1 = d[2]
            h2 = ret[2]
            if h1 & 0x40:  # convert to 24 hour format
                h1Bin = ((h1 >> 4) & 0x01) * 10 + (h1 & 0x0F)
                h1Bin = h1Bin if h1Bin < 12 else 0
                h1Bin = h1Bin + (12 if h1 & 0x20 else 0)
            else:
                h1Bin = ((h1 >> 4) & 0x03) * 10 + (h1 & 0x0F)
            if h2 & 0x40:  # convert to 24 hour format
                h2Bin = ((h2 >> 4) & 0x01) * 10 + (h2 & 0x0F)
                h2Bin = h2Bin if h2Bin < 12 else 0
                h2Bin = h2Bin + (12 if h2 & 0x20 else 0)
            else:
                h2Bin = ((h2 >> 4) & 0x03) * 10 + (h2 & 0x0F)
            d[2] = h1Bin | (d[2] & 0x80)
            ret[2] = h2Bin | (ret[2] & 0x80)
            if d == ret:
                return {'error': 'NO_ERROR'}
            else:
                raise WriteMismatchError


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
