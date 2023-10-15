#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from __future__ import print_function, division

import os
import os.path
import sys
from signal import signal, SIGUSR1, SIGUSR2
import json
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk as gtk
from gi.repository import GObject as gobject
from gi.repository import GLib as glib

from pijuice import PiJuice, get_versions

I2C_ADDRESS_DEFAULT = 0x14
I2C_BUS_DEFAULT = 1
REFRESH_INTERVAL = 5000
CHECK_SIGNAL_INTERVAL = 200
ICON_DIR = '/usr/share/pijuice/data/images'
TRAY_PID_FILE = '/run/pijuice/pijuice_tray.pid'
configPath = '/var/lib/pijuice/pijuice_config.JSON'

class PiJuiceStatusTray(object):

    def __init__(self):
        self.tray = gtk.StatusIcon()
        self.tray.connect('activate', self.refresh)

        # Create menu
        self.menu = gtk.Menu()

        i = gtk.MenuItem(label="Settings")
        self.settings_item = i
        i.show()
        i.connect("activate", self.ConfigurePiJuice)
        self.menu.append(i)

        i = gtk.MenuItem(label="About...")
        i.show()
        i.connect("activate", self.show_about)
        self.menu.append(i)

        self.tray.connect('popup-menu', self.show_menu)

        self.init_pijuice_interface()
        
        # Initalise and start battery display
        self.refresh_err = 0
        self.refresh(None)
        self.tray.set_visible(True)

        glib.timeout_add(REFRESH_INTERVAL, self.refresh, self.tray)
        glib.timeout_add(CHECK_SIGNAL_INTERVAL, self.check_signum)

    def init_pijuice_interface(self):
        try:
            addr = I2C_ADDRESS_DEFAULT
            bus = I2C_BUS_DEFAULT

            configData = {}
            with open(configPath, 'r') as outputConfig:
                config_dict = json.load(outputConfig)
                configData.update(config_dict)
                
            if 'board' in configData and 'general' in configData['board']:
                if 'i2c_addr' in configData['board']['general']:
                    addr = int(configData['board']['general']['i2c_addr'], 16)
                if 'i2c_bus' in configData['board']['general']:
                    bus = configData['board']['general']['i2c_bus']

            self.pijuice = PiJuice(bus, addr)
        except:
            sys.exit(0)
            
    def show_menu(self, widget, event_button, event_time):
        self.menu.popup(None, None,
        self.tray.position_menu,
        self.tray,
        event_button,
        gtk.get_current_event_time())

    def show_about(self, widget):
        widget.set_sensitive(False)
        sw_version, fw_version, os_version = get_versions()
        if fw_version is None:
            fw_version = "No connection to PiJuice"
        message = "\n".join([
            "Software version: %s" % sw_version,
            "Firmware version: %s" % fw_version,
            "OS version: %s" % os_version,
        ])
        dialog = gtk.MessageDialog(
            parent=None,
            modal=True,
            message_type=gtk.MessageType.INFO,
            buttons=gtk.ButtonsType.OK,
            text=message
        )
        dialog.set_title("About")
        dialog.run()
        dialog.destroy()
        widget.set_sensitive(True)

    def ConfigurePiJuice(self, widget):
        os.system("/usr/bin/pijuice_gui &")

    def check_signum(self):
        global sig
        if sig > 0:
            if sig == SIGUSR1:
                self.settings_item.set_sensitive(False)
                sig = -1
            elif sig == SIGUSR2:
                self.settings_item.set_sensitive(True)
                sig = -1
            else:
                sig = -1
        return True

    def refresh(self, widget):
        try:
            charge = self.pijuice.status.GetChargeLevel()

            if charge['error'] == 'NO_ERROR':
                b_level = charge['data']
                print('{}%'.format(b_level))
            else:
                self.init_pijuice_interface()

            b_file = ICON_DIR + '/battery_near_full.png'

            status = self.pijuice.status.GetStatus()
            if status['error'] == 'NO_ERROR':
                self.status = status['data']

                if self.status['battery'] == 'NOT_PRESENT':
                    if self.status['powerInput'] != 'NOT_PRESENT':
                        b_file = ICON_DIR + '/no-bat-in-0.png'
                    else:
                        b_file = ICON_DIR + '/no-bat-rpi-0.png'

                elif self.status['battery'] == 'CHARGING_FROM_IN' or self.status['powerInput'] != 'NOT_PRESENT':
                    b_file = ICON_DIR + '/bat-in-' + str((b_level//10)*10) + '.png'
                elif self.status['battery'] == 'CHARGING_FROM_5V_IO' or self.status['powerInput5vIo'] != 'NOT_PRESENT':
                    b_file = ICON_DIR + '/bat-rpi-' + str((b_level//10)*10) + '.png'
                else:
                    b_file = ICON_DIR + '/bat-' + str((b_level//10)*10) + '.png'
            else:
                b_file = ICON_DIR + '/connection-error.png'

            self.tray.set_tooltip_text("%d%%" % (b_level))
            if os.path.exists(b_file):
                self.tray.set_from_file(b_file)
            else:
                print(b_file)
                print('icon file not exist')

        except:
            print('refresh error')  # happens when no PiJuice present
            self.refresh_err += 1
            if self.refresh_err > 4:
                sys.exit(0)
        return True

def receive_signal(signum, stack):
    global sig
    if signum == SIGUSR1 or signum == SIGUSR2:
        sig = signum
    else:
        sig = -1

if __name__ == '__main__':
    global sig

    sig = -1

    # Make our pid available for pijuice_gui
    pid = os.getpid()
    with open(TRAY_PID_FILE, 'w') as f:
        f.write(str(pid))
    # Allow other user to overwrite this file
    if oct(os.stat(TRAY_PID_FILE).st_mode & 0o777) != '0o666':
        os.chmod(TRAY_PID_FILE, 0o666)

    # Set up signal handlers.
    # SIGUSR1 to disable the Settings menu item
    # SIGUSR2 to enable the Settings menu item
    signal(SIGUSR1, receive_signal)
    signal(SIGUSR2, receive_signal)

    app = PiJuiceStatusTray()
    gtk.main()
