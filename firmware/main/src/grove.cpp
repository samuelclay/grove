#include "grove.h"
#include "WS2812.h"

void setup() { 
    leds.begin();
    leds.show();
    randomSeed(analogRead(0));
    
    breathState = STATE_RESTING;
    proximityState = STATE_PIR_INACTIVE;
    pinMode (slaveSelectPin, OUTPUT);
    pinMode(PIR1_PIN, INPUT_PULLUP);

    SPI.begin(); 

    Serial.begin(9600); // USB is always 12 Mbit/sec
    HWSERIAL.begin(9600);
    
    clearDispatcher();

    return;

    testDispatcherRGB(1);
}

void clearDispatcher() {
    setDispatcherGlobalRGB(0, 0, 0);
}

void setDispatcherGlobalRGB(uint8_t rValue, uint8_t gValue, uint8_t bValue) {
    for (uint8_t c = 0; c <= 4; c++) {
        dispatcher(c*3 + 1, rValue);
        dispatcher(c*3 + 2, gValue);
        dispatcher(c*3 + 3, bValue);
    }
}

void testDispatcherRGB(int delayTime) {
    for (uint8_t cGroup = 0; cGroup < 3; cGroup++) {
        // Raise light
        for (uint8_t i=0; i < 255; i++) {
            setDispatcherGlobalRGB(
                cGroup == 0 ? i : 0,
                cGroup == 1 ? i : 0,
                cGroup == 2 ? i : 0
            );
            delay(delayTime);
        }
        // Lower light
        for (uint8_t i=255; i > 0; i--) {
            setDispatcherGlobalRGB(
                cGroup == 0 ? i : 0,
                cGroup == 1 ? i : 0,
                cGroup == 2 ? i : 0
            );
            delay(delayTime);
        }
    }
}

void loop() {
    addRandomDrip();
    advanceRestDrips();
    
    updatePIR(0);
    updatePIR(1);
    transmitSensor();
    receiveSensor();
    
#if RANDOMBREATHS
    if (millis() % (60 * 1000) < 5000) {
        addBreath();
    }
#else
    if (proximityState == STATE_BREATHING_ACTIVE && detectedBreath) {
        addBreath();
    }
#endif
    advanceBreaths();
    
    runLeaves();
    runBase();
    
    leds.show();    
}

/**
 * Set the analog output on the dispatcher board.
 * Channels are numbered 1 to 12 inclusive (THEY DON'T START AT ZERO !!) 
 * value is 0-255 range analog output of the dac/LED brightness
 */
void dispatcher(uint8_t chan, uint8_t value) {
    // We could normalize here to a "real" range
    // There's nothing below 45 because below 0.5 the pico buck is off
    // value = value * 210 / 255;
    // if (value > 0) value += 45;

    // take the SS pin low to select the chip:
    SPI.beginTransaction(dispatcherSPISettings);

    digitalWrite(slaveSelectPin, LOW);
    //  send in the address and value via SPI:
    SPI.transfer(flipByte((chan & 0x0F) << 4));
    SPI.transfer(value);
    // take the SS pin high to de-select the chip:
    digitalWrite(slaveSelectPin,HIGH);

    SPI.endTransaction();
}

uint8_t flipByte(uint8_t val) {
    uint8_t result = 0;
    for (uint8_t i = 0; i < 8; i++) {
        result |= ((val & ((0x01) << i)) >> i) << (7-i);
    }
    return result;
}

