#include <OctoWS2811.h>

// ===================
// = Pin Definitions =
// ===================

const uint8_t treeAPin = 14;

// ===========
// = Globals =
// ===========

const int ledsPerStrip = 300;

DMAMEM int displayMemory[ledsPerStrip*6];
int drawingMemory[ledsPerStrip*6];

const int config = WS2811_GRB | WS2811_800kHz;

const int REST_DRIP_WIDTH = 12;
const double REST_DRIP_DECAY = 0.6;
const int msRestDrip = 60000;
unsigned long beginTime   = 0;
double activeDrips[] = {0, .2, .4, .7, .9};

// ========
// = LEDs =
// ========

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config);

void advanceRestDrips();
int randomGreen();
