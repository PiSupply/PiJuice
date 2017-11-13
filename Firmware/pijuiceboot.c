/*******************************************************************************
file: rpi_zb_server.c
rev: 1.0
description: Functions for connected devices caching.

Copyright 2015 - 2016, electronicshowto.com. All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
		* Redistributions of source code must retain the above copyright
		  notice, this list of conditions and the following disclaimer.
		* Redistributions in binary form must reproduce the above copyright
		  notice, this list of conditions and the following disclaimer in the
		  documentation and/or other materials provided with the distribution.
		* Neither the name of electronicshowto.com nor the
		  names of its contributors may be used to endorse or promote products
		  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL ANDY GOCK BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
	
compile: ~/ cc rpi_zb_server.c -o zb_server -lwebsockets
run: ~/ ./zb_server /dev/ttyAMA0
code depends on libwebsockets library: ~/ sudo apt-get install libwebsockets-dev
********************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> 
#include <poll.h>
#include <unistd.h>
#include <termios.h>
#include<signal.h>
#include <errno.h>
#include <fcntl.h>
#include<sys/ioctl.h>
#include <linux/i2c-dev.h>
//#include <libwebsockets.h>

#define BOOT_START	0x7F
#define ACK	0x79
#define ERASE_PAGE_SIZE             2048
#define WRITE_PAGE_SIZE             256 //((uint32_t)0x0400)  // Page size = 1KByte 
#define EEPROM_START_ADDRESS  ((uint32_t)0x08000000)


static uint8_t rcvData[1024];
//static uint8_t sendData[1024];

uint8_t blVersion = 0;
uint8_t chipID = 0;

static int destroy_flag = 0;

struct per_session_data {
	int fd;
};

int sysBootloaderFd = 0;
int appI2CFd = 0;

// Send message over UART interface
void UartSend(uint8_t* buf, uint8_t len)
{
	int remain = len;
	int offset = 0;

	while (remain > 0)
	{
		int sub = (remain >= 8 ? 8 : remain);
		//printf("writing %d bytes (offset = %d, remain = %d)\n", sub, offset, remain);
		write(sysBootloaderFd, buf + offset, sub);

		tcflush(sysBootloaderFd, TCOFLUSH);
		usleep(5000);
		remain -= 8;
		offset += 8;
	}
	return;
}

// Initialize UART port
int32_t OpenUart(char *devicePath)
{
	struct termios tio;
	/* open the device to be non-blocking (read will return immediatly) */
	sysBootloaderFd = open(devicePath, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (sysBootloaderFd < 0)
	{
		perror(devicePath);
		printf("%s open failed\n", devicePath);
		return (-1);
	} else {
			printf("Serial open: %d\n", sysBootloaderFd);
	}

	//make the access exclusive so other instances will return -1 and exit
	ioctl(sysBootloaderFd, TIOCEXCL, 0);
	
	tcgetattr(sysBootloaderFd, &tio); //added
 
	/* c-iflags
	 B115200 : set board rate to 115200
	 CRTSCTS : HW flow control (disabled below)
	 7E1     : 7E1 (7bit,even parity,1 stopbit)
	 CLOCAL  : local connection, no modem contol
	 CREAD   : enable receiving characters*/
	tio.c_cflag = B38400 | CRTSCTS | CS8 | CLOCAL | CREAD;// | CSTOPB;
	tio.c_cflag &= ~PARODD; // even parity
	/* c-iflags
	 ICRNL   : maps 0xD (CR) to 0x10 (LR), we do not want this.
	 IGNPAR  : ignore bits with parity erros, I guess it is
	 better to ignStateore an erronious bit then interprit it incorrectly. */
	//tio.c_iflag = IGNPAR & ~ICRNL;
	tio.c_oflag = 0;
	tio.c_lflag = 0;

	tcflush(sysBootloaderFd, TCIFLUSH);
	tcsetattr(sysBootloaderFd, TCSANOW, &tio);
	
	//NOTE: To get work uart with raspbian, disable serial console via raspi-config

	//Send the bootloader force boot incase we have a bootloader that waits
	//uint8_t forceBoot[] = {SB_FORCE_RUN, SB_FORCE_RUN_1};
	//UartSend(forceBoot, 2);
	
	return sysBootloaderFd;
}

