Full discharge test procedure
	- Connect power supply to Rpi
	- Use pijuice setup GUI to enter custom battery profile, for Li-po battery set cutoff voltage to somewhat below nominal for li-po 2850 is good value.
	- Charge battery to full level
	- Edis pijuice-soc-test.py script and enter c0 - nominal battery capacity value and Rload - resistor value attached to Vsys pin.
	- Run test with: python pijuice-soc-test.py
	- wait until test completes, it can take hours
	- Test results are logged on screen and in log file: battery_soc_test.txt (before test rename or delete log file from previous test, otherwise log  will be appended)
	
Battery parameters derived from full discharge test can be written to pijuice with pijuice_calib.py script to calibrte state of charge evaluation.