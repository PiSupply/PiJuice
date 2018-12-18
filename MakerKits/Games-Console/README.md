# PiJuice Games-Console

The gaming software for this project is based on the latest version of RetroPie, which an operating system for the Raspberry Pi that runs a number of retro-gaming projects such as emulationstation, RetroArch and many others. It allows you to play all your favourite retro arcade games, home-console and some classic PC games.

## What you will need

- Raspberry Pi board
- Media Center HAT
- PiJuice HAT
- Wired/Bluetooth controller
- Keyboard & mouse (initial setup)
-

## Download RetroPie

Go to https://retropie.org.uk and download the latest image for the Raspberry Pi. Flash the RetroPie image to an 16GB or 32GB micro SD card using Etcher software. You can download Etcher from https://etcher.io.

## Boot up RetroPie and connect to Wi-Fi

Before we begin to install the Media Center HAT software you will need to boot RetroPie and connect to your local Wi-Fi for internet access. You will need to connect a keyboard and mouse to the Raspberry Pi during this initial setup but then may be removed later on. You will also need to connect the Raspberry Pi to a HDMI monitor or TV until we install the Media Center HAT software.

Insert the microSD card into the Raspberry Pi and apply power either using the PiJuice or mains power. Pressing SW1 on the PiJuice will apply power to the Raspberry PI via the GPIO header.

When RetroPie boots up it will prompt you to setup gaming controls, this will also allow you to navigate its menu system. Go ahead and assign keys, you can skip others by holding down a key already used.

Once done you will see the emulationstation menu. In here enter the Retropie menu, which should be the first option in emulationstation. Once the Retropie menu opens up you should see an option which says “Configure WiFi”, select this to access the WiFi configuration menu. Select option 1 “Connect to WiFi network”.

The configuration program will then begin to scan for local SSID and show you a list. Select your WiFi router from the list and then enter your password.

Note: You can usually find your credentials on the reverse side of your router box

Once connected you will go back to the main WiFi menu where you should see an IP address assigned to the Raspberry Pi, you are now connected to your Wi-Fi.

## Setting up the Media Center HAT

If you are in the emulationstation menu or RetroPie menu then press F4 to exit and go to the command line. In the command line enter the following command to install the media center HAT:
```bash
curl -sSL https://pisupp.ly/mediacentersoftware | sudo bash
```
Follow the install procedure and then reboot the Raspberry Pi. Then enter the following command to copy the framebuffer to the Media Center HAT:
```bash
sudo nano /etc/rc.local
```
Before the last line exit 0 add the following:
```bash
fbcp &
```
CTRL+O to save the changes and the CTRL+X to exit.

Now we need to change some of the output settings in the config.txt file so that it outputs the correct resolution for our Media Center HAT screen size, this will stop anything from getting cut off at the edge of the screen. To edit the file enter the following command:
```bash
sudo nano /boot/config.txt
```
In the config.txt file add the following or comment out and change the values of the following:
```bash
hdmi_force_hotplug=1
hdmi_group=2
hdmi_mode=87
hdmi_cvt=320 240 60 1 0 0 0
```
## Setting up PS3 Bluetooth Controller