int32_t OpenI2C(char *devicePath) {
	int addr = 0x41;
    if ((sysBootloaderFd = open(devicePath,O_RDWR)) < 0) {
        printf("Failed to open the bus.");
        /* ERROR HANDLING; you can check errno to see what went wrong */
        return -1;
    }

    if (ioctl(sysBootloaderFd,I2C_SLAVE,addr) < 0) {
        printf("Failed to acquire bus access and/or talk to slave.\n");
        /* ERROR HANDLING; you can check errno to see what went wrong */
        return -1;
    }
	return sysBootloaderFd;
}

int32_t OpenAppI2C(char *devicePath, int addr) {
	int fd;
    if ((fd = open(devicePath,O_RDWR)) < 0) {
        printf("Failed to open the bus.");
        /* ERROR HANDLING; you can check errno to see what went wrong */
        return -1;
    }

    if (ioctl(fd,I2C_SLAVE,addr) < 0) {
        printf("Failed to acquire bus access and/or talk to slave.\n");
        /* ERROR HANDLING; you can check errno to see what went wrong */
        return -1;
    }
	return fd;
}

void ClosePort(void)
{
	tcflush(sysBootloaderFd, TCOFLUSH);
	close(sysBootloaderFd);

	return;
}

void CloseI2C(void) {
	close(sysBootloaderFd);
}

// populates the Frame Check Sequence of the mt message
void calcFcs(uint8_t *msg, int size)
{
	uint8_t result = 0;
	int idx = 1; //skip SOF
	int len = (size - 1); // skip FCS

	while ((len--) != 0)
	{
		result ^= msg[idx++];
	}

	msg[(size - 1)] = result;
}

uint8_t GetCheckSum(uint8_t *msg, int size)
{
	uint8_t result = 0;
	int i;

	for (i=0;i<size;i++) result ^= msg[i];
	return result;
}

// send mt command to soc
/*uint8_t SendCommand(uint8_t pBuf[], uint32_t clientFd)
{
	uint8_t *buff = malloc(3+pBuf[1]);
	if (buff)
	{
		uint8_t *cmd = buff;
		*cmd++ = 0xFE; // sof
		*cmd++ = pBuf[1] - 2; // length
		memcpy(cmd, &pBuf[2], pBuf[1]);
		buff[2+pBuf[1]] = 0; // fcs

		calcFcs(buff, 3+pBuf[1]);
		UartSend(buff, 3+pBuf[1]);

		free(buff);
	}
}*/


int Receive(uint8_t *data)
{
	uint8_t ack, dataLen;
	int bytesRead;
	static uint8_t retryAttempts = 0;

	//bytesRead=read(sysBootloaderFd, &ack, 1);
	//if (bytesRead < 1) return -1;
	// get ACK
	int i = 0;
	while (read(sysBootloaderFd, &ack, 1) < 1 && i < 200) {
		i++;
		usleep(10000);
	}
	if (i >= 200) return -1;
	if (ack != ACK) {
		printf("no ack %x\n", ack);
		return -2;
	}
	
	//if (ack != ACK) return -2;
	
	/*bytesRead = read(sysBootloaderFd, &dataLen, 12);
	printf("dataLen %d, %d\n", bytesRead, dataLen);
	if (bytesRead < 1) return -3;*/
	
	bytesRead = read(sysBootloaderFd, data, 13);
	dataLen = data[0];
	printf("bytesRead %d  dataLen %d\n", bytesRead, dataLen);
	if (bytesRead < dataLen) return -4;
	
	bytesRead=read(sysBootloaderFd, &ack, 1);
	printf("ack %x\n", ack);
	printf("data: %x, %x, %x\n", data[0], data[1], data[12]);
	if (bytesRead < 1) return -5;
	
	if (ack != ACK) return -6;
	
	//while ( (bytesRead=read(sysBootloaderFd, &sofByte, 1)) == 1 && sofByte != 0xFE );

	return dataLen;
}

int ReceiveACK()
{
	uint8_t ack;
	int bytesRead;
	
	bytesRead=read(sysBootloaderFd, &ack, 1);
	printf("ack %x\n", ack);
	if (bytesRead < 1) return -5;
	if (ack != ACK) return -6;
	return ack;
}

