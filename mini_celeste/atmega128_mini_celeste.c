// NOTE: lines below added to allow compilation in simavr's build system
#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");

#define __AVR_ATMEGA128__ 1
#define __AVR_ATmega128__ 1
#include "avr/io.h"
#include "avr/sleep.h"
#include "avr/interrupt.h"
#include "lcd_helper.h"
#include "hd44780_cgrom.h"
#include "mini_celeste_level_data.h"
#include "gameplay_constants.h"

#define PLAYER_MAX_VELOCITY 0xFF0

#define PLAYER_DASH_VISUAL_FRAMES_1 5
#define PLAYER_DASH_VISUAL_FRAMES_2 10

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

static uint16_t player_x = 0x3000;
static uint16_t player_y = 0x3000;
static int16_t player_vel_x = 0;
static int16_t player_vel_y = 0;
static uint8_t player_y_char = 13;
static uint8_t player_y_char_old = 13;
static uint8_t player_jump_frames = 0;
static uint8_t player_on_floor = 0;
static uint8_t player_on_e_wall = 0;
static uint8_t player_on_w_wall = 0;
static uint8_t player_jump_button_down = 0;
static uint8_t player_dash_button_down = 0;
static uint8_t player_jump_buffer = 0;
static uint8_t player_dash_remaining = 1;
static uint8_t player_dash_timer = 0;
static uint8_t player_dash_frames_in_movement = 0;
static uint8_t player_dash_input_timer = 0;
static int8_t player_dash_default_x_direction = 1;
static int16_t player_vel_x_before_dash = 0;
static uint8_t player_wall_friction = 0;
static int8_t player_dash_dir_x = 0;
static int8_t player_dash_dir_y = 0;
static uint8_t player_death_timer = 0;
static uint16_t camera_position = LEVEL_SEGMENT_HEIGHT * (LEVEL_SEGMENT_NUMBER - 1);
static int8_t camera_remaining_delta = 0;
static uint8_t camera_delta_timer = 0;
static uint8_t *level_row_1 = level_data_characters[0][0][0];
static uint8_t *level_row_2 = level_data_characters[0][0][1];
static uint8_t *level_row_alt_1 = level_data_characters[0][1][0];
static uint8_t *level_row_alt_2 = level_data_characters[0][1][1];
static uint16_t player_surrounding[16] = {};
static uint16_t player_surrounding_alt[16] = {};
static uint16_t player_display_buffer[16] = {};
static uint16_t level_switch_timer = 0;
static uint8_t level_switch_state = 0;
static uint16_t level_current_segment = INIT_SEGMENT;
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

void draw_dash()
{
	if (player_dash_timer == 0)
		return;
	uint8_t player_x_pixel = player_x >> 12;
	uint8_t player_y_pixel = player_y >> 12;
	if (player_dash_dir_x == 1 && player_dash_dir_y == 0)
	{
		if (player_x_pixel > 0 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_1)
			player_display_buffer[player_x_pixel - 1] |= 0b001000000000 >> player_y_pixel;
		if (player_x_pixel > 1 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_2)
			player_display_buffer[player_x_pixel - 2] |= 0b000100000000 >> player_y_pixel;
	}
	else if (player_dash_dir_x == -1 && player_dash_dir_y == 0)
	{
		if (player_x_pixel < 14 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_1)
			player_display_buffer[player_x_pixel + 2] |= 0b001000000000 >> player_y_pixel;
		if (player_x_pixel < 13 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_2)
			player_display_buffer[player_x_pixel + 3] |= 0b000100000000 >> player_y_pixel;
	}
	else if (player_dash_dir_x == 0 && player_dash_dir_y == -1)
	{
		if (player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_1)
			player_display_buffer[player_x_pixel + 0] |= 0b000010000000 >> player_y_pixel;
		if (player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_2)
			player_display_buffer[player_x_pixel + 1] |= 0b000001000000 >> player_y_pixel;
	}
	else if (player_dash_dir_x == 0 && player_dash_dir_y == 1)
	{
		if (player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_1)
			player_display_buffer[player_x_pixel + 0] |= 0b100000000000 >> player_y_pixel;
		if (player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_2)
			player_display_buffer[player_x_pixel + 1] |= 0b010000000000 >> player_y_pixel;
	}
	else if (player_dash_dir_x == 1 && player_dash_dir_y == -1)
	{
		if (player_x_pixel > 0 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_1)
			player_display_buffer[player_x_pixel - 1] |= 0b000010000000 >> player_y_pixel;
		if (player_x_pixel > 1 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_2)
			player_display_buffer[player_x_pixel - 2] |= 0b000001000000 >> player_y_pixel;
	}
	else if (player_dash_dir_x == 1 && player_dash_dir_y == 1)
	{
		if (player_x_pixel > 0 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_1)
			player_display_buffer[player_x_pixel - 1] |= 0b010000000000 >> player_y_pixel;
		if (player_x_pixel > 1 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_2)
			player_display_buffer[player_x_pixel - 2] |= 0b100000000000 >> player_y_pixel;
	}
	else if (player_dash_dir_x == -1 && player_dash_dir_y == -1)
	{
		if (player_x_pixel < 14 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_1)
			player_display_buffer[player_x_pixel + 2] |= 0b000010000000 >> player_y_pixel;
		if (player_x_pixel < 13 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_2)
			player_display_buffer[player_x_pixel + 3] |= 0b000001000000 >> player_y_pixel;
	}
	else if (player_dash_dir_x == -1 && player_dash_dir_y == 1)
	{
		if (player_x_pixel < 14 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_1)
			player_display_buffer[player_x_pixel + 2] |= 0b010000000000 >> player_y_pixel;
		if (player_x_pixel < 13 && player_dash_frames_in_movement > PLAYER_DASH_VISUAL_FRAMES_2)
			player_display_buffer[player_x_pixel + 3] |= 0b100000000000 >> player_y_pixel;
	}
}

