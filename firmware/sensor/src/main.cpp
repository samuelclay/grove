#include "WProgram.h"

extern "C" int main(void)
{
	setup();
	while (1) {
		loop();
		yield();
	}
}
