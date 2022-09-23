#include "gpio.h"

sFintek_sio_data sio_data;

uint8_t _rs_pin; // LOW: command.  HIGH: character.
uint8_t _enable_pin; // activated by a HIGH pulse.
uint8_t _data_pins[8];

uint8_t _displayfunction;
uint8_t _displaycontrol;
uint8_t _displaymode;
uint8_t _numlines,_currline;

unsigned char gpioOutRs = 0x00;
unsigned char gpioOutEn = 0x00;
unsigned char gpioData = 0x00;

/****************************************************************
 *Function Name: init_fintek_sio
 *Description:   Fintek GPIO API initialization
 *
 *Return:        0x01 - Initialization success
 *               0xFF - Initialization failed
 *
 ***************************************************************/
unsigned char init_fintek_gpio(void)
{
    int nRet = 0;

    // Initialize the Fintek library
    // Initialization failed
    if (nRet = init_fintek_sio(eSIO_TYPE_F81804, 0, &sio_data))
    {
        return 0xff;
    }
    // Initialization successfull
    else
    {
        ActiveSIO(sio_data.ic_port, sio_data.key);
        return 0x01;
    }
}

/****************************************************************
 *Function Name: read_gpio
 *Description:   Read GPIO input current value.
 *               Will pass the GPIOID, e.g. GPIO17, GPIOID = 0x17
 *               and direction which is always as the INPUT
 *
 *Return:        Current READ value either 0 or 1
 *               0xFF - Failed
 *
 ***************************************************************/
unsigned int read_gpio (unsigned int gpioId, unsigned int direction, unsigned char init)
{
    unsigned int data = 0;
    int status;

    // Enable GPIO for the first time - Set the flag
    // If the flag already set, no need to enable GPIO again - For read and write operation
    if (init == 0x00)
    {
        // Enable GPIO for read process
        CHECK_RET(_EnableGPIO(gpioId, eGPIO_Mode_Enable));
    }
    // Always GPIO as input
    if (direction != eGPIO_Direction_Out)
    {
        // Start read GPIO current value
        status = _GetGpioInputDataIdx(gpioId, &data);

        // Only interested in single input mode
        // Other than this will consider as a read process failed
        // READ failed
        if (status)
        {
            return 0xff;
        }
        // READ success
        else
        {
            //DeactiveSIO(sio_data.ic_port);
            return data;
        }
    }
}

/****************************************************************
 *Function Name: write_gpio
 *Description:   Write GPIO output with value.
 *               Will pass the GPIOID, e.g. GPIO17, GPIOID = 0x17
 *               and direction which is always as the OUTPUT
 *
 *Return:        Current WRITE value
 *               0xFF - Failed
 *
 ***************************************************************/
unsigned int write_gpio (unsigned int gpioId, unsigned int direction, unsigned int outMode, unsigned int outVal, unsigned char init)
{
    unsigned int data = 0;
    int status;

    // Enable GPIO for the first time - Set the flag
    // If the flag already set, no need to enable GPIO again - For read and write operation
    if (init == 0x00)
    {
        // Enable GPIO for write process
        CHECK_RET(_EnableGPIO(gpioId, eGPIO_Mode_Enable));
    }
    // Always GPIO as output
    if (direction == eGPIO_Direction_Out)
	{
		// Enable GPIO for given ID
		CHECK_RET(_SetGpioOutputEnableIdx(gpioId, eGPIO_Direction_Out));

        // Set OUTPUT mode - Set the flag
        // If the flag already set - No need to set the OUTPUT mode again
        // for this particular GPIO OUTPUT
        if (init == 0x00)
        {
            // Set GPIO output mode
            // Output mode - 0 -> Open drain - MUST use pull up resistor at the output
            // Output mode - 1 -> Push-pull  - NO pull up resistor are required
            CHECK_RET(_SetGpioDriveEnable(gpioId, outMode));
        }
        // Start write GPIO with pass value
        status = _SetGpioOutputDataIdx(gpioId, outVal);

        // Only interested in single mode
        // WRITE success
        if (!status )
        {
            return outVal;
        }
        // WRITE failed
        else
        {
            return 0xff;
        }
    }
}

void pulseEnable (void)
{
	// Enable pin set to LOW
	write_gpio(_enable_pin,1,0,0,gpioOutEn);
	usleep(1);
	// Enable pin set to HIGH
	write_gpio(_enable_pin,1,0,1,gpioOutEn);
	usleep(1); // enable pulse must be >450ns
	// Enable pin set to LOW
	write_gpio(_enable_pin,1,0,0,gpioOutEn);
	usleep(100); // commands need > 37us to settle
}

