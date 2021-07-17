#!/usr/bin/python3

# Author: Milan Neskovic, github.com/mmilann, Pi Supply, 2021

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# Description: Python tcript loads firmware to PiJuice boards
# Write firmware: python3 pijuiceboot.py 14 ./PiJuice-V1.5_2021_02_06.elf.binary
# No start option: python3 pijuiceboot.py 14 ./PiJuice-V1.5_2021_02_06.elf.binary --no_start
# Bulk erase option: python3 pijuiceboot.py 14 ./PiJuice-V1.5_2021_02_06.elf.binary --bulk_erase
# Dump config to file: python3 pijuiceboot.py 14  --dump_config ./pjConfig.bin
# Write config file: python3 pijuiceboot.py 14 --load_config ./pjConfig.bin

import time
#import serial
import os, sys, getopt
from smbus import SMBus
from fcntl import ioctl
#import wiringpi

WRITE_PAGE_SIZE = 256
ERASE_PAGE_SIZE = 2048
MAX_ERASE_SECTORS = 40
EEPROM_START_ADDRESS  = 0x08000000
EEPROM_SIZE = 256*1024
CONFIG_START_ADDRESS  = 0x0803BC00

ACK = 0x79
NACK = 0x1F
NO_RESP	= 0x00

adr=0x41
ser = open('/dev/i2c-{0}'.format(1),'r+b',buffering=0)
ioctl(ser.fileno(), 0x0706, adr&0x7F)

if not 'pytest' in sys.modules:  # workaround for https://github.com/pytest-dev/pytest/issues/4843
    sys.stdout.reconfigure(encoding='utf-8', errors='namereplace')
    sys.stderr.reconfigure(encoding='utf-8', errors='namereplace')
    
#ser = serial.Serial(
#	port='/dev/serial0',
#	baudrate = 115200,
#	parity=serial.PARITY_EVEN,
#	stopbits=serial.STOPBITS_ONE,
#	bytesize=serial.EIGHTBITS, 
#	timeout=0.1
#)
counter=0
def StartUart():
    ser.reset_input_buffer()
    cmd = [0x7F]
    ser.write(cmd) 
    time.sleep(0.1) 
    rsp=ser.read(1)
    if rsp:
        print(hex(ord(rsp)))
    else:
        print ("failed to start uart, no ack") 

def Receive():
    #uint8_t ack, dataLen;
    #int bytesRead;
    retryAttempts = 0

    #bytesRead=read(sysBootloaderFd, &ack, 1);
    #if (bytesRead < 1) return -1;
    # get ACK
    i = 0;
    #while ser.in_waiting == 0 and i < 200: #while (ser.read(sysBootloaderFd, &ack, 1) < 1 && i < 200) {
    #    i+=1
    time.sleep(0.01);

    #if (i >= 200): return -1
    ack = ser.read(1)[0]
    #print('ACK:',ack)
    if (ack != ACK):
        print("No ack", ack)
        return -2

    bytesRead = 13#ser.in_waiting
    data = ser.read(13)
    dataLen = data[0];
    #print("bytesRead %d  dataLen", bytesRead, dataLen);
    if (bytesRead < dataLen): return -4

    bytesRead = 1#ser.in_waiting
    ack = ser.read(1)[0]
    #print("Ack:", ack);
    #print("data: %x, %x, %x\n", data[0], data[1], data[12]);
    if (bytesRead < 1): return -5

    if (ack != ACK): return -6

    return data

def GetCheckSum(msg, size):
    result = (0x00)
    for i in range(0, size):
        result = result ^ msg[i];
    return result

def WaitAck(tout):
    try:
        start_time=time.time()
        ack = None
        while (ack==None) and ((time.time()-start_time) < tout):
            ack=ser.read(1)
            #print "wait ack", (time.time()-start_time)
        if ack:
            return ord(ack)
        else:
            return NO_RESP
    except:
        return NO_RESP
        
