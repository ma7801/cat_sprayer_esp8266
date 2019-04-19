/* TODO 
 *  
 *  Code:
 *  - fix bug with delay of notification: delays the first hour without any notification sent
 *  - change code to use "enterDisabledState"
 *  - decide to use const's or #defines (research which is better)
 *  - make Blynk pins const's or #defines, not literals in code
 *  - look for any other literals in code and use const's or #defines

 *  
 *  
 *  
 *  
 *  Notes:
 *  - Not using real time clock -- will probably take too much power and cause a headache once I implement low power; Blynk records timestamps, albeit it doesn't display them on app (only for CSV download?)
 *  
 */

#include "BlynkSimpleEsp8266.h"
#include "ESP8266WiFi.h"   

//#define DEBUG 1
//#define IO_USERNAME  "ma7801"
//#define IO_KEY       "98bc56ceffa54facb2458d5e4f27aae1"


const uint8_t pirPin = D7;        
const uint8_t sprayerPin = D5; 
const uint8_t LED1Pin = D1;
const uint8_t LED2Pin = D2;
const uint8_t buttonPin = D6;     
const uint8_t enabledLEDPin = LED_BUILTIN;

const int sprayDuration = 500;   // in milliseconds
const int sprayDelayDuration = 10000; // ms -- should be greater than 8000 because PIR stays high for about 8 seconds or so, even if no motion
const long int sprayNotificationInterval = 60 * 60 * 1000; // ms - minimum time between push notifications from spray; 60*60*1000 = 1 hour
const int buttonCheckInterval = 150; //in milliseconds
const int checkForMotionInterval = 1000; // ms
const int disabledTimerDuration = 5 * 60 * 1000; // ms
const int sprayInterval = 250; //ms
const int blynkHIGH = 1023;  // for binary display in "SuperChart"
const int startupDisableDuration = 15 * 1000;

int LEDOnCount = 0;
bool LED1State = false;
bool LED2State = false;
bool buttonLock = false;
bool justSprayed = false;
bool sprayDelayState = false;
long lastSprayTime = 0;
long lastSprayNotificationTime = 0;
bool blynkSprayButton = false;
bool disabledState = false;
int disabledTimerId;

byte powerOnBlinkIteration = 1;
bool motionDetected;



// Blynk auth key
#define BLYNK_AUTH "2c1db245cdc649ecb62a966a78a83204"

// WIFI router credentials
#define WIFI_SSID "Pilsa_EXT"
#define WIFI_PW   "MJLLAnder$729"

BlynkTimer timer;
WidgetTerminal terminal(V0);



void setup() {
  #ifdef DEBUG
  Serial.begin(115200);
  #endif

  digitalWrite(pirPin, LOW);   //per HC-SR505 datasheet
  digitalWrite(sprayerPin, LOW);
  digitalWrite(LED1Pin, LOW);
  digitalWrite(LED2Pin, LOW);
  digitalWrite(buttonPin, LOW);
  #ifdef DEBUG
  digitalWrite(LED_BUILTIN, HIGH);
  #endif
  
  Blynk.begin(BLYNK_AUTH, WIFI_SSID, WIFI_PW);

  // Set up pins that aren't inputs
  pinMode(sprayerPin, OUTPUT);
  pinMode(LED1Pin, OUTPUT);
  pinMode(LED2Pin, OUTPUT);
  #ifdef DEBUG
  pinMode(LED_BUILTIN, OUTPUT);
  #endif
  //pinMode(buttonPin, INPUT);

  // Flash external LEDs to indicate power on
  timer.setTimer(250L, t_powerOnBlinks, 5);
  Serial.println("Serial test!");

  // Initialize pin states

  //digitalWrite(enabledLEDPin, LOW);

  terminal.clear();
  terminal.println("Power on");


  // Set event handlers
  timer.setInterval(buttonCheckInterval, t_buttonHandler);
  timer.setInterval(checkForMotionInterval, t_checkForMotion); 
  timer.setInterval(sprayInterval, t_spray);

  // Set to disabled state 
  enterDisabledState(startupDisableDuration);
  
}