int ReceiveByteNum()
{
	uint8_t ack, dataLen;
	int bytesRead;
	static uint8_t retryAttempts = 0;

	bytesRead=read(sysBootloaderFd, &ack, 1);
	if (bytesRead < 1) return -1;
	
	if (ack != ACK) return -2;
	
	bytesRead = read(sysBootloaderFd, &dataLen, 1);
	printf("dataLen %d\n", dataLen);
	if (bytesRead < 1) 
		return -3;
	else 
		return dataLen;
}

int SendByte(uint8_t b) {
	int n = write(sysBootloaderFd, &b, 1);
	usleep(500);	
	return n;
}

int SendGetCmd() {
	uint8_t sendData[2] = {0x00, 0xFF};
	
	//tcflush(sysBootloaderFd, TCIFLUSH);
	return write(sysBootloaderFd, sendData, 2);
	usleep(2000);
	return write(sysBootloaderFd, &(sendData[1]), 1);
	usleep(2000);
}

int GetVerReadProtection() {
	uint8_t sendData = 0x01;
	uint8_t data[100], ack;
	
	tcflush(sysBootloaderFd, TCIFLUSH);
	write(sysBootloaderFd, &sendData, 1);
	usleep(10000);
	sendData = 0xFE;
	write(sysBootloaderFd, &sendData, 1);
	usleep(50000);
	int n = ReceiveByteNum();
	if (n < 1) return -1;
	int bytesRead = read(sysBootloaderFd, data, n);
	if (bytesRead >= 0) {
		blVersion = data[0];
		return blVersion; // return bootloader version
	} else return -1;
	
	bytesRead=read(sysBootloaderFd, &ack, 1);
	printf("ack %x\n", ack);
	if (bytesRead < 1) return -5;
	if (ack != ACK) return -6;
	
	return 0;
}

int GetID() {
	uint8_t sendData = 0x02, ack;
	uint8_t data[5];
	
	tcflush(sysBootloaderFd, TCIFLUSH);
	write(sysBootloaderFd, &sendData, 1);
	usleep(1000);
	sendData = 0xFD;
	write(sysBootloaderFd, &sendData, 1);
	usleep(10000);
	int n = ReceiveByteNum();
	if (n < 1) return -1;
	int bytesRead = read(sysBootloaderFd, data, n);
	
	if (bytesRead >= 0) {
		chipID = data[0];
		return chipID; // return PID
	} else return -1;
	
	bytesRead=read(sysBootloaderFd, &ack, 1);
	printf("ack %x\n", ack);
	if (bytesRead < 1) return -5;
	if (ack != ACK) return -6;
	
	return 0;
}

int ReadMemory(uint32_t addr, uint8_t *data, int32_t size)
{
	uint8_t ack, dataLen;
	int bytesRead, i;
	uint8_t sendData[10];
	
		// send read command
	tcflush(sysBootloaderFd, TCIFLUSH);
	sendData[0] = 0x11;
	sendData[1] = 0xEE;
	write(sysBootloaderFd, sendData, 2);
	usleep(10000);
	
	// get ACK
	bytesRead=read(sysBootloaderFd, &ack, 1);
	if (bytesRead < 1) return -1;
	if (ack != ACK) return -2;
	
	// send address
	sendData[3] = addr & 0x000000FF;
	addr >>= 8;
	sendData[2] = addr & 0x000000FF;
	addr >>= 8;
	sendData[1] = addr & 0x000000FF;
	addr >>= 8;
	sendData[0] = addr & 0x000000FF;
	sendData[4] = GetCheckSum(sendData, 4);
	write(sysBootloaderFd, sendData, 5);
	usleep(500);
	
	// get address ACK
	bytesRead=read(sysBootloaderFd, &ack, 1);
	if (bytesRead < 1) return -3;
	if (ack != ACK) return -4;
	usleep(500);
	
	// send number of bytes to read
	//SendByte(size-1);
	//SendByte(~(size-1));
	sendData[0] = size-1;
	uint8_t csum = 0;
	csum = ~sendData[0];
	sendData[1] = csum;
	write(sysBootloaderFd, sendData, 2);
	usleep(500);
	// get ACK
	i = 0;
	while (read(sysBootloaderFd, &ack, 1) < 1 && i < 200) {
		i++;
		usleep(10000);
	}
	if (i >= 200) return -5;
	if (ack != ACK) {
		printf("no ack %x\n", ack);
		return -6;
	}
	
	bytesRead = read(sysBootloaderFd, data, size);
	printf("mem data read %d, %d\n", size, bytesRead);
	if (bytesRead < size) return -7;
	
	return size;
}

