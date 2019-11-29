#include "State.h"


/* constructor */
StateMachine::StateMachine(NTPClient &ntp_ref, Print *logfile_ref) {
	_logger = logfile_ref;
	_time = &ntp_ref;
}


State StateMachine::getState() {
	return state;
}

void StateMachine::startCooking() {
	// TODO
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
			startTime = endtime - 3600; // TODO!!! p.set_time;
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
	*_logger << "Change state from " << stateAsString(state) << " to " << stateAsString(newState) << endl;

	if (newState == State::COOKING) {
		startTime = _time->getEpochTime();
		setTime = 300; // TODO
	}

	state = newState;
}


void StateMachine::update() {
switch (state) {

	case State::WAITING:
		if (_time->getEpochTime() >= startTime) {
			*_logger << "Time: " << _time->getEpochTime() << ", startTime: " << startTime << endl;
			setState(State::COOKING);
		}
		break;

	case State::COOKING:
		actTime = _time->getEpochTime() - startTime;
		if (actTime >= setTime) {
			*_logger << "actTime: " << actTime << ", setTime: " << setTime << endl;
			actTime = setTime;
			setState(State::FINISHED);
		}
		break;
	}
}
