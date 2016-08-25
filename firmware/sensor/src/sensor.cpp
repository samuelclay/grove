#include "sensor.h"

float lowAvgFactor = 0.9;
float lowAvg = 700;
float highAvgFactor = 0.98;
float highAvg = 700;

void setup() { 
    Serial.begin(9600);

    leds.begin();
    leds.show();

    pinMode(PIR_PIN, INPUT_PULLUP);
    pinMode(SERVO_PIN, OUTPUT);
    servo.attach(SERVO_PIN); //set up the servo on pin 3
    servo.write(servoClosePos); //Close flower to start

#ifdef USE_IR_PROX
    if (!pulse.isPresent()) {
      Serial.print("No SI114x found on pin ");
      Serial.println(SI114_PIN);
    }
    pulse.initPulsePlug(); //Initialize I2C bus
    pulse.setLEDcurrents(7, 7, 7); //set prox sensor LED currents
    pulse.setLEDdrive(1, 2, 4); //set which LEDs are active on which pulse
#endif
}

int getRawWind() {
    return analogRead(RAW_VOLT_ANALOG_WING_SENSOR_PIN);
}

int getRawWindTemp() {
    return analogRead(TEMPERATURE_ANALOG_WIND_SENSOR_PIN);
}


void printProx(){
    pulse.fetchLedData();

    int red = pulse.ps1;
    int IR1 = pulse.ps2;
    int IR2 = pulse.ps3;
    int total = red + IR1 + IR2;

    Serial.print(red);
    Serial.print("\t");
    Serial.print(IR1);
    Serial.print("\t");
    Serial.print(IR2);
    Serial.print("\t");
    Serial.println((long)total);    
    Serial.print("\t");
}

float getWind() {
    // This code is from Modern Devices

    //Wind sensor globals
    const float zeroWindAdjustment =  .2; // negative numbers yield smaller wind speeds and vice versa.
    int TMP_Therm_ADunits;  //temp termistor value from wind sensor
    float RV_Wind_ADunits;    //RV output from wind sensor
    float RV_Wind_Volts;
    int TempCtimes100;
    float zeroWind_ADunits;
    float zeroWind_volts;
    float WindSpeed_MPH;

    TMP_Therm_ADunits = analogRead(TEMPERATURE_ANALOG_WIND_SENSOR_PIN);
    RV_Wind_ADunits = analogRead(RAW_VOLT_ANALOG_WING_SENSOR_PIN);
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

    return WindSpeed_MPH;
}

int getUltrasonic(){
    return analogRead(ULTRASONIC_ANALOG_PIN);
}

void setOnboardLEDs(uint8_t rValue, uint8_t gValue, uint8_t bValue) {
    for (int i = 0; i < 60; i++) {
        leds.setPixelColor(0, rValue, gValue, bValue);
        leds.setPixelColor(1, rValue, gValue, bValue);
    }
    leds.show();
}

void runWindAvgs() {
    int raw = getRawWind();

    lowAvg = lowAvg * lowAvgFactor + raw * (1 - lowAvgFactor);
    highAvg = highAvg * highAvgFactor + raw * (1 - highAvgFactor);
}

void loop() {
    // printProx();
    setOnboardLEDs(255, 255, 0);
    // Serial.println(getUltrasonic(), DEC);
    servo.write(servoClosePos); //Close flower to start
    delay(3000);
    servo.write(servoOpenPos);
    delay(3000);

    // runWindAvgs();

    // Serial.println((int)(highAvg-lowAvg), DEC);

    // delay(20);
}
