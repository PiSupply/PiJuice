#!/bin/bash
sudo apt update

if grep "start_x=1" /boot/config.txt
then
        pass
else
        sudo sed -i "$ a start_x=1" /boot/config.txt
fi

sudo apt-get install ffmpeg libmariadb3 libpq5 libmicrohttpd12 -y
wget https://github.com/Motion-Project/motion/releases/download/release-4.2.2/pi_buster_motion_4.2.2-1_armhf.deb
sudo dpkg -i pi_buster_motion_4.2.2-1_armhf.deb
sudo apt-get install python-pip python-dev libssl-dev libcurl4-openssl-dev libjpeg-dev libz-dev -y
sudo pip install motioneye
sudo mkdir -p /etc/motioneye
sudo cp /usr/local/share/motioneye/extra/motioneye.conf.sample /etc/motioneye/motioneye.conf
sudo mkdir -p /var/lib/motioneye
sudo cp /usr/local/share/motioneye/extra/motioneye.systemd-unit-local /etc/systemd/system/motioneye.service
sudo systemctl daemon-reload
sudo systemctl enable motioneye
sudo systemctl start motioneye

wget https://raw.githubusercontent.com/PiSupply/PiJuice/master/MakerKits/remote-camera/monitor_1
sudo mv monitor_1 /etc/motioneye/
sudo apt-get install pijuice-base -y
sudo reboot