void draw_character_dying()
{
	uint8_t player_x_pixel = player_x >> 12;
	uint8_t player_y_pixel = player_y >> 12;
	if ((frame & 0x1F) >= 0x10)
	{
		if (player_x_pixel > 0)
			player_display_buffer[player_x_pixel - 1] ^= 0b010010000000 >> player_y_pixel;
		if (player_x_pixel < 14)
			player_display_buffer[player_x_pixel + 2] ^= 0b010010000000 >> player_y_pixel;
	}
	else
	{
		if (player_x_pixel > 0)
			player_display_buffer[player_x_pixel - 1] ^= 0b001100000000 >> player_y_pixel;
		if (player_x_pixel < 14)
			player_display_buffer[player_x_pixel + 2] ^= 0b001100000000 >> player_y_pixel;
		player_display_buffer[player_x_pixel] ^= 0b010010000000 >> player_y_pixel;
		player_display_buffer[player_x_pixel + 1] ^= 0b010010000000 >> player_y_pixel;
	}
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
	if (player_death_timer > 0)
	{
		draw_character_dying();
	}
	else
	{
		if (player_dash_remaining || (frame & 0x8) >= 0x4)
		{
			player_display_buffer[player_x_pixel] |= 0b1000000000 >> player_y_pixel;
			player_display_buffer[player_x_pixel + 1] |= 0b0100000000 >> player_y_pixel;
		}
		if (player_dash_remaining || (frame & 0x8) < 0x4)
		{
			player_display_buffer[player_x_pixel] |= 0b0100000000 >> player_y_pixel;
			player_display_buffer[player_x_pixel + 1] |= 0b1000000000 >> player_y_pixel;
		}
	}
	draw_dash();
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

void process_controls() //
{
	if (!(PINA & 0b00000001))
	{
		player_vel_x -= player_on_floor ? PLAYER_WALK_VELOCITY : PLAYER_AIR_VELOCITY_X;
		player_dash_default_x_direction = -1;
	}
	if (!(PINA & 0b00000010))
	{
		if (player_jump_button_down == 0)
		{
			player_jump_buffer = PLAYER_JUMP_BUFFER_FRAMES;
		}
		player_jump_button_down = 1;
	}
	else
	{
		player_jump_button_down = 0;
		player_jump_frames = 0;
	}
	if (player_jump_buffer > 0)
	{
		player_jump_buffer--;
		if (player_on_floor)
		{
			player_vel_y = -PLAYER_JUMP_VELOCITY;
			player_jump_frames = PLAYER_JUMP_FRAMES_MAX;
			player_jump_buffer = 0;
			player_dash_timer = 0;
		}
		else if (!player_on_e_wall && player_on_w_wall)
		{
			player_vel_y = (player_dash_timer > 0 && player_dash_dir_x == 0 && player_dash_dir_y == -1) ? -PLAYER_WALL_JUMP_VELOCITY_Y_BOOSTED : -PLAYER_WALL_JUMP_VELOCITY_Y;
			player_vel_x = PLAYER_WALL_JUMP_VELOCITY_X;
			player_dash_default_x_direction = 1;
			player_jump_frames = PLAYER_WALL_JUMP_FRAMES_MAX;
			player_jump_buffer = 0;
			player_dash_timer = 0;
		}
		else if (player_on_e_wall && !player_on_w_wall)
		{
			player_vel_y = (player_dash_timer > 0 && player_dash_dir_x == 0 && player_dash_dir_y == -1) ? -PLAYER_WALL_JUMP_VELOCITY_Y_BOOSTED : -PLAYER_WALL_JUMP_VELOCITY_Y;
			player_vel_x = -PLAYER_WALL_JUMP_VELOCITY_X;
			player_dash_default_x_direction = -1;
			player_jump_frames = PLAYER_WALL_JUMP_FRAMES_MAX;
			player_jump_buffer = 0;
			player_dash_timer = 0;
		}
		else if (player_on_e_wall && player_on_w_wall)
		{
			player_vel_y = -PLAYER_WALL_JUMP_VELOCITY_Y;
			player_jump_frames = PLAYER_WALL_JUMP_FRAMES_MAX;
			player_jump_buffer = 0;
			player_dash_timer = 0;
		}
	}
	if (!(PINA & 0b00001000))
	{
		player_vel_y += PLAYER_AIR_VELOCITY_Y;
	}
	if (!(PINA & 0b00010000))
	{
		player_vel_x += player_on_floor ? PLAYER_WALK_VELOCITY : PLAYER_AIR_VELOCITY_X;
		player_dash_default_x_direction = 1;
	}
	if (!(PINA & 0b00000100))
	{
		if (!player_dash_button_down && player_dash_remaining > 0)
		{
			player_dash_remaining = 0;
			player_dash_timer = PLAYER_DASH_FRAMES;
			player_dash_input_timer = PLAYER_DASH_INPUT_FRAMES;
			player_dash_frames_in_movement = 0;
			player_dash_dir_x = 0;
			player_dash_dir_y = 0;
			player_vel_x_before_dash = player_vel_x;
		}
		player_dash_button_down = 1;
	}
	else
	{
		player_dash_button_down = 0;
	}
	if (player_dash_timer > 0)
	{
		player_dash_timer--;
		if (player_dash_input_timer > 0)
		{
			player_dash_input_timer--;
			// listen for input
			if (player_dash_dir_x == 0)
			{
				if (!(PINA & 0b00000001))
					player_dash_dir_x--;
				if (!(PINA & 0b00010000))
					player_dash_dir_x++;
			}
			if (player_dash_dir_y == 0)
			{
				if (!(PINA & 0b00000010))
					player_dash_dir_y--;
				if (!(PINA & 0b00001000))
					player_dash_dir_y++;
			}
			player_vel_x = 0;
			player_vel_y = 0;
		}
		else
		{
			player_dash_frames_in_movement++;
			if (player_dash_dir_x == 0 && player_dash_dir_y == 0)
			{
				player_dash_dir_x = player_dash_default_x_direction;
				player_dash_dir_y = 0;
			}
			if (player_dash_dir_x != 0 && player_dash_dir_y == 1)
			{
				player_vel_x = PLAYER_DASH_DIAGONAL_VELOCITY * player_dash_dir_x + player_vel_x_before_dash;
				player_vel_y = PLAYER_DASH_DIAGONAL_VELOCITY * player_dash_dir_y;
			}
			else if (player_dash_dir_x != 0 && player_dash_dir_y == -1)
			{
				player_vel_x = PLAYER_DASH_DIAGONAL_VELOCITY * player_dash_dir_x;
				player_vel_y = PLAYER_DASH_DIAGONAL_VELOCITY * player_dash_dir_y;
			}
			else
			{
				player_vel_x = PLAYER_DASH_VELOCITY * player_dash_dir_x;
				player_vel_y = PLAYER_DASH_VELOCITY * player_dash_dir_y;
			}
		}
	}
	if (player_on_floor && player_dash_timer < PLAYER_DASH_REGAIN_FRAMES)
		player_dash_remaining = 1;
}

void friction()
{
	int16_t vel_x_reduction = player_on_floor ? FRICTION_X_GROUND(player_vel_x) : FRICTION_X_AIR(player_vel_x);
	if (vel_x_reduction != 0)
		player_vel_x -= vel_x_reduction;
	else if (player_vel_x > 0)
		player_vel_x--;
	else if (player_vel_x < 0)
		player_vel_x++;
	if (player_vel_x > PLAYER_MAX_VELOCITY)
		player_vel_x = PLAYER_MAX_VELOCITY;
	else if (player_vel_x < -PLAYER_MAX_VELOCITY)
		player_vel_x = -PLAYER_MAX_VELOCITY;
	int16_t vel_y_reduction = (player_wall_friction && player_vel_y > 0) ? FRICTION_Y_WALL(player_vel_y) : FRICTION_Y_AIR(player_vel_y);
	if (vel_y_reduction != 0)
		player_vel_y -= vel_y_reduction;
	else if (player_vel_y > 0)
		player_vel_y--;
	else if (player_vel_y < 0)
		player_vel_y++;
	if (player_vel_y > PLAYER_MAX_VELOCITY)
		player_vel_y = PLAYER_MAX_VELOCITY;
	else if (player_vel_y < -PLAYER_MAX_VELOCITY)
		player_vel_y = -PLAYER_MAX_VELOCITY;
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
		player_wall_friction = 1;
	}
}

