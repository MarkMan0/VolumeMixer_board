/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.io/license.html
 */

#include "gfx.h"
#include <string.h>

#if (GFX_USE_GINPUT && GINPUT_NEED_MOUSE) 

#define GMOUSE_DRIVER_VMT		GMOUSEVMT_ADS7843
#include "../../../../src/ginput/ginput_driver_mouse.h"

// Get the hardware interface
#include "gmouse_lld_ADS7843_board.h"

#define CMD_X				0xD1
#define CMD_Y				0x91
#define CMD_ENABLE_IRQ		0x80

#define LPF_FACTOR	(0.9)

static gCoord xPrev = 240 / 4, yPrev = 320/4;

static gBool MouseXYZ(GMouse* m, GMouseReading* pdr)
{
	(void)m;

	// No buttons
	pdr->buttons = 0;
	pdr->z = 0;
	
	if (getpin_pressed(m)) {
		pdr->z = 1;						// Set to Z_MAX as we are pressed

		aquire_bus(m);
		
		read_value(m, CMD_X);				// Dummy read - disable PenIRQ
		float v = read_value(m, CMD_X);		// Read X-Value
		pdr->x = LPF_FACTOR*xPrev + (1-LPF_FACTOR)*v;
		v = pdr->x;

		read_value(m, CMD_Y);				// Dummy read - disable PenIRQ
		v = read_value(m, CMD_Y);		// Read Y-Value
		pdr->y = LPF_FACTOR*yPrev + (1-LPF_FACTOR)*v;
		v = pdr->y;

		read_value(m, CMD_ENABLE_IRQ);		// Enable IRQ

		release_bus(m);
	}
	return gTrue;
}


static gBool calibration_load(GMouse *m, void *buf, gMemSize sz) {

	(void)m;
	const GMouseCalibration cal = {
		.ax = -0.62154454,
		.bx = -0.0104754698,
		.cx = 280.954254,
		.ay = -0.0186230578,
		.by = 0.898562551,
		.cy = -78.7429428
	};

	if (sz != sizeof(cal)) {
		return gFalse;
	}

	memcpy(buf, &cal, sz);
	return gTrue;
}

const GMouseVMT const GMOUSE_DRIVER_VMT[1] = {{
	{
		GDRIVER_TYPE_TOUCH,
		GMOUSE_VFLG_TOUCH | GMOUSE_VFLG_CALIBRATE | GMOUSE_VFLG_CAL_TEST |
			GMOUSE_VFLG_ONLY_DOWN | GMOUSE_VFLG_POORUPDOWN | GMOUSE_VFLG_DEFAULTFINGER,
		sizeof(GMouse)+GMOUSE_ADS7843_BOARD_DATA_SIZE,
		_gmouseInitDriver,
		_gmousePostInitDriver,
		_gmouseDeInitDriver
	},
	1,				// z_max - (currently?) not supported
	0,				// z_min - (currently?) not supported
	1,				// z_touchon
	0,				// z_touchoff
	{				// pen_jitter
		GMOUSE_ADS7843_PEN_CALIBRATE_ERROR,			// calibrate
		GMOUSE_ADS7843_PEN_CLICK_ERROR,				// click
		GMOUSE_ADS7843_PEN_MOVE_ERROR				// move
	},
	{				// finger_jitter
		GMOUSE_ADS7843_FINGER_CALIBRATE_ERROR,		// calibrate
		GMOUSE_ADS7843_FINGER_CLICK_ERROR,			// click
		GMOUSE_ADS7843_FINGER_MOVE_ERROR			// move
	},
	init_board, 	// init
	0,				// deinit
	MouseXYZ,		// get
	0,				// calsave
	calibration_load				// calload
}};

#endif /* GFX_USE_GINPUT && GINPUT_NEED_MOUSE */

