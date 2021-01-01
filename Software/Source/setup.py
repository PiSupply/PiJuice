#!/usr/bin/env python3
import setuptools
import functools

import glob
import os

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
build_base = int(os.environ.get('PIJUICE_BUILD_BASE', 0)) != 0

setup = functools.partial(
    setuptools.setup,
    version=version,
    author="Ton van Overbeek",
    author_email="tvoverbeek@gmail.com",
    url="https://github.com/PiSupply/PiJuice/",
    license='GPL v2',
)

if build_base:
    setup(
        name="pijuice-base",
        description="Software package for PiJuice",
        install_requires=["smbus", "urwid", "marshmallow"],
        py_modules=['pijuice'],
        packages=setuptools.find_packages(),
        include_package_data=True,
        data_files=[
            ('share/pijuice/data/firmware', glob.glob('data/firmware/*')),
            ('/etc/sudoers.d', ['data/020_pijuice-nopasswd']),
            ('bin', ['bin/pijuiceboot']),
            ('bin', ['bin/pijuice_cli']),
        ],
        scripts = ['src/pijuice_cli.py'],
        package_data={
            "pijuice_sys.scripts": ["*.sh"],
        },
        entry_points={
            'console_scripts': [
                "pijuice_cmd=pijuice_cmd.__main__:main",
                "pijuice_sys=pijuice_sys.__main__:main",
            ]
        },
    )

else:
    try:
        set_desktop_entry_versions(version)
    except:
        pass
    setup(
        name="pijuice-gui",
        description="GUI package for PiJuice",
        install_requires=["smbus", "urwid"],
        data_files=[
            ('share/applications', ['data/pijuice-gui.desktop']),
            ('/etc/xdg/autostart', ['data/pijuice-tray.desktop']),
            ('share/pijuice/data/images', glob.glob('data/images/*')),
            ('/etc/X11/Xsession.d', ['data/36x11-pijuice_xhost']),
            ('bin', ['bin/pijuice_gui']),
        ],
        scripts=['src/pijuice_tray.py', 'src/pijuice_gui.py'],
    )