int WriteMemory(uint32_t addr, uint8_t *data, int32_t size) {
	uint8_t ack, dataLen;
	int bytesRead, i;
	uint8_t sendData[1024];
	
	// send read command
	tcflush(sysBootloaderFd, TCIFLUSH);
	//SendByte(0x31);
	//SendByte(0xCE);
	sendData[0] = 0x31;
	sendData[1] = 0xCE;
	write(sysBootloaderFd, sendData, 2);
	usleep(500);
	
	// get ACK
	bytesRead=read(sysBootloaderFd, &ack, 1);
	if (bytesRead < 1) return -1;
	if (ack != ACK) return -2;
	
	// send address
	sendData[3] = addr & 0x000000FF;
	addr >>= 8;
	sendData[2] = addr & 0x000000FF;
	addr >>= 8;
	sendData[1] = addr & 0x000000FF;
	addr >>= 8;
	sendData[0] = addr & 0x000000FF;
	sendData[4] = GetCheckSum(sendData, 4);
	write(sysBootloaderFd, sendData, 5);
	usleep(500);
	
	// get address ACK
	bytesRead=read(sysBootloaderFd, &ack, 1);
	if (bytesRead < 1) return -3;
	if (ack != ACK) return -4;
	
	// send number of bytes to read
	//SendByte(size-1);
	//SendByte(~(size-1));
	sendData[0] = size-1;
	uint8_t csum = 0;
	csum ^= size-1;
	for (i = 0; i < size; i++) {
		//SendByte(data[i]);
		sendData[i+1] = data[i];
		csum ^= data[i];
	}
	sendData[size+1] = csum;
	//SendByte(csum);
	write(sysBootloaderFd, sendData, size+2);
	
	usleep(11000);

	// get ACK
	i = 0;
	while (read(sysBootloaderFd, &ack, 1) < 1 && i < 200) {
		i++;
		usleep(1000);
	}
	if (i >= 200) return -5;
	if (ack != ACK) {
		printf("no ack %x\n", ack);
		return -6;
	}
	
	return 0;
}

int ExtendedEraseMemory(uint16_t *pages, int32_t count) {
	uint8_t ack, dataLen;
	int bytesRead, i;
	uint8_t sendData[1000];
	
	// send read command
	tcflush(sysBootloaderFd, TCIFLUSH);
	sendData[0] = 0x44;
	sendData[1] = 0xBB;
	write(sysBootloaderFd, sendData, 2);
	//SendByte(0x44);
	//SendByte(0xBB);
	usleep(10000);
	
	// get ACK
	bytesRead=read(sysBootloaderFd, &ack, 1);
	if (bytesRead < 1) return -1;
	if (ack != ACK) return -2;
	
	if (count > 0) {
		// send number of pages
		uint16_t N = count - 1;
		sendData[1] = N & 0x00FF;
		N >>= 8;
		sendData[0] = N & 0x00FF;
		//SendByte(sendData[0]);
		//SendByte(sendData[1]);

		uint8_t csum = 0;
		csum ^= sendData[0];
		csum ^= sendData[1];
		sendData[2] = csum;
		write(sysBootloaderFd, sendData, 3);
		usleep(1000);
		// get ACK
		bytesRead=read(sysBootloaderFd, &ack, 1);
		if (bytesRead < 1) return -3;
		if (ack != ACK) return -4;
		
		csum = 0;
		for (i = 0; i < count; i++) {
			uint16_t page = pages[i];
			sendData[1 + 2*i] = page & 0x00FF;
			page >>= 8;
			sendData[0 + 2*i] = page & 0x00FF;
			//SendByte(sendData[0]);
			//SendByte(sendData[1]);
			csum ^= sendData[0 + 2*i];
			csum ^= sendData[1 + 2*i];
		}
		sendData[2*i] = csum;
		write(sysBootloaderFd, sendData, 2 * count + 1);
		usleep(count * 50000);
		//SendByte(csum);
	} else {
		// Send special erase command
			uint16_t code = pages[0];
			sendData[1] = code & 0x00FF;
			code >>= 8;
			sendData[0] = code & 0x00FF;
			//SendByte(sendData[0]);
			//SendByte(sendData[1]);
			uint8_t csum = 0;
			csum ^= sendData[0];
			csum ^= sendData[1];
			sendData[2] = csum;
			//SendByte(csum);
			write(sysBootloaderFd, sendData, 3);
			usleep(50000);
	}

	// get ACK
	i = 0;
	while (read(sysBootloaderFd, &ack, 1) < 1 && i < 200) {
		i++;
		usleep(10000);
	}
	if (i >= 200) return -5;
	if (ack != ACK) {
		printf("no ack %x\n", ack);
		return -6;
	}
	
	return 0;
}

