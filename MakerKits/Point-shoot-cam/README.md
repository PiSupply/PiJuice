# Point-Shoot-Cam

The Point and Shoot Camera Kit uses a combination of the PiJuice HAT, Media Center HAT and Raspberry Pi camera board to create a touch screen standalone camera. This project is designed to highlight the features of the PiJuice HAT as well as the new Media Center HAT.

This Point and Shoot Camera project is based on the Adafruit "DIY WiFi Raspberry Pi Touchscreen Camera" by Phil Burgess with some additional changes, including a battery level indicator for the PiJuice so you will know when your battery gets low.

## What you will need

To build this project your will need the following parts:
* Point and Shoot Camera Maker Kit
* Raspberry Pi Computer
* PiJuice HAT
* Media Center HAT
* Official Raspberry Pi Camera board
* PiSupply Point and shoot Case
* 8/16GB micro SD card
* Raspberry Pi power supply (To charge the PiJuice)

## Installing the Software

There are three different ways in which you can run the software for this project:

1. Download the pre-compiled image and flash to a micro SD card and insert it into the Raspberry Pi
2. Use the auto installation script provided below
3. Manual install the software using the below steps

### 2. Auto-installation

Just run the following script in a terminal window and the point and shoot camera software will be automatically setup:
```bash
# Run this line and the point and shoot software will be setup and installed
curl -sSL https://raw.githubusercontent.com/PiSupply/PiJuice/master/MakerKits/Point-shoot-cam/install.sh | sudo bash
```
### 3. Manual Installation

The manual installation process requires that you have the latest Raspbian Stretch which can be downloaded from the Raspberry Pi website and that you also known some basic linux commands.

1. Make sure you have the most up to date version by running the following command:

```bash
sudo apt-get update
```

2. Install the PiJuice Base software which is required to interact with the PiJuice board. For this project we install the base package but you can also install the GUI:

```bash
#Install Base package
sudo apt-get install pijuice-base

#Install GUI package
sudo apt-get install pijuice-gui
```

3. Download the PiJuice GitHub repository where you will find all the project files for the point and shoot camera:

```bash
git clone https://github.com/PiSupply/PiJuice.git
```

4. Compile the YUV to RGB convertor for the Raspberry Pi camera and copy the Desktop file to the Desktop directory:

```bash
cd PiJuice/MakerKits/Point-shoot-cam
sudo make
cp Point-Shoot.desktop /home/pi/Desktop/
cd ~
```

5. Enable the Raspberry Pi camera by using either `raspi-config` or you can edit the `/boot/config.txt` and add the following line: `start_x=1`.

6. Download the Media Center HAT GitHub repository:

```bash
git clone https://github.com/PiSupply/Media-Center-HAT.git
```

7. Install the Media Center HAT software:

```bash
cd Media-Center-HAT/Software/
sudo bash media-center.sh 90
```

## Usage

To launch the point and shoot camera software, simply double tab on the desktop icon.

### Configure DropBox

Dropbox is a cloud storage solution and synchronising service. Anyone can sign up for free to this service with a basic package of 2GB of storage, and you can increase this using a number of methods such as paying for a subscription or by inviting your friends to sign up.

Due to the nature of the cloud service, it does require an internet connection either through WI-Fi or Ethernet, Wi-Fi being the most practical solution for this project.

1. Create a Dropbox Account

Create a Dropbox account if you don’t already have one. A basic Dropbox account is free and will give you 2GB of storage. Sign up at Dropbox.com

2. Download the Dropbox software using the following commands:
```bash
wget https://github.com/andreafabrizi/Dropbox-Uploader/archive/master.zip
unzip master.zip
rm master.zip
mv Dropbox-Uploader-master Dropbox-Uploader
```
3. Create a new Dropbox App

Visit https://www.dropbox.com/developers/apps and log in with your Dropbox account credentials.

Use the **“Create app”** button to begin the process. Select **“Dropbox API”** and **“Full Dropbox”**, then assign your app a unique name, then click **“Create App”**.

In the setting tab of your new app, there is a section with the heading **“OAuth 2”**, look for the button **“Generate access token”**. This will give us a long string of seemingly random letters, which is a unique identifier for linking your camera to the Dropbox account.

4. Set up Dropbox Uploader

On the Raspberry Pi navigate to the Dropbox uploader directory with the following command:
```bash
cd /home/pi/Dropbox-Uploader
```
Now run the dropbox_uploader.sh script from that directory:
```bash
./dropbox_uploader.sh
```
During the uploader script you will be prompted to enter your access token that was generated in Step 3. This must be exact, which is why using SSH to copy paste can be useful.

**Note:** If you make a mistake during this process you can simply run the script again.

Now let's test everything is working by uploading a sample file:
```bash
echo 12345 > foo.txt
./dropbox_uploader.sh upload foo.txt /
```
This will create text file with the text **”12345”** and then upload it to your Dropbox folder. Open up your web browser and login to Dropbox and see fi the file is there.

Now when you run the point and shoot camera program and select Dropbox from the settings menu, all photos will automatically get uploaded to your Dropbox account.
