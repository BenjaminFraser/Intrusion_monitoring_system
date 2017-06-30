/*************************************************************************
 * Command Unit System Program:                                          *
 *      A program to operate a master command device that works as part  *
 *      of a larger intrusion and monitoring security detection system   *
 *                                                                       *
 * Usage:                                                                *
 *      This program is specifically designed for use with the Arduino   *
 *      MEGA micro-controller, the NRF24L01+ transceiver unit, an LCD    *  
 *      screen a remote system of motion detection nodes. Each remote    *
 *      node senses and monitors nearby motion using both Passive        *
 *      Infrared (PIR) and X-Band Radar Doppler sensing. The detection   *
 *      status of each remote node is sent to this device, which acts    *
 *      as a master control device that displays detection statuses and  *   
 *      allows for reset control of each node.                           *
 *      On reception of a HIGH (int '11') PIR and Doppler motion detect  * 
 *      the system shows an alert in the form of visual light and an     *
 *      audible noise. It only displays this alarm when both sensors     *     
 *      are activated so that the frequency of false alarms are          *
 *      dramatically lowered. If Doppler is detected on its own, an      *
 *      amber 'motion' light is triggered for a short period.            *
 *                                                                       *
 *      Author: Benjamin D Fraser                                         *
 *                                                                       *
 *        Last modified: 30/06/2017                                      *
 *                                                                       *
 *************************************************************************/
 
#include <LiquidCrystal.h>

// include external libs for nrf24l01+ radio transceiver communications
#include <RF24.h> 
#include <SPI.h> 
#include <nRF24l01.h>

// set Chip-Enable (CE) and Chip-Select-Not (CSN) radio setup pins
#define CE_PIN 48
#define CSN_PIN 53
RF24 radio(CE_PIN,CSN_PIN);

// interrupt pin on arduino MEGA for reset
const int RESET = 18;

// int array to store node, pirMotionDetected status, doppler_motion_status.
// takes the form remoteNode[NODE_NUM] = {nodeID, pirMotionDetectedStatus, dopplerMotionStatus}
// status '22' means ALL CLEAR, status '11' means DETECTION or HIGH
int remoteNodeData[3][3] = {{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}};

// int array to store master device tx messages: {systemCount, systemReset}
int masterDeviceData[2] = {0};

// setup radio pipe addresses for radio communication - 1 address per remote node
const byte nodeAddresses[3][5] = {
                                        {'P','O','S','T','A'},   // remote node 1
                                        {'P','O','S','T','B'},   // remote node 2
                                        {'P','O','S','T','C'}    // remote node 3
                                       };

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(0, 1, 5, 4, 3, 2); // LCD pins

// set LED pins
byte safeLight = 6;      // output for green LED
byte motionLight = 7;    // output for amber LED
byte alertLight = 8;     // output for buzzer and red LED

// boolean alarm flag - changed during interrupt - make volatile 
volatile bool alarmFlag = false;

// global vars to indicate detected PIR and Doppler motion detection
bool pirMotionDetected = false;
bool motionDetected = false;

// system operation timing variables
unsigned long currentTime;
unsigned long lastSentTime;
unsigned long sendRate = 200; // tx-loop rate - once per 1/5 second


/* Function: setup
 *    Initialises the system wide configuration and settings prior to start
 */
void setup()
{
  // ----------------------------- RADIO SETUP CONFIGURATION AND SETTINGS -------------------------// 

  // begin radio object
  radio.begin();
  
  // set power level of the radio
  radio.setPALevel(RF24_PA_LOW);

  // set RF datarate
  radio.setDataRate(RF24_250KBPS); // MAX for radio spec

  // set radio channel to use - ensure it matches the target host
  radio.setChannel(0x76);  // made up HEX code

  // set time between retries and max no. of retries
  radio.setRetries(4, 10);

  // enable ack payload - each slave replies with sensor data using this feature
  radio.enableAckPayload();

  // --------------------------------------------------------------------------------------------//

  // ----------------------------- LCD DISPLAY CONFIGURATION AND SETTINGS -----------------------// 
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2); // clears screen
  
  // Print welcome message.
  lcd.print("   Intrusion"); 
  lcd.setCursor(0, 1);
  lcd.print(" Monitor System");
  
  // set pins:
  pinMode(safeLight, OUTPUT);
  pinMode(motionLight, OUTPUT);
  pinMode(alertLight, OUTPUT);

  // setup reset interrupt sequence - input pullup on digital 18 - calls resetProgram 
  pinMode(RESET, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RESET), resetProgram, CHANGE);
}


/* Function: loop
 *    main loop program for the master device - repeats continuously during system operation
 */
void loop()
{
    // collect sensor data from all 3 nodes
    receiveNodeData();

    // assess each sensor status and update system indications
    analyseNodeData();

    // delay temporarily before next loop
    customDelay(100);
}


/* Function: analyseNodeData
 *    Checks the status of the alert status data for each remote node, and raises
 *    the applicable alert if one is found. int '22' is 'clear', whilst '11' indicates
 *    an alert with the associated field.
 */
void analyseNodeData(void) 
{
    // boolean variable to indicate any alert found
    bool alertFound = false;

    // check states of doppler motion sensed data - set dopplerAlert if status '11'
    for (int node = 0; node < 3; node++) {
      if (remoteNodeData[node][2] == 11) { 
        motionDetected = true;
        alertFound = true; 

        // check if PIR motion also detected
        if (remoteNodeData[node][1] == 11) {
          pirMotionDetected = true;
        
          // call system alert with corresponding node num
          systemAlert(node + 1);
          break;
        }

        // no PIR - call motion alert but not full-system alert
        else {
        // call motion alert with corresponding node num
        motionAlert(node + 1);
        break;
        }
      }
    }

    // if no alert found and radio comms achieved - indicate system clear
    if (!alertFound) {
      if (remoteNodeData[0][2] == 22 || remoteNodeData[1][2] == 22 || remoteNodeData[2][2] == 22) {
        systemClear();
      }
    }
}


