#ifndef state_h
#define state_h_h

#include <Arduino.h>
#include <NTPClient.h> 
#include "Logfile.h"
#include <Streaming.h>

enum class State {IDLE, WAITING, COOKING, FINISHED, ERROR};

class StateMachine {
  public:
    StateMachine(NTPClient &ntp_ref, Print *logfile_ref);
  
    State getState();
    void startCooking();
    void startByStartTime(unsigned long starttime);
    void startByEndTime(unsigned long endtime);
    String stateAsString(State s);
    
    void next();
    void update();
    
  private:
    State state = State::IDLE;
    unsigned long startTime;
    int actTime, setTime;
    void setState(State newState);

    NTPClient* _time;
    Print* _logger;
};

#endif
