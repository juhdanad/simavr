// NOTE: lines below added to allow compilation in simavr's build system
#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");

#include "avr/io.h"
#include "lcd_helper.h"
#include "hd44780_cgrom.h"

#define __AVR_ATMEGA128__ 1

static void port_init()
{
	PORTA = 0b00011111;
	DDRA = 0b01000000; // NOTE: set A4-0 to initialize buttons to unpressed state
	PORTB = 0b00000000;
	DDRB = 0b00000000;
	PORTC = 0b00000000;
	DDRC = 0b11110111;
	PORTD = 0b11000000;
	DDRD = 0b00001000;
	PORTE = 0b00000000;
	DDRE = 0b00110000;
	PORTF = 0b00000000;
	DDRF = 0b00000000;
	PORTG = 0b00000000;
	DDRG = 0b00000000;
}

static uint16_t player_reset_x = 0x4000;
static uint16_t player_reset_y = 0x4000;
static uint16_t player_reset_y_char = 14;
static uint16_t player_x = 0x4000;
static uint16_t player_y = 0x4000;
static int16_t player_vel_x = 0;
static int16_t player_vel_y = 0;
static uint8_t player_y_char = 14;
static uint8_t player_y_char_old = 14;
static uint8_t player_jump_frames = 0;
static uint8_t player_on_floor = 0;
static uint8_t level_row[2][2][2][16] = {{{{16, 16, 16, 45, 16, 43, 16, 40, 16, 16, 16, 40, 16, 16, 16, 255},
										   {16, 16, 46, 16, 44, 16, 42, 16, 16, 40, 16, 16, 16, 40, 4, 255}},
										  {{16, 16, 16, 45, 16, 43, 16, 40, 16, 16, 16, 40, 16, 16, 16, 255},
										   {16, 16, 46, 16, 44, 16, 42, 16, 16, 40, 16, 16, 16, 40, 5, 255}}},
										 {{{16, 16, 16, 45, 16, 43, 16, 40, 16, 16, 16, 40, 16, 16, 16, 255},
										   {16, 16, 46, 16, 44, 16, 42, 16, 16, 40, 16, 16, 16, 40, 4, 255}},
										  {{16, 16, 16, 45, 16, 43, 16, 40, 16, 16, 16, 40, 16, 16, 16, 255},
										   {16, 16, 46, 45, 44, 16, 42, 16, 16, 40, 16, 16, 16, 40, 5, 255}}}};
static uint8_t *level_row_1 = level_row[0][0][0];
static uint8_t *level_row_2 = level_row[0][0][1];
static uint8_t *level_row_alt_1 = level_row[0][1][0];
static uint8_t *level_row_alt_2 = level_row[0][1][1];
static uint16_t player_surrounding[16] = {};
static uint16_t player_surrounding_alt[16] = {};
static uint16_t player_display_buffer[16] = {};
static uint32_t frame = 0;

