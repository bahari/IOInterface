#include "lcd.h"

unsigned int _rs_pin;
unsigned int _rw_pin;
unsigned int _enable_pin;
unsigned int _data_pins[4];

unsigned char gpioOutRs = 0x00;

unsigned char LiquidCrystal_init(unsigned int rs, unsigned int enable,
                        unsigned int d0, unsigned int d1, unsigned int d2,
                        unsigned int d3)
{
    _rs_pin = rs;
    _enable_pin = enable;

	_data_pins[0] = d0;
	_data_pins[1] = d1;
	_data_pins[2] = d2;
	_data_pins[3] = d3; 
	/*
	// Initialize GPIO for LCD RS pin
	// Initialization failed
	if (write_gpio(0x50,1,0,0,gpioOutAlv) == 0xff)
	{
		
	}
	*/
}
