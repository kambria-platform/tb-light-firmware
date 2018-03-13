/*
 * OhmniLabs light animation engine
 *
 * Jared
 */

/* TODO: instead of states could make this programmable slots that get
 * set up from serial for flexibility, and/or even save to flash for boot anim */

/* Including needed modules to compile this module/procedure */
#include "Cpu.h"
#include "Events.h"
/* Including shared modules, which are used for whole project */
#include "PE_Types.h"

/* Our source */
#include "systime.h"
#include "spi_apa102.h"
#include "anim_engine.h"
#include "flash.h"


/* ---- Global ---- */

struct anim_engine _anim = {0};

// 1 millisecond frames, more than needed but convenient for now
#define FRAME_TICKS 1500

// Helper to set lighting
#define SET_HSV(x, h, s, v) { (x)[0] = h; (x)[1] = s; (x)[2] = v; }


/* ---- Methods ---- */

void anim_setup()
{
	// Synchronize clock
	_anim.fticks = _systime.ticks;

	// Start with rise
	_anim.state = ANIM_STATE_STARTING_RISE;
	anim_state_begin();
}

void anim_poll()
{
	// Increment our frame counter
	int fadv = 0;
	while(1) {
		
		// See if we advanced one frame
		int delta = _systime.ticks - _anim.fticks;
		if (delta < FRAME_TICKS) break;

		// Okay, then we moved, adjust
		_anim.fticks += 1500;
		fadv += 1;
	}

	// Check if anything changed
	if (fadv == 0) return;

	// Increment frames when not animating too
	_anim.fnum += fadv;

	// If not animating also no-op
	if (_anim.state == ANIM_STATE_NONE)
		return;

	// Compute interpolation factor
	int df = _anim.fnum - _anim.start_frame;

	// Check if we reached end state
	if (df >= _anim.duration_frames) {
		SET_HSV(_anim.current_hsv, 
			_anim.end_hsv[0], _anim.end_hsv[1], _anim.end_hsv[2]);
		SET_HSV(_anim.start_hsv, 
			_anim.end_hsv[0], _anim.end_hsv[1], _anim.end_hsv[2]);
		anim_state_done();
		return;
	}

	// Otherwise, compute interpolation and update
	int *curr = _anim.current_hsv;
	int *start = _anim.start_hsv;
	int *step = _anim.step_hsv;
	for (int i = 0; i < 3; i++) {
		curr[i] = start[i] + step[i] * df;
	}

	// Copy over to SPI engine now
	spi_set_global_hsv(curr[0] >> 20, curr[1] >> 20, curr[2] >> 20);
}


void anim_begin(struct hsv *start, struct hsv *end, int nframes)	
{
	// Snapshot frame start and frame duration
	_anim.start_frame = _anim.fnum;
	_anim.duration_frames = nframes;

	// Save start and end in our scaled space
	int* s = _anim.start_hsv;
	if (start != NULL) {
		s[0] = start->h << 20;
		s[1] = start->s << 20;
		s[2] = start->v << 20;
	}

	int* e = _anim.end_hsv;
	if (end != NULL) {
		e[0] = end->h << 20;
		e[1] = end->s << 20;
		e[2] = end->v << 20;
	}

	// Compute step values here per frame in our scaled space
	for (int i = 0; i < 3; i++) {
		_anim.step_hsv[i] = (e[i] - s[i]) / nframes;
		_anim.current_hsv[i] = s[i];
	}
	
}

void anim_state_begin()
{
	switch(_anim.state) {

		case ANIM_STATE_STARTING_RISE:
		{			
			// Start animation up
			struct hsv start = { 130, 240, 0 };
			struct hsv end = { 130, 140, 120 };

			// Use the boot hue
			start.h = _flash.boot_hue;
			end.h = _flash.boot_hue;

			anim_begin(&start, &end, 2000);
			break;
		}
		case ANIM_STATE_STARTING_SETTLE:
		{
			// Start from wherever we last finished (rise)
			struct hsv end = { 130, 180, 30 };
			end.h = _flash.boot_hue;

			anim_begin(NULL, &end, 1500);
			break;
		}
		default:
			// Normalize any other state
			_anim.state = ANIM_STATE_NONE;
			break;
	}

}


void anim_state_done()
{

	switch(_anim.state) {

		case ANIM_STATE_STARTING_RISE:

			_anim.state = ANIM_STATE_STARTING_SETTLE;
			anim_state_begin();

			break;

		default:
			
			_anim.state = ANIM_STATE_NONE;
			anim_state_begin();

			break;

	}

}

