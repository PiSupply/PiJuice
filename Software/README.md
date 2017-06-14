# PiJuice Software

Default battery profile is defined by dip switch position. On v1.1 it is position 01 for BP7X, on v1.0 version, it might be different, you can try different positions, and power circle pijuice to get updated.

It is not possible to detect battery not present when powered through on board usb micro, so it might show 0% only.

User functions are 4 digit binary coded and have 15 combinations, code 0 is USER_EVENT meant that it will not be processed by system task, but left to user and python API to manage it. I thought it is rare case that all 15 will be needed so on gui there is 8 (it will make big window also). However if someone needs more scripts it can be manualy added by editing config json file: /var/lib/pijuice/pijuice_config.JSON. Also all other configurations can be managed manually in this file if gui is not available. 
