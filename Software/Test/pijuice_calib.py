from smbus import SMBus
import time
import datetime
import calendar
import sys, getopt

i2cbus=SMBus(1)

def _GetChecksum(data):
	fcs=0xFF
	for x in data[:]:
		fcs=fcs^x
	return fcs

data = [0x55, 0x26, 0xa0, 0x2b]
fcs = _GetChecksum(data)
d = data[:]
d.append(fcs)

i2cbus.write_i2c_block_data(0x14, 248, d) 
