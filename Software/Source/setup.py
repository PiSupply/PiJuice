#!/usr/bin/env python

##
#   Project: bluewho
#            Information and notification of new discovered bluetooth devices.
#    Author: Fabio Castelli <muflone@vbsimple.net>
# Copyright: 2009 Fabio Castelli
#   License: GPL-2+
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the Free
#  Software Foundation; either version 2 of the License, or (at your option)
#  any later version.
# 
#  This program is distributed in the hope that it will be useful, but WITHOUT
#  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
#  more details.
# 
# On Debian GNU/Linux systems, the full text of the GNU General Public License
# can be found in the file /usr/share/common-licenses/GPL-2.
##

from distutils.core import setup
from distutils.command.install_data import install_data
from distutils.dep_util import newer
from distutils.log import info
#from setuptools import setup
import glob
import os
import sys
import pijuice

class InstallData(install_data):
    def run (self):
        self.data_files.extend (self._compile_po_files())
        install_data.run (self)

    def _compile_po_files (self):
        data_files = []

        # Don't install language files on win32
        if sys.platform == 'win32':
            return data_files

        PO_DIR = 'locales'
        for po in glob.glob (os.path.join(PO_DIR,'*.po')):
            lang = os.path.basename(po[:-3])
            mo = os.path.join('build', 'mo', lang, 'bluewho.mo')

            directory = os.path.dirname(mo)
            if not os.path.exists(directory):
                info('creating %s' % directory)
                os.makedirs(directory)

            if newer(po, mo):
                # True if mo doesn't exist
                cmd = 'msgfmt -o %s %s' % (mo, po)
                info('compiling %s -> %s' % (po, mo))
                if os.system(cmd) != 0:
                    raise SystemExit('Error while running msgfmt')

                dest = os.path.dirname(os.path.join('share', 'locale', lang, 'LC_MESSAGES', 'bluewho.mo'))
                data_files.append((dest, [mo]))

        return data_files


def set_desktop_entry_versions(version):
    entries = ("data/pijuice-gui.desktop", "data/pijuice-tray.desktop")
    for entry in entries:
        with open(entry, "r") as f:
            lines = f.readlines()
        for i in range(len(lines)):
            if lines[i].startswith("Version="):
                break
        lines[i] = "Version=" + version + "\n"
        with open(entry, "w") as f:
            f.writelines(lines)


if os.environ.get('PIJUICE_BUILD_LIGHT'):
    data_files = [('share/pijuice/data/firmware', glob.glob('data/firmware/*'))]
    scripts = ['src/pijuice_sys.py']
    description = "Software package for PiJuice (Light version)"
else:
    data_files= [
        ('share/applications', ['data/pijuice-gui.desktop']),
        ('/etc/xdg/autostart', ['data/pijuice-tray.desktop']),#(os.environ['XDG_CONFIG_DIRS'] + '/autostart', ['data/pijuice-tray.desktop']),
        ('share/pijuice/data/images', glob.glob('data/images/*')), 
        ('share/pijuice/data/firmware', glob.glob('data/firmware/*')),
        #('/var/lib/pijuice', ['data/pijuice_config.JSON']),
        #('/etc/systemd/user', ['data/pijuice.service']),
        #('share/pijuice/src', glob.glob('src/*.py')),
    ]
    scripts = ['src/pijuice_tray.py', 'src/pijuice_gui.py', 'src/pijuice_sys.py']
    description = "Software package for PiJuice"

try:
    set_desktop_entry_versions(pijuice.__version__)
except:
    pass

setup(
	name="pijuice",
	version=pijuice.__version__,
	author="Milan Neskovic",
	author_email="milan@pi-supply.com",
	description=description,
	url="https://github.com/PiSupply/PiJuice/",
	license='GPL v2',
	py_modules=['pijuice'],
	data_files=data_files,
	scripts=scripts,
	#entry_points = {
		#'gui_scripts' : ['pijuice_gui = pijuice_gui:main'],
		#'console_scripts' : ['pijuicetest = pijuicetest']
	#},
	#cmdclass={'install_data': InstallData}
    )
