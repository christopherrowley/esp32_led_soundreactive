#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ALLOW_INTERRUPTS 0

#include <FastLED.h> // Code for LEDs

/* initialize Variables */
#define LED_PIN               4 // Where LED strip is connected to board
#define NUM_LEDS             60


/* setup memory block for the leds */
CRGB leds[NUM_LEDS];


float globalHue;
int globalBrightness = 200; // max value causes it to freeze. 
float hueIncrement = 0.1; // change to 0 if you want no change
int whiteSpacing = 20; // add some effect to the lights
int modCheck = 0;

void setup() {
  globalHue = 0.0;

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);


  Serial.println("Starting up...");  
 
    
  /* Setup Pin for LED data export */
  //pinMode(READ_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
}


void loop() {

  soundReactive(hueIncrement);

  delay(60); // account for extra calc time in master for fft
}



void soundReactive(float hueIncrement) {

  /* Increment this each call so the white block moves up. */
  modCheck += 1;
  if (modCheck >= whiteSpacing){
    modCheck = 0;
  }
    
     
  /* Set the colour of lights and turn some off */
  for (int i = 0; i < NUM_LEDS; i++)
  {
    if (i%whiteSpacing == modCheck){ 
      leds[i] = CHSV(globalHue, 50, globalBrightness); // Set colours of leds to brighten
    } else {
    leds[i] = CHSV(globalHue, 255, globalBrightness); // Set colours of leds to brighten
    }

    globalHue += hueIncrement; // Shift colour
    if (globalHue > 255){
     globalHue -= 255;
    } 
    
  }

  delay(5);
  FastLED.show();
}
