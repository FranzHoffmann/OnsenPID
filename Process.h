#ifndef state_h
#define state_h

#include <Arduino.h>
#include <NTPClient.h> 
#include "Logfile.h"
#include <Streaming.h>
#include "recipe.h"

enum class Parameter {KP, TN, TV, EMAX, TIME, TEMP};
enum class State {IDLE, WAITING, COOKING, FINISHED, ERROR};

class Process {
	public:
		Process();

		State getState();
		int getRemainingTime();

		void setCookingTime(int t);
		int getCookingTime();

		void setCookingTemp(double t);
		double getCookingTemp();
	
		void startCooking();
		void startByStartTime(unsigned long starttime);
		void startByEndTime(unsigned long endtime);
		
		void setParam(Parameter p, double value);
		void setParam(Parameter p, int value);
		double getParamDouble(Parameter p);
		int getParamInt(Parameter p);
		
		String stateAsString(State s);
		
		void next();
		void update();
		
	private:
		recipe_t _recipe;
		State state = State::IDLE;
		unsigned long startTime;

		void setState(State newState);
};

#endif