void addRandomDrip() {
    // Wait until drip is far enough away from beginning
    float distanceTraveled = 0;
    int previousLength = 0;
    int d = dripCount % DRIP_LIMIT;
    
    if (dripCount) {
        int latestDripIndex = d - 1;
        if (latestDripIndex < 0) latestDripIndex = DRIP_LIMIT - 1; // wrap around
        int latestDripStart = dripStarts[latestDripIndex];
        // distanceTraveled = (millis() - latestDripStart) / float(REST_DRIP_TRIP_MS);
        distanceTraveled = dripEase[latestDripIndex].easeIn(float(millis() - latestDripStart));
        previousLength = dripWidth[latestDripIndex];
        
        // Don't start a new drip if the latest drip isn't done coming out
        if (distanceTraveled <= previousLength/2+1) {
            Serial.print(" ---> Not starting drip, last not done yet: ");
            Serial.print(distanceTraveled);
            Serial.print(" <= ");
            Serial.println(previousLength/2+1);            
            return;
        } else if (newDripDelayEnd == 0) {
            newDripDelayEnd = millis() + random(REST_DRIP_DELAY_MIN, REST_DRIP_DELAY_MAX);
        }
    }

    Serial.print(" ---> New Drip? (#");
    Serial.print(dripCount);
    Serial.print("): ");
    Serial.print(distanceTraveled);
    Serial.print(" -- ");
    Serial.print(furthestBreathPosition);
    Serial.print(" millis:");
    Serial.print(millis());
    Serial.print(" > ");
    Serial.println(newDripDelayEnd);
    
    if (!dripCount || millis() > newDripDelayEnd) {
        newDripDelayEnd = 0;
        dripStarts[d] = millis();
        dripColors[d] = randomGreen();
        dripWidth[d] = random(REST_DRIP_WIDTH_MIN, REST_DRIP_WIDTH_MAX);
        dripEaten[d] = false;
        dripEase[d].setDuration(float(REST_DRIP_TRIP_MS));
        dripEase[d].setTotalChangeInPosition(float(ledsPerStrip*2));
        dripCount++;
    }
}

bool hasNewRandomBreath() {
#if !RANDOMBREATHS
    return false;
#endif
    
    if (activeBreath != -1) return false;
    
    if (millis() > lastNewBreathMs) {
        Serial.print(" ---> millis() > lastNewBreathMs: ");
        // Serial.print(millis());
        // Serial.print(" > ");
        // Serial.print(lastNewBreathMs);
        // Serial.print(" - ");
        // Serial.println(endActiveBreathMs);
        endActiveBreathMs = millis() + random(500, 1000);
        lastNewBreathMs = endActiveBreathMs + random(800, 1550);
        // Serial.print(" -> (Now) ");
        // Serial.print(lastNewBreathMs);
        // Serial.print(" - ");
        // Serial.println(endActiveBreathMs);
        return true;
    }
    
    // Serial.print(" ---> hasNewBreath is not stale nor running: ");
    // Serial.print(millis());
    // Serial.print(" < ");
    // Serial.print(lastNewBreathMs);
    // Serial.print(" - ");
    // Serial.println(endActiveBreathMs);
    
    return false;
}

bool hasNewBreath() {
    if (activeBreath != -1) return false;
    
    if (detectedBreath) {
        Serial.println(" ---> hasNewBreath has detected a breath");
        return true;
    }
    
    Serial.println(" ---> No detected breath and no inactive breath");
    
    return false;
}

bool hasActiveRandomBreath() {
    if (activeBreath == -1) return false;
    
    if (millis() < endActiveBreathMs) {
        return true;
    }
    
    activeBreath = -1;
    
    return false;
}

bool hasActiveBreath() {
    return detectedBreath;
}

void addBreath() {
    // Serial.print(" -> (Now) ");
    // Serial.print(lastNewBreathMs);
    // Serial.print(" - ");
    // Serial.println(endActiveBreathMs);
    
    int b = breathCount % BREATH_LIMIT;
    bool newBreath = hasNewBreath() || hasNewRandomBreath();
    
    if (newBreath) {
        Serial.print(" ---> New breath: ");
        Serial.println(b);
        breathStarts[b] = millis();
        breathPosition[b] = 0;
        breathWidth[b] = 1;
        breathEase[b].setDuration(float(BREATH_TRIP_MS));
        breathEase[b].setTotalChangeInPosition(float(ledsPerStrip*2));
        breathCount++;
        activeBreath = breathCount % BREATH_LIMIT;
    } else {
        if (hasActiveRandomBreath() || hasActiveBreath()) {
            // Still breathing, so extend breath
            int latestBreathIndex = b - 1;
            if (latestBreathIndex < 0) latestBreathIndex = BREATH_LIMIT - 1; // wrap around
            int latestBreathStart = breathStarts[latestBreathIndex];
            // float progress = float(millis() - latestBreathStart) / float(BREATH_TRIP_MS);
            float progress = breathEase[latestBreathIndex].easeOut(float(millis() - latestBreathStart)) / float(ledsPerStrip);
        
            breathWidth[latestBreathIndex] = ceil(progress * ledsPerStrip);
        
            Serial.print(" ---> Active breath #");
            Serial.print(latestBreathIndex);
            Serial.print(": ");
            Serial.print(breathWidth[latestBreathIndex]);
            Serial.print("(");
            Serial.print(breathCount);
            Serial.println(")");
        } else {
            // Not actively breathing
        }
    }
    
}

