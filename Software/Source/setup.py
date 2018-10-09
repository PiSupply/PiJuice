#!/usr/bin/env python3

from distutils.core import setup
import fileinput
import glob
import os


def set_desktop_entry_versions(version):
    entries = ("data/pijuice-gui.desktop", "data/pijuice-tray.desktop")

    for line in fileinput.input(files=entries, inplace=True):
        if line.startswith("Version="):
            print("Version={}".format(version))
        else:
            print(line, end="")


version = os.environ.get('PIJUICE_VERSION')

if int(os.environ.get('PIJUICE_BUILD_BASE', 0)) > 0:
    name = "pijuice-base"
    data_files = [
        ('share/pijuice/data/firmware', glob.glob('data/firmware/*')),
        ('/etc/sudoers.d', ['data/020_pijuice-nopasswd']),
        ('bin', ['bin/pijuiceboot']),
        ('bin', ['bin/pijuice_cli']),
    ]
    scripts = ['src/pijuice_sys.py', 'src/pijuice_cli.py']
    description = "Software package for PiJuice"
    py_modules = ['pijuice']
else:
    name = "pijuice-gui"
    py_modules = None
    data_files = [
        ('share/applications', ['data/pijuice-gui.desktop']),
        ('/etc/xdg/autostart', ['data/pijuice-tray.desktop']),
        ('share/pijuice/data/images', glob.glob('data/images/*')), 
        ('/etc/X11/Xsession.d', ['data/36x11-pijuice_xhost']),
        ('bin', ['bin/pijuice_gui']),
    ]
    scripts = ['src/pijuice_tray.py', 'src/pijuice_gui.py']
    description = "GUI package for PiJuice"

try:
    set_desktop_entry_versions(version)
except:
    pass

setup(
    name=name,
    version=version,
    author="Ton van Overbeek",
    author_email="tvoverbeek@gmail.com",
    description=description,
    url="https://github.com/PiSupply/PiJuice/",
    license='GPL v2',
    py_modules=py_modules,
    data_files=data_files,
    scripts=scripts,
    )