void write4bits(uint8_t value)
{
	// LCD 4 bits mode of data
	for (int i = 0; i < 4; i++)
	{
		// Initialize GPIO for LCD data  - GPIO51,53,55,57
		// Initialization once, the rest process are write data
		// Initialization failed
		if (write_gpio(_data_pins[i],1,0,(value >> i) & 0x01,gpioData) == 0xff)
		{}
		// Initialization successfull
		else
		{
			// Only enable and initialize this GPIO once
			if (gpioData == 0x00)
			{
				//printf ("masuk lagi\n");
				gpioData = 0x01;
			}
		}
	}
	pulseEnable();
}

void sendData (uint8_t value, uint8_t mode)
{
	write_gpio(_rs_pin,1,0,mode,gpioOutRs);

	write4bits(value>>4);
    write4bits(value);
}

void command (uint8_t value)
{
	sendData(value, 0); // For command data to LCD, write RS pin LOW
}

void writeData (uint8_t value)
{
	sendData(value, 1); // For normal data to LCD, write RS pin HIGH
}

void clear (void)
{
	command(LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
	usleep (2000); // this command takes a long time!
}

void display (void)
{
	_displaycontrol |= LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | _displaycontrol);
}

void begin(uint8_t cols, uint8_t lines)
{
	if (lines > 1)
	{
		_displayfunction |= LCD_2LINE;
	}
	_numlines = lines;
	_currline = 0;

	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// according to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
	usleep (50000);

	// Now we pull both RS low to begin commands
	write_gpio(_rs_pin,1,0,0,gpioOutRs);
	write_gpio(_enable_pin,1,0,0,gpioOutEn);

	// Put the LCD into 4 bit or 8 bit mode
	if (!(_displayfunction & LCD_8BITMODE))
	{
		// We start in 8bit mode, try to set 4 bit mode
		write4bits(0x03);
		usleep(4500); // wait min 4.1ms

		// Second try
		write4bits(0x03);
		usleep(4500); // wait min 4.1ms

		// finally, set to 4-bit interface
		write4bits(0x02);
	}
	// finally, set # lines, font size, etc.
	command(LCD_FUNCTIONSET | _displayfunction);

	// turn the display on with no cursor or blinking default
	_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
	display();

	// clear it off
	clear();

	// Initialize to default text direction (for romance languages)
	_displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
	// set the entry mode
	command(LCD_ENTRYMODESET | _displaymode);
}

size_t printlcd(uint8_t str[], size_t len)
{
	// Start print string to LCD
	for (int i = 0; i < len; i++)
	{
		sendData(str[i], 1); // For normal data to LCD, write RS pin HIGH
	}
}

void setCursor (uint8_t col, uint8_t row)
{
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	if (row >= _numlines)
	{
		row = _numlines - 1;    // we count rows starting w/0
	}
	command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

unsigned char LiquidCrystal_init(uint8_t rs, uint8_t enable,
                        uint8_t d0, uint8_t d1, uint8_t d2,
                        uint8_t d3)
{
    int retCnt = 0;

	_rs_pin = rs;         // GPIO54
    _enable_pin = enable; // GPIO56

	_data_pins[0] = d0; // GPIO51
	_data_pins[1] = d1; // GPIO53
	_data_pins[2] = d2; // GPIO55
	_data_pins[3] = d3; // GPIO57
	_data_pins[4] = 0;
	_data_pins[5] = 0;
	_data_pins[6] = 0;
	_data_pins[7] = 0;

	// Initialize GPIO for LCD RS pin - GPIO54
	// Initialization failed
	if (write_gpio(_rs_pin,1,0,0,gpioOutRs) == 0xff)
	{
		return 0xff;
	}
	// Initialization successfull
	else
	{
		retCnt++;

		// Only enable and initialize this GPIO once
		if (gpioOutRs == 0x00)
		{
			gpioOutRs = 0x01;
		}
	}

	// Initialize GPIO for LCD ENABLE pin - GPIO56
	// Initialization failed
	if (write_gpio(_enable_pin,1,0,0,gpioOutEn) == 0xff)
	{
		return 0xff;
	}
	// Initialization successfull
	else
	{
		retCnt++;

		// Only enable and initialize this GPIO once
		if (gpioOutEn == 0x00)
		{
			gpioOutEn = 0x01;
		}
	}
	if (retCnt != 2)
	{
		return 0xff;
	}

	// 4 bits LCD mode
	_displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;

	//begin(16, 1);

	return 0x01;
}
