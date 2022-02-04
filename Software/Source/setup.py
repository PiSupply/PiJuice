from distutils.core import setup
#from distutils.command.install_data import install_data
#from distutils.dep_util import newer
#from distutils.log import info
import glob
import os
#import sys

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


version = os.environ.get('PIJUICE_VERSION')

if int(os.environ.get('PIJUICE_BUILD_BASE', 0)) > 0:
    name = "pijuice-base"
    data_files = [
        ('share/pijuice/data/firmware', glob.glob('data/firmware/*')),
        ('/etc/udev/rules.d', ['data/99-i2c.rules']),
        ('/etc/sudoers.d', ['data/020_pijuice-nopasswd']),
        ('bin', ['bin/pijuiceboot32']),
        ('bin', ['bin/pijuiceboot64']),
        ('bin', ['bin/pijuice_cli32']),
        ('bin', ['bin/pijuice_cli64']),
    ]
    scripts = ['src/pijuice_sys.py', 'src/pijuice_cli.py', 'src/pijuice_log.py']
    description = "Software package for PiJuice"
    py_modules=['pijuice']
else:
    name = "pijuice-gui"
    py_modules = None
    data_files= [
        ('share/applications', ['data/pijuice-gui.desktop']),
        ('/etc/xdg/autostart', ['data/pijuice-tray.desktop']),
        ('share/pijuice/data/images', glob.glob('data/images/*')), 
        ('/etc/X11/Xsession.d', ['data/36x11-pijuice_xhost']),
        ('bin', ['bin/pijuice_gui32']),
        ('bin', ['bin/pijuice_gui64']),
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
