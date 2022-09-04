#include <tasks.h>
#include <Arduino.h>
#include <Streaming.h>

task_t tasklist[MAX_TASKS];

boolean start_task(task t, const char *name) {
  for (int i=0; i<MAX_TASKS; i++) {
    if (tasklist[i].t == NULL) {
      tasklist[i].t = t;
      tasklist[i].nextrun = millis();
      tasklist[i].taskname = name;
      init_stats(&tasklist[i]);
      return true;
    }
  }
  return false;
}


void run_tasks(unsigned long t) {
	unsigned long t0, dt;

	for (int i=0; i<MAX_TASKS; i++) {
		if (tasklist[i].t != NULL && t >= tasklist[i].nextrun) {
			t0 = millis();
			tasklist[i].nextrun += (tasklist[i].t)();
			// statistics
			dt = millis()- t0;
			tasklist[i].tsum += dt;
			tasklist[i].count++;
			tasklist[i].tmax = max(tasklist[i].tmax, dt);
			tasklist[i].tmin = min(tasklist[i].tmin, dt);			
		}
	}
}


void init_stats(task_t *t) {
	t->count = 0;
    t->tsum = 0;
	t->tmax = 0;
	t->tmin = 1 << 31;
}


// -------------------------------------------------------------------------- Statistik Task
/* task: calculate debug statistics */
void print_statistics() {
	Serial << "Active tasks:" << endl;
	Serial << "Tmin" << '\t' << "Tavg" << '\t' << "Tmax" << '\t' << "Name" << endl;
	for (int i=0; i<MAX_TASKS; i++) {
		if (tasklist[i].t != NULL) {
			if (tasklist[i].count > 0) {
				Serial << tasklist[i].tmin << '\t';
				Serial << (double)(tasklist[i].tsum) / tasklist[i].count << '\t';
				Serial << tasklist[i].tmax;
			} else {
				Serial << '-' << '\t' << '-' << '\t' << '-';
			}
			Serial << '\t' << tasklist[i].taskname << endl;
			init_stats(&tasklist[i]);
		}
	}
}
