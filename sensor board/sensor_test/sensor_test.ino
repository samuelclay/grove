#include <Adafruit_NeoPixel.h>
#include <SI114.h>

#define LEDPIN 8 //The pin that the Neopixels are on
//#define PRINT_AMBIENT_LIGHT_SAMPLING
#define PRINT_RAW_LED_VALUES //prints raw LED values from prox sensor for debug
#define analogPinForRV  1 //Pin for raw voltage from wind sensor
#define analogPinForTMP  2 //Pin for temp from wind sensor

//Prox sense globals//
const int PORT_FOR_SI114 = 0; //I2C on Teensy is port 0
const int samples = 4;
float Avect = 0.0;
float Bvect = 0.0;
float Cvect = 0.0;
float Tvect, x, y, angle = 0;
unsigned long lastMillis, red, IR1, IR2;

//Wind sensor globals
const float zeroWindAdjustment =  .2; // negative numbers yield smaller wind speeds and vice versa.
int TMP_Therm_ADunits;  //temp termistor value from wind sensor
float RV_Wind_ADunits;    //RV output from wind sensor
float RV_Wind_Volts;
int TempCtimes100;
float zeroWind_ADunits;
float zeroWind_volts;
float WindSpeed_MPH;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, LEDPIN, NEO_GRB + NEO_KHZ800); //RGB LED object
PortI2C myBus (PORT_FOR_SI114); //I2C object
PulsePlug pulse (myBus); //for prox sense

void setup() {
  // put your setup code here, to run once:
  strip.begin(); //Initialize RGB LEDs
  strip.show(); // Initialize all pixels to 'off'

  Serial.begin(57600);
  Serial.println("\n[Grove sensor demo]");
  //Check for prox sensor
  if (!pulse.isPresent()) {
    Serial.print("No SI114x found on Port ");
    Serial.println(PORT_FOR_SI114);
  }
  Serial.begin(57600);
  pulse.initPulsePlug(); //Initialize I2C bus
  pulse.setLEDcurrents(6, 6, 6); //set prox sensor LED currents
  pulse.setLEDdrive(1, 2, 4); //set which LEDs are active on which pulse


  //Set up servo output
  pinMode(3, OUTPUT);

  //Setup PIR input
 pinMode(20, INPUT_PULLUP);

}

