#include "sensor.h"

void setup() { 
    Serial.begin(9600);
    HWSERIAL.begin(9600);

    leds.begin();
    leds.show();

    pinMode(PIR_PIN, INPUT_PULLUP);
    pinMode(SERVO_PIN, OUTPUT);

    servo.attach(SERVO_PIN); //set up the servo on pin 3
    servo.write(servoOpenPos); //open flower to start
    servo.detach();

#ifdef USE_IR_PROX
    if (!pulse.isPresent()) {
      Serial.print("No SI114x found on port ");
      Serial.println(SI114_PORT);
    } else {
        Serial.println("setup");
    }
    pulse.initPulsePlug(); //Initialize I2C bus
    pulse.setLEDcurrents(6, 6, 6); //set prox sensor LED currents
    pulse.setLEDdrive(4, 2, 1); //set which LEDs are active on which pulse
#endif

    closeFlower();
}

void setOnboardLEDs(uint8_t rValue, uint8_t gValue, uint8_t bValue) {
    for (int i = 0; i < 60; i++) {
        leds.setPixelColor(0, rValue, gValue, bValue);
        leds.setPixelColor(1, rValue, gValue, bValue);
    }
    leds.show();
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
    flowerOpenStartTime = millis();

    ledStartColorR = ledCloseColorR;
    ledStartColorG = ledCloseColorG;
    ledStartColorB = ledCloseColorB;

    ledEndColorR = ledOpenColorR;
    ledEndColorG = ledOpenColorG;
    ledEndColorB = ledOpenColorB;

    fadeStartTime = millis();
}

void closeFlower() {
    servoTargetPosition = servoClosePos;
    servoStartPosition = servoPosition;
    flowerOpenStartTime = millis();

    ledStartColorR = ledOpenColorR;
    ledStartColorG = ledOpenColorG;
    ledStartColorB = ledOpenColorB;

    ledEndColorR = ledCloseColorR;
    ledEndColorG = ledCloseColorG;
    ledEndColorB = ledCloseColorB;

    fadeStartTime = millis();
}

void updateLEDs() {
    long now = millis();
    if (now - lastLedUpdateTime > ledUpdateInterval) {
        lastLedUpdateTime = now;
        long deltaT = now - fadeStartTime;
        if (deltaT <= fadeTime) {
            setOnboardLEDs(
                (ledEndColorR - ledStartColorR) * deltaT / fadeTime + ledStartColorR,
                (ledEndColorG - ledStartColorG) * deltaT / fadeTime + ledStartColorG,
                (ledEndColorB - ledStartColorB) * deltaT / fadeTime + ledStartColorB
            );   
        } else {
            if (overallState == STATE_NEUTRAL) {
                setOnboardLEDs(
                    ledCloseColorR,
                    ledCloseColorG,
                    ledCloseColorB
                );
            } else {
                setOnboardLEDs(
                    ledOpenColorR,
                    ledOpenColorG,
                    ledOpenColorB
                );
            }
        }
    }
}

void updateFlowerServo() {
    long deltaT = millis() - flowerOpenStartTime;
    if (deltaT <= flowerOpenTime) {
        if (!isOpenOrClosing) {
            servo.attach(SERVO_PIN); //set up the servo on pin 3
            isOpenOrClosing = true;
        }
        servoPosition = (servoTargetPosition - servoStartPosition) * deltaT / flowerOpenTime + servoStartPosition;
        servo.write(servoPosition); //Close flower to start
    } else {
        if (isOpenOrClosing) {
            servo.detach();
            isOpenOrClosing = false;
        }   
    }
}

const int samples = 4;            // samples for smoothing 1 to 10 seem useful
                                  // increase for smoother waveform (with less resolution) 
float Avect = 0.0; 
float Bvect = 0.0;
float Cvect = 0.0;
float Tvect, x, y, angle = 0;


// some printing options for experimentation (sketch is about the same)
//#define SEND_TO_PROCESSING_SKETCH
#define PRINT_RAW_LED_VALUES   // prints Raw LED values for debug or experimenting
// #define PRINT_AMBIENT_LIGHT_SAMPLING   // also samples ambient slight (slightly slower)
                                          // good for ambient light experiments, comparing output with ambient
 
unsigned long lastMillis, red, IR1, IR2;