void t_powerOnBlinks() {

  // Last iteration
  if (powerOnBlinkIteration == 5) {
    LED1State = false;
    LED2State = false;
  }
  // During blinking
  else {
    LED1State = !LED1State;
    LED2State = !LED1State;
  }
  digitalWrite(LED1Pin, LED1State);
  digitalWrite(LED2Pin, LED2State);
  
  powerOnBlinkIteration++;
  
}


void t_buttonHandler() {

  // Button handler
 
  // If the button has been released, release the "lock" on the button
  if (digitalRead(buttonPin) == LOW) {
    buttonLock = false;
  }
  
  if (digitalRead(buttonPin) == HIGH && buttonLock == false) {
    
    #ifdef DEBUG
    terminal.println("Button pressed");
    #endif

    // Put a the button in a "lock" state so that this code doesn't get run again during the same button press
    buttonLock = true;

    // Prevent idle state
    //okToIdle = false;
    
    // Case 0: No LEDs lit
    if (LEDOnCount == 0) {
      // Turn on LED1
      changeLEDs(1,0);
      
      //LEDOnCount = 1;

      // Set disabled flag
      disabledState = true;

      // Report as disabled to Blynk App (det disable button as disabled)
      Blynk.virtualWrite(V3, HIGH);
      
      // Create a disable timer
      disabledTimerId = timer.setTimeout(disabledTimerDuration, t_cancelDisable);
      
      // Set "remainingDisabledIterations" to the disabledIterationsInteveral (acts as a flag and a counter)
      //remainingDisabledIterations = disabledIterationInterval;

    }
  
    // Case 1: Only LED1 is lit; restart disabled mode with double the iteration length:
    else if (LEDOnCount == 1) {    
      // Turn on LEDs 1 & 2
      changeLEDs(1,1);
      
      // Now 2 LEDs are on
      //LEDOnCount = 2;

      // Delete the previous disable timer and set a new one double the disabledTimerDuration length
      timer.deleteTimer(disabledTimerId);
      disabledTimerId = timer.setTimeout(disabledTimerDuration * 2, t_cancelDisable);
      
      //secondInterval = true;

      // Reset the iterations amount
      //remainingDisabledIterations = disabledIterationInterval;

    }
    else if (LEDOnCount == 2) {
    
      #ifdef DEBUG
      Serial.println("Turning off both LEDs - user cancelling disable");
      #endif

      //Turn off both LEDs
      changeLEDs(0,0);

      // Delete the disabled timer and reset the disabledState flag
      timer.deleteTimer(disabledTimerId);
      disabledState = false;

      // Report as enabled to Blynk App (det disable button as enabled)
      Blynk.virtualWrite(V3, LOW);
      
      // Reset flags / counters
      LEDOnCount = 0;
      //remainingDisabledIterations = 1;  // The "1" is for a delay after coming out of disabled mode
      //secondInterval = false;      
    }
  }
}

// If disabled button is pressed on Blynk app
BLYNK_WRITE(V3) {
  disabledState = (bool)param.asInt();
  changeLEDs(disabledState, disabledState);

  // Set a timer to cancel the disable if disabled was pressed (double duration)
  if(disabledState == true) {
    // Delete any existing timer
    timer.deleteTimer(disabledTimerId);

    // Set timer
    disabledTimerId = timer.setTimeout(disabledTimerDuration * 2, t_cancelDisable);
  }
  else {
    // Cancel timer
    timer.deleteTimer(disabledTimerId);
  }
}

void changeLEDs(bool LED1State, bool LED2State) {
  digitalWrite(LED1Pin, LED1State);
  digitalWrite(LED2Pin, LED2State);
  LEDOnCount = LED1State + LED2State;
}

