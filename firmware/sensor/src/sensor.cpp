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

    delay(3000);

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
    fadeStartTime = millis();
}

void closeFlower() {
    servoTargetPosition = servoClosePos;
    servoStartPosition = servoPosition;
    flowerOpenStartTime = millis();
    fadeStartTime = millis();
}

void updateLEDs() {
    long deltaT = millis() - fadeStartTime;
    if (deltaT <= fadeTime) {
        if (!isFadingInOrOut) {
            isFadingInOrOut = true;
        }

        setOnboardLEDs(
            (ledOpenColorR - ledCloseColorR) * deltaT / fadeTime + ledCloseColorR,
            (ledOpenColorG - ledCloseColorG) * deltaT / fadeTime + ledCloseColorG,
            (ledOpenColorB - ledCloseColorB) * deltaT / fadeTime + ledCloseColorB
        );   
    } else {
        if (isFadingInOrOut) {
            if (overallState == STATE_OPEN) {
                setOnboardLEDs(
                    ledOpenColorR,
                    ledOpenColorG,
                    ledOpenColorB
                );
            } else {
                setOnboardLEDs(
                    ledCloseColorR,
                    ledCloseColorG,
                    ledCloseColorB
                );
            }
            isFadingInOrOut = false;
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


void printProx(){
      delay(100);

        unsigned long total=0, start;
        int i=0;
        red = 0;
        IR1 = 0;
        IR2 = 0;
        total = 0;
        start = millis();



     while (i < samples){ 


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

     #ifdef PRINT_AMBIENT_LIGHT_SAMPLING

        Serial.print(pulse.resp, HEX);     // resp
        Serial.print("\t");
        Serial.print(pulse.als_vis);       //  ambient visible
        Serial.print("\t");
        Serial.print(pulse.als_ir);        //  ambient IR
        Serial.print("\t");
       
     #endif
     
    #ifdef PRINT_RAW_LED_VALUES

        Serial.print(red);
        Serial.print("\t");
        Serial.print(IR1);
        Serial.print("\t");
        Serial.print(IR2);
        Serial.print("\t");
        Serial.println((long)total);    
        Serial.print("\t");      

    #endif                                
                                        

    #ifdef SEND_TO_PROCESSING_SKETCH

        /* Add LED values as vectors - treat each vector as a force vector at
         * 0 deg, 120 deg, 240 deg respectively
         * parse out x and y components of each vector
         * y = sin(angle) * Vect , x = cos(angle) * Vect
         * add vectors then use atan2() to get angle
         * vector quantity from pythagorean theorem
         */

    Avect = IR1;
    Bvect = red;
    Cvect = IR2;

    // cut off reporting if total reported from LED pulses is less than the ambient measurement
    // eliminates noise when no signal is present
    if (total > 900){    //determined empirically, you may need to adjust for your sensor or lighting conditions

        x = (float)Avect -  (.5 * (float)Bvect ) - ( .5 * (float)Cvect); // integer math
        y = (.866 * (float)Bvect) - (.866 * (float)Cvect);

        angle = atan2((float)y, (float)x) * 57.296 + 180 ; // convert to degrees and lose the neg values
        Tvect = (long)sqrt( x * x + y * y); 
    }
    else{  // report 0's if no signal (object) present above sensor
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
        // Serial.println("B");
        breathState = BREATH;
    } else if (abs(bpassWind - maxBpassHistory()) < 3 && breathState == BREATH) {
        HWSERIAL.print("E");
        // Serial.println("E");
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
        pirState = PIR_ON;

        if (pirState == PIR_ON) {
            openTimeoutLastEvent = now;
        }

        lastPIRSampleTime = now;
    }
}

void updateProx() {
    long now = millis();
    if (now - lastUltraSampleTime > ultraSampleInterval) {
        int ultraVal = getUltrasonic();

        ultraHistory[ultraHistoryIndex] = ultraVal;
        ultraHistoryIndex++;
        if (ultraHistoryIndex >= ULTRA_HIST_LEN) ultraHistoryIndex = 0;

        lastUltraSampleTime = now;

        float average = 0;
        for (int i = 0; i < ULTRA_HIST_LEN; i++) {
            average += ultraVal * 1.0f / ULTRA_HIST_LEN;
        }

        if (average < ultraThres) {
            openTimeoutLastEvent = now;
            isProximate = true;
        } else {
            isProximate = false;
        }
    }
}

bool isProx() {
    return isProximate;
}

void evaluateState() {
    switch (overallState) {
        case STATE_NEUTRAL: {
            if (pirState == PIR_ON) {
                overallState = STATE_OPEN;
                HWSERIAL.print("O");
                // Serial.println("O");
                openFlower();
            }
            break;
        }
        case STATE_OPEN: {
            long now = millis();
            if (now - openTimeoutLastEvent > openTimeout) {
                HWSERIAL.print("C");
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
            if (!isProx()) {
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

void loop() {
    // setOnboardLEDs(255, 255, 0);
    // updateLEDs();
    // updateFlowerServo();
    // updatePIR();
    
    // runWindAvgs();
    // updateProx();
    setOnboardLEDs(100, 100, 0);
    delay(100);
    printProx();

    // evaluateState();
}
