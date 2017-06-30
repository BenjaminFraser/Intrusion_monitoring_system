# import Rasp Pi GPIO lib
import RPi.GPIO as GPIO

# import lib for NRF24L01 support library
from lib_nrf24 import NRF24

# import lib for interfacing with SPI devices
import spidev

import threading

# Set up remote node addresses (Ascii POSTA, POSTB, POSTC)
PIPES = [[0x41, 0x54, 0x53, 0x4f, 0x50],
         [0x42, 0x54, 0x53, 0x4f, 0x50],
         [0x43, 0x54, 0x53, 0x4f, 0x50]]

# set up GPIO so it knows what pins we are referencing
GPIO.setmode(GPIO.BCM)

GPIO.setwarnings(False)

# set up radio object from NRF24 lib
radio = NRF24(GPIO, spidev.SpiDev())

class RaspRadio(object):
    """ Our radio object to communicate with remote slaves using nrf24l01+ transceiver """

    def __init__(self):
        """ Initialise with required radio settings """
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
        self._lock = threading.Lock()

    def send_message(self, node_num_minus_1, send_data):
        """ Sends a radio message over the nRF24L01+ transceiver to the designated
            remote node number with the given send_data message.
        Args:
            node_num (int): The remote node number - 1, as an int from 0 to 5.
            send_data (int array): The data to send, as an array of ints, up to 
                                    a maximum of 32 Bytes.
        """
        # setup address to write messages to arduino smart-post units
        radio.openWritingPipe(PIPES[node_num_minus_1])

        message_success = False
        rx_data = []

        # if tx success - receive and read slave ack reply
        tx_success = radio.write(send_data)
        if tx_success:

            # if ack-payload received - gather message
            if radio.isAckPayloadAvailable():

                # read ack payload and update node objects with latest data
                radio.read(rx_data, radio.getDynamicPayloadSize())

                message_success = True

        return message_success, rx_data

    def receive_node_data(self, reset=False):
        """ Receives updated sensor states from all system nodes. Uses the send_message
            class function for each of the remote nodes.
        Args:
            reset (bool): whether to reset the remote nodes or not (default false)
        """
        commandData = [1, 11] if reset else [1, 22]

        # array to store data from each node: [node_id, pir_state, doppler_state]
        receivedMessage = [[],[],[]]

        msg_success = [[],[],[]]

        for index, address in enumerate(PIPES):

            msg_success[index], receivedMessage[index] = self.send_message(index, commandData)

        return msg_success, receivedMessage



class NodeData:
    """ Creates a master data object that stores the detection states
        of up to six remote PIR and Doppler motion sensing nodes.
    Attributes:
        node_x: a dictionary containing the IR motion and Doppler
                motion status for node 'x', where x is any node 
                id from 1 to 6. The status for each motion is as
                follows: '22' = all-clear, '11' = detection,
                '-1' = no communication made (i.e. turned off)
    """
    def __init__(self):
        self.node_1 = { 'pir_motion' : -1, 'doppler_motion' : -1 }
        self.node_2 = { 'pir_motion' : -1, 'doppler_motion' : -1 }
        self.node_3 = { 'pir_motion' : -1, 'doppler_motion' : -1 }
        self.node_4 = { 'pir_motion' : -1, 'doppler_motion' : -1 }
        self.node_5 = { 'pir_motion' : -1, 'doppler_motion' : -1 }
        self.node_6 = { 'pir_motion' : -1, 'doppler_motion' : -1 }


    def set_pir_motion(self, node_number, motion_state):
        """ Updates the state of the selected nodes pir_motion value
            within the node_x dictionary, where 'x' is the selected node.
        Args: 
            node_number (int): the number of the node, from 1 - 6
            motion_state (int): The detection state, either '11' (alert) or
                                '22' (All-clear)
        Raises:
            ValueError: incorrect node or motion state input.
        """
        if  0 <= node_number < 6 and (motion_state == 11 or motion_state == 22):
            node = "node_" + str(node_number)
            getattr(self, node)['pir_motion'] = int(motion_state)
        else:
            raise ValueError("The node must be a number from 0 - 5, and state must be either '11' or '22'!")

    def set_doppler_motion(self, node_number, motion_state):
        """ Updates the state of the selected nodes doppler_motion value
            within the node_x dictionary, where 'x' is the selected node.
        Args: 
            node_number (int): the number of the node, from 1 - 6 minus 1. So it
                                must be from 0 to 5.
            motion_state (int): The detection state, either '11' (alert) or
                                '22' (All-clear)
        Raises:
            ValueError: incorrect node or motion state input.
        """
        if  0 <= node_number < 6 and (motion_state == 11 or motion_state == 22):
            node = "node_" + str(node_number)
            getattr(self, node)['doppler_motion'] = int(motion_state)
        else:
            raise ValueError("The node must be a number from 0 - 5, and state must be either '11' or '22'!")
