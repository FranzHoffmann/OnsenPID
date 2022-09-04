#ifndef Util_h
#define Util_h

#include <Arduino.h>

/**
 *  scales analog input (0-1023) to min..max
 **/
double scale(int in, double min, double max);

/** first order low pass.
 * T - sample time
 * tau - filter time
 **/
double pt1(double x, double tau, double T);

/**
 * generic scaling function
 */
double xscale(double in, double in_min, double in_max, double out_min, double out_max);

/**
 * linearize using line segments
 **/
double linearize(double x, double *xval, double *yval, int len);

/**
 * simple limiter
 **/
double limit(double x, double xmin, double xmax);
int32_t limit(int32_t x, int32_t xmin, int32_t xmax);

/**
 * convert seconds-since-midnight in hh:mm
 */
String secToTime(int sec);

#endif