#include "inttypes.h"

#define LEVEL_SEGMENT_HEIGHT 12
#define LEVEL_SEGMENT_NUMBER 2
#define INIT_SEGMENT 1

static uint8_t level_data_characters[2][2][2][LEVEL_SEGMENT_HEIGHT * LEVEL_SEGMENT_NUMBER + 4] = {{{{45, 16, 43, 16, 40, 4, 16, 16, 45, 255, 45, 16, 16, 16, 16, 45, 16, 43, 16, 40, 16, 16, 16, 4, 4, 16, 255, 255},
                                                                                                    {45, 16, 43, 16, 40, 5, 16, 16, 45, 45, 45, 16, 16, 16, 46, 16, 44, 16, 42, 16, 16, 40, 16, 5, 5, 95, 255, 255}},
                                                                                                   {{45, 16, 43, 16, 40, 4, 16, 16, 45, 45, 45, 16, 16, 16, 16, 45, 16, 43, 16, 40, 16, 16, 16, 4, 4, 26, 255, 255},
                                                                                                    {45, 16, 43, 16, 40, 5, 16, 16, 45, 45, 45, 16, 16, 16, 46, 16, 44, 16, 42, 16, 16, 40, 16, 5, 5, 95, 255, 255}}},
                                                                                                  {{{45, 16, 43, 16, 40, 4, 16, 16, 45, 45, 45, 16, 16, 16, 16, 45, 16, 43, 16, 40, 16, 16, 16, 4, 4, 26, 255, 255},
                                                                                                    {45, 16, 43, 16, 40, 5, 16, 16, 45, 45, 45, 16, 16, 16, 46, 16, 44, 16, 42, 16, 16, 40, 16, 5, 5, 95, 255, 255}},
                                                                                                   {{45, 16, 43, 16, 40, 4, 16, 16, 45, 45, 45, 16, 16, 16, 16, 45, 16, 43, 16, 40, 16, 16, 16, 4, 4, 16, 255, 255},
                                                                                                    {45, 16, 43, 16, 40, 5, 16, 16, 45, 45, 45, 16, 16, 16, 46, 45, 44, 16, 42, 16, 16, 40, 16, 5, 5, 95, 255, 255}}}};
static uint16_t level_data_switch_times[LEVEL_SEGMENT_NUMBER][2] = {{20, 200}, {300, 400}};
static uint16_t level_data_player_reset_x[LEVEL_SEGMENT_NUMBER] = {0x3000, 0xd000};
static uint16_t level_data_player_reset_y[LEVEL_SEGMENT_NUMBER] = {0x3000, 0x4000};
static uint16_t level_data_player_reset_y_char[LEVEL_SEGMENT_NUMBER] = {13, 13};
static uint8_t level_data_spec_cherecters[LEVEL_SEGMENT_NUMBER][8][4] = {
    {{0b11111, 0b00000, 0b00000, 0b00000},
     {0b00001, 0b00000, 0b00000, 0b00000},
     {0b00001, 0b00001, 0b00000, 0b00000},
     {0b00001, 0b00001, 0b00000, 0b00000},
     {0b00001, 0b00001, 0b00000, 0b00000},
     {0b00001, 0b00001, 0b00000, 0b00000},
     {0b00000, 0b00001, 0b00000, 0b00000},
     {0b00000, 0b11111, 0b00000, 0b00000}},
    {{0b00000, 0b00000, 0b00000, 0b00000},
     {0b00000, 0b00000, 0b00000, 0b00000},
     {0b00000, 0b00000, 0b00000, 0b00000},
     {0b01100, 0b00100, 0b00000, 0b00000},
     {0b01100, 0b01000, 0b00000, 0b00000},
     {0b00000, 0b00000, 0b00000, 0b00000},
     {0b00000, 0b00000, 0b00000, 0b00000},
     {0b00000, 0b00000, 0b00000, 0b00000}}};