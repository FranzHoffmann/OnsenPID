/*
 * Non-generic data Logger 
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
 * setup data Logger
 */
void setup_dl() {
  start_task(dl_log, "Data Logger");
  start_task(dl_cleanup, "Data Logger cleanup");
}

/**
 * Task for logging
 */
int dl_log() {
  //Serial << "dl_log(" << dl.recording << ")" << endl;
  if (dl.recording) {
    dl.last_ts = Clock.getEpochTime();  
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
  Logger << "dataLogger cleanup" << endl;
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
  Logger << "dataLogger: started logfile '" << dl.filename << "'" << endl;
  f.write("TODO: Rezeptname\r");
  f.close();
}

/**
 * 
 */
void dl_endBatch() {
  Logger << "dataLogger: closed logfile '" << dl.filename << "'" << endl;
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
File readfile;
void dl_rewind(String filename, unsigned long after) {
  dl.it_filename = "/data.dat"; // TODO
  dl.it_offset = 0;
  dl.it_hasMore = false;

  if (readfile) {
    readfile.close();
  }

  readfile = filesystem.open(dl.it_filename, "r");
  if (!readfile) {
    Logger << "dl_rewind: file not found: '" << dl.it_filename << "'" << endl;
    return;
  }
  dl_data_t data;
  String recipe = readfile.readStringUntil('\r');
  size_t filepos = readfile.position();
  size_t filelen = readfile.size();
  while (filepos < filelen) {
    readfile.readBytes((char*)&data, sizeof(data));
    if (data.ts > after) {                        // found entry after 'after'
      dl.it_hasMore = true;
      dl.it_offset = filepos;
      break;
    } else {
      filepos = readfile.position();                     // entry is too old, try next
    }
  }
  Logger << "dl_rewind: after=" << after <<", latest=" << data.ts << endl; 
}

bool dl_hasMore() {
  return dl.it_hasMore;
}

/* returns the next entry as json string */
struct dl_data_t dl_getNext() {
  dl_data_t data;

  readfile.readBytes((char*)&data, sizeof(data));
  if (readfile.position() == readfile.size()) {
    dl.it_hasMore = false;
  }
  return data;
}
