#include "Clock.h"
#include <NTPClient.h>      // note: git pull request 22 (asynchronus update) required
#include <WiFiUdp.h>


ClockT::ClockT() {
	_ntpUDP = new WiFiUDP();
	_timeClient = new NTPClient(*_ntpUDP);
	_timeClient->begin();
}


void ClockT::setTimeOffset (int timeOffset) {
	_timeClient->setTimeOffset(timeOffset);
}

void ClockT::update() {
	_timeClient->update();
}


unsigned long ClockT::getEpochTime() {
	return _timeClient->getEpochTime();
}


String ClockT::getFormattedTime() {
	return _timeClient->getFormattedTime();
}


ClockT Clock;
