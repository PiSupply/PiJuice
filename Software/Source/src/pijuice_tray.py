#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function

import os
import os.path
import sys
import time
from glob import glob
from multiprocessing import Process

import gobject
import gtk
from pijuice import PiJuice, get_versions
from pijuice_gui import start_app

REFRESH_INTERVAL = 5000
ICON_DIR = '/usr/share/pijuice/data/images'


class PiJuiceStatusTray(object):

	def __init__(self):
		self.tray = gtk.StatusIcon()
		self.tray.connect('activate', self.refresh)

		# Create menu
		menu = gtk.Menu()

		i = gtk.MenuItem("Settings")
		i.show()
		i.connect("activate", self.ConfigurePiJuice)
		menu.append(i)

		i = gtk.MenuItem("About...")
		i.show()
		i.connect("activate", self.show_about)
		menu.append(i)

		self.tray.connect('popup-menu', self.show_menu, menu)

		self.pijuice = PiJuice(1, 0x14)

		# Initalise and start battery display
		self.refresh(None)
		self.tray.set_visible(True)

		gobject.timeout_add(REFRESH_INTERVAL, self.refresh, False)

	def show_menu(self, widget, event_button, event_time, menu):
		menu.popup(None, None,
                    gtk.status_icon_position_menu,
                    event_button,
                    event_time,
                    self.tray
             )

	def show_about(self, widget):
		sw_version, fw_version, os_version = get_versions()
		if fw_version is None:
			fw_version = "No connection to PiJuice"
		message = "\n".join([
			"Software version: %s" % sw_version,
			"Firmware version: %s" % fw_version,
			"OS version: %s" % os_version,
		])
		dialog = gtk.MessageDialog(
			None,
			gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
			gtk.MESSAGE_INFO,
			gtk.BUTTONS_OK,
			message
		)
		dialog.set_title("About")
		dialog.run()
		dialog.destroy()

	def ConfigurePiJuice(self, widget):
		p = Process(target=start_app)
		p.start()

	def refresh(self, widget):
		try:
			charge = self.pijuice.status.GetChargeLevel()

			if charge['error'] == 'NO_ERROR':
				b_level = charge['data']
				print(b_level + '%')
			else:
				print(charge)

			# "." + str(b_level / 10) + ".png"
			b_file = ICON_DIR + '/battery_near_full.png'

			status = self.pijuice.status.GetStatus()
			#print status
			if status['error'] == 'NO_ERROR':
				self.status = status['data']

				if self.status['battery'] == 'NOT_PRESENT':
					if self.status['powerInput'] != 'NOT_PRESENT':
						b_file = ICON_DIR + '/no-bat-in-0.png'
					else:
						b_file = ICON_DIR + '/no-bat-rpi-0.png'

				elif self.status['battery'] == 'CHARGING_FROM_IN' or self.status['powerInput'] != 'NOT_PRESENT':
					b_file = ICON_DIR + '/bat-in-' + str((b_level/10)*10) + '.png'
				elif self.status['battery'] == 'CHARGING_FROM_5V_IO' or self.status['powerInput5vIo'] != 'NOT_PRESENT':
					b_file = ICON_DIR + '/bat-rpi-' + str((b_level/10)*10) + '.png'
				else:
					b_file = ICON_DIR + '/bat-' + str((b_level/10)*10) + '.png'
			else:
				b_file = ICON_DIR + '/connection-error.png'
				self.tray.set_blinking(b_level < 5)

			self.tray.set_tooltip("%d%%" % (b_level))
			if os.path.exists(b_file):
				self.tray.set_from_file(b_file)
			else:
				print('icon file not exist')

		except:
			print('charge proccess error')
		return True


if __name__ == '__main__':
	app = PiJuiceStatusTray()
	gtk.main()
