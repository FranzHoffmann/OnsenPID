#include <Clock.h>
#include <NTPClient.h>      // note: git pull request 22 (asynchronus update) required
#include <WiFiUdp.h>
#include <TimeLib.h>

ClockT::ClockT() {
	_ntpUDP = new WiFiUDP();
	_timeClient = new NTPClient(*_ntpUDP);
	_timeClient->begin();
}

/*
void ClockT::setTimeOffset (int timeOffset) {
	_timeClient->setTimeOffset(timeOffset);
}
*/

/*
 * this can be used to set the time when no NTP server is available
 */
void ClockT::setTime (int timeOffset) {
	// TODO
	_timeClient->setTimeOffset(timeOffset);
}


void ClockT::update() {
	_timeClient->update();
}


unsigned long ClockT::getEpoch() {
	return _timeClient->getEpochTime();
}


String ClockT::getFormattedTime() {
	return _timeClient->getFormattedTime();
}





// ----------------------------------------------------------- time stuff
String ClockT::f(int x) {
	String s;
	if (x < 10) s = '0';
	s = s + x;
	return s;
}

/* returns yyyy-mm-dd hh:mm:ss */
String ClockT::formatTime(unsigned long local) {
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

/* returns hh:mm:ss */
String ClockT::formatTimeShort(unsigned long local) {
	String str;
	str.reserve(9); // hh:mm:ss
	int h = hour(local);
	int m = minute(local);
	int s = second(local);
	str = f(h) + ':' + f(m) + ':' + f(s);
	return str;
}

/* 
 * this returns the number of seconds since1970-01-01 00:00 local time
 * it can be usted with hour(), minute() etc. to get local time
 */
time_t ClockT::getEpochLocal() {
	return UtcToLocal(getEpoch());
}

time_t ClockT::UtcToLocal(time_t utc) {
	return utc + getTZOffset(utc);
}


time_t ClockT::LocalToUtc(time_t local) {
	// UtcToLocal is easy
	// LocalToUtc is not always clear: during DST switch, some hh:mm don't exist, other exist twice
	// but: we don't get hh:mm, we get ssss
	// try and see ...:
	time_t utc = local - 3600;
	if (local != UtcToLocal(utc))
		utc = local - 7200;
	return utc;
}


bool ClockT::isDST(time_t utc) {
	time_t localWinterTime = utc + 3600;
	int m = month(localWinterTime); // month: 1-12
	if (m < 3 || m > 10)  return false; 
	if (m > 3 && m < 10)  return true;

	int dow = weekday(localWinterTime) - 1; // 0 == Su
	int previousSunday = day(localWinterTime) - dow;
	
	if (m == 3) {
		if (dow == 0 && previousSunday >= 25) {
			// it's the last sunday of march
			// summer time from 02:00 localWinterTime
			return hour(localWinterTime) >= 2;
		} else {
			return previousSunday >= 25;
		}
	}

	if (m == 10) {
		if (dow == 0 && previousSunday < 25) {
			// it's the last sunday of october
			// summer time until 02:00 localWinterTime
			return hour(localWinterTime) < 2;
		} else {
			return previousSunday < 25;
		}
	}

	return false; // this line should never be reached
}


int ClockT::getTZOffset(time_t utc) {
	return isDST(utc) ? 7200 : 3600;
}


int yday[2][13] = {
  /* Normal Years.  */
  { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
  /* Leap Years.  */
  { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};


bool ClockT::isLeapYear (int year) {
  return ((year % 4) == 0 && (year % 100 != 0)) 
  	||   ((year % 400) == 0);
}


int ClockT::getDayNo(int d, int m, int y) {
	int index = isLeapYear(y) ? 1 : 0;
	return (yday[index][m-1] + d);
}




ClockT Clock;
