#ifndef tasks_h
#define tasks_h

#include <Arduino.h>

/* simple scheduler */

#define MAX_TASKS 20

typedef int (*task)();    // a task is a function "int f()". returns deltaT for next call.

struct task_t {
  task t;
  unsigned long nextrun;
  const char *taskname;
  unsigned long tsum;
};

boolean start_task(task t, const char *name);
void run_tasks(unsigned long t);

void init_stats(task_t *t);
int task_statistics();

#endif
