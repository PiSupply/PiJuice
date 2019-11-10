# PiJuice Remote Camera

This project uses the PiJuice HAT and official Raspberry Pi camera module to create a CCTV camera system with battery backup in case of sudden power loss. It can also be used in-conjunction with the Pi Supply PoE Switch HAT for a complete power and backup power solution to keep you camera up and monitoring 24/7. The software for this project is provided by motioneye OS, which allows you to setup the camera module as a CCTV monitor with motion sensing and recording. We have configured a install script that will install and configure this project on Raspbian Lite OS.

## What you will need

- Raspberry Pi Board
- Pijuice HAT
- Official Raspberry Pi camera board
- PoE HAT (optional)
- micro SD card (16GB min)
- Power supply

## Installing the Software
This project runs on the latest version of [Raspbian Lite OS](https://www.raspberrypi.org/downloads/) for the Raspberry Pi. You will need to run the following commands in the terminal window to install the libraries for the weather sensors.

### Auto Installation

Just run the following line in the terminal to automatically install all the libraries and project files to the Raspberry Pi.

Raspbian OS Install Script:
```bash
# Run this line and the weather station will be setup and installed
curl -sSL https://raw.githubusercontent.com/PiSupply/PiJuice/master/MakerKits/remote-camera/motioneye-install.sh | sudo bash
```

### Manual Installation
Install `ffmpeg` and other `motion` dependancies:
```bash
sudo apt-get install ffmpeg libmariadb3 libpq5 libmicrohttpd12
```

Install `motion`:
```bash
wget https://github.com/Motion-Project/motion/releases/download/release-4.2.2/pi_buster_motion_4.2.2-1_armhf.deb
sudo dpkg -i pi_buster_motion_4.2.2-1_armhf.deb
```

Install `motioneye` dependancies:
```bash
sudo apt-get install python-pip python-dev libssl-dev libcurl4-openssl-dev libjpeg-dev libz-dev
```

Install `motioneye`:
```bash
sudo pip install motioneye
```

Prepare the configuration directory:
```bash
sudo mkdir -p /etc/motioneye
sudo cp /usr/local/share/motioneye/extra/motioneye.conf.sample /etc/motioneye/motioneye.conf
```
Prepare the media directory:
```bash
sudo mkdir -p /var/lib/motioneye
```

Add service script and load daemon:
```bash
sudo cp /usr/local/share/motioneye/extra/motioneye.systemd-unit-local /etc/systemd/system/motioneye.service
sudo systemctl daemon-reload
sudo systemctl enable motioneye
sudo systemctl start motioneye
```

Install `PiJuice` Base:
```bash
sudo apt-get install pijuice-base
```

Finally reboot your device:
`sudo reboot`
