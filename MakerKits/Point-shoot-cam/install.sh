#!/bin/bash
cd ~
sudo apt-get update -y
sudo apt-get install pijuice-base -y
git clone https://github.com/PiSupply/PiJuice.git
git clone https://github.com/PiSupply/Media-Center-HAT.git
cd PiJuice/MakerKits/Point-shoot-cam
sudo make
cp Point-Shoot.desktop /home/pi/Desktop/
cd ~
grep "start_x=1" /boot/config.txt
if grep "start_x=1" /boot/config.txt
then
        pass
else
        sed -i "s/start_x=0/start_x=1/g" /boot/config.txt
fi
cd Media-Center-HAT/Software/
sudo bash install.sh
