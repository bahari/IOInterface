#ifndef GPIO_H_INCLUDED
#define GPIO_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "fintek_api.h"

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

extern unsigned char init_fintek_gpio(void);
//extern unsigned int read_gpio (unsigned int gpioId, unsigned int direction);
extern unsigned int read_gpio (unsigned int gpioId, unsigned int direction, unsigned char init);
extern unsigned int write_gpio (unsigned int gpioId, unsigned int direction, unsigned int outMode, unsigned int outVal, unsigned char init);
extern void clear (void);
extern void begin(uint8_t cols, uint8_t lines);
extern size_t printlcd(uint8_t str[], size_t len);
extern void setCursor (uint8_t col, uint8_t row);
extern unsigned char LiquidCrystal_init(uint8_t rs, uint8_t enable,
                        uint8_t d0, uint8_t d1, uint8_t d2,
                        uint8_t d3);

#endif // GPIO_H_INCLUDED
