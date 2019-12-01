#ifndef recipe_h
#define recipe_h

#define REC_MAX_STEPS 10

struct Param_t {
	char id[6];
	char name[17];
	double value;
	char unit[6];
	double min, max;
};
/*
Param_t par[] = {
	{"kp",		"Verstärkung",		5.0,	"%/°C",	0.0,	100.0},
	{"tn",		"Nachstellzeit",	40.0,	"s"	,	0.0,	1000.0},
	{"emax",	"Max. Reglerabw.",	10.0,	"°C",	0.0,	100.0}
};
*/
struct recipe_t {
	String name;
	
	double Kp;
	double Tn;
	double Emax;
	
	int noOfSteps;
	int timeArray[REC_MAX_STEPS];
	double tempArray[REC_MAX_STEPS];
};


#endif
