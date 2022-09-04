#include <util.h>

/**
 *  scales analog input (0-1023) to min..max
 **/
double scale(int in, double min, double max) {
  double in_r = in;
  return (in_r / 1024) * (max - min) + min;
}

/** first order low pass.
 * T - sample time
 * tau - filter time
 **/
double pt1(double x, double tau, double T) {
  static double y_old;
  double a = T / tau;
  double y =  a * x + (1.0 - a) * y_old;
  y_old = y;
  return y;
}

/**
 * generic scaling function
 */
double xscale(double in, double in_min, double in_max, double out_min, double out_max) {
  return (in - in_min)/(in_max - in_min) * (out_max - out_min) + out_min;
}

/**
 * linearize using line segments
 **/
double linearize(double x, double *xval, double *yval, int len) {
  for (int idx = 1; idx<len; idx++) {
    if (xval[idx-1] > x && xval[idx] <= x) {
      return xscale(x, xval[idx], xval[idx-1], yval[idx], yval[idx-1]);
    }
  }
  return yval[len-1];
}

/**
 * simple limiter
 **/
double limit(double x, double xmin, double xmax) {
  if (x < xmin) return xmin;
  if (x > xmax) return xmax;
  return x; 
}
int32_t limit(int32_t x, int32_t xmin, int32_t xmax) {
  if (x < xmin) return xmin;
  if (x > xmax) return xmax;
  return x; 
}

/**
 * convert seconds-since-midnight in hh:mm
 */
String secToTime(int sec) {
	int hours = sec / 3600;
	int min = (sec - 3600 * hours) / 60;
	String s;
	s += (hours > 9) ? "" + String(hours) : "0" + String(hours);
	s += ":";
	s += (min > 9) ? "" + String(min) : "0" + String(min);
	return s; 
}

