/* TODO 
 *  
 *  Code:

 *  
 *  
 *  
 *  
 *  Notes:
 *  - D0 cannot be used as an interrupt pin
 *  - Don't use D9 & up (used for serial communication)
 *  
 */


#define DEBUG 1
#define SERIAL_DELAY 200

const uint8_t pirPin = D4;        
const uint8_t sprayerPin = D2; 
const uint8_t LED1Pin = D0;
const uint8_t LED2Pin = D1;
const uint8_t buttonPin = D3;     
const uint8_t enabledLEDPin = LED_BUILTIN;

//const period_t sleepIterationLength = SLEEP_8S;
const int sprayDuration = 250;   // in milliseconds
//const int disabledIterationInterval = 36; // in 8 second "chunks" 
const int buttonDelay = 350; //in milliseconds

//volatile bool button_pressed;
//volatile bool pir_triggered;
int LEDOnCount;
//int remainingDisabledIterations;
//bool secondInterval;
//bool okToIdle;
//unsigned long int lastButtonPress;


void setup() {
  #ifdef DEBUG
  Serial.begin(115200);
  #endif

  // Set up pins that aren't inputs
  pinMode(sprayerPin, OUTPUT);
  pinMode(LED1Pin, OUTPUT);
  pinMode(LED2Pin, OUTPUT);

  // Flash external LEDs to indicate power on
  for(int i = 0; i < 2; i++) {
    digitalWrite(LED1Pin, HIGH);
    digitalWrite(LED2Pin, LOW);
    delay(250);
    digitalWrite(LED2Pin, HIGH);
    digitalWrite(LED1Pin, LOW);
    delay(250);
  }

  // Initialize flags, etc.
  //pir_triggered = false;
  LEDOnCount = 0;
  //button_pressed = false;
  //secondInterval = false;
  //okToIdle = false;
  //lastButtonPress = millis();

  // Start off disabled for 2 iterations ~= 16 seconds
  //remainingDisabledIterations = 2;  

  // Initialize pin states
  digitalWrite(pirPin, LOW);   //per HC-SR505 datasheet
  digitalWrite(sprayerPin, LOW);
  digitalWrite(LED1Pin, LOW);
  digitalWrite(LED2Pin, LOW);
  digitalWrite(buttonPin, LOW);
  digitalWrite(enabledLEDPin, LOW);

  // Attach interrupts
  //attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, RISING);
  //attachInterrupt(digitalPinToInterrupt(pirPin), pirOnISR, RISING);
  
  Serial.println(enabledLEDPin);
}

void loop() {
  
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
  
  /* Button handler -- button_pressed set to true in buttonISR() 
  if (button_pressed) {

    button_pressed = false;

    // Prevent idle state
    okToIdle = false;
    
    // Case 0: No LEDs lit
    if (LEDOnCount == 0) {
      // Turn on LED1
      digitalWrite(LED1Pin, HIGH);
  
      LEDOnCount = 1;
      
      // Set "remainingDisabledIterations" to the disabledIterationsInteveral (acts as a flag and a counter)
      remainingDisabledIterations = disabledIterationInterval;

    }
  
    // Case 1: Only LED1 is lit; restart disabled mode with double the iteration length:
    else if (LEDOnCount == 1) {    
      // Turn on LED2
      digitalWrite(LED2Pin, HIGH);
      
      // Now 2 LEDs are on
      LEDOnCount = 2;
  
      secondInterval = true;

      // Reset the iterations amount
      remainingDisabledIterations = disabledIterationInterval;

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
      remainingDisabledIterations = 1;  // The "1" is for a delay after coming out of disabled mode
      secondInterval = false;      
    }

    // Don't want to run rest of code (avoids possible spray immediately after disable cancellation).
    return;
  }

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

void buttonISR() {
  // Wait for the button to be released
  //while (digitalRead(buttonPin) == HIGH);
  
  //if(millis() - lastButtonPress > 350) {
    //lastButtonPress = millis();
    
    #ifdef DEBUG 
    Serial.println("Button input!");
    delay(SERIAL_DELAY);
    #endif
    
    //delay(buttonDelay);
    button_pressed = true;
  //}
}
*/
