/*************************************************************************
 * Master-unit - nRF24L01+ radio communications                          *
 *      A program to operate as a master unit that transmits to and      *
 *      receives data from three remote slave devices. Each slave        *
 *      also has an nrf24l01+ transceiver, and replies with data         *
 *      using the Acknowledgement payload facility of the Enhanced       *
 *      ShockBurst packet structure.                                     *
 *                                                                       *
 *      Author: B.D. Fraser                                              *
 *                                                                       *
 *        Last modified: 25/06/2017                                      *
 *                                                                       *
 *************************************************************************/

// include external libs for nrf24l01+ radio transceiver communications
#include <RF24.h>
#include <SPI.h>
#include <nRF24l01.h>
#include <printf.h>

// set Chip-Enable (CE) and Chip-Select-Not (CSN) radio setup pins
#define CE_PIN 9
#define CSN_PIN 10

// set transmission cycle send rate - in milliseconds
#define SEND_RATE 1000

// create RF24 radio object using selected CE and CSN pins
RF24 radio(CE_PIN,CSN_PIN);

// setup radio pipe addresses for each sensor node
const byte nodeAddresses[3][5] = {
    {'N','O','D','E','1'},
    {'N','O','D','E','2'},
    {'N','O','D','E','3'}
};

// integer to store count of successful transmissions
int masterSendCount = 0;

// simple integer array for data from each slave node: { node_id, returned_count }
int remoteNodeData[3][3] = {{1, 1,}, {2, 1}, {3, 1}};

// system operation timing variables - set SEND_RATE to limit transmit rate
unsigned long currentTime;
unsigned long lastSentTime;

/* Function: setup
 *    Initialises the system wide configuration and settings prior to start
 */
void setup()
{
    // setup serial communications for basic program display
    Serial.begin(9600);
    Serial.println("[*][*][*] Beginning nRF24L01+ master-multiple-slave program [*][*][*]");
    
    // ----------------------------- RADIO SETUP CONFIGURATION AND SETTINGS -------------------------//
    
    // begin radio object
    radio.begin();
    
    // set power level of the radio
    radio.setPALevel(RF24_PA_LOW);
    
    // set RF datarate - lowest rate for longest range capability
    radio.setDataRate(RF24_250KBPS);
    
    // set radio channel to use - ensure all slaves match this
    radio.setChannel(0x76);
    
    // set time between retries and max no. of retries
    radio.setRetries(4, 10);
    
    // enable ack payload - each slave replies with sensor data using this feature
    radio.enableAckPayload();
    
    // --------------------------------------------------------------------------------------------//
}


/* Function: loop
 *    main loop program for the master device - repeats continuously during operation
 */
void loop()
{
    // ensure we dont collect data from nodes faster than selected rate
    currentTime = millis();
    while (currentTime - lastSentTime <= SEND_RATE) {}
    
    // collect sensor data from all nodes
    receiveNodeData();
    
    lastSentTime = millis();
}


/* Function: receiveNodeData
 *    Make a radio call to each node in turn and retreive a message from each
 */
void receiveNodeData()
{
    Serial.print("[*] Master unit has successfully sent and received data ");
    Serial.print(masterSendCount);
    Serial.println(" times.");
    
    // make a call for data to each node in turn
    for (byte node = 0; node < 3; node++) {
        
        // setup a write pipe to current sensor node - must match the remote node listening pipe
        radio.openWritingPipe(nodeAddresses[node]);
        
        Serial.print("[*] Attempting to transmit data to node ");
        Serial.println(node + 1);
        Serial.print("[*] The master unit count being sent is: ");
        Serial.println(masterSendCount);
        
        // boolean to indicate if radio.write() tx was successful
        bool tx_sent;
        tx_sent = radio.write( &masterSendCount, sizeof(masterSendCount) );
        
        // if tx success - receive and read slave node ack reply
        if (tx_sent) {
            if (radio.isAckPayloadAvailable()) {
                
                // read ack payload and copy data to relevant remoteNodeData array
                radio.read(&remoteNodeData[node], sizeof(remoteNodeData[node]));
                
                Serial.print("[+] Successfully received data from node: ");
                Serial.println(node);
                Serial.print("  ---- The node count received was: ");
                Serial.println(remoteNodeData[node][1]);
                
                // iterate master device count - keeps data changing between transmissions
                if (masterSendCount < 500) {
                    masterSendCount++;
                } else {
                    masterSendCount = 1;
                }
            }
        }
        else {
            Serial.print("[-] The transmission to the selected node failed.");
        }
    }
    Serial.println("--------------------------------------------------------");
}
