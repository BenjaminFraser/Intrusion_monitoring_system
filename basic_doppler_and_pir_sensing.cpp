/*************************************************************************
 * Intrusion Monitoring System node:                                     *
 *      A program to operate as a remote node that senses both passive   *
 *      infrared (PIR) and X-Band Radar Doppler data to reliably         *
 *      detect nearby motion.                                            *
 *                                                                       *
 * Usage:                                                                *
 *      This program is specifically designed for use with the Arduino   *
 *      UNO microcontroller, a custom built HB100 doppler motion         *
 *      detector, and a passive IR motion sensing module.                *
 *                                                                       *
 *      Author: Sgt. B.D. Fraser                                         *
 *                                                                       *
 *        Last modified: 22/06/2017                                      *
 *                                                                       *
 *************************************************************************/

// Freq Measure lib - uses digital pin 8 of Arduino Uno for measurement
#include <FreqMeasure.h>

// SYSTEM SETTING PARAMETERS
#define MOTION_SENSITIVITY 10  // 10 = High, 30 = Medium, 45 = Low
#define IR_HOLD_TIME 50        // the number of loops to hold IR motion high
bool IR_MOTION_ON = true;      // if no PIR motion detection is needed - set to false

// PIR motion sensor input pin - HIGH if motion detected
const int IR_MOTION_PIN = 2; 

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
}


/* Function: loop
 *    main loop program for the command post - repeats continuously during system operation
 */
void loop() {

  // sense current environment conditions for IR motion and doppler motion
  senseAndDelay(250);

  // update current smartPost structure using sensed data
  updatePostData();

  // print the current motion status to Serial monitor
  printMotionStatus();
}


/* Function: printMotionStatus
 *    Outputs the current detection status to the Serial console.
 */
void printMotionStatus(void) {

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


/* Function: updatePostData
 *    updates the system states of the IR beam and doppler motion variables, and stores
 *    them in the acknowledgement payload ready for transmission
 */
void updatePostData(void) 
{
  // if receive beam mode selected, check state of beam-break
  if (IR_MOTION_ON == true) pirMotionUpdate();

  // check state of doppler motion
  dopplerMotionStatus();
}


/* Function: pirMotionUpdate
 *    Updates the IR motion status in smartPostData[POST_ID][1] based on 
 *    the sensed IR motion data.
 */
void pirMotionUpdate(void) {

  
  // if beam break detected - raise flag and update post structure data
  if (IRMotionStarted) {
    IRMotion = true;

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
          IRMotion = false;
      }
  }
}


/* Function: dopplerMotionUpdate
 *    Updates the doppler motion status in smartPostData[POST_ID][2] based on 
 *    the sensed radar data.
 */
void dopplerMotionStatus(void) {
  
    // if doppler motion detected - raise flag and update post structure
    if (motionValue > MOTION_SENSITIVITY) {
      dopplerMotionDetected = true;

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
            dopplerMotionDetected = false;
        }
    }
    // reset motion val before next loop
    motionValue = 0;
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
