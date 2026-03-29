//========================================================================//
//
// Purpose: 
//
//=============================================================================//

#ifndef EFFECT_COLOR_TABLES_H
#define EFFECT_COLOR_TABLES_H
#ifdef _WIN32
#pragma once
#endif

struct colorentry_t
{
	unsigned char	index;
	
	unsigned char	r;
	unsigned char	g;
	unsigned char	b;
};

#define COLOR_TABLE_SIZE(ct) sizeof(ct)/sizeof(colorentry_t)

// Commander mode indicators (HL2)
enum
{
	COMMAND_POINT_RED = 0,
	COMMAND_POINT_BLUE,
	COMMAND_POINT_GREEN,
	COMMAND_POINT_YELLOW,
};

// Commander mode table
static colorentry_t commandercolors[] =
{
#ifdef DARKINTERVAL  // Colors are unsigned char in range 0...255, not floats 0...1!
	{ COMMAND_POINT_RED,	255,	0,		0	},
	{ COMMAND_POINT_BLUE,	0,		0,		255	},
	{ COMMAND_POINT_GREEN,	0,		255,	0	},
	{ COMMAND_POINT_YELLOW,	255,	255,	0	},
#else
	{ COMMAND_POINT_RED,	1.0,	0.0,	0.0	},
	{ COMMAND_POINT_BLUE,	0.0,	0.0,	1.0	},
	{ COMMAND_POINT_GREEN,	0.0,	1.0,	0.0	},
	{ COMMAND_POINT_YELLOW,	1.0,	1.0,	0.0	},
#endif
};

static colorentry_t bloodcolors[] =
{
#ifdef DARKINTERVAL
	{ BLOOD_COLOR_RED,		120,	25,		0	},
	{ BLOOD_COLOR_YELLOW,	195,	195,	0	},
	{ BLOOD_COLOR_GREEN,	195,	215,	0	},
	{ BLOOD_COLOR_MECH,		20,		20,		20 },
	{ BLOOD_COLOR_ANTLION,	215,	225,	20 },
	{ BLOOD_COLOR_ZOMBIE,	205,	195,	0 },
	{ BLOOD_COLOR_ANTLION_WORKER,	195,	225,	40 },
	{ BLOOD_COLOR_BLACK,		17,		17,		17 },
	{ BLOOD_COLOR_HYDRA,		200,		215,		255 },
#else
	{ BLOOD_COLOR_RED,		72,		0,		0 },
	{ BLOOD_COLOR_YELLOW,	195,	195,	0 },
	{ BLOOD_COLOR_MECH,		20,		20,		20 },
	{ BLOOD_COLOR_GREEN,	195,	195,	0 },
#endif
};

#endif // EFFECT_COLOR_TABLES_H
