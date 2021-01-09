/*
  TODO
  ====
  - data Logger / chart display webpage
  - log data access with "batch no"

  - D-Part
  - repalce "Einschalten" by "Start dialog"

  more ideas
  ==========
  - autotuning
  - improve anti-windup
  - complex recipes, more recipes, selection via LCD/web
  - better statistics: runtime and ms/s for each task
*/

#define VERSION "0.916"

#include <Streaming.h>
#include <ESP8266WiFi.h>
#include <PString.h>
#include "Logfile.h"
#include <LittleFS.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include "Config.h"
#include <DS18B20.h>
#include "Process.h"
#include "src/Clock/Clock.h"
#include "LCDMenu.h"
#include "src/util.h"
#include "src/tasks/tasks.h"
#include "src/MemInfo/MemoryInfo.h"

#define TMP_PORT D7


struct param {
  double set;			// temperature setpoint, from recipe
  double kp, tn, tv, emax, pmax;
  double act;			// actual temperature, from thermometer
  double out;			// actual power, from controller
  boolean released;				// controller is released
  boolean sensorOK;
} p;

ESP8266WebServer server(80);
FS filesystem = LittleFS;
OneWire oneWire(TMP_PORT);
DS18B20 thermometer(&oneWire);
Config cfg;
Process sm(&cfg);


// ------------------------------------------------------------ NTP Task
/* task: ntp client */
int task_ntp() {
  Clock.update();
  return 10;
}


// --------------------------------------------------------- Recipe Task
int task_recipe() {
  sm.update();
  // write values from recipe to global struct for PID controller
  noInterrupts();
  p.set  = sm.out.set;
  p.kp   = sm.out.kp;
  p.tn   = sm.out.tn;
  p.tv   = sm.out.tv;
  p.emax = sm.out.emax;
  p.pmax = sm.out.pmax;
  p.released = (sm.getState() == State::COOKING);
  interrupts();

  // debug Serial << "Free RAM: " << getTotalAvailableMemory() << ", largest: " << getLargestAvailableBlock() << endl;

  return 1000;
}

// ---------------------------------------------------- Temperature Task
/* task: read temperature */
int task_read_temp() {
  static uint32_t counter = 0;
  if (thermometer.isConversionComplete()) {
    p.act = thermometer.getTempC();
    counter = 0;
    if (!p.sensorOK) {
      p.sensorOK = true;
      Logger << "Thermometer ok" << endl;
    }
    thermometer.requestTemperatures();
  } else {
    counter++;
    if (counter > 5) {
      if (p.sensorOK) {
        p.sensorOK = false;
        Logger << "Thermometer gestört" << endl;
      }
      thermometer.requestTemperatures();
    }
  }

  // simulation:
  //p.act = pt1((p.out_b ? 150.0 : 20.0), 200, cycle/1000.0);
  return 1000;
}

// ------------------------------------------------------------ LCD Task
/* task: update LCD */
int task_lcd() {
  LCDMenu_update();
  return 50;
}

// ------------------------------------------------------ Webserver Task
int task_webserver() {
  server.handleClient();
  MDNS.update();
  ArduinoOTA.handle();
  return 20;
}


// ---------------------------------------------------------------- loop
void loop() {
  run_tasks(millis());
}


// -------------------------------------------------- setup Temp. sensor
void setup_ds18b20() {
  Logger << "DS18B20 Bibliothek version: " << DS18B20_LIB_VERSION << endl;
  thermometer.begin();
  thermometer.setResolution(12);
  thermometer.requestTemperatures();
}


// --------------------------------------------------------------- setup
void setup() {
  Serial.begin(115200);

  filesystem.begin();
  Logger << "------ REBOOT" << endl;
  Dir dir = filesystem.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial << "- '" << fileName << "', " << fileSize << " bytes" << endl;
  }
  cfg.setFilename("/config.ini");
  cfg.read();

  Clock.setTimeOffset(cfg.p.tzoffset * 3600);

  LCDMenu_setup();
  Logger << "LCD initialisiert" << endl;

  start_WiFi();
  Logger << "WiFi gestartet" << endl;

  setup_webserver(&server);
  Logger << "Webserver gestartet" << endl;

  MDNS.begin(cfg.p.hostname);
  MDNS.addService("http", "tcp", 80);
  Logger << "MDNS gestartet" << endl;

  setup_OTA();
  Logger << "OTA gestartet" << endl;

  setup_dl();
  Logger << "DataLogger gestartet" << endl;

  setup_ds18b20();
  Logger << "Thermometer initialisiert" << endl;

  start_task(task_lcd, "lcd");
  start_task(task_ntp, "ntp");
  start_task(task_recipe, "recipe");
  start_task(task_read_temp, "r_temp");
  start_task(task_statistics, "stats");
  start_task(task_webserver, "apache");

  // controller is running in timer interrupt
  setup_controller();

  sm.setCallback(&onStateChanged);
}


void onStateChanged() {
  switch (sm.getState()) {
    case State::COOKING:
      dl_startBatch();
      setScreen(Screen::MAIN_COOK);
      break;
    case State::FINISHED:
      dl_endBatch();
      break;
    case State::IDLE:
      dl_endBatch();
  }
}
