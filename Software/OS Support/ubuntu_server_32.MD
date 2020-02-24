# Installing PiJuice Software on Ubuntu Server 32-bit


https://ubuntu.com/download/raspberry-pi

1. Make sure you have the latest version of Kali Linux by running an update:
`sudo apt-get update`

2. Install the dependancies:
`sudo apt install i2c-tools lua5.3`

3. Add user `ubuntu` to I2C user group:
`sudo usermod -aG i2c ubuntu`

4. Add user `ubuntu` to pijuice user group:
`sudo visudo`
Add:
`ubuntu ALL=(pijuice) ALL`

5. Clone the PiJuice repository from GitHub:
`git clone https://github.com/PiSupply/PiJuice.git`

6. Install pijuice-base:
`cd PiJuice/Software/Install`
`dpkg -i pijuice-base_1.6_all.deb`

Fix broken packages:
`sudo apt --fix-broken install`

7. Reboot and run PiJuice CLI:
`sudo reboot`
Once rebooted run:
`pijuice_cli`
