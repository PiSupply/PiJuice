# This PiJuice test program polls pijuice HAT for state of charge and prints present configuration

```
python pijuicetest.py
```

user_func1.py can be used for you to test user scripts. It will log events that are enabled. Attached are images of gui how to setup. It will generate log file with time of event and event id. You can leave it to fully discharge as test and than when you plug in you can look at log file to see if there is forced power off event, also you can set button to log press events, and any other case you think is good to test.

![system_events](https://user-images.githubusercontent.com/3359418/27130530-8a728f0e-50fe-11e7-9a90-7b8e30b90c68.jpg)
![system_task](https://user-images.githubusercontent.com/3359418/27130531-8b854828-50fe-11e7-9125-e43f82b70806.jpg)
![user_scripts](https://user-images.githubusercontent.com/3359418/27130533-8ca06044-50fe-11e7-8ab9-e50e47a9f8aa.jpg)

Also on fresh unit it is needed to do current sense calibration, usually during production test. For your unit use pijuice_calib.py script. Procedure is to power rpi and pijuice separately (rpi with adaptor, pijuice with battery) and connect I2C with cable, no power wire connection. Add 100 ohm resistor to pijuice 5V gpio, that will draw around 50mA from hat, and than run pijuice_calib.py once. If it is succesfull when you open config gui at HAT tab you will see GPIO power input current is around 50mA. 50mA is used as threshold in detecting lower power mode of work, so when below 50mA pijuice will draw less than 1mA from battery and in that state charge status LED will have short blinks.
