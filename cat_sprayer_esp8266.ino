/* TODO 
 *  
 *  Code:
 *  - use timers for disabled feature?  
 *  - also, disable remote from Blynk app
 *  

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
#include "user_interface.h"

#define DEBUG 1
#define MAX_SLEEP_TIME 0xFFFFFFF
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
const int buttonCheckInterval = 150; //in milliseconds
const int checkForMotionInterval = 1000; // ms
const int disabledTimerDuration = 5 * 60 * 1000; // ms
const int sprayInterval = 250; //ms
const int blynkHIGH = 1023;  // for binary display in "SuperChart"


volatile bool button_pressed;
//volatile bool pir_triggered;
int LEDOnCount = 0;
bool LED1State = false;
bool LED2State = false;
bool buttonLock = false;
bool justSprayed = false;
bool sprayDelayState = false;
long lastSprayTime = 0;
bool blynkSprayButton = false;
bool disabledState = false;
int disabledTimerId;
int buttonCheckTimerId;
int checkForMotionTimerId;
int sprayTimerId;
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

  // Initialize pin states
  digitalWrite(pirPin, LOW);   //per HC-SR505 datasheet
  digitalWrite(sprayerPin, LOW);
  digitalWrite(LED1Pin, LOW);
  digitalWrite(LED2Pin, LOW);
  digitalWrite(buttonPin, LOW);
  #ifdef DEBUG
  digitalWrite(LED_BUILTIN, HIGH);
  #endif
  
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

  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH, WIFI_SSID, WIFI_PW);

  //digitalWrite(enabledLEDPin, LOW);

  terminal.clear();
  terminal.println("Power on");


  // Set event handlers
  buttonCheckTimerId = timer.setInterval(buttonCheckInterval, t_buttonHandler);
  checkForMotionTimerId = timer.setInterval(checkForMotionInterval, t_checkForMotion); 
  sprayTimerId = timer.setInterval(sprayInterval, t_spray);

  // Set low power state to light_sleep
  wifi_set_sleep_type(LIGHT_SLEEP_T);
  
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

void wakeUpCallBack() {
  Serial.println("Waking up...");
  
}

void goToSleep() {
  //Disable all timers
  /*timer.disable(disabledTimerId);
  timer.disable(buttonCheckTimerId);
  timer.disable(checkForMotionTimerId);
  timer.disable(sprayTimerId);
  */
  
  Serial.println("Attempting to go to sleep...");
  //wifi_station_disconnect();
  //wifi_set_opmode(NULL_MODE);                // set WiFi  mode  to  null  mode.
  //wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);    // light sleep
  //wifi_fpm_open();                           // enable  force sleep

  //PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,  FUNC_GPIO13);

  wifi_enable_gpio_wakeup(GPIO_ID_PIN(7), GPIO_PIN_INTR_HILEVEL);
  wifi_fpm_set_wakeup_cb(wakeUpCallBack);     //  Set wakeup  callback  
  //wifi_fpm_do_sleep(MAX_SLEEP_TIME);  
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
  
  WiFi.forceSleepBegin();
  //delay(2000);
  Serial.println("Shouldn't print immediately.");

  
}


void loop() {

  Blynk.run();
  timer.run();

  goToSleep();
 
}
