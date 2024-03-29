#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ALLOW_INTERRUPTS 0

#include "reactive_common.h" // File with functions that will be common between all lamps. All display stuff will be moved here.
#include <FastLED.h> // Code for LEDs
#include <WiFi.h>
#include <WiFiUdp.h>  // Required for UDP
#include <arduinoFFT.h> // Decompose audio signal into spectral freq

/* initialize Variables */
#define READ_PIN             32 // data pin 2 works, 25, 26 and 27 were doing something funny. center appears to be around 368
#define LED_PIN               4 // Where LED strip is connected to board
#define NUM_LEDS             60
#define MIC_LOW              15
#define MIC_HIGH            255

#define SAMPLE_SIZE           4 // Size to sample for music loudness
#define LONG_TERM_SAMPLES    20 // Long term reference
#define BUFFER_DEVIATION    400
#define BUFFER_SIZE           3

// Setup for FFT
#define SAMPLES              16 //Must be a power of 2  for FFT
double vReal[SAMPLES];  // CR.spect
double vImag[SAMPLES];
int data_avgs[3];
arduinoFFT FFT = arduinoFFT();      // FFT object

/* setup memory block for the leds */
CRGB leds[NUM_LEDS];

struct averageCounter *samples;
struct averageCounter *longTermSamples;
struct averageCounter* sanityBuffer;

float globalHue;
float globalBrightness = 255;
float SatVal = 255;
float hueLedLngth = 0;
int CENTER_VAL  = 1420;// 1820; // The value that ESP32 read_pin takes with no music
float fadeScale = 1.2; // lower values slow the fade.
float hueIncrement = 3; // 20% change in sound, shifts colour wheel by 20%

// Wifi setup
const char * ssid = "sound_reactive";
const char * pass = "led4thawin";

IPAddress local_IP(192, 168, 4, 20);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 0, 0);

//IP address to send UDP data to:
// either use the ip address of the server or
// a network broadcast address
const char * udpAddress = "192.168.4.20";
const int udpPort = 2390;
const char * udpClientAdd = "192.168.4.30";
//Are we currently connected?
boolean connected = false;

WiFiUDP udp;

void setup() {

  globalHue = 0;
  samples = new averageCounter(SAMPLE_SIZE);
  longTermSamples = new averageCounter(LONG_TERM_SAMPLES);
  sanityBuffer    = new averageCounter(BUFFER_SIZE);

  while(sanityBuffer->setSample(250) == true) {}
  while (longTermSamples->setSample(200) == true) {}

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);

  Serial.begin(9600);
  setupWifi(ssid, pass);

  /* Setup Pin for LED data export */
  pinMode(READ_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  delay(50);            // wait to get reference voltage stabilized
}

void loop() {

    int analogRaw;
    analogReadResolution(32);
    analogRaw =analogRead(READ_PIN);

    // ++ Sampling
    for(int i=0; i<SAMPLES; i++)
     {
       vReal[i]= analogRaw - CENTER_VAL;                      // Copy to bins after compressing
       vImag[i] = 0;
     }
     // -- Sampling

     // ++ FFT
     FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
     FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
     FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);
     // -- FFT

     // ++ re-arrange FFT result to match with no. of columns on display ( xres )
     int step = 1;
     int c=0;

     //for(int i=0; i<(SAMPLES/2); i+=step)
     for(int i=0; i<3; i+=step) //C.R. shrink because we only care about small ones
     {
       data_avgs[c] = 0;
       for (int k=0 ; k< step ; k++) {
           data_avgs[c] = data_avgs[c] + vReal[i+k];
       }
       data_avgs[c] = data_avgs[c]/step;
       c++;
     }


     // -- re-arrange FFT result to match with no. of columns on display ( xres )
    int vol = data_avgs[2];
    vol = map(vol, 30, 110, 0, 255); // C.r. seems to be some floating number.
    vol = constrain(vol,0,255); // C.R. limit for sending to slave

    //Serial.println(vol); // C.R. for debugging...

    uint8_t analogRaw8 = vol; // C.R.convert for sending via UDP

    //C.R. send the data to the other lamp
    udp.beginPacket(udpClientAdd, udpPort);
    udp.write(analogRaw8);
    udp.endPacket();

    soundReactive(analogRaw8); // C.R. commented out for debugging bluetooth...

}

