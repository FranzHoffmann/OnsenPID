/*
 * Non-generic data logger 
 * 
 * dl_startBatch(): legte ein neues file an und aktiviert logging
 * dl_endBatch():   deaktiviert das logging
 * dl_log(...):     speichert daten, zuerst im RAM und dann häppchenweise auf disk, zyklisch aufrufen
 * 
 * dl_cleanup():    löscht alte logdaten, zyklisch aufrufen
 * 
 * dl_getData(?):   don't know yet. Stream directly to server maybe?
 * 
 */

#define DL_RAMBUF 100

struct dl_t {
  bool recording;
  String filename;
  int pointer;      // index of first unused ram buffer index
};

struct dl_data_t {
  unsigned long int ts;
  double act, set, out;
};

dl_t dl;
dl_data_t dl_data[DL_RAMBUF];


/**
 * setup data logger
 */
void setup_dl() {
  start_task(dl_log, "Data Logger");
  start_task(dl_cleanup, "Data logger cleanup");
}

/**
 * Task for logging
 */
int dl_log() {
  if (dl.recording) {
    dl_data[dl.pointer].set = p.set;
    dl_data[dl.pointer].act = p.act;
    dl_data[dl.pointer].out = p.out;
    dl_data[dl.pointer].ts = timeClient.getEpochTime();
  }
  dl.pointer++;
  if (dl.pointer == DL_RAMBUF) {
    dl_flush();
  }
  return 1000;
}

int dl_cleanup() {
  //TODO
  return 20000; // only int!
}


/**
 * 
 */
void dl_startBatch() {
  dl.recording = true;
  dl.pointer = 0;
  // TODO
  dl.filename = "data.dat";
  File f = filesystem.open(dl.filename, "w");
  f.write("TODO: Rezeptname\r");
  f.close();
}

/**
 * 
 */
void dl_endBatch() {
  dl.recording = false;
  dl_flush();
}


void dl_flush() {
  File f = filesystem.open(dl.filename, "a");
  for (int i=0; i<dl.pointer; i++) {
    f.write((char *)&dl_data[i], sizeof(dl_data_t)/sizeof(char));
  }
  f.close();
}
