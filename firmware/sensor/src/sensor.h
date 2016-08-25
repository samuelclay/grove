#include <avr/power.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <SI114.h>

#define USE_IR_PROX

#define NEO_PIXEL_PIN 8
#define ULTRASONIC_ANALOG_PIN 17
#define RAW_VOLT_ANALOG_WING_SENSOR_PIN 1
#define TEMPERATURE_ANALOG_WIND_SENSOR_PIN 2
#define PIR_PIN 20
#define SI114_PIN 0

Adafruit_NeoPixel leds = Adafruit_NeoPixel(2, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);

#ifdef USE_IR_PROX
	PortI2C pulseI2C(SI114_PIN);
	PulsePlug pulse(pulseI2C);
#endif