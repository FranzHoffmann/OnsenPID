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

//double Process::getCookingTemp() {return recipe[act_rec].temps[0];}


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
	double set =0.0;
	switch (state) {
	case State::WAITING:
		if (Clock.getEpochTime() >= startTime) {
			Logger << "Time: " << Clock.getEpochTime() << ", startTime: " << startTime << endl;
			setState(State::COOKING);
		}
		break;

	case State::COOKING:
		uint32_t actTime = Clock.getEpochTime() - startTime;
		if (actTime >= calcRecipeDuration(act_rec)) {
			setState(State::FINISHED);
		}
		set = calcRecipeRamp(actTime);
		break;
	}

	out.set  = set;
	out.kp   = recipe[act_rec].param[(int)Parameter::KP];
	out.tn   = recipe[act_rec].param[(int)Parameter::TN];
	out.tv   = recipe[act_rec].param[(int)Parameter::TV];
	out.emax = recipe[act_rec].param[(int)Parameter::EMAX];
	out.pmax = recipe[act_rec].param[(int)Parameter::PMAX];
	out.released = (state == State::COOKING);
}

double Process::calcRecipeRamp(uint32_t actTime) {
	recipe_t *rec = &recipe[act_rec]; // for convenience
	if (actTime <= rec->times[0]) {
		return rec->temps[0];
	}
	for (int i=1; i<REC_STEPS; i++) {
		actTime -= rec->times[i-1];
		if (actTime < rec->times[i]) {
			// we are now ramping from setpoint i-1 to i
			return (rec->temps[i-1] + (rec->temps[i] - rec->temps[i-1]) * actTime / rec->times[i]);
		}
	}
	// should not happen
	Logger << "Process::calcRecipeRamp: actTime > duration " << endl;
	return 0.0;
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