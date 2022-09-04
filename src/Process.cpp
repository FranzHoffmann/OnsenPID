#include <Process.h>

#include <Clock.h>
#include <Logfile.h>
#include <recipe.h>
#include <Config.h>


/* constructor */
Process::Process(Config *cfg) {
	_cfg = cfg;
	_callback = NULL;
}


int Process::getRemainingTime() {
	switch (state) {
		case State::COOKING:
			return (calcRecipeDuration(act_rec) - (Clock.getEpoch() - startTime));

		case State::WAITING:
			return (startTime - Clock.getEpoch());

		default:
			return 0;
	}
}

void Process::setCallback(void (*f)()) {
	_callback = f;
}


// ---------------------------------------------------- state management
State Process::getState() {
	return state;
}

void Process::startCooking(int recno) {
	act_rec = recno;
	setState(State::COOKING);
}


/**
 * start cooking at specified time.
 * time is "seconds after midnight" today (or tomorrow)
 */

#include <TimeLib.h>
String f(int x) {
	String s;
	if (x < 10) s = '0';
	s = s + x;
	return s;
}
String formatTime(unsigned long local) {
	String str;
	str.reserve(20); // yyyy-mm-dd hh:mm:ss
	int h = hour(local);
	int m = minute(local);
	int s = second(local);
	int y = year(local);
	int M = month(local);
	int d = day(local);
	str =  String(y) + '-' + f(M) + '-' + f(d) + ' ' + f(h) + ':' + f(m) + ':' + f(s);
	return str;
}
void Process::startByStartTime(int recno, unsigned long t) {
	unsigned long now = Clock.getEpochLocal();
	unsigned long midnight = now - (now % 86400L);
	unsigned long starttime = midnight + t;
	if (starttime < now) starttime += 86400;
	
	Logger << "Process::startByStartTime(" << recno << ", " << t << ")" << endl;
	Logger << "Now:       " << String(now) << " (" << formatTime(now) << ")" << endl;
	Logger << "Midnight:  " << String(midnight) << " (" << formatTime(midnight) << ")" << endl;
	Logger << "Starttime: " << String(starttime) << " (" << formatTime(starttime) << ")" << endl;
	
	switch (state) {
		case State::IDLE:
		case State::WAITING:
		case State::FINISHED:
			act_rec = recno;
			startTime = Clock.LocalToUtc(starttime);		// note: startTime is UTC
			setState(State::WAITING);
			break;
	}
}

/**
 * start cooking so that it finishes at specified time.
 * time is "seconds after midnight" today (or later)
 */
void Process::startByEndTime(int recno, unsigned long t) {
	unsigned long now = Clock.getEpochLocal();
	unsigned long midnight = now - (now % 86400L);
	unsigned long starttime = midnight + t - calcRecipeDuration(recno);
	while (starttime < now) starttime += 86400;
	
	Logger << "Process::startByEndTime(" << recno << ", " << t << ")" << endl;
	Logger << "Uhrzeit:     " << String(now) << " (" << formatTime(now) << ")" << endl;
	Logger << "Mitternacht: " << String(midnight) << " (" << formatTime(midnight) << ")" << endl;
	Logger << "Rezeptlänge: " << calcRecipeDuration(recno) << endl;
	Logger << "Startzeit:   " << String(starttime) << " (" << formatTime(starttime) << ")" << endl;
	
	switch (state) {
		case State::IDLE:
		case State::WAITING:
		case State::FINISHED:
			act_rec = recno;
			startTime = Clock.LocalToUtc(starttime);
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
		case State::IDLE:		return String("tut nichts");
		case State::WAITING:	return String("wartet");
		case State::COOKING:	return String("kocht");
		case State::FINISHED:	return String("fertig");
		case State::ERROR:   ; // fallthrough
	}
 return String("Fehler");
}


void Process::setState(State newState) {
	Logger << "Status:  '" << stateAsString(state) << "' -> '" << stateAsString(newState) << "'"  << endl;

	if (newState == State::COOKING) {
		startTime = Clock.getEpoch();
	}
	state = newState;
	if (_callback != NULL) _callback();
}

void Process::update() {
	double set =0.0;
	switch (state) {
	case State::WAITING:
		if (Clock.getEpoch() >= startTime) {
			setState(State::COOKING);
		}
		break;

	case State::COOKING:
		uint32_t actTime = Clock.getEpoch() - startTime;
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
		if (actTime <= rec->times[i]) {
			// we are now ramping from setpoint i-1 to i
			return (rec->temps[i-1] + (rec->temps[i] - rec->temps[i-1]) * actTime / rec->times[i]);
		}
	}
	// should not happen
	Logger << "Fehler: Process::calcRecipeRamp: actTime > duration " << endl;
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
