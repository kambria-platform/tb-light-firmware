/*
 * OhmniLabs light animation engine
 *
 * Jared
 */
#ifndef __ANIM_ENGINE_H__
#define __ANIM_ENGINE_H__

struct hsv
{
	uint8_t h;
	uint8_t s;
	uint8_t v;
};

enum anim_state
{
	ANIM_STATE_NONE,

	ANIM_STATE_ONESHOT,

	ANIM_STATE_STARTING_RISE,
	ANIM_STATE_STARTING_SETTLE
};


struct anim_engine
{
	// Animating flag/state in the future
	enum anim_state state;

	// Frame timing, ticks of current frame
	uint32_t fticks;

	// Frame timing, current frame number
	uint32_t fnum;

	// Animation start frame and number of frames
	uint32_t start_frame;
	uint32_t duration_frames;

	// Linear target hsv, specified as 0-255
	// but ints just to make computations without casting
	int start_hsv[3];	
	int end_hsv[3];

	// Computed step value per frame
	int step_hsv[3];

	// Cache of current frame HSV
	int current_hsv[3];
};


/* Global structure */

extern struct anim_engine _anim;


/* Methods */

void anim_setup();
void anim_poll();

void anim_begin(struct hsv *start, struct hsv *end, int nframes);

void anim_state_begin();
void anim_state_done();


#endif