void advanceRestDrips() {
    for (int i=0; i <= ledsPerStrip; i++) {
        leds.setPixel(ledsPerStrip*3+i, 0);
    }
    
    for (unsigned int d=0; d < min(dripCount, DRIP_LIMIT); d++) {
        unsigned int dripStart = dripStarts[d];
        // The 2 is to ensure that the tail of each drip disappears, since
        // this only considers the position of the head of the drip.
        // float progress = (millis() - dripStart) / float(REST_DRIP_TRIP_MS);
        float progress = dripEase[d].easeIn(float(millis() - dripStart)) / float(ledsPerStrip);
        if (progress < 0 || dripStart == 0) {
            dripStarts[d] = 0;
            continue;
        }
        if (dripEaten[d]) continue;
        unsigned int dripColor = dripColors[d];

        drawDrip(d, dripStart, dripColor);
    }
}

void advanceBreaths() {
    for (unsigned int b=0; b < min(breathCount, BREATH_LIMIT); b++) {
        unsigned int breathStart = breathStarts[b];
        // float progress = (millis() - breathStart) / float(BREATH_TRIP_MS);
        if (breathStart == 0) {
            continue;
        }
        drawBreath(b, breathStart);
    }
}

void drawDrip(int d, int dripStart, int dripColor) {
    // float progress = (millis() - dripStart) / float(REST_DRIP_TRIP_MS);
    float progress = dripEase[d].easeIn(float(millis() - dripStart));
    int32_t windowHead = 0;
    int32_t windowTail = dripWidth[d];
    double dripHeadPosition = 0;
    double headFractional = modf(progress, &dripHeadPosition);
    double tailFractional = (1 - REST_DRIP_DECAY) * (1 - headFractional);
    int32_t dripTail = dripHeadPosition-dripWidth[d];
    
    int baseColor = dripColor;
    int color;

    windowHead = constrain(dripHeadPosition, 0, ledsPerStrip);
    windowTail = constrain(dripTail, 0, ledsPerStrip);
    
    if (windowHead == windowTail) return;

    if (d == 0) {
        // Serial.print(" ---> Drip #");
        // Serial.print(d);
        // Serial.print(": ");
        // Serial.print(progress);
        // Serial.print(" (");
        // Serial.print(float(millis() - dripStart));
        // Serial.print(") = ");
        // Serial.print(dripHeadPosition);
        // Serial.print("<>");
        // Serial.print(dripTail);
        // Serial.print(". ");
        // Serial.print(ledsPerStrip - dripTail);
        // Serial.print(" < ");
        // Serial.print(furthestBreathPosition);
        // Serial.print(". ");
        // Serial.print(windowHead);
        // Serial.print(" --> ");
        // Serial.println(windowTail);
    }
    for (int32_t i=windowHead; i >= windowTail; i--) {
        // Head and tail pixel is the fractional fader
        // double tailDecay = 1 - REST_DRIP_DECAY*i/tail;
        // Serial.println(i);
        // if (i > ledsPerStrip || i < 0) continue;
        if ((ledsPerStrip - i) <= furthestBreathPosition && furthestBreathPosition != 0) continue;

        int t = millis();
        int positionMultipler = 2;
        int timeMultiplier = 20;
        int sinPos = ((positionMultipler*i) + (t/timeMultiplier)) % 360;
        float tailRange = 0.95f;
        float tailDecay = (sinTable[sinPos] * (tailRange/2)) + (1 - tailRange/2);
        if (i == dripHeadPosition) headFractional *= tailDecay;
        if (i == dripTail) tailFractional *= tailDecay;
        
        color = baseColor;
        uint8_t r = ((color & 0xFF0000) >> 16) * 
            (i == dripHeadPosition ? headFractional : i == dripTail ? tailFractional : tailDecay);
        uint8_t g = ((color & 0x00FF00) >> 8) * 
            (i == dripHeadPosition ? headFractional : i == dripTail ? tailFractional : tailDecay);
        uint8_t b = ((color & 0x0000FF) >> 0) * 
            (i == dripHeadPosition ? headFractional : i == dripTail ? tailFractional : tailDecay);
        color = ((r<<16)&0xFF0000) | ((g<<8)&0x00FF00) | ((b<<0)&0x0000FF);
        
        if (d == 0 && ledsPerStrip - i == 0) {
            // Serial.print(" Setting 0 pixel (drip #");
            // Serial.print(d);
            // Serial.print("): ");
            // Serial.print(color);
            // Serial.print(" --> ");
            // Serial.println(i);
        }
        leds.setPixel(ledsPerStrip*3+(ledsPerStrip - i), color);
    }
    
    if ((ledsPerStrip - dripTail) <= furthestBreathPosition && furthestBreathPosition != 0) {
        dripEaten[d] = true;
    }
    
    if (dripTail > ledsPerStrip*2) {
        dripEaten[d] = true;
    }
}

