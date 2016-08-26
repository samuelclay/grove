#include <Servo.h>
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
#define SERVO_PIN 3
#define SI114_PIN 0

Adafruit_NeoPixel leds = Adafruit_NeoPixel(2, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Servo

Servo servo; 

const int servoClosePos = 35; // recommended servo angle to close the flower. (previous values used: 40)
const int servoOpenPos = 110; // recommended servo angle to open the flower (previous values used: 130)

const long servoMoveTime = 5000;
long servoMoveStartTime = 0;
int servoTargetPosition = servoClosePos;
int servoStartPosition = servoClosePos;
int servoPosition = servoClosePos;
bool servoAttached = false;

// Wind

float lowWindAvgFactor = 0.9;
float lowWindAvg = 700;
float highWindAvgFactor = 0.98;
float highWindAvg = 700;
int bpassWind = 0;
#define BPASS_HIST_LEN 50
int bpassHistoryIndex = 0;
int bpassHistory[BPASS_HIST_LEN];

typedef enum {
	BREATH,
	REST
} breathState;
breathState bState = REST;

int windState = 0;

// IR prox

#ifdef USE_IR_PROX
	PortI2C pulseI2C(SI114_PIN);
	PulsePlug pulse(pulseI2C);
#endif