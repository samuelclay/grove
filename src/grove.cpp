#include "grove.h"

void setup() { 
    leds.begin();
    leds.show();
    randomSeed(analogRead(0));
}
void loop() {
    beginTime = millis();
    // for (int d=0; d < sizeof(activeDrips); d++) {
        // int activeDrip = activeDrips[d];
        advanceRestDrips();
    // }
}
void advanceRestDrips() {
    float progress = (millis() % msRestDrip) / float(msRestDrip);
    int head = 0;
    int tail = REST_DRIP_WIDTH;
    double currentLed = 0;
    double headFractional = modf(progress * ledsPerStrip, &currentLed);

    // color = ((color<<8)&0xFF0000) | ((color>>8)&0x00FF00) | (color&0x0000FF);

    int baseColor = randomGreen();
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
    leds.show();
}

int randomGreen() {
    int greens[] = {
        0x003F00,
        0x41AF11,
        0x11EF20,
        0x00CF22,
        0x22BF00,
        0x34DF12
    };
    return greens[random(0, 0)];
}