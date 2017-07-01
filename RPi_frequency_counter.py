#!/usr/bin/python
import RPi.GPIO as GPIO

import time

HB100_INPUT_PIN = 27

GPIO.setmode(GPIO.BCM)
GPIO.setup(HB100_INPUT_PIN, GPIO.IN)

MAX_PULSE_COUNT = 10
MOTION_SENSITIVITY = 10

def count_frequency(GPIO_pin, max_pulse_count=10, ms_timeout=50):
    """ Monitors the desired GPIO input pin and measures the frequency
        of an incoming signal. For this example it measures the output of
        a HB100 X-Band Radar Doppler sensor, where the frequency represents
        the measured Doppler frequency due to detected motion.
    """
    start_time = time.time()
    pulse_count = 0

    # count time it takes for 10 pulses
    for count in range(max_pulse_count):

        # wait for falling pulse edge - or timeout after 50 ms
        edge_detected = GPIO.wait_for_edge(GPIO_pin, GPIO.FALLING, timeout=ms_timeout)

        # if pulse detected - iterate count
        if edge_detected is not None:
            pulse_count += 1

    # work out duration of counting and subsequent frequency (Hz)
    duration = time.time() - start_time 
    frequency = pulse_count / duration   

    return frequency

# loop continuously, measuring Doppler frequency and printing if motion detected
while True:
    doppler_freq = count_frequency(HB100_INPUT_PIN)

    if doppler_freq < MOTION_SENSITIVITY:
        print("No motion was detected")

    else:
        print("Motion was detected, Doppler frequency was: {0}".format(doppler_freq))