void drawBreath(int b, int breathStart) {
    // float progress = (millis() - breathStart) / float(BREATH_TRIP_MS);
    float progress = breathEase[b].easeOut(float(millis() - breathStart));
    int head = 0;
    int tail = breathWidth[b];
    double currentLed = 0;
    double headFractional = modf(progress, &currentLed);
    double tailFractional = (1 - BREATH_DECAY) * (1 - headFractional);
    breathPosition[b] = currentLed;
    if (currentLed-tail > ledsPerStrip) {
        furthestBreathPosition = 0;
        breathStarts[b] = 0;
    } else if (currentLed > furthestBreathPosition) {
        furthestBreathPosition = breathPosition[b];
    }
    
    if (currentLed > ledsPerStrip) {
        if (breathState == STATE_RESTING) {
            breathBoostStart = millis() - 300;
            breathFallingStart = breathBoostStart + BREATH_RISE_MS;
            breathState = STATE_RISING;
        } else if (breathState == STATE_FALLING) {
            float breathProgress = (millis() - breathFallingStart) / float(BREATH_RISE_MS);
            breathBoostStart = millis() - int(round(BREATH_RISE_MS*(1-breathProgress)));
            // Serial.print(breathProgress);
            // Serial.print(" ");
            // Serial.print(breathBoostStart);
            // Serial.print(" ");
            breathFallingStart = breathBoostStart + BREATH_RISE_MS;
            breathState = STATE_RISING;
        }
    }

    Serial.print(" ---> Breath #");
    Serial.print(b);
    Serial.print(": ");
    Serial.print(progress);
    Serial.print(" = ");
    Serial.print(currentLed);
    Serial.print(" - ");
    Serial.print(tail);
    Serial.print(" >? ");
    Serial.println(furthestBreathPosition);
    
    int baseColor = ROYALBLUE;
    int color;

    for (int i=head; i <= tail; i++) {
        // Head and tail pixel is the fractional fader
        color = baseColor;
        double tailDecay = 1 - BREATH_DECAY*i/tail;
        uint8_t r = ((color & 0xFF0000) >> 16) * (i == head ? headFractional : i == tail ? tailFractional : tailDecay);
        uint8_t g = ((color & 0x00FF00) >> 8) * (i == head ? headFractional : i == tail ? tailFractional : tailDecay);
        uint8_t b = ((color & 0x0000FF) >> 0) * (i == head ? headFractional : i == tail ? tailFractional : tailDecay);
        color = ((r<<16)&0xFF0000) | ((g<<8)&0x00FF00) | ((b<<0)&0x0000FF);

        leds.setPixel(ledsPerStrip*3+currentLed-i, color);
    }
    
    for (int i=1; i <= 5; i++) {
        leds.setPixel(ledsPerStrip*3+currentLed-tail-i, 0);
    }
}

void updatePIR(int p) {
    long now = millis();
    if (now < 5000) return; // Let the PIR setup
    if (now - lastPIRSampleTime[p] < pirSampleInterval) return;

    int value = digitalRead(p == 0 ? PIR1_PIN : PIR2_PIN);
    if (p == 0) value = !value; // Handle one active LOW and one active HIGH PIR sensor
    // Serial.print("PIR #");
    // Serial.print(p);
    // Serial.print(": ");
    // Serial.println(value);
    
    pirHistory[p][pirHistoryIndex[p]] = value;
    pirHistoryIndex[p]++;
    if (pirHistoryIndex[p] >= PIR_HIST_LEN) pirHistoryIndex[p] = 0;

    PirState newState = PIR_ON;

    for (int i = 0; i < PIR_HIST_LEN; i++) {
        if (pirHistory[p][i] == 0) {
            newState = PIR_OFF;
            break;
        }
    }
    
    if (p == 1) {
        pir1State = newState;
    } else if (p == 2) {
        pir2State = newState;
    }

    if (pir1State == PIR_ON || pir2State == PIR_ON) {
        openTimeoutLastEvent = now;
    }

    lastPIRSampleTime[p] = now;
}

