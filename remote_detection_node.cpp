/*************************************************************************
 * Remore node security system program:                                  *
 *      A program to operate a remote node unit that works as part of    *
 *      a larger intrusion monitoring motion detection system            *
 *                                                                       *
 * Usage:                                                                *
 *      This program is designed to use with the Arduino UNO micro-      *
 *      controller, the NRF24L01+ radio transceiver unit, a custom       *  
 *      built HB100 doppler motion detector, and a Passive Infrared      *
 *      (PIR) motion sensor. Each node interfaces with a centrally       *
 *      operated master device unit using radio communications.          *
 *      The master device also uses nRF24L01+ communications, and can    *
 *      be any device you like. For this project, two varients of        *
 *      master device are used - one using Arduino MEGA and one using    *
 *      a Raspberry Pi.                                                  *
 *                                                                       *
 *      Author: Benjamin Fraser                                          *
 *                                                                       *
 *        Last modified: 01/07/2017                                      *
 *                                                                       *
 *************************************************************************/

// Freq Measure lib - uses digital pin 8 of Arduino Uno for measurement
#include <FreqMeasure.h>

// nRF24L01 radio transceiver external libraries
#include <SPI.h>
#include <RF24.h>
#include <nRF24L01.h>
#include <printf.h>

// define node ID - node ID should be 1 less than the node number, i.e. node 1 = 0
#define NODE_ID 1

// SYSTEM SETTING PARAMETERS
#define MOTION_SENSITIVITY 10   // 10 = High, 30 = Medium, 45 = Low
#define IR_HOLD_TIME 50        // the number of loops to hold IR motion high
bool IR_MOTION_ON = true;       // if no PIR motion detection is needed - set to false

// chip select and RF24 radio setup pins
#define CE_PIN 9
#define CSN_PIN 10
RF24 radio(CE_PIN,CSN_PIN);

// PIR sensor pin input - HIGH if motion detected
const int IR_MOTION_PIN = 2; 

// int array to store node_id, PIR_motion status, doppler_motion_status.
// takes the form remoteNodeData[NODE_ID] = {node_id, pirMotionStatus, dopplerMotionStatus}
// status '22' means ALL CLEAR, status '11' means DETECTION or HIGH
int remoteNodeData[3][3] = {{1, 22, 22}, {2, 22, 22}, {3, 22, 22}};

// int array to store incoming master device data: masterData = {systemCount, systemReset}
int masterData[2] = {0};

// setup radio pipe addresses for communication with master device
const byte nodeAddresses[3][5] = { 
                                        {'P','O','S','T','A'},
                                        {'P','O','S','T','B'},
                                        {'P','O','S','T','C'}
                                      };

// global bool - to be changed by the interrupt service routine when IR motion detected
int IRMotionStarted = false;

// global bool flag - alert for detected IR motion
bool IRMotion = false;

// global bool flag - alert for a doppler motion detection
bool dopplerMotionDetected = false;

// global vars for doppler motion sensing
int motionValue = 0;
double total=0;
int counter=0;

// global vars for remembering an alert for a short period
int dopplerMotionDelay = 0;  
int pirMotionDelay = 0;

/* Function: setup
 *    Initialises the system wide configuration and settings prior to start
 */
void setup() {

  // initialise freq measurement on digital pin 8 for doppler motion
  FreqMeasure.begin();

  Serial.begin(9600);

  // initialise Interrupt service routine for detection passive IR motion
  attachInterrupt(digitalPinToInterrupt(IR_MOTION_PIN), pirMotionTriggered, RISING);

  // ----------------------------- RADIO SETUP CONFIGURATION AND SETTINGS -------------------------// 
  
  radio.begin();
  // set power level of the radio
  radio.setPALevel(RF24_PA_LOW);

  // set RF datarate
  radio.setDataRate(RF24_250KBPS);

  // set radio channel to use - ensure it matches the target host
  radio.setChannel(0x76);

  radio.openReadingPipe(1, nodeAddresses[NODE_ID]);         

  // enable ack payload - remote nodes reply with data using this feature
  radio.enableAckPayload();
  radio.writeAckPayload(1, &remoteNodeData[NODE_ID], sizeof(remoteNodeData[NODE_ID]));

  // print radio config details to console
  printf_begin();
  radio.printDetails();

  // start listening on radio
  radio.startListening();
  
  // --------------------------------------------------------------------------------------------//
}


/* Function: loop
 *    main loop program for the master device - repeats continuously during system operation
 */
void loop() {

  // sense current environment conditions for IR motion and doppler motion
  senseAndDelay(250);

  // update current node data using sensed data
  updateNodeData();

  if (IRMotion && dopplerMotionDetected) {
    Serial.println("Motion was definitely detected! Both PIR and doppler were alerted!");
  }

  else if (dopplerMotionDetected) {
    Serial.println("Doppler motion was detected!");
  }

  else if (IRMotion) {
    Serial.println("IR motion was detected!");
  }

  else {
    Serial.println("No motion was detected! SYSTEM SAFE.");
  }
}


