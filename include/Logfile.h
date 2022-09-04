#ifndef logfile_h
#define logfile_h

#include <Arduino.h>
#include <LittleFS.h>

#define MSG_LEN 255
#define MAX_FILESIZE 65535
#define FILENAME_ACT "/logfile"
#define FILENAME_OLD "/logfile.0"

typedef void (*streamCallback)(String);    // callback := void foo(String)


class LogfileT : public Print {							// extend Print to make Stream work
	public:
		struct LogEntryStruct {
			unsigned long timestamp;
			char message[MSG_LEN];
		};

		void begin();
		void enableLogToFile(bool b);
		void enableLogToSerial(bool b);
		void update(); 									// to be called from loop()

		size_t write(uint8_t character);				// write one character. needed for Stream (logfile << "foo")

		void streamLog(streamCallback cb, unsigned long starttime);


		// primitive iterator-like interface 
		void rewind(unsigned long after);
		bool hasMore();
		String getNext();

		LogfileT::LogEntryStruct strToEntry(String s);

		LogfileT();      // Constructor

	private:
		bool _logToFile;
		bool _logToSerial;
		File logfile;

		void store(LogEntryStruct &entry);
		bool iterator_hasMore = false;
		uint8_t iterator_file = 0;   // oldest
		size_t iterator_offset = 0;
		unsigned long latest_timestamp;
		unsigned int lastWriteTime;

		File streamFile;
		unsigned long streamFilePos;



};

extern LogfileT Logger;

#endif
