#ifndef state_h
#define state_h

#include <Arduino.h>
#include <NTPClient.h> 
#include "Logfile.h"
#include <Streaming.h>
#include "recipe.h"

enum class State {IDLE, WAITING, COOKING, FINISHED, ERROR};

class Process {
	public:
		Process();
		void update();

		// get/change state
		State getState();
		void startCooking(int recno);
		void startByStartTime(int recno, unsigned long starttime);
		void startByEndTime(int recno, unsigned long endtime);
		void next();
		void abort();

		// get recipe or setpoints
		int getRemainingTime();
		double getCookingTemp();

		// TODO: deprecated
		void setCookingTime(int t);
		int getCookingTime();
		void setCookingTemp(double t);

		void setParam(Parameter p, double value);
		void setParam(Parameter p, int value);
		double getParamDouble(Parameter p);
		int getParamInt(Parameter p);

		String stateAsString(State s);

	private:
		uint32_t calcRecipeDuration(int recno);
		int act_rec;
		State state = State::IDLE;
		unsigned long startTime;

		void setState(State newState);
};

#endif