void update_player_surrounding()
{
	uint8_t current_char_code;
	const uint8_t *current_char;
	current_char_code = level_row_1[player_y_char];
	current_char = current_char_code < 8 ? &lcd_cgram[current_char_code * 8] : hd44780_cgrom[current_char_code];
	for (uint8_t i = 0; i < 8; i++)
		player_surrounding[i] = current_char[i] << 5;
	current_char_code = level_row_2[player_y_char];
	current_char = current_char_code < 8 ? &lcd_cgram[current_char_code * 8] : hd44780_cgrom[current_char_code];
	for (uint8_t i = 0; i < 8; i++)
		player_surrounding[8 + i] = current_char[i] << 5;
	current_char_code = level_row_1[player_y_char + 1];
	current_char = current_char_code < 8 ? &lcd_cgram[current_char_code * 8] : hd44780_cgrom[current_char_code];
	for (uint8_t i = 0; i < 8; i++)
		player_surrounding[i] |= current_char[i];
	current_char_code = level_row_2[player_y_char + 1];
	current_char = current_char_code < 8 ? &lcd_cgram[current_char_code * 8] : hd44780_cgrom[current_char_code];
	for (uint8_t i = 0; i < 8; i++)
		player_surrounding[8 + i] |= current_char[i];
	current_char_code = level_row_alt_1[player_y_char];
	current_char = current_char_code < 8 ? &lcd_cgram[current_char_code * 8] : hd44780_cgrom[current_char_code];
	for (uint8_t i = 0; i < 8; i++)
		player_surrounding_alt[i] = current_char[i] << 5;
	current_char_code = level_row_alt_2[player_y_char];
	current_char = current_char_code < 8 ? &lcd_cgram[current_char_code * 8] : hd44780_cgrom[current_char_code];
	for (uint8_t i = 0; i < 8; i++)
		player_surrounding_alt[8 + i] = current_char[i] << 5;
	current_char_code = level_row_alt_1[player_y_char + 1];
	current_char = current_char_code < 8 ? &lcd_cgram[current_char_code * 8] : hd44780_cgrom[current_char_code];
	for (uint8_t i = 0; i < 8; i++)
		player_surrounding_alt[i] |= current_char[i];
	current_char_code = level_row_alt_2[player_y_char + 1];
	current_char = current_char_code < 8 ? &lcd_cgram[current_char_code * 8] : hd44780_cgrom[current_char_code];
	for (uint8_t i = 0; i < 8; i++)
		player_surrounding_alt[8 + i] |= current_char[i];
}

void draw_character()
{
	update_player_surrounding();
	for (uint8_t i = 0; i < 16; i++)
	{
		player_display_buffer[i] = player_surrounding[i];
	}
	uint8_t player_x_pixel = player_x >> 12;
	uint8_t player_y_pixel = player_y >> 12;
	player_display_buffer[player_x_pixel] ^= 0b1100000000 >> player_y_pixel;
	player_display_buffer[player_x_pixel + 1] ^= 0b1100000000 >> player_y_pixel;
	for (uint8_t i = 0; i < 8; i++)
	{
		lcd_cgram[i] = player_display_buffer[i] >> 5;
		lcd_cgram[8 + i] = player_display_buffer[i] & 0b11111;
		lcd_cgram[16 + i] = player_display_buffer[8 + i] >> 5;
		lcd_cgram[24 + i] = player_display_buffer[8 + i] & 0b11111;
	}
	lcd_row_1[player_y_char] = 0;
	lcd_row_1[player_y_char + 1] = 1;
	lcd_row_2[player_y_char] = 2;
	lcd_row_2[player_y_char + 1] = 3;
}

void process_controls()
{
	if (!(PINA & 0b00000001))
	{
		player_vel_x -= 15;
	}
	if (!(PINA & 0b00000010))
	{
		if (player_on_floor)
		{
			player_vel_y = -800;
			player_jump_frames = 100;
		}
	}
	else
	{
		player_jump_frames = 0;
	}
	if (!(PINA & 0b00001000))
	{
		player_vel_y += 15;
	}
	if (!(PINA & 0b00010000))
	{
		player_vel_x += 15;
	}
}

void friction()
{
	if (player_vel_x > 5)
	{
		player_vel_x -= 5;
	}
	else if (player_vel_x > -5)
	{
		player_vel_x = 0;
	}
	else
	{
		player_vel_x += 5;
	}
	if (player_vel_y > 5)
	{
		player_vel_y -= 5;
	}
	else if (player_vel_y > -5)
	{
		player_vel_y = 0;
	}
	else
	{
		player_vel_y += 5;
	}
}

void move_with_speed()
{
	if (player_vel_x < 0 && player_x < -player_vel_x)
	{
		player_vel_x = 0;
		player_x = 0;
	}
	else if (player_vel_x > 0 && player_x > 0xEFFF - player_vel_x)
	{
		player_vel_x = 0;
		player_x = 0xEFFF;
	}
	else
	{
		player_x += player_vel_x;
	}
	player_y += player_vel_y;
	if (player_y >= 0x7000)
	{
		player_y -= 0x5000;
		player_y_char++;
	}
	else if (player_y < 0x2000)
	{
		player_y += 0x5000;
		player_y_char--;
	}
}