def SendAddress(addr):
    sendData = [0x00]*5
    sendData[3] = addr & 0x000000FF;
    addr >>= 8;
    sendData[2] = addr & 0x000000FF;
    addr >>= 8;
    sendData[1] = addr & 0x000000FF;
    addr >>= 8;
    sendData[0] = addr & 0x000000FF;
    sendData[4] = GetCheckSum(sendData, 4);
    ser.write(bytearray(sendData[0:5]));
    time.sleep(0.01)
    
def GoCommand(addr):
    #ser.reset_input_buffer()
    ser.write(bytearray([0x21, 0xDE]))
    time.sleep(0.01)
    # get ACK
    ackRsp = WaitAck(0.2)
    if ackRsp != ACK:
        print("go cmd start failed, no ack", hex(ackRsp))
        return "ERROR ACK=" + hex(ackRsp)

    # send address
    SendAddress(addr)

    # get ACK
    ackRsp = WaitAck(2)
    if ackRsp != ACK:
        print("go cmd address failed, no ack", hex(ackRsp))
        return "ERROR ACK=" + hex(ackRsp)
    else:
        return "OK"

def ExtendedEraseMemory(pages, count):
    sendData = [0x00]*256

    # send read command
    #ser.reset_input_buffer()
    ser.write(bytearray([0x44, 0xBB]))
    # get ACK
    ackRsp = WaitAck(0.2)
    if ackRsp != ACK:
        print("Erase start no ack", hex(ackRsp))
        return -2

    if (count > 0):
        #print("Erase of pages started")
        # send number of pages
        N = count - 1;
        sendData[1] = N & 0x00FF;
        N >>= 8;
        sendData[0] = N & 0x00FF;

        csum = 0x00
        csum = csum ^ sendData[0]
        csum = csum ^ sendData[1]
        sendData[2] = csum;
        time.sleep(0.1)
        ser.write(bytearray(sendData[0:3]))
        #print(sendData[1:3])
        #bus.write_block_data(adr, sendData[0], sendData[1:3])
        time.sleep(0.1)
        # get ACK
        ackRsp = WaitAck(0.2)
        if ackRsp != ACK:
            #if (bytesRead < 1) return -3;
            #if (ack != ACK) return -4;
            print("Erase start no ack", hex(ackRsp))
            return -4#"ERROR ACK=" + hex(ackRsp)

        csum = 0x00
        for i in range(0, count):#(i = 0; i < count; i++) {
            page = pages[i];
            sendData[1 + 2*i] = page & 0x00FF
            page = page >> 8
            sendData[0 + 2*i] = page & 0x00FF
            csum = csum ^ sendData[0 + 2*i]
            csum = csum ^ sendData[1 + 2*i]

        sendData[2*count] = csum
        time.sleep(0.1)
        ser.write(bytearray(sendData[0:2 * count + 1]))
        time.sleep(count * 0.05)
    else:
        #Send special erase command
        #print("Special erase started")
        code = pages[0];
        sendData[1] = code & 0x00FF;
        code >>= 8;
        sendData[0] = code & 0x00FF;
        csum = 0x00;
        csum = csum ^ sendData[0];
        csum = csum ^ sendData[1];
        sendData[2] = csum;
        ser.write(bytearray(sendData[0:3]))
        time.sleep(0.5)

    ackRsp = WaitAck(2)
    if ackRsp == ACK:
        return "OK";
    else:
        return -6

def GetCommandsI2C():
    #ser.reset_input_buffer()
    cmd = 0xFF
    try:
        ser.write(bytearray([0x00, 0xFF]))#bus.write_byte_data(adr, 0x00, cmd)
    except:
        print("Error, cannot communicate, embedded bootloader may not be started properly!")
        return -1
    #rsp=ser.read(1) #ser.read(1)
    time.sleep(0.01)
    data = Receive()
    if len(data):
        print('Commands:',[((data[i])) for i in range(0,len(data))])
    return 'OK'

def GetCommands():
    ser.reset_input_buffer()
    cmd = [0x00, 0xFF]
    ser.write(bytearray(cmd)) 
    rsp=ser.read(1)
    time.sleep(0.01)
    print(hex(ord(rsp)))
    rsp=ser.read(20)
    time.sleep(0.01)
    if len(rsp):
        print([hex(ord(rsp[i])) for i in range(0,len(rsp))]) #print hex(ord(rsp))

