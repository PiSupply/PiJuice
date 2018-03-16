#! /usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function, division

import os
import os.path
import sys
from time import sleep
from glob import glob
from multiprocessing import Process
from signal import signal, SIGUSR1, SIGUSR2

py3 = sys.version_info > (3, 0)

if py3 == False:
	# Python 2
	import gtk
	import gobject
else:
	# Python 3
	import gi
	gi.require_version('Gtk', '3.0')
	from gi.repository import Gtk as gtk
	from gi.repository import GObject as gobject

from pijuice import PiJuice, get_versions
from pijuice_gui import start_app

REFRESH_INTERVAL = 5000
CHECK_SIGNAL_INTERVAL = 200
ICON_DIR = '/usr/share/pijuice/data/images'
TRAY_PID_FILE = '/tmp/pijuice_tray.pid'


class PiJuiceStatusTray(object):

	def __init__(self):
		self.tray = gtk.StatusIcon()
		self.tray.connect('activate', self.refresh)

		# Create menu
		self.menu = gtk.Menu()

		i = gtk.MenuItem("Settings")
		self.settings_item = i
		i.show()
		i.connect("activate", self.ConfigurePiJuice)
		self.menu.append(i)

		i = gtk.MenuItem("About...")
		i.show()
		i.connect("activate", self.show_about)
		self.menu.append(i)

		self.tray.connect('popup-menu', self.show_menu)

		self.pijuice = PiJuice(1, 0x14)

		# Initalise and start battery display
		self.refresh(None)
		self.tray.set_visible(True)

		gobject.timeout_add(REFRESH_INTERVAL, self.refresh, self.tray)
		gobject.timeout_add(CHECK_SIGNAL_INTERVAL, self.check_signum)

	def show_menu(self, widget, event_button, event_time):
		if py3 == False:
			# Python 2
			self.menu.popup(None, None,
			gtk.status_icon_position_menu,
			event_button,
			event_time,
			self.tray)
		else:
			# Python 3
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
		if py3 == False:
			# Python 2
			dialog = gtk.MessageDialog(
				None,
				gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
				gtk.MESSAGE_INFO,
				gtk.BUTTONS_OK,
				message
			)
		else:
			# Python 3
			dialog = gtk.MessageDialog(
				None,
				gtk.DialogFlags.MODAL,
				gtk.MessageType.INFO,
				gtk.ButtonsType.OK,
				message
			)
		dialog.set_title("About")
		dialog.run()
		dialog.destroy()
		widget.set_sensitive(True)

	def ConfigurePiJuice(self, widget):
		p = Process(target=start_app)
		p.start()

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
				print(charge)

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
					b_file = ICON_DIR + '/bat-rpi-' + str((b_level/10)*10) + '.png'
				else:
					b_file = ICON_DIR + '/bat-' + str((b_level/10)*10) + '.png'
			else:
				b_file = ICON_DIR + '/connection-error.png'
				if py3 == False:
					# Python 2
					self.tray.set_blinking(b_level < 5)

			self.tray.set_tooltip_text("%d%%" % (b_level))
			if os.path.exists(b_file):
				self.tray.set_from_file(b_file)
			else:
				print(b_file)
				print('icon file not exist')

		except:
			print('refresh error')
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

	# Set up signal handlers.
	# SIGUSR1 to disable the Settings menu item
	# SIGUSR2 to enable the Settings menu item
	signal(SIGUSR1, receive_signal)
	signal(SIGUSR2, receive_signal)

	app = PiJuiceStatusTray()
	gtk.main()
