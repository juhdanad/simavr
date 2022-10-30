#include "avr/io.h"
#include "util/delay_basic.h"

#define CLR_DISP 0x00000001
#define DISP_OFF 0x00000008
#define DD_RAM_ADDR 0x00000080
#define DD_RAM_ADDR2 0x000000C0

static unsigned char lcd_cgram[64] = {};
static unsigned char lcd_row_1[16] = {};
static unsigned char lcd_row_2[16] = {};

static void lcd_long_delay(unsigned int b)
{
	volatile unsigned int a = b; // NOTE: volatile added to prevent the compiler to optimization the loop away
	while (a)
	{
		a--;
	}
}

static void lcd_long_e_pulse()
{
	PORTC = PORTC | 0b00000100; // set E to high
	lcd_long_delay(1400);		// delay ~110ms
	PORTC = PORTC & 0b11111011; // set E to low
}

void lcd_send_long_command(unsigned char a)
{
	unsigned char data = 0b00001111 | a; // get high 4 bits
	PORTC = (PORTC | 0b11110000) & data; // set D4-D7
	PORTC = PORTC & 0b11111110;			 // set RS port to 0
	lcd_long_e_pulse();					 // pulse to set D4-D7 bits

	data = a << 4;						 // get low 4 bits
	PORTC = (PORTC & 0b00001111) | data; // set D4-D7
	PORTC = PORTC & 0b11111110;			 // set RS port to 0 -> display set to command mode
	lcd_long_e_pulse();					 // pulse to set d4-d7 bits
}

void lcd_init()
{
	// LCD initialization
	// step by step (from Gosho) - from DATASHEET

	PORTC = PORTC & 0b11111110;

	lcd_long_delay(10000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	lcd_long_e_pulse(); // high->low to E port (pulse)
	lcd_long_delay(1000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	lcd_long_e_pulse(); // high->low to E port (pulse)
	lcd_long_delay(1000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	lcd_long_e_pulse(); // high->low to E port (pulse)
	lcd_long_delay(1000);

	PORTC = 0b00100000; // set D4 to 0, D5 port to 1
	lcd_long_e_pulse(); // high->low to E port (pulse)

	// NOTE: added missing initialization steps
	lcd_send_long_command(0x28);	 // function set: 4 bits interface, 2 display lines, 5x8 font
	lcd_send_long_command(DISP_OFF); // display off, cursor off, blinking off
	lcd_send_long_command(CLR_DISP); // clear display
	lcd_send_long_command(0x06);	 // entry mode set: cursor increments, display does not shift
}

static void lcd_e_pulse()
{
	PORTC = PORTC | 0b00000100; // set E to high
	_delay_loop_1(255);
	PORTC = PORTC & 0b11111011; // set E to low
}

void lcd_send_command(unsigned char a)
{
	unsigned char data = 0b00001111 | a; // get high 4 bits
	PORTC = (PORTC | 0b11110000) & data; // set D4-D7
	PORTC = PORTC & 0b11111110;			 // set RS port to 0
	lcd_e_pulse();						 // pulse to set D4-D7 bits

	data = a << 4;						 // get low 4 bits
	PORTC = (PORTC & 0b00001111) | data; // set D4-D7
	PORTC = PORTC & 0b11111110;			 // set RS port to 0 -> display set to command mode
	lcd_e_pulse();						 // pulse to set d4-d7 bits
}

void lcd_send_char(unsigned char a)
{
	unsigned char data = 0b00001111 | a; // get high 4 bits
	PORTC = (PORTC | 0b11110000) & data; // set D4-D7
	PORTC = PORTC | 0b00000001;			 // set RS port to 1
	lcd_e_pulse();						 // pulse to set D4-D7 bits

	data = a << 4;						 // get low 4 bits
	PORTC = (PORTC & 0b00001111) | data; // clear D4-D7
	PORTC = PORTC | 0b00000001;			 // set RS port to 1 -> display set to command mode
	lcd_e_pulse();						 // pulse to set d4-d7 bits
}

void lcd_update()
{
	lcd_send_command(0x40);
	for (uint8_t i = 0; i < 64; ++i)
	{
		lcd_send_char(lcd_cgram[i]);
	}
	lcd_send_command(DD_RAM_ADDR);
	for (uint8_t i = 0; i < 16; ++i)
	{
		lcd_send_char(lcd_row_1[i]);
	}
	lcd_send_command(DD_RAM_ADDR2);
	for (uint8_t i = 0; i < 16; ++i)
	{
		lcd_send_char(lcd_row_2[i]);
	}
}