/* Function: receiveNodeData
 *    Make a radio call to each node in turn and retreive the sensed system states
 */
void receiveNodeData() 
{
    // collect sensor data from all 3 poles no faster than once per second
    currentTime = millis();
    if (currentTime - lastSentTime >= sendRate) {
        // make a call for data to each node in turn
        for (byte node = 0; node < 3; node++) {

            // setup a write pipe to the node - must match the associated reading pipe
            radio.openWritingPipe(nodeAddresses[node]);

            // boolean to indicate if radio.write() tx was successful
            bool tx_sent;
            tx_sent = radio.write( &masterDeviceData, sizeof(masterDeviceData) );

            // if tx success - receive and read node ack reply
            if (tx_sent) {
                if (radio.isAckPayloadAvailable()) {

                    // read ack payload and copy sensor status to remoteNodeData array
                    radio.read(&remoteNodeData[node], sizeof(remoteNodeData[node]));
                    
                        // iterate master count
                        if (masterDeviceData[0] < 800) {
                            masterDeviceData[0]++;
                        }
                }

            }
        }
        lastSentTime = millis();
    }
 }


/* Function: customDelay
 *    Custom delay to allow concurrent activities during program delays
 */
void customDelay(unsigned long duration) {
    unsigned long start = millis();

    // loop for the required time without the need for delay()
    while((millis() - start < duration)) {}
}


/* Function: systemAlert
 *    Displays alert status on the LCD and operates a system alarm.
 *    Also indicates the location of the node given by the passed 'node' int
 */
void systemAlert(int node)
{
    alarmFlag = true;
    turnOff(safeLight);
    turnOff(motionLight);

    // print alert warning message to LCD screen
    // print before loop as screen flickers if in while loop.
    lcd.begin(16, 2);
    lcd.setCursor(0, 0); 
    lcd.print("*ALERT: NODE "); 
    lcd.print(node);
    lcd.print("*");
    lcd.setCursor(1, 1);
    lcd.print("Reset to clear");

    // keep in alarm state until alarmFlag changes by reset button
    while (alarmFlag == true) {

      // turn on red LED and audio buzzer
      turnOn(alertLight);

      // continue monitoring for Doppler motion status '11' and indicate alert light if so
      receiveNodeData();
      if (remoteNodeData[0][2] == 11 || remoteNodeData[1][2] == 11 || remoteNodeData[2][2] == 11) {
          turnOn(motionLight); 
      }
      customDelay(500);
    }

    // send reset command to all remote nodes
    sendReset();
}


/* Function: sendReset
 *    sends a reset status (int 11) of masterDeviceData[1] to each node, and resets
 *    stored system motion data for each node
 */
void sendReset() {

    // set master device reset field to true ID (11)
    masterDeviceData[1] = 11;

    // make a call for data to each node in turn
    for (byte node = 0; node < 3; node++) {

        // setup a write pipe to the node - must match the nodes reading pipe
        radio.openWritingPipe(nodeAddresses[node]);

        // boolean to indicate if radio.write() tx was successful
        bool tx_sent;

        // var to limit the number of reset messages sent
        int reset_counter = 0;

        // send reset command to all node until success, or 5 attempts are made
        do {  
          tx_sent = radio.write( &masterDeviceData, sizeof(masterDeviceData) );
          reset_counter++;
        } while (!tx_sent && reset_counter < 5);

        // store ack reply in new buffer and ignore first message after reset
        int bufferData[3] = {0};
        if (tx_sent) {
            if (radio.isAckPayloadAvailable()) {
                radio.read(&bufferData, sizeof(bufferData));
            }
        }
    }

    // update last sent time to avoid radio spamming
    lastSentTime = millis();
    
    // reset node sensor parameters to normal
    masterDeviceData[1] = 22;
    for (byte node = 0; node < 3; node++) {
        remoteNodeData[node][1] = 22;
        remoteNodeData[node][2] = 22;
    }
 }


/* Function: motionDetected
 *    Displays a motion alert on the LCD and provides a system LED indication
 */
void motionAlert(int node)
{
    turnOff(safeLight);
    lcd.begin(16, 2);
    lcd.setCursor(0, 0); 
    lcd.print("*CAUTION NODE: ");
    lcd.print(node);
    lcd.setCursor(2, 1);
    lcd.print("Motion sensed");
    turnOn(motionLight);              
}


/* Function: systemClear
 *    Displays a system clear status on the LCD and provides a safe indication
 */
void systemClear()
{
    turnOff (alertLight);
    turnOff (motionLight);
    lcd.begin(16, 2);
    lcd.setCursor(2, 0); 
    lcd.print("System Clear");
    lcd.setCursor(0, 1);
    lcd.print("# nodes: 1");
    turnOn(safeLight);
}


/* Function: resetProgram
 *    Interrupt service routine to reset alarm flag and continue system operation
 */
void resetProgram()
{
    alarmFlag = false;
}


/* Function: turnOn
 *    Sets the given pin number to high for switching on LEDs
 */
void turnOn(int light)
{
    // set digital pin corresponding to given argument high
    digitalWrite(light, HIGH);
}


/* Function: turnOff
 *    Sets the given pin number to low for switching off LEDs
 */
void turnOff(int light)
{
    // set digital pin corresponding to given argument low
    digitalWrite(light, LOW);
}