# main.py for the security system web app
import datetime
import time
# import Rasp Pi GPIO lib
import RPi.GPIO as GPIO
# Import required flask lib functions
from flask import Flask, render_template, url_for, Response
# import library functions for concurrent tasks
import threading
# import lib for NRF24L01 support library
from lib_nrf24 import NRF24
# import lib for interfacing with SPI devices
import spidev

import helper_classes

app = Flask(__name__)

# start radio object for nRF24L01+ comms - see RaspRadio class in helper_classes.py
PiRadio = helper_classes.RaspRadio()
MasterData = helper_classes.NodeData()


@app.route('/')
@app.route('/home')
def display_security_page():

    # create a timecheck for confirmation of up-to-date check
    timeCheck = datetime.datetime.now()
    timeString = timeCheck.strftime("%Y-%m-%d %H:%M")
    detection = False

    for num in range(1,7):
        node = "node_" + str(num)
        pir_state = getattr(MasterData, node)['pir_motion'] 
        doppler_state = getattr(MasterData, node)['doppler_motion']
        if pir_state == 11 or doppler_state == 11:
            detection = True
            print("A detection has been made - ALERT!!!")

    # dictionary of variables for the jinja HTML template
    inputData = {
            'time' : timeString,
            'detection' : detection,
            'node_data' : {
                            'node_1_pir' : MasterData.node_1['pir_motion'],
                            'node_1_doppler' : MasterData.node_1['doppler_motion']
                            }
                }

    return render_template('index.html', **inputData)


@app.route("/radio_rx")
def radio_rx():
    """ Server-sent event endpoint that obtains data from remote nodes every
        two seconds. The data is sent as Server-Sent Event (SSE) stream that 
        puts the data in a JSON format, that must be parsed by the client.
        It passes the most up-to-date states of the node pir and doppler.
    """
    def read_radio_rx():
        while True:
            ## call each remote slave and obtain sensor states using PiRadio object
            msg_success, receivedMessage = PiRadio.receive_node_data()
            for node, tx_success in enumerate(msg_success):

                # if tx was successful for a given node - update MasterData states
                if tx_success:
                    MasterData.set_pir_motion(node, receivedMessage[node][2])
                    MasterData.set_doppler_motion(node, receivedMessage[node][4])

            # format the sensor state data as JSON - multiple data fields are received as one by client
            yield 'data: {\n'
            yield 'data: "node_1_pir": "{0}",\n'.format(MasterData.node_1['pir_motion'])
            yield 'data: "node_1_doppler": "{0}",\n'.format(MasterData.node_1['doppler_motion'])
            yield 'data: "node_2_pir": "{0}",\n'.format(MasterData.node_2['pir_motion'])
            yield 'data: "node_2_doppler": "{0}",\n'.format(MasterData.node_2['doppler_motion'])
            yield 'data: "node_3_pir": "{0}",\n'.format(MasterData.node_3['pir_motion'])
            yield 'data: "node_3_doppler": "{0}",\n'.format(MasterData.node_3['doppler_motion'])
            yield 'data: "node_4_pir": "{0}",\n'.format(MasterData.node_4['pir_motion'])
            yield 'data: "node_4_doppler": "{0}",\n'.format(MasterData.node_4['doppler_motion'])
            yield 'data: "node_5_pir": "{0}",\n'.format(MasterData.node_5['pir_motion'])
            yield 'data: "node_5_doppler": "{0}",\n'.format(MasterData.node_5['doppler_motion'])
            yield 'data: "node_6_pir": "{0}",\n'.format(MasterData.node_6['pir_motion'])
            yield 'data: "node_6_doppler": "{0}"\n'.format(MasterData.node_6['doppler_motion'])
            # terminate data field stream with two newline chars
            yield 'data: }\n\n'
            time.sleep(2.0)
    return Response(read_radio_rx(), mimetype='text/event-stream')


if __name__ == "__main__":
    # run app on localhost (equivalent to 127.0.0.1) on port 80, allow threading for radio rx
    app.run(host='0.0.0.0', port=80, debug=True, threaded=True)

