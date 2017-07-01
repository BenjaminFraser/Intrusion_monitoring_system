/*******************************
 * Basic PIR sensing program   *
 *******************************/
 
int pirInput = 2;               // choose the input pin (for PIR sensor)
bool pirMotion = false;         // set motion flag to false initially
int state = 0;                  // var for reading pin status
 
void setup() {
  
  // declare sensor as input
  pinMode(pirInput, INPUT);
 
  Serial.begin(9600);
}
 
void loop(){
  
  // read state of digital pin connected to pir output
  state = digitalRead(pirInput);

  // check if pir input is high and last state had no motion 
  if (state == HIGH && pirMotion == false) {
      Serial.println("The PIR sensor detected motion.");
      
      // change the global ir flag to indicate motion 
      pirMotion = true;
      
  } else {
    
    // if last state was motion indicate end
    if (pirMotion == true) {

      // indicate end of motion
      Serial.println("Detection of motion has ended.");

      // reset motion flag
      pirMotion = false;
    }
  }
}