#include "tasks.h"
#include <Arduino.h>
#include <Streaming.h>
#include "../../Logfile.h"

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
			dt = millis()- t0;
			tasklist[i].tsum += dt;
		}
	}
}


void init_stats(task_t *t) {
      t->tsum = 0;
}


// -------------------------------------------------------------------------- Statistik Task
/* task: calculate debug statistics */
int task_statistics() {
	//Serial << endl << "Statistics: " << endl;
	//Serial << "task,min,average,max" << endl;
	static unsigned long Tlast;
	unsigned long Tnow = millis();
	double dt = (double)(Tnow -Tlast);
	Tlast = Tnow;

	for (int i=0; i<MAX_TASKS; i++) {
		if (tasklist[i].t != NULL) {
			double percent = (double)tasklist[i].tsum / dt * 100.0;
	//		Logger << tasklist[i].taskname;
	//		Logger << ": " << percent << '%' << endl;
			init_stats(&tasklist[i]);
		}
	}
	//Logger << "System: " << tsys << "ms/10s" << endl;
	//Serial << "Free RAM: " << getTotalAvailableMemory() << ", largest: " << getLargestAvailableBlock() << endl;
	return 10000;
}
