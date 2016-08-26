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

const long windSampleInterval = 20;

long lastWindSampleTime = 0;
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
} BreathState;
BreathState breathState = REST;

int windState = 0;

// UART to mainboard

#define HWSERIAL Serial1

// PIR

const long pirSampleInterval = 20;
long lastPIRSampleTime = 0;

#define PIR_HIST_LEN 10
int pirHistoryIndex = 0;
int pirHistory[PIR_HIST_LEN];

typedef enum {
	PIR_ON,
	PIR_OFF
} PirState;

PirState pirState = PIR_OFF;

// Ultrasonic

const long ultraThres = 250;
const long ultraSampleInterval = 20;
long lastUltraSampleTime = 0;
bool isProximate = false;

#define ULTRA_HIST_LEN 10
int ultraHistoryIndex = 0;
int ultraHistory[ULTRA_HIST_LEN];

// IR prox

#ifdef USE_IR_PROX
	PortI2C pulseI2C(SI114_PIN);
	PulsePlug pulse(pulseI2C);
#endif

// App flow

typedef enum {
	STATE_NEUTRAL,
	STATE_OPEN,
	STATE_PROX
} SystemState;

SystemState overallState = STATE_NEUTRAL;
const long openTimeout = 5*1000;
long openTimeoutLastEvent = 0;