void loop() {

  ///////////////////////
  //////Prox sensor//////
  ///////////////////////
  delay(100);
  unsigned long total = 0, start;
  int i = 0;
  red = 0;
  IR1 = 0;
  IR2 = 0;
  total = 0;
  start = millis();

  while (i < samples) {
#ifdef PRINT_AMBIENT_LIGHT_SAMPLING
    pulse.fetchData();    // gets ambient readings and LED (pulsed) readings
#else
    pulse.fetchLedData();    // gets just LED (pulsed) readings - bit faster
#endif
    red += pulse.ps1;
    IR1 += pulse.ps2;
    IR2 += pulse.ps3;
    i++;
  }

  red = red / i;  // get averages
  IR1 = IR1 / i;
  IR2 = IR2 / i;
  total = red + IR1 + IR2;
  uint32_t color = (min(total, 300000000) - 100000000) / 100000000 * 255;
  strip.setPixelColor(1, color);
  strip.show();

#ifdef PRINT_AMBIENT_LIGHT_SAMPLING
  Serial.print(pulse.resp, HEX);     // resp
  Serial.print(" ");
  Serial.print(pulse.als_vis);       //  ambient visible
  Serial.print(" ");
  Serial.print(pulse.als_ir);        //  ambient IR
  Serial.print(" ");
#endif

#ifdef PRINT_RAW_LED_VALUES
  Serial.print(red);
  Serial.print(" ");
  Serial.print(IR1);
  Serial.print(" ");
  Serial.print(IR2);
  Serial.print(" ");
  Serial.print((long)total);
  Serial.println(" ");
#endif

#ifdef SEND_TO_PROCESSING_SKETCH
  /* Add LED values as vectors - treat each vector as a force vector at
     0 deg, 120 deg, 240 deg respectively
     parse out x and y components of each vector
     y = sin(angle) * Vect , x = cos(angle) * Vect
     add vectors then use atan2() to get angle
     vector quantity from pythagorean theorem
  */

  Avect = IR1;
  Bvect = red;
  Cvect = IR2;

  // cut off reporting if total reported from LED pulses is less than the ambient measurement
  // eliminates noise when no signal is present
  if (total > 900) {   //determined empirically, you may need to adjust for your sensor or lighting conditions
    x = (float)Avect -  (.5 * (float)Bvect ) - ( .5 * (float)Cvect); // integer math
    y = (.866 * (float)Bvect) - (.866 * (float)Cvect);
    angle = atan2((float)y, (float)x) * 57.296 + 180 ; // convert to degrees and lose the neg values
    Tvect = (long)sqrt( x * x + y * y);
  }
  else { // report 0's if no signal (object) present above sensor
    angle = 0;
    Tvect = 0;
    total =  900;
  }

  // angle is the resolved angle from vector addition of the three LED values
  // Tvect is the vector amount from vector addition of the three LED values
  // Basically a ratio of differences of LED values to each other
  // total is just the total of raw LED amounts returned, proportional to the distance of objects from the sensor.

  Serial.print(angle);
  Serial.print("\t");
  Serial.print(Tvect);
  Serial.print("\t");
  Serial.println(total);

#endif

  delay(10);

  ///////////////////////
  /////Wind Sensor///////
  ///////////////////////
  TMP_Therm_ADunits = analogRead(analogPinForTMP);
  RV_Wind_ADunits = analogRead(analogPinForRV);
  RV_Wind_Volts = (RV_Wind_ADunits *  0.0048828125);

  // these are all derived from regressions from raw data as such they depend on a lot of experimental factors
  // such as accuracy of temp sensors, and voltage at the actual wind sensor, (wire losses) which were unaccouted for.
  TempCtimes100 = (0.005 * ((float)TMP_Therm_ADunits * (float)TMP_Therm_ADunits)) - (16.862 * (float)TMP_Therm_ADunits) + 9075.4;

  zeroWind_ADunits = -0.0006 * ((float)TMP_Therm_ADunits * (float)TMP_Therm_ADunits) + 1.0727 * (float)TMP_Therm_ADunits + 47.172; //  13.0C  553  482.39

  zeroWind_volts = (zeroWind_ADunits * 0.0048828125) - zeroWindAdjustment;

  // This from a regression from data in the form of
  // Vraw = V0 + b * WindSpeed ^ c
  // V0 is zero wind at a particular temperature
  // The constants b and c were determined by some Excel wrangling with the solver.

  WindSpeed_MPH =  pow(((RV_Wind_Volts - zeroWind_volts) / .2300) , 2.7265);

  //Serial.print("  TMP volts ");
  //Serial.print(TMP_Therm_ADunits * 0.0048828125);

  //Serial.print(" RV volts ");
  //Serial.print((float)RV_Wind_Volts);

  //Serial.print("\t  TempC*100 ");
  //Serial.print(TempCtimes100 );

  //Serial.print("   ZeroWind volts ");
  //Serial.print(zeroWind_volts);

//  Serial.print("   WindSpeed MPH ");
//  Serial.println((float)WindSpeed_MPH);
  uint32_t range = 250 - 150;
  uint32_t windColor = (WindSpeed_MPH - 150) / range * 255;
  strip.setPixelColor(0, min(windColor, 255));
  strip.show();



  ///////////////////
  //////PWM TEST/////
  ///////////////////
  analogWrite(3, 127); //Write Pin 3 (PWM out for servo) to be 50% duty cycle square wave


  ////////////////////
  //////PIR TEST//////
  ////////////////////
  int alarm = digitalRead(20);
  if (alarm == LOW) {
//    Serial.print("PIR is LOW");
  } else {
//    Serial.print("PIR is HIGH");
  }

  //rainbow(.1); //Run a cool light show
  //rainbowCycle(20); //Another cool light show
}


// simple smoothing function for future heartbeat detection and processing
float smooth(float data, float filterVal, float smoothedVal) {

  if (filterVal > 1) {     // check to make sure param's are within range
    filterVal = .999;
  }
  else if (filterVal <= 0.0) {
    filterVal = 0.001;
  }

  smoothedVal = (data * (1.0 - filterVal)) + (smoothedVal  *  filterVal);
  return smoothedVal;
}


void rainbow(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256; j++) {
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