def WriteUnprotect():
    # send readout unprotect  command
    #ser.reset_input_buffer()
    ser.write(bytearray([chr(0x73), chr(0x8C)]))
    time.sleep(0.02)#usleep(10000);

    # get ACK
    ack=ser.read(1)
    if (not ack or ord(ack) != ACK):
        print("0x73 WriteUnprotect failed, no ack", hex(ord(ack)) if ack else 0)
        return "ERROR"

    return 0

def WritePage(addr, data, size):
    # send read command
    #ser.reset_input_buffer()
    ser.write(bytearray([0x31, 0xCE]))
    time.sleep(0.0005)
    ackRsp = WaitAck(0.5)
    if ackRsp != ACK:
        print("0x31 write cmd failed", hex(ackRsp))
        return -2
        
    # send address
    SendAddress(addr)

    ackRsp = WaitAck(0.2)
    if ackRsp != ACK:
        print("Send address cmd failed", hex(ackRsp))
        return -4

    sendData = [0x00]*(size+2)
    sendData[0] = size-1;
    csum = (0x00)
    csum = csum ^ (size-1)
    for i in range(0,size): #(i = 0; i < size; i++):
        sendData[i+1] = data[i];
        csum = csum ^ data[i];
        #print i, len(data), sendData[i+1], csum
    sendData[size+1] = csum&0xFF;
    #print('Write',sendData[size+1],size+2)
    ser.write(bytearray(sendData[0:int(size+2)]));
    #time.sleep(0.001)

    ackRsp = WaitAck(0.5)
    if ackRsp != ACK:
        print("Write failed", hex(ackRsp))
        return -6
        # get address ACK
    #else:
    #	print "page", j, "succ"

    return "OK"

def ReadPage(addr, size):
    #send read command
    ser.write(bytearray([0x11, 0xEE]))
    time.sleep(0.01)
    ackRsp = WaitAck(0.2)
    if ackRsp != ACK:
        print("0x11 write cmd failed", hex(ackRsp))
        return {'error':-5}

    #send address
    SendAddress(addr)

    # get address ACK
    ackRsp = WaitAck(0.2)
    if ackRsp != ACK:
        print("Send address cmd failed", hex(ackRsp))
        return {'error':-4}
    #time.sleep(0.001)

    # send number of bytes to read
    sendData = [0x00]*(size+2)
    sendData[0] = size-1
    csum = 0x00
    csum = ~ sendData[0]
    sendData[1] = csum&0xFF;
    #get ACK
    ser.write(bytearray(sendData[0:2]))
    time.sleep(0.0005)

    ackRsp = WaitAck(0.5)
    if ackRsp != ACK:
        print("Send number of bytes to read ACK failed", hex(ackRsp))
        return {'error':-6}
    try:
        rsp=ser.read(size) #bytesRead = read(sysBootloaderFd, data, size);
        #print("mem data read", len(rsp))
    except:
        return {'error':-7}
        #if (bytesRead < size): return {'error':-7}

    return {'data':rsp, 'error':'OK'}

def ReadBinary(address, dumpFile = None):
    adr = address if address >= EEPROM_START_ADDRESS else EEPROM_START_ADDRESS
    startAdr = adr
    d = [0xff]*((EEPROM_START_ADDRESS+EEPROM_SIZE) - adr)
    while adr < (EEPROM_START_ADDRESS+EEPROM_SIZE):#for i in range(pageCount-1, -1, -1):#range(0, pageCount): #
        time.sleep(0.001)

        status = ReadPage(adr, WRITE_PAGE_SIZE);
        if (status['error'] != 'OK'):
            print("Error reading binary:", adr, status['error'])
            return {'error':'READ_BINARY,'+status['error']}
        else:
            #print('Page read:',len(status['data']), status['data'])
            d[adr-startAdr:adr-startAdr+WRITE_PAGE_SIZE] = status['data']
        adr += WRITE_PAGE_SIZE
            
    end = 0
    for i in range(len(d)-1, -1, -1):
        if d[i] != 0xff:
            end = i+1
            break
    d= d[0:end]
    
    if dumpFile != None:
        try:
            with open(dumpFile, "wb") as cfgFile:
                cfgFile.write(bytearray(d))
        except:
            print('Configuration file dump error, check if path is valid', sys.argv[fi])
            return {'error':'DUMP_FILE_WRITE'}
    
    return {'data':d, 'error':'OK'}

