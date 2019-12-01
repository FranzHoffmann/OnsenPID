#include "Process.h"

#include "src/Clock/Clock.h"
#include "Logfile.h"
#include "recipe.h"

/* constructor */
Process::Process() {
	// create a simple recipe for now
	_recipe.name = String("SimpleRecipe");
	_recipe.Kp = 5.0;
	_recipe.Tn = 40.0;
	_recipe.Emax = 10.0;;
	_recipe.noOfSteps = 1;				//TODO: now max. 1 step
	_recipe.timeArray[0] = 3600;
	_recipe.tempArray[0] = 64.5;
}

State Process::getState() {
	return state;
}

int Process::getRemainingTime() {
	int actTime = Clock.getEpochTime() - startTime;
	
	switch (state) {

		case State::COOKING:
			return (_recipe.timeArray[0] - actTime);

		case State::WAITING:
			return (startTime - Clock.getEpochTime());

		default:
			return 0;
	}
}

void Process::setCookingTime(int t) {_recipe.timeArray[0] = t;}
int Process::getCookingTime() {return _recipe.timeArray[0];}

void Process::setCookingTemp(double t) {_recipe.tempArray[0] = t;}
double Process::getCookingTemp() {return _recipe.tempArray[0];}

void Process::startCooking() {
	setState(State::COOKING);
}

void Process::setParam(Parameter p, double value) {
	switch (p) {
	case Parameter::KP: 
		_recipe.Kp = value;
		break;
	case Parameter::TN:
		_recipe.Tn = value;
		break;
	case Parameter::EMAX:
		_recipe.Emax = value;
		break;
	case Parameter::TEMP:
		_recipe.timeArray[0] = value;
		break;
	}
}

void Process::setParam(Parameter p, int value) {
	switch (p) {
	case Parameter::TIME:
		_recipe.timeArray[0] = value;
		break;
	}
}

double Process::getParamDouble(Parameter p) {
	switch (p) {
	case Parameter::KP: 	return _recipe.Kp;
	case Parameter::TN:		return _recipe.Tn;
	case Parameter::EMAX:	return _recipe.Emax;
	case Parameter::TEMP:	return _recipe.tempArray[0];
	default:				return 0.0;
	}
	
}

int Process::getParamInt(Parameter p) {
	switch (p) {
	case Parameter::TEMP:	return _recipe.timeArray[0];
	default:				return 0;
	}
}


void Process::startByStartTime(unsigned long starttime) {
	switch (state) {

		case State::IDLE:
		case State::WAITING:
			startTime = starttime;
			setState(State::WAITING);
			break;
	}
}

void Process::startByEndTime(unsigned long endtime) {
	switch (state) {

		case State::IDLE:
		case State::WAITING:
			startTime = endtime - _recipe.timeArray[0];
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

		default:
			;
	}
}

String Process::stateAsString(State s) {
	switch (s) {
		case State::IDLE: return String("IDLE");
		case State::WAITING: return String("WAITING");
		case State::COOKING: return String("COOKING");
		case State::FINISHED: return String("FINISHED");
		case State::ERROR: return String("ERROR");
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
		if (actTime >= _recipe.timeArray[0]) {
			Logger << "actTime: " << actTime << ", cookingTime: " << _recipe.timeArray[0] << endl;
			setState(State::FINISHED);
		}
		break;
	}
}
