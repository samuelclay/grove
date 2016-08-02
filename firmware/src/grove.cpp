#include "grove.h"

void setup() { 
    leds.begin();
    leds.show();
    randomSeed(analogRead(0));

    pinMode (slaveSelectPin, OUTPUT);
    SPI.begin(); 
}

void loop() {
    addRandomDrip();
    advanceRestDrips();
    
    addBreath();
    advanceBreaths();
    
    leds.show();
}

/**
 * Set the analog output on the dispatcher board.
 * Channels are numbered 1 to 12 inclusive (THEY DON'T START AT ZERO !!) 
 * value is 0-255 range analog output of the dac/LED brightness
 */
void dispatcher(uint8_t chan, uint8_t value) {
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
    
    if (dripCount) {
        int latestDripIndex = (dripCount % DRIP_LIMIT) - 1;
        if (latestDripIndex < 0) latestDripIndex = DRIP_LIMIT - 1;
        int latestDripStart = dripStarts[latestDripIndex];
        progress = (millis() - latestDripStart) / float(REST_DRIP_TRIP_MS);
        
        // Don't start a new drip if the latest drip isn't done coming out
        if (progress <= ((REST_DRIP_WIDTH_MAX+REST_DRIP_WIDTH_MIN)/float(ledsPerStrip))) return;
    }
    
    if (!dripCount || random(0, floor(250 * max(1.f - progress, 1))) <= 1) {
        dripStarts[dripCount % DRIP_LIMIT] = millis();
        dripColors[dripCount % DRIP_LIMIT] = randomGreen();
        dripWidth[dripCount % DRIP_LIMIT] = random(REST_DRIP_WIDTH_MIN, REST_DRIP_WIDTH_MAX);
        dripCount++;
    }
}

void addBreath() {
    
}

void advanceRestDrips() {
    for (unsigned int d=0; d < min(dripCount, DRIP_LIMIT); d++) {
        unsigned int dripStart = dripStarts[d];
        // The 1.1 is to ensure that the tail of each drip disappears, since
        // this only considers the position of the head of the drip.
        float progress = (millis() - dripStart) / float(REST_DRIP_TRIP_MS);
        if (progress >= 1.1) continue;
        unsigned int dripColor = dripColors[d];

        drawDrip(d, dripStart, dripColor);
    }
}

void advanceBreaths() {
    for (unsigned int b=0; b < min(breathCount, BREATH_LIMIT); b++) {
        unsigned int breathStart = breathStarts[b];
        drawBreath(b, breathStart);
    }
}

void drawDrip(int d, int dripStart, int dripColor) {
    float progress = (millis() - dripStart) / float(REST_DRIP_TRIP_MS);
    int head = 0;
    int tail = dripWidth[d];
    double currentLed = 0;
    double headFractional = modf(progress * ledsPerStrip, &currentLed);
    currentLed = ledsPerStrip - currentLed;

    int baseColor = dripColor;
    int color = 0;
    uint8_t r = ((baseColor & 0xFF0000) >> 16) * headFractional;
    uint8_t g = ((baseColor & 0x00FF00) >> 8) * headFractional;
    uint8_t b = ((baseColor & 0x0000FF) >> 0) * headFractional;
    color = ((r<<16)&0xFF0000) | ((g<<8)&0x00FF00) | ((b<<0)&0x0000FF);

    // First pixel is the fractional fader
    if (currentLed > 0) {
        leds.setPixel(ledsPerStrip*2+currentLed-1, color);
    }
    double tailFractional = (1 - REST_DRIP_DECAY) * (1 - headFractional);

    for (int i=head; i <= tail; i++) {
        color = baseColor;
        uint8_t r = ((color & 0xFF0000) >> 16) * (i == tail ? tailFractional : 1 - REST_DRIP_DECAY*i/tail);
        uint8_t g = ((color & 0x00FF00) >> 8) * (i == tail ? tailFractional : 1 - REST_DRIP_DECAY*i/tail);
        uint8_t b = ((color & 0x0000FF) >> 0) * (i == tail ? tailFractional : 1 - REST_DRIP_DECAY*i/tail);
        color = ((r<<16)&0xFF0000) | ((g<<8)&0x00FF00) | ((b<<0)&0x0000FF);

        leds.setPixel(ledsPerStrip*2+currentLed+i, color);
    }
    
    for (int i=1; i <= 5; i++) {
        leds.setPixel(ledsPerStrip*2+currentLed+tail+i, 0);
    }
}

void drawBreath(int b, int breathStart) {
    float progress = (millis() - breathStart) / float(BREATH_TRIP_MS);
    int head = 0;
    int tail = breathWidth[b];
    double currentLed = 0;
    double headFractional = modf(progress * ledsPerStrip, &currentLed);
    
    int baseColor = 0xFFFFFF;
    int color = 0;
    uint8_t red = ((baseColor & 0xFF0000) >> 16) * headFractional;
    uint8_t green = ((baseColor & 0x00FF00) >> 8) * headFractional;
    uint8_t blue = ((baseColor & 0x0000FF) >> 0) * headFractional;
    color = ((red<<16)&0xFF0000) | ((green<<8)&0x00FF00) | ((blue<<0)&0x0000FF);

    // First pixel is the fractional head fader
    leds.setPixel(ledsPerStrip*2+currentLed+1, color);
    double tailFractional = (1 - BREATH_DECAY) * (1 - headFractional);

    for (int i=head; i <= tail; i++) {
        color = baseColor;
        uint8_t r = ((color & 0xFF0000) >> 16) * (i == tail ? tailFractional : 1 - BREATH_DECAY*i/tail);
        uint8_t g = ((color & 0x00FF00) >> 8) * (i == tail ? tailFractional : 1 - BREATH_DECAY*i/tail);
        uint8_t b = ((color & 0x0000FF) >> 0) * (i == tail ? tailFractional : 1 - BREATH_DECAY*i/tail);
        color = ((r<<16)&0xFF0000) | ((g<<8)&0x00FF00) | ((b<<0)&0x0000FF);

        leds.setPixel(ledsPerStrip*2+currentLed-i, color);
    }

    leds.setPixel(ledsPerStrip*2+currentLed-tail-1, 0);
}

int randomGreen() {
    int greens[] = {
        0x36CD00, // Light green
        0x06FD00, // Pure green
        0x06CD00, // Bright green
        0x8CCC00, // Green-Yellowish
        0xACEC00, // Yellowish-Green
        0x45DB06, // Teal green
        0x29BD06  // Seafoam green
    };

    return greens[random(0, sizeof(greens)/sizeof(int))];
}