void fix_player_position_n()
{
	if ((player_y & 0xFFF) >= 0x800)
	{
		player_vel_x = 0;
	}
	player_y |= 0x0FFF;
	player_y -= 0x1000;
	if (player_vel_y > 0)
	{
		player_vel_y = 0;
	}
}

void fix_player_position_s()
{
	player_y &= 0xF000;
	player_y += 0x1000;
	if (player_vel_y < 0)
	{
		player_vel_y = 0;
	}
}

void fix_player_position_e()
{
	player_x &= 0xF000;
	player_x += 0x1000;
	if (player_vel_x < 0)
	{
		player_vel_x = 0;
	}
}

void fix_player_position_w()
{
	player_x |= 0x0FFF;
	player_x -= 0x1000;
	if (player_vel_x > 0)
	{
		player_vel_x = 0;
	}
}

void floor_detection()
{
	uint8_t player_x_pixel = player_x >> 12;
	uint8_t player_y_pixel = player_y >> 12;
	player_on_floor = (((player_surrounding[player_x_pixel] & player_surrounding_alt[player_x_pixel]) << (player_y_pixel)) & 0b10000000) ||
					  (((player_surrounding[player_x_pixel + 1] & player_surrounding_alt[player_x_pixel + 1]) << (player_y_pixel)) & 0b10000000);
}

uint8_t is_player_dead()
{
	uint8_t player_x_pixel = player_x >> 12;
	uint8_t player_y_pixel_red = (player_y >> 12) - 1;
	uint8_t player_collision1 = ((player_surrounding[player_x_pixel] | player_surrounding_alt[player_x_pixel]) << player_y_pixel_red) >> 6;
	uint8_t player_collision2 = ((player_surrounding[player_x_pixel + 1] | player_surrounding_alt[player_x_pixel + 1]) << player_y_pixel_red) >> 6;
	return player_collision1 & 0b0100 || player_collision2 & 0b0100;
}

void fix_player_position()
{
	uint8_t player_x_pixel = player_x >> 12;
	uint8_t player_y_pixel_red = (player_y >> 12) - 1;
	uint8_t player_collision0 = 0b1111;
	if (player_x_pixel > 0)
	{
		player_collision0 = ((player_surrounding[player_x_pixel - 1] & player_surrounding_alt[player_x_pixel - 1]) << player_y_pixel_red) >> 6;
	}
	uint8_t player_collision1 = ((player_surrounding[player_x_pixel] & player_surrounding_alt[player_x_pixel]) << player_y_pixel_red) >> 6;
	uint8_t player_collision2 = ((player_surrounding[player_x_pixel + 1] & player_surrounding_alt[player_x_pixel + 1]) << player_y_pixel_red) >> 6;
	uint8_t player_collision3 = 0b1111;
	if (player_x_pixel < 14)
	{
		player_collision3 = ((player_surrounding[player_x_pixel + 2] & player_surrounding_alt[player_x_pixel + 2]) << player_y_pixel_red) >> 6;
	}
	if (!(player_collision1 & 0b0110) &&
		!(player_collision2 & 0b0110))
	{
		return;
	}
	uint8_t can_translate_n = !(player_collision1 & 0b1100) &&
							  !(player_collision2 & 0b1100);
	uint8_t can_translate_s = !(player_collision1 & 0b0011) &&
							  !(player_collision2 & 0b0011);
	uint8_t can_translate_e = !(player_collision2 & 0b0110) &&
							  !(player_collision3 & 0b0110);
	uint8_t can_translate_ne = !(player_collision2 & 0b1100) &&
							   !(player_collision3 & 0b1100);
	uint8_t can_translate_se = !(player_collision2 & 0b0011) &&
							   !(player_collision3 & 0b0011);
	uint8_t can_translate_w = !(player_collision0 & 0b0110) &&
							  !(player_collision1 & 0b0110);
	uint8_t can_translate_nw = !(player_collision0 & 0b1100) &&
							   !(player_collision1 & 0b1100);
	uint8_t can_translate_sw = !(player_collision0 & 0b0011) &&
							   !(player_collision1 & 0b0011);
	if (can_translate_n)
	{
		fix_player_position_n();
	}
	else if (can_translate_e)
	{
		fix_player_position_e();
	}
	else if (can_translate_w)
	{
		fix_player_position_w();
	}
	else if (can_translate_s)
	{
		fix_player_position_s();
	}
	else if (player_vel_y >= 0)
	{
		if (can_translate_ne)
		{
			fix_player_position_n();
			fix_player_position_e();
		}
		else if (can_translate_nw)
		{
			fix_player_position_n();
			fix_player_position_w();
		}
		else if (can_translate_se)
		{
			fix_player_position_s();
			fix_player_position_e();
		}
		else if (can_translate_sw)
		{
			fix_player_position_s();
			fix_player_position_w();
		}
	}
	else
	{
		if (can_translate_se)
		{
			fix_player_position_s();
			fix_player_position_e();
		}
		else if (can_translate_sw)
		{
			fix_player_position_s();
			fix_player_position_w();
		}
		else if (can_translate_ne)
		{
			fix_player_position_n();
			fix_player_position_e();
		}
		else if (can_translate_nw)
		{
			fix_player_position_n();
			fix_player_position_w();
		}
	}
}

