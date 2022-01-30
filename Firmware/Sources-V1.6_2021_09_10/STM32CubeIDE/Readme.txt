Intall STM32CubeIDE:
	Download and Install PC software from the page: https://www.st.com/en/development-tools/stm32cubeide.html

Load Project:
	- From STM32CubeIDE, File->Open Projects from File System.. and brouse path to PiJuice/STM32CubeIDE in the 
	Import sources box and click finish.
	Alternatively Double click on .project inside PiJuice\STM32CubeIDE project directory.

Build:
	-Project->Build Project. Find generated binary and hex files in: PiJuice\STM32CubeIDE\Debug\ directory.

Debug:
	- Connect ST-Link to PiJuice HAT board. Connect ST link(https://www.st.com/en/development-tools/st-link-v2.html) to 
	PC using USB Cable. Connect ST Link to PiJuice HAT (J7 connector) using Tag-connect cable TC2050-IDC
	(https://www.digikey.com/en/products/detail/tag-connect-llc/TC2050-IDC/2605366), break one leg so it fits inside three holes.
	Also you need Tag-connect adapter TC2050-ARM2010 (https://www.digikey.com/en/products/detail/tag-connect-llc/TC2050-ARM2010/3528170) 
	to connect ST-Link to Tag-connect Cable. NOTE: This connection only works for PiJuice HAT, PiJuice Zero does not feature J7.

	- Launch Debugger:
		Run->Debug As->STM32 Cortex...

	- Start Debugger:
		Run->Resume
	

Use stm32flash to write binary:
	Install stm32flash tool:
		sudo apt install stm32flash
	Start bootloader: 
		i2cset -y 1 0x14 0xfe 0xfe01 w
	Backup config:
		stm32flash -a 0x41 /dev/i2c-1 -S 0x0803BC00  -r ./PiJuice-config.bin
	Write firmware (update file path):
		stm32flash -a 0x41 /dev/i2c-1 -w ./PiJuice-V1.5_2021_02_06.elf.binary
	Restore config:
		stm32flash -a 0x41 /dev/i2c-1 -s 120 -e 0 -vR -w ./PiJuice-config.bin
	