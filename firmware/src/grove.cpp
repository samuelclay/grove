#include "grove.h"
#include "WS2812.h"

void setup() { 
    leds.begin();
    leds.show();
    randomSeed(analogRead(0));
    
    breathState = STATE_RESTING;
    pinMode (slaveSelectPin, OUTPUT);
    SPI.begin(); 

    Serial.begin(9600); // USB is always 12 Mbit/sec

    clearDispatcher();

    // return;

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
    
    if (millis() % 60000 < 15000) {
        // addBreath();
    }
    advanceBreaths();
    
    runLeaves();
    
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
    float progress = 0;
    int previousLength = 0;
    int d = dripCount % DRIP_LIMIT;
    
    if (dripCount) {
        int latestDripIndex = d - 1;
        if (latestDripIndex < 0) latestDripIndex = DRIP_LIMIT - 1; // wrap around
        int latestDripStart = dripStarts[latestDripIndex];
        // progress = (millis() - latestDripStart) / float(REST_DRIP_TRIP_MS);
        progress = dripEase[latestDripIndex].easeIn(float(millis() - latestDripStart));
        previousLength = dripWidth[latestDripIndex];
        
        // Don't start a new drip if the latest drip isn't done coming out
        if (progress <= previousLength/2+1) {
            // Serial.println(" ---> Not starting drip, last not done yet.");
            return;
        } else if (newDripDelayEnd == 0) {
            newDripDelayEnd = millis() + random(REST_DRIP_DELAY_MIN, REST_DRIP_DELAY_MAX);
        }
    }

    // Serial.print(" ---> Drip progress: ");
    // Serial.print(progress);
    // Serial.print(" -- ");
    // Serial.print(ceil(progress * ledsPerStrip));
    // Serial.print(" < ");
    // Serial.println(furthestBreathPosition);
    
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

bool hasNewBreath() {
    if (activeBreath != -1) return false;
    
    if (millis() > lastNewBreathMs) {
        endActiveBreathMs = millis() + random(500, 1000);
        lastNewBreathMs = endActiveBreathMs + random(800, 1550);
        return true;
    }
    
    return false;
}

bool hasActiveBreath() {
    if (activeBreath == -1) return false;
    
    if (millis() < endActiveBreathMs) {
        return true;
    }
    
    activeBreath = -1;
    return false;
}

void addBreath() {
    int b = breathCount % BREATH_LIMIT;
    bool newBreath = hasNewBreath();
    
    if (newBreath) {
        // Serial.print(" ---> New breath: ");
        // Serial.println(b);
        breathStarts[b] = millis();
        breathPosition[b] = 0;
        breathWidth[b] = 1;
        breathEase[b].setDuration(float(BREATH_TRIP_MS));
        breathEase[b].setTotalChangeInPosition(float(ledsPerStrip*2));
        breathCount++;
        activeBreath = breathCount % BREATH_LIMIT;
    }

    if (!newBreath && hasActiveBreath()) {
        // Still breathing, so extend breath
        int latestBreathIndex = b - 1;
        if (latestBreathIndex < 0) latestBreathIndex = BREATH_LIMIT - 1; // wrap around
        int latestBreathStart = breathStarts[latestBreathIndex];
        // float progress = float(millis() - latestBreathStart) / float(BREATH_TRIP_MS);
        float progress = breathEase[latestBreathIndex].easeOut(float(millis() - latestBreathStart)) / float(ledsPerStrip);
        
        breathWidth[latestBreathIndex] = ceil(progress * ledsPerStrip);
        
        // Serial.print(" ---> Active breath #");
        // Serial.print(latestBreathIndex);
        // Serial.print(": ");
        // Serial.print(breathWidth[latestBreathIndex]);
        // Serial.print(": ");
    } else if (!newBreath) {
        // Not actively breathing
    }
    
}

void advanceRestDrips() {
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
    double dripHeadPosition = 0;
    double headFractional = modf(progress, &dripHeadPosition);
    double tailFractional = (1 - REST_DRIP_DECAY) * (1 - headFractional);
    int32_t windowTail = dripWidth[d];
    int32_t dripTail = dripHeadPosition-dripWidth[d];
    
    int baseColor = dripColor;
    int color;

    windowHead = constrain(dripHeadPosition, 0, ledsPerStrip);
    windowTail = constrain(dripTail, 0, ledsPerStrip);
    if (d == 0) {
        Serial.print(" ---> Drip #");
        Serial.print(d);
        Serial.print(": ");
        Serial.print(progress);
        Serial.print(" (");
        Serial.print(float(millis() - dripStart));
        Serial.print(") = ");
        Serial.print(dripHeadPosition);
        Serial.print(". ");
        Serial.print(ledsPerStrip - dripTail);
        Serial.print(" < ");
        Serial.print(furthestBreathPosition);
        Serial.print(". ");
        Serial.print(windowHead);
        Serial.print(" --> ");
        Serial.println(windowTail);
    }
    for (int32_t i=max(windowHead-1, 0); i >= windowTail; i--) {
        // Head and tail pixel is the fractional fader
        // double tailDecay = 1 - REST_DRIP_DECAY*i/tail;
        uint32_t x = i;
        // Serial.println(x);
        // if (x > ledsPerStrip || x < 0) continue;
        if (x > (uint32_t)furthestBreathPosition && furthestBreathPosition != 0) continue;

        int t = millis();
        int positionMultipler = 2;
        int timeMultiplier = 20;
        int sinPos = ((positionMultipler*x) + (t/timeMultiplier)) % 360;
        float tailRange = 0.95f;
        float tailDecay = (sinTable[sinPos] * (tailRange/2)) + (1 - tailRange/2);
        if (i == windowHead-1) headFractional *= tailDecay;
        if (i == windowTail) tailFractional *= tailDecay;

        color = baseColor;
        uint8_t r = ((color & 0xFF0000) >> 16) * 
            (i == windowHead-1 ? headFractional : i == windowTail ? tailFractional : tailDecay);
        uint8_t g = ((color & 0x00FF00) >> 8) * 
            (i == windowHead-1 ? headFractional : i == windowTail ? tailFractional : tailDecay);
        uint8_t b = ((color & 0x0000FF) >> 0) * 
            (i == windowHead-1 ? headFractional : i == windowTail ? tailFractional : tailDecay);
        color = ((r<<16)&0xFF0000) | ((g<<8)&0x00FF00) | ((b<<0)&0x0000FF);
        
        leds.setPixel(ledsPerStrip*2+(ledsPerStrip - x), color);
    }
    
    if ((ledsPerStrip - dripHeadPosition) <= furthestBreathPosition && furthestBreathPosition != 0) {
        dripEaten[d] = true;
    }
    
    if (dripTail > ledsPerStrip*2) {
        dripEaten[d] = true;
    }
    
    
    
    for (int i=dripTail; i <= dripTail+5; i++) {
        if (i > ledsPerStrip || i < 0) continue;
        leds.setPixel(ledsPerStrip*2+(ledsPerStrip - i), 0);
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

    // Serial.print(" ---> Breath #");
    // Serial.print(b);
    // Serial.print(": ");
    // Serial.print(progress);
    // Serial.print(" = ");
    // Serial.print(currentLed);
    // Serial.print(" - ");
    // Serial.print(tail);
    // Serial.print(" >? ");
    // Serial.println(furthestBreathPosition);
    
    int baseColor = 0xFFFFFF;
    int color;

    for (int i=head; i <= tail; i++) {
        // Head and tail pixel is the fractional fader
        color = baseColor;
        double tailDecay = 1 - BREATH_DECAY*i/tail;
        uint8_t r = ((color & 0x320000) >> 16) * (i == head ? headFractional : i == tail ? tailFractional : tailDecay);
        uint8_t g = ((color & 0x003200) >> 8) * (i == head ? headFractional : i == tail ? tailFractional : tailDecay);
        uint8_t b = ((color & 0x000036) >> 0) * (i == head ? headFractional : i == tail ? tailFractional : tailDecay);
        color = ((r<<16)&0xFF0000) | ((g<<8)&0x00FF00) | ((b<<0)&0x0000FF);

        leds.setPixel(ledsPerStrip*2+currentLed-i, color);
    }
    
    for (int i=1; i <= 5; i++) {
        leds.setPixel(ledsPerStrip*2+currentLed-tail-i, 0);
    }
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
            if (breathState == STATE_RESTING || true) {
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
    
    // Serial.println("");
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
