#ifndef state_h
#define state_h

#include <Arduino.h>
#include <NTPClient.h> 
#include "Logfile.h"
#include <Streaming.h>
#include "recipe.h"
#include "Config.h"

enum class State {IDLE, WAITING, COOKING, FINISHED, ERROR};

class Process {
	public:
		Process(Config *cfg);
		void update();

		// get/change state
		State getState();
		void startCooking(int recno);
		void startByStartTime(int recno, unsigned long t);
		void startByEndTime(int recno, unsigned long t);
		void next();
		void abort();

		// get recipe or setpoints
		int getRemainingTime();

		String stateAsString(State s);

	struct  {
		double set;
		double kp, tn, tv, emax, pmax;
		boolean released;
	} out;


	private:
		Config *_cfg;
		uint32_t calcRecipeDuration(int recno);
		double calcRecipeRamp(uint32_t actTime);
		int act_rec;
		State state = State::IDLE;
		unsigned long startTime;

		void setState(State newState);
};

#endif
