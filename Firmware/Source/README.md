How to setup PiJuice firmware project:

-Download and install Development Environment Atollic for ARM or IAR (Atollic recommended).
-Download and install STM32CubeMX windows utility from STM site. http://www.st.com/en/development-tools/stm32cubemx.html
-Run utility and create new project, File->New Project, then select mcu: STM32F030CCTx.
-From CubeMX utility go to Project->Generate Code for desired environment (Atollic recommended) and it will automatically create initial template project.
-Replace and add all files found in project directory with ones from Drivers, Inc and Src in this directory.
	
In Atollic environment go to:

Properties->C/C++ Build->Settings->C Compiler->Optimization
select: Optimize for speed (-Ofast), from drop-down list and click apply.

Then go to:
Properties->C/C++ Build->Settings->Other->Output format
select Binary from drop-down list and click ok.

Click on Project->Build All and after build finish, binary file: PiJuice.elf.binary, 
will be generated inside Debug directory.
	
