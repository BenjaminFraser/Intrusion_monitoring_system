# main.py for the security system web app
import datetime
import time
# import Rasp Pi GPIO lib
import RPi.GPIO as GPIO
# Import required flask lib functions
from flask import Flask, render_template, url_for
# import library functions for concurrent tasks
from multiprocessing import Process, Value
# import lib for NRF24L01 support library
from lib_nrf24 import NRF24
# import lib for interfacing with SPI devices
import spidev

# set up GPIO so it knows what pins we are referencing
GPIO.setmode(GPIO.BCM)

# Set up remote node addresses (Ascii POSTA, POSTB, POSTC)
pipes = [[0x41, 0x54, 0x53, 0x4f, 0x50],
         [0x42, 0x54, 0x53, 0x4f, 0x50],
         [0x43, 0x54, 0x53, 0x4f, 0x50]]

# set up radio object from NRF24 lib
radio = NRF24(GPIO, spidev.SpiDev())

# begin radio on CE 0 (GPIO 8) and GPIO 17 as CE value
radio.begin(0, 17)

# setup radio message size, channel, data-rate and power level settings
radio.setChannel(0x76)
radio.setDataRate(NRF24.BR_250KBPS)
radio.setPALevel(NRF24.PA_LOW)

# setup auto-acknowledgement for messages and dynamic payloads
radio.enableAckPayload()
radio.enableDynamicPayloads()
radio.setRetries(4, 10)

# log radio details for debugging and validation of radio
radio.printDetails()

# global variable flag for resetting remote nodes
RESET = False

app = Flask(__name__)

class Detector:
    """ Creates a new sensor node that detects the presence of both
        Passive Infrared (PIR) and X-Band Radar Doppler motion.
    Attributes:
        node_id: An integer designating the unique id of the node
        pir_motion: An integer to represent the current detection
                    status. 11 = detection, 22 = all clear.
        doppler_motion: An integer to represent status of doppler
                        motion. 11 = detection, 22 = all clear.
    """
    def __init__(self, node_id):
        self.node_id = node_id
        self.pir_motion = -1
        self.doppler_motion = -1

    def set_pir_motion(self, motion_state):
        self.pir_motion = int(motion_state)

    def set_doppler_motion(self, motion_state):
        self.doppler_motion = int(motion_state)

node_1, node_2, node_3 = Detector(1), Detector(2), Detector(3)

@app.route('/')
@app.route('/home')
def display_security_page():
    # carry out process of changing which processing script should be 
    # run, eg. set a variable to script_1, script_2... depending on 
    # status of incoming radio signal received

    # create a timecheck for confirmation of up-to-date check
    timeCheck = datetime.datetime.now()
    timeString = timeCheck.strftime("%Y-%m-%d %H:%M")

    print("The current status of node 1 is - Doppler motion: {0}, IR motion: {1}".format(node_1.pir_motion, node_1.doppler_motion))

    if node_1.pir_motion == 22:
        detection = True
        print("A detection has been made - ALERT!!!")
    else:
        detection = False
        print("System detected as SAFE!!!")

    # dictionary of variables for the jinja HTML template
    inputData = {
            'time' : timeString,
            'detection' : detection
                }

    return render_template('index.html', **inputData)


def receive_radio():
    """ Continuously check status of incoming radio transmissions
        if a priority message alert comes in - do something to change
        the loaded processing script in display_security_page.
    """
    # while the program runs, continuously check for incoming msg and receive
    while True:
        start = time.time()
        print("Started loop - beginning data retreival...")

        if RESET:
            msg_success, receivedMessage = receivePostData(True)
        else:
            msg_success, receivedMessage = receivePostData()

        if msg_success:
            print("Successfully gathered sensor data from node...")
            print("Node 1 pir motion status: {0}, doppler motion status: {1}".format(node_1.pir_motion, node_1.doppler_motion))
            print("Node 2 pir motion status: {0}, doppler motion status: {1}".format(node_2.pir_motion, node_2.doppler_motion))
            print("Node 3 pir motion status: {0}, doppler motion status: {1}".format(node_3.pir_motion, node_3.doppler_motion))

        time.sleep(5)
    return 0


def receivePostData(reset=False):
    
    commandData = [1, 11] if reset else [1, 22]

    # array to store data from each node
    receivedMessage = [[1, 0, 0], [2, 0, 0], [3, 0, 0]]

    message_success = False

    for index, address in enumerate(pipes):
        # setup address to write messages to arduino smart-post units
        radio.openWritingPipe(pipes[index])

        print("Sending request to address: {0}".format(address))

        tx_success = radio.write(commandData)
    
        # if tx success - receive and read smart-post ack reply
        if tx_success:
            
            print("The radio message reached node {0}!".format(index + 1))
            
            if radio.isAckPayloadAvailable():

                # read ack payload and update node objects with latest data
                radio.read(receivedMessage[index], radio.getDynamicPayloadSize())

                print("Received a acknowledgement reply!")

                updatePostData(index, receivedMessage[index])

                message_success = True

    return message_success, receivedMessage


def updatePostData(node_number, receivedMessage):
    """ Updates the Detector objects with the received sensor state
        data for the given node.
    """
    if node_number == 0:
        global node_1
        node_1.pir_motion = receivedMessage[2]
        node_1.doppler_motion = receivedMessage[4]
        #node_1.set_pir_motion(receivedMessage[2])
        #node_1.set_doppler_motion(receivedMessage[4])
        return node_1
    
    elif node_number == 1:
        global node_2
        node_2.pir_motion = receivedMessage[2]
        node_2.doppler_motion = receivedMessage[4]
        #node_2.set_pir_motion(receivedMessage[2])
        #node_2.set_doppler_motion(receivedMessage[4])
        return node_2
    
    elif node_number == 2:
        global node_3
        node_3.pir_motion = receivedMessage[2]
        node_3.doppler_motion = receivedMessage[4]
        #node_3.set_pir_motion(receivedMessage[2])
        #node_3.set_doppler_motion(receivedMessage[4])
        return node_3
        
    return 0

if __name__ == "__main__":
    p = Process(target=receive_radio)
    p.start()

    # run app on localhost (equivalent to 127.0.0.1) on port 80
    app.run(host='0.0.0.0', port=80, debug=True, use_reloader=False)

