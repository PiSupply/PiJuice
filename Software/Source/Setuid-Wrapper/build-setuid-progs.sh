#!/bin/bash
gcc -DFULL_PATH=\"/usr/bin/pijuice_gui.py\" -Wall -o pijuice_gui setuid-prog.c
gcc -DFULL_PATH=\"/usr/bin/pijuice_cli.py\" -Wall -o pijuice_cli setuid-prog.c
