#include "Arduino.h"
#include "S32K144.h"
int main () {
	setup();
	pinMode(LED_RED, OUTPUT);
	while (1)
	{
	    digitalToggle(LED_RED);
	    delay(1000);
	}
}