void fix_player_position_w()
{
	player_x |= 0x0FFF;
	player_x -= 0x1000;
	if (player_vel_x > 0)
	{
		player_vel_x = 0;
		player_wall_friction = 1;
	}
}

void floor_detection()
{
	uint8_t player_x_pixel = player_x >> 12;
	uint8_t player_y_pixel = player_y >> 12;
	player_on_floor = (((player_surrounding[player_x_pixel] & player_surrounding_alt[player_x_pixel]) << player_y_pixel) & 0b10000000) ||
					  (((player_surrounding[player_x_pixel + 1] & player_surrounding_alt[player_x_pixel + 1]) << player_y_pixel) & 0b10000000);
	if (player_x_pixel > 0)
	{
		player_on_w_wall = (player_surrounding[player_x_pixel - 1] & player_surrounding_alt[player_x_pixel - 1]) & (0b1100000000 >> player_y_pixel);
	}
	else
	{
		player_on_w_wall = 0;
	}
	if (player_x_pixel < 14)
	{
		player_on_e_wall = (player_surrounding[player_x_pixel + 2] & player_surrounding_alt[player_x_pixel + 2]) & (0b1100000000 >> player_y_pixel);
	}
	else
	{
		player_on_e_wall = 0;
	}
}

uint8_t is_player_dead()
{
	uint8_t player_x_pixel = player_x >> 12;
	uint8_t player_y_pixel_red = (player_y >> 12) - 1;
	uint8_t player_collision1 = ((player_surrounding[player_x_pixel] | player_surrounding_alt[player_x_pixel]) << player_y_pixel_red) >> 6;
	uint8_t player_collision2 = ((player_surrounding[player_x_pixel + 1] | player_surrounding_alt[player_x_pixel + 1]) << player_y_pixel_red) >> 6;
	return player_collision1 & 0b0110 || player_collision2 & 0b0110;
}

