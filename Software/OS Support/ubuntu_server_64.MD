# Installing PiJuice Software on Ubuntu Server 64-bit

https://ubuntu.com/download/raspberry-pi

1. Run the following script to build the pijuice packages for 64-bit (thanks to @MarkusLange):
```
#!/bin/bash

cd $HOME

sudo apt update
sudo apt install -y python3-smbus python3-pip python3-stdeb dh-python debhelper fakeroot python3-all
sudo pip3 install stdeb3

sudo git clone https://github.com/PiSupply/PiJuice.git

cd $HOME/PiJuice/Software/Source/Setuid-Wrapper
sudo ./build-setuid-progs.sh
sudo cp p* $HOME/PiJuice/Software/Source/bin

cd $HOME/PiJuice/Software/Source/
sudo ./pckg-pijuice.sh

sudo cp $HOME/PiJuice/Software/Source/deb_dist_base/pi*.deb $HOME
sudo cp $HOME/PiJuice/Software/Source/deb_dist_gui/pi*.deb $HOME

cd $HOME
sudo rm -rf PiJuice
```

2. Install the localpijuice-base:
```
cd $HOME
sudo apt install ./pijuice-base_1.6_all.deb
```

3. Install the local pijuice-gui (if needed):
```
cd $HOME
sudo apt install ./pijuice-gui_1.6_all.deb
```

4. Fix broken packages:\
`sudo apt --fix-broken install`

5. Reboot and run PiJuice CLI or PiJuice GUI when on Desktop:\
`sudo reboot`\
Once rebooted run:\
`pijuice_cli` or `pijuice_gui`