void printProx() {         
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

void runWindAvgs() {
    long now = millis();
    if (now - lastWindSampleTime > windSampleInterval) {
        // Currently tuned to sample every 20ms roughly
        int raw = getRawWind();

        lowWindAvg = lowWindAvg * lowWindAvgFactor + raw * (1 - lowWindAvgFactor);
        highWindAvg = highWindAvg * highWindAvgFactor + raw * (1 - highWindAvgFactor);

        lastBpassWind = bpassWind;
        bpassWind = (int)(highWindAvg-lowWindAvg);

        windHistory[windHistoryIndex] = bpassWind - lastBpassWind;
        windHistoryIndex++;

        if (windHistoryIndex >= WIND_HIST_LEN) windHistoryIndex = 0;

        lastWindSampleTime = now;
    }
}

int maxBpassHistory() {
    int maxVal = -1000000;
    for (int i = 0; i < WIND_HIST_LEN; i++) {
        if (windHistory[i] > maxVal) maxVal = windHistory[i];
    }
    return maxVal;
}

void runBreathDetection() {
    int diffSum = 0;
    for (int i = 0; i < WIND_HIST_LEN; i++) {
        diffSum += windHistory[i];
    }

    if (diffSum < -10 && breathState == REST) {
        HWSERIAL.print("B");
        // Serial.println("B");
        breathState = BREATH;
    } else if (diffSum > -2 && breathState == BREATH) {
        HWSERIAL.print("E");
        // Serial.printintln("E");
        breathState = REST;
    }
}

void updatePIR() {
    long now = millis();
    if (now < 5000) return; // Let the PIR setup
    if (now - lastPIRSampleTime > pirSampleInterval) {
        int value = digitalRead(PIR_PIN);

        pirHistory[pirHistoryIndex] = value;
        pirHistoryIndex++;
        if (pirHistoryIndex >= PIR_HIST_LEN) pirHistoryIndex = 0;

        PirState newState = PIR_ON;

        for (int i = 0; i < PIR_HIST_LEN; i++) {
            if (pirHistory[i] == 1) {
                newState = PIR_OFF;
                break;
            }
        }

        pirState = newState;

        if (pirState == PIR_ON) {
            openTimeoutLastEvent = now;
        }

        lastPIRSampleTime = now;
    }
}

void updateProx() {
    long now = millis();
    if (now - lastUltraSampleTime > ultraSampleInterval) {
        int ultraVal = (int)ultrasonic.Ranging(CM);

        ultraHistory[ultraHistoryIndex] = ultraVal;
        ultraHistoryIndex++;
        if (ultraHistoryIndex >= ULTRA_HIST_LEN) ultraHistoryIndex = 0;

        lastUltraSampleTime = now;

        // Serial.println(ultraVal);

        isProximate = false;
        for (int i = 0; i < ULTRA_HIST_LEN; i++) {
            if (ultraHistory[i] < ultraThres) {
                isProximate = true;
                break;
            }
        }
    }
}

bool isProx() {
    return isProximate;
}

void evaluateState() {
    switch (overallState) {
        case STATE_NEUTRAL: {
            // if (pirState == PIR_ON) {
            if (remoteState == STATE_OPEN) {
                overallState = STATE_OPEN;
                // HWSERIAL.print("O");
                // Serial.println("O");
                openFlower();
            }
            break;
        }
        case STATE_OPEN: {
            // long now = millis();
            // if (now - openTimeoutLastEvent > openTimeout) {
            if (remoteState == STATE_NEUTRAL) {
                // HWSERIAL.print("C");
                // Serial.println("C");
                overallState = STATE_NEUTRAL;
                closeFlower();
            } else if (isProx()) {
                overallState = STATE_PROX;
                HWSERIAL.print("P");
                // Serial.println("P");
            }
            break;
        }
        case STATE_PROX : {
            if (remoteState == STATE_NEUTRAL) {
                // HWSERIAL.print("C");
                // Serial.println("C");
                overallState = STATE_NEUTRAL;
                closeFlower();
            } else if (!isProx()) {
                overallState = STATE_OPEN;
                HWSERIAL.print("F");
                // Serial.println("F");
            } else {
                runBreathDetection();
            }
            break;
        }
    }

}

void readRemoteState() {
    if (HWSERIAL.available() > 0) {
        int incomingByte;
        incomingByte = HWSERIAL.read();
        // Serial.println(incomingByte);
        
        if (incomingByte == 'O') {
            remoteState = STATE_OPEN;
        } else if (incomingByte == 'C') {
            remoteState = STATE_NEUTRAL;
        }
    }
}

void loop() {
    readRemoteState();
    updateLEDs();
    updateFlowerServo();
    // updatePIR();
    
    runWindAvgs();
    updateProx();

    evaluateState();

    // if (millis() % 2000 < 1000) {
    //     runBreathDetection();
    // } else {
    //     Serial.print(runningAvg);
    //     Serial.print("\t");
    //     Serial.println(pirState);
    // }

}
