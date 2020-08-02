#!/bin/bash
gcc -DFULL_PATH=\"/usr/bin/pijuice_gui.py\" -Wall -o pijuice_gui setuid-prog.c
gcc -DFULL_PATH=\"/usr/bin/pijuice_cli.py\" -Wall -o pijuice_cli setuid-prog.c
# Also build pijuiceboot so all programs are build for the current architecture (32 or 64 bit)
gcc -Wall -o pijuiceboot ../../../Firmware/pijuiceboot.c
