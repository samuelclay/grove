#include <Ultrasonic.h>
#include <Servo.h>
#include <avr/power.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <SI114.h>

// #define USE_IR_PROX
// #define FAKE_PROX


#define NEO_PIXEL_PIN 8
#define ULTRASONIC_ANALOG_PIN 17
#define RAW_VOLT_ANALOG_WIND_SENSOR_PIN 1
#define TEMPERATURE_ANALOG_WIND_SENSOR_PIN 2
#define PIR_PIN 20
#define SERVO_PIN 3
#define SI114_PORT 0
#define ULTRASONIC_2_TRIG_PIN 5
#define ULTRASONIC_2_ECHO_PIN 4

#define PIR_REMOTE_PIN 1
#define BREATH_REMOTE_PIN 0

Adafruit_NeoPixel leds = Adafruit_NeoPixel(2, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Remote

long lastRemotePIRRead = 0;
const long remotePIRReadInteval = 100;

// LEDs

int ledCloseColorR = 0, ledCloseColorG = 0, ledCloseColorB = 0;
int ledOpenColorR = 255, ledOpenColorG = 85, ledOpenColorB = 0;
int ledProxColorR = 255, ledProxColorG = 85, ledProxColorB = 85;

int ledStartColorR = 0, ledStartColorG = 0, ledStartColorB = 0;
int ledEndColorR = 255, ledEndColorG = 255, ledEndColorB = 0;

const long fadeTime = 10000;
long fadeStartTime = -2*fadeTime;

long ledUpdateInterval = 100;
long lastLedUpdateTime = 0;

// Servo

Servo servo; 

const int servoClosePos = 35; // recommended servo angle to close the flower. (previous values used: 40)
const int servoOpenPos = 110; // recommended servo angle to open the flower (previous values used: 130)

const long flowerOpenTime = 5000;
long flowerOpenStartTime = -2*flowerOpenTime;
bool isOpenOrClosing = false;

int servoTargetPosition = servoOpenPos;
int servoStartPosition = servoOpenPos;
int servoPosition = servoOpenPos;

// Wind

const long windSampleInterval = 20;

long lastWindSampleTime = 0;
float lowWindAvgFactor = 0.6;
float lowWindAvg = 700;
float highWindAvgFactor = 0.98;
float highWindAvg = 700;
int bpassWind = 0;
int lastBpassWind = 0;

#define WIND_HIST_LEN 50
int windHistoryIndex = 0;
int windHistory[WIND_HIST_LEN];

typedef enum {
	BREATH,
	REST
} BreathState;
BreathState breathState = REST;

int windState = 0;

// UART to mainboard

// #define HWSERIAL Serial1

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

// New Ultrasonic with two eyes

Ultrasonic ultrasonic(ULTRASONIC_2_TRIG_PIN, ULTRASONIC_2_ECHO_PIN, 6000); // (Trig PIN, Echo PIN)

// Ultrasonic

const long ultraThres = 50;
const long ultraSampleInterval = 100;
long lastUltraSampleTime = 0;
bool isProximate = false;

#define ULTRA_HIST_LEN 10
int ultraHistoryIndex = 0;
int ultraHistory[ULTRA_HIST_LEN];

// IR prox

#ifdef USE_IR_PROX
	PortI2C pulseI2C(SI114_PORT);
	PulsePlug pulse(pulseI2C);
#endif

// App flow

typedef enum {
	STATE_NEUTRAL,
	STATE_OPEN,
	STATE_PROX
} SystemState;

SystemState overallState = STATE_NEUTRAL;

SystemState remoteState = STATE_NEUTRAL;

const long openTimeout = 5*1000;
long openTimeoutLastEvent = 0;

void closeFlower();
void openFlower();
void breathOff();
void breathOn();