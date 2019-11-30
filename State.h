#ifndef state_h
#define state_h_h

#include <Arduino.h>
#include <NTPClient.h> 
#include "Logfile.h"
#include <Streaming.h>

enum class State {IDLE, WAITING, COOKING, FINISHED, ERROR};

class StateMachine {
	public:
		StateMachine();

		State getState();
		int getRemainingTime();

		void setCookingTime(int t);
		int getCookingTime();

		void setCookingTemp(double t);
		double getCookingTemp();
	
		void startCooking();
		void startByStartTime(unsigned long starttime);
		void startByEndTime(unsigned long endtime);
		String stateAsString(State s);
		
		void next();
		void update();
		
	private:
		int _cookingTime;
		double _cookingTemp;
		State state = State::IDLE;

		unsigned long startTime;

		void setState(State newState);
};

#endif
