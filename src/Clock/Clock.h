#ifndef clock_h
#define clock_h

// global Clock
//
// Idea:
// #include "Clock.h"
// then use t = Clock.getEpochTime() everywhere.

#include <NTPClient.h>      // note: git pull request 22 (asynchronus update) required
#include <WiFiUdp.h>

class ClockT {
	public:
		ClockT();

		unsigned long getEpochTime();
		void update();
		String getFormattedTime();
		
	private:
	NTPClient* _timeClient;
	WiFiUDP* _ntpUDP;
};

extern ClockT Clock;

#endif