void fix_player_position()
{
	player_wall_friction = 0;
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
	uint8_t can_translate_e_not_extremum = ((player_x & 0xFFF) != 0x000);
	uint8_t can_translate_w_not_extremum = ((player_x & 0xFFF) != 0xFFF);
	uint8_t can_translate_n = !(player_collision1 & 0b1100) &&
							  !(player_collision2 & 0b1100);
	uint8_t can_translate_s = !(player_collision1 & 0b0011) &&
							  !(player_collision2 & 0b0011);
	uint8_t can_translate_e = !(player_collision2 & 0b0110) &&
							  !(player_collision3 & 0b0110) &&
							  can_translate_e_not_extremum;
	uint8_t can_translate_ne = !(player_collision2 & 0b1100) &&
							   !(player_collision3 & 0b1100) &&
							   can_translate_e_not_extremum;
	uint8_t can_translate_se = !(player_collision2 & 0b0011) &&
							   !(player_collision3 & 0b0011) &&
							   can_translate_e_not_extremum;
	uint8_t can_translate_w = !(player_collision0 & 0b0110) &&
							  !(player_collision1 & 0b0110) &&
							  can_translate_w_not_extremum;
	uint8_t can_translate_nw = !(player_collision0 & 0b1100) &&
							   !(player_collision1 & 0b1100) &&
							   can_translate_w_not_extremum;
	uint8_t can_translate_sw = !(player_collision0 & 0b0011) &&
							   !(player_collision1 & 0b0011) &&
							   can_translate_w_not_extremum;
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

ISR(TIMER1_COMPA_vect)
{
}

void screen_main_game()
{
	player_x = level_data_player_reset_x[INIT_SEGMENT];
	player_y = level_data_player_reset_y[INIT_SEGMENT];
	player_y_char = level_data_player_reset_y_char[INIT_SEGMENT];
	player_vel_x = 0;
	player_vel_y = 0;
	player_y_char_old = player_y_char;
	player_jump_frames = 0;
	player_on_floor = 0;
	player_on_e_wall = 0;
	player_on_w_wall = 0;
	player_jump_button_down = 0;
	player_dash_button_down = 0;
	player_jump_buffer = 0;
	player_dash_remaining = 1;
	player_dash_timer = 0;
	player_dash_frames_in_movement = 0;
	player_dash_input_timer = 0;
	player_dash_default_x_direction = 1;
	player_vel_x_before_dash = 0;
	player_wall_friction = 0;
	player_dash_dir_x = 0;
	player_dash_dir_y = 0;
	player_death_timer = 0;
	camera_remaining_delta = 0;
	camera_delta_timer = 0;
	level_switch_timer = 0;
	level_switch_state = 0;
	level_current_segment = INIT_SEGMENT;
	camera_position = LEVEL_SEGMENT_HEIGHT * (LEVEL_SEGMENT_NUMBER - 1 - INIT_SEGMENT);
	frame = 0;
	for (uint8_t i = 0; i < 4; i++)
		for (uint8_t j = 0; j < 8; j++)
			lcd_cgram[i * 8 + j + 32] = level_data_spec_cherecters[level_current_segment][j][i];
	while (1)
	{
		sleep_mode();
		uint8_t is_alt = (frame & 0x7) >= 0x4 ? 1 : 0;
		level_switch_timer++;
		if (level_switch_timer >= level_data_switch_times[level_current_segment][level_switch_state])
		{
			level_switch_state = 1 - level_switch_state;
			level_switch_timer = 0;
		}
		level_row_1 = level_data_characters[level_switch_state][1 - is_alt][0] + camera_position;
		level_row_2 = level_data_characters[level_switch_state][1 - is_alt][1] + camera_position;
		level_row_alt_1 = level_data_characters[level_switch_state][is_alt][0] + camera_position;
		level_row_alt_2 = level_data_characters[level_switch_state][is_alt][1] + camera_position;
		if (player_death_timer == 0 && camera_remaining_delta == 0)
		{
			floor_detection();
			process_controls();
			if (player_dash_timer == 0)
			{
				if (player_vel_y >= 0)
				{
					player_jump_frames = 0;
				}
				if (player_jump_frames > 0)
				{
					player_jump_frames--;
				}
				if (!(player_on_floor && (player_y & 0xFFF) == 0xFFF))
				{
					player_vel_y += (player_jump_frames > 0) ? GRAVITY_REDUCED : GRAVITY;
				}
				friction();
			}
			move_with_speed();
			update_player_surrounding();
			fix_player_position();
			if (is_player_dead())
			{
				player_death_timer = DEATH_TIMER_FRAMES;
			}
			if (player_y_char == 0)
			{
				camera_remaining_delta = -LEVEL_SEGMENT_HEIGHT;
				camera_delta_timer = 0;
				level_current_segment++;
			}
			else if (player_y_char >= 14)
			{
				camera_remaining_delta = LEVEL_SEGMENT_HEIGHT;
				camera_delta_timer = 0;
				level_current_segment--;
			}
		}
		else if (camera_remaining_delta != 0)
		{
			if (camera_delta_timer == 0)
			{
				camera_delta_timer = CAMERA_DELTA_FRAMES;
				if (camera_remaining_delta > 0)
				{
					camera_remaining_delta--;
					camera_position++;
					player_y_char--;
				}
				else
				{
					camera_remaining_delta++;
					camera_position--;
					player_y_char++;
				}
			}
			else
			{
				camera_delta_timer--;
			}
		}
		else if (player_death_timer > 0)
		{
			player_death_timer--;
			if (player_death_timer == DEATH_TIMER_RESET_FRAME)
			{
				player_x = level_data_player_reset_x[level_current_segment];
				player_y = level_data_player_reset_y[level_current_segment];
				player_vel_x = 0;
				player_vel_y = 0;
				player_y_char = level_data_player_reset_y_char[level_current_segment];
				player_jump_frames = 0;
				player_on_floor = 0;
				player_on_e_wall = 0;
				player_on_w_wall = 0;
				player_jump_buffer = 0;
				player_dash_remaining = 1;
				player_dash_timer = 0;
				player_dash_frames_in_movement = 0;
				player_dash_input_timer = 0;
				player_dash_default_x_direction = 1;
				player_vel_x_before_dash = 0;
				player_wall_friction = 0;
				player_dash_dir_x = 0;
				player_dash_dir_y = 0;
				update_player_surrounding();
			}
		}
		if (level_current_segment >= LEVEL_SEGMENT_NUMBER - 1 && player_y_char < 7 && camera_remaining_delta == 0)
		{
			return;
		}
		if ((frame & 0b11) == 0)
		{
			// draw
			for (uint8_t i = 0; i < 4; i++)
				for (uint8_t j = 0; j < 8; j++)
					lcd_cgram[i * 8 + j + 32] = level_data_spec_cherecters[level_current_segment][j][i];
			for (unsigned int i = 0; i < 16; ++i)
			{
				lcd_row_1[i] = level_row_1[i];
			}
			for (unsigned int i = 0; i < 16; ++i)
			{
				lcd_row_2[i] = level_row_2[i];
			}
			draw_character();
			if (player_death_timer > 0)
			{
				if (player_death_timer >= DEATH_TIMER_ANIM_5_FRAME && player_death_timer < DEATH_TIMER_ANIM_1_FRAME)
					for (uint8_t i = 0; i < 16; i += 2)
						lcd_row_1[i] = 255;
				if (player_death_timer >= DEATH_TIMER_ANIM_6_FRAME && player_death_timer < DEATH_TIMER_ANIM_2_FRAME)
					for (uint8_t i = 0; i < 16; i += 2)
						lcd_row_2[i] = 255;
				if (player_death_timer >= DEATH_TIMER_ANIM_7_FRAME && player_death_timer < DEATH_TIMER_ANIM_3_FRAME)
					for (uint8_t i = 1; i < 16; i += 2)
						lcd_row_1[i] = 255;
				if (player_death_timer < DEATH_TIMER_ANIM_4_FRAME)
					for (uint8_t i = 1; i < 16; i += 2)
						lcd_row_2[i] = 255;
			}
			else if (player_y_char_old != player_y_char)
			{
				// hide character before modifying cgram significantly
				lcd_send_command(DD_RAM_ADDR + player_y_char_old);
				lcd_send_char(level_row_1[player_y_char_old]);
				lcd_send_char(level_row_1[player_y_char_old + 1]);
				lcd_send_command(DD_RAM_ADDR2 + player_y_char_old);
				lcd_send_char(level_row_2[player_y_char_old]);
				lcd_send_char(level_row_2[player_y_char_old + 1]);
				player_y_char_old = player_y_char;
			}
			lcd_update();
		}
		frame++;
	}
}

void wait_full_middle_press()
{
	// wait for button release
	while (!(PINA & 0b00000100))
		sleep_mode();
	// wait for button press & release
	while (PINA & 0b00000100)
		sleep_mode();
	while (!(PINA & 0b00000100))
		sleep_mode();
}

void screen_finish()
{
	for (uint8_t i = 0; i < 16; i++)
		lcd_row_1[i] = ' ';
	for (uint8_t i = 0; i < 16; i++)
		lcd_row_2[i] = ' ';
	lcd_row_1[4] = 'F';
	lcd_row_1[5] = 'i';
	lcd_row_1[6] = 'n';
	lcd_row_1[7] = 'i';
	lcd_row_1[8] = 's';
	lcd_row_1[9] = 'h';
	lcd_row_1[10] = '!';
	lcd_row_2[0] = 'T';
	lcd_row_2[1] = 'i';
	lcd_row_2[2] = 'm';
	lcd_row_2[3] = 'e';
	lcd_row_2[4] = ':';
	lcd_row_2[15] = 's';
	lcd_row_2[12] = '.';
	uint32_t frame_tmp = frame;
	lcd_row_2[14] = frame_tmp % 10 + '0';
	frame_tmp /= 10;
	lcd_row_2[13] = frame_tmp % 10 + '0';
	frame_tmp /= 10;
	lcd_row_2[11] = frame_tmp % 10 + '0';
	frame_tmp /= 10;
	uint8_t i = 10;
	while (frame_tmp > 0)
	{
		lcd_row_2[i] = frame_tmp % 10 + '0';
		frame_tmp /= 10;
		i--;
	}

	lcd_update();
	wait_full_middle_press();
}

void lcd_print_delay(uint8_t c, uint8_t x, uint8_t row, uint8_t wait)
{
	(row ? lcd_row_2 : lcd_row_1)[x] = c;
	lcd_update();
	for (uint8_t i = 0; i < wait; i++)
		sleep_mode();
}

void lcd_shift()
{
	for (uint8_t j = 0; j < 16; j++)
	{
		for (uint8_t i = 15; i > 0; i--)
			lcd_row_1[i] = lcd_row_1[i - 1];
		for (uint8_t i = 15; i > 0; i--)
			lcd_row_2[i] = lcd_row_2[i - 1];
		lcd_row_1[0] = 255;
		lcd_row_2[0] = 255;
		lcd_update();
		for (uint8_t i = 0; i < 5; i++)
			sleep_mode();
	}
}

void screen_start()
{
	for (uint8_t i = 0; i < 16; i++)
		lcd_row_1[i] = ' ';
	for (uint8_t i = 0; i < 16; i++)
		lcd_row_2[i] = ' ';
	lcd_row_1[6] = 'M';
	lcd_row_1[7] = 'i';
	lcd_row_1[8] = 'n';
	lcd_row_1[9] = 'i';
	lcd_row_2[4] = 'C';
	lcd_row_2[5] = 'E';
	lcd_row_2[6] = 'L';
	lcd_row_2[7] = 'E';
	lcd_row_2[8] = 'S';
	lcd_row_2[9] = 'T';
	lcd_row_2[10] = 'E';
	lcd_update();
	wait_full_middle_press();
	for (uint8_t i = 0; i < 16; i++)
		lcd_row_1[i] = 255;
	for (uint8_t i = 0; i < 16; i++)
		lcd_row_2[i] = 255;
	lcd_update();
	for (uint8_t i = 0; i < 80; i++)
	{
		sleep_mode();
	}
	// this is it, Madeline.
	lcd_print_delay('T', 2, 0, 5);
	lcd_print_delay('h', 3, 0, 5);
	lcd_print_delay('i', 4, 0, 5);
	lcd_print_delay('s', 5, 0, 5);
	lcd_print_delay(' ', 6, 0, 5);
	lcd_print_delay('i', 7, 0, 5);
	lcd_print_delay('s', 8, 0, 5);
	lcd_print_delay(' ', 9, 0, 5);
	lcd_print_delay('i', 10, 0, 5);
	lcd_print_delay('t', 11, 0, 5);
	lcd_print_delay(',', 12, 0, 5);
	lcd_print_delay('M', 2, 1, 5);
	lcd_print_delay('a', 3, 1, 5);
	lcd_print_delay('d', 4, 1, 5);
	lcd_print_delay('e', 5, 1, 5);
	lcd_print_delay('l', 6, 1, 5);
	lcd_print_delay('i', 7, 1, 5);
	lcd_print_delay('n', 8, 1, 5);
	lcd_print_delay('e', 9, 1, 5);
	lcd_print_delay('.', 10, 1, 5);
	lcd_print_delay('.', 11, 1, 5);
	lcd_print_delay('.', 12, 1, 5);
	wait_full_middle_press();
	lcd_shift();
	// just breathe.
	lcd_print_delay('j', 1, 0, 5);
	lcd_print_delay('u', 2, 0, 5);
	lcd_print_delay('s', 3, 0, 5);
	lcd_print_delay('t', 4, 0, 5);
	lcd_print_delay(' ', 5, 0, 5);
	lcd_print_delay('b', 6, 0, 5);
	lcd_print_delay('r', 7, 0, 5);
	lcd_print_delay('e', 8, 0, 5);
	lcd_print_delay('a', 9, 0, 5);
	lcd_print_delay('t', 10, 0, 5);
	lcd_print_delay('h', 11, 0, 5);
	lcd_print_delay('e', 12, 0, 5);
	lcd_print_delay('.', 13, 0, 5);
	wait_full_middle_press();
	lcd_shift();
	// why are you so nervous?
	lcd_print_delay('W', 2, 0, 5);
	lcd_print_delay('h', 3, 0, 5);
	lcd_print_delay('y', 4, 0, 5);
	lcd_print_delay(' ', 5, 0, 5);
	lcd_print_delay('a', 6, 0, 5);
	lcd_print_delay('r', 7, 0, 5);
	lcd_print_delay('e', 8, 0, 5);
	lcd_print_delay(' ', 9, 0, 5);
	lcd_print_delay('y', 10, 0, 5);
	lcd_print_delay('o', 11, 0, 5);
	lcd_print_delay('u', 12, 0, 5);
	lcd_print_delay('s', 2, 1, 5);
	lcd_print_delay('o', 3, 1, 5);
	lcd_print_delay(' ', 4, 1, 5);
	lcd_print_delay('n', 5, 1, 5);
	lcd_print_delay('e', 6, 1, 5);
	lcd_print_delay('r', 7, 1, 5);
	lcd_print_delay('v', 8, 1, 5);
	lcd_print_delay('o', 9, 1, 5);
	lcd_print_delay('u', 10, 1, 5);
	lcd_print_delay('s', 11, 1, 5);
	lcd_print_delay('?', 12, 1, 5);
	wait_full_middle_press();
	lcd_shift();
	// you can do this.
	lcd_print_delay('Y', 4, 0, 5);
	lcd_print_delay('o', 5, 0, 5);
	lcd_print_delay('u', 6, 0, 5);
	lcd_print_delay(' ', 7, 0, 5);
	lcd_print_delay('c', 8, 0, 5);
	lcd_print_delay('a', 9, 0, 5);
	lcd_print_delay('n', 10, 0, 5);
	lcd_print_delay(' ', 11, 0, 5);
	lcd_print_delay('d', 4, 1, 5);
	lcd_print_delay('o', 5, 1, 5);
	lcd_print_delay(' ', 6, 1, 5);
	lcd_print_delay('t', 7, 1, 5);
	lcd_print_delay('h', 8, 1, 5);
	lcd_print_delay('i', 9, 1, 5);
	lcd_print_delay('s', 10, 1, 5);
	lcd_print_delay('.', 11, 1, 5);
	wait_full_middle_press();
	for (uint8_t i = 0; i < 4; i++)
		for (uint8_t j = 0; j < 8; j++)
			lcd_cgram[i * 8 + j + 32] = level_data_spec_cherecters[INIT_SEGMENT][j][i];
	for (uint8_t j = 0; j < 16; j++)
	{
		for (uint8_t i = 15; i > 0; i--)
			lcd_row_1[i] = lcd_row_1[i - 1];
		for (uint8_t i = 15; i > 0; i--)
			lcd_row_2[i] = lcd_row_2[i - 1];
		lcd_row_1[0] = level_data_characters[0][0][0][LEVEL_SEGMENT_HEIGHT * LEVEL_SEGMENT_NUMBER + 3 - j];
		lcd_row_2[0] = level_data_characters[0][0][1][LEVEL_SEGMENT_HEIGHT * LEVEL_SEGMENT_NUMBER + 3 - j];
		lcd_update();
		for (uint8_t i = 0; i < 8; i++)
			sleep_mode();
	}
}

int main()
{
	TCCR1B = (1 << WGM12) | (1 << CS12);
	OCR1A = 625;
	TIMSK |= 1 << OCIE1A;
	port_init();
	lcd_init();
	sei();
	screen_start();
	while (1)
	{
		screen_main_game();
		screen_finish();
	}

	return 0;
}
