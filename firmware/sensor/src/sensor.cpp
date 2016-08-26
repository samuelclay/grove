#include "sensor.h"

void setup() { 
    Serial.begin(9600);
    HWSERIAL.begin(9600);

    leds.begin();
    leds.show();

    pinMode(PIR_PIN, INPUT_PULLUP);
    pinMode(SERVO_PIN, OUTPUT);

    servo.attach(SERVO_PIN); //set up the servo on pin 3
    servo.write(servoClosePos); //Close flower to start
    servo.detach();

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

void openFlower() {
    servoTargetPosition = servoOpenPos;
    servoStartPosition = servoPosition;
    servoMoveStartTime = millis();
}

void closeFlower() {
    servoTargetPosition = servoClosePos;
    servoStartPosition = servoPosition;
    servoMoveStartTime = millis();
}

void updateFlowerServo() {
    long deltaT = millis() - servoMoveStartTime;
    if (deltaT <= servoMoveTime) {
        if (!servoAttached) {
            servo.attach(SERVO_PIN); //set up the servo on pin 3
            servoAttached = true;
        }
        servoPosition = (servoTargetPosition - servoStartPosition) * deltaT / servoMoveTime + servoStartPosition;
        servo.write(servoPosition); //Close flower to start
    } else {
        if (servoAttached) {
            servo.detach();
            servoAttached = false;
        }   
    }
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
    long now = millis();
    if (now - lastWindSampleTime > 20) {
        // Currently tuned to sample every 20ms roughly
        int raw = getRawWind();

        lowWindAvg = lowWindAvg * lowWindAvgFactor + raw * (1 - lowWindAvgFactor);
        highWindAvg = highWindAvg * highWindAvgFactor + raw * (1 - highWindAvgFactor);

        bpassWind = (int)(highWindAvg-lowWindAvg);
        bpassHistory[bpassHistoryIndex] = bpassWind;
        bpassHistoryIndex++;
        if (bpassHistoryIndex >= BPASS_HIST_LEN) bpassHistoryIndex = 0;

        lastWindSampleTime = now;
    }
}

int maxBpassHistory() {
    int maxVal = -1000000;
    for (int i = 0; i < BPASS_HIST_LEN; i++) {
        if (bpassHistory[i] > maxVal) maxVal = bpassHistory[i];
    }
    return maxVal;
}

void runBreathDetection() {
    if (abs(bpassWind - maxBpassHistory()) > 6 && breathState == REST) {
        HWSERIAL.print("B");
        breathState = BREATH;
    } else if (abs(bpassWind - maxBpassHistory()) < 3 && breathState == BREATH) {
        HWSERIAL.print("E");
        breathState = REST;
    }
}

void updatePIR() {
    long now = millis();
    if (now - lastPIRSampleTime > 20) {
        int value = digitalRead(PIR_PIN);

        // pirHistory[pirHistoryIndex] = value;
        // pirHistoryIndex++;
        // if (pirHistoryIndex >= PIR_HIST_LEN) pirHistoryIndex = 0;

        PirState newPirState = value == 0 ? PIR_ON : PIR_OFF;
        // for (int i = 0; i < PIR_HIST_LEN; i++) {
        //     if (pirHistory[i] != 0) {
        //         newPirState = PIR_OFF;
        //         break;
        //     }
        // }

        if (newPirState != pirState) {
            pirState = newPirState;
            switch (pirState) {
                case PIR_ON: {
                    // Serial.println("PIR trig");
                    break;
                }
                case PIR_OFF: {
                    // Serial.println("PIR quiet");
                    break;
                }
            }
        }

        lastPIRSampleTime = now;
    }
}

void loop() {
    // setOnboardLEDs(255, 255, 0);
    updateFlowerServo();
    runWindAvgs();
    runBreathDetection();
    updatePIR();
}