def WriteBinary(data, address):
    pageCount = len(data) // WRITE_PAGE_SIZE
    if (len(data) % WRITE_PAGE_SIZE):
        pageCount = pageCount + 1
    print('pageCount',pageCount)
    adr = address if address >= EEPROM_START_ADDRESS else EEPROM_START_ADDRESS
    for i in range(pageCount-1, -1, -1):#range(0, pageCount): #
        #WriteUnprotect()
        time.sleep(0.001)
        d = data[i * WRITE_PAGE_SIZE:(i+1) * WRITE_PAGE_SIZE]
        #print('writing page: ',i, len(d))
        err = WritePage(adr + i * WRITE_PAGE_SIZE, d, len(d))
        if err != "OK":
            print("Page:", i, "write failed")
            return -7
        #time.sleep(0.01)
        # read verify
        status = ReadPage(adr + i * WRITE_PAGE_SIZE, len(d));
        if (status['error'] != 'OK'):
            print("Error reading page:", i, status['error'])
            return -8
        else:
            #print('Page read:',len(status['data']), status['data'])
            if bytearray(d) != status['data']:
                print("Page:", i, "read verify failed");
                return -9
            else:
                #print('mem data read success')
                print("Page:", i, "write success")
    return "OK"

def StartBootloaderGPIO():
    os.system("gpio -g mode 10 out") #wiringpi.pinMode(10, 1)# gpio -g mode 10 out
    os.system("gpio -g write 10 1") #wiringpi.digitalWrite(10, 1) # activate BOOT pin
    time.sleep(0.01)
    os.system("gpio -g mode 6 out") #wiringpi.pinMode(6, 1) # gpio -g mode 6 out
    os.system("gpio -g write 6 0") #wiringpi.digitalWrite(6, 0) # activate reset
    time.sleep(0.2)
    os.system("gpio -g write 6 1") #wiringpi.digitalWrite(6, 1) # return from reset
    time.sleep(0.1)
    os.system("gpio -g write 10 0") #wiringpi.digitalWrite(10, 0) # deactivate BOOT pin

def StartBootloaderI2C(addr):
    print("Starting bootloader \n");
    try:
        bus=SMBus(1)
        #startCmd = [0xFE, 0x01, 0xFE] 
        bus.write_i2c_block_data(addr, 0xFE, [0x01, 0xFE])
        time.sleep(0.01)  #usleep(10000)
        print("Starting bootloader old firmware\n");
        bus.write_i2c_block_data(addr, 0xFE, [0x01])
        time.sleep(0.01)
    except:
        return

#print ((sys.argv))
#data = open(sys.argv[2]).read() #map(lambda c: ord(c), open("PiJuice-V1.5_2021_02_06.elf.binary").read()) # TO-DO recognise file path by pijuice firmware name format
cfgFp = None
loadConfig = None
if '--dump_config' in sys.argv:
    fi = sys.argv.index('--dump_config')+1
    if len(sys.argv) > fi:
        cfgFp = sys.argv[fi]
        if fi < 4:
            # Start embedded bootloader
            if not ('--no_start' in sys.argv):
                StartBootloaderI2C(int(sys.argv[1], 16))

            status = ReadBinary(CONFIG_START_ADDRESS, cfgFp)
            if status['error'] == "OK":
                print("Configuration read success");
            else:
                print("Configuration read error", status['error']);
                exit(status['error'])

            err = GoCommand(EEPROM_START_ADDRESS)
            if err != "OK":
                print("Cannot execute code")
            else:
                print("Code executed successfully")
            exit(err)
        
