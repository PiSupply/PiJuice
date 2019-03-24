#!/usr/bin/env python3
#
# Naturebytes Wildlife Cam Kit | V1.07a (Scratch)
# Customised version for PiJuice
# Based on the excellent official Raspberry Pi tutorials and a little extra from Naturebytes and Francesco Vannini
#
# Usage: python3 naturejuice.py [<options>] [--]
# ======================================================================

from picamera import PiCamera
from time import sleep
from subprocess import call
from datetime import datetime
import logging
import getopt
import sys

import io
from PIL import Image
from PIL import ImageFont
from PIL import ImageDraw
from pijuice import PiJuice
import subprocess

# Logging all of the camera's activity to the "naturebytes_camera.log" file. If you want to watch what your camera
# is doing step by step you can open a Terminal window and type "cd /NatureJuice" and then type
# "tail -f naturebytes_camera.log" - leave this Terminal window open and you can view the logs live
# Alternatively, add -v to the launch to use verbosity mode

logging.basicConfig(format="%(asctime)s %(message)s",filename="/var/log/naturejuice_camera.log",level=logging.INFO)
logging.info("Naturebytes Wildlife Cam Kit running off PiJuice power started up successfully")

def main(argv):
    # Set default save location
    #save_location = "/media/usb0/"
    save_location = "/var/lib/nature-juice/"
    # Command line defaults
    verbose = False
    logo = False
    forceTrigger = False
    # Create a pijuice and a camera objects
    pijuice = PiJuice(1, 0x14)
    camera = PiCamera()
    # Time for watchdog
    detectTime = datetime.now()
    maxTimeIdle = 10  # minutes

    # Set save location
    try:
        opts, args = getopt.getopt(argv, "ho:lvf")
        for opt, arg in opts:
            if opt == "-h":
                printHelp()
                sys.exit()
            elif opt == "-l":
                logo = True
            elif opt == "-o": # Basic error checking
                save_location = arg.strip()
                save_location = save_location if save_location.startswith("/") else "/" + save_location
                save_location = save_location if save_location.endswith("/") else save_location + "/"
            elif opt == "-v":
                verbose = True
            elif opt == "-f":
                forceTrigger = True
    except getopt.GetoptError:
        printHelp()
        sys.exit(2)

    logging.info("Starting a new session. Charge level: " + str(pijuice.status.GetChargeLevel()["data"]) + "%")

    try:
        while datetime.now().minute-detectTime.minute <= maxTimeIdle:
            pijuiceIo2 = pijuice.status.GetIoDigitalInput(2)
            if (pijuiceIo2['error'] == 'NO_ERROR' and pijuiceIo2['data'] == 1) or forceTrigger:
                if forceTrigger: forceTrigger = not forceTrigger # Toggle forceTrigger flag until next reboot

                detectTime = datetime.now() # Get current time
                get_date = detectTime.strftime("%Y-%m-%d") # Get and format the date
                get_time = detectTime.strftime("%H-%M-%S.%f") # Get and format the time

                # Recording that a PIR trigger was detected and logging the battery level at this time
                message = "PIR trigger detected"
                logging.info(message)
                printVerbose(verbose, message)

                # Assigning a variable so we can create a photo JPG file that contains the date and time as its name
                photo_name = save_location +  get_date + '_' + get_time + '.jpg'

                        # Log that we have just taking a photo
                message = "About to take a photo and save to " + photo_name
                logging.info(message)
                printVerbose(verbose, message)

                if(logo):
                    # Prepare the base picture
                    stream = io.BytesIO()
                    # Prepare the font
                    fontsize = 16
                    padding = 4
                    # Check under /usr/share/fonts/truetype/freefont/ for the available fonts
                    font = ImageFont.truetype("FreeSerif.ttf", fontsize)

                    camera.capture(stream, format='jpeg')
                    stream.seek(0)
                    base = Image.open(stream)
                    draw = ImageDraw.Draw(base)
                    font = ImageFont.truetype("FreeSerif.ttf", fontsize)
                    # Prepare date and time ina different format for the picture
                    overlayText = get_date + " " + detectTime.strftime("%H:%M:%S")
                    printVerbose(verbose, "Date and time used for the overlay " + overlayText)
                    baseWidth, baseHeight = base.size
                    printVerbose(verbose, "Width and height of the picture saved " + str(baseWidth) + "X" + str(baseHeight))
                    draw.text((padding, baseHeight-fontsize-padding), overlayText, (255, 255, 255), font=font)
                    # Load the logo watermarks
                    watermark = Image.open("nature-juice_logo_90.png", 'r')
                    # Add watermark
                    xy_offset = (1, 1)
                    base.paste(watermark, xy_offset, mask=watermark)
                    # Save the picture with the logo
                    message = "Added the overlay logo and text"
                    logging.info(message)
                    printVerbose(verbose, message)
                    base.save(photo_name)
                    watermark.close()
                else:
                    # Not using PIL saves the picture with all details untouched
                    # Check the properties of both pictures with/without logo to see the differences
                    camera.capture(photo_name, use_video_port=False, format='jpeg', thumbnail=None)
                message = "Picture saved successfully"
                logging.info(message)
                printVerbose(verbose, message)

            sleep(1)
        message = "Shutting down due to inactivity to conserve battery. Charge level: " + str(pijuice.status.GetChargeLevel()["data"]) + "%"
        logging.info(message)
        printVerbose(verbose, message)
        subprocess.Popen(['sudo', 'shutdown', '-h', 'now']
)

    except KeyboardInterrupt:
        print("KeyboardInterrupt detected. Exiting program")
        sys.exit()
    except:
        print("Error detected. Exiting program")
        sys.exit(2)

def printHelp():
    options = """
            usage: sudo python nbcamera.py [<options>] [--]
            Options:
            \t -l                  \t Overlay NatureJuice logo onto captured image
            \t -o <outputlocation> \t specify output file location
            \t -v                  \t be verbose - print log in terminal
            \t -f                  \t force a trigger when launching the script
            """
    print(options)

def printVerbose(verbose, msg):
    if(verbose): print("Log: " + msg)

if __name__ == "__main__":
    main(sys.argv[1:])
