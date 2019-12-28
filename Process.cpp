#include "Process.h"

#include "src/Clock/Clock.h"
#include "Logfile.h"
#include "recipe.h"


/* constructor */
Process::Process() {
	// create a simple recipe for now
	/*
	recipe[act_rec].name = String("SimpleRecipe");
	recipe[act_rec].Kp = 5.0;
	recipe[act_rec].Tn = 40.0;
	recipe[act_rec].Emax = 10.0;;
	recipe[act_rec].noOfSteps = 1;				//TODO: now max. 1 step
	recipe[act_rec].times[0] = 3600;
	recipe[act_rec].tempArray[0] = 64.5;
	**/
}

int Process::getRemainingTime() {
	int actTime = Clock.getEpochTime() - startTime;
	
	switch (state) {

		case State::COOKING:
			return (recipe[act_rec].times[0] - actTime);

		case State::WAITING:
			return (startTime - Clock.getEpochTime());

		default:
			return 0;
	}
}
//doof
void Process::setCookingTime(int t) {recipe[act_rec].times[0] = t;}

//doof
int Process::getCookingTime() {return recipe[act_rec].times[0];}

//doof
void Process::setCookingTemp(double t) {recipe[act_rec].temps[0] = t;}

double Process::getCookingTemp() {return recipe[act_rec].temps[0];}

// doof
void Process::setParam(Parameter p, double value) {
/*	switch (p) {
	case Parameter::KP: 
		recipe[act_rec].Kp = value;
		break;
	case Parameter::TN:
		recipe[act_rec].Tn = value;
		break;
	case Parameter::EMAX:
		recipe[act_rec].Emax = value;
		break;
	case Parameter::TEMP:
		recipe[act_rec].times[0] = value;
		break;
	}
	*/
}

// ---------------------------------------------------- state management
State Process::getState() {
	return state;
}

void Process::startCooking(int recno) {
	act_rec = recno;
	setState(State::COOKING);
}

void Process::startByStartTime(int recno, unsigned long starttime) {
	switch (state) {

		case State::IDLE:
		case State::WAITING:
			act_rec = recno;
			startTime = starttime;
			setState(State::WAITING);
			break;
	}
}

void Process::startByEndTime(int recno, unsigned long endtime) {
	switch (state) {

		case State::IDLE:
		case State::WAITING:
			act_rec = recno;
			startTime = endtime - calcRecipeDuration(act_rec);
			setState(State::WAITING);
			break;
	}
}

void Process::next() {
	switch (Process::state) {

		case State::ERROR:
		case State::FINISHED:
			setState(State::IDLE);
			break;

		case State::WAITING:
			setState(State::COOKING);
			break;
	}
}

void Process::abort() {
	setState(State::IDLE);
}

// ---------------------------------------------------- ????????????????
void Process::setParam(Parameter p, int value) {
}

double Process::getParamDouble(Parameter p) {
	return recipe[act_rec].param[(int)p];
}

// doof
int Process::getParamInt(Parameter p) {
return 0;
}




String Process::stateAsString(State s) {
	switch (s) {
		case State::IDLE:		return String("IDLE");
		case State::WAITING:	return String("WAITING");
		case State::COOKING:	return String("COOKING");
		case State::FINISHED:	return String("FINISHED");
		case State::ERROR:		return String("ERROR");
	}
}


void Process::setState(State newState) {
	Logger << "Change state from " << stateAsString(state) << " to " << stateAsString(newState) << endl;

	if (newState == State::COOKING) {
		startTime = Clock.getEpochTime();
	}

	state = newState;
}

void Process::update() {
	switch (state) {
	case State::WAITING:
		if (Clock.getEpochTime() >= startTime) {
			Logger << "Time: " << Clock.getEpochTime() << ", startTime: " << startTime << endl;
			setState(State::COOKING);
		}
		break;

	case State::COOKING:
		int actTime = Clock.getEpochTime() - startTime;
		if (actTime >= recipe[act_rec].times[0]) {
			Logger << "actTime: " << actTime << ", cookingTime: " << recipe[act_rec].times[0] << endl;
			setState(State::FINISHED);
		}
		break;
	}
}

/**
 * returns the duration of the recipe (sum of all steps) in seconds
 * uint_32t, max: 136 years...
 */
uint32_t Process::calcRecipeDuration(int recno) {
	uint32_t d = 0;
	if (recno < 0 | recno >= REC_COUNT) return 0;
	for (int i=0; i<REC_STEPS; i++) d += recipe[recno].times[i];
	return d; 
}
