#include "State.h"

#include "src/Clock/Clock.h"

/* constructor */
StateMachine::StateMachine() {}

State StateMachine::getState() {
	return state;
}

int StateMachine::getRemainingTime() {
	int actTime = Clock.getEpochTime() - startTime;
	
	switch (state) {

		case State::COOKING:
			return (_cookingTime - actTime);

		case State::WAITING:
			return (startTime - Clock.getEpochTime());

		default:
			return 0;
	}
}

void StateMachine::setCookingTime(int t) {_cookingTime = t;}
int StateMachine::getCookingTime() {return _cookingTime;}

void StateMachine::setCookingTemp(double t) {_cookingTemp = t;}
double StateMachine::getCookingTemp() {return _cookingTemp;}

void StateMachine::startCooking() {
	setState(State::COOKING);
}


void StateMachine::startByStartTime(unsigned long starttime) {
	switch (state) {

		case State::IDLE:
		case State::WAITING:
			startTime = starttime;
			setState(State::WAITING);
			break;
	}
}

void StateMachine::startByEndTime(unsigned long endtime) {
	switch (state) {

		case State::IDLE:
		case State::WAITING:
			startTime = endtime - _cookingTime;
			setState(State::WAITING);
			break;
	}
}



void StateMachine::next() {
	switch (StateMachine::state) {

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

String StateMachine::stateAsString(State s) {
	switch (s) {
		case State::IDLE: return String("IDLE");
		case State::WAITING: return String("WAITING");
		case State::COOKING: return String("COOKING");
		case State::FINISHED: return String("FINISHED");
		case State::ERROR: return String("ERROR");
	}
}


void StateMachine::setState(State newState) {
	//*_logger << "Change state from " << stateAsString(state) << " to " << stateAsString(newState) << endl;

	if (newState == State::COOKING) {
		startTime = Clock.getEpochTime();
	}

	state = newState;
}


void StateMachine::update() {
switch (state) {

	case State::WAITING:
		if (Clock.getEpochTime() >= startTime) {
			//*_logger << "Time: " << _time->getEpochTime() << ", startTime: " << startTime << endl;
			setState(State::COOKING);
		}
		break;

	case State::COOKING:
		int actTime = Clock.getEpochTime() - startTime;
		if (actTime >= _cookingTime) {
			//*_logger << "actTime: " << actTime << ", cookingTime: " << _cookingTime << endl;
			setState(State::FINISHED);
		}
		break;
	}
}
