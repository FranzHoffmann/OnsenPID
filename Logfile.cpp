#include "Logfile.h"
#include <Arduino.h>
#include <Streaming.h>
#include <FS.h>
#include "src/Clock/Clock.h"


/* constructor */
LogfileT::LogfileT() {
	latest_timestamp = 0;
	_logToFile = true;
	_logToSerial = true;
}

	void LogfileT::enableLogToFile(bool b) {
		_logToFile = b;
	}

	void LogfileT::enableLogToSerial(bool b) {
		_logToSerial = b;
	}

size_t LogfileT::write(uint8_t character) {
	static int pos = 0;
	static LogEntryStruct entry;
	
	if (character == '\n') return 1;                     // ignore linefeed

	if (entry.timestamp == 0) {                          // set timestamp when first character arrives
		entry.timestamp = Clock.getEpochTime();
	}

	if (character == '\r') {                            // write to logfile on endline character
		entry.message[pos] = '\0';                        // make sure message is terminated
		store(entry);    
		latest_timestamp = entry.timestamp;
		if (_logToSerial) Serial << entry.message << endl;
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
void LogfileT::store(LogEntryStruct &entry) {
		File f = SPIFFS.open(FILENAME_ACT, "a");
		if (f.size() > MAX_FILESIZE - sizeof(entry)) {
			f.close();
			SPIFFS.remove(FILENAME_OLD);
			SPIFFS.rename(FILENAME_ACT, FILENAME_OLD);
			f = SPIFFS.open(FILENAME_ACT, "a");
		}
		if (f) {
			f << entry.timestamp << ';' << entry.message << '\r';
			f.close();
		}
}

/**
 * convert string to struct 
 */
LogfileT::LogEntryStruct LogfileT::strToEntry(String s) {
	LogfileT::LogEntryStruct entry;
	entry.timestamp = strtoul(s.c_str(), NULL, 10);
	int pos = s.indexOf(';') + 1;
	String msg = s.substring(pos);
	strncpy(entry.message, msg.c_str(), MSG_LEN - 1);
	return entry;
}


/* simple Iterator-like interface: check for more messages */
bool LogfileT::hasMore() {
	return this->iterator_hasMore;
}

/* simple Iterator-like interface: get next message */
String LogfileT::getNext() {
	File f;
	size_t file_len, file_pos;
	String empty_string = String("");
	
	if (this->iterator_file == 0) 
		f = SPIFFS.open(FILENAME_OLD, "r");
	else
		f = SPIFFS.open(FILENAME_ACT, "r");
	
	if (!f) {
		if (this->iterator_file == 0) {
			this->iterator_file = 1;
			this->iterator_offset = 0;
			return getNext();
		} else {
			this->iterator_hasMore = false;
			return empty_string;
		}
	}
	
	f.seek(this->iterator_offset, SeekSet);
	String s = f.readStringUntil('\r');
	file_pos = f.position();
	file_len = f.size();
	f.close();
	
	if (file_pos == file_len) {
		this->iterator_offset = 0;
		if (this->iterator_file == 0) {
			this->iterator_file = 1;
		} else {
			this->iterator_hasMore = false;
		}
	} else {
		this->iterator_offset = file_pos;
	}

	return s;
}

/* simple Iterator-like interface reset readNextMessage to first message after t */
void LogfileT::rewind(unsigned long t) {
	if (t >= latest_timestamp) {
		this->iterator_hasMore = false;    

	} else {
		this->iterator_hasMore = true;
		this->iterator_file = 0;   // oldest
		this->iterator_offset = 0;
		while (this->hasMore()) {
			unsigned long last_offset = iterator_offset;
			int last_file = iterator_file;
			LogfileT::LogEntryStruct entry;
			
			String s = getNext();
			entry = strToEntry(s);
			if (entry.timestamp > t) {
				iterator_offset = last_offset;  // go back one step
				iterator_file = last_file;
				iterator_hasMore = true;
				return;
			}
		}
	}
}

LogfileT Logger;
