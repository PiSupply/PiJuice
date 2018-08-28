#!/bin/bash
FULL_PATH=/usr/bin/pijuice_gui.py gcc -Wall -o pijuice_gui setuid-prog.c
FULL_PATH=/usr/bin/pijuice_cli.py gcc -Wall -o pijuice_cli setuid-prog.c
