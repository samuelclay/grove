#include <OctoWS2811.h>
#include <avr/power.h>
#include <SPI.h>

// ===================
// = Pin Definitions =
// ===================

const uint8_t treeAPin = 14;
const int slaveSelectPin = 10;

// ===========
// = Globals =
// ===========

const int ledsPerStrip = 16;

DMAMEM int displayMemory[ledsPerStrip*6];
int drawingMemory[ledsPerStrip*6];

const int config = WS2811_GRB | WS2811_800kHz;

const int REST_DRIP_WIDTH = 12;
const double REST_DRIP_DECAY = 0.54;
const unsigned long REST_DRIP_TRIP_MS = 60000;
unsigned long beginTime   = 0;

uint32_t dripCount = 0;
const int DRIP_LIMIT = 20;
double dripStarts[DRIP_LIMIT];
unsigned int dripColors[DRIP_LIMIT];
unsigned int dripWidth[DRIP_LIMIT];

// ========
// = LEDs =
// ========

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config);

// ========
// = SPI =
// ========

SPISettings dispatcherSPISettings(1e6, MSBFIRST, SPI_MODE3); 


// =============
// = Functions =
// =============

void drawDrip(int d, int dripStart, int color);
void addRandomDrip();
void advanceRestDrips();
int randomGreen();
void dispatcher(uint8_t chan, uint8_t value);
uint8_t flipByte(uint8_t val);