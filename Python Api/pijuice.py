import smbus
import sys
import getopt
import time

class PiJuice(object):
    COMMAND_NULL    = 0x00
    COMMAND_VERSION = 0x01


    def __init__():
        self._address = 0x09 # PiJuice connection address
        self._bus = smbus.SMBus(0)
        self._version = "V1.3.0"

    @property
    def version(self):
        return self._version

    def get_version(self):
        return int(self.read_byte(PiJuice.COMMAND_VERSION, 0x00))

    def read_byte(command, data):
        self._bus.write_byte_data(self._address, command, data)

    