int WriteUnprotect() {
	uint8_t ack, dataLen;
	int bytesRead, i;
	
	// send readout unprotect  command
	tcflush(sysBootloaderFd, TCIFLUSH);
	SendByte(0x73);
	SendByte(0x8C);
	usleep(10000);
	
	// get ACK
	bytesRead=read(sysBootloaderFd, &ack, 1);
	if (bytesRead < 1) return -1;
	if (ack != ACK) return -2;
	
	// wait for ACK
	i = 0;
	while (read(sysBootloaderFd, &ack, 1) < 1 && i < 200) {
		i++;
		usleep(10000);
	}
	if (i >= 200) return -5;
	if (ack != ACK) {
		printf("no ack %x\n", ack);
		return -6;
	}
	
	return 0;
}

int  ReadoutUnprotect() {
	uint8_t ack, dataLen;
	int bytesRead;
	
	// send readout unprotect  command
	tcflush(sysBootloaderFd, TCIFLUSH);
	SendByte(0x92);
	SendByte(0x6D);
	usleep(10000);
	
	// get ACK
	bytesRead=read(sysBootloaderFd, &ack, 1);
	if (bytesRead < 1) return -1;
	if (ack != ACK) return -2;
	
	while(read(sysBootloaderFd, &ack, 1) < 1); // wait for mass erase to finish
	if (ack != ACK) {
		printf("no ack erase failed %x\n", ack);
		return -3;
	}
	
	printf("readout unprotect success, mem erased \n");
	
	return 0;
}

int GoCommand(uint32_t addr) {
	uint8_t ack, dataLen;
	int bytesRead, i;
	uint8_t sendData[10];
	
	// send read command
	tcflush(sysBootloaderFd, TCIFLUSH);
	//SendByte(0x21);
	//SendByte(0xDE);
	sendData[0] = 0x21;
	sendData[1] = 0xDE;
	write(sysBootloaderFd, sendData, 2);
	usleep(10000);
	
	// get ACK
	bytesRead=read(sysBootloaderFd, &ack, 1);
	if (bytesRead < 1) return -1;
	if (ack != ACK) return -2;
	
	// send address
	// send address
	sendData[3] = addr & 0x000000FF;
	addr >>= 8;
	sendData[2] = addr & 0x000000FF;
	addr >>= 8;
	sendData[1] = addr & 0x000000FF;
	addr >>= 8;
	sendData[0] = addr & 0x000000FF;
	sendData[4] = GetCheckSum(sendData, 4);
	write(sysBootloaderFd, sendData, 5);
	usleep(10000);

	// wait for ACK
	i = 0;
	while (read(sysBootloaderFd, &ack, 1) < 1 && i < 200) {
		i++;
		usleep(10000);
	}
	if (i >= 200) return -5;
	if (ack != ACK) {
		printf("no ack %x\n", ack);
		return -6;
	}
	
	return 0;
}

void app_terminate_handler(int signo)
{
  if (signo == SIGINT) {
	ClosePort();
    destroy_flag = 1;
	usleep(100);
	printf("Server stopped\n"); 
  }
}

int Start() {
	uint8_t ack;
	
	SendByte(BOOT_START);

	// wait for ACK
	int i = 0;
	while (read(sysBootloaderFd, &ack, 1) < 1 && i < 200) {
		i++;
		usleep(1000);
	}
	if (i >= 200) {
		printf("start error, ACK timeout\n");
		return -5;
	}
	if (ack != ACK) {
		printf("error starting bootloader no ACK  %d\n", ack);
		return -6;
	}
	printf("started\n");

	return 0;
}

