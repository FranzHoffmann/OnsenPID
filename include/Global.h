#ifndef Global_h
#define Global_h

#include <Arduino.h>

#define VERSION "1.100"

struct param {
  double set;			// temperature setpoint, from recipe
  double kp, tn, tv, emax, pmax;
  double act;			// actual temperature, from thermometer
  double out;			// actual power, from controller
  boolean released;				// controller is released
  boolean sensorOK;
};


void start_WiFi();

extern struct param p;

#endif