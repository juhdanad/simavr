#define GRAVITY 50
#define GRAVITY_REDUCED 15

#define PLAYER_WALK_VELOCITY 90
#define PLAYER_AIR_VELOCITY_X 30
#define PLAYER_AIR_VELOCITY_Y 30

#define PLAYER_JUMP_VELOCITY 1200
#define PLAYER_WALL_JUMP_VELOCITY_X 1000
#define PLAYER_WALL_JUMP_VELOCITY_Y 900
#define PLAYER_WALL_JUMP_VELOCITY_Y_BOOSTED 1600
#define PLAYER_JUMP_BUFFER_FRAMES 6
#define PLAYER_JUMP_FRAMES_MAX 40
#define PLAYER_WALL_JUMP_FRAMES_MAX 20

#define PLAYER_DASH_VELOCITY 1200
#define PLAYER_DASH_DIAGONAL_VELOCITY 900
#define PLAYER_DASH_FRAMES 40
#define PLAYER_DASH_INPUT_FRAMES 10
#define PLAYER_DASH_REGAIN_FRAMES 10

#define FRICTION_X_GROUND(x) (x >> 3)
#define FRICTION_X_AIR(x) (x >> 5)
#define FRICTION_Y_WALL(x) (x >> 2)
#define FRICTION_Y_AIR(x) (x >> 6)

#define CAMERA_DELTA_FRAMES 3

#define DEATH_TIMER_FRAMES 100
#define DEATH_TIMER_ANIM_1_FRAME 70
#define DEATH_TIMER_ANIM_2_FRAME 60
#define DEATH_TIMER_ANIM_3_FRAME 50
#define DEATH_TIMER_ANIM_4_FRAME 40
#define DEATH_TIMER_RESET_FRAME 35
#define DEATH_TIMER_ANIM_5_FRAME 30
#define DEATH_TIMER_ANIM_6_FRAME 20
#define DEATH_TIMER_ANIM_7_FRAME 10