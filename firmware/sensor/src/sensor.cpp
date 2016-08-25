#include "sensor.h"

void setup() { 
    leds.begin();
    leds.show();

    Serial.begin(9600);
}

void setOnboardLEDs(uint8_t rValue, uint8_t gValue, uint8_t bValue) {
    for (int i = 0; i < 60; i++) {
        leds.setPixelColor(0, rValue, gValue, bValue);
        leds.setPixelColor(1, rValue, gValue, bValue);
    }
    leds.show();
}

void loop() {
    setOnboardLEDs(255, 0, 0);
    Serial.println("R");
    delay(1000);
    setOnboardLEDs(100, 100, 100);
    Serial.println("W");
    delay(1000);
}