int main()
{
	port_init();
	lcd_init();

	lcd_cgram[4 * 8] = 0b00100;
	lcd_cgram[4 * 8 + 1] = 0b00100;
	lcd_cgram[4 * 8 + 2] = 0b00100;
	lcd_cgram[4 * 8 + 3] = 0b00100;
	lcd_cgram[4 * 8 + 4] = 0b11100;
	lcd_cgram[4 * 8 + 5] = 0b00100;
	lcd_cgram[4 * 8 + 6] = 0b00100;
	lcd_cgram[4 * 8 + 7] = 0b00100;
	lcd_cgram[5 * 8] = 0b00010;
	lcd_cgram[5 * 8 + 1] = 0b00010;
	lcd_cgram[5 * 8 + 2] = 0b00010;
	lcd_cgram[5 * 8 + 3] = 0b00010;
	lcd_cgram[5 * 8 + 4] = 0b00010;
	lcd_cgram[5 * 8 + 5] = 0b00010;
	lcd_cgram[5 * 8 + 6] = 0b00010;
	lcd_cgram[5 * 8 + 7] = 0b00010;
	while (1)
	{
		uint8_t is_alt = (frame & 0x7) >= 0x4 ? 1 : 0;
		uint8_t frame_num = (frame & 0xFF) >= 0x80 ? 1 : 0;
		level_row_1 = level_row[frame_num][1 - is_alt][0];
		level_row_2 = level_row[frame_num][1 - is_alt][1];
		level_row_alt_1 = level_row[frame_num][is_alt][0];
		level_row_alt_2 = level_row[frame_num][is_alt][1];
		floor_detection();
		process_controls();
		if (player_vel_y >= 0)
		{
			player_jump_frames = 0;
		}
		if (!player_on_floor)
		{
			if (player_jump_frames > 0)
			{
				player_vel_y += 5;
				player_jump_frames--;
			}
			else
			{
				player_vel_y += 20;
			}
		}
		friction();
		move_with_speed();
		update_player_surrounding();
		fix_player_position();
		if (is_player_dead())
		{
			player_x = player_reset_x;
			player_y = player_reset_y;
			player_vel_x = 0;
			player_vel_y = 0;
			player_y_char = player_reset_y_char;
		}
		for (unsigned int i = 0; i < 16; ++i)
		{
			lcd_row_1[i] = level_row_1[i];
		}
		for (unsigned int i = 0; i < 16; ++i)
		{
			lcd_row_2[i] = level_row_2[i];
		}
		draw_character();
		// hide character before modifying cgram significantly
		if (player_y_char_old != player_y_char)
		{
			lcd_send_command(DD_RAM_ADDR + player_y_char_old);
			lcd_send_char(level_row_1[player_y_char_old]);
			lcd_send_char(level_row_1[player_y_char_old + 1]);
			lcd_send_command(DD_RAM_ADDR2 + player_y_char_old);
			lcd_send_char(level_row_2[player_y_char_old]);
			lcd_send_char(level_row_2[player_y_char_old + 1]);
			player_y_char_old = player_y_char;
		}
		lcd_update();
		frame++;
	}

	return 0;
}
