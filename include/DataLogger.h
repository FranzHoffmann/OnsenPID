#ifndef DataLogger_h
#define DataLogger_h

#include <LittleFS.h>


struct dl_data_t {
  unsigned long int ts;
  double act, set, out;
};

void setup_dl();

int dl_log();
int dl_cleanup();
void dl_startBatch();
void dl_endBatch();
void dl_flush();
bool dl_hasMore();
void dl_rewind(String filename, unsigned long after);
struct dl_data_t dl_getNext();

extern FS filesystem;

#endif