void setupWifi(const char * ssid, const char * pwd){

  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(ssid, pwd) ? "Ready" : "Failed!");

  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());

}

void soundReactive(int analogRaw) {

 // C.R. this is used to logarithmically scale the intensity, because sound is interpretted that way
  analogRaw = fscale(MIC_LOW, MIC_HIGH, MIC_LOW, MIC_HIGH, analogRaw, 0.4);

  if (samples->setSample(analogRaw))
    return;

  int longTermAverage = longTermSamples->computeAverage(); // C.R. average over a specificied time // was uint16_t
  int useVal = samples->computeAverage();                  // C.R. average over a smaller time, currently 1/10th the time
  longTermSamples->setSample(useVal);

 
  // Start of Deciding colour of lights
  // Slowly cycle through colours, and with big changes shift towards white

  int diff = (useVal - longTermAverage);
  if (diff > 120)  // If the difference between the two is large enough, change the colour. C.R. I may increase this value...
  {
    globalHue += hueIncrement; // Shift colour

    SatVal -= 120; // White moves closer to 0
    
    if (globalHue > 255)
    {
       globalHue -= 255;
    } 
    if (SatVal < 0)
    {
       SatVal = 0;
    } 
  } else if (SatVal < 255) // start to degrade the saturation value
  {
    SatVal += 5;
    if (SatVal > 255)
    {
       SatVal = 255;
    } 
  }
  
  Serial.println(SatVal); // C.R. for debugging...

  /* Calculate how many LED's to light up */
  int curshow = fscale(MIC_LOW, MIC_HIGH, 0.0, (float)NUM_LEDS, (float)useVal, 0);

  /* Set the colour of lights and turn some off */
  for (int i = 0; i < NUM_LEDS; i++)
  {
    if (i <= curshow){
      leds[i] = CHSV(globalHue, SatVal, 255); // Set colours of leds to brighten
    }
    // Then fade the rest
    leds[i] = CRGB(leds[i].r / fadeScale, leds[i].g / fadeScale, leds[i].b / fadeScale);
    
  }
  
  delay(5);
  FastLED.show();
}


float fscale(float originalMin, float originalMax, float newBegin, float newEnd, float inputValue, float curve)
{
  float OriginalRange = 0;
  float NewRange = 0;
  float zeroRefCurVal = 0;
  float normalizedCurVal = 0;
  float rangedValue = 0;
  boolean invFlag = 0;

  // condition curve parameter
  // limit range

  if (curve > 10)
    curve = 10;
  if (curve < -10)
    curve = -10;

  curve = (curve * -.1);  // - invert and scale - this seems more intuitive - postive numbers give more weight to high end on output
  curve = pow(10, curve); // convert linear scale into lograthimic exponent for other pow function

  // Check for out of range inputValues
  if (inputValue < originalMin)
  {
    inputValue = originalMin;
  }
  if (inputValue > originalMax)
  {
    inputValue = originalMax;
  }

  // Zero Refference the values
  OriginalRange = originalMax - originalMin;

  if (newEnd > newBegin)
  {
    NewRange = newEnd - newBegin;
  }
  else
  {
    NewRange = newBegin - newEnd;
    invFlag = 1;
  }

  zeroRefCurVal = inputValue - originalMin;
  normalizedCurVal = zeroRefCurVal / OriginalRange; // normalize to 0 - 1 float

  // Check for originalMin > originalMax  - the math for all other cases i.e. negative numbers seems to work out fine
  if (originalMin > originalMax)
  {
    return 0;
  }

  if (invFlag == 0)
  {
    rangedValue = (pow(normalizedCurVal, curve) * NewRange) + newBegin;
  }
  else // invert the ranges
  {
    rangedValue = newBegin - (pow(normalizedCurVal, curve) * NewRange);
  }

  return rangedValue;
}
