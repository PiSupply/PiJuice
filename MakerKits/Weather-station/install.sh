#!/usr/bin/env bash

sudo apt-get update

sudo apt-get install python-pip

sudo pip3 install Adafruit_DHT

sudo apt-get install pijuice-gui -y

git clone https://github.com/ChristopherRush/weather.git

sed -e '$i sudo python3 /home/pi/weather/weather.py' rc.local

sudo sh -c "echo 'dtparam=spi=on' >> /boot/config.txt"

sudo reboot