void enterDisabledState(long duration) {
  // Delete any existing disabled timer
  timer.deleteTimer(disabledTimerId);

  // Set disabled state to true
  disabledState = true;
  
  // Set disabled timer
  disabledTimerId = timer.setTimeout(duration, t_cancelDisable);

  // Light both LEDs
  changeLEDs(1,1);

  // Report to Blynk app
  Blynk.virtualWrite(V3, HIGH);
}

void t_cancelDisable() {
  // Turn off LEDs
  changeLEDs(0,0);

  // Reset disabledState flag
  disabledState = false;

  // Report to Blynk app
  Blynk.virtualWrite(V3, LOW);
}

void t_checkForMotion() {
  bool pirState = digitalRead(pirPin);

  // if PIR is triggered (low -> high)
  if(pirState == HIGH && motionDetected == false) {
    motionDetected = true;
    Serial.println("Motion detected!");
    Blynk.virtualWrite(V1, blynkHIGH);
  }

  // if PIR goes low from being high
  if(pirState == LOW && motionDetected == true) {
    motionDetected = false;
    Serial.println("No motion detected.");
    Blynk.virtualWrite(V1, 0);
  }
}

void t_spray() {
  /* Spray will occur if:
   *  - motion was detected
   *  - Blynk button was pressed
   */
  // Turn off the sprayer if we just sprayed
  if (justSprayed == true) {
    //If sprayDuration elapsed
    if (millis() - lastSprayTime >= sprayDuration) {
      #ifdef DEBUG
      digitalWrite(LED_BUILTIN, HIGH);
      #endif
      digitalWrite(sprayerPin, LOW);
      Blynk.virtualWrite(V2, 0);
      Serial.println("Spray turned off.");
      justSprayed = false;  
    }

    // don't run rest of code in function
    return;  
  }

  // If in disabled state
  if (disabledState == true) {
    // don't run rest of code in this function
    return;
  }
  
  // See if we're delaying between sprays -- if so don't allow another spray until spray delay timer has elapsed
  if (sprayDelayState == true) {
    // Bug fix -- Blynk doesn't always get the first virtualWrite that the sprayer is off in the "justSprayed == true" if statement above
    Blynk.virtualWrite(V2,0);
    
    if (millis() - lastSprayTime >= sprayDelayDuration) {
      sprayDelayState = false;
    }
    else return;
  }
  
  // Spray if motion detected
  if (motionDetected) {

    // Push notification code
    // If notification hasn't already been made in the last sprayNotificationInterval, send a push notification
    if (millis() - lastSprayNotificationTime >= sprayNotificationInterval) {
      Blynk.notify("Cat Sprayer {DEVICE_NAME} sprayed via motion detection.");  
      lastSprayNotificationTime = millis();
    }

    // Spray the sprayer
    spray();
    
    #ifdef DEBUG
    digitalWrite(LED_BUILTIN, LOW);
    #endif
    
    justSprayed = true;  
    sprayDelayState = true;
    lastSprayTime = millis();
    blynkSprayButton = false;
  }
}

// If sprayer button pressed on Blynk
BLYNK_WRITE(V4) {
  blynkSprayButton = (bool)param.asInt();

  if(blynkSprayButton) {
    #ifdef DEBUG
    digitalWrite(LED_BUILTIN, LOW);
    #endif
    spray();
  }
  else {
    // turn off sprayer
    #ifdef DEBUG
    digitalWrite(LED_BUILTIN, HIGH);
    #endif
    Serial.println("Turning off sprayer...");
    digitalWrite(sprayerPin, LOW);
  }
}

void spray() {
  Serial.println("Spray!!!");
  Blynk.virtualWrite(V2, blynkHIGH);
  digitalWrite(sprayerPin, HIGH);  
}

void loop() {
  Blynk.run();
  timer.run();
 
}