int ProgramFlash(const char* filePath) {
	int i, status;
	
	uint8_t pageData[WRITE_PAGE_SIZE];
	uint8_t readData[WRITE_PAGE_SIZE];
	FILE* f = fopen(filePath, "rb");
	if (!f)
	{
		printf("Unable to open input file!\n");
		return -1;
	}
	
	fseek(f, 0L, SEEK_END);
	int fSize = ftell(f);
	rewind(f);
	
	/*status = WriteUnprotect();
	if (status < 0) {
		printf("Error Write Unprotect %d\n", status);
	} else {
		printf("Write Unprotect success\n");
	}
	usleep (100000);
	
	if (Start() < 0) return -2;*/
	
	/*uint16_t eraseCode = 0xFFFF; // mass erase
	status = ExtendedEraseMemory(&eraseCode, 0);
	if (status != 0) {
		printf("Error erasing EEPROM %d\n", status);
		return -3; 
	} else {
		printf("EEPROM successfully erased\n");
	}*/
	// get page count
	int32_t erasePageCount = fSize / ERASE_PAGE_SIZE;
	if (fSize % ERASE_PAGE_SIZE) erasePageCount++;
	printf("erase page count %d\n", erasePageCount);
	
	// erase first page
	uint16_t pages[256] = {0x0000};
	int n = ExtendedEraseMemory(pages, 1);
	if (n < 0) 
		printf("erase error %d\n", n);
	else
		printf("first page erase succcess %d\n", n);
	
	// erase all other pages
	for (i = 1; i < erasePageCount; i++) {
		pages[i - 1] = i;
	}
	n = ExtendedEraseMemory(pages, erasePageCount - 1);
	if (n >=0) 
		printf("erase success %d\n", n);
	else 
		printf("erase error %d\n", n);
	
	// get page count
	int32_t pageCount = fSize / WRITE_PAGE_SIZE;
	if (fSize % WRITE_PAGE_SIZE) pageCount++;
	printf("page count %d\n", pageCount);
	
	for (i = pageCount - 1; i >= 0; i--) {
		memset(pageData, 0xFF, WRITE_PAGE_SIZE);
		fseek ( f , i * WRITE_PAGE_SIZE, SEEK_SET );
		fread(pageData, 1, WRITE_PAGE_SIZE, f);
		if (ferror(f)) {
			 printf("Error reading input file!\n");
			 return -4;
		}
		status = WriteMemory(EEPROM_START_ADDRESS + i * WRITE_PAGE_SIZE, pageData, WRITE_PAGE_SIZE);
		if (status < 0) {
			printf("Error writting to EEPROM %d\n", status);
			return -5;
		}
		
		// read verify
		status = ReadMemory(EEPROM_START_ADDRESS + (int32_t)i * WRITE_PAGE_SIZE, readData, WRITE_PAGE_SIZE);
		if (status < 0) {
			printf("error reading %d %d\n", i, status);
			fclose(f);
			return -6;
		} else {
			//printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[0], rcvData[1], rcvData[2], rcvData[3], rcvData[4], rcvData[5], rcvData[6], rcvData[7]);
			//printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[8], rcvData[9], rcvData[10], rcvData[11], rcvData[2], rcvData[13], rcvData[14], rcvData[15]);
			//printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[16], rcvData[17], rcvData[18], rcvData[19], rcvData[20], rcvData[21], rcvData[22], rcvData[23]);
			//printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[24], rcvData[25], rcvData[26], rcvData[27], rcvData[28], rcvData[29], rcvData[30], rcvData[31]);
			if (memcmp(pageData, readData, WRITE_PAGE_SIZE) != 0) {
				printf("verify failed %d %d\n", i, status);
				fclose(f);
				return -7;
			}
		}
		printf("Page %d programmed successfully\n", i);
		//usleep(200000);
	}
	
	printf("EEPROM programming finished successfully\n");

	usleep(10000);
	status = GoCommand(EEPROM_START_ADDRESS);
	if (status < 0) {
		printf("Cannot execute code %d\n", status);
		return -8;
	} else {
		printf("Code executed successfully\n");
	}
	
	fclose(f);
	return 0;
}

