#include "WProgram.h"

extern "C" int main(void)
{
	setup();
	while (1) {
		loop();
		yield();
	}
}

#include <OctoWS2811.h>

const int ledsPerStrip = 300;

DMAMEM int displayMemory[ledsPerStrip*6];
int drawingMemory[ledsPerStrip*6];

const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config);

#define RED    0xFF0000
#define GREEN  0x00FF00
#define BLUE   0x0000FF
#define YELLOW 0xFFFF00
#define PINK   0xFF1088
#define ORANGE 0xE05800
#define WHITE  0xFFFFFF

void setup() { 
    leds.begin();
    leds.show();
}
void loop() {
  for (int i = 0; i < ledsPerStrip*2; i++) {
      leds.setPixel(i, PINK);
      leds.show();
      leds.setPixel(i, 0);
  }
}