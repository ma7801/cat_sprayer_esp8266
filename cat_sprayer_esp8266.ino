/* TODO 
 *  
 *  Code:
 *  - Putnam wifi if no home wifi
 *  - fix spraying twice on same motion detect -- logic error.  Probably need yet another flag
 *  
 *  
 *  Maybe:
 *  - prevent spraying additional times within the same motion detection (i.e. wait until another low to high on motion detector before allowing sprayer to spray again)
 *  
 *  Notes:
 *  - Not using real time clock -- will probably take too much power and cause a headache once I implement low power; Blynk records timestamps, albeit it doesn't display them on app (only for CSV download?)
 *  - Notification will not be sent in first hour
 */

#include "BlynkSimpleEsp8266.h"
#include "ESP8266WiFi.h"   

#define DEBUG 1
#define BLYNK_PRINT Serial

const uint8_t pirPin = D7;        
const uint8_t sprayerPin = D5; 
const uint8_t LED1Pin = D1;
const uint8_t LED2Pin = D2;
const uint8_t buttonPin = D6;     
const uint8_t enabledLEDPin = LED_BUILTIN;

const int sprayDuration = 300;   // in milliseconds
const int sprayDelayDuration = 10 * 1000; // ms -- should be greater than 8000 because PIR stays high for about 8 seconds or so, even if no motion
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
bool firstDisable = true;
int disabledTimerId;
bool notificationsAllowed = true;

byte powerOnBlinkIteration = 1;
bool motionDetected;



// Blynk auth key
#define BLYNK_AUTH "2c1db245cdc649ecb62a966a78a83204"

// WIFI router credentials
#define WIFI_SSID "Pilsa"
#define WIFI_PW   "MJLLAnder$729"

// School WIFI router credentials
#define SCHOOL_WIFI_SSID "NClack_Guest"
#define SCHOOL_WIFI_PW "nclackguest"

BlynkTimer timer;
WidgetTerminal terminal(V0);

#ifdef DEBUG
//Print to serial and Blynk terminal
void p(String msg) {
  Serial.println(msg);
  terminal.println(msg);
}
#endif

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
   
  
  // Clear Blynk terminal
  terminal.clear();

  #ifdef DEBUG
  p("Serial and terminal test!");
  #endif
  
  // Set event handlers
  timer.setInterval(buttonCheckInterval, t_buttonHandler);
  timer.setInterval(checkForMotionInterval, t_checkForMotion); 
  timer.setInterval(sprayInterval, t_spray);

  // Set to disabled state 
  enterDisabledState(startupDisableDuration);
  
}



void t_buttonHandler() {

  // Button handler
 
  // If the button has been released, release the "lock" on the button
  if (digitalRead(buttonPin) == LOW) {
    buttonLock = false;
  }

  
  if (digitalRead(buttonPin) == HIGH && buttonLock == false) {

    // Put a the button in a "lock" state so that this code doesn't get run again during the same button press
    buttonLock = true;

    #ifdef DEBUG
    p("Button pressed!");
    #endif 
    
    // Case 0: No LEDs lit
    if (LEDOnCount == 0) {
      // Turn on LED1
      //changeLEDs(1,0);

      #ifdef DEBUG
      p("Entering short disabled state...");
      #endif
      enterDisabledState(disabledTimerDuration, true, false);
      //LEDOnCount = 1;

    }
  
    // Case 1: Only LED1 is lit; restart disabled mode with double the iteration length:
    else if (LEDOnCount == 1) {    
      
      #ifdef DEBUG
      p("Entering long disabled state...");
      #endif
      enterDisabledState(disabledTimerDuration*2);
    
    }
    else if (LEDOnCount == 2) {
    
      #ifdef DEBUG
      p("User cancelling disable");
      #endif

      t_cancelDisable();

    }
  }
}

// If temp disabled button is pressed on Blynk app (Virtual pin 3)
BLYNK_WRITE(V3) {

  // If button pressed on Blynk app
  if ((bool)param.asInt() == true) {
    enterDisabledState(disabledTimerDuration * 2);  
  }
  // If button unpressed
  else {
    // Cancel disable
    t_cancelDisable();
  }
}

// If hold disabled button is pressed on Blynk app (Virtual pin 5)
BLYNK_WRITE(V5) {
   // If button pressed on blynk app
  if ((bool)param.asInt() == true) {
    enterDisabledState(LONG_MAX);  
  }
   // If button unpressed
  else {
    // Cancel disable
    t_cancelDisable();
  }  
}

void changeLEDs(bool LED1State, bool LED2State) {
  digitalWrite(LED1Pin, LED1State);
  digitalWrite(LED2Pin, LED2State);
  LEDOnCount = LED1State + LED2State;
}

void enterDisabledState(long duration, bool LED1State, bool LED2State) {
  // Delete any existing disabled timer (skip if this is the first disable (bug fix); otherwise, button handler timer deleted because disableTimerId initialized to 0 which matches id of first timer created, I think
  if (firstDisable == true) {
    firstDisable = false;
  }
  else {
    timer.deleteTimer(disabledTimerId);  
  }

  // Set disabled state to true
  disabledState = true;
  
  // Set disabled timer
  disabledTimerId = timer.setTimeout(duration, t_cancelDisable);

  // Light LEDs
  changeLEDs(LED1State,LED2State);

  // Report to Blynk app
  Blynk.virtualWrite(V3, HIGH);
  Blynk.virtualWrite(V5, HIGH);
}

// Overloaded enterDisabledState: will light both LEDs - this version of function called most of the time
void enterDisabledState(long duration) {
  enterDisabledState(duration, true, true);
}

void t_cancelDisable() {
  // Turn off LEDs
  changeLEDs(0,0);

  // Reset disabledState flag
  disabledState = false;

  // Delete disabled timer
  timer.deleteTimer(disabledTimerId);

  // Report to Blynk app
  Blynk.virtualWrite(V3, LOW);
  Blynk.virtualWrite(V5, LOW);
}

void t_checkForMotion() {
  bool pirState = digitalRead(pirPin);

  // if PIR is triggered (low -> high)
  if(pirState == HIGH && motionDetected == false) {
    motionDetected = true;
    p("Motion detected! (PIR went high.)");
    Blynk.virtualWrite(V1, blynkHIGH);
  }

  // if PIR goes low from being high
  if(pirState == LOW && motionDetected == true) {
    motionDetected = false;
    p("PIR went low.");
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
      p("Spray turned off.");
      #endif
      
      digitalWrite(sprayerPin, LOW);
      Blynk.virtualWrite(V2, 0);
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
    if ((millis() - lastSprayNotificationTime >= sprayNotificationInterval) && notificationsAllowed) {
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
    p("Turning off sprayer...");
    #endif
    digitalWrite(sprayerPin, LOW);
  }
}

void spray() {
  #ifdef DEBUG
  p("Spray!!!");
  #endif
  
  Blynk.virtualWrite(V2, blynkHIGH);
  digitalWrite(sprayerPin, HIGH);  
}

// If notifications button pressed
BLYNK_WRITE(V6) {
  notificationsAllowed = (bool) param.asInt();
}

void loop() {
  Blynk.run();
  timer.run();
 
}