int ReadFlash() {
	int i, status;
	int pageCount = 10;
	for (i = 0; i < 48; i++) {
		status = ReadMemory(EEPROM_START_ADDRESS + (int32_t)i * 16, rcvData, 16);
		if (status < 0) {
			printf("error reading %d %d\n", i, status);
		} else {
			printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[0], rcvData[1], rcvData[2], rcvData[3], rcvData[4], rcvData[5], rcvData[6], rcvData[7]);
			printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[8], rcvData[9], rcvData[10], rcvData[11], rcvData[2], rcvData[13], rcvData[14], rcvData[15]);
			//printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[16], rcvData[17], rcvData[18], rcvData[19], rcvData[20], rcvData[21], rcvData[22], rcvData[23]);
			//printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[24], rcvData[25], rcvData[26], rcvData[27], rcvData[28], rcvData[29], rcvData[30], rcvData[31]);
		}
	}
}

int main(int argc, char* argv[])
{
	struct pollfd mcuPollFd;
	int n;
	uint8_t adr = 0x15;
	int ret = 0;
	char *inputFile = "PiJuice.elf.binar";
	 
	if (signal(SIGINT, app_terminate_handler) == SIG_ERR)
		printf("\ncan't catch SIGINT\n"); 
	
	if (argc != 3) {
		printf("Usage: %s <address> <input file path>\n", argv[0]);
		printf("Example: %s 14 ./Filename.binary\n", argv[0]);
		//sysBootloaderFd = OpenI2C("/dev/i2c-1");//OpenUart("/dev/ttyAMA0");
		//appI2CFd = OpenAppI2C("/dev/i2c-1", 0x14);
	}
	/*else {
		sysBootloaderFd = OpenI2C(1);//OpenUart(argv[1]);
		appI2CFd = OpenAppI2C(1, argv[0]);
	}*/
	
	if (argc >= 2) {
		adr = (int)strtol(argv[1], NULL, 16);
		printf("using address %x\n", adr);
	} else {
		printf("attempting to default address %x\n", adr);
	}
	
	sysBootloaderFd = OpenI2C("/dev/i2c-1");//OpenUart("/dev/ttyAMA0");
	appI2CFd = OpenAppI2C("/dev/i2c-1", adr);

	if (sysBootloaderFd < 0 || appI2CFd < 0) {
		printf("Error opening I2C ports, aborting\n");
		close(appI2CFd);
		ClosePort();
		ret = -1;
		goto end;
	}

	if (argc >= 3) {
		printf("Input file %s\n", argv[2]);
		inputFile = argv[2];
	} else {
		printf("Using default input file path %s\n", "PiJuice.elf.binary");
	}
	
	//ret = ProgramFlash(inputFile);
	//ReadFlash();
	
	/*n = GetVerReadProtection();
	printf("bootloader version %x\n", blVersion);
	printf("PID %d\n", n);*/
	
	/*uint16_t pages[] = {0x0000, 0x0001, 0x0002};
	n = ExtendedEraseMemory(pages, 3);
	if (n >=0) 
		printf("erase success %d\n", n);
	else 
		printf("erase error %d\n", n);*/
	
	//ReadoutUnprotect();
	
	int i, status;
	
	uint8_t pageData[WRITE_PAGE_SIZE];
	uint8_t readData[WRITE_PAGE_SIZE];
	FILE* f = fopen(inputFile, "rb");
	if (!f)
	{
		printf("Unable to open input file!\n");
		ret = -2;
		goto end;
	}
	
	fseek(f, 0L, SEEK_END);
	int fSize = ftell(f);
	rewind(f);
	
	//Start();
	printf("Starting bootloader \n");
	uint8_t startCmd[] = {0xFE, 0x01, 0xFE}; 
	write(appI2CFd, startCmd, 3);
	usleep(10000);
	// try old firmware
	printf("Starting bootloader old firmware\n");
	write(appI2CFd, startCmd, 2);
	usleep(10000);
	
	printf("Sending get command \n");
	SendGetCmd();
	usleep(10000);
	n = Receive(rcvData);
	if (n >=0) {
		printf("data received %d\n", n);
		printf("bootloader version: %x\n", rcvData[0]);
		printf("commands: %x, %x, %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[1], rcvData[2], rcvData[3], rcvData[4], rcvData[5], rcvData[6], rcvData[7], rcvData[8], rcvData[9], rcvData[10]);
	}
	else {
		printf("error receiving data %d\n", n);
		ret = -3;
		goto end;
	}
	
	/*status = WriteUnprotect();
	if (status < 0) {
		printf("Error Write Unprotect %d\n", status);
	} else {
		printf("Write Unprotect success\n");
	}
	usleep (100000);
	
	if (Start() < 0) return -2;*/
	
	/*uint16_t eraseCode = 0xFFFF; // mass erase
	status = ExtendedEraseMemory(&eraseCode, 0);
	if (status != 0) {
		printf("Error erasing EEPROM %d\n", status);
		return -3; 
	} else {
		printf("EEPROM successfully erased\n");
	}*/
	
	// get page count
	int32_t erasePageCount = fSize / ERASE_PAGE_SIZE;
	if (fSize % ERASE_PAGE_SIZE) erasePageCount++;
	printf("erase page count %d\n", erasePageCount);
	
	// erase first page
	uint16_t pages[256] = {0x0000};
    n = ExtendedEraseMemory(pages, 1);
	if (n < 0) {
		printf("erase error %d\n", n);
		ret = -4;
		goto end;
	} else
		printf("first page erase succcess %d\n", n);
	
	// erase all other pages
	for (i = 1; i < erasePageCount; i++) {
		pages[i - 1] = i;
	}
	n = ExtendedEraseMemory(pages, erasePageCount - 1);
	if (n >=0)
		printf("erase success %d\n", n);
	else {
		printf("erase error %d\n", n);
		ret = -5;
		goto end;
	}
	
	// get page count
	int32_t pageCount = fSize / WRITE_PAGE_SIZE;
	if (fSize % WRITE_PAGE_SIZE) pageCount++;
	printf("page count %d\n", pageCount);
	
	for (i = pageCount - 1; i >= 0; i--) {
		memset(pageData, 0xFF, WRITE_PAGE_SIZE);
		fseek ( f , i * WRITE_PAGE_SIZE, SEEK_SET );
		fread(pageData, 1, WRITE_PAGE_SIZE, f);
		if (ferror(f)) {
			 printf("Error reading input file!\n");
			 ret = -6;
			 goto end;
		}
		status = WriteMemory(EEPROM_START_ADDRESS + i * WRITE_PAGE_SIZE, pageData, WRITE_PAGE_SIZE);
		if (status < 0) {
			printf("Error writting to EEPROM %d\n", status);
			ret = -7;
			goto end;
		}
		
		// read verify
		status = ReadMemory(EEPROM_START_ADDRESS + (int32_t)i * WRITE_PAGE_SIZE, readData, WRITE_PAGE_SIZE);
		if (status < 0) {
			printf("error reading %d %d\n", i, status);
			fclose(f);
			ret = -8;
			goto end;
		} else {
			//printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[0], rcvData[1], rcvData[2], rcvData[3], rcvData[4], rcvData[5], rcvData[6], rcvData[7]);
			//printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[8], rcvData[9], rcvData[10], rcvData[11], rcvData[2], rcvData[13], rcvData[14], rcvData[15]);
			//printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[16], rcvData[17], rcvData[18], rcvData[19], rcvData[20], rcvData[21], rcvData[22], rcvData[23]);
			//printf("commands: %x, %x, %x, %x, %x, %x, %x, %x\n", rcvData[24], rcvData[25], rcvData[26], rcvData[27], rcvData[28], rcvData[29], rcvData[30], rcvData[31]);
			if (memcmp(pageData, readData, WRITE_PAGE_SIZE) != 0) {
				printf("verify failed %d %d\n", i, status);
				fclose(f);
				ret = -9;
				goto end;
			}
		}
		printf("Page %d programmed successfully\n", i);
		//usleep(200000);
	}
	
	printf("EEPROM programming finished successfully\n");

	usleep(10000);
	status = GoCommand(EEPROM_START_ADDRESS);
	if (status < 0) {
		printf("Cannot execute code %d\n", status);
		ret = -10;
		goto end;
	} else {
		printf("Code executed successfully\n");
	}
	
end:

	if (f) fclose(f);
	close(sysBootloaderFd);
	close(appI2CFd);
	
	return ret;
}
