/* TODO 
 *  
 *  Code:
 *  - PIR code
 *  - Sprayer code -- maybe have an internal LED represent a spray for right now
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

#define DEBUG 1
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
const int sprayInterval = 250; //ms
const int blynkHIGH = 1023;


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

byte powerOnBlinkIteration = 1;
//int remainingDisabledIterations;
//bool secondInterval;
//bool okToIdle;
//unsigned long int lastButtonPress;

bool motionDetected;



// Blynk auth key
#define BLYNK_AUTH "2c1db245cdc649ecb62a966a78a83204"

// WIFI router credentials
#define WIFI_SSID "Pilsa_EXT"
#define WIFI_PW   "MJLLAnder$729"

BlynkTimer timer;
WidgetTerminal terminal(V0);

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

void setup() {
  #ifdef DEBUG
  Serial.begin(115200);
  #endif

  digitalWrite(pirPin, LOW);   //per HC-SR505 datasheet
  digitalWrite(sprayerPin, LOW);
  digitalWrite(LED1Pin, LOW);
  digitalWrite(LED2Pin, LOW);
  digitalWrite(buttonPin, LOW);

  Blynk.begin(BLYNK_AUTH, WIFI_SSID, WIFI_PW);

  // Set up pins that aren't inputs
  pinMode(sprayerPin, OUTPUT);
  pinMode(LED1Pin, OUTPUT);
  pinMode(LED2Pin, OUTPUT);
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
      digitalWrite(LED1Pin, HIGH);
  
      LEDOnCount = 1;
      
      // Set "remainingDisabledIterations" to the disabledIterationsInteveral (acts as a flag and a counter)
      //remainingDisabledIterations = disabledIterationInterval;

    }
  
    // Case 1: Only LED1 is lit; restart disabled mode with double the iteration length:
    else if (LEDOnCount == 1) {    
      // Turn on LED2
      digitalWrite(LED2Pin, HIGH);
      
      // Now 2 LEDs are on
      LEDOnCount = 2;
  
      //secondInterval = true;

      // Reset the iterations amount
      //remainingDisabledIterations = disabledIterationInterval;

    }
    else if (LEDOnCount == 2) {
    
      #ifdef DEBUG
      Serial.println("Turning off both LEDs - user cancelling disable");
      #endif

      //Turn off both LEDs
      digitalWrite(LED1Pin, LOW);
      digitalWrite(LED2Pin, LOW);

      // Reset flags / counters
      LEDOnCount = 0;
      //remainingDisabledIterations = 1;  // The "1" is for a delay after coming out of disabled mode
      //secondInterval = false;      
    }
  }

}

void t_checkForMotion() {
  bool pirState = digitalRead(pirPin);

  // if PIR is triggered (low -> high)
  if(pirState == HIGH && motionDetected == false) {
    //***TEMPORARY REMARK OUT BELOW-- NEED TO UNREMARK FOR PIR TO WORK!!!
    //motionDetected = true;
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
      digitalWrite(sprayerPin, LOW);
      Blynk.virtualWrite(V2, 0);
      Serial.println("Spray turned off.");
      justSprayed = false;  
    }

    // don't run rest of code in function
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
    spray();
  }
  else {
    // turn off sprayer
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
 
  
  // If disabled
  /*
  if(remainingDisabledIterations > 0) {
    
    #ifdef DEBUG
    Serial.println("Sleeping for 8 seconds...");
    delay(SERIAL_DELAY);
    #endif

    //Delay to improve button operation
    delay(buttonDelay);
    
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);

    #ifdef DEBUG
    Serial.println("Waking up!");
    delay(SERIAL_DELAY);
    #endif
    
    remainingDisabledIterations--;

    // If last iteration elapsed:
    if(remainingDisabledIterations == 0) {
      // If there is a second disable interval (i.e. user hit button twice)
      if(secondInterval) {
        secondInterval = false;
    
        // Turn off LED2
        digitalWrite(LED2Pin, LOW);

        // Indicate that only one LED is on now
        LEDOnCount = 1;
    
        // Reset the iteration count to disabledIterationsInterval
        remainingDisabledIterations = disabledIterationInterval;
    
      }
      
      // Otherwise, disabled period totally elapsed
      else {
        // Turn off LED1
        digitalWrite(LED1Pin, LOW);

        // Indicate now LEDs on
        LEDOnCount = 0;

        // Set Ok to power down flag
        okToIdle = true;
      }
    } 
  }
*/
  

/*
  // If motion detected
  if(pir_triggered) {

    // Reset flag
    pir_triggered = false;
    
    // Check if in disabled state; if so, don't spray, just repeat the loop()
    if (remainingDisabledIterations > 0) return;

    // Activate sprayer
    digitalWrite(sprayerPin, HIGH);
    
    #ifdef DEBUG
    Serial.println("Spray!");
    #endif

    #ifdef DEBUG
    digitalWrite(13, HIGH);
    #endif
    
    delay(sprayDuration);

    #ifdef DEBUG
    digitalWrite(13, LOW);
    #endif
    
    digitalWrite(sprayerPin, LOW);
  }

  // Enter a low power idle until interrupt activated
  if(okToIdle) {
    #ifdef DEBUG
    Serial.print("button_pressed=");
    Serial.println(button_pressed);
    Serial.print("millis()=");
    Serial.println(millis());
    Serial.println("Going to idle state...");
    delay(SERIAL_DELAY);
    #endif

    // Delay to improve button operation
    delay(350);
    
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

    #ifdef DEBUG
    Serial.println("Coming out of idle state...");
    Serial.print("button_pressed=");
    Serial.println(button_pressed);
    Serial.print("millis()=");
    Serial.println(millis());
    delay(SERIAL_DELAY);
    #endif
  } 

  */
}

/*
void pirOnISR() {
  #ifdef DEBUG
  Serial.println("PIR trigger!");
  delay(SERIAL_DELAY);
  #endif

  pir_triggered = true;

  // Bug fix for button somehow getting triggered when either PIR is triggered or spray occurs:
  delay(350);
  button_pressed = false;
}

*/
