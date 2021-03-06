#include "recipe.h"

// Note:
// this must be the same order as in /enum class Parameter/.
//
// I can't define the enum here.
// I can't declare this in the header file.
// :-(

Param_t pararray[REC_PARAM_COUNT] = {
{"kp",	"Verstaerkung",		5.0,	{'%', '/', '\1', 'C', '\0'},	0.1,	100.0},
{"tn",	"Nachstellzeit",	30.0,	"s"	,	0.0,	1000.0},
{"tv",	"Vorhaltezeit",		40.0,	"s"	,	0.0,	1000.0},
{"emax","Regler EMax.",		10.0,	{'\1', 'C', '\0'},	0.0,	100.0},
{"pmax","Max. Leistung",	100.0,	"%",	1.0,	100.0}
};

// global recipe structure
recipe_t recipe[REC_COUNT];

