/*************************************************************************
 * Basic Doppler Frequency motion sensing program:                       *
 *      A program to process an input waveform from a Doppler motion     *
 *      sensing module, such as the HB100 or Parallax X-Band Radar       *
 *      motion detector. It uses the FreqMeasure library, which needs    *
 *      digital pin 8 or the Arduino UNO (or 49 on the MEGA)             *                                                                 *
 *                                                                       *
 *      Author: Sgt. B.D. Fraser                                         *
 *                                                                       *
 *        Last modified: 12/06/2017                                      *
 *                                                                       *
 *************************************************************************/

// Freq Measure lib - uses digital pin 8 of Arduino Uno for measurement
#include <FreqMeasure.h>

void setup() {
  // initialise serial monitor for display messages
  Serial.begin(9600);

  // initialise frequency measurement on digital pin 8
  FreqMeasure.begin();
}

// global variables for calculating Doppler freq
double total=0;
int counter=0;

void loop() {
  if (FreqMeasure.available()) {
    
    // take an average of frequency readings
    total += FreqMeasure.read();
    counter++;
    // after five freq readings - take average and print result
    if (counter > 5) {
      float dopplerFreq = FreqMeasure.countToFrequency(total/counter);
      if (dopplerFreq > 5) {
        Serial.print("Motion was detected! The measured doppler frequency was: ");
        Serial.println(dopplerFreq);
      }
      total = 0;
      counter = 0;
    }
  }
}
