#include <avr/power.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>

#define NEO_PIXEL_PIN 8
#define analogPinForRV  1 //Pin for raw voltage from wind sensor
#define analogPinForTMP  2 //Pin for temp from wind sensor

Adafruit_NeoPixel leds = Adafruit_NeoPixel(2, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);