/* Function: updateNodeData
 *    updates the system states of the PIR motion and doppler motion variables, and stores
 *    them in the acknowledgement payload ready for transmission
 */
void updateNodeData(void) 
{
  // if PIR mode selected, check state of pir motion
  if (IR_MOTION_ON == true) pirMotionUpdate();

  // check state of doppler motion
  dopplerMotionStatus();

  // set the ack payload ready for next request for data
  radio.writeAckPayload(1, &remoteNodeData[NODE_ID], sizeof(remoteNodeData[NODE_ID]));
}


/* Function: pirMotionUpdate
 *    Updates the IR motion status in remoteNodeData[NODE_ID][1] based on 
 *    the sensed IR motion data.
 */
void pirMotionUpdate(void) {

  
  // if pir motion detected - raise flag and update node data
  if (IRMotionStarted) {
    IRMotion = true;
    remoteNodeData[NODE_ID][1] = 11;

    // reset motion count to keep motion-alert for a delay period
    pirMotionDelay = 0;

    // reset global bool IRMotionStarted for ISR
    IRMotionStarted = false;
  }

  // if motion status HIGH, keep on until delay count reaches IR_HOLD_TIME
  if (IRMotion) {
      if (pirMotionDelay < IR_HOLD_TIME) {
          pirMotionDelay++;
      }
      // reset when count reaches IR_HOLD_TIME
      else {
          remoteNodeData[NODE_ID][1] = 22;
          IRMotion = false;
      }
  }
}


/* Function: dopplerMotionUpdate
 *    Updates the doppler motion status in remoteNodeData[NODE_ID][2] based on 
 *    the sensed radar data.
 */
void dopplerMotionStatus(void) {
  
    // if doppler motion detected - raise flag and update node data
    if (motionValue > MOTION_SENSITIVITY) {
      dopplerMotionDetected = true;
      remoteNodeData[NODE_ID][2] = 11;

      // reset motion count to keep motion-alert for a delay period
      dopplerMotionDelay = 0;
    }

    // if motion status HIGH, keep on until delay count reaches 5
    if (dopplerMotionDetected) {
        if (dopplerMotionDelay < 5) {
            dopplerMotionDelay++;
        }
        // reset when count reaches 5
        else {
            remoteNodeData[NODE_ID][2] = 22;
            dopplerMotionDetected = false;
        }
    }
    // reset motion val before next loop
    motionValue = 0;
}



/* Function: radioCheckAndReply
 *    sends the node data (remoteNodeData) over the nrf24l01+ radio communications
 *    when prompted to by the master device
 */
void radioCheckAndReply(void)
{
    // check for radio message and send sensor data using auto-ack
    if ( radio.available() ) {
          radio.read( &masterData, sizeof(masterData) );
          Serial.println("Received request from master device - sending sensor data.");

          // check for reset signal from master device - if so, reset alert states
          if (masterData[1] == 11) {
            resetNode();
          }

          Serial.print("Sending the following data: pir status - ");
          Serial.print(remoteNodeData[NODE_ID][1]);
          Serial.print(" , doppler status - ");
          Serial.println(remoteNodeData[NODE_ID][2]);
    }
}


/* Function: resetNode
 *    Performs a reset of all node sensor values and detection states
 */
void resetNode(void) {
    remoteNodeData[NODE_ID][1] = 22;
    remoteNodeData[NODE_ID][2] = 22;
    masterData[1] = 22;
    motionValue = 0;
    IRMotion = false;

    // update the acknowledgement payload so alarm is not instantly retriggered
    radio.writeAckPayload(1, &remoteNodeData[NODE_ID], sizeof(remoteNodeData[NODE_ID]));
}

/* Function: senseAndDelay
 *    Custom delay to allow concurrent activities during program delays. Performs a continuous
 *    update of sensed IR motion, doppler motion and radio request checks for the delay period
 */
void senseAndDelay(unsigned long duration)
{
    unsigned long start = millis();
    
    // loop for the required time without the need for delay()
    while((millis() - start < duration)) {

        // read doppler sensor data and update global motionValue
        int dopplerReturn = readDoppler();
        if (motionValue < dopplerReturn) motionValue = dopplerReturn; 

        // transmit current operational conditions to master device if required
        radioCheckAndReply();
    }
}


/* Function: readDoppler
 *    obtains a sensed reading (if any) from the X-band radar doppler
 *    using FreqMeasure library and returns the value as an integer
 */
int readDoppler(void) {
    int dopplerFreq = 0;
    if (FreqMeasure.available()) {
        // take an average of frequency readings
        total += FreqMeasure.read();
        counter++;
        // after five freq readings - take average
        if (counter > 5) {
            dopplerFreq = FreqMeasure.countToFrequency(total / counter);
            total = 0;
            counter = 0;
        }
    }
    return dopplerFreq;
}

/* Function: pirMotionTriggered
 *    Interrupt service routine to detect PIR motion
 */
void pirMotionTriggered(void) {
    IRMotionStarted = true;
}
