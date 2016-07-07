#include "grove.h"

void setup() { 
    // leds.begin();
    // leds.show();
    // randomSeed(analogRead(0));

    pinMode (slaveSelectPin, OUTPUT);
    SPI.begin(); 
}

void loop() {
    addRandomDrip();
    advanceRestDrips();
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
        int latestDripStart = dripStarts[(dripCount % DRIP_LIMIT)-1];
        progress = (millis() - latestDripStart) / float(REST_DRIP_TRIP_MS);

        if (progress <= (REST_DRIP_WIDTH*2/float(ledsPerStrip))) return;
    }
    
    if (!dripCount || random(0, floor(250 * max(1.f - progress, 1))) <= 1) {
        dripStarts[dripCount % DRIP_LIMIT] = millis();
        dripColors[dripCount % DRIP_LIMIT] = randomGreen();
        dripCount++;
    }
}
void advanceRestDrips() {
    for (unsigned int d=0; d < min(dripCount, DRIP_LIMIT); d++) {
        unsigned int dripStart = dripStarts[d];
        if (dripStart < millis() - float(REST_DRIP_TRIP_MS)) continue;
        unsigned int dripColor = dripColors[d];

        drawDrip(d, dripStart, dripColor);
    }

    leds.show();
}

void drawDrip(int d, int dripStart, int dripColor) {
    float progress = (millis() - dripStart) / float(REST_DRIP_TRIP_MS);
    int head = 0;
    int tail = REST_DRIP_WIDTH;
    double currentLed = 0;
    double headFractional = modf(progress * ledsPerStrip, &currentLed);

    int baseColor = dripColor;
    int color = 0;
    uint8_t r = ((baseColor & 0xFF0000) >> 16) * headFractional;
    uint8_t g = ((baseColor & 0x00FF00) >> 8) * headFractional;
    uint8_t b = ((baseColor & 0x0000FF) >> 0) * headFractional;
	color = ((r<<16)&0xFF0000) | ((g<<8)&0x00FF00) | ((b<<0)&0x0000FF);

    leds.setPixel(max(currentLed+1, head), color);
    double tailFractional = (1 - REST_DRIP_DECAY) * (1 - headFractional);

    for (int i=head; i <= tail; i++) {
        color = baseColor;
        uint8_t r = ((color & 0xFF0000) >> 16) * (i == tail ? tailFractional : 1 - REST_DRIP_DECAY*i/tail);
        uint8_t g = ((color & 0x00FF00) >> 8) * (i == tail ? tailFractional : 1 - REST_DRIP_DECAY*i/tail);
        uint8_t b = ((color & 0x0000FF) >> 0) * (i == tail ? tailFractional : 1 - REST_DRIP_DECAY*i/tail);
    	color = ((r<<16)&0xFF0000) | ((g<<8)&0x00FF00) | ((b<<0)&0x0000FF);

        leds.setPixel(currentLed-i, color);
    }

    leds.setPixel(currentLed-tail-1, 0);
}

int randomGreen() {
    int greens[] = {
        0x3BF323,
        0x58AB46,
        0x4AF023,
        0x45DB00,
        0x36CD00,
        0x56EE00,
        0x5CFC00,
        0x5FDA45,
        0x45DD32,
        0x42E632,
        0x57EA3A,
        0x39BD02
    };
    return greens[random(0, sizeof(greens)/sizeof(int))];
}