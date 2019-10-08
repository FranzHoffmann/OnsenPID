#include "Logfile.h"

/* constructor */
Logfile::Logfile(FS &fs_ref, Stream &stream_ref, NTPClient &ntp_ref) {
  fs_ptr = &fs_ref;
  serial_ptr = &stream_ref;
  ntp_ptr = &ntp_ref;
}
    
/** 
 * write one character. needed for Stream (log << "foo")
 * One line is completed with '\r' ('\n' is discarded) and then stored
 */
/*
size_t Logfile::write(uint8_t character) {
  static int pos = 0;

  if (character == '\n') return 1;

  if (new_ts == 0) {                          // set timestamp when first character arrives
    new_ts = millis();
  }

  if (character == '\r') {                    // write to logfile on endline character
    timestamps[end] = new_ts;                 // write timestamp of new message to array
    new_ts = 0;
    new_msg_buf[pos] = '\0';                  // terminate buffer, write to array, and reset
    strcpy(messages[end], new_msg_buf);
    pos = 0;
    end = inc(end);                           // update array index
    if (end == start) start = inc(start);
    store(new_msg_buf);
    if (serial_ptr) *serial_ptr << new_msg_buf << endl;
    return 1;
  }

  if (pos < MSG_LEN - 1) {                    // add normal characters to buffer
    new_msg_buf[pos] = character;
    pos++;
  }
  return 1;
}

void Logfile::store(char buffer[]) {
    File f = fs_ptr->open("/log.txt", "a");
    if (f) {
      f << buffer << endl;
      f.close();
    }
    //file.setTimeout(0)
    //int readSize = file.readBytes((byte*) myConfigStruct, sizeof(myConfigStruct));

}
*/

size_t Logfile::write(uint8_t character) {
  static int pos = 0;
  static LogEntryStruct entry;
  
  if (character == '\n') return 1;                     // ignore linefeed

  if (entry.timestamp == 0) {                          // set timestamp when first character arrives
    entry.timestamp = ntp_ptr->getEpochTime();
  }

  if (character == '\r') {                            // write to logfile on endline character
    entry.message[pos] = '\0';                        // make sure message is terminated
    store(entry);    
    if (serial_ptr) *serial_ptr << entry.message << endl;
    pos = 0;
    entry.message[0] = '\0';
    entry.timestamp = 0;
    return 1;
  }

  if (pos < MSG_LEN - 2) {                            // add normal characters to buffer
    entry.message[pos] = character;                     // keep space for terminator!
    pos++;
  }
  return 1;
}


/**
 * write message to logfile
 * format: "timestamp;message\r"
 * rotate files when reaching max. size
 */
void Logfile::store(LogEntryStruct &entry) {
    File f = fs_ptr->open(FILENAME_ACT, "a");
    if (f.size() > MAX_FILESIZE - sizeof(entry)) {
      Serial << "Rotating log files" << endl;
      f.close();
      fs_ptr->remove(FILENAME_OLD);
      fs_ptr->rename(FILENAME_ACT, FILENAME_OLD);
      f = fs_ptr->open(FILENAME_ACT, "a");
    }
    if (f) {
      f << entry.timestamp << ';' << entry.message << '\r';
      f.close();
    }
}


void Logfile::getNewMessag(char &buf, size_t buflen, unsigned long timestamp) {
  ;
}

    

/* simple Iterator-like interface: check for more messages */
bool Logfile::hasMore() {
  return this->iterator_hasMore;
}

/* simple Iterator-like interface: get next message */
void Logfile::getNext(char *buf, size_t buflen) {
  File f;
  if (this->iterator_file == 0) 
    f = fs_ptr->open(FILENAME_OLD, "r");
  else
    f = fs_ptr->open(FILENAME_ACT, "r");
  if (!f) {
    if (this->iterator_file == 0) {
      //old file not found, try new file
      Serial << "Old file not found" << endl;
      this->iterator_file = 1;
      this->iterator_offset = 0;
      getNext(buf, buflen);
    } else {
      buf[0]='\0';
      this->iterator_hasMore = false;
    }
    return;
  }
  f.seek(this->iterator_offset, SeekSet);
  size_t n = f.readBytesUntil('\r', buf, buflen-1);
  buf[n] = '\0';
  unsigned long pos = f.position();
  if (pos == f.size()) {
    this->iterator_offset = 0;
    if (this->iterator_file == 0) {
      this->iterator_file = 1;
    } else {
      this->iterator_hasMore = false;
    }
  } else {
    this->iterator_offset = pos;
  }
}


/* simple Iterator-like interface reset readNextMessage to first message */
void Logfile::rewind() {
  Serial << "reset iterator" << endl;
  this->iterator_hasMore = true;
  this->iterator_file = 0;   // oldest
  this->iterator_offset = 0;
}
