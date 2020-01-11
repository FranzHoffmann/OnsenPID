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

#define VERSION "0.904"

#include <Streaming.h>
#include <ESP8266WiFi.h>
#include <PString.h>
#include "Logfile.h"
#include <FS.h>
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

//#define PWM_PORT D4
#define PWM_PORT D8
#define TMP_PORT D7


ESP8266WebServer server(80);
FS filesystem = SPIFFS;
OneWire oneWire(TMP_PORT);
DS18B20 thermometer(&oneWire);


enum WiFiEnum {WIFI_OFFLINE, WIFI_CONN, WIFI_APMODE};

struct param {
  WiFiEnum AP_mode;
  double set;			// temperature setpoint, from recipe
  double kp, tn, tv, emax, pmax;
  double act;			// actual temperature, from thermometer
  double out;			// actual power, from controller
  boolean released;				// controller is released
  int screen;                   // active screen
  boolean sensorOK;
} p;

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
      Logger << "thermometer ok" << endl;
    }
    thermometer.requestTemperatures();
  } else {
    counter++;
    if (counter > 5) {
      if (p.sensorOK) {
        p.sensorOK = false;
        Logger << "thermometer failure, resetting" << endl;
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

    
// ---------------------------------------------------------- setup WiFi
void start_WiFi() {
  Logger << "WiFi-Configuration:" << endl;
  WiFi.printDiag(Logger);
 
  Logger << "Versuche verbindung mit '" << cfg.p.ssid << "'" << endl;
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.p.ssid, cfg.p.pw);
  for (int i=0; i<100; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Logger << "WiFi verbunden (IP: " << WiFi.localIP() << ")" << endl;
      p.AP_mode = WIFI_CONN;
      return;
    }
    delay(100);
  }
  Logger << "WiFi-Verbindung fehlgeschlagen" << endl;

  // open AP
  uint8_t macAddr[6];
  String ssid = "Onsenei-";
  WiFi.softAPmacAddress(macAddr);
  for (int i = 4; i < 6; i++) {
    ssid += String(macAddr[i], HEX);
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);
  Logger << "Access Point erstellt: '" << ssid << "'" << endl;
  Logger << "WiFi-Configuration:" << endl;
  WiFi.printDiag(Logger);

  return;
}


// -------------------------------------------------- setup Temp. sensor
void setup_ds18b20() {
  Logger  << ("Initializing thermometer DS18B20 ") << endl;
  Logger << "DS18B20 Library version: " << DS18B20_LIB_VERSION << endl;
  thermometer.begin();
  thermometer.setResolution(12);
  thermometer.requestTemperatures();
}


// --------------------------------------------------------------- setup
void setup() {
  pinMode(PWM_PORT, OUTPUT);
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
  Logger << "LCD initialized" << endl;
    
  start_WiFi();
  Logger << "WiFi initialized" << endl;

  setup_webserver(&server);
  Logger << "Webserver initialized" << endl;
  
  MDNS.begin(cfg.p.hostname);
  MDNS.addService("http", "tcp", 80);
  Logger << "MDNS initialized" << endl;
  
  setup_OTA();
  Logger << "OTA initialized " << endl;
  
  setup_dl();
  Logger << "DataLogger initialized" << endl;

  setup_ds18b20();
  Logger << "Temperature sensor initialized" << endl;
  
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
	Serial << "onStateChanged() callback" << endl;
	switch(sm.getState()) {
		case State::COOKING:
			dl_startBatch();
			break;
		case State::FINISHED:
			dl_endBatch();
			break;
	}
}
