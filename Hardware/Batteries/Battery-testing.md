# Battery Testing

This guide is for **advanced** users only who fully understand the battery testing procedure and the risks involved if done incorrectly. This test is designed to evaluate the battery capacity and internal resistance using the script `pijuice-soc-test.py`.

The test must be performed by attaching a test load resistor to VSYS output, that is ON/OFF controlled by the system switch on the PiJuice using the script `pijuice-soc-test.py`. Max current through the VSYS switch is 2.1A, so to the test load resistor should be chosen drawing current less than this limiting factor.

Battery capacity is usually specified at discharge load of 0.2, so for 12000mA battery is 2.4A, so resistor should be chosen to draw current close to this value. Around 2 Ohm 10W will be a good value giving mac current of 4.2V/2Ohm of 2.1A when the battery is near full and is no more than the limit switch.

## Full discharge test procedure

- Connect power supply to the Raspberry Pi
- Use PIJuice setup GUI to enter custom battery profile, for Li-Po battery set cut of voltage to somewhat below nominal for Li-Po 2850 is a good value
- Charge battery to full level
- Edit `pijuice-soc-test.py` script and enter c0 - nominal battery capacity value and Rload - resistor value attached to VSYS
- Run test with: `python pijuice-soc-test.py`
- Wait until test is complete, it may take hours
- Test results are logged onscreen and in log file: battery_soc_test.txt
NOTE: Before running the test rename of delete any previous test logs, otherwise it will append to the existing log

Battery parameters derived from full discharge test can be written to pijuice with `pijuice_calib.py` script to calibrate state of charge evaluation.
