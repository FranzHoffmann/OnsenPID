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

		void setTime(int time); 			// seconds
		unsigned long getEpoch();		// epoch seconds
		void update();
		String getFormattedTime();
		
		// -- specific for Germany! --
		bool isDST(time_t utc);
		int getTZOffset(time_t utc);
		time_t getEpochLocal();
		time_t UtcToLocal(time_t utc);
		time_t LocalToUtc(time_t local);
		bool isLeapYear (int year);
		int getDayNo(int md, int m, int y);
		String formatTime(unsigned long local);
		String formatTimeShort(unsigned long local);

	private:
		NTPClient* _timeClient;
		WiFiUDP* _ntpUDP;
		String f(int x);

};

extern ClockT Clock;

#endif
