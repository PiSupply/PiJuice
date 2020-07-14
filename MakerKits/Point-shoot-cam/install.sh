#!/bin/bash
cd ~
sudo apt-get update -y
sudo apt-get install pijuice-base -y
sudo git clone https://github.com/PiSupply/PiJuice.git
cd PiJuice/MakerKits/Point-shoot-cam
# Build yuv2rgb extension module and install in current directory.
python3 setup.py install --instal-platlib=.
rm -fr build # delete temporary build directory
cp Point-Shoot.desktop /home/pi/Desktop/
cd ~
grep "start_x=1" /boot/config.txt
if grep "start_x=1" /boot/config.txt
then
        pass
else
        sed -i "s/start_x=0/start_x=1/g" /boot/config.txt
fi

#Install Media Center HAT
rotate="270"
if grep -q "dtoverlay=media-center" /boot/config.txt
    then
      sed -i 's/dtoverlay=media-center,speed=32000000,rotate=.*/dtoverlay=media-center,speed=32000000,rotate='$rotate'/g' "/boot/config.txt"
    else
      cat >> /boot/config.txt <<EOF
dtoverlay=media-center,speed=32000000,rotate=$rotate
EOF
    fi
    if ! grep -q "dtparam=spi=on" "/boot/config.txt"; then
        cat >> /boot/config.txt <<EOF
dtparam=spi=on
EOF
    fi

if [[ -d /usr/share/X11/xorg.conf.d ]]; then

  cat > /usr/share/X11/xorg.conf.d/99-fbdev.conf <<EOF
Section "ServerLayout"
    Identifier "TFT"
    Option "BlankTime" "10"
    Screen 0 "ScreenTFT"
EndSection

Section "ServerLayout"
    Identifier "HDMI"
    Option "BlankTime" "10"
    Screen 0 "ScreenHDMI"
EndSection

Section "ServerLayout"
    Identifier "HDMITFT"
    Option "BlankTime" "10"
    Screen 0 "ScreenHDMI"
    Screen 1 "ScreenTFT" RightOf "ScreenHDMI"
#   Screen 1 "ScreenTFT" LeftOf "ScreenHDMI"
#   Screen 1 "ScreenTFT" Above "ScreenHDMI"
#   Screen 1 "ScreenTFT" Below "ScreenHDMI"
#   Screen 1 "ScreenTFT" Relative "ScreenHDMI" x y
#   Screen 1 "ScreenTFT" Absolute x y
EndSection

Section "Screen"
    Identifier "ScreenHDMI"
    Monitor "MonitorHDMI"
    Device "DeviceHDMI"
Endsection

Section "Screen"
    Identifier "ScreenTFT"
    Monitor "MonitorTFT"
    Device "DeviceTFT"
Endsection

Section "Monitor"
    Identifier "MonitorHDMI"
Endsection

Section "Monitor"
    Identifier "MonitorTFT"
Endsection

Section "Device"
    Identifier "DeviceHDMI"
    Driver "fbturbo"
    Option "fbdev" "/dev/fb0"
    Option "SwapbuffersWait" "true"
EndSection

Section "Device"
    Identifier "DeviceTFT"
    Driver "fbturbo"
    Option "fbdev" "/dev/fb1"
EndSection
EOF

  if [ "${rotate}" == "0" ]; then
    invertx="0"
    inverty="0"
    swapaxes="0"
    tmatrix="1 0 0 0 1 0 0 0 1"
  fi
  if [ "${rotate}" == "90" ]; then
    invertx="1"
    inverty="0"
    swapaxes="1"
    tmatrix="0 -1 1 1 0 0 0 0 1"
  fi
  if [ "${rotate}" == "180" ]; then
    invertx="1"
    inverty="1"
    swapaxes="0"
    tmatrix="-1 0 1 0 -1 1 0 0 1"
  fi
  if [ "${rotate}" == "270" ]; then
    invertx="0"
    inverty="1"
    swapaxes="1"
    tmatrix="0 1 0 -1 0 1 0 0 1"
  fi

  filename="/usr/share/X11/xorg.conf.d/99-ads7846-cal.conf"
  if [ ! -f $filename ]
  then
    touch $filename
  fi

  cat > /usr/share/X11/xorg.conf.d/99-ads7846-cal.conf <<EOF
Section "InputClass"
  Identifier "calibration"
  MatchProduct "ADS7846 Touchscreen"
  Option "EmulateThirdButton" "1"
  Option "EmulateThirdButtonButton" "3"
  Option "EmulateThirdButtonTimeout" "1500"
  Option "EmulateThirdButtonMoveThreshold" "30"
  Option "InvertX" "$invertx"
  Option "InvertY" "$inverty"
  Option "SwapAxes" "$swapaxes"
  Option "TransformationMatrix" "$tmatrix"
EndSection
EOF
  else
    echo "X Server not installed"
fi

sed -i 's/#xserver-layout=/xserver-layout=TFT/g' /etc/lightdm/lightdm.conf
sed -i "/xserver-layout=*/c xserver-layout=TFT" /etc/lightdm/lightdm.conf

echo "Remove the HDMI cable and reboot the Raspberry Pi"
