#ifndef recipe_h
#define recipe_h

#define REC_STEPS 5
#define REC_NAME_LEN 16
#define REC_PARAM_COUNT 5
#define REC_COUNT 10


// these are the parameters.
// Param_t is initialized in the .cpp file
enum class Parameter {KP, TN, TV, EMAX, PMAX};
struct Param_t {
	char id[6];
	char name[17];
	double value;
	char unit[6];
	double min, max;
};
extern Param_t pararray[REC_PARAM_COUNT];

struct recipe_t {
	char name[REC_NAME_LEN+1];
	int times[REC_STEPS];
	double temps[REC_STEPS];
	double param[REC_PARAM_COUNT];
};

extern recipe_t recipe[];

#endif
