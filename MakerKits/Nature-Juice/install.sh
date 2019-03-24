#!/usr/bin/env bash

sudo apt-get update -y
sudo apt-get install pijuice-base -y

sudo apt-get install git i2c-tools python3-smbus python3-urwid python3-picamera python3-pil fonts-freefont-ttf -y

cd ~

git clone https://github.com/PiSupply/PiJuice.git

cd ~/PiJuice/MakerKits/Nature-Juice

sudo cp naturejuice.py naturejuice.sh /usr/local/bin/
sudo cp naturejuice.service /etc/systemd/system/
sudo systemctl enable naturejuice.service

sudo reboot
