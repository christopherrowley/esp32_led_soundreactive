#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ALLOW_INTERRUPTS 0

#include "reactive_common.h" // File with functions that will be common between all lamps. All display stuff will be moved here.
#include <FastLED.h> // Code for LEDs
#include <WiFi.h>
#include <WiFiUdp.h>  // Required for UDP?


/* initialize Variables */
#define LED_PIN               4 // Where LED strip is connected to board
#define NUM_LEDS             60
#define MIC_LOW              15
#define MIC_HIGH            255

#define SAMPLE_SIZE           4 // Size to sample for music loudness
#define LONG_TERM_SAMPLES    20 // Long term reference
#define BUFFER_DEVIATION    400
#define BUFFER_SIZE           3

/* setup memory block for the leds */
CRGB leds[NUM_LEDS];

struct averageCounter *samples;
struct averageCounter *longTermSamples;
struct averageCounter* sanityBuffer;

float globalHue;
float globalBrightness = 200;
float hueLedLngth = 0;
float fadeScale = 1.3;
float hueIncrement = 20; // 20% change in sound, shifts colour wheel by 20%

// Wifi Stuff
uint8_t buffer[50] = "0"; // will hold incoming values
const char * ssid = "sound_reactive";
const char * pass = "led4thawin";

const char * udpAddress = "192.168.4.30";
const int udpPort = 2390;      // local port to listen on
IPAddress local_IP(192, 168, 4, 30);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 0, 0);

char packetBuffer[255]; //buffer to hold incoming packet
char  ReplyBuffer[] = "recieved";       // a string to send back

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

  /* WiFi Part */
  Serial.begin(9600);
  connectToWiFi(ssid, pass);
  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  udp.begin(udpPort);

  /* Setup Pin for LED data export */
  //pinMode(READ_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
}


void loop() {

  int packetSize = udp.parsePacket();
  uint8_t cmd;
  if (packetSize)
  {
    udp.read((uint8_t *)&cmd, packetSize);
    //Serial.print("Server to client: ");
    //Serial.println(cmd);
  }

  int analogRaw = (int)cmd;
   //Serial.println(analogRaw); // C.R. for debugging...
   

    soundReactive(analogRaw);

  delay(5); // account for extra calc time in master for fft
}


void connectToWiFi(const char * ssid, const char * pwd){
  Serial.println("Connecting to WiFi network: " + String(ssid));

  // delete old config
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }

  //register event handler
  WiFi.onEvent(WiFiEvent);

  //Initiate connection
  WiFi.begin(ssid, pwd);

  Serial.println("Waiting for WIFI connection...");
}

//wifi event handler
void WiFiEvent(WiFiEvent_t event){
    switch(event) {
      case SYSTEM_EVENT_STA_GOT_IP:
          //When connected set
          Serial.print("WiFi connected! IP address: ");
          Serial.println(WiFi.localIP());
          //initializes the UDP state
          //This initializes the transfer buffer
          udp.begin(WiFi.localIP(),udpPort);
          connected = true;
          break;
      case SYSTEM_EVENT_STA_DISCONNECTED:
          Serial.println("WiFi lost connection");
          connected = false;
          break;
      default: break;
    }
}

void soundReactive(int analogRaw) {

 //int sanityValue = sanityBuffer->computeAverage(); // C.R. seems to be for debugging

 // C.R. this is used to logarithmically scale the intensity, because sound is interpretted that way
 // Also adjust based on microphone setting. Maybe I will be able to hard code this into reading from bluetooth?
  analogRaw = fscale(MIC_LOW, MIC_HIGH, MIC_LOW, MIC_HIGH, analogRaw, 0.4);

  //Serial.println(analogRaw); // C.R. for debugging...

  if (samples->setSample(analogRaw))
    return;

  int longTermAverage = longTermSamples->computeAverage(); // C.R. average over a specificied time
  int useVal = samples->computeAverage();                  // C.R. average over a smaller time, currently 1/10th the time
  longTermSamples->setSample(useVal);

  //////////////////////////////////////////////////////////
  // Here we might want a flash of white create a flag
  int diff = (useVal - longTermAverage);
  if (diff > 100)  // If the difference between the two is large enough, change the colour. C.R. I may increase this value...
  {
    globalHue += hueIncrement;
    if (globalHue > 255)
    {
       globalHue -= 255;
    }
  }

  /* Calculate how many LED's to light up */
  int curshow = fscale(MIC_LOW, MIC_HIGH, 0.0, (float)NUM_LEDS, (float)useVal, 0);

  /* Set the colour of lights and turn some off */
  // Using a reverse fire colour - red - orange yellow white. 
  // For HSV, = 0 to 64ish, so just use NUM_LEDS assuming 60.
  for (int i = 0; i < NUM_LEDS; i++)
  {
    if (i < curshow)  {
      leds[i] = CHSV(i, 255-4*i, 255); // dimmer as you go up
    }
    else    {
      leds[i] = CRGB(leds[i].r / fadeScale, leds[i].g / fadeScale, leds[i].b / fadeScale);
    }
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
