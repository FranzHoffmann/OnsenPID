#ifndef logfile_h
#define logfile_h

#include <Arduino.h>
#include <Streaming.h>
#include <FS.h>
#include <NTPClient.h>

#define MSG_LEN 80
#define MSG_NUM 200
#define MAX_FILESIZE 8192
#define FILENAME_ACT "/logfile"
#define FILENAME_OLD "/logfile.0"


class Logfile : public  Print {                                       // extend Print to make Stream work
  public:
    struct LogEntryStruct {
      unsigned long timestamp;
      char message[MSG_LEN];
    };

    size_t write(uint8_t character);                                  // write one character. needed for Stream (logfile << "foo")

    // primitive iterator-like interface 
    void rewind(unsigned long after);
    bool hasMore();
    String getNext();

    Logfile::LogEntryStruct strToEntry(String s);
    
    Logfile(FS &fs_ref, Stream &stream_ref, NTPClient &ntp_ref);      // Constructor

  private:
    FS *fs_ptr;
    Stream *serial_ptr;
    NTPClient *ntp_ptr;

    void store(LogEntryStruct &entry);
    bool iterator_hasMore = false;
    uint8_t iterator_file = 0;   // oldest
    size_t iterator_offset = 0;
    unsigned long latest_timestamp;

};

#endif