elif '--load_config' in sys.argv:
    fi = sys.argv.index('--load_config')+1
    if len(sys.argv) > fi:
        loadCfgFile = sys.argv[fi]
        cfgFile = open(sys.argv[fi], "rb")
        loadConfig = cfgFile.read()
        cfgFile.close()
        if fi < 4:
            # Start embedded bootloader
            if not ('--no_start' in sys.argv):
                StartBootloaderI2C(int(sys.argv[1], 16))

            erasePageCount = len(loadConfig) // ERASE_PAGE_SIZE;
            if (len(loadConfig) % ERASE_PAGE_SIZE):
                erasePageCount = erasePageCount + 1
            sectorOffset = (CONFIG_START_ADDRESS-EEPROM_START_ADDRESS) // ERASE_PAGE_SIZE
            print("Erase page count", erasePageCount, sectorOffset)
            pages = [0x0000]*256
            for i in range(0, erasePageCount):
                pages[i] = sectorOffset + i
            err = ExtendedEraseMemory(pages, erasePageCount);
            if err == "OK":
                print("Erase success");
            else:
                print("Erase error", err);
                exit(err) #ret = -5;
            time.sleep(0.2)
            err = WriteBinary(loadConfig, CONFIG_START_ADDRESS)
            if err != "OK":
                print("Write failed exiting..", err)
                exit(err)
            else:
                print("Firmware load success")
                
            err = GoCommand(EEPROM_START_ADDRESS)
            if err != "OK":
                print("Cannot execute code")
            else:
                print("Code executed successfully")
            exit(err)
            
file = open(sys.argv[2], "rb")
data = file.read()
file.close()
print("File size:", len(data))

# Start embedded bootloader
if not ('--no_start' in sys.argv):
    StartBootloaderI2C(int(sys.argv[1], 16))

if GetCommandsI2C() != 'OK':
    exit(-1)
#WriteUnprotect()

# erase first sector
pages = [0x0000]
err = ExtendedEraseMemory(pages, len(pages))
if err != "OK":
    print("Erase failed", err)
    exit(-4) #ret = -4;
else:
    print("First page erase succcess")

# erase all other pages
if '--bulk_erase' in sys.argv:     
    status = ReadBinary(CONFIG_START_ADDRESS, cfgFp)
    if status['error'] == "OK":
        print("Configuration read success");
    else:
        print("Configuration read error", status['error'] );
        exit(status['error'])

    # mass erase command
    pages = [0xFFFF]
    err = ExtendedEraseMemory(pages, 0)
    if err == "OK":
        print("Bulk erase success");
    else:
        print("Bulk erase error", err);
        exit(err) #ret = -5;
        
    #print('config', status['data'])
    err = WriteBinary(status['data'], CONFIG_START_ADDRESS)
    if err != "OK":
        print("Configuration write failed exiting..", err)
        exit(err)
    else:
        print("Configuration load success", len(status['data']))
        
    err = WriteBinary(data, EEPROM_START_ADDRESS)
    if err != "OK":
        print("Write failed exiting..", err)
        exit(err)
    else:
        print("Firmware load success")
        
else:
    # get page count
    erasePageCount = len(data) // ERASE_PAGE_SIZE;
    if (len(data) % ERASE_PAGE_SIZE):
        erasePageCount = erasePageCount + 1
    print("Erase page count", erasePageCount)
    pages = [0x0000]*erasePageCount
    for i in range(1, erasePageCount):
        pages[i-1] = i

    erasePageCount -= 1 # first sector alredy erased
    erased = 0
    while erased<erasePageCount:
        p = pages[erased:erased+MAX_ERASE_SECTORS]
        print('erasing',erased,len(p))
        err = ExtendedEraseMemory(p, len(p));
        if err != "OK":
            print("Erase error", err);
            exit(err) #ret = -5;
        erased += MAX_ERASE_SECTORS
    print("Erase success")

    err = WriteBinary(data, EEPROM_START_ADDRESS)
    if err != "OK":
        print("Write failed exiting..", err)
        exit(err)
    else:
        print("Firmware load success", err)

err = GoCommand(EEPROM_START_ADDRESS)
if err != "OK":
    print("Cannot execute code")
else:
    print("Code executed successfully")
#print("EEPROM programming finished successfully");
