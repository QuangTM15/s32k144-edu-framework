#include "S32K144.h"
#include "analog_read_blocking.h"
#include "analog_read_nonblocking.h"

int main(void)
{
	Demo_AnalogReadNonBlocking();
	while(1){}
}
