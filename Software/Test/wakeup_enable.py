#!/usr/bin/env python
#
# This script is started at reboot by cron
# Since the start is very early in the boot sequence we wait for the i2c-1 device

import pijuice, time, os

while not os.path.exists('/dev/i2c-1'):
    time.sleep(0.1)

pj = pijuice.PiJuice(1, 0x14)

pj.rtcAlarm.SetWakeupEnabled(True)

