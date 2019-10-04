#ifndef log2_h
#define log2_h

#include "Arduino.h"

#define MSG_LEN 80
#define MSG_NUM 200


class Log2 : public Print {

  private:
    char new_msg_buf[MSG_LEN];                    // buffer for incoming message
    unsigned long new_ts = 0;

  public:

    /*
        write one character. needed for stream (log << "foo")
    */
    size_t write(uint8_t character) {
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
        Serial << new_msg_buf << endl;
        return 1;
      }

      if (pos < MSG_LEN - 1) {                    // add normal characters to buffer
        new_msg_buf[pos] = character;
        pos++;
      }
      return 1;
    }


    /*
       simple Iterator-like interface
       get next message
    */
    void readNextMsg(char *buf, size_t buflen) {
      if (current == end) {
        snprintf(buf, buflen, "EOL");
        return;
      }
      snprintf(buf, buflen, "%2d: %10ld: %s", current, timestamps[current], messages[current]);
      current = inc(current);
    }


    /*
       simple Iterator-like interface
       reset readNextMessage to first message
    */
    void rewind() {
      current = start;
    }

    /*
       simple Iterator-like interface
       check for more messages
    */
    bool hasMore() {
      return (current != end);
    }


  private:
    uint16_t start = 0;   // index of oldest message
    uint16_t end = 0;     // index for next message
    uint16_t current = 0; // "iterator" for reading

    unsigned long timestamps[MSG_NUM];
    char messages[MSG_NUM][MSG_LEN];


    /*
       increase and wrap-around
    */
    uint16_t inc(uint16_t index) {
      return (index + 1) % MSG_NUM;
    }

};

#endif
