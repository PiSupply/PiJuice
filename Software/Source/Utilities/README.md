# Software Utilities

## Command Line Utility

There is a command line utility `pijuice_util.py` which you can find at [Software/Source/Utilities](https://github.com/PiSupply/PiJuice/tree/master/Software/Source/Utilities). The intent of the script is to get stats from the command line as well as to perform other basic functions such as dumping the PiJuice settings to a file, then loading those same settings into a new PiJuice. The functionality is as folllows:

* `--dump > dumpfile.js` to dump the settings to a file.
* `--load < dumpfile.js` to load the settings from a file.
* `--enable-wakeup` to enable the wakeup flag.
* `--get-time` to print the RTC time.
* `--get-alarm` to print the RTC alarm.
* `--get-status` to print the pijiuce status.
* `--get-config` to print the pijiuce config.
* `--get-battery` to print the pijiuce battery status.
* `--get-input` to print the pijiuce input status.

So, for example to use this you would navigate to the files location and then run the following on the command line:

`pijuice_util.py --dump > dumpfile.js`