void runLeaves() {
    unsigned long boostDuration = 500;
    // Serial.print(" ---> Leaves: ");
    
    for (int c=1; c <= 12; c++) {
        int channelOffset = millis() + ((c<=4 ? 0 : c <= 8 ? 1 : c <= 12 ? 2 : 3) * LEAVES_REST_MS/4);
        float multiplier = (channelOffset % LEAVES_REST_MS) / float(LEAVES_REST_MS);
        int sinPos = multiplier * 360.0f;
        float progress = sinTable[sinPos];
        int center = 100;
        int width = 35;
        int brightness = center + width*progress;

        
        if (dripCount) {
            int d = dripCount % DRIP_LIMIT;
            int latestDripIndex = d - 1;
            if (latestDripIndex < 0) latestDripIndex = DRIP_LIMIT - 1; // wrap around
            if (breathState == STATE_RESTING) {
                float boost = 255 - center - width;
                if (millis() - dripStarts[latestDripIndex] < boostDuration) {
                    // Linear boost up to max brightness
                    progress = ((millis() - dripStarts[latestDripIndex]) / float(boostDuration));
                    boost = boost * progress;
                    brightness += boost;
                } else if (millis() - dripStarts[latestDripIndex] - boostDuration < boostDuration) {
                    // Hold boost
                    brightness += boost;
                } else if (millis() - dripStarts[latestDripIndex] - 2*boostDuration < boostDuration) {
                    // Linear boost down back to baseline
                    progress = ((millis() - dripStarts[latestDripIndex] - 2*boostDuration) / float(boostDuration));
                    boost = boost * (1 - progress);
                    brightness += boost;
                }
            }
            
            if ((c-1)%3 == 0) {
                dispatcher(c, max(70, brightness));
            } else if ((c-1)%3 == 2) {
                dispatcher(c, 0);
            }
        }
        
        
        if (breathState != STATE_RESTING) {
            if (breathState == STATE_RISING) {
                float breathProgress = (millis() - (breathBoostStart)) / float(BREATH_RISE_MS);
                brightness = breathProgress * 255;
                if (breathProgress >= 1) {
                    breathState = STATE_FALLING;
                }
            } else if (breathState == STATE_FALLING) {
                float breathProgress = (millis() - breathFallingStart) / float(BREATH_RISE_MS);
                brightness = (1 - breathProgress) * 255;
                if (breathProgress >= 1) {
                    breathState = STATE_RESTING;
                }
            }
            
            // if ((c-1)%3 == 1 || (c-1)%3 == 2) {
            if ((c-1)%3 == 1) {
                // Serial.print(brightness);
                // Serial.print(" ");
                dispatcher(c, brightness);
            }
        } else {
            // if ((c-1)%3 == 1 || (c-1)%3 == 2) {
            if ((c-1)%3 == 1) {
                dispatcher(c, 0);
            }
        }
    }
}

void transmitSensor() {
    switch (overallState) {
        case STATE_NEUTRAL: {
            if (pir1State == PIR_ON || pir2State == PIR_ON) {
                overallState = STATE_OPEN;
                HWSERIAL.print("X");
                Serial.println(" ---> PIR active");
            }
            break;
        }
        case STATE_OPEN: {
            long now = millis();
            if (now - openTimeoutLastEvent > openTimeout) {
                HWSERIAL.print("Z");
                Serial.println(" ---> PIR inactive");
                overallState = STATE_NEUTRAL;
            }
            break;
        }
    }
}

