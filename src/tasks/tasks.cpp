#include "tasks.h"
#include <Arduino.h>

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
    if (tasklist[i].t != NULL && t > tasklist[i].nextrun) {
      t0 = millis();
      tasklist[i].nextrun += (tasklist[i].t)();
      dt = millis()- t0;
      tasklist[i].tcount++;
      tasklist[i].tsum += dt;
      tasklist[i].tmin = min(tasklist[i].tmin, dt);
      tasklist[i].tmax = max(tasklist[i].tmax, dt);
    }
  }
}


void init_stats(task_t *t) {
      t->tsum = 0;
      t->tmin = 999999;
      t->tmax = 0;
      t->tcount = 0;
}


// -------------------------------------------------------------------------- Statistik Task
/* task: calculate debug statistics */
int task_statistics() {
  //Logger << endl << "Statistics: " << endl;
  for (int i=0; i<MAX_TASKS; i++) {
    if (tasklist[i].t != NULL) {
      double avg = tasklist[i].tcount > 0 
                 ? (double)tasklist[i].tsum / tasklist[i].tcount
                 : 0;
      //Logger << tasklist[i].taskname;
      //for (int i=strlen(tasklist[i].taskname); i<20; i++) { Serial << '.'; }  // crashes?
      //Logger << ',' << tasklist[i].tmin
     //      << ','<< avg
     //      << ','<< tasklist[i].tmax
     //      << "ms" << endl;
      init_stats(&tasklist[i]);
    }
  }   
  //Logger << "Free RAM: " << getTotalAvailableMemory() << ", largest: " << getLargestAvailableBlock() << endl;
  return 600000; // good thing int seems to be 32 bit
}
