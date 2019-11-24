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

#define DL_RAMBUF 10

struct dl_t {
  bool recording;
  String filename;
  int pointer;      // index of first unused ram buffer index
  unsigned long last_ts;
  String it_filename;
  size_t it_offset;
  unsigned long it_after;
  bool it_hasMore;
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
  //Serial << "dl_log(" << dl.recording << ")" << endl;
  if (dl.recording) {
    dl.last_ts = timeClient.getEpochTime();  
    dl_data[dl.pointer].set = p.set;
    dl_data[dl.pointer].act = p.act;
    dl_data[dl.pointer].out = p.out;
    dl_data[dl.pointer].ts = dl.last_ts;

    dl.pointer++;
    if (dl.pointer == DL_RAMBUF) {
      dl_flush();
      dl.pointer = 0;
    }
  }
  
  return 1000;
}

int dl_cleanup() {
  logger << "datalogger cleanup" << endl;
  return 3600000;
}


/**
 * 
 */
void dl_startBatch() {
  dl.recording = true;
  dl.pointer = 0;
  // TODO
  dl.filename = "/data.dat";
  File f = filesystem.open(dl.filename, "w");
  logger << "datalogger: started logfile '" << dl.filename << "'" << endl;
  f.write("TODO: Rezeptname\r");
  f.close();
}

/**
 * 
 */
void dl_endBatch() {
  logger << "datalogger: closed logfile '" << dl.filename << "'" << endl;
  dl.recording = false;
  dl_flush();
  dl.filename = "";
}


void dl_flush() {
  Serial << "dl_flush()" << endl;
  File f = filesystem.open(dl.filename, "a");
  if (f) {
    for (int i=0; i<dl.pointer; i++) {
      f.write((char *)&dl_data[i], sizeof(dl_data_t)/sizeof(char));
    }
    f.close();
  }
}

/*
 * simple Java-iterator-like interace
 * what's the proper way to do this in C++?
 */
/* resets the iterator to the specified file/time */
void dl_rewind(String filename, unsigned long after) {
  dl.it_filename = "/data.dat"; // TODO
  dl.it_offset = 0;
  dl.it_hasMore = false;

  File f = filesystem.open(dl.it_filename, "r");
  if (!f) {
    logger << "dl_rewind: file not found: '" << dl.it_filename << "'" << endl;
    return;
  }
  dl_data_t data;
  String recipe = f.readStringUntil('\r');
  size_t filepos = f.position();
  size_t filelen = f.size();
  while (filepos < filelen) {
    f.readBytes((char*)&data, sizeof(data));
    Serial << "data.ts: " << data.ts << endl;
    if (data.ts > after) {                        // found entry after 'after'
      dl.it_hasMore = true;
      dl.it_offset = filepos;
      break;
    } else {
      filepos = f.position();                     // entry is too old, try next
    }
  }
  logger << "dl_rewind: after=" << after <<", latest=" << data.ts << endl; 
  f.close();
}

bool dl_hasMore() {
  return dl.it_hasMore;
}

/* returns the next entry as json string */
struct dl_data_t dl_getNext_new() {
  dl_data_t data;
  File f = filesystem.open(dl.it_filename, "r");
  if (!f) {
    // ???
    dl.it_hasMore = false;
    return data;
  }

  f.seek(dl.it_offset, SeekSet);
  f.readBytes((char*)&data, sizeof(data));
  if (f.position() == f.size()) {
    dl.it_hasMore = false;
  } else {
    dl.it_offset = f.position();
  }
  f.close();
  return data;
}