void receiveSensor() {
    if (HWSERIAL.available() > 0) {
        uint8_t incomingByte;
        incomingByte = HWSERIAL.read();
        // Serial.println(incomingByte, HEX);
        
        if (incomingByte == 'B') {
            // Breath started
            Serial.println(" ---> New breath");
            detectedBreath = true;
            addBreath();
            proximityState = STATE_BREATHING_ACTIVE;
        } else if (incomingByte == 'E') {
            // Breath ended
            Serial.println(" ---> Breath ended <---");
            detectedBreath = false;
            activeBreath = -1;
        } else if (incomingByte == 'O') {
            // PIR active
            pirStart = millis();
            if (proximityState == STATE_PIR_INACTIVE) {
                proximityState = STATE_PIR_ACTIVE;
            }
        } else if (incomingByte == 'C') {
            // PIR inactive
            pirEnd = millis();
            proximityState = STATE_PIR_INACTIVE;
        } else if (incomingByte == 'P') {
            // Proximity near
            proximityState = STATE_PROX_NEAR;
        } else if (incomingByte == 'F') {
            // Proximity far
            proximityState = STATE_PIR_ACTIVE;
        }
    }
}

void runBase() {
    if (proximityState == STATE_PIR_INACTIVE && millis() - BASE_PIR_FADE_OUT > pirEnd) return;
    
    int color = ROYALBLUE;
    int period = 2; // seconds
    float brightnessMax = 0.5f;
    float brightnessMin = 0.25f;
    double progress = (millis() % (period * 1000)) / (period * 1000.f);
    double multiplier = (sinTable[(int)ceil(progress * 360.f)] + 1)/2.f;
    double brightness = multiplier * (brightnessMax - brightnessMin) + brightnessMin;
    
    if (millis() - BASE_PIR_FADE_IN < pirStart) {
        double pirProgress = (millis() - pirStart) / float(BASE_PIR_FADE_IN);
        brightness *= pirProgress;
    } else if (millis() - BASE_PIR_FADE_OUT < pirEnd) {
        double pirProgress = 1 - (millis() - pirStart) / float(BASE_PIR_FADE_IN);
        brightness *= pirProgress;
    }

    uint8_t r = ((color & 0xFF0000) >> 16) * brightness;
    uint8_t g = ((color & 0x00FF00) >> 8) * brightness;
    uint8_t b = ((color & 0x0000FF) >> 0) * brightness;
    color = ((r<<16)&0xFF0000) | ((g<<8)&0x00FF00) | ((b<<0)&0x0000FF);

    for (int i=0; i < ledsPerStrip; i++) {
        leds.setPixel(ledsPerStrip*0 + i, color);
    }
}


unsigned long randomGreen() {
    unsigned long greens[] = {
#if CAMPLIGHTS
        NAVY, DARKBLUE, MEDIUMBLUE, BLUE, DARKGREEN, GREEN, TEAL, DARKCYAN,
        DEEPSKYBLUE, DARKTURQUOISE, MEDIUMSPRINGGREEN, LIME, SPRINGGREEN,
        AQUA, CYAN, MIDNIGHTBLUE, DODGERBLUE, LIGHTSEAGREEN, FORESTGREEN,
        SEAGREEN, DARKSLATEGRAY, LIMEGREEN, MEDIUMSEAGREEN, TURQUOISE,
        ROYALBLUE, STEELBLUE, DARKSLATEBLUE, MEDIUMTURQUOISE, INDIGO,
        LAWNGREEN, CHARTREUSE, AQUAMARINE, MAROON, PURPLE, OLIVE,
        SKYBLUE, BLUEVIOLET, DARKRED, DARKMAGENTA, SADDLEBROWN,
        DARKORCHID, YELLOWGREEN, SIENNA, MEDIUMPURPLE, DARKVIOLET, BROWN,
        GREENYELLOW, FIREBRICK, SALMON,
        DARKGOLDENROD, MEDIUMORCHID, ROSYBROWN, DARKKHAKI,
        MEDIUMVIOLETRED, CHOCOLATE, PALEVIOLETRED, CRIMSON, GOLDENROD,
        RED, FUCHSIA, MAGENTA, DEEPPINK, ORANGERED, TOMATO, DARKORANGE,
        ORANGE, GOLD, YELLOW
#else
        0x36CD00, // Light green
        0x06FD00, // Pure green
        0x06CD00, // Bright green
        0x8CCC00, // Green-Yellowish
        0xACEC00, // Yellowish-Green
        0x45DB06, // Teal green
        0x29BD06  // Seafoam green
#endif
    };

    return greens[random(0, sizeof(greens)/sizeof(int))];
}
