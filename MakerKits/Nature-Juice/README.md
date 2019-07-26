# NatureJuice

The NatureJuice is a variation of the NatureBytes kit and leverages on the possibilities offered by PiJuice.

There is an [assembly guide available on the PiSupply MakerZone](https://learn.pi-supply.com/make/pijuice-naturejuice-project/) which shows how to put together the kit and make the necessary connections.

NatureJuice comes with a sample script which can be installed as a Systemd service and run at startup.
A modified PIR Motion Sensor is connected to the PiJuice and allows to powerup the Raspberry Pi when motion is detected. This is triggered via a rising edge on IO2.
The PiJuice P3 connector provides 5V as well as 3V3 but only 3V3 remains available once power is removed to the Raspberry Pi. The PIR Motion Sensor then needed to be modified to run at 3v3 as it's natively intended to run at 5V.
The sensor will have to be modified as in the picture below. The voltage regulator and the diode will need to be removed and a small wire soldered in so that VCC will carry directly 3V3 from P3 on the PiJuice.

![PIR Mod](https://user-images.githubusercontent.com/16068311/54878254-63b65f00-4e2a-11e9-922e-37ad1f833c22.jpg "PIR Mod")

Using the API provided by PiJuice we can monitor the PIR at runtime and the script will continue to take pictures as far as motion is captured.
The script will wait for 10 minutes and if no motion is detected it is going to shutdown the Raspberry Pi. The PiJuice will have to be setup so that it will remove power after the shutdown is complete.

## Installing the software

This project requires [Raspbian Lite](https://www.raspberrypi.org/downloads/raspbian/). There is no reasons why Raspbian full cannot be used for this setup thereby making preparation steps easier but given the purposes of the project it would be quite wasteful to use this version.

As this is going to be an headless setup make sure to add an [empty ssh file](https://www.raspberrypi.org/documentation/remote-access/ssh/) to your SD card. Similarly you can also create a wpa_supplicant.conf to be placed in the boot partition of the SD card.
It will be looking like:

```text
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1
country=GB

network={
    ssid="yourSSID"
    psk="yourWPAKey"
    key_mgmt=WPA-PSK
}
```

Using the raspi-config command enable the [Raspberry Pi camera](https://www.raspberrypi.org/documentation/usage/camera/).

### Optimise the configuration

One of the main point of this projects is to do battery efficiency usage. We have made some modifications to the OS configuration so that hardware and softwware modules are disabled to conserve power.

#### Bluetooth

As we don't need Bluetooth we need to disable it.
Edit /boot/config.txt and add the following line at the bottom:

`dtoverlay=pi3-disable-bt`

then run:

```bash
sudo systemctl disable bluetooth.service
```

on the next boot you can run the following command to check that BT is disabled:

```bash
hcitool dev
```

#### Wi-Fi

You could also disable Wi-Fi if you have other means to connect to you Raspberry Pi. For this project we assumed Wi-Fi was required as we were using an A+ and wanted remote control on the OS.

Add the following line to your /boot/config.txt to disable the Wi-Fi

`dtoverlay=pi3-disable-wifi`

#### USB

Disabling the USB is also an option that could help drawing less power. The USB part is a bit more tricky and cannot be directly added to the main config file.
It can be added though to the naturejuice.sh script or executed on the command line each time.

*If you are going to use a USB stick for your pictures you will obviously not want to do this part*

For an A+ you will need to execute or add to naturejuice.sh the following line:

```bash
echo 'usb1' | sudo tee /sys/bus/usb/drivers/usb/unbind
```

for a B+:
```bash
echo '1-1' | sudo tee /sys/bus/usb/drivers/usb/unbind
```

#### Disable the camera LED

*This is only applicable to version 1.3 of the Raspberry Pi Camera*

Although this won't have visible repercussions on the battery duration you can disable the camera LED. You may want to do this to limit factors that might scare wildlife especially in the dark.

You can do this by adding the following line to /boot/config.txt
```text
disable_camera_led=1
```

#### On-board LEDs and HDMI

Disabling the on-board LEDs and the HDMI is delegated to the naturejuice.sh script which at boot will run the following commands:

```bash
# Disables Raspberry Pi LEDs. Determines is it is a Zero board or not
revision=`cat /proc/cpuinfo | grep 'Revision' | awk '{print $3}'`
# Is it a Zero?
if [[ "9000" == ${revision:0:4} ]]; then
    echo 1 | sudo tee /sys/class/leds/led0/brightness
else
    echo 0 | sudo tee /sys/class/leds/led0/brightness
    echo 0 | sudo tee /sys/class/leds/led1/brightness
fi
```
This part of the script determines if the board is a Raspberry Pi Zero or not as the LEDs are used in a different way on the Zero compared to the full boards.

```bash
# Disables the HDMI port
/opt/vc/bin/tvservice -off
```

### Auto-installation

```bash
# Run this line and the point and shoot software will be setup and installed
curl -sSL https://raw.githubusercontent.com/PiSupply/PiJuice/master/MakerKits/Nature-Juice/install.sh | sudo bash
```

Move to [Configure the PiJuice](#configure-the-pijuice)

### Manual installation

A part from the intial configuration there are a series of packages that need to be added to Raspbian Lite.
First issue an update:
```bash
sudo apt-get update
```

then install the PiJuice software:
```bash
sudo apt-get install pijuice-base
```

finally install the extra packages:
```bash
sudo apt-get install git i2c-tools python3-smbus python3-urwid python3-picamera python3-pil fonts-freefont-ttf
```

You will then need to install the PiJuice source code which contains the scripts for this project:

```bash
cd ~
git clone https://github.com/PiSupply/PiJuice.git
```

Move into the NatureJuice folder:
```bash
cd ~/PiJuice/MakerKits/Nature-Juice
```

Then copy the various scripts and enable the service:
```bash
sudo cp naturejuice.py naturejuice.sh /usr/local/bin/
sudo cp naturejuice.service /etc/systemd/system/
sudo systemctl enable naturejuice.service
```

Finally reboot the OS:
```bash
sudo reboot
```

Move to [Configure the PiJuice](#configure-the-pijuice)

#### Default folders

The default folders where the various parts of the script will be installed are:

* bin: /usr/local/bin
* logs: /var/log/naturejuice_camera.log
* images: /var/lib/nature-juice
* fonts: /usr/share/fonts/truetype/freefont/

### Configure the PiJuice

The PiJuice can be configured by using:
```bash
pijuice_cli
```

#### System Tasks

Enable `Software Halt Power Off` and change the `Delay Period [seconds]` to 30.
This option tells the PiJuice to remove power to the Raspberry Pi after a shutdown, halt, poweroff have been issued.
For this project the default 10 might be good enough but it is best to give more time to the OS to properly shutdown.

#### IO -> IO2

So that the PIR Motion Sensor Dout pin can trigger the event to power on the Raspberry Pi we need to change the IO2 settings as follows:
```text
IO2:  Mode: DIGITAL_IN
      Pull: PULLDOWN
      wakeup [0/1]: RISING_EDGE
```

#### Battery Profile
You will also need to configure the battery characteristics. For this project we have used a 5000mAh which can be chosen from the list of available profiles as LIPO_5000.

## Usage

The Python script has been developed following the existing ones from [NatureBytes](https://github.com/naturebytes/Naturebytes-RaspPi-Dev).
It can be invoked from the command line as follows:

```bash
./naturejuice.py
```
```text
-l                  Overlay NatureJuice logo onto captured image
-o <outputlocation> Specify output file location
-v                  Verbose - print log in terminal
-f                  Force a trigger when launching the script
```

We have developed a new logo that can be used as a watermark for the pictures. You can add it and a timestamp by using the -l option.

![naturejuice-logo](https://raw.githubusercontent.com/PiSupply/PiJuice/master/MakerKits/Nature-Juice/nature-juice_logo_90.png)

Despite not using the -v flag you can easily check the logs as they are created with:

```bash
tail -f /var/log/naturejuice_camera.log
```

or the whole of the file using:

```bash
less /var/log/naturejuice_camera.log
```

The -f option is used by the naturejuice.sh when invoking naturejuice.py so that a picture will be forcefully taken on power up. The assumption is that the Raspberry Pi will only be awoken when a PIR event is triggered therefore a picture needs to be taken as soon as the OS is up. The naturejuice.py script will then start function as normal waiting for further PIR motion detection events.

### Disable the service

should you wish to use the script without running as a service you can disable it by issuing:

```bash
sudo systemctl stop naturejuice.service
```

then

```bash
sudo systemctl disable naturejuice.service